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

      const auto last_timestamp = _gstate2.last_block_num;
      _gstate2.last_block_num = timestamp;

      /** until activated stake crosses this threshold no new rewards are paid */
      if( _gstate.total_activated_stake < min_activated_stake )
         return;

      if( _gstate.last_pervote_bucket_fill == time_point() )  /// start the presses
         _gstate.last_pervote_bucket_fill = current_time_point();

      uint16_t    ignored1;
      checksum256 previous;

      _ds >> ignored1 >> previous;

      // TODO: uncomment and subsequently remove after upgrading testnet
      upgrade_rewards_table();

      if (_greward.activated) {
         checksum256 ignored2;
         uint32_t schedule_version;

         _ds >> ignored2 >> ignored2 >> schedule_version;

         update_producer_reward_status(schedule_version);
         track_blocks(producer, last_timestamp.slot, timestamp.slot);
      }
      else {
         /**
         * At startup the initial producer may not be one that is registered / elected
         * and therefore there may be no producer object for them.
         */
         if ( const auto prod = _producers.find( producer.value ); prod != _producers.end() ) {
            _gstate.total_unpaid_blocks++;
            _producers.modify( prod, same_payer, [&](auto& p ) {
               p.unpaid_blocks++;
            });
         }
      }

      /// only update block producers once every minute, block_timestamp is in half seconds
      if( timestamp.slot - _gstate.last_producer_schedule_update.slot > 120 ) {
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
         const auto unstake_time = std::min(current_time_point(), gbm_final_time);
         const int64_t delta_time_usec = (gbm_final_time - unstake_time).count();
         auto new_tokens = static_cast<int64_t>( (continuous_rate * double(token_supply.amount) * double(usecs_since_last_fill)) / double(useconds_per_year) );

         // Inflation gets split: 7/15 Savings, 1/5 Voters, 1/3 Producers (and split 60% to full bps, 40% to standbys),
         // GBM receives a linearly deflating share over three years
         int64_t to_voters             = new_tokens / 5;
         int64_t to_per_block_pay      = to_voters;
         double adjusted_producer_perc_reward = producer_perc_reward;
         if (_greward.activated) {
            // The standbys bucket gets added to in proportion to the number of standbys that are active,
            // so as to not overpay them when they are below capacity
            double standby_perc_reward = (1.0 - adjusted_producer_perc_reward) * static_cast<double>(num_standbys()) / static_cast<double>(max_standbys);
            adjusted_producer_perc_reward = adjusted_producer_perc_reward / (adjusted_producer_perc_reward + standby_perc_reward);
            // This ensures full producers get the same payout as before standbys were introduced
            // At full standby capacity, the ratio works out to 1/5 / 60% == 1/3 of inflation going to producers
            to_per_block_pay /= adjusted_producer_perc_reward;
         }
         int64_t to_per_block_pay_prod  = to_per_block_pay * adjusted_producer_perc_reward;
         int64_t to_per_block_pay_stb   = to_per_block_pay - to_per_block_pay_prod;
         int64_t to_gbm                 = (to_voters + to_per_block_pay) * (delta_time_usec / double(useconds_in_gbm_period));
         int64_t to_savings             = new_tokens - (to_voters + to_per_block_pay);
         new_tokens                     += to_gbm;

         {
            token::issue_action issue_act{ token_account, { {get_self(), active_permission} } };
            issue_act.send( get_self(), asset(new_tokens, core_symbol()), "issue tokens for producer pay and savings" );
         }
         {
            token::transfer_action transfer_act{ token_account, { {get_self(), active_permission} } };
            transfer_act.send( get_self(), saving_account, asset(to_savings, core_symbol()), "unallocated inflation" );
            transfer_act.send( get_self(), voters_account, asset(to_voters, core_symbol()), "fund voters bucket" );

            if (to_gbm > 0)
               transfer_act.send( get_self(), genesis_account, asset(to_gbm, core_symbol()), "fund gbm bucket" );

            if (_greward.activated) {
               transfer_act.send( get_self(), bpay_account, asset(to_per_block_pay_prod, core_symbol()), "fund bps bucket" );
               if (to_per_block_pay_stb > 0)
                  transfer_act.send( get_self(), spay_account, asset(to_per_block_pay_stb, core_symbol()), "fund sps bucket" );
            }
            else
               transfer_act.send( get_self(), bpay_account, asset(to_per_block_pay, core_symbol()), "fund bps bucket" );
         }

         if (_greward.activated) {
            _greward.get_counters(reward_type::producer).perblock_bucket += to_per_block_pay_prod;
            _greward.get_counters(reward_type::standby).perblock_bucket += to_per_block_pay_stb;
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

      if (_greward.activated) {
          struct data {
            int64_t     per_block_pay = 0;
            eosio::name account;
            std::string check_msg;
            std::string tx_msg;
         };

         std::map<reward_type, data> info {
            {  reward_type::producer,
               { 0,
                 bpay_account,
                 "producer per block pay must be greater or equal to 0",
                 "producer block pay"
               }
            },
            {  reward_type::standby,
               { 0,
                 spay_account,
                 "standby per block pay must be greater or equal to 0",
                 "standby block pay"
               }
            }
         };

         const auto& reward = _rewards.get(owner.value);

         for (auto type: { reward_type::producer, reward_type::standby }) {
            auto& curr_info = info[type];
            auto& curr_gcnt = _greward.get_counters(type);
            auto& curr_cnt = reward.get_counters(type);

            if (const auto total_unpaid_blocks = curr_gcnt.total_unpaid_blocks; total_unpaid_blocks > 0) {
               if (curr_cnt.unpaid_blocks > 0) {
                 curr_info.per_block_pay +=
                     curr_gcnt.perblock_bucket * curr_cnt.unpaid_blocks / total_unpaid_blocks;
               }
            }

            check(curr_info.per_block_pay >= 0, curr_info.check_msg);

            curr_gcnt.perblock_bucket -= curr_info.per_block_pay;
            curr_gcnt.total_unpaid_blocks -= curr_cnt.unpaid_blocks;

            if (curr_info.per_block_pay > 0) {
               if (as_gbm)
                  send_genesis_token( curr_info.account, owner, asset(curr_info.per_block_pay, core_symbol()));
               else {
                  token::transfer_action transfer_act{ token_account, { {curr_info.account, active_permission}, {owner, active_permission} } };
                  transfer_act.send(curr_info.account, owner, asset(curr_info.per_block_pay, core_symbol()), curr_info.tx_msg );
               }
            }
         }

         _producers.modify( prod, same_payer, [&](auto& p) {
            p.last_claim_time = ct;
         });

         _rewards.modify( reward, same_payer, [&](auto& rec) {
            rec.reset_rewards_counters();
         });

      }
      else {
         int64_t per_block_pay = 0;

         if (_gstate.total_unpaid_blocks > 0)
            per_block_pay = (static_cast<double>(_gstate.perblock_bucket) * prod.unpaid_blocks) / _gstate.total_unpaid_blocks;

         check( per_block_pay >= 0, "producer per block pay must be greater or equal to 0" );

         _gstate.perblock_bucket     -= per_block_pay;
         _gstate.total_unpaid_blocks -= prod.unpaid_blocks;

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

   // TODO: remove after upgrading testnet
   void system_contract::upgrade_rewards_table() {
      if(_grewardold.producer_blocks_performance_window == 0) {
        return;
      }

      // This just proves the upgrade mechanism will work. Temporarily uncomment to test
      // if(_grewardold.producer_blocks_performance_window != 1) {
      //   _grewardold.activated = _greward.activated;
      //   _grewardold.counters = _greward.counters;
      //   _grewardold.proposed_top_producers = _greward.proposed_top_producers;
      //   _grewardold.current_producers = _greward.current_producers;
      //   _grewardold.producer_blocks_performance_window = _greward.producer_blocks_performance_window;
      //   _grewardold.standby_blocks_performance_window = _greward.standby_blocks_performance_window;
      //   _grewardold.random_standby_selection = _greward.random_standby_selection;
      //   _grewardold.last_standby_index = _greward.last_standby_index;
      //   _grewardold.avg_standby_blocks_ratio = _greward.avg_standby_blocks_ratio;
      //   _grewardold.avg_standby_blocks_samples = _greward.avg_standby_blocks_samples;
      //   _grewardold.avg_producer_performances = _greward.avg_producer_performances;
      //   _grewardold.producer_blocks_performance_window = 1;
      // } else {
      //   _grewardold.producer_blocks_performance_window = _greward.producer_blocks_performance_window;
      // }

      _greward.activated = _grewardold.activated;
      _greward.counters = _grewardold.counters;
      _greward.proposed_top_producers = _grewardold.proposed_top_producers;
      _greward.current_producers = _grewardold.current_producers;
      _greward.producer_blocks_performance_window = _grewardold.producer_blocks_performance_window;
      _greward.standby_blocks_performance_window = _grewardold.standby_blocks_performance_window;
      _greward.random_standby_selection = _grewardold.random_standby_selection;
      _greward.last_standby_index = _grewardold.last_standby_index;
      _greward.avg_standby_blocks_ratio = _grewardold.avg_standby_blocks_ratio;
      _greward.avg_standby_blocks_samples = _grewardold.avg_standby_blocks_samples;
      _greward.avg_producer_performances = _grewardold.avg_producer_performances;

      _grewardold.producer_blocks_performance_window = 0;
   }

   void system_contract::claimrewards( const name& owner ) {
      claim_producer_rewards(owner, false);
   }

   void system_contract::claimgbmprod( const name owner ) {
      claim_producer_rewards(owner, true);
   }

} //namespace eosiosystem
