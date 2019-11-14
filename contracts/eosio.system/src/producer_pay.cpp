#include <eosio.system/eosio.system.hpp>
#include <eosio.token/eosio.token.hpp>

namespace eosiosystem {
   
   using eosio::current_time_point;
   using eosio::microseconds;
   using eosio::token;

   void system_contract::onblock( ignore<block_header> ) {
      using namespace eosio;

      require_auth(get_self());

      block_timestamp timestamp;
      name producer;

      _ds >> timestamp >> producer;

      // _gstate2.last_block_num is not used anywhere in the system contract code anymore.
      // Although this field is deprecated, we will continue updating it for now until the last_block_num field
      // is eventually completely removed, at which point this line can be removed.
      _gstate2.last_block_num = timestamp;

      /** until activated stake crosses this threshold no new rewards are paid */
      if( _gstate.total_activated_stake < min_activated_stake )
         return;

      if( _gstate.last_pervote_bucket_fill == time_point() )  /// start the presses
         _gstate.last_pervote_bucket_fill = current_time_point();

      if (_grewards.activated) {
         // Counts blocks according to producer type
         if (auto it = _rewards.find( producer.value ); it != _rewards.end() ) {
            _grewards.get_counters(it->get_current_type()).total_unpaid_blocks++;

            _rewards.modify( it, same_payer, [&](auto& rec ) {
               rec.get_counters(it->get_current_type()).unpaid_blocks++;
            });
         }
      }
      else {
         /**
         * At startup the initial producer may not be one that is registered / elected
         * and therefore there may be no producer object for them.
         */
         auto prod = _producers.find( producer.value );
         if ( prod != _producers.end() ) {
            _gstate.total_unpaid_blocks++;
            _producers.modify( prod, same_payer, [&](auto& p ) {
               p.unpaid_blocks++;
            });
         }
      }

      /// only update block producers once every minute, block_timestamp is in half seconds
      if( timestamp.slot - _gstate.last_producer_schedule_update.slot > 120 ) {
         uint16_t _;
         checksum256 previous; 

         _ds >> _ >> previous;

         update_elected_producers( timestamp, previous );

         if( (timestamp.slot - _gstate.last_name_close.slot) > blocks_per_day ) {
            name_bid_table bids(get_self(), get_self().value);
            auto idx = bids.get_index<"highbid"_n>();
            auto highest = idx.lower_bound( std::numeric_limits<uint64_t>::max()/2 );
            if( highest != idx.end() &&
                highest->high_bid > 0 &&
                (current_time_point() - highest->last_bid_time) > microseconds(useconds_per_day) &&
                _gstate.thresh_activated_stake_time > time_point() &&
                (current_time_point() - _gstate.thresh_activated_stake_time) > microseconds(14 * useconds_per_day)
            ) {
               _gstate.last_name_close = timestamp;
               idx.modify( highest, same_payer, [&]( auto& b ){
                  b.high_bid = -b.high_bid;
               });
            }
         }
      }
   }

   using namespace eosio;
   void system_contract::fill_buckets() {
      const asset token_supply   = eosio::token::get_supply(token_account, core_symbol().code() );
      const auto ct = current_time_point();
      const auto usecs_since_last_fill = (ct - _gstate.last_pervote_bucket_fill).count();

      if( usecs_since_last_fill > 0 && _gstate.last_pervote_bucket_fill > time_point() ) {
         auto new_tokens = static_cast<int64_t>( (continuous_rate * double(token_supply.amount) * double(usecs_since_last_fill)) / double(useconds_per_year) );
         // needs to be 1/2 Savings, 1/6 Producers (60% to producers, 40% to standbys), 1/6 Voters, 1/6 Genesis Block Member
         auto to_voters             = new_tokens / 6;
         auto to_per_block_pay      = to_voters;
         auto to_per_block_pay_prod = to_voters * producer_perc_reward;
         auto to_per_block_pay_stb  = to_voters * standby_perc_reward;
         auto to_gbm                = to_voters;
         auto to_savings            = new_tokens - (to_voters + to_per_block_pay + to_gbm);

         {
            token::issue_action issue_act{ token_account, { {get_self(), active_permission} } };
            issue_act.send( get_self(), asset(new_tokens, core_symbol()), "issue tokens for producer pay and savings" );
         }
         {
            token::transfer_action transfer_act{ token_account, { {get_self(), active_permission} } };
            transfer_act.send( get_self(), saving_account, asset(to_savings, core_symbol()), "unallocated inflation" );
            transfer_act.send( get_self(), voters_account, asset(to_voters, core_symbol()), "fund voters bucket" );
            transfer_act.send( get_self(), genesis_account, asset(to_gbm, core_symbol()), "fund gbm bucket" );

            if (_grewards.activated) {
               transfer_act.send( get_self(), bpay_account, asset(to_per_block_pay_prod, core_symbol()), "fund bps bucket" );
               transfer_act.send( get_self(), spay_account, asset(to_per_block_pay_stb, core_symbol()), "fund sps bucket" );
            }
            else
               transfer_act.send( get_self(), bpay_account, asset(to_per_block_pay, core_symbol()), "fund bps bucket" );
         }

         if (_grewards.activated) {
            _grewards.get_counters(reward_type::producer).perblock_bucket += to_per_block_pay_prod;
            _grewards.get_counters(reward_type::standby).perblock_bucket += to_per_block_pay_stb;
         }
         else
            _gstate.perblock_bucket += to_per_block_pay;

         _gstate.voters_bucket += to_voters;
         _gstate.last_pervote_bucket_fill = ct;
      }
   }

   void system_contract::claim_producer_rewards( const name owner, bool as_gbm ) {
      require_auth( owner );

      const auto& prod = _producers.get( owner.value );
      check( prod.active(), "producer does not have an active key" );

      check( _gstate.total_activated_stake >= min_activated_stake,
                    "cannot claim rewards until the chain is activated (at least 15% of all tokens participate in voting)" );

      const auto ct = current_time_point();

      check( ct - prod.last_claim_time > microseconds(useconds_per_day), "already claimed rewards within past day" );

      fill_buckets();

      auto prod2 = _producers2.find( owner.value );

      /// New metric to be used in pervote pay calculation. Instead of vote weight ratio, we combine vote weight and
      /// time duration the vote weight has been held into one metric.
      const auto last_claim_plus_3days = prod.last_claim_time + microseconds(3 * useconds_per_day);

      bool crossed_threshold       = (last_claim_plus_3days <= ct);
      bool updated_after_threshold = true;
      if ( prod2 != _producers2.end() ) {
         updated_after_threshold = (last_claim_plus_3days <= prod2->last_votepay_share_update);
      } else {
         prod2 = _producers2.emplace( owner, [&]( producer_info2& info  ) {
            info.owner                     = owner;
            info.last_votepay_share_update = ct;
         });
      }

      // Note: updated_after_threshold implies cross_threshold (except if claiming rewards when the producers2 table row did not exist).
      // The exception leads to updated_after_threshold to be treated as true regardless of whether the threshold was crossed.
      // This is okay because in this case the producer will not get paid anything either way.
      // In fact it is desired behavior because the producers votes need to be counted in the global total_producer_votepay_share for the first time.

      if (_grewards.activated) {
         constexpr reward_type reward_types[] = { reward_type::producer, reward_type::standby };

         struct data {
            int64_t     per_block_pay = 0;
            double      percent = 0.0;
            eosio::name account;
            std::string check_msg;
            std::string tx_msg;
         };

         std::map<reward_type, data> info {
            {  reward_type::producer,
               { 0,
                 producer_perc_reward,
                 bpay_account,
                 "producer per block pay must be greater or equal to 0",
                 "producer block pay"
               }
            },
            {  reward_type::standby,
               { 0,
                 standby_perc_reward,
                 spay_account,
                 "standby per block pay must be greater or equal to 0",
                 "standby block pay"
               }
            }
         };

         const auto& reward = _rewards.get(owner.value);

         for (auto type: reward_types) {
            auto& curr_info = info[type];

            if (auto total_unpaid_blocks = _grewards.get_counters(type).total_unpaid_blocks; total_unpaid_blocks > 0) {
               auto counters = reward.get_counters(type);

               if (counters.selection > 0 && counters.unpaid_blocks > 0) {
                  const auto perblock_buckets = _gstate.perblock_bucket * curr_info.percent;
                  
                  curr_info.per_block_pay += perblock_buckets * counters.unpaid_blocks / total_unpaid_blocks;
               }
            }

            check(curr_info.per_block_pay >= 0, curr_info.check_msg);
         }

         double new_votepay_share = update_producer_votepay_share( prod2,
                                    ct,
                                    updated_after_threshold ? 0.0 : prod.total_votes,
                                    true // reset votepay_share to zero after updating
                                 );

         for (auto type: reward_types) {
            auto& cnt = _grewards.get_counters(type);

            cnt.perblock_bucket -= info[type].per_block_pay;
            cnt.total_unpaid_blocks -= _rewards.get(owner.value).get_counters(type).unpaid_blocks;
         }

         update_total_votepay_share( ct, -new_votepay_share, (updated_after_threshold ? prod.total_votes : 0.0) );

         _producers.modify( prod, same_payer, [&](auto& p) {
            p.last_claim_time = ct;
            //p.unpaid_blocks   = 0;
         });

         _rewards.modify( reward, same_payer, [&](auto& rec) {
            rec.reset_counters();
         });

         for (auto type: reward_types) {
            auto& curr_info = info[type];

            if (curr_info.per_block_pay > 0) {
               if (as_gbm){
                  send_genesis_token( curr_info.account, owner, asset(curr_info.per_block_pay, core_symbol()));
               }
               else {
                  token::transfer_action transfer_act{ token_account, { {curr_info.account, active_permission}, {owner, active_permission} } };
                  transfer_act.send( curr_info.account, owner, asset(curr_info.per_block_pay, core_symbol()), curr_info.tx_msg );
               }
            }
         }
      }
      else {
         int64_t per_block_pay = 0;

         if (_gstate.total_unpaid_blocks > 0)
            per_block_pay = (static_cast<double>(_gstate.perblock_bucket) * prod.unpaid_blocks) / _gstate.total_unpaid_blocks;

         check( per_block_pay >= 0, "producer per block pay must be greater or equal to 0" );

         double new_votepay_share = update_producer_votepay_share( prod2,
                                       ct,
                                       updated_after_threshold ? 0.0 : prod.total_votes,
                                       true // reset votepay_share to zero after updating
                                    );

         _gstate.perblock_bucket     -= per_block_pay;
         _gstate.total_unpaid_blocks -= prod.unpaid_blocks;

         update_total_votepay_share( ct, -new_votepay_share, (updated_after_threshold ? prod.total_votes : 0.0) );

         _producers.modify( prod, same_payer, [&](auto& p) {
            p.last_claim_time = ct;
            p.unpaid_blocks   = 0;
         });

         if( per_block_pay > 0 ) {
            if(as_gbm){
               send_genesis_token( bpay_account, owner, asset(per_block_pay, core_symbol()));
            }
            else {
               token::transfer_action transfer_act{ token_account, { {bpay_account, active_permission}, {owner, active_permission} } };
               transfer_act.send( bpay_account, owner, asset(per_block_pay, core_symbol()), "producer block pay" );
            }
         }
      }
   }

   void system_contract::claimrewards( const name& owner ) {
      claim_producer_rewards(owner, false);
   }

   void system_contract::claimgbmprod( const name owner ) {
      claim_producer_rewards(owner, true);
   }

} //namespace eosiosystem
