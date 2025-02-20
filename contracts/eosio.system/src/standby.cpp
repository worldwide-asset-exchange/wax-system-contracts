
#include <eosio.system/eosio.system.hpp>
#include <eosio.token/eosio.token.hpp>

namespace eosiosystem {

   using eosio::const_mem_fun;
   using eosio::current_time_point;
   using eosio::indexed_by;
   using eosio::microseconds;
   using eosio::singleton;
   using namespace eosio;

   void system_contract::addstdbblock(const name account) {
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

   void system_contract::rmstdbblock(const name account) {
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

   void system_contract::setstdbratio( double ratio ){
      require_auth( get_self() );
      check(ratio >= 0 && ratio <= 1, "ratio must be between 0 and 1");
      _gstate4.standby_pay_ratio = ratio;
   }

   void system_contract::setstdbslot( uint32_t num_slots ){
      require_auth( get_self() );
      check(num_slots >= 0, "num_slots must be greater than 0");
      _gstate4.num_standby_slots = num_slots;
   }
   
   void system_contract::update_standby_share(){
      const auto ct = current_time_point();

      auto idx = _standbys.get_index<"byactive"_n>();
      double total_standby_time_share_increase = 0;
      for ( auto itr = idx.begin(); itr != idx.end(); itr++ ) {
          if(itr->is_active){
              time_point last_update = itr->last_standby_share_update;
              double share_increase = double((ct - last_update).count());
              double new_account_share = itr->standby_share + share_increase;
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
      update_standby_share();
      require_auth( get_self() );
      // disable _standbys active record if not in standby_producers
      auto idx = _standbys.get_index<"byactive"_n>();
      double total_standby_time_share_increase = 0;
      for ( auto itr = idx.begin(); itr != idx.end(); itr++ ) {
          if(itr->is_active){
              if(std::find(standby_producers.begin(), standby_producers.end(), itr->owner) == standby_producers.end()){
                  idx.modify( itr, same_payer, [&](auto& row) {
                      row.is_active = false;
                  });
              }
          }else{
              // sort by active so break if not active
              break;
          }
      }
      auto ct = current_time_point();
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

    double share = itr->standby_share;
    check(share > 0, "no standby share to claim");

    double total_share = _gstate4.total_standby_share;
    double amount = 0;
    if (total_share > 0){
        double total_bucket = _gstate4.standby_bucket;
        amount = total_bucket * share / total_share;
    }
    check(amount > 0, "no standby reward to claim");

    _gstate4.standby_bucket -= amount;
    _gstate4.total_standby_share -= share;

    _standbys.modify( itr, same_payer, [&](auto& row) {
        row.standby_share = 0;
        row.last_standby_share_update = ct;
        row.last_claim_time = ct;
    });

    if( amount > 0 ) {
        token::transfer_action transfer_act{ token_account, { {bpay_account, active_permission}, {owner, active_permission} } };
        transfer_act.send( bpay_account, owner, asset(amount, core_symbol()), "standby producer pay" );
      }
   }
}