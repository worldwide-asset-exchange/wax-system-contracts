#include <eosio.system/eosio.system.hpp>
#include <eosio.token/eosio.token.hpp>
#include <eosio.system/guilds.hpp>

namespace eosiosystem {

  using eosio::current_time_point;
  using eosio::microseconds;
  using eosio::token;

  void system_contract::fundguilds() {
    check(has_auth(guilds_account) || has_auth(_self), "Lacking OIG authority" );
    const auto ct = current_time_point();
    check(ct > guild_bp_pay_start_time, "Only available after guilds pay time");

    fill_buckets();

    guild_contract_state_singleton guild_contract_state(guilds_account, guilds_account.value);
    auto guild_contract_status = guild_contract_state.get();

    check(guild_contract_status.pending.amount <= max_daily_guilds_fund, "guild fund greater than daily limit");

    guild_funding_state_singleton guild_funding_state(_self, _self.value);
    auto guild_funding_status = guild_funding_state.get_or_create(_self);
    check( ct - guild_funding_status.lst_fund > microseconds(useconds_per_day), "already funded within past day" );

    guild_funding_status.lst_fund = ct;
    guild_funding_state.set(guild_funding_status, get_self());

    if (guild_contract_status.pending.amount > 0) {
      token::transfer_action transfer_act{ token_account, { {saving_account, active_permission} } };
      transfer_act.send( saving_account, guilds_account, guild_contract_status.pending, "fund daily bpay" );
    }

    burnexsaving_action burn_act{ get_self(), { {get_self(), active_permission} } };
    burn_act.send();
  }

  void system_contract::burnexsaving() {
    require_auth(_self);

    guild_funding_state_singleton guild_funding_state(_self, _self.value);
    auto guild_funding_status = guild_funding_state.get_or_create(_self);
    const asset saving_balance = token::get_balance(token_account, saving_account, core_symbol().code() );
    if (saving_balance.amount > guild_funding_status.max_saving_balance) {
      auto burn_amount = saving_balance.amount - guild_funding_status.max_saving_balance;
      token::transfer_action transfer_act{ token_account, { {saving_account, active_permission} } };
      transfer_act.send( saving_account, _self, asset(burn_amount, core_symbol()), "burn exeeded saving amount" );

      token::retire_action retire_act{ token_account, { {get_self(), active_permission} } };
      retire_act.send( asset(burn_amount, core_symbol()), "burn exeeded saving amount" );
    }
  }

  void system_contract::configguilds(int64_t max_saving_balance) {
    require_auth(_self);

    check(max_saving_balance > 0, "max_saving_balance must be greater than 0");

    guild_funding_state_singleton guild_funding_state(_self, _self.value);
    auto guild_funding_status = guild_funding_state.get_or_create(_self);

    guild_funding_status.max_saving_balance = max_saving_balance;
    guild_funding_state.set(guild_funding_status, get_self());
  }

}