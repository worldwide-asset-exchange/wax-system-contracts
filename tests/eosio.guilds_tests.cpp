#include <boost/test/unit_test.hpp>
#include <eosio/chain/contract_table_objects.hpp>
#include <eosio/chain/global_property_object.hpp>
#include <eosio/chain/resource_limits.hpp>
#include <eosio/chain/wast_to_wasm.hpp>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <fc/log/logger.hpp>
#include <eosio/chain/exceptions.hpp>
#include <Runtime/Runtime.h>

#include "eosio.system_tester.hpp"

using namespace eosio_system;

BOOST_AUTO_TEST_SUITE(eosio_guilds_tests)

class eosio_guilds_tester : public eosio_system_tester {
public:
    symbol core_symbol = symbol{CORE_SYM};

    action_result fundguilds(name sender) {
        return push_action(
            sender,
            "fundguilds"_n,
            mvo()
        );
    }

    action_result configguilds(name sender, int64_t max_saving_balance) {
        return push_action(
            sender,
            "configguilds"_n,
            mvo()("max_saving_balance",max_saving_balance)
        );
    }

    void set_guilds_pending_fund(int64_t pending) {
        base_tester::push_action( "guilds.oig"_n, "setstate"_n, "guilds.oig"_n, mvo()
                                ("minimum",    100)
                                ("decimals",     8)
                                ("scaling_threshhold", 1)
                                ("pending", asset(pending, core_symbol))
                                );
    }

    fc::variant get_funding_state() {
        vector<char> data = get_row_by_account(config::system_account_name, config::system_account_name, "guildstate"_n, "guildstate"_n);
        return data.empty() ? fc::variant() : abi_ser.binary_to_variant("guild_funding_state", data, abi_serializer_max_time);
    }
};

BOOST_FIXTURE_TEST_CASE(missing_authority_to_call_fundguilds, eosio_guilds_tester) try {
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("Lacking OIG authority"), fundguilds("alice1111111"_n));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(call_fundguilds_before_guilds_start_time, eosio_guilds_tester) try {
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("Only available after guilds pay time"), fundguilds("eosio"_n));

    produce_block();
    produce_block( fc::days(365 * 4 + 14) );
     create_account_with_resources("guilds.oig"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("Only available after guilds pay time"), fundguilds("guilds.oig"_n));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(guilds_funding_amount_greater_than_max, eosio_guilds_tester) try {
    create_account_with_resources("guilds.oig"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    set_code( "guilds.oig"_n, contracts::util::guilds_wasm() );
    set_abi( "guilds.oig"_n, contracts::util::guilds_abi().data() );

    set_guilds_pending_fund(801'000'00000000);
    produce_block();
    produce_block( fc::days(365 * 4 + 16) );

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("guild fund greater than daily limit"), fundguilds("guilds.oig"_n));
} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(fundguilds_success, eosio_guilds_tester) try {
    const double continuous_rate = 0.04879;
    const double usecs_per_year  = 52 * 7 * 24 * 3600 * 1000000ll;
    const double secs_per_year   = 52 * 7 * 24 * 3600;
    const uint64_t guild_bp_pay_start_time_us = 1689379200 * 1000000ll;

    // set up voting to pass thresh_activated_stake_time
    const asset large_asset = core_sym::from_string("80.0000");
    create_account_with_resources( "defproducera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );
    create_account_with_resources( "producvotera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );
    BOOST_REQUIRE_EQUAL(success(), regproducer("defproducera"_n));
    transfer( config::system_account_name, "producvotera", core_sym::from_string("400000000.0000"), config::system_account_name);
    BOOST_REQUIRE_EQUAL(success(), stake("producvotera", core_sym::from_string("100000000.0000"), core_sym::from_string("100000000.0000")));
    BOOST_REQUIRE_EQUAL(success(), vote( "producvotera"_n, { "defproducera"_n }));
    BOOST_REQUIRE_EQUAL(success(), regproducer("defproducera"_n));

    // setup guilds.oig
    create_account_with_resources("guilds.oig"_n, config::system_account_name, core_sym::from_string("1000.0000"), false,
core_sym::from_string("100.0000"), core_sym::from_string("100.0000"));
    set_code( "guilds.oig"_n, contracts::util::guilds_wasm() );
    set_abi( "guilds.oig"_n, contracts::util::guilds_abi().data() );

    set_guilds_pending_fund(10'00000000);
    configguilds("eosio"_n, 1'000'000'00000000);
    produce_block();
    auto lastest_block_before_funding = produce_block( fc::days(365 * 4 + 16) );

    auto initial_funding_state = get_funding_state();
    BOOST_REQUIRE_EQUAL(initial_funding_state["lst_fund"].as<time_point>() == time_point(), true);

    const asset initial_guilds_balance = get_balance("guilds.oig"_n);
    BOOST_REQUIRE_EQUAL(initial_guilds_balance.get_amount(), 0);

    const asset initial_saving_balance = get_balance("eosio.saving"_n);
    BOOST_REQUIRE_EQUAL(initial_saving_balance.get_amount(), 0);

    const asset initial_supply  = get_token_supply();
    const auto     initial_global_state      = get_global_state();
    const uint64_t initial_claim_time        = microseconds_since_epoch_of_iso_string( initial_global_state["last_pervote_bucket_fill"] );

    BOOST_REQUIRE_EQUAL(success(), fundguilds("guilds.oig"_n));

    auto     global_state      = get_global_state();
    uint64_t claim_time        = microseconds_since_epoch_of_iso_string( global_state["last_pervote_bucket_fill"] );

    asset guilds_balance = get_balance("guilds.oig"_n);
    BOOST_REQUIRE_EQUAL(guilds_balance.get_amount(), 10'00000000);

    auto funding_state = get_funding_state();
    auto last_funding_time = funding_state["lst_fund"].as<time_point>();
    BOOST_REQUIRE_EQUAL(last_funding_time > lastest_block_before_funding->timestamp.to_time_point(), true);

    auto usecs_between_fills = claim_time - initial_claim_time;
    int64_t new_tokens = (initial_supply.get_amount() * double(usecs_between_fills) * continuous_rate) / usecs_per_year;
    int64_t to_guild_bp_pay = ((new_tokens / 5)*double(claim_time - guild_bp_pay_start_time_us))/double(usecs_between_fills);

    asset saving_balance = get_balance("eosio.saving"_n);
    BOOST_REQUIRE_EQUAL(int64_t(new_tokens - (new_tokens / 5) * 3) + to_guild_bp_pay - 10'00000000, saving_balance.get_amount());

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("already funded within past day"), fundguilds("guilds.oig"_n));

    // config lower saving to test burn feature
    configguilds("eosio"_n, 123'00000000);
    produce_block();
    produce_block( fc::days(1) );

    const asset supply_before_burn  = get_token_supply();

    const auto     global_state_before_burn      = get_global_state();
    const uint64_t claim_time_before_burn        = microseconds_since_epoch_of_iso_string( initial_global_state["last_pervote_bucket_fill"] );

    // request guilds fund for next day
    BOOST_REQUIRE_EQUAL(success(), fundguilds("guilds.oig"_n));

    guilds_balance = get_balance("guilds.oig"_n);
    // guild fund now double
    BOOST_REQUIRE_EQUAL(guilds_balance.get_amount(), 20'00000000);
    const asset saving_balance_after_burn = get_balance("eosio.saving"_n);
    // all saving token exceed limit are burned
    BOOST_REQUIRE_EQUAL(saving_balance_after_burn.get_amount(), 123'00000000);

    funding_state = get_funding_state();
    BOOST_REQUIRE_EQUAL(funding_state["lst_fund"].as<time_point>().sec_since_epoch() == last_funding_time.sec_since_epoch() + 24*60*60, true);

    global_state      = get_global_state();
    claim_time        = microseconds_since_epoch_of_iso_string( global_state["last_pervote_bucket_fill"] );

    usecs_between_fills = claim_time - claim_time_before_burn;
    new_tokens = (supply_before_burn.get_amount() * double(usecs_between_fills) * continuous_rate) / usecs_per_year;

    // make sure token are burned and total supply are reduced
    const asset supply_after_burn  = get_token_supply();
    BOOST_REQUIRE_EQUAL(supply_after_burn.get_amount() < supply_before_burn.get_amount() + new_tokens, true);
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(missing_authority_to_call_configguilds, eosio_guilds_tester) try {
    BOOST_REQUIRE_EQUAL(error("missing authority of eosio"), configguilds("alice1111111"_n, 123));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(configguilds_with_max_saving_balance_less_than_zero, eosio_guilds_tester) try {
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("max_saving_balance must be greater than 0"), configguilds("eosio"_n, -1));

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("max_saving_balance must be greater than 0"), configguilds("eosio"_n, 0));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(configguilds_success, eosio_guilds_tester) try {
    BOOST_REQUIRE_EQUAL(success(), configguilds("eosio"_n, 1234599999));

    auto initial_funding_state = get_funding_state();
    BOOST_REQUIRE_EQUAL(initial_funding_state["max_saving_balance"].as<int64_t>(), 1234599999);
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
