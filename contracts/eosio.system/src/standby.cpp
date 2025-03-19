#include <eosio.system/eosio.system.hpp>
#include <eosio.token/eosio.token.hpp>
#include <math.h>

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
      check(ratio >= 0 && ratio <= STANDBY_PAY_RATIO_DENOMINATOR, "ratio must be between 0 and RATIO_DENOMINATOR");
      _gstate4.standby_pay_ratio_numerator = ratio;
   }

   void system_contract::setsbslot( uint32_t num_slots ){
      require_auth( get_self() );
      check(num_slots >= 0, "num_slots must be greater than 0");
      _gstate4.num_standby_slots = num_slots;
   }

    void system_contract::setusdbp( uint32_t usd_per_bp ){
      require_auth( get_self() );
      check(usd_per_bp >= 0, "usd_per_bp must be greater than 0");
      _gstate4.usd_per_bp = usd_per_bp;
   }
   
   void system_contract::setbpsparams( uint32_t min_bps, uint32_t max_bps, uint32_t standby_offset ){
      require_auth( get_self() );
      check(min_bps >= 0, "min_bps must be greater than 0");
      check(max_bps >= 0, "max_bps must be greater than 0");
      check(standby_offset >= 0, "standby_offset must be greater than 0");
        check(min_bps <= max_bps, "min_bps must be less than or equal to max_bps");

        _gstate4.min_bps = min_bps;
        _gstate4.max_bps = max_bps;
        _gstate4.standby_offset = standby_offset;
   }

   void system_contract::enabledynbp( bool enable_dynamic_bp ) {
      require_auth( get_self() );
      // check for other conditions before enabling dynamic bp
      if (enable_dynamic_bp){
            check(_gstate4.usd_per_bp > 0, "usd_per_bp must be set");
            check(_gstate4.min_bps > 0, "min_bps must be set");
            check(_gstate4.max_bps > 0, "max_bps must be set");
            check(_gstate4.delphi_pair != name(), "delphi_pair must be set");
            check(_gstate4.price_average_days > 0, "price_average_days must be set");
            check(_gstate4.last_average_price > 0, "last_average_price must > 0");
            // TODO: check more for delphioracle value here
      }
      _gstate4.enable_dynamic_bp = enable_dynamic_bp;
   }
   
   void system_contract::setdelphipr( const name delphi_pair, uint32_t price_average_days ) {
      require_auth( get_self() );
      check(delphi_pair != name(), "delphi_pair must be a valid name");
      check(price_average_days > 0, "price_average_days must be greater than 0");

      auto delphi_pair_itr = delphioracle::get_pairs().require_find(delphi_pair.value,
                                                            "pair name does not exist in the delphi oracle contract");    
      _gstate4.delphi_pair = delphi_pair;
      _gstate4.price_average_days = price_average_days;

      // update delphi price
      update_delphi_price();
   }

   void system_contract::update_delphi_price(){
      const auto ct = current_time_point();
      // check if last price update is more than 1 day
      if (ct - _gstate4.last_price_update < microseconds(useconds_per_day)){
        return;
      }

      // check if delphi pair is set
      if (_gstate4.delphi_pair == name()){
        return;
      } 

      auto delphi_pair_name = _gstate4.delphi_pair;
      eosio::print("delphi_pair_name: ", delphi_pair_name);
      delphioracle::datapoints_t datapoints = delphioracle::get_datapoints(delphi_pair_name);
      auto delphi_pair_itr = delphioracle::get_pairs().require_find(delphi_pair_name.value, "delphi pair does not exist");
      // calculate the price base on base and quote symbol and price point
      uint64_t median_price = 0;
      for (auto itr = datapoints.begin(); itr != datapoints.end(); itr++) {
         eosio::print("datapoint: ", itr->id, " ", itr->value, " ", itr->owner, " ", itr->median, "\n");
         // take first median price
         if (itr->median > 0){
            median_price = itr->median;
            break;
         }
      }
      if (median_price == 0){
         return;
      }

      eosio::print("median_price: ", median_price, "\n");
      uint64_t current_price_rate = median_price * RATE_DECIMAL / pow(10, delphi_pair_itr->quoted_precision);
      eosio::print("current_price_rate: ", current_price_rate, "\n");
      // rolling update the average price in _gstate4.price_average_days
      uint64_t price_average_days = _gstate4.price_average_days;
      uint64_t price_average = _gstate4.last_average_price;
      eosio::print("old price average: ", price_average, "\n");
      if (price_average == 0){
        price_average = current_price_rate;
      }else{
        price_average = (price_average * (price_average_days - 1) + current_price_rate) / price_average_days;
      }
      _gstate4.last_average_price = price_average;
      _gstate4.last_price_update = ct;
      eosio::print("new price average: ", price_average, "\n");
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
      update_standby_share();
      // disable _standbys active record if not in standby_producers
      auto idx = _standbys.get_index<"byactive"_n>();
      double total_standby_time_share_increase = 0;
      std::vector<eosio::name> remove_standby_producers;
      remove_standby_producers.reserve(standby_producers.size());
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

        for(auto& name : remove_standby_producers){
            auto itr = _standbys.find( name.value );
            if( itr != _standbys.end() ) {
                _standbys.modify( itr, same_payer, [&](auto& row) {
                    row.is_active = false;
                });
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
    // amount already > 0
    token::transfer_action transfer_act{ token_account, { {bpay_account, active_permission}, {owner, active_permission} } };
    transfer_act.send( bpay_account, owner, asset(amount, core_symbol()), "standby producer pay" );
   }
}