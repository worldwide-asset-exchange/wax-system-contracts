#pragma once

#include <eosio/asset.hpp>
#include <eosio/multi_index.hpp>

namespace eosiosystem {
  static const time_point guild_bp_pay_start_time(eosio::seconds(1689379200));     // July 15st 2023 00:00:00
  static constexpr int64_t  max_daily_guilds_fund = 321'000'00000000; // to_savings + to_per_block_pay per day
  TABLE guild_contract_state { 
        vector<name> top21;         // top21 guilds sorted by score
        vector<name> standby;
        asset reserves;             // treasury
        asset pending;              // uncredited standby pay
        uint32_t minimum;           // minimum score to pass as standby
        uint8_t decimals;           // decimal places of scores 
        double scaling_threshhold;  // score percentage that needs to be hit for full standby pay
        asset lst_supply;           // base value for the next reward approximation
        time_point  lst_proc;       // starting point for the next reward calculation
        EOSLIB_SERIALIZE(guild_contract_state, (top21)(standby)(reserves)(pending)(minimum)(decimals)(scaling_threshhold)(lst_supply)(lst_proc))
  };
  typedef eosio::singleton<name("state"), guild_contract_state> guild_contract_state_singleton;

  /**
   * Store guild.oig fund status
   * 
   */
   struct [[eosio::table("guildstate"), eosio::contract("eosio.system")]] guild_funding_state {
      time_point  lst_fund = time_point();
      int64_t     max_saving_balance = 1'000'000'00000000;

      EOSLIB_SERIALIZE( guild_funding_state, (lst_fund)(max_saving_balance) )
   };

   typedef eosio::singleton<name("guildstate"), guild_funding_state> guild_funding_state_singleton;

} /// namespace eosiosystem