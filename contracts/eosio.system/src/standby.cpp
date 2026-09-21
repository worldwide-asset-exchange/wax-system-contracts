
#include <eosio.system/eosio.system.hpp>
#include <eosio.token/eosio.token.hpp>

namespace eosiosystem {

   using eosio::const_mem_fun;
   using eosio::current_time_point;
   using eosio::indexed_by;
   using eosio::microseconds;
   using eosio::singleton;
   using namespace eosio;

   void system_contract::disallowsb(const name account) {
    require_auth( get_self() );
    auto itr = _standby_disallow.find( account.value );
    if( itr == _standby_disallow.end() ) {
      _standby_disallow.emplace( _self, [&]( auto& row ) {
          row.owner = account;
       });
    }else{
      check(false, "account already in standby disallow list");
    }
   }

   void system_contract::allowsb(const name account) {
    require_auth( get_self() );
    auto itr = _standby_disallow.find( account.value );
    if( itr == _standby_disallow.end() ) {
      check(false, "account not exist in standby disallow list");
    }else{
      _standby_disallow.erase(itr);
    }
   }

   bool system_contract::is_disallow_standby( name account )
   {
      const auto & itr = _standby_disallow.find( account.value );
      return itr != _standby_disallow.end();
   }

   void system_contract::setsbratio( uint64_t ratio ){
      require_auth( get_self() );
      // Settle the interval since the last fill under the split in force, so the new value
      // applies from now rather than to time already served (WCAP-SYS-2026-013).
      fill_buckets();
      // WCAP-SYS-2026-002: `ratio >= 0` was always true for an unsigned value, and the
      // message named a constant the code did not compare against.
      check( ratio <= PAY_SPLIT_SCALE, "ratio cannot exceed PAY_SPLIT_SCALE (" + std::to_string( PAY_SPLIT_SCALE ) + ")" );
      _gstate4.standby_slot_weight = ratio;
   }

   void system_contract::setsbslot( uint32_t num_slots ){
      require_auth( get_self() );
      fill_buckets();   // as setsbratio
      // WCAP-SYS-2026-002: `num_slots >= 0` was always true while the message promised
      // `> 0`. Zero is a valid value: it is the struct default, no standbys are elected and
      // no block pay is routed to the standby bucket (producer_pay.cpp keeps the split free
      // of a zero divisor because apc >= 1). Every election reconciles the standby table, so
      // rows elected before a switch to zero are deactivated at the next one
      // (WCAP-SYS-2026-013). What the action lacked was a ceiling: until now only
      // max_considered_producers, in another file, kept the standby list bounded.
      check( num_slots <= max_standby_slots, "num_slots cannot exceed " + std::to_string( max_standby_slots ) );
      _gstate4.num_standby_slots = num_slots;
   }
   
   void system_contract::update_standby_share(){
      const auto ct = current_time_point();

      auto idx = _standbys.get_index<"byactive"_n>();
      uint64_t total_standby_time_share_increase = 0;
      for ( auto itr = idx.begin(); itr != idx.end(); itr++ ) {
          if(itr->is_active){
              time_point last_update = itr->last_standby_share_update;
              uint64_t share_increase = (ct - last_update).count();
              uint64_t new_account_share = itr->standby_share + share_increase;
              total_standby_time_share_increase += share_increase;

              idx.modify( itr, same_payer, [&](auto& row) {
                  row.standby_share = new_account_share;
                  row.last_standby_share_update = ct;
              });
          }else{
              break;
          }
      }
      _gstate4.last_standby_state_update = ct;
      _gstate4.total_standby_share += total_standby_time_share_increase;
   }

   void system_contract::update_standby_producers(const std::vector<eosio::name>& standby_producers) {
      // disable _standbys active record if not in standby_producers
      auto idx = _standbys.get_index<"byactive"_n>();
      const auto ct = current_time_point();

      std::vector<eosio::name> remove_standby_producers;
      for ( auto itr = idx.begin(); itr != idx.end(); itr++ ) {
        if(itr->is_active){
            if(std::find(standby_producers.begin(), standby_producers.end(), itr->owner) == standby_producers.end()){
                remove_standby_producers.push_back(itr->owner);
            }
        }else{
            // sort by active so break if not active
            break;
        }
      }
      uint64_t total_standby_time_share_increase = 0;
      for(auto& name : remove_standby_producers){
        auto itr = _standbys.find( name.value );
        if( itr != _standbys.end() ) {
            time_point last_update = itr->last_standby_share_update;
            uint64_t share_increase = (ct - last_update).count();
            uint64_t new_account_share = itr->standby_share + share_increase;
            total_standby_time_share_increase += share_increase;
            _standbys.modify( itr, same_payer, [&](auto& row) {
                row.is_active = false;
                row.standby_share = new_account_share;
                row.last_standby_share_update = ct;
            });
        }
      }
      if (total_standby_time_share_increase > 0){
        _gstate4.last_standby_state_update = ct;
        _gstate4.total_standby_share += total_standby_time_share_increase;  
      }

      for(auto& producer: standby_producers){
         // check if record is in _standbys
          auto itr = _standbys.find( producer.value );
          if( itr == _standbys.end() ) {
              _standbys.emplace( _self, [&]( auto& row ) {
                  row.owner = producer;
                  row.standby_share = 0;
                  row.last_standby_share_update = ct;
                  row.is_active = true;
              });
          }else{
              if (itr->is_active == false){
                  _standbys.modify( itr, same_payer, [&](auto& row) {
                      row.is_active = true;
                      row.last_standby_share_update = ct;
                  });
              }
          }  
      }
   }

   void system_contract::claimstandby(const name owner) {
    require_auth( owner );
    fill_buckets();
    update_standby_share();

    auto itr = _standbys.find( owner.value );
    check(itr != _standbys.end(), "account not in standby list");

    const auto ct = current_time_point();
    check( ct - itr->last_claim_time > microseconds(useconds_per_day), "already claimed rewards within past day" );

    const uint64_t share = itr->standby_share;
    check( share > 0, "no standby share to claim" );

    // WCAP-SYS-2026-006: integer arithmetic, and the bucket defended the way
    // collect_voter_reward defends voters_bucket. A share above the total is not reachable
    // through this contract's own accounting, so it is treated as corruption and refused
    // (fail closed) rather than paid out. With share <= total_share the exact integer
    // division cannot exceed the bucket; the last check states that invariant. The old
    // double arithmetic would have trapped on the uint64 narrowing instead.
    const uint64_t total_share = _gstate4.total_standby_share;
    check( share <= total_share, "standby share exceeds the total standby share" ); //should never happen
    const uint64_t amount = static_cast<uint64_t>( (uint128_t)_gstate4.standby_bucket * share / total_share );
    check( amount >= 1, "no standby reward to claim" );
    check( amount <= _gstate4.standby_bucket, "standby reward exceeds the standby bucket" ); //should never happen

    _gstate4.standby_bucket      -= amount;
    _gstate4.total_standby_share -= share;

    _standbys.modify( itr, same_payer, [&](auto& row) {
        row.standby_share = 0;
        row.last_standby_share_update = ct;
        row.last_claim_time = ct;
    });
    token::transfer_action transfer_act{ token_account, { {bpay_account, active_permission}, {owner, active_permission} } };
    transfer_act.send( bpay_account, owner, asset( static_cast<int64_t>(amount), core_symbol() ), "standby producer pay" );
   }
}