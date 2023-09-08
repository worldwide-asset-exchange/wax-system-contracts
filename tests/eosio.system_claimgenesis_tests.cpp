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
struct _abi_hash {
   name owner;
   fc::sha256 hash;
};
FC_REFLECT( _abi_hash, (owner)(hash) );

struct connector {
   asset balance;
   double weight = .5;
};
FC_REFLECT( connector, (balance)(weight) );

using namespace eosio_system;

BOOST_AUTO_TEST_SUITE(eosio_system_claimgenesis_tests)

BOOST_FIXTURE_TEST_CASE( claim_genesis_too_late, eosio_system_tester ) try {
   deploy_system_v31_contract();
   const uint64_t nonce = 1;
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> accounts = {"user11111111"_n, "user22222222"_n };
   for (const auto& a: accounts) {
      create_account_with_resources( a, config::system_account_name, core_sym::from_string("10.0000"), false, net, cpu );
   }

   // This transer creates a sub_balance for genesis.wax account
   transfer( "eosio"_n, "genesis.wax"_n, core_sym::from_string("1000.0000"), "eosio"_n );

   const asset genesis_tokens_user1{core_sym::from_string("2.0000")};
   const asset genesis_tokens_user2{core_sym::from_string("1.0000")};

   // Lock genesis tokens to users
   awardgenesis( "user11111111"_n, genesis_tokens_user1, nonce + 1);
   awardgenesis( "user22222222"_n, genesis_tokens_user2, nonce + 2);

   // user11111111 claims full period rewards 1 year after period end
   produce_block(fc::days(3*365 + 366));
   // test claimgenesis function
   deploy_contract(false);
   BOOST_REQUIRE_EQUAL( success(), claimgenesis( "user11111111"_n ));

   BOOST_REQUIRE_EQUAL( core_sym::from_string("2.0000"), get_balance("user11111111"_n));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( claim_genesis_no_rewards_yet, eosio_system_tester ) try {
   deploy_system_v31_contract();
   const uint64_t nonce = 1;
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> accounts = {"user11111111"_n, "user22222222"_n, "user33333333"_n };
   for (const auto& a: accounts) {
      create_account_with_resources( a, config::system_account_name, core_sym::from_string("10.0000"), false, net, cpu );
   }

   // This transer creates a sub_balance for genesis.wax account
   transfer( "eosio"_n, "genesis.wax"_n, core_sym::from_string("1000.0000"), "eosio"_n );

   const asset genesis_tokens_user1{core_sym::from_string("2.0000")};
   const asset genesis_tokens_user2{core_sym::from_string("1.0000")};
   const asset genesis_tokens_user3{core_sym::from_string("73.0000")};

   // Lock genesis tokens to users
   awardgenesis( "user11111111"_n, genesis_tokens_user1, nonce + 1);
   awardgenesis( "user22222222"_n, genesis_tokens_user2, nonce + 2);
   awardgenesis( "user33333333"_n, genesis_tokens_user3, nonce + 3);

   // Wait 23h 55m hours
   produce_block(fc::seconds(23 * 3600 + 55 * 60));

   // All claim actions fail because time period elapsed since locking is less than 1 day
   deploy_contract(false);
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("already claimed rewards within past day"), claimgenesis( "user11111111"_n ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("already claimed rewards within past day"), claimgenesis( "user22222222"_n ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("already claimed rewards within past day"), claimgenesis( "user33333333"_n ) );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( claim_genesis_1_day_rewards, eosio_system_tester ) try {
   deploy_system_v31_contract();
   const uint64_t nonce = 1;
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> accounts = {"user11111111"_n, "user22222222"_n, "user33333333"_n };
   for (const auto& a: accounts) {
      create_account_with_resources( a, config::system_account_name, core_sym::from_string("10.0000"), false, net, cpu );
   }

   // This transer creates a sub_balance for genesis.wax account
   transfer( "eosio"_n, "genesis.wax"_n, core_sym::from_string("1000.0000"), "eosio"_n );

   const asset genesis_tokens_user1{core_sym::from_string("2.0000")};
   const asset genesis_tokens_user2{core_sym::from_string("1.0000")};
   const asset genesis_tokens_user3{core_sym::from_string("73.0000")};

   // Lock genesis tokens to users
   awardgenesis( "user11111111"_n, genesis_tokens_user1, nonce + 1);
   awardgenesis( "user22222222"_n, genesis_tokens_user2, nonce + 2);
   awardgenesis( "user33333333"_n, genesis_tokens_user3, nonce + 3);

   produce_block( fc::days(1) ); //Wait until July 1st

   // Wait 1 day and claim period reward tokens
   produce_block( fc::days(1) );

   // Send rewarded tokens back to users
   deploy_contract(false);
   claimgenesis( "user11111111"_n );
   claimgenesis( "user22222222"_n );
   claimgenesis( "user33333333"_n );

   // Get users' balances
   const asset user1_balance = get_balance("user11111111"_n);
   const asset user2_balance = get_balance("user22222222"_n);
   const asset user3_balance = get_balance("user33333333"_n);

   // "genesis.wax" locked 2.0000 tokens to user11111111
   // after 1 day, user11111111 claims the corresponding inflation for this one-day period.
   // So, we have:
   // initial_staked: 2.0000 TST
   // inflation period: 1 day (24*3600*1000000 microseconds) = 86400000000 us (8.64e+10)
   // 3 years lock period: 9.46944e+13 microseconds
   // period percentage of 3 years: (8.64e10) / (9.46944e+13) = (1 day / 1096 days) = 0.00091240875
   // expected received tokens for user11111111: (2.0000 TST) * (1 day / 1096 days) = 0.00182481751
   BOOST_REQUIRE_EQUAL( core_sym::from_string("0.0018"), user1_balance );

   // "genesis.wax" locked 1.0000 token to user22222222
   // after 1 day, user22222222 claims the corresponding inflation for this one-day period.
   // So, we have:
   // initial_staked: 1.0000 TST
   // inflation period: 1 day (24*3600*1000000 microseconds) = 86400000000 us (8.64e+10)
   // 3 years lock period: 9.46944e+13 microseconds
   // period percentage of 3 years: (8.64e10) / (9.46944e+13) = (1 day / 1096 days) = 0.00091240875
   // expected received tokens for user22222222: (1.0000 TST) * (1 day / 1096 days) = 0.00091240875
   BOOST_REQUIRE_EQUAL( core_sym::from_string("0.0009"), user2_balance );

   // "genesis.wax" locked 73.0000 tokens to user33333333
   // after 1 day, user33333333 claims the corresponding inflation for this one-day period.
   // So, we have:
   // initial_staked: 73.0000 TST
   // inflation period: 1 day (24*3600*1000000 microseconds) = 86400000000 us (8.64e+10)
   // 3 years lock period: 9.46944e+13 microseconds
   // period percentage of 3 years: (8.64e10) / (9.46944e+13) = (1 day / 1096 days) = 0.00091240875
   // expected received tokens for user33333333: (73.0000 TST) * (1 day / 1096 days) = 0.06660583941
   BOOST_REQUIRE_EQUAL( core_sym::from_string("0.0666"), user3_balance );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( claim_genesis_3_years_reward, eosio_system_tester ) try {
   deploy_system_v31_contract();
   const uint64_t nonce = 1;
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> accounts = {"user11111111"_n, "user22222222"_n, "user33333333"_n };
   for (const auto& a: accounts) {
      create_account_with_resources( a, config::system_account_name, core_sym::from_string("10.0000"), false, net, cpu );
   }

   // This transer creates a sub_balance for genesis.wax account
   transfer( "eosio"_n, "genesis.wax"_n, core_sym::from_string("1000.0000"), "eosio"_n );

   //const asset genesis_tokens_user1{core_sym::from_string("2.0000")};
   //const asset genesis_tokens_user2{core_sym::from_string("1.0000")};
   const asset genesis_tokens_user3{core_sym::from_string("73.0000")};

   // Lock genesis tokens to users
   //awardgenesis( "user11111111"_n, genesis_tokens_user1, nonce + 1);
   //awardgenesis( "user22222222"_n, genesis_tokens_user2, nonce + 2);
   awardgenesis( "user33333333"_n, genesis_tokens_user3, nonce + 3);

   produce_block( fc::days(1) ); //Wait until July 1st

   // Wait 3 years (1096 days) and claim whole period reward tokens
   produce_block( fc::days(1096) );

   // Send rewarded tokens back to users
   deploy_contract(false);
   //claimgenesis( "user11111111"_n );
   //claimgenesis( "user22222222"_n );
   claimgenesis( "user33333333"_n );

   // Get users' balances
   //const asset user1_balance = get_balance("user11111111"_n);
   //const asset user2_balance = get_balance("user22222222"_n);
   const asset user3_balance = get_balance("user33333333"_n);


   // "genesis.wax" locked 2.0000 tokens to user11111111
   // after 3 years, she gets back the same amount she was locked
   // So, we have:
   // initial_staked: 2.0000 TST
   // inflation period: 3 years
   // 3 years lock period: 9.46944e+13 microseconds
   // period percentage of 3 years: (9.46944e+13) / (9.46944e+13) = (1096 day / 1096 days) = 1.00000000000
   // expected received tokens for user11111111: (2.0000 TST) * (1096 days / 1096 days) = 2.0000
   //BOOST_REQUIRE_EQUAL( genesis_tokens_user1, user1_balance );


   // "genesis.wax" locked 1.0000 tokens to user22222222
   // after 3 years, she gets back the same amount she was locked
   // So, we have:
   // initial_staked: 1.0000 TST
   // inflation period: 3 years
   // 3 years lock period: 9.46944e+13 microseconds
   // period percentage of 3 years: (9.46944e+13) / (9.46944e+13) = (1096 day / 1096 days) = 1.00000000000
   // expected received tokens for user22222222: (1.0000 TST) * (1096 days / 1096 days) = 1.0000
   //BOOST_REQUIRE_EQUAL( genesis_tokens_user2, user2_balance );

   // "genesis.wax" locked 73.0000 tokens to user33333333
   // after 3 years, she gets back the same amount she was locked
   // So, we have:
   // initial_staked: 73.0000 TST
   // inflation period: 3 years
   // 3 years lock period: 9.46944e+13 microseconds
   // period percentage of 3 years: (9.46944e+13) / (9.46944e+13) = (1096 day / 1096 days) = 1.00000000000
   // expected received tokens for user33333333: (73.0000 TST) * (1096 days / 1096 days) = 73.0000
   BOOST_REQUIRE_EQUAL( genesis_tokens_user3, user3_balance);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( claim_more_than_once_a_day, eosio_system_tester ) try {
   deploy_system_v31_contract();
   const uint64_t nonce = 1;
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> accounts = {"user11111111"_n };
   for (const auto& a: accounts) {
      create_account_with_resources( a, config::system_account_name, core_sym::from_string("10.0000"), false, net, cpu );
   }

   // This transer creates a sub_balance for genesis.wax account
   transfer( "eosio"_n, "genesis.wax"_n, core_sym::from_string("1000.0000"), "eosio"_n );

   // tokens to lock for user11111111
   const asset genesis_tokens_user1{core_sym::from_string("2.0000")};

   produce_blocks( 1 );
   produce_block( fc::days(1) ); //Wait until July 1st

   // Lock genesis tokens to users
   awardgenesis( "user11111111"_n, genesis_tokens_user1, nonce + 1 );

   // claiming tokens once an hour within 1 day since lock-up
   for(auto i=1; i <= 23 ; ++i) {
     produce_block( fc::hours(1) );
     BOOST_REQUIRE_EQUAL( wasm_assert_msg("already claimed rewards within past day"), claimgenesis( "user11111111"_n ) );
   }

   // complete wait-period of 1 day and claim rewards
   produce_block( fc::hours(1) );
   deploy_contract(false);
   claimgenesis( "user11111111"_n );

   const asset user1_balance = get_balance("user11111111"_n);
   BOOST_REQUIRE_EQUAL( core_sym::from_string("0.0018"), user1_balance );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( claim_half_period_twice, eosio_system_tester ) try {
   deploy_system_v31_contract();
   const uint64_t nonce = 1;
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> accounts = { "user11111111"_n };
   for (const auto& a: accounts) {
      create_account_with_resources( a, config::system_account_name, core_sym::from_string("10.0000"), false, net, cpu );
   }

   // This transer creates a sub_balance for genesis.wax account
   transfer( "eosio"_n, "genesis.wax"_n, core_sym::from_string("1000.0000"), "eosio"_n );

   // tokens to lock for user11111111
   const asset genesis_tokens_user1{core_sym::from_string("2.0000")};
   asset user1_balance;

   // Lock genesis tokens to user11111111
   awardgenesis( "user11111111"_n, genesis_tokens_user1, nonce + 1 );
   produce_block( fc::hours(24) );

   // Wait initial 1.5 years (1096/2 days) and claim first half period reward tokens
   produce_block( fc::days(548) );
   claimgenesis( "user11111111"_n );
   user1_balance = get_balance("user11111111"_n);
   BOOST_REQUIRE_EQUAL( core_sym::from_string("1.0000"), user1_balance );

   // Wait last 1.5 years (1096/2 days) and claim second half period reward tokens
   produce_block( fc::days(548) );
   deploy_contract(false);
   claimgenesis( "user11111111"_n );
   user1_balance = get_balance("user11111111"_n);
   // Adding one_day_genesis_rewards because the last day cannot be claimed due the award was after the initial time
   BOOST_REQUIRE( user1_balance <= core_sym::from_string("2.0000"));
   BOOST_REQUIRE( user1_balance >= core_sym::from_string("1.9999"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( claim_once_a_day_during_until_final_day, eosio_system_tester ) try {
   deploy_system_v31_contract();
   const uint64_t nonce = 1;
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> accounts = { "user11111111"_n };
   for (const auto& a: accounts) {
      create_account_with_resources( a, config::system_account_name, core_sym::from_string("10.0000"), false, net, cpu );
   }

   // This transer creates a sub_balance for genesis.wax account
   transfer( "eosio"_n, "genesis.wax"_n, core_sym::from_string("1000.0000"), "eosio"_n );

   // tokens to lock for user11111111912
   const asset genesis_tokens_user1{core_sym::from_string("2.0000")};

   // Lock genesis tokens to user11111111
   awardgenesis( "user11111111"_n, genesis_tokens_user1, nonce + 1 );

   produce_block( fc::days(1) ); //Wait until July 1st

   auto genesis_info = get_genesis("user11111111"_n);

   // Check acc is incrementally equal
   asset acc = core_sym::from_string("0.0000");
   for(auto i=1; i <= 1096; ++i) { //The test chain starts on July 1st 2019
     produce_block( fc::days(1) );
     acc += core_sym::from_string("0.0018");
     BOOST_REQUIRE_EQUAL(success(), claimgenesis( "user11111111"_n ));
     BOOST_REQUIRE_EQUAL( acc, get_balance("user11111111"_n) );
   }

   // Check that we cannot claim any more after July 1st 2022
   produce_block( fc::days(1) );
   deploy_contract(false);
   claimgenesis( "user11111111"_n );
   BOOST_REQUIRE_EQUAL( acc, get_balance("user11111111"_n) );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( unstake_all_genesis_balance_after_1_day, eosio_system_tester ) try {
   deploy_system_v31_contract();
   cross_15_percent_threshold();
   const uint64_t nonce = 1;
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> accounts = { "user11111111"_n };
   for (const auto& a: accounts) {
      create_account_with_resources( a, config::system_account_name, core_sym::from_string("10.0000"), false, net, cpu );
   }

   // This transer creates a sub_balance for genesis.wax account
   transfer( "eosio"_n, "genesis.wax"_n, core_sym::from_string("1000.0000"), "eosio"_n );

   // tokens to lock for user11111111
   const asset genesis_tokens_user1{core_sym::from_string("2.0000")};

   // rewards for 1 day locked genesis, this is: 2*(1/1096)
   const asset rewards_1_day_user1{core_sym::from_string("0.0018")};

   // Lock genesis tokens to user11111111
   awardgenesis( "user11111111"_n, genesis_tokens_user1, nonce + 1 );

   produce_block( fc::hours(24) ); // Wait until July 1st

   // unstake locked amount one day after locking
   produce_block( fc::hours(24) );
   BOOST_REQUIRE_EQUAL( success(), unstake( "user11111111"_n, "user11111111"_n, core_sym::from_string("1.0000"), core_sym::from_string("1.0000") ) );
   auto genesis_info = get_genesis("user11111111"_n);
   BOOST_REQUIRE_EQUAL( rewards_1_day_user1, genesis_info["unclaimed_balance"].as<asset>() );

   // after 2 days and 23 hours since unstaking genesis balance should be zero
   produce_block( fc::hours(3*24 - 1) );
   BOOST_REQUIRE_EQUAL( 0, get_balance("user11111111"_n).get_amount() );

   // completing 3 days wait, staked funds should be paid back
   produce_block( fc::hours(1) );
   BOOST_REQUIRE_EQUAL( genesis_tokens_user1 , get_balance( "user11111111"_n ) );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( unstake_without_decreasing_genesis_balance, eosio_system_tester ) try {
   deploy_system_v31_contract();
   cross_15_percent_threshold();
   const uint64_t nonce = 1;
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> accounts = { "user11111111"_n };
   for (const auto& a: accounts) {
      create_account_with_resources( a, config::system_account_name, core_sym::from_string("10.0000"), false, net, cpu );
   }

   // This transer creates a sub_balance for genesis.wax account
   transfer( "eosio"_n, "genesis.wax"_n, core_sym::from_string("1000.0000"), "eosio"_n );

   // tokens to lock for user11111111
   const asset genesis_tokens_user1{core_sym::from_string("2.0000")};

   // rewards for 1 day locked genesis, this is: 2*(1/1096)
   const asset rewards_1_day_user1{core_sym::from_string("0.0018")};

   // Lock genesis tokens to user11111111
   awardgenesis( "user11111111"_n, genesis_tokens_user1, nonce + 1 );

   // tokens to stake besides genesis balance
   const asset net_tokens_user1{core_sym::from_string("10.0000")};
   const asset cpu_tokens_user1{core_sym::from_string("10.0000")};

   // Stake-transer non-genesis tokens to user11111111
   stake_with_transfer( "eosio"_n, "user11111111"_n, net_tokens_user1, cpu_tokens_user1 );

   // Wait 1 day and unstake non-genesis tokens
   produce_block( fc::hours(24) );
   BOOST_REQUIRE_EQUAL( success(), unstake( "user11111111"_n, "user11111111"_n, net_tokens_user1, cpu_tokens_user1 ) );

   // after 3 days all non-genesis tokens should be paid back (we're asking them back)
   produce_block( fc::hours(3*24) );
   BOOST_REQUIRE_EQUAL( net_tokens_user1
		        + cpu_tokens_user1,
		        get_balance( "user11111111"_n ) );

   // remaining staked tokens should be equal to genesis balance,
   // since we're unstaking all but the locked ones tokens
   BOOST_REQUIRE_EQUAL( genesis_tokens_user1, get_genesis_balance( "user11111111"_n ) );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( unstake_decreasing_genesis_balance, eosio_system_tester ) try {
   deploy_system_v31_contract();
   cross_15_percent_threshold();
   const uint64_t nonce = 1;
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> accounts = { "user11111111"_n };
   for (const auto& a: accounts) {
      create_account_with_resources( a, config::system_account_name, core_sym::from_string("10.0000"), false, net, cpu );
   }

   // This transer creates a sub_balance for genesis.wax account
   transfer( "eosio"_n, "genesis.wax"_n, core_sym::from_string("1000.0000"), "eosio"_n );

   // tokens to lock for user11111111
   const asset locked_4_net = {core_sym::from_string("1.0000")};
   const asset locked_4_cpu = {core_sym::from_string("1.0000")};
   const asset genesis_tokens_user1 = locked_4_net + locked_4_cpu;

   // rewards for 1 day locked genesis, this is: 2*(1/1096)
   const asset rewards_1_day_user1{core_sym::from_string("0.0018")};

   // Lock genesis tokens to user11111111
   awardgenesis( "user11111111"_n, genesis_tokens_user1, nonce + 1 );

   produce_block( fc::hours(24) ); //Wait until July 1st

   // tokens to stake besides genesis balance
   const asset net_tokens_user1{core_sym::from_string("10.0000")};
   const asset cpu_tokens_user1{core_sym::from_string("10.0000")};

   // Stake-transer non-genesis tokens to user11111111
   stake_with_transfer( "eosio"_n, "user11111111"_n, net_tokens_user1, cpu_tokens_user1 );

   // Check owned tokens match genesis + transfered-staked for NET and CPU
   auto delegated_bw_user1 = get_delegated_bw( "user11111111"_n );
   BOOST_REQUIRE_EQUAL( locked_4_net + net_tokens_user1, delegated_bw_user1["net_weight"].as<asset>());
   BOOST_REQUIRE_EQUAL( locked_4_cpu + cpu_tokens_user1, delegated_bw_user1["cpu_weight"].as<asset>());

   // At this point we have:
   // Genesis Balance user11111111: 2.0000,
   // Staked NET and CPU without TRANSFER: 80.0000 + 80.0000 = 160.0000
   // Staked NET and CPU with TRANSFER: 10.0000 + 10.0000 = 20.0000
   // Delegated Balance (row for user11111111 in delband table): 22.0000

   // Unstake non-genesis tokens, after one day since locking
   // Here we unstake all Genesis-Staked + Transfered-Staked:
   // NET_STAKED_TRANSFER + CPU_STAKED_TRANSFER + NET_FERTILE + CPU_FERTILE
   //       10.0000       +       10.0000       +    1.0000   +    1.0000
   // TOTAL BEING UNSTAKING: 22.0000
   produce_block( fc::hours(24) );
   BOOST_REQUIRE_EQUAL( success(),
		        unstake( "user11111111"_n, "user11111111"_n,
		          net_tokens_user1 + locked_4_net,
			  cpu_tokens_user1 + locked_4_cpu
		       ));

   // after 3 days all OWNED tokens (staked and genesis-locked) should be paid back
   produce_block( fc::hours(3*24) );
   BOOST_REQUIRE_EQUAL( genesis_tokens_user1    // <- Genesis Tokens
		        + net_tokens_user1     // <- Non Genesis Tokens (NET)
			+ cpu_tokens_user1,     // <- Non Genesis Tokens (CPU)
			get_balance( "user11111111"_n ) );

   // remaining genesis balance should be 0,
   // since we unstaked all OWNED staked tokens and genesis-locked
   BOOST_REQUIRE_EQUAL( core_sym::from_string("0.0000"), get_genesis_balance( "user11111111"_n ) );

   // The rewards for the day the user kept the genesis tokens should be available to be claimed
   auto genesis_info = get_genesis("user11111111"_n);
   BOOST_REQUIRE_EQUAL( rewards_1_day_user1, genesis_info["unclaimed_balance"].as<asset>() );

   deploy_contract(false);
   BOOST_REQUIRE_EQUAL( success(), claimgenesis( "user11111111"_n ));
   BOOST_REQUIRE_EQUAL( genesis_tokens_user1    // <- Genesis Tokens
            + net_tokens_user1     // <- Non Genesis Tokens (NET)
            + cpu_tokens_user1     // <- Non Genesis Tokens (CPU)
            + rewards_1_day_user1,  // <- Genesis rewards
            get_balance( "user11111111"_n ) );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( genesis_then_transfer_finally_claim, eosio_system_tester ) try {
   deploy_system_v31_contract();
   cross_15_percent_threshold();
   const uint64_t nonce = 1;
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> accounts = { "user11111111"_n, "user22222222"_n };
   const asset last_day_rewards = core_sym::from_string("0.0019");;
   for (const auto& a: accounts) {
      create_account_with_resources( a, config::system_account_name, core_sym::from_string("10.0000"), false, net, cpu );
   }

   // This transer creates a sub_balance for genesis.wax account
   transfer( "eosio"_n, "genesis.wax"_n, core_sym::from_string("1000.0000"), "eosio"_n );

   // tokens to lock for user11111111
   const asset locked_4_net = {core_sym::from_string("1.0000")};
   const asset locked_4_cpu = {core_sym::from_string("1.0000")};
   const asset genesis_tokens_user1  = locked_4_net + locked_4_cpu;
   const asset liquid_tokens_user1   = {core_sym::from_string("10.0000")};
   const asset tokens_user1_to_user2 = {core_sym::from_string("5.0000")};


   // Transfer which puts liquid tokens into user11111111 account
   transfer( "eosio"_n, "user11111111"_n, liquid_tokens_user1, "eosio"_n );

   // Lock genesis tokens to user11111111
   awardgenesis( "user11111111"_n, genesis_tokens_user1, nonce + 1 );

   // Then, send some liquid tokens from user11111111 to user22222222
   transfer( "user11111111"_n, "user22222222"_n, tokens_user1_to_user2, "user11111111"_n );

   // Before claiming, check user11111111 genesis_balance keeps the same as awarded
   BOOST_REQUIRE_EQUAL( genesis_tokens_user1, get_genesis_balance( "user11111111"_n ) );
   produce_blocks(1);

   // After 3 years, user11111111 claims full period rewards
   // So, check liquid balance equals:
   // REWARDS_FULL_PERIOD + REMAINING_LIQUID_TOKENS
   produce_block(fc::days(2*365 + 366));
   deploy_contract(false);
   claimgenesis( "user11111111"_n );
   BOOST_REQUIRE_EQUAL( liquid_tokens_user1     // Non-Genesis Tokens
         - tokens_user1_to_user2 // Transfered u1 -> u2
			+ genesis_tokens_user1 // Rewards full period
         - last_day_rewards,  // Given the award was done some seconds after July 1st 2019 00:00:00 user11111111 losts 1 day worth of rewards
         get_balance("user11111111"_n));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( genesis_plus_delegate_extra_bw_to_self, eosio_system_tester ) try {
   deploy_system_v31_contract();
   cross_15_percent_threshold();
   const uint64_t nonce = 1;
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> accounts = { "user11111111"_n, "user22222222"_n };
   for (const auto& a: accounts) {
      create_account_with_resources( a, config::system_account_name, core_sym::from_string("10.0000"), false, net, cpu );
   }

   // This transer creates a sub_balance for genesis.wax account
   transfer( "eosio"_n, "genesis.wax"_n, core_sym::from_string("1000.0000"), "eosio"_n );

   // Placeholder for 0 tokens
   const asset zero_asset = {core_sym::from_string("0.0000")};

   // tokens to GENESIS-LOCK for user11111111
   const asset locked_4_net = {core_sym::from_string("2.7400")};
   const asset locked_4_cpu = {core_sym::from_string("2.7400")};
   const asset genesis_tokens_user1 = locked_4_net + locked_4_cpu;
   const asset one_day_genesis_rewards = {core_sym::from_string("0.0050")};

   // liquid tokens to (be transfered to) user11111111
   const asset net_to_stake_user1 = {core_sym::from_string("5.0000")};
   const asset cpu_to_stake_user1 = {core_sym::from_string("5.0000")};
   const asset liquid_tokens_user1 = net_to_stake_user1 + cpu_to_stake_user1;

   // check user1's liquid tokens are zero before receiving any tokens
   BOOST_REQUIRE_EQUAL(zero_asset, get_balance("user11111111"_n) );

   // Transfer which puts liquid tokens into user11111111 account
   transfer( "eosio"_n, "user11111111"_n, liquid_tokens_user1, "eosio"_n );
   BOOST_REQUIRE_EQUAL(liquid_tokens_user1, get_balance("user11111111"_n) );

   // user1 stakes all her liquid tokens (delegates extra bw to herself)
   BOOST_REQUIRE_EQUAL( success(), stake( "user11111111"_n, "user11111111"_n, net_to_stake_user1, cpu_to_stake_user1 ));

   // in Between liquid tokens stake/unstake user1 locks genesis tokens
   awardgenesis( "user11111111"_n, genesis_tokens_user1, nonce + 1 );

   produce_block( fc::days(1) ); //Wait until July 1st

   // check liquid tokens are zero after staking them all
   BOOST_REQUIRE_EQUAL(zero_asset, get_balance("user11111111"_n) );

   // check total staked is now initially delegated-to-without-transfer
   // plus all liquid tokens which were staked
   // plus awardgenesis'ed tokens
   auto total = get_total_stake( "user11111111"_n );
   BOOST_REQUIRE_EQUAL( net + locked_4_net + net_to_stake_user1, total["net_weight"].as<asset>());
   BOOST_REQUIRE_EQUAL( cpu + locked_4_cpu + cpu_to_stake_user1, total["cpu_weight"].as<asset>());

   // unstake all previously staked tokens
   // must wait 1 day since awardgenesis call
   // otherwise claimgenesis inside undelegatebw fails
   produce_block( fc::days(1) );
   BOOST_REQUIRE_EQUAL( success(), unstake( "user11111111"_n, "user11111111"_n, net_to_stake_user1, cpu_to_stake_user1 ));
   BOOST_REQUIRE_EQUAL( success(), claimgenesis( "user11111111"_n ));

   // rewards for 1 day period should be liquid
   BOOST_REQUIRE_EQUAL( one_day_genesis_rewards, get_balance("user11111111"_n) );

   // after 3 days refund takes place
   // so, user1's balance is only liquid tokens
   produce_block( fc::days(3) );
   BOOST_REQUIRE_EQUAL(  one_day_genesis_rewards
		       + net_to_stake_user1
		       + cpu_to_stake_user1, get_balance("user11111111"_n) );

   // while awardgenesis'ed remain staked because
   // amount being unstaked is <= staked_liquid_tokens
   // this means, user1 genesis balance stays untouched
   BOOST_REQUIRE_EQUAL( genesis_tokens_user1, get_genesis_balance( "user11111111"_n ) );

   // Complete full period for claiming rewards
   produce_block( fc::days(2*365 + 366) );
   deploy_contract(false);
   claimgenesis( "user11111111"_n );

   // so, user1's balance is now full period rewards
   // plus already unstaked liquid tokens
   BOOST_REQUIRE(  genesis_tokens_user1 // -> full period equals initial genesis balance
		       + net_to_stake_user1
		       + cpu_to_stake_user1
             >= get_balance("user11111111"_n));
   BOOST_REQUIRE(genesis_tokens_user1 // -> full period equals initial genesis balance
		       + net_to_stake_user1
		       + cpu_to_stake_user1
             <= get_balance("user11111111"_n) + core_sym::from_string("0.0001"));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( genesis_plus_delegate_extra_bw_to_self_no_unstake, eosio_system_tester ) try {
   deploy_system_v31_contract();
   cross_15_percent_threshold();
   const uint64_t nonce = 1;
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> accounts = { "user11111111"_n, "user22222222"_n };
   for (const auto& a: accounts) {
      create_account_with_resources( a, config::system_account_name, core_sym::from_string("10.0000"), false, net, cpu );
   }

   // This transer creates a sub_balance for genesis.wax account
   transfer( "eosio"_n, "genesis.wax"_n, core_sym::from_string("1000.0000"), "eosio"_n );

   // Placeholder for 0 tokens
   const asset zero_asset = {core_sym::from_string("0.0000")};

   // tokens to GENESIS-LOCK for user11111111
   const asset locked_4_net = {core_sym::from_string("2.7400")};
   const asset locked_4_cpu = {core_sym::from_string("2.7400")};
   const asset genesis_tokens_user1 = locked_4_net + locked_4_cpu;
   const asset one_day_genesis_rewards = {core_sym::from_string("0.0050")};

   // liquid tokens to (be transfered to) user11111111
   const asset net_to_stake_user1 = {core_sym::from_string("5.0000")};
   const asset cpu_to_stake_user1 = {core_sym::from_string("5.0000")};
   const asset liquid_tokens_user1 = net_to_stake_user1 + cpu_to_stake_user1;

   // check user1's liquid tokens are zero before receiving any tokens
   BOOST_REQUIRE_EQUAL(zero_asset, get_balance("user11111111"_n) );

   // Transfer which puts liquid tokens into user11111111 account
   transfer( "eosio"_n, "user11111111"_n, liquid_tokens_user1, "eosio"_n );
   BOOST_REQUIRE_EQUAL(liquid_tokens_user1, get_balance("user11111111"_n) );

   // user1 stakes all her liquid tokens (delegates extra bw to herself)
   BOOST_REQUIRE_EQUAL( success(), stake( "user11111111"_n, "user11111111"_n, net_to_stake_user1, cpu_to_stake_user1 ));

   // in Between liquid tokens stake/unstake user1 locks genesis tokens
   awardgenesis( "user11111111"_n, genesis_tokens_user1, nonce + 1 );

   produce_block( fc::days(1) ); //Wait until July 1st

   // check liquid tokens are zero after staking them all
   BOOST_REQUIRE_EQUAL(zero_asset, get_balance("user11111111"_n) );

   // check total staked is now initially delegated-to-without-transfer
   // plus all liquid tokens which were staked
   // plus awardgenesis'ed tokens
   auto total = get_total_stake( "user11111111"_n );
   BOOST_REQUIRE_EQUAL( net + locked_4_net + net_to_stake_user1, total["net_weight"].as<asset>());
   BOOST_REQUIRE_EQUAL( cpu + locked_4_cpu + cpu_to_stake_user1, total["cpu_weight"].as<asset>());

   // unstake all previously staked tokens
   // must wait 1 day since awardgenesis call
   // otherwise claimgenesis inside undelegatebw fails
   produce_block( fc::days(1) );
   BOOST_REQUIRE_EQUAL( success(), claimgenesis( "user11111111"_n ) );

   // rewards for 1 day period should be liquid
   BOOST_REQUIRE_EQUAL( one_day_genesis_rewards, get_balance("user11111111"_n) );

   // user1 genesis balance stays untouched
   BOOST_REQUIRE_EQUAL( genesis_tokens_user1, get_genesis_balance( "user11111111"_n ) );

   // Complete full period for claiming rewards
   produce_block( fc::days(2*365 + 366) );
   deploy_contract(false);
   claimgenesis( "user11111111"_n );

   // so, user1's balance is now full period rewards
   // plus already unstaked liquid tokens
   BOOST_REQUIRE(  genesis_tokens_user1 // -> full period equals initial genesis balance
		      >= get_balance("user11111111"_n) );
   BOOST_REQUIRE(genesis_tokens_user1 // -> full period equals initial genesis balance
		      <= core_sym::from_string("0.0001") + get_balance("user11111111"_n) );

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( genesis_plus_delegate_extra_bw_to_someone_else, eosio_system_tester ) try {
   deploy_system_v31_contract();
   cross_15_percent_threshold();
   const uint64_t nonce = 1;
   const auto three_years = 2*365 + 366;
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> accounts = { "user11111111"_n, "user22222222"_n };
   for (const auto& a: accounts) {
      create_account_with_resources( a, config::system_account_name, core_sym::from_string("10.0000"), false, net, cpu );
   }

   // This transer creates a sub_balance for genesis.wax account
   transfer( "eosio"_n, "genesis.wax"_n, core_sym::from_string("1000.0000"), "eosio"_n );

   // Placeholder for 0 tokens
   const asset zero_asset = {core_sym::from_string("0.0000")};

   // tokens to GENESIS-LOCK for user11111111
   const asset locked_4_net = {core_sym::from_string("2.7400")};
   const asset locked_4_cpu = {core_sym::from_string("2.7400")};
   const asset half_locked_4_net = {core_sym::from_string("1.3700")};
   const asset half_locked_4_cpu = {core_sym::from_string("1.3700")};
   const asset genesis_tokens_user1 = locked_4_net + locked_4_cpu;
   const asset one_day_genesis_rewards = {core_sym::from_string("0.0025")};

   // liquid tokens to (be transfered to) user11111111
   const asset net_to_stake_user2 = {core_sym::from_string("5.0000")};
   const asset cpu_to_stake_user2 = {core_sym::from_string("5.0000")};
   const asset liquid_tokens_user1 = net_to_stake_user2 + cpu_to_stake_user2;

   // check user1's liquid tokens are zero before receiving any tokens
   BOOST_REQUIRE_EQUAL(zero_asset, get_balance("user11111111"_n) );

   // Transfer which puts liquid tokens into user11111111 account
   transfer( "eosio"_n, "user11111111"_n, liquid_tokens_user1, "eosio"_n );
   BOOST_REQUIRE_EQUAL(liquid_tokens_user1, get_balance("user11111111"_n) );

   // user1 stakes all her liquid tokens to user2 (delegates extra bw to her friend)
   BOOST_REQUIRE_EQUAL( success(), stake( "user11111111"_n, "user22222222"_n, net_to_stake_user2, cpu_to_stake_user2 ));

   // in Between liquid tokens stake/unstake user1 locks genesis tokens
   awardgenesis( "user11111111"_n, genesis_tokens_user1, nonce + 1 );

   // check liquid tokens are zero after staking them all
   BOOST_REQUIRE_EQUAL(zero_asset, get_balance("user11111111"_n) );

   produce_block(fc::days(1)); //Wait until July 1st

   // check total staked is now initially delegated-to-without-transfer
   // plus all liquid tokens which were staked
   // plus awardgenesis'ed tokens
   auto total = get_total_stake( "user11111111"_n );
   BOOST_REQUIRE_EQUAL( net + locked_4_net, total["net_weight"].as<asset>());
   BOOST_REQUIRE_EQUAL( cpu + locked_4_cpu, total["cpu_weight"].as<asset>());

   // unstake half previously staked tokens after half the award interval (1.5 years)
   produce_block( fc::days(three_years / 2) );
   BOOST_REQUIRE_EQUAL( success(), unstake( "user11111111"_n, "user11111111"_n, half_locked_4_net, half_locked_4_cpu ));
   BOOST_REQUIRE_EQUAL( success(), claimgenesis( "user11111111"_n ) );

   // rewards for 1.5 year period should be liquid
   BOOST_REQUIRE_EQUAL( half_locked_4_net + half_locked_4_cpu, get_balance("user11111111"_n) );

   // after 3 days refund takes place
   // so, user1's balance is only liquid tokens
   produce_block( fc::days(3) );
   BOOST_REQUIRE_EQUAL( locked_4_net + locked_4_cpu, get_balance("user11111111"_n) );

   // while remaining half of awardgenesis'ed remain staked because
   BOOST_REQUIRE_EQUAL( half_locked_4_net + half_locked_4_cpu, get_genesis_balance( "user11111111"_n ) );

   // Complete full period for claiming rewards
   produce_block( fc::days(three_years) );
   deploy_contract(false);
   claimgenesis( "user11111111"_n );

   // so, user1's balance is now full period rewards
   // plus already unstaked liquid tokens
   BOOST_REQUIRE( locked_4_net + locked_4_cpu + half_locked_4_net >= get_balance("user11111111"_n));
   BOOST_REQUIRE( locked_4_net + locked_4_cpu + half_locked_4_net - core_sym::from_string("0.0001") <= get_balance("user11111111"_n));

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
