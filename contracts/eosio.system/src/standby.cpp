
#include <eosio.system/eosio.system.hpp>
#include <eosio.token/eosio.token.hpp>


namespace eosiosystem {

   using eosio::const_mem_fun;
   using eosio::current_time_point;
   using eosio::indexed_by;
   using eosio::microseconds;
   using eosio::singleton;

   void system_contract::add_standby_block(const name account) {
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

   void system_contract::remove_standby_block(const name account) {
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

   
   void system_contract::update_standby_share(){
      const auto ct = current_time_point();

      if( ct <= _gstate4.last_standby_state_update ) {
         return;
      }

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
      _gstate4.total_standy_share += total_standby_time_share_increase;
   }

   /*

   bool system_contract::add_standby_account( name account )
   {
   }

   bool system_contract::remove_standby_account( name account )
   {
   }
   */


}