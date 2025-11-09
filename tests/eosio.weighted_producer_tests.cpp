#include <boost/test/unit_test.hpp>
#include <cstdlib>
#include <eosio/chain/contract_table_objects.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/global_property_object.hpp>
#include <eosio/chain/resource_limits.hpp>
#include <eosio/chain/wast_to_wasm.hpp>
#include <eosio/chain/fixed_bytes.hpp>
#include <fc/log/logger.hpp>
#include <iostream>
#include <sstream>

#include "eosio.system_tester.hpp"

inline const auto alice = "alice1111111"_n;
inline const auto bob = "bob111111111"_n;
inline const auto GUILDS_OIG = "guilds.oig"_n;

struct guild {
    name producer;
    uint32_t score;
    uint32_t prv_score;
    asset balance;
    asset eligibility;
    bool autopay;
    bool retired;
};

FC_REFLECT(guild, (producer)(score)(prv_score)(balance)(eligibility)(autopay)(retired))

struct standby_producer_state {
  name            owner;
  u_int64_t       standby_share = 0;
  time_point      last_standby_share_update;
  time_point      last_claim_time;
  bool            is_active = true;
};

FC_REFLECT(standby_producer_state, (owner)(standby_share)(last_standby_share_update)(last_claim_time)(is_active))

using namespace eosio_system;


struct eosio_weighted_producer_tester : eosio_system_tester {
   abi_serializer guilds_abi_ser;

   eosio_weighted_producer_tester() { 
      const asset net = core_sym::from_string("800.0000");
      const asset cpu = core_sym::from_string("800.0000");
      const std::vector<account_name> accounts = { GUILDS_OIG,  };
      for (const auto& v: accounts) {
         create_account_with_resources( v, config::system_account_name, core_sym::from_string("100.0000"), false, net, cpu );
         transfer( config::system_account_name, v, core_sym::from_string("100000000.0000"), config::system_account_name );
         BOOST_REQUIRE_EQUAL(success(), stake(v, core_sym::from_string("30000000.0000"), core_sym::from_string("30000000.0000")) );
      }

      produce_blocks( 2 );

      // fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);


      set_code( GUILDS_OIG, contracts::util::guild_test_wasm() );
      set_abi( GUILDS_OIG, contracts::util::guild_test_abi().data() );

      {
         const auto& accnt = control->db().get<account_object,by_name>( GUILDS_OIG );
         abi_def abi;
         BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
         guilds_abi_ser.set_abi(abi, abi_serializer::create_yield_function(abi_serializer_max_time));
    }

   }

  fc::variant get_global_state4() {
    vector<char> data = get_row_by_account( config::system_account_name, config::system_account_name, "global4"_n, "global4"_n );
    return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "eosio_global_state4", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
  }

  fc::variant get_global_state6() {
    vector<char> data = get_row_by_account( config::system_account_name, config::system_account_name, "global.a"_n, "global.a"_n );
    return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "eosio_global_state6", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
  }

  // read guild data from guilds.oig contract
  guild get_guild_table(name producer) {
    vector<char> data = get_row_by_account( GUILDS_OIG, GUILDS_OIG, "guild"_n, producer );
    return fc::raw::unpack<guild>(data);
  }

  // push action to guilds contract
  action_result push_guild_action( const account_name& signer, const action_name &name, const variant_object &data, bool auth = true ) {
      string action_type_name = guilds_abi_ser.get_action_type(name);

      action act;
      act.account = GUILDS_OIG;
      act.name = name;
      act.data = guilds_abi_ser.variant_to_binary( action_type_name, data, abi_serializer::create_yield_function(abi_serializer_max_time) );

      return base_tester::push_action( std::move(act), (auth ? signer : signer == "bob111111111"_n ? "alice1111111"_n : "bob111111111"_n).to_uint64_t() );
  }

  // read standby producer state
  standby_producer_state get_standby_producer_state(name acc) {
    vector<char> data = get_row_by_account(config::system_account_name, config::system_account_name, "standbys"_n, acc);
    return fc::raw::unpack<standby_producer_state>(data);
  }

  // get all standby producers
  std::vector<standby_producer_state> get_standby_table() {
    std::vector<standby_producer_state> result;

    const auto* table_id_itr = control->db().find<eosio::chain::table_id_object, eosio::chain::by_code_scope_table>(
      boost::make_tuple(eosio::chain::config::system_account_name, eosio::chain::config::system_account_name, "standbys"_n));

    if (!table_id_itr) {
      return result;
    }

    const auto& idx = control->db().get_index<eosio::chain::key_value_index, eosio::chain::by_scope_primary>();
    auto table_id = table_id_itr->id;

    standby_producer_state r;

    auto lower = idx.lower_bound(boost::make_tuple(table_id, 0));
    for (auto itr = lower; itr->t_id == table_id && itr != idx.end(); ++itr){
         fc::datastream<const char*> ds(itr->value.data(), itr->value.size());
         fc::raw::unpack(ds, r);
         result.push_back(r);
    }
    return result;
  }

  vector<name> active_and_vote_producers() {
    //stake more than 15% of total EOS supply to activate chain
    const asset net = core_sym::from_string("80.0000");
    const asset cpu = core_sym::from_string("80.0000");
    const std::vector<account_name> voters = { "producvotera"_n, "producvoterb"_n, "producvoterc"_n, "producvoterd"_n };
    for (const auto& v: voters) {
       create_account_with_resources( v, config::system_account_name, core_sym::from_string("1.0000"), false, net, cpu );
       transfer( config::system_account_name, v, core_sym::from_string("100000000.0000"), config::system_account_name );
       BOOST_REQUIRE_EQUAL(success(), stake(v, core_sym::from_string("30000000.0000"), core_sym::from_string("30000000.0000")) );
    }

    // create accounts {defproducera, defproducerb, ..., defproducerz} and register as producers
    std::vector<account_name> producer_names;
    {
       producer_names.reserve('z' - 'a' + 1);
       const std::string root("defproducer");
       for ( char c = 'a'; c <= 'z'; ++c ) {
          producer_names.emplace_back(root + std::string(1, c));
       }
       setup_producer_accounts(producer_names);
       for (const auto& p: producer_names) {
          BOOST_REQUIRE_EQUAL( success(), regproducer(p) );
          produce_blocks(1);
          ilog( "------ registered producer ----------" );
          wdump((p));
          BOOST_TEST(0 == get_producer_info(p)["total_votes"].as<double>());
       }
    }

    produce_block( fc::hours(24) );

    {
       BOOST_REQUIRE_EQUAL(success(), vote("producvotera"_n, vector<account_name>(producer_names.begin(), producer_names.begin()+10)));
       BOOST_REQUIRE_EQUAL(success(), vote("producvoterb"_n, vector<account_name>(producer_names.begin(), producer_names.begin()+21)));
       BOOST_REQUIRE_EQUAL(success(), vote("producvoterc"_n, vector<account_name>(producer_names.begin(), producer_names.begin()+26)));
       BOOST_REQUIRE_EQUAL(success(), vote("producvoterd"_n, vector<account_name>(producer_names.begin()+26, producer_names.end())));
    }
    produce_blocks(23 * 12 + 20);

    auto producer_keys = control->head_block_state()->active_schedule.producers;
    BOOST_REQUIRE_EQUAL( 21, producer_keys.size() );
    BOOST_REQUIRE_EQUAL( name("defproducera"), producer_keys[0].producer_name );

    return producer_names;
  }

};


BOOST_AUTO_TEST_SUITE(eosio_weighted_producer_tests)

BOOST_FIXTURE_TEST_CASE(test_config_set_and_get, eosio_weighted_producer_tester) try {
   // Test setting and getting weighted producer config values

   // Get initial global state 5
   fc::variant initial_state = get_global_state6();

   // Test 1: Set guilds contract name
   const name new_guilds_contract = GUILDS_OIG;
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setguildcont"_n, mvo()
      ("contract", new_guilds_contract)
   ));
   produce_blocks(1);

   // Verify guilds contract was set
   fc::variant state_after_guild = get_global_state6();
   BOOST_REQUIRE_EQUAL(state_after_guild["guilds_contract"].as<name>(), new_guilds_contract);

   // Test 2: Set BP score scaling factor
   const uint32_t new_scaling_factor = 2000; // 2.0x multiplier
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpscale"_n, mvo()
      ("scaling_factor", new_scaling_factor)
   ));
   produce_blocks(1);

   // Verify scaling factor was set
   fc::variant state_after_scale = get_global_state6();
   BOOST_REQUIRE_EQUAL(state_after_scale["bp_score_scaling_factor"].as<uint32_t>(), new_scaling_factor);

   // Test 3: Set default BP score
   const uint32_t new_default_score = 1500; // 1.5x default
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpdefscore"_n, mvo()
      ("default_score", new_default_score)
   ));
   produce_blocks(1);

   // Test 4: Set min considered producers
   const uint32_t max_considered_producers = 150;
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setmaxprod"_n, mvo()
      ("max_considered_producers", max_considered_producers)
   ));
   produce_blocks(1);

   // Test 5: Set min producers vote threshold
   const double min_producer_vote_threshold = 155.678;
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setminvote"_n, mvo()
      ("min_producer_vote_threshold", min_producer_vote_threshold)
   ));
   produce_blocks(1);

   // Verify default score was set
   fc::variant final_state = get_global_state6();
   BOOST_REQUIRE_EQUAL(final_state["bp_default_score"].as<uint32_t>(), new_default_score);

   // Verify max_considered_producers was set
   BOOST_REQUIRE_EQUAL(final_state["max_considered_producers"].as<uint32_t>(), max_considered_producers);

   // Verify min_producer_vote_threshold was set
   BOOST_REQUIRE_EQUAL(final_state["min_producer_vote_threshold"].as<double>(), min_producer_vote_threshold);

   // Verify all settings persist together
   BOOST_REQUIRE_EQUAL(final_state["guilds_contract"].as<name>(), new_guilds_contract);
   BOOST_REQUIRE_EQUAL(final_state["bp_score_scaling_factor"].as<uint32_t>(), new_scaling_factor);
   BOOST_REQUIRE_EQUAL(final_state["bp_default_score"].as<uint32_t>(), new_default_score);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_config_set_missing_self_authority, eosio_weighted_producer_tester) try {
   fc::variant before_state = get_global_state6();

   create_account_with_resources("fakeeosio"_n, config::system_account_name, core_sym::from_string("1.0000"), false);

   // Test 1: Set guilds contract name
   const name new_guilds_contract = "fakeguild"_n;
   BOOST_REQUIRE_EQUAL(error("missing authority of eosio"),
      push_action("fakeeosio"_n, "setguildcont"_n, mvo()
         ("contract", new_guilds_contract)
      )
   );

   // Test 2: Set BP score scaling factor
   const uint32_t new_scaling_factor = 2345;
   BOOST_REQUIRE_EQUAL(error("missing authority of eosio"),
      push_action("fakeeosio"_n, "setbpscale"_n, mvo()
         ("scaling_factor", new_scaling_factor)
      )
   );

   // Test 3: Set default BP score
   const uint32_t new_default_score = 5412;
   BOOST_REQUIRE_EQUAL(error("missing authority of eosio"),
      push_action("fakeeosio"_n, "setbpdefscore"_n, mvo()
         ("default_score", new_default_score)
      )
   );
   produce_blocks(1);

   // Test 4: Set min considered producers
   const uint32_t max_considered_producers = 87612;
   BOOST_REQUIRE_EQUAL(error("missing authority of eosio"),
      push_action("fakeeosio"_n, "setmaxprod"_n, mvo()
         ("max_considered_producers", max_considered_producers)
      )
   );
   produce_blocks(1);

   // Test 5: Set min producers vote threshold
   const double min_producer_vote_threshold = 8761234;
   BOOST_REQUIRE_EQUAL(error("missing authority of eosio"),
      push_action("fakeeosio"_n, "setminvote"_n, mvo()
         ("min_producer_vote_threshold", min_producer_vote_threshold)
      )
   );
   produce_blocks(1);

   // Verify state doesn't not change
   fc::variant final_state = get_global_state6();
   BOOST_REQUIRE_EQUAL(final_state["bp_default_score"].as<uint32_t>(), before_state["bp_default_score"].as<uint32_t>());

   // Verify max_considered_producers was set
   BOOST_REQUIRE_EQUAL(final_state["max_considered_producers"].as<uint32_t>(), before_state["max_considered_producers"].as<uint32_t>());

   // Verify min_producer_vote_threshold was set
   BOOST_REQUIRE_EQUAL(final_state["min_producer_vote_threshold"].as<double>(), before_state["min_producer_vote_threshold"].as<double>());

   // Verify all settings persist together
   BOOST_REQUIRE_EQUAL(final_state["guilds_contract"].as<name>(), before_state["guilds_contract"].as<name>());
   BOOST_REQUIRE_EQUAL(final_state["bp_score_scaling_factor"].as<uint32_t>(), before_state["bp_score_scaling_factor"].as<uint32_t>());
   BOOST_REQUIRE_EQUAL(final_state["bp_default_score"].as<uint32_t>(), before_state["bp_default_score"].as<uint32_t>());

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_setmaxprod_validation, eosio_weighted_producer_tester) try {
   fc::variant before_state = get_global_state6();

   // Test 1: max_considered_producers less than 21 should fail
   BOOST_REQUIRE_EQUAL(error("assertion failure with message: max_considered_producers must be at least 21"),
      push_action(config::system_account_name, "setmaxprod"_n, mvo()
         ("max_considered_producers", 20)
      )
   );
   produce_blocks(1);

   // Test 2: max_considered_producers greater than 500 should fail
   BOOST_REQUIRE_EQUAL(error("assertion failure with message: max_considered_producers cannot exceed 500"),
      push_action(config::system_account_name, "setmaxprod"_n, mvo()
         ("max_considered_producers", 501)
      )
   );
   produce_blocks(1);

   // Verify state hasn't changed
   fc::variant final_state = get_global_state6();
   BOOST_REQUIRE_EQUAL(final_state["max_considered_producers"].as<uint32_t>(), before_state["max_considered_producers"].as<uint32_t>());

   // Test 3: Valid boundary values should succeed
   // Test minimum valid value (21)
   BOOST_REQUIRE_EQUAL(success(),
      push_action(config::system_account_name, "setmaxprod"_n, mvo()
         ("max_considered_producers", 21)
      )
   );
   produce_blocks(1);

   fc::variant state_after_min = get_global_state6();
   BOOST_REQUIRE_EQUAL(state_after_min["max_considered_producers"].as<uint32_t>(), 21);

   // Test maximum valid value (500)
   BOOST_REQUIRE_EQUAL(success(),
      push_action(config::system_account_name, "setmaxprod"_n, mvo()
         ("max_considered_producers", 500)
      )
   );
   produce_blocks(1);

   fc::variant state_after_max = get_global_state6();
   BOOST_REQUIRE_EQUAL(state_after_max["max_considered_producers"].as<uint32_t>(), 500);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_setminvote_validation, eosio_weighted_producer_tester) try {
   fc::variant before_state = get_global_state6();

   // Test 1: Negative min_producer_vote_threshold should fail
   BOOST_REQUIRE_EQUAL(error("assertion failure with message: min_producer_vote_threshold cannot be negative"),
      push_action(config::system_account_name, "setminvote"_n, mvo()
         ("min_producer_vote_threshold", -0.1)
      )
   );
   produce_blocks(1);

   // Test 2: Verify state hasn't changed after failed attempt
   fc::variant after_fail = get_global_state6();
   BOOST_REQUIRE_EQUAL(after_fail["min_producer_vote_threshold"].as<double>(), before_state["min_producer_vote_threshold"].as<double>());

   // Test 3: Valid boundary value (0.0) should succeed
   BOOST_REQUIRE_EQUAL(success(),
      push_action(config::system_account_name, "setminvote"_n, mvo()
         ("min_producer_vote_threshold", 0.0)
      )
   );
   produce_blocks(1);

   fc::variant state_after_zero = get_global_state6();
   BOOST_REQUIRE_EQUAL(state_after_zero["min_producer_vote_threshold"].as<double>(), 0.0);

   // Test 4: Positive value should succeed
   const double test_threshold = 150000.5;
   BOOST_REQUIRE_EQUAL(success(),
      push_action(config::system_account_name, "setminvote"_n, mvo()
         ("min_producer_vote_threshold", test_threshold)
      )
   );
   produce_blocks(1);

   fc::variant state_after_positive = get_global_state6();
   BOOST_REQUIRE_EQUAL(state_after_positive["min_producer_vote_threshold"].as<double>(), test_threshold);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_config_and_guild_score, eosio_weighted_producer_tester) try {
   // Test setting config and inserting guild scores

   // Step 1: Set guilds contract name
   const name guilds_contract = GUILDS_OIG;
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setguildcont"_n, mvo()
      ("contract", guilds_contract)
   ));
   produce_blocks(1);

   // Verify guilds contract was set
   fc::variant state = get_global_state6();
   BOOST_REQUIRE_EQUAL(state["guilds_contract"].as<name>(), guilds_contract);

   // Step 2: Set BP score scaling factor
   const uint32_t scaling_factor = 1500; // 1.5x multiplier
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpscale"_n, mvo()
      ("scaling_factor", scaling_factor)
   ));
   produce_blocks(1);

   // Step 3: Set default BP score
   const uint32_t default_score = 1000; // 1.0x default
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpdefscore"_n, mvo()
      ("default_score", default_score)
   ));
   produce_blocks(1);

   // Step 4: Insert guild score for a producer
   const name test_producer = "defproducera"_n;
   const uint32_t producer_score = 2000; // 2.0x score

   // First create and register the producer account
   create_account_with_resources(test_producer, config::system_account_name, core_sym::from_string("1.0000"), false);
   transfer(config::system_account_name, test_producer, core_sym::from_string("10000.0000"), config::system_account_name);
   BOOST_REQUIRE_EQUAL(success(), stake(test_producer, core_sym::from_string("1000.0000"), core_sym::from_string("1000.0000")));
   BOOST_REQUIRE_EQUAL(success(), regproducer(test_producer));
   produce_blocks(1);

   // Insert guild data using the insertguild action
   BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
      ("producer", test_producer)
      ("score", producer_score)
   ));
   produce_blocks(1);

   // Step 5: Verify the guild data was inserted correctly
   guild guild_data = get_guild_table(test_producer);
   BOOST_REQUIRE_EQUAL(guild_data.producer, test_producer);
   BOOST_REQUIRE_EQUAL(guild_data.score, producer_score);

   // Verify the config values persist
   fc::variant final_state = get_global_state6();
   BOOST_REQUIRE_EQUAL(final_state["guilds_contract"].as<name>(), guilds_contract);
   BOOST_REQUIRE_EQUAL(final_state["bp_score_scaling_factor"].as<uint32_t>(), scaling_factor);
   BOOST_REQUIRE_EQUAL(final_state["bp_default_score"].as<uint32_t>(), default_score);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_producer_sorting_with_votes_and_scores, eosio_weighted_producer_tester) try {
   // Test that producers are correctly sorted by weighted votes (votes * score)
   fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);

   // Step 1: Set up configuration
   const name guilds_contract = GUILDS_OIG;
   const uint32_t scaling_factor = 1000; // 1.0x multiplier
   const uint32_t default_score = 1000; // 1.0x default

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setguildcont"_n, mvo()
      ("contract", guilds_contract)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpscale"_n, mvo()
      ("scaling_factor", scaling_factor)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpdefscore"_n, mvo()
      ("default_score", default_score)
   ));
   produce_blocks(1);

   // Get the code hash of the deployed guilds contract
   // Compute hash from the WASM code that was deployed
   auto wasm = contracts::util::guild_test_wasm();
   auto hash_result = fc::sha256::hash(reinterpret_cast<const char*>(wasm.data()), wasm.size());
   ilog( "Guilds contract hash: ${hash}", ("hash", hash_result.str()) );
   // Add the correct hash to approved list
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "addguildhash"_n, mvo()
      ("hash", hash_result.str())
   ));
   produce_blocks(1);

   // Verify configuration was set correctly
   fc::variant state = get_global_state6();
   ilog( "=== Configuration ===" );
   ilog( "guilds_contract: ${guilds}", ("guilds", state["guilds_contract"].as<name>()) );
   ilog( "bp_score_scaling_factor: ${scale}", ("scale", state["bp_score_scaling_factor"].as<uint32_t>()) );
   ilog( "bp_default_score: ${default}", ("default", state["bp_default_score"].as<uint32_t>()) );

   // Verify hash was added
   auto hash_list = state["guilds_code_hashes"].get_array();
   BOOST_REQUIRE_EQUAL(hash_list.size(), 1);
   ilog( "✓ Correct hash added to approved list" );

   // Step 2: Create voters with significant stake to activate chain
   // Need to stake more than 15% of total supply to activate
   // Using multiple voters but they will all vote for the same producers
   // This means all producers get EQUAL raw votes - differences are ONLY from scores!
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> voters = { "voter1111111"_n, "voter2222222"_n, "voter3333333"_n, "voter4444444"_n };
   for (const auto& v: voters) {
      create_account_with_resources( v, config::system_account_name, core_sym::from_string("1.0000"), false, net, cpu );
      transfer( config::system_account_name, v, core_sym::from_string("100000000.0000"), config::system_account_name );
      BOOST_REQUIRE_EQUAL(success(), stake(v, core_sym::from_string("30000000.0000"), core_sym::from_string("30000000.0000")) );
   }

   // Step 3: Create 26 producers (a-z) to test weighted selection
   // With 26 producers competing for 21 slots, we can verify that
   // low-score producers get excluded while high-score ones are selected
   std::vector<account_name> producer_names;
   const std::string root("producer");

   // Create producers: a-z (26 producers total)
   for ( char c = 'a'; c <= 'z'; ++c ) {
      producer_names.emplace_back(root + std::string(1, c));
   }

   setup_producer_accounts(producer_names);
   for (const auto& p: producer_names) {
      BOOST_REQUIRE_EQUAL( success(), regproducer(p) );
      produce_blocks(1);
   }

   ilog( "=== Created 26 producers (a->z) ===" );
   ilog( "Total producers: ${count}", ("count", producer_names.size()) );

   // Step 4: Set different scores for different producers
   // Group 1: LOW score (0.5x) - producers a-e (5 producers) - should be EXCLUDED
   ilog( "=== Setting Scores ===" );
   ilog( "Group 1 (a-e): LOW score 0.5x - these 5 should be EXCLUDED from top 21" );
   for (size_t i = 0; i < 5; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 500) // 0.5x - LOW - should be excluded!
      ));
   }

   // Group 2: Medium score (1.5x) - producers f-p (11 producers)
   ilog( "Group 2 (f-p): MEDIUM score 1.5x" );
   for (size_t i = 5; i < 16; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 1500) // 1.5x - MEDIUM
      ));
   }

   // Group 3: HIGH score (2.0x) - producers q-z (10 producers)
   ilog( "Group 3 (q-z): HIGH score 2.0x - these should be selected" );
   for (size_t i = 16; i < 26; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 2000) // 2.0x - HIGH
      ));
   }
   produce_blocks(1);

   // Verify scores were inserted correctly
   ilog( "=== Verifying Guild Scores ===" );
   ilog( "Low score group (a-e, score=500):" );
   for (size_t i = 0; i < 5; ++i) {
      guild g = get_guild_table(producer_names[i]);
      ilog( "  ${name}: score=${score}", ("name", producer_names[i])("score", g.score) );
   }
   ilog( "Medium score group (f-p, score=1500):" );
   for (size_t i = 5; i < 16; ++i) {
      guild g = get_guild_table(producer_names[i]);
      ilog( "  ${name}: score=${score}", ("name", producer_names[i])("score", g.score) );
   }
   ilog( "High score group (q-z, score=2000):" );
   for (size_t i = 16; i < 26; ++i) {
      guild g = get_guild_table(producer_names[i]);
      ilog( "  ${name}: score=${score}", ("name", producer_names[i])("score", g.score) );
   }

   // Wait 24 hours before voting
   produce_block( fc::hours(24) );

   // Step 5: All voters vote for ALL 26 producers
   // This gives ALL producers EQUAL raw votes
   // The ONLY difference in rankings will be from their SCORES!
   ilog( "=== Voting for all 26 producers ===" );
   for (const auto& v: voters) {
      BOOST_REQUIRE_EQUAL(success(), vote(v, producer_names));
   }

   // Wait for producer schedule to update
   produce_blocks(250);

   // Step 6: Get the active producer schedule
   auto producer_keys = control->head_block_state()->active_schedule.producers;
   BOOST_REQUIRE_EQUAL( 21, producer_keys.size() );

   // Step 7: Verify the selection is based on weighted votes
   // ALL producers have EQUAL raw votes (all voters voting for all 26)
   // Expected weighted votes calculation:
   // Group 1 (a-e, 5 producers): score 0.5x = LOWEST weighted votes - should be EXCLUDED
   // Group 2 (f-p, 11 producers): score 1.5x = MEDIUM weighted votes - should be INCLUDED
   // Group 3 (q-z, 10 producers): score 2.0x = HIGHEST weighted votes - should be INCLUDED
   //
   // Since raw votes are IDENTICAL, the top 21 should be: f-z (11 medium + 10 high = 21)
   // Producers a-e should be EXCLUDED due to low scores!

   ilog( "=== Active Producer Schedule (21 selected from 26 total) ===" );
   ilog( "Expected: f-z should be selected, a-e should be EXCLUDED" );

   std::set<name> active_producers;
   for (size_t i = 0; i < producer_keys.size(); ++i) {
      active_producers.insert(producer_keys[i].producer_name);
      ilog( "${idx}: ${name}", ("idx", i)("name", producer_keys[i].producer_name) );
   }

   // CRITICAL TEST: Verify that low-score producers (a-e) are NOT in the top 21
   ilog( "=== Verifying Low-Score Producers Exclusion ===" );
   int excluded_count = 0;
   for (size_t i = 0; i < 5; ++i) {
      name low_score_producer = producer_names[i]; // a-e
      if (active_producers.find(low_score_producer) == active_producers.end()) {
         ilog( "✓ ${name} (low score 0.5x) is EXCLUDED - CORRECT!", ("name", low_score_producer) );
         excluded_count++;
      } else {
         ilog( "✗ ${name} (low score 0.5x) is INCLUDED - WRONG!", ("name", low_score_producer) );
      }
   }

   ilog( "Excluded low-score producers: ${count}/5", ("count", excluded_count) );
   BOOST_REQUIRE_EQUAL(excluded_count, 5); // ALL 5 low-score producers should be excluded!

   // Verify that medium and high-score producers ARE in the top 21
   ilog( "=== Verifying Medium/High-Score Producers Inclusion ===" );
   int included_count = 0;
   for (size_t i = 5; i < 26; ++i) { // f-z
      name producer = producer_names[i];
      if (active_producers.find(producer) != active_producers.end()) {
         included_count++;
      }
   }

   ilog( "Included medium/high-score producers: ${count}/21", ("count", included_count) );
   BOOST_REQUIRE_EQUAL(included_count, 21); // All 21 slots should be filled with f-z

   ilog( "SUCCESS: Weighted voting correctly selected top 21 by score!" );
   ilog( "Low-score producers (a-e) were EXCLUDED despite having equal votes!" );

   // Verify individual producer weighted votes
   ilog( "=== Producer Vote Details ===" );
   for (const auto& p : producer_names) {
      fc::variant producer_info = get_producer_info(p);
      double total_votes = producer_info["total_votes"].as<double>();

      // Get the score for this producer
      guild guild_data = get_guild_table(p);
      uint32_t score = guild_data.score;

      // Calculate weighted votes
      double weighted_votes = total_votes * score / 1000.0;

      ilog( "${name}: votes=${votes}, score=${score}, weighted=${weighted}",
            ("name", p)
            ("votes", total_votes)
            ("score", score)
            ("weighted", weighted_votes) );
   }

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_weighted_producer_with_standby_selection, eosio_weighted_producer_tester) try {
   // Test that weighted voting affects both active (top 21) and standby producer selection
   fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);

   // Step 1: Configure weighted voting system
   const name guilds_contract = GUILDS_OIG;
   const uint32_t scaling_factor = 1000; // 1.0x multiplier
   const uint32_t default_score = 1000; // 1.0x default

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setguildcont"_n, mvo()
      ("contract", guilds_contract)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpscale"_n, mvo()
      ("scaling_factor", scaling_factor)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpdefscore"_n, mvo()
      ("default_score", default_score)
   ));
   produce_blocks(1);

   // Get the code hash of the deployed guilds contract
   // Compute hash from the WASM code that was deployed
   auto wasm = contracts::util::guild_test_wasm();
   auto hash_result = fc::sha256::hash(reinterpret_cast<const char*>(wasm.data()), wasm.size());
   ilog( "Guilds contract hash: ${hash}", ("hash", hash_result.str()) );
   // Add the correct hash to approved list
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "addguildhash"_n, mvo()
      ("hash", hash_result.str())
   ));
   produce_blocks(1);

   ilog( "=== Weighted Voting Configuration ===" );
   fc::variant state5 = get_global_state6();
   ilog( "guilds_contract: ${guilds}", ("guilds", state5["guilds_contract"].as<name>()) );
   ilog( "bp_score_scaling_factor: ${scale}", ("scale", state5["bp_score_scaling_factor"].as<uint32_t>()) );
   ilog( "bp_default_score: ${default}", ("default", state5["bp_default_score"].as<uint32_t>()) );
   // Verify hash was added
   auto hash_list = state5["guilds_code_hashes"].get_array();
   BOOST_REQUIRE_EQUAL(hash_list.size(), 1);
   ilog( "✓ Correct hash added to approved list" );

   // Step 2: Configure standby system
   const uint32_t standby_slots = 5;
   const uint32_t standby_ratio = 5000; // 0.5 weight

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setsbslot"_n, mvo()
      ("num_slots", standby_slots)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setsbratio"_n, mvo()
      ("ratio", standby_ratio)
   ));
   produce_blocks(1);

   ilog( "=== Standby Configuration ===" );
   fc::variant state4 = get_global_state4();
   ilog( "num_standby_slots: ${slots}", ("slots", state4["num_standby_slots"].as<uint32_t>()) );
   ilog( "standby_slot_weight: ${weight}", ("weight", state4["standby_slot_weight"].as<uint32_t>()) );

   // Step 3: Create voters with significant stake
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> voters = { "voter1111111"_n, "voter2222222"_n, "voter3333333"_n, "voter4444444"_n };
   for (const auto& v: voters) {
      create_account_with_resources( v, config::system_account_name, core_sym::from_string("1.0000"), false, net, cpu );
      transfer( config::system_account_name, v, core_sym::from_string("100000000.0000"), config::system_account_name );
      BOOST_REQUIRE_EQUAL(success(), stake(v, core_sym::from_string("30000000.0000"), core_sym::from_string("30000000.0000")) );
   }

   // Step 4: Create 30 producers to test both active and standby selection
   // We need: 21 active + 5 standby = 26 slots, plus extras to test exclusion
   // Use EOSIO-compliant names: 12 chars max, a-z and 1-5 only
   std::vector<account_name> producer_names;

   // Create prода111111aa through prod111111ad (30 producers)
   // Using pattern: prod111111aa, prod111111ab, ..., prod111111az,
   //                prod111111ba, prod111111bb, prod111111bc, prod111111bd
   const std::string root("prod111111");
   const std::vector<std::string> suffixes = {
      "aa", "ab", "ac", "ad", "ae", "af", "ag", "ah", "ai", "aj",  // 10
      "ak", "al", "am", "an", "ao", "ap", "aq", "ar", "as", "at",  // 20
      "au", "av", "aw", "ax", "ay", "az", "ba", "bb", "bc", "bd"   // 30
   };

   for (const auto& suffix : suffixes) {
      producer_names.emplace_back(root + suffix);
   }

   // Names are already sorted since we created them in alphabetical order
   setup_producer_accounts(producer_names);
   for (const auto& p: producer_names) {
      BOOST_REQUIRE_EQUAL( success(), regproducer(p) );
      produce_blocks(1);
   }

   ilog( "=== Created ${count} producers ===" , ("count", producer_names.size()));
   ilog( "First producer: ${first}, Last producer: ${last}",
         ("first", producer_names.front())("last", producer_names.back()) );

   // Step 5: Assign weighted scores to producers
   // We have: prod111111aa, prod111111ab, ..., prod111111bd (30 producers in alphabetical order)
   // Group 1 (indices 0-3): VERY LOW score 0.3x - should be EXCLUDED from all (4 producers: aa-ad)
   // Group 2 (indices 4-7): LOW score 0.8x - may be in standby or excluded (4 producers: ae-ah)
   // Group 3 (indices 8-21): MEDIUM score 1.5x - should be in active slots (14 producers: ai-av)
   // Group 4 (indices 22-29): HIGH score 2.5x - should be in active slots (8 producers: aw-bd)

   ilog( "=== Setting Producer Scores ===" );

   ilog( "Group 1 (indices 0-3, prod111111aa-ad): VERY LOW score 0.3x - should be EXCLUDED" );
   for (size_t i = 0; i < 4; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 300) // 0.3x - VERY LOW
      ));
      ilog( "  ${name}: score=300", ("name", producer_names[i]) );
   }

   ilog( "Group 2 (indices 4-7, prod111111ae-ah): LOW score 0.8x - may be standby or excluded" );
   for (size_t i = 4; i < 8; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 800) // 0.8x - LOW
      ));
      ilog( "  ${name}: score=800", ("name", producer_names[i]) );
   }

   ilog( "Group 3 (indices 8-21, prod111111ai-av): MEDIUM score 1.5x - should be in active" );
   for (size_t i = 8; i < 22; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 1500) // 1.5x - MEDIUM
      ));
   }

   ilog( "Group 4 (indices 22-29, prod111111aw-bd): HIGH score 2.5x - should be in active" );
   for (size_t i = 22; i < 30; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 2500) // 2.5x - HIGH
      ));
   }
   produce_blocks(1);

   // Step 6: All voters vote for ALL producers (equal raw votes)
   produce_block( fc::hours(24) );

   ilog( "=== Voting for all ${count} producers ===" , ("count", producer_names.size()));
   for (const auto& v: voters) {
      BOOST_REQUIRE_EQUAL(success(), vote(v, producer_names));
   }

   // Wait for schedule to update
   produce_blocks(250);

   // Step 7: Verify active producer selection (top 21)
   auto producer_keys = control->head_block_state()->active_schedule.producers;
   BOOST_REQUIRE_EQUAL( 21, producer_keys.size() );

   ilog( "=== Active Producers (Top 21 by weighted votes) ===" );
   std::set<name> active_producers;
   for (size_t i = 0; i < producer_keys.size(); ++i) {
      active_producers.insert(producer_keys[i].producer_name);
      ilog( "${idx}: ${name}", ("idx", i)("name", producer_keys[i].producer_name) );
   }

   // Step 8: Verify standby producer selection
   auto standby_producers = get_standby_table();
   ilog( "=== Standby Producers (Next ${count} by weighted votes) ===" , ("count", standby_producers.size()));

   std::set<name> standby_producer_names;
   for (size_t i = 0; i < standby_producers.size(); ++i) {
      standby_producer_names.insert(standby_producers[i].owner);
      ilog( "${idx}: ${name} (active=${active})",
            ("idx", i)
            ("name", standby_producers[i].owner)
            ("active", standby_producers[i].is_active) );
   }

   // Step 9: Verification - Check that weighted voting affected both lists

   // Verify VERY LOW score producers (prod111111aa-ad) are EXCLUDED from BOTH active and standby
   ilog( "=== Verifying VERY LOW Score Producers (prod111111aa-ad) Exclusion ===" );
   int excluded_from_both = 0;
   for (size_t i = 0; i < 4; ++i) {
      name producer = producer_names[i];
      bool in_active = active_producers.find(producer) != active_producers.end();
      bool in_standby = standby_producer_names.find(producer) != standby_producer_names.end();

      if (!in_active && !in_standby) {
         ilog( "✓ ${name} (score 0.3x) EXCLUDED from both - CORRECT!", ("name", producer) );
         excluded_from_both++;
      } else {
         ilog( "✗ ${name} (score 0.3x) found in ${where} - WRONG!",
               ("name", producer)
               ("where", in_active ? "active" : "standby") );
      }
   }
   BOOST_REQUIRE_EQUAL(excluded_from_both, 4); // All 4 very low score producers excluded

   // Verify HIGH score producers (prod111111aw-bd) are in ACTIVE (not standby)
   ilog( "=== Verifying HIGH Score Producers (prod111111aw-bd) in Active ===" );
   int high_score_in_active = 0;
   for (size_t i = 22; i < 30; ++i) {
      name producer = producer_names[i];
      bool in_active = active_producers.find(producer) != active_producers.end();

      if (in_active) {
         high_score_in_active++;
         ilog( "✓ ${name} (score 2.5x) in ACTIVE - CORRECT!", ("name", producer) );
      } else {
         ilog( "✗ ${name} (score 2.5x) NOT in active - checking standby...", ("name", producer) );
      }
   }

   // At least most high-score producers should be in active
   BOOST_REQUIRE(high_score_in_active >= 6); // At least 6 of 8 high-score producers in active

   // Verify standby count matches configuration
   BOOST_REQUIRE_EQUAL(standby_producers.size(), standby_slots);

   // Verify no overlap between active and standby
   ilog( "=== Verifying No Overlap Between Active and Standby ===" );
   int overlap_count = 0;
   for (const auto& sp : standby_producer_names) {
      if (active_producers.find(sp) != active_producers.end()) {
         ilog( "✗ ${name} found in BOTH active and standby!", ("name", sp) );
         overlap_count++;
      }
   }
   BOOST_REQUIRE_EQUAL(overlap_count, 0); // No producer should be in both lists

   // Step 10: Log detailed weighted votes for analysis
   ilog( "=== Producer Weighted Vote Details ===" );
   for (const auto& p : producer_names) {
      fc::variant producer_info = get_producer_info(p);
      double total_votes = producer_info["total_votes"].as<double>();

      guild guild_data = get_guild_table(p);
      uint32_t score = guild_data.score;
      double weighted_votes = total_votes * score / 1000.0;

      bool in_active = active_producers.find(p) != active_producers.end();
      bool in_standby = standby_producer_names.find(p) != standby_producer_names.end();
      std::string status = in_active ? "ACTIVE" : (in_standby ? "STANDBY" : "EXCLUDED");

      ilog( "${name}: votes=${votes}, score=${score}, weighted=${weighted}, status=${status}",
            ("name", p)
            ("votes", total_votes)
            ("score", score)
            ("weighted", weighted_votes)
            ("status", status) );
   }

   ilog( "=== TEST SUMMARY ===" );
   ilog( "✓ Active producers: ${active} (expected 21)", ("active", producer_keys.size()) );
   ilog( "✓ Standby producers: ${standby} (expected ${expected})",
         ("standby", standby_producers.size())
         ("expected", standby_slots) );
   ilog( "✓ Very low score producers excluded: ${excluded}/4", ("excluded", excluded_from_both) );
   ilog( "✓ High score producers in active: ${high}/${total}",
         ("high", high_score_in_active)
         ("total", 8) );
   ilog( "✓ No overlap between active and standby" );
   ilog( "SUCCESS: Weighted voting correctly affects both active and standby selection!" );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_min_vote_threshold_filters_standby_bps, eosio_weighted_producer_tester) try {
   // Test that min_vote_threshold correctly filters out producers with insufficient votes
   // Only producers with total_votes > min_vote_threshold should be considered for top 21 or standby
   fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);

   // Step 1: Configure weighted voting system
   const name guilds_contract = GUILDS_OIG;
   const uint32_t scaling_factor = 1000; // 1.0x multiplier
   const uint32_t default_score = 1000; // 1.0x default

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setguildcont"_n, mvo()
      ("contract", guilds_contract)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpscale"_n, mvo()
      ("scaling_factor", scaling_factor)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpdefscore"_n, mvo()
      ("default_score", default_score)
   ));
   produce_blocks(1);

   // Get the code hash of the deployed guilds contract
   auto wasm = contracts::util::guild_test_wasm();
   auto hash_result = fc::sha256::hash(reinterpret_cast<const char*>(wasm.data()), wasm.size());
   ilog( "Guilds contract hash: ${hash}", ("hash", hash_result.str()) );
   // Add the correct hash to approved list
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "addguildhash"_n, mvo()
      ("hash", hash_result.str())
   ));
   produce_blocks(1);

   // Step 2: Configure standby system
   const uint32_t standby_slots = 5;
   const uint32_t standby_ratio = 5000; // 0.5 weight

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setsbslot"_n, mvo()
      ("num_slots", standby_slots)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setsbratio"_n, mvo()
      ("ratio", standby_ratio)
   ));
   produce_blocks(1);

   // Step 3: Create voters with different stake amounts to create vote variation
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const asset initial_supply = core_sym::from_string("100000000.0000");
   const asset high_vote = core_sym::from_string("30000000.0000");
   const asset low_vote = core_sym::from_string("10000000.0000");

   // Create high-stake voters
   const std::vector<account_name> high_voters = { "voter1111111"_n, "voter2222222"_n };
   for (const auto& v: high_voters) {
      create_account_with_resources( v, config::system_account_name, core_sym::from_string("1.0000"), false, net, cpu );
      transfer( config::system_account_name, v, initial_supply, config::system_account_name );
      BOOST_REQUIRE_EQUAL(success(), stake(v, high_vote, high_vote) );
   }

   // Create low-stake voters
   const std::vector<account_name> low_voters = { "voter3333333"_n, "voter4444444"_n };
   for (const auto& v: low_voters) {
      create_account_with_resources( v, config::system_account_name, core_sym::from_string("1.0000"), false, net, cpu );
      transfer( config::system_account_name, v, core_sym::from_string("100000000.0000"), config::system_account_name );
      BOOST_REQUIRE_EQUAL(success(), stake(v, low_vote, low_vote) );
   }

   // Step 4: Create 30 producers
   std::vector<account_name> producer_names;
   const std::string root("prod111111");
   const std::vector<std::string> suffixes = {
      "aa", "ab", "ac", "ad", "ae", "af", "ag", "ah", "ai", "aj",  // 10
      "ak", "al", "am", "an", "ao", "ap", "aq", "ar", "as", "at",  // 20
      "au", "av", "aw", "ax", "ay", "az", "ba", "bb", "bc", "bd"   // 30
   };

   for (const auto& suffix : suffixes) {
      producer_names.emplace_back(root + suffix);
   }

   setup_producer_accounts(producer_names);
   for (const auto& p: producer_names) {
      BOOST_REQUIRE_EQUAL( success(), regproducer(p) );
      produce_blocks(1);
   }

   ilog( "=== Created ${count} producers ===" , ("count", producer_names.size()));

   for (size_t i = 0; i < 30; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 300)
      ));
      ilog( "  ${name}: score=300", ("name", producer_names[i]) );
   }

   // Step 5: Vote to create variation in vote weights
   produce_block( fc::hours(24) );

   // High-stake voters vote for first 25 producers (aa-ay)
   std::vector<account_name> high_vote_producers(producer_names.begin(), producer_names.begin() + 23);
   for (const auto& v: high_voters) {
      BOOST_REQUIRE_EQUAL(success(), vote(v, high_vote_producers));
   }

   // Low-stake voters vote for last 5 producers (az-bd) - these will have low votes
   std::vector<account_name> low_vote_producers(producer_names.begin() + 23, producer_names.end());
   for (const auto& v: low_voters) {
      BOOST_REQUIRE_EQUAL(success(), vote(v, low_vote_producers));
   }

   // Wait for votes to be recorded
   produce_blocks(10);

   // Step 6: Calculate the vote threshold - set it between high and low vote weights
   // Get vote weight of a producer with high votes
   fc::variant high_vote_prod_info = get_producer_info(producer_names[0]); // prod111111aa
   fc::variant high_vote_prod_info1 = get_producer_info(producer_names[1]); // prod111111aa
   fc::variant high_vote_prod_info2 = get_producer_info(producer_names[2]); // prod111111aa
   double high_votes = high_vote_prod_info["total_votes"].as<double>();

   // Get vote weight of a producer with low votes
   fc::variant low_vote_prod_info = get_producer_info(producer_names[25]); // prod111111az
   fc::variant low_vote_prod_info1 = get_producer_info(producer_names[24]); // prod111111az
   double low_votes = low_vote_prod_info["total_votes"].as<double>();

   ilog( "=== Vote Weight Analysis ===" );
   ilog( "High-vote producer (${name}): ${votes}", ("name", producer_names[0])("votes", high_votes) );
   ilog( "Low-vote producer (${name}): ${votes}", ("name", producer_names[25])("votes", low_votes) );

   // Set threshold between low and high votes to filter out low-vote producers
   double min_vote_threshold = (high_votes + low_votes) / 2.0;
   ilog( "Setting min_vote_threshold to: ${threshold}", ("threshold", min_vote_threshold) );

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setminvote"_n, mvo()
      ("min_producer_vote_threshold", min_vote_threshold)
   ));
   produce_blocks(1);

   // Verify threshold was set
   auto state6 = get_global_state6();
   double actual_threshold = state6["min_producer_vote_threshold"].as<double>();
   BOOST_REQUIRE_EQUAL(actual_threshold, min_vote_threshold);
   ilog( "✓ min_vote_threshold confirmed: ${threshold}", ("threshold", actual_threshold) );

   // Step 7: Trigger schedule update
   produce_blocks(250);

   // Step 8: Verify active producer selection (should only include high-vote producers)
   auto producer_keys = control->head_block_state()->active_schedule.producers;
   ilog( "${idx}: ${name}", ("idx", 0)("name", producer_keys[0].producer_name) );
   BOOST_REQUIRE_EQUAL( 21, producer_keys.size() );

   ilog( "=== Active Producers (Top 21 - should only have high-vote producers) ===" );
   std::set<name> active_producers;
   for (size_t i = 0; i < producer_keys.size(); ++i) {
      active_producers.insert(producer_keys[i].producer_name);
      ilog( "${idx}: ${name}", ("idx", i)("name", producer_keys[i].producer_name) );
   }

   // Step 9: Verify standby producer selection
   auto standby_producers = get_standby_table();
   ilog( "=== Standby Producers (should only have high-vote producers) ===" );

   std::set<name> standby_producer_names;
   for (size_t i = 0; i < standby_producers.size() && standby_producers[i].is_active; ++i) {
      standby_producer_names.insert(standby_producers[i].owner);
      ilog( "${idx}: ${name} (active=${active})",
            ("idx", i)
            ("name", standby_producers[i].owner)
            ("active", standby_producers[i].is_active) );
   }
   // only 23 bps are above min_vote_threshold, 21 are main bps, 2 are standbys
   BOOST_REQUIRE_EQUAL( 2, standby_producer_names.size() );

   // Step 10: Verification - Check that low-vote producers are EXCLUDED
   ilog( "=== Verifying Low-Vote Producers Exclusion ===" );
   int excluded_count = 0;
   for (size_t i = 23; i < producer_names.size(); ++i) {
      name producer = producer_names[i];
      fc::variant prod_info = get_producer_info(producer);
      double votes = prod_info["total_votes"].as<double>();

      bool in_active = active_producers.find(producer) != active_producers.end();
      bool in_standby = standby_producer_names.find(producer) != standby_producer_names.end();

      if (!in_active && !in_standby) {
         BOOST_REQUIRE(votes <= min_vote_threshold);
         ilog( "✓ ${name} (votes=${votes}) EXCLUDED - votes below threshold ${threshold}",
               ("name", producer)
               ("votes", votes)
               ("threshold", min_vote_threshold) );
         excluded_count++;
      } else {
         ilog( "✗ ${name} (votes=${votes}) found in ${where} - should be EXCLUDED!",
               ("name", producer)
               ("votes", votes)
               ("where", in_active ? "active" : "standby") );
      }
   }

   // All 7 low-vote producers should be excluded
   BOOST_REQUIRE_EQUAL(excluded_count, 7);

   // Step 11: Verify high-vote producers are INCLUDED
   ilog( "=== Verifying High-Vote Producers Inclusion ===" );
   for (size_t i = 0; i < 21; ++i) {
      name producer = producer_names[i];
      fc::variant prod_info = get_producer_info(producer);
      double votes = prod_info["total_votes"].as<double>();

      bool in_active = active_producers.find(producer) != active_producers.end();

      BOOST_REQUIRE(in_active);
      BOOST_REQUIRE(votes > min_vote_threshold);
   }

   for (size_t i = 21; i < 23; ++i) {
      name producer = producer_names[i];
      fc::variant prod_info = get_producer_info(producer);
      double votes = prod_info["total_votes"].as<double>();

      bool in_standby = standby_producer_names.find(producer) != active_producers.end();

      BOOST_REQUIRE(in_standby);
      BOOST_REQUIRE(votes > min_vote_threshold);
   }

   ilog( "=== TEST SUMMARY ===" );
   ilog( "✓ min_vote_threshold set to: ${threshold}", ("threshold", min_vote_threshold) );
   ilog( "✓ Low-vote producers excluded: ${excluded}/5", ("excluded", excluded_count) );
   ilog( "✓ Active producers: ${active}", ("active", producer_keys.size()) );
   ilog( "✓ Standby producers: ${standby}", ("standby", standby_producers.size()) );
   ilog( "SUCCESS: min_vote_threshold correctly filters producers!" );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_min_vote_threshold_filters_main_bps, eosio_weighted_producer_tester) try {
   // Test that min_vote_threshold correctly limits main (active) BPs when threshold is high
   // Only 15 producers will have votes above threshold -> 15 active BPs, 0 standbys
   fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);

   // Step 1: Configure weighted voting system
   const name guilds_contract = GUILDS_OIG;
   const uint32_t scaling_factor = 1000; // 1.0x multiplier
   const uint32_t default_score = 1000; // 1.0x default

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setguildcont"_n, mvo()
      ("contract", guilds_contract)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpscale"_n, mvo()
      ("scaling_factor", scaling_factor)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpdefscore"_n, mvo()
      ("default_score", default_score)
   ));
   produce_blocks(1);

   // Get the code hash of the deployed guilds contract
   auto wasm = contracts::util::guild_test_wasm();
   auto hash_result = fc::sha256::hash(reinterpret_cast<const char*>(wasm.data()), wasm.size());
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "addguildhash"_n, mvo()
      ("hash", hash_result.str())
   ));
   produce_blocks(1);

   // Step 2: Configure standby system
   const uint32_t standby_slots = 5;
   const uint32_t standby_ratio = 5000; // 0.5 weight

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setsbslot"_n, mvo()
      ("num_slots", standby_slots)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setsbratio"_n, mvo()
      ("ratio", standby_ratio)
   ));
   produce_blocks(1);

   // Step 3: Create voters with different stake amounts
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const asset initial_supply = core_sym::from_string("100000000.0000");
   const asset high_vote = core_sym::from_string("30000000.0000");
   const asset low_vote = core_sym::from_string("10000000.0000");

   // Create high-stake voters
   const std::vector<account_name> high_voters = { "voter1111111"_n, "voter2222222"_n };
   for (const auto& v: high_voters) {
      create_account_with_resources( v, config::system_account_name, core_sym::from_string("1.0000"), false, net, cpu );
      transfer( config::system_account_name, v, initial_supply, config::system_account_name );
      BOOST_REQUIRE_EQUAL(success(), stake(v, high_vote, high_vote) );
   }

   // Create low-stake voters
   const std::vector<account_name> low_voters = { "voter3333333"_n, "voter4444444"_n };
   for (const auto& v: low_voters) {
      create_account_with_resources( v, config::system_account_name, core_sym::from_string("1.0000"), false, net, cpu );
      transfer( config::system_account_name, v, core_sym::from_string("100000000.0000"), config::system_account_name );
      BOOST_REQUIRE_EQUAL(success(), stake(v, low_vote, low_vote) );
   }

   // Step 4: Create 30 producers
   std::vector<account_name> producer_names;
   const std::string root("prod111111");
   const std::vector<std::string> suffixes = {
      "aa", "ab", "ac", "ad", "ae", "af", "ag", "ah", "ai", "aj",  // 10
      "ak", "al", "am", "an", "ao", "ap", "aq", "ar", "as", "at",  // 20
      "au", "av", "aw", "ax", "ay", "az", "ba", "bb", "bc", "bd"   // 30
   };

   for (const auto& suffix : suffixes) {
      producer_names.emplace_back(root + suffix);
   }

   setup_producer_accounts(producer_names);
   for (const auto& p: producer_names) {
      BOOST_REQUIRE_EQUAL( success(), regproducer(p) );
      // produce_blocks(1);
   }

   ilog( "=== Created ${count} producers ===" , ("count", producer_names.size()));

   // Set all producers with same guild score
   for (size_t i = 0; i < 30; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 1000)
      ));
   }

   // Step 5: Vote to create variation - only 15 producers get high votes
   produce_block( fc::hours(24) );

   // High-stake voters vote for first 15 producers only (aa-ao)
   std::vector<account_name> high_vote_producers(producer_names.begin(), producer_names.begin() + 15);
   for (const auto& v: high_voters) {
      BOOST_REQUIRE_EQUAL(success(), vote(v, high_vote_producers));
   }

   // Step 6: Set threshold between high and low vote weights
   fc::variant high_vote_prod_info = get_producer_info(producer_names[0]); // prod111111aa
   double high_votes = high_vote_prod_info["total_votes"].as<double>();

   ilog( "=== Vote Weight Analysis ===" );
   ilog( "High-vote producer (${name}): ${votes}", ("name", producer_names[0])("votes", high_votes) );

   // Set threshold right below high votes to filter out low-vote producers
   // Set min vote before low vote producer
   // To make sure that threshold was set before update_elected_producers trigger cause thresh_activated_stake_time
   double min_vote_threshold = high_votes - high_votes/10.0;
   ilog( "Setting min_vote_threshold to: ${threshold}", ("threshold", min_vote_threshold) );

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setminvote"_n, mvo()
      ("min_producer_vote_threshold", min_vote_threshold)
   ));

   // Low-stake voters vote for producers 15-29 (ap-bd) - these will have low votes
   std::vector<account_name> low_vote_producers(producer_names.begin() + 15, producer_names.end());
   for (const auto& v: low_voters) {
      BOOST_REQUIRE_EQUAL(success(), vote(v, low_vote_producers));
   }

   fc::variant low_vote_prod_info = get_producer_info(producer_names[15]); // prod111111ap
   double low_votes = low_vote_prod_info["total_votes"].as<double>();
   ilog( "Low-vote producer (${name}): ${votes}", ("name", producer_names[15])("votes", low_votes) );

   // Verify threshold was set
   auto state6 = get_global_state6();
   double actual_threshold = state6["min_producer_vote_threshold"].as<double>();
   BOOST_REQUIRE_EQUAL(actual_threshold, min_vote_threshold);
   ilog( "✓ min_vote_threshold confirmed: ${threshold}", ("threshold", actual_threshold) );

   // Step 7: Trigger schedule update
   produce_blocks(250);

   // Step 8: Verify active producer selection - should only have 15 BPs
   auto producer_keys = control->head_block_state()->active_schedule.producers;

   ilog( "=== Active Producers (should be 15 only) ===" );
   std::set<name> active_producers;
   for (size_t i = 0; i < producer_keys.size(); ++i) {
      active_producers.insert(producer_keys[i].producer_name);
      ilog( "${idx}: ${name}", ("idx", i)("name", producer_keys[i].producer_name) );
   }

   // Only 15 producers have votes above threshold -> only 15 active BPs
   BOOST_REQUIRE_EQUAL( 15, producer_keys.size() );

   // Step 9: Verify standby producer selection - should be ZERO
   auto standby_producers = get_standby_table();

   std::set<name> standby_producer_names;
   for (size_t i = 0; i < standby_producers.size() && standby_producers[i].is_active; ++i) {
      standby_producer_names.insert(standby_producers[i].owner);
      ilog( "${idx}: ${name} (active=${active})",
            ("idx", i)
            ("name", standby_producers[i].owner)
            ("active", standby_producers[i].is_active) );
   }

   // No producers qualify for standby since only 15 total are above threshold
   BOOST_REQUIRE_EQUAL( 0, standby_producer_names.size() );

   // Step 10: Verify first 15 producers (high votes) are in active
   ilog( "=== Verifying High-Vote Producers (first 15) in Active ===" );
   for (size_t i = 0; i < 15; ++i) {
      name producer = producer_names[i];
      fc::variant prod_info = get_producer_info(producer);
      double votes = prod_info["total_votes"].as<double>();

      bool in_active = active_producers.find(producer) != active_producers.end();

      BOOST_REQUIRE(in_active);
      BOOST_REQUIRE(votes > min_vote_threshold);
      ilog( "✓ ${name} (votes=${votes}) in ACTIVE", ("name", producer)("votes", votes) );
   }

   // Step 11: Verify remaining producers (low votes) are EXCLUDED
   ilog( "=== Verifying Low-Vote Producers (15-29) Excluded ===" );
   int excluded_count = 0;
   for (size_t i = 15; i < producer_names.size(); ++i) {
      name producer = producer_names[i];
      fc::variant prod_info = get_producer_info(producer);
      double votes = prod_info["total_votes"].as<double>();

      bool in_active = active_producers.find(producer) != active_producers.end();
      bool in_standby = standby_producer_names.find(producer) != standby_producer_names.end();

      if (!in_active && !in_standby) {
         BOOST_REQUIRE(votes <= min_vote_threshold);
         excluded_count++;
      }
   }

   // All 15 low-vote producers should be excluded
   BOOST_REQUIRE_EQUAL(excluded_count, 15);

   ilog( "=== TEST SUMMARY ===" );
   ilog( "✓ min_vote_threshold set to: ${threshold}", ("threshold", min_vote_threshold) );
   ilog( "✓ Active producers: ${active} (expected 15)", ("active", producer_keys.size()) );
   ilog( "✓ Standby producers: ${standby} (expected 0)", ("standby", standby_producer_names.size()) );
   ilog( "✓ Low-vote producers excluded: ${excluded}/15", ("excluded", excluded_count) );
   ilog( "SUCCESS: min_vote_threshold correctly limits main BPs to 15!" );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_max_considered_producers_filters_main_bps, eosio_weighted_producer_tester) try {
   // Test that max_considered_producers correctly limits how many producers are considered
   // Setting max_considered_producers to 15 -> only 15 active BPs, 0 standbys
   fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);

   // Step 1: Configure weighted voting system
   const name guilds_contract = GUILDS_OIG;
   const uint32_t scaling_factor = 1000; // 1.0x multiplier
   const uint32_t default_score = 1000; // 1.0x default

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setguildcont"_n, mvo()
      ("contract", guilds_contract)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpscale"_n, mvo()
      ("scaling_factor", scaling_factor)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpdefscore"_n, mvo()
      ("default_score", default_score)
   ));
   produce_blocks(1);

   // Get the code hash of the deployed guilds contract
   auto wasm = contracts::util::guild_test_wasm();
   auto hash_result = fc::sha256::hash(reinterpret_cast<const char*>(wasm.data()), wasm.size());
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "addguildhash"_n, mvo()
      ("hash", hash_result.str())
   ));
   produce_blocks(1);

   // Step 2: Configure standby system
   const uint32_t standby_slots = 5;
   const uint32_t standby_ratio = 5000; // 0.5 weight

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setsbslot"_n, mvo()
      ("num_slots", standby_slots)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setsbratio"_n, mvo()
      ("ratio", standby_ratio)
   ));
   produce_blocks(1);

   // Step 3: Set max_considered_producers to 15
   const uint32_t max_considered = 15;
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setmaxprod"_n, mvo()
      ("max_considered_producers", max_considered)
   ));
   produce_blocks(1);

   // Verify max_considered_producers was set
   auto state6 = get_global_state6();
   uint32_t actual_max_considered = state6["max_considered_producers"].as<uint32_t>();
   BOOST_REQUIRE_EQUAL(actual_max_considered, max_considered);
   ilog( "✓ max_considered_producers confirmed: ${max}", ("max", actual_max_considered) );

   // Step 4: Create voters with equal stake
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const asset initial_supply = core_sym::from_string("100000000.0000");
   const asset stake_amount = core_sym::from_string("30000000.0000");

   // Create voters - all with same stake so all producers get equal votes
   const std::vector<account_name> voters = { "voter1111111"_n, "voter2222222"_n, "voter3333333"_n, "voter4444444"_n };
   for (const auto& v: voters) {
      create_account_with_resources( v, config::system_account_name, core_sym::from_string("1.0000"), false, net, cpu );
      transfer( config::system_account_name, v, initial_supply, config::system_account_name );
      BOOST_REQUIRE_EQUAL(success(), stake(v, stake_amount, stake_amount) );
   }

   // Step 5: Create 30 producers
   std::vector<account_name> producer_names;
   const std::string root("prod111111");
   const std::vector<std::string> suffixes = {
      "aa", "ab", "ac", "ad", "ae", "af", "ag", "ah", "ai", "aj",  // 10
      "ak", "al", "am", "an", "ao", "ap", "aq", "ar", "as", "at",  // 20
      "au", "av", "aw", "ax", "ay", "az", "ba", "bb", "bc", "bd"   // 30
   };

   for (const auto& suffix : suffixes) {
      producer_names.emplace_back(root + suffix);
   }

   setup_producer_accounts(producer_names);
   for (const auto& p: producer_names) {
      BOOST_REQUIRE_EQUAL( success(), regproducer(p) );
      produce_blocks(1);
   }

   ilog( "=== Created ${count} producers ===" , ("count", producer_names.size()));

   // Set all producers with same guild score
   for (size_t i = 0; i < 30; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 1000)
      ));
   }

   // Step 6: All voters vote for ALL producers (equal votes for all)
   produce_block( fc::hours(24) );

   for (const auto& v: voters) {
      BOOST_REQUIRE_EQUAL(success(), vote(v, producer_names));
   }

   // Wait for votes to be recorded
   produce_blocks(10);

   // Verify all producers have equal votes
   fc::variant first_prod_info = get_producer_info(producer_names[0]);
   double first_votes = first_prod_info["total_votes"].as<double>();

   fc::variant last_prod_info = get_producer_info(producer_names[29]);
   double last_votes = last_prod_info["total_votes"].as<double>();

   ilog( "First producer votes: ${votes}", ("votes", first_votes) );
   ilog( "Last producer votes: ${votes}", ("votes", last_votes) );
   BOOST_REQUIRE_EQUAL(first_votes, last_votes); // All should have equal votes

   // Step 7: Trigger schedule update
   produce_blocks(250);

   // Step 8: Verify active producer selection - should only have 15 BPs
   auto producer_keys = control->head_block_state()->active_schedule.producers;

   ilog( "=== Active Producers (should be 15 only due to max_considered_producers) ===" );
   std::set<name> active_producers;
   for (size_t i = 0; i < producer_keys.size(); ++i) {
      active_producers.insert(producer_keys[i].producer_name);
      ilog( "${idx}: ${name}", ("idx", i)("name", producer_keys[i].producer_name) );
   }

   // Only 15 producers are considered -> only 15 active BPs
   BOOST_REQUIRE_EQUAL( 15, producer_keys.size() );

   // Step 9: Verify standby producer selection - should be ZERO
   auto standby_producers = get_standby_table();

   std::set<name> standby_producer_names;
   for (size_t i = 0; i < standby_producers.size() && standby_producers[i].is_active; ++i) {
      standby_producer_names.insert(standby_producers[i].owner);
      ilog( "${idx}: ${name} (active=${active})",
            ("idx", i)
            ("name", standby_producers[i].owner)
            ("active", standby_producers[i].is_active) );
   }

   // No producers qualify for standby since only 15 total are considered
   BOOST_REQUIRE_EQUAL( 0, standby_producer_names.size() );

   // Step 10: Verify that only first 15 producers (by name, since all have equal votes) are active
   // Since all producers have equal votes and scores, selection is deterministic by name order
   ilog( "=== Verifying First 15 Producers in Active ===" );
   for (size_t i = 0; i < 15; ++i) {
      name producer = producer_names[i];
      bool in_active = active_producers.find(producer) != active_producers.end();

      BOOST_REQUIRE(in_active);
      ilog( "✓ ${name} in ACTIVE (position ${pos})", ("name", producer)("pos", i) );
   }

   // Step 11: Verify remaining 15 producers are NOT considered
   ilog( "=== Verifying Last 15 Producers NOT Considered ===" );
   int not_considered = 0;
   for (size_t i = 15; i < producer_names.size(); ++i) {
      name producer = producer_names[i];
      bool in_active = active_producers.find(producer) != active_producers.end();

      if (!in_active) {
         not_considered++;
         ilog( "✓ ${name} NOT considered (beyond max_considered_producers limit)", ("name", producer) );
      } else {
         ilog( "✗ ${name} was considered - should not be!", ("name", producer) );
      }
   }

   // All 15 producers beyond the limit should not be considered
   BOOST_REQUIRE_EQUAL(not_considered, 15);

   ilog( "=== TEST SUMMARY ===" );
   ilog( "✓ max_considered_producers set to: ${max}", ("max", max_considered) );
   ilog( "✓ Active producers: ${active} (expected 15)", ("active", producer_keys.size()) );
   ilog( "✓ Standby producers: ${standby} (expected 0)", ("standby", standby_producer_names.size()) );
   ilog( "✓ Producers not considered: ${not_considered}/15", ("not_considered", not_considered) );
   ilog( "SUCCESS: max_considered_producers correctly limits BPs to 15!" );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_max_considered_producers_filters_standby_bps, eosio_weighted_producer_tester) try {
   // Test that max_considered_producers correctly limits producers for both active and standby
   // Setting max_considered_producers to 23 -> 21 active BPs, 2 standby BPs
   fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);

   // Step 1: Configure weighted voting system
   const name guilds_contract = GUILDS_OIG;
   const uint32_t scaling_factor = 1000; // 1.0x multiplier
   const uint32_t default_score = 1000; // 1.0x default

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setguildcont"_n, mvo()
      ("contract", guilds_contract)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpscale"_n, mvo()
      ("scaling_factor", scaling_factor)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpdefscore"_n, mvo()
      ("default_score", default_score)
   ));
   produce_blocks(1);

   // Get the code hash of the deployed guilds contract
   auto wasm = contracts::util::guild_test_wasm();
   auto hash_result = fc::sha256::hash(reinterpret_cast<const char*>(wasm.data()), wasm.size());
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "addguildhash"_n, mvo()
      ("hash", hash_result.str())
   ));
   produce_blocks(1);

   // Step 2: Configure standby system
   const uint32_t standby_slots = 5;
   const uint32_t standby_ratio = 5000; // 0.5 weight

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setsbslot"_n, mvo()
      ("num_slots", standby_slots)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setsbratio"_n, mvo()
      ("ratio", standby_ratio)
   ));
   produce_blocks(1);

   ilog( "=== Standby Configuration ===" );
   fc::variant state4 = get_global_state4();
   ilog( "num_standby_slots: ${slots}", ("slots", state4["num_standby_slots"].as<uint32_t>()) );
   ilog( "standby_slot_weight: ${weight}", ("weight", state4["standby_slot_weight"].as<uint32_t>()) );

   // Step 3: Set max_considered_producers to 23 (21 active + 2 standby)
   const uint32_t max_considered = 23;
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setmaxprod"_n, mvo()
      ("max_considered_producers", max_considered)
   ));
   produce_blocks(1);

   // Verify max_considered_producers was set
   auto state6 = get_global_state6();
   uint32_t actual_max_considered = state6["max_considered_producers"].as<uint32_t>();
   BOOST_REQUIRE_EQUAL(actual_max_considered, max_considered);
   ilog( "✓ max_considered_producers confirmed: ${max}", ("max", actual_max_considered) );

   // Step 4: Create voters with equal stake
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const asset initial_supply = core_sym::from_string("100000000.0000");
   const asset stake_amount = core_sym::from_string("30000000.0000");

   // Create voters - all with same stake so all producers get equal votes
   const std::vector<account_name> voters = { "voter1111111"_n, "voter2222222"_n, "voter3333333"_n, "voter4444444"_n };
   for (const auto& v: voters) {
      create_account_with_resources( v, config::system_account_name, core_sym::from_string("1.0000"), false, net, cpu );
      transfer( config::system_account_name, v, initial_supply, config::system_account_name );
      BOOST_REQUIRE_EQUAL(success(), stake(v, stake_amount, stake_amount) );
   }

   // Step 5: Create 30 producers
   std::vector<account_name> producer_names;
   const std::string root("prod111111");
   const std::vector<std::string> suffixes = {
      "aa", "ab", "ac", "ad", "ae", "af", "ag", "ah", "ai", "aj",  // 10
      "ak", "al", "am", "an", "ao", "ap", "aq", "ar", "as", "at",  // 20
      "au", "av", "aw", "ax", "ay", "az", "ba", "bb", "bc", "bd"   // 30
   };

   for (const auto& suffix : suffixes) {
      producer_names.emplace_back(root + suffix);
   }

   setup_producer_accounts(producer_names);
   for (const auto& p: producer_names) {
      BOOST_REQUIRE_EQUAL( success(), regproducer(p) );
      produce_blocks(1);
   }

   ilog( "=== Created ${count} producers ===" , ("count", producer_names.size()));

   // Set all producers with same guild score
   for (size_t i = 0; i < 30; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 1000)
      ));
   }

   // Step 6: All voters vote for ALL producers (equal votes for all)
   produce_block( fc::hours(24) );

   for (const auto& v: voters) {
      BOOST_REQUIRE_EQUAL(success(), vote(v, producer_names));
   }

   // Wait for votes to be recorded
   produce_blocks(10);

   // Verify all producers have equal votes
   fc::variant first_prod_info = get_producer_info(producer_names[0]);
   double first_votes = first_prod_info["total_votes"].as<double>();

   fc::variant last_prod_info = get_producer_info(producer_names[29]);
   double last_votes = last_prod_info["total_votes"].as<double>();

   ilog( "First producer votes: ${votes}", ("votes", first_votes) );
   ilog( "Last producer votes: ${votes}", ("votes", last_votes) );
   BOOST_REQUIRE_EQUAL(first_votes, last_votes); // All should have equal votes

   // Step 7: Trigger schedule update
   produce_blocks(250);

   // Step 8: Verify active producer selection - should have 21 BPs
   auto producer_keys = control->head_block_state()->active_schedule.producers;

   ilog( "=== Active Producers (should be 21) ===" );
   std::set<name> active_producers;
   for (size_t i = 0; i < producer_keys.size(); ++i) {
      active_producers.insert(producer_keys[i].producer_name);
      ilog( "${idx}: ${name}", ("idx", i)("name", producer_keys[i].producer_name) );
   }

   // First 21 of the considered producers become active
   BOOST_REQUIRE_EQUAL( 21, producer_keys.size() );

   // Step 9: Verify standby producer selection - should be 2
   auto standby_producers = get_standby_table();

   std::set<name> standby_producer_names;
   for (size_t i = 0; i < standby_producers.size() && standby_producers[i].is_active; ++i) {
      standby_producer_names.insert(standby_producers[i].owner);
      ilog( "${idx}: ${name} (active=${active})",
            ("idx", i)
            ("name", standby_producers[i].owner)
            ("active", standby_producers[i].is_active) );
   }

   // Next 2 of the considered producers become standby
   BOOST_REQUIRE_EQUAL( 2, standby_producer_names.size() );

   // Step 10: Verify that first 21 producers are active
   ilog( "=== Verifying First 21 Producers in Active ===" );
   for (size_t i = 0; i < 21; ++i) {
      name producer = producer_names[i];
      bool in_active = active_producers.find(producer) != active_producers.end();

      BOOST_REQUIRE(in_active);
      ilog( "✓ ${name} in ACTIVE (position ${pos})", ("name", producer)("pos", i) );
   }

   // Step 11: Verify that next 2 producers (21-22) are standby
   ilog( "=== Verifying Next 2 Producers in Standby ===" );
   for (size_t i = 21; i < 23; ++i) {
      name producer = producer_names[i];
      bool in_standby = standby_producer_names.find(producer) != standby_producer_names.end();

      BOOST_REQUIRE(in_standby);
      ilog( "✓ ${name} in STANDBY (position ${pos})", ("name", producer)("pos", i) );
   }

   // Step 12: Verify remaining producers (23-29) are NOT considered
   ilog( "=== Verifying Last 7 Producers NOT Considered ===" );
   int not_considered = 0;
   for (size_t i = 23; i < producer_names.size(); ++i) {
      name producer = producer_names[i];
      bool in_active = active_producers.find(producer) != active_producers.end();
      bool in_standby = standby_producer_names.find(producer) != standby_producer_names.end();

      if (!in_active && !in_standby) {
         not_considered++;
         ilog( "✓ ${name} NOT considered (beyond max_considered_producers limit)", ("name", producer) );
      } else {
         ilog( "✗ ${name} was considered - should not be!", ("name", producer) );
      }
   }

   // All 7 producers beyond the limit should not be considered
   BOOST_REQUIRE_EQUAL(not_considered, 7);

   ilog( "=== TEST SUMMARY ===" );
   ilog( "✓ max_considered_producers set to: ${max}", ("max", max_considered) );
   ilog( "✓ Active producers: ${active} (expected 21)", ("active", producer_keys.size()) );
   ilog( "✓ Standby producers: ${standby} (expected 2)", ("standby", standby_producer_names.size()) );
   ilog( "✓ Producers not considered: ${not_considered}/7", ("not_considered", not_considered) );
   ilog( "✓ No overlap between active and standby" );
   ilog( "SUCCESS: max_considered_producers correctly limits to 21 active + 2 standby BPs!" );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_weighted_producer_with_guilds_code_hash_match_one_of_many, eosio_weighted_producer_tester) try {
   // Test that weighted voting affects both active (top 21) and standby producer selection
   fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);

   // Step 1: Configure weighted voting system
   const name guilds_contract = GUILDS_OIG;
   const uint32_t scaling_factor = 1000; // 1.0x multiplier
   const uint32_t default_score = 1000; // 1.0x default

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setguildcont"_n, mvo()
      ("contract", guilds_contract)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpscale"_n, mvo()
      ("scaling_factor", scaling_factor)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpdefscore"_n, mvo()
      ("default_score", default_score)
   ));
   produce_blocks(1);

   // Get the code hash of the deployed guilds contract
   // Compute hash from the WASM code that was deployed
   auto wasm = contracts::util::guild_test_wasm();
   auto hash_result = fc::sha256::hash(reinterpret_cast<const char*>(wasm.data()), wasm.size());
   ilog( "Guilds contract hash: ${hash}", ("hash", hash_result.str()) );
   // Add the correct hash to approved list
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "addguildhash"_n, mvo()
      ("hash", hash_result.str())
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "addguildhash"_n, mvo()
      ("hash", "9b6d8a5e1761e682e62f6722b08defc8c6ddbd99f2422bab75f726565b0002fc")
   ));
   produce_blocks(1);

   ilog( "=== Weighted Voting Configuration ===" );
   fc::variant state5 = get_global_state6();
   ilog( "guilds_contract: ${guilds}", ("guilds", state5["guilds_contract"].as<name>()) );
   ilog( "bp_score_scaling_factor: ${scale}", ("scale", state5["bp_score_scaling_factor"].as<uint32_t>()) );
   ilog( "bp_default_score: ${default}", ("default", state5["bp_default_score"].as<uint32_t>()) );
   // Verify hash was added
   auto hash_list = state5["guilds_code_hashes"].get_array();
   BOOST_REQUIRE_EQUAL(hash_list.size(), 2);
   ilog( "✓ Correct hash added to approved list" );
   BOOST_REQUIRE_EQUAL(state5["enable_weighted_voting"].as<bool>(), true);

   // Step 2: Configure standby system
   const uint32_t standby_slots = 5;
   const uint32_t standby_ratio = 5000; // 0.5 weight

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setsbslot"_n, mvo()
      ("num_slots", standby_slots)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setsbratio"_n, mvo()
      ("ratio", standby_ratio)
   ));
   produce_blocks(1);

   ilog( "=== Standby Configuration ===" );
   fc::variant state4 = get_global_state4();
   ilog( "num_standby_slots: ${slots}", ("slots", state4["num_standby_slots"].as<uint32_t>()) );
   ilog( "standby_slot_weight: ${weight}", ("weight", state4["standby_slot_weight"].as<uint32_t>()) );

   // Step 3: Create voters with significant stake
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> voters = { "voter1111111"_n, "voter2222222"_n, "voter3333333"_n, "voter4444444"_n };
   for (const auto& v: voters) {
      create_account_with_resources( v, config::system_account_name, core_sym::from_string("1.0000"), false, net, cpu );
      transfer( config::system_account_name, v, core_sym::from_string("100000000.0000"), config::system_account_name );
      BOOST_REQUIRE_EQUAL(success(), stake(v, core_sym::from_string("30000000.0000"), core_sym::from_string("30000000.0000")) );
   }

   // Step 4: Create 30 producers to test both active and standby selection
   // We need: 21 active + 5 standby = 26 slots, plus extras to test exclusion
   // Use EOSIO-compliant names: 12 chars max, a-z and 1-5 only
   std::vector<account_name> producer_names;

   // Create prода111111aa through prod111111ad (30 producers)
   // Using pattern: prod111111aa, prod111111ab, ..., prod111111az,
   //                prod111111ba, prod111111bb, prod111111bc, prod111111bd
   const std::string root("prod111111");
   const std::vector<std::string> suffixes = {
      "aa", "ab", "ac", "ad", "ae", "af", "ag", "ah", "ai", "aj",  // 10
      "ak", "al", "am", "an", "ao", "ap", "aq", "ar", "as", "at",  // 20
      "au", "av", "aw", "ax", "ay", "az", "ba", "bb", "bc", "bd"   // 30
   };

   for (const auto& suffix : suffixes) {
      producer_names.emplace_back(root + suffix);
   }

   // Names are already sorted since we created them in alphabetical order
   setup_producer_accounts(producer_names);
   for (const auto& p: producer_names) {
      BOOST_REQUIRE_EQUAL( success(), regproducer(p) );
      produce_blocks(1);
   }

   ilog( "=== Created ${count} producers ===" , ("count", producer_names.size()));
   ilog( "First producer: ${first}, Last producer: ${last}",
         ("first", producer_names.front())("last", producer_names.back()) );

   // Step 5: Assign weighted scores to producers
   // We have: prod111111aa, prod111111ab, ..., prod111111bd (30 producers in alphabetical order)
   // Group 1 (indices 0-3): VERY LOW score 0.3x - should be EXCLUDED from all (4 producers: aa-ad)
   // Group 2 (indices 4-7): LOW score 0.8x - may be in standby or excluded (4 producers: ae-ah)
   // Group 3 (indices 8-21): MEDIUM score 1.5x - should be in active slots (14 producers: ai-av)
   // Group 4 (indices 22-29): HIGH score 2.5x - should be in active slots (8 producers: aw-bd)

   ilog( "=== Setting Producer Scores ===" );

   ilog( "Group 1 (indices 0-3, prod111111aa-ad): VERY LOW score 0.3x - should be EXCLUDED" );
   for (size_t i = 0; i < 4; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 300) // 0.3x - VERY LOW
      ));
      ilog( "  ${name}: score=300", ("name", producer_names[i]) );
   }

   ilog( "Group 2 (indices 4-7, prod111111ae-ah): LOW score 0.8x - may be standby or excluded" );
   for (size_t i = 4; i < 8; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 800) // 0.8x - LOW
      ));
      ilog( "  ${name}: score=800", ("name", producer_names[i]) );
   }

   ilog( "Group 3 (indices 8-21, prod111111ai-av): MEDIUM score 1.5x - should be in active" );
   for (size_t i = 8; i < 22; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 1500) // 1.5x - MEDIUM
      ));
   }

   ilog( "Group 4 (indices 22-29, prod111111aw-bd): HIGH score 2.5x - should be in active" );
   for (size_t i = 22; i < 30; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 2500) // 2.5x - HIGH
      ));
   }
   produce_blocks(1);

   // Step 6: All voters vote for ALL producers (equal raw votes)
   produce_block( fc::hours(24) );

   ilog( "=== Voting for all ${count} producers ===" , ("count", producer_names.size()));
   for (const auto& v: voters) {
      BOOST_REQUIRE_EQUAL(success(), vote(v, producer_names));
   }

   // Wait for schedule to update
   produce_blocks(250);

   // Step 7: Verify active producer selection (top 21)
   auto producer_keys = control->head_block_state()->active_schedule.producers;
   BOOST_REQUIRE_EQUAL( 21, producer_keys.size() );

   ilog( "=== Active Producers (Top 21 by weighted votes) ===" );
   std::set<name> active_producers;
   for (size_t i = 0; i < producer_keys.size(); ++i) {
      active_producers.insert(producer_keys[i].producer_name);
      ilog( "${idx}: ${name}", ("idx", i)("name", producer_keys[i].producer_name) );
   }

   // Step 8: Verify standby producer selection
   auto standby_producers = get_standby_table();
   ilog( "=== Standby Producers (Next ${count} by weighted votes) ===" , ("count", standby_producers.size()));

   std::set<name> standby_producer_names;
   for (size_t i = 0; i < standby_producers.size(); ++i) {
      standby_producer_names.insert(standby_producers[i].owner);
      ilog( "${idx}: ${name} (active=${active})",
            ("idx", i)
            ("name", standby_producers[i].owner)
            ("active", standby_producers[i].is_active) );
   }

   // Step 9: Verification - Check that weighted voting affected both lists

   // Verify VERY LOW score producers (prod111111aa-ad) are EXCLUDED from BOTH active and standby
   ilog( "=== Verifying VERY LOW Score Producers (prod111111aa-ad) Exclusion ===" );
   int excluded_from_both = 0;
   for (size_t i = 0; i < 4; ++i) {
      name producer = producer_names[i];
      bool in_active = active_producers.find(producer) != active_producers.end();
      bool in_standby = standby_producer_names.find(producer) != standby_producer_names.end();

      if (!in_active && !in_standby) {
         ilog( "✓ ${name} (score 0.3x) EXCLUDED from both - CORRECT!", ("name", producer) );
         excluded_from_both++;
      } else {
         ilog( "✗ ${name} (score 0.3x) found in ${where} - WRONG!",
               ("name", producer)
               ("where", in_active ? "active" : "standby") );
      }
   }
   BOOST_REQUIRE_EQUAL(excluded_from_both, 4); // All 4 very low score producers excluded

   // Verify HIGH score producers (prod111111aw-bd) are in ACTIVE (not standby)
   ilog( "=== Verifying HIGH Score Producers (prod111111aw-bd) in Active ===" );
   int high_score_in_active = 0;
   for (size_t i = 22; i < 30; ++i) {
      name producer = producer_names[i];
      bool in_active = active_producers.find(producer) != active_producers.end();

      if (in_active) {
         high_score_in_active++;
         ilog( "✓ ${name} (score 2.5x) in ACTIVE - CORRECT!", ("name", producer) );
      } else {
         ilog( "✗ ${name} (score 2.5x) NOT in active - checking standby...", ("name", producer) );
      }
   }

   // At least most high-score producers should be in active
   BOOST_REQUIRE(high_score_in_active >= 6); // At least 6 of 8 high-score producers in active

   // Verify standby count matches configuration
   BOOST_REQUIRE_EQUAL(standby_producers.size(), standby_slots);

   // Verify no overlap between active and standby
   ilog( "=== Verifying No Overlap Between Active and Standby ===" );
   int overlap_count = 0;
   for (const auto& sp : standby_producer_names) {
      if (active_producers.find(sp) != active_producers.end()) {
         ilog( "✗ ${name} found in BOTH active and standby!", ("name", sp) );
         overlap_count++;
      }
   }
   BOOST_REQUIRE_EQUAL(overlap_count, 0); // No producer should be in both lists

   // Step 10: Log detailed weighted votes for analysis
   ilog( "=== Producer Weighted Vote Details ===" );
   for (const auto& p : producer_names) {
      fc::variant producer_info = get_producer_info(p);
      double total_votes = producer_info["total_votes"].as<double>();

      guild guild_data = get_guild_table(p);
      uint32_t score = guild_data.score;
      double weighted_votes = total_votes * score / 1000.0;

      bool in_active = active_producers.find(p) != active_producers.end();
      bool in_standby = standby_producer_names.find(p) != standby_producer_names.end();
      std::string status = in_active ? "ACTIVE" : (in_standby ? "STANDBY" : "EXCLUDED");

      ilog( "${name}: votes=${votes}, score=${score}, weighted=${weighted}, status=${status}",
            ("name", p)
            ("votes", total_votes)
            ("score", score)
            ("weighted", weighted_votes)
            ("status", status) );
   }

   ilog( "=== TEST SUMMARY ===" );
   ilog( "✓ Active producers: ${active} (expected 21)", ("active", producer_keys.size()) );
   ilog( "✓ Standby producers: ${standby} (expected ${expected})",
         ("standby", standby_producers.size())
         ("expected", standby_slots) );
   ilog( "✓ Very low score producers excluded: ${excluded}/4", ("excluded", excluded_from_both) );
   ilog( "✓ High score producers in active: ${high}/${total}",
         ("high", high_score_in_active)
         ("total", 8) );
   ilog( "✓ No overlap between active and standby" );
   ilog( "SUCCESS: Weighted voting correctly affects both active and standby selection!" );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_guild_hash_empty_disabled, eosio_weighted_producer_tester) try {
   // Test that guilds weight is not effected if guild code hashes is empty
   fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);

   // Step 1: Configure weighted voting system
   const name guilds_contract = GUILDS_OIG;
   const uint32_t scaling_factor = 1000; // 1.0x multiplier
   const uint32_t default_score = 1000; // 1.0x default

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setguildcont"_n, mvo()
      ("contract", guilds_contract)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpscale"_n, mvo()
      ("scaling_factor", scaling_factor)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpdefscore"_n, mvo()
      ("default_score", default_score)
   ));
   produce_blocks(1);

   ilog( "=== Weighted Voting Configuration ===" );
   fc::variant state5 = get_global_state6();
   ilog( "guilds_contract: ${guilds}", ("guilds", state5["guilds_contract"].as<name>()) );
   ilog( "bp_score_scaling_factor: ${scale}", ("scale", state5["bp_score_scaling_factor"].as<uint32_t>()) );
   ilog( "bp_default_score: ${default}", ("default", state5["bp_default_score"].as<uint32_t>()) );
   // Verify guilds hashes empty
   auto hash_list = state5["guilds_code_hashes"].get_array();
   BOOST_REQUIRE_EQUAL(hash_list.size(), 0);
   ilog( "✓ Guilds hash is empty" );

   // Step 2: Configure standby system
   const uint32_t standby_slots = 5;
   const uint32_t standby_ratio = 5000; // 0.5 weight

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setsbslot"_n, mvo()
      ("num_slots", standby_slots)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setsbratio"_n, mvo()
      ("ratio", standby_ratio)
   ));
   produce_blocks(1);

   ilog( "=== Standby Configuration ===" );
   fc::variant state4 = get_global_state4();
   ilog( "num_standby_slots: ${slots}", ("slots", state4["num_standby_slots"].as<uint32_t>()) );
   ilog( "standby_slot_weight: ${weight}", ("weight", state4["standby_slot_weight"].as<uint32_t>()) );

   // Step 3: Create voters with significant stake
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> voters = { "voter1111111"_n, "voter2222222"_n, "voter3333333"_n, "voter4444444"_n };
   for (const auto& v: voters) {
      create_account_with_resources( v, config::system_account_name, core_sym::from_string("1.0000"), false, net, cpu );
      transfer( config::system_account_name, v, core_sym::from_string("100000000.0000"), config::system_account_name );
      BOOST_REQUIRE_EQUAL(success(), stake(v, core_sym::from_string("30000000.0000"), core_sym::from_string("30000000.0000")) );
   }

   // Step 4: Create 30 producers to test both active and standby selection
   // We need: 21 active + 5 standby = 26 slots, plus extras to test exclusion
   // Use EOSIO-compliant names: 12 chars max, a-z and 1-5 only
   std::vector<account_name> producer_names;

   // Create prода111111aa through prod111111ad (30 producers)
   // Using pattern: prod111111aa, prod111111ab, ..., prod111111az,
   //                prod111111ba, prod111111bb, prod111111bc, prod111111bd
   const std::string root("prod111111");
   const std::vector<std::string> suffixes = {
      "aa", "ab", "ac", "ad", "ae", "af", "ag", "ah", "ai", "aj",  // 10
      "ak", "al", "am", "an", "ao", "ap", "aq", "ar", "as", "at",  // 20
      "au", "av", "aw", "ax", "ay", "az", "ba", "bb", "bc", "bd"   // 30
   };

   for (const auto& suffix : suffixes) {
      producer_names.emplace_back(root + suffix);
   }

   // Names are already sorted since we created them in alphabetical order
   setup_producer_accounts(producer_names);
   int location = 0;
   for (const auto& p: producer_names) {
      BOOST_REQUIRE_EQUAL( success(), regproducer(p, 1, location) );
      produce_blocks(1);
      location++;
   }

   ilog( "=== Created ${count} producers ===" , ("count", producer_names.size()));
   ilog( "First producer: ${first}, Last producer: ${last}",
         ("first", producer_names.front())("last", producer_names.back()) );

   // Step 5: Assign weighted scores to producers
   // We have: prod111111aa, prod111111ab, ..., prod111111bd (30 producers in alphabetical order)
   // Group 1 (indices 0-3): VERY LOW score 0.3x - should be EXCLUDED from all (4 producers: aa-ad)
   // Group 2 (indices 4-7): LOW score 0.8x - may be in standby or excluded (4 producers: ae-ah)
   // Group 3 (indices 8-21): MEDIUM score 1.5x - should be in active slots (14 producers: ai-av)
   // Group 4 (indices 22-29): HIGH score 2.5x - should be in active slots (8 producers: aw-bd)

   ilog( "=== Setting Producer Scores ===" );

   ilog( "Group 1 (indices 0-3, prod111111aa-ad): VERY LOW score 0.3x - should be EXCLUDED" );
   for (size_t i = 0; i < 4; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 300) // 0.3x - VERY LOW
      ));
      ilog( "  ${name}: score=300", ("name", producer_names[i]) );
   }

   ilog( "Group 2 (indices 4-7, prod111111ae-ah): LOW score 0.8x - may be standby or excluded" );
   for (size_t i = 4; i < 8; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 800) // 0.8x - LOW
      ));
      ilog( "  ${name}: score=800", ("name", producer_names[i]) );
   }

   ilog( "Group 3 (indices 8-21, prod111111ai-av): MEDIUM score 1.5x - should be in active" );
   for (size_t i = 8; i < 22; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 1500) // 1.5x - MEDIUM
      ));
   }

   ilog( "Group 4 (indices 22-29, prod111111aw-bd): HIGH score 2.5x - should be in active" );
   for (size_t i = 22; i < 30; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 2500) // 2.5x - HIGH
      ));
   }
   produce_blocks(1);

   // Step 6: All voters vote for ALL producers (equal raw votes)
   produce_block( fc::hours(24) );

   ilog( "=== Voting for all ${count} producers ===" , ("count", producer_names.size()));
   for (const auto& v: voters) {
      BOOST_REQUIRE_EQUAL(success(), vote(v, producer_names));
   }

   // Wait for schedule to update
   produce_blocks(250);

   // Step 7: Verify active producer selection (top 21)
   auto producer_keys = control->head_block_state()->active_schedule.producers;
   BOOST_REQUIRE_EQUAL( 21, producer_keys.size() );

   ilog( "=== Active Producers (Top 21 by weighted votes) ===" );
   std::set<name> active_producers;
   for (size_t i = 0; i < producer_keys.size(); ++i) {
      active_producers.insert(producer_keys[i].producer_name);
      ilog( "${idx}: ${name}", ("idx", i)("name", producer_keys[i].producer_name) );
   }

   // Step 8: Verify standby producer selection
   auto standby_producers = get_standby_table();
   ilog( "=== Standby Producers (Next ${count} by weighted votes) ===" , ("count", standby_producers.size()));

   std::set<name> standby_producer_names;
   for (size_t i = 0; i < standby_producers.size(); ++i) {
      standby_producer_names.insert(standby_producers[i].owner);
      ilog( "${idx}: ${name} (active=${active})",
            ("idx", i)
            ("name", standby_producers[i].owner)
            ("active", standby_producers[i].is_active) );
   }

   // Step 9: Verification - Because guild weight is not effected then all producers has the same weight, Check that it select by sorting location

   // Verify first 21 producers are in ACTIVE (not standby)
   ilog( "=== Verifying HIGH Location Producers (prod111111aw-bd) in Active ===" );
   int high_location_in_active = 0;
   for (size_t i = 0; i < 21; ++i) {
      name producer = producer_names[i];
      bool in_active = active_producers.find(producer) != active_producers.end();

      if (in_active) {
         high_location_in_active++;
         ilog( "✓ ${name} in ACTIVE - CORRECT!", ("name", producer) );
      } else {
         ilog( "✗ ${name} NOT in active - checking standby...", ("name", producer) );
      }
   }
   BOOST_REQUIRE_EQUAL(high_location_in_active, 21);

   // Verify next 5 producers are standby
   ilog( "=== Verifying next five producers is standby ===" );
   int next_five_standby = 0;
   for (size_t i = 21; i < 26; ++i) {
      name producer = producer_names[i];
      bool in_standby = standby_producer_names.find(producer) != standby_producer_names.end();

      if (in_standby) {
         ilog( "✓ ${name} standby - CORRECT!", ("name", producer) );
         next_five_standby++;
      } else {
         ilog( "✗ ${name} standby found in ${where} - WRONG!",
               ("name", producer));
      }
   }
   BOOST_REQUIRE_EQUAL(next_five_standby, 5);

   // Verify remaining is excluded
   ilog( "=== Verifying remaining bp is excluded ===" );
   int excluded_bp = 0;
   for (size_t i = 26; i < 30; ++i) {
      name producer = producer_names[i];
      bool in_standby = standby_producer_names.find(producer) != standby_producer_names.end();
      bool in_active = active_producers.find(producer) != active_producers.end();

      if (!in_standby && !in_active) {
         ilog( "✓ ${name} remaining is excluded - CORRECT!", ("name", producer) );
         excluded_bp++;
      } else {
         ilog( "✗ ${name} remaining is excluded - WRONG!",
               ("name", producer));
      }
   }
   BOOST_REQUIRE_EQUAL(excluded_bp, 4); // All 4 very low score producers excluded

   // Verify standby count matches configuration
   BOOST_REQUIRE_EQUAL(standby_producers.size(), standby_slots);

   // Verify no overlap between active and standby
   ilog( "=== Verifying No Overlap Between Active and Standby ===" );
   int overlap_count = 0;
   for (const auto& sp : standby_producer_names) {
      if (active_producers.find(sp) != active_producers.end()) {
         ilog( "✗ ${name} found in BOTH active and standby!", ("name", sp) );
         overlap_count++;
      }
   }
   BOOST_REQUIRE_EQUAL(overlap_count, 0); // No producer should be in both lists
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_admin_disabled_guild_weight, eosio_weighted_producer_tester) try {
   // Test that weighted voting affects both active (top 21) and standby producer selection
   fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);

   // Step 1: Configure weighted voting system
   const name guilds_contract = GUILDS_OIG;
   const uint32_t scaling_factor = 1000; // 1.0x multiplier
   const uint32_t default_score = 1000; // 1.0x default

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setguildcont"_n, mvo()
      ("contract", guilds_contract)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpscale"_n, mvo()
      ("scaling_factor", scaling_factor)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpdefscore"_n, mvo()
      ("default_score", default_score)
   ));

   // disable weight voting
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setenablewv"_n, mvo()
         ("enable", false)
   ));
   produce_blocks(1);

   // Get the code hash of the deployed guilds contract
   // Compute hash from the WASM code that was deployed
   auto wasm = contracts::util::guild_test_wasm();
   auto hash_result = fc::sha256::hash(reinterpret_cast<const char*>(wasm.data()), wasm.size());
   ilog( "Guilds contract hash: ${hash}", ("hash", hash_result.str()) );
   // Add the correct hash to approved list
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "addguildhash"_n, mvo()
      ("hash", hash_result.str())
   ));
   produce_blocks(1);

   ilog( "=== Weighted Voting Configuration ===" );
   fc::variant state5 = get_global_state6();
   ilog( "guilds_contract: ${guilds}", ("guilds", state5["guilds_contract"].as<name>()) );
   ilog( "bp_score_scaling_factor: ${scale}", ("scale", state5["bp_score_scaling_factor"].as<uint32_t>()) );
   ilog( "bp_default_score: ${default}", ("default", state5["bp_default_score"].as<uint32_t>()) );
   // Verify guilds hashes empty
   auto hash_list = state5["guilds_code_hashes"].get_array();
   BOOST_REQUIRE_EQUAL(hash_list.size(), 1);
   ilog( "✓ Guilds hash set" );
   BOOST_REQUIRE_EQUAL(state5["enable_weighted_voting"].as<bool>(), false);

   // Step 2: Configure standby system
   const uint32_t standby_slots = 5;
   const uint32_t standby_ratio = 5000; // 0.5 weight

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setsbslot"_n, mvo()
      ("num_slots", standby_slots)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setsbratio"_n, mvo()
      ("ratio", standby_ratio)
   ));
   produce_blocks(1);

   ilog( "=== Standby Configuration ===" );
   fc::variant state4 = get_global_state4();
   ilog( "num_standby_slots: ${slots}", ("slots", state4["num_standby_slots"].as<uint32_t>()) );
   ilog( "standby_slot_weight: ${weight}", ("weight", state4["standby_slot_weight"].as<uint32_t>()) );

   // Step 3: Create voters with significant stake
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> voters = { "voter1111111"_n, "voter2222222"_n, "voter3333333"_n, "voter4444444"_n };
   for (const auto& v: voters) {
      create_account_with_resources( v, config::system_account_name, core_sym::from_string("1.0000"), false, net, cpu );
      transfer( config::system_account_name, v, core_sym::from_string("100000000.0000"), config::system_account_name );
      BOOST_REQUIRE_EQUAL(success(), stake(v, core_sym::from_string("30000000.0000"), core_sym::from_string("30000000.0000")) );
   }

   // Step 4: Create 30 producers to test both active and standby selection
   // We need: 21 active + 5 standby = 26 slots, plus extras to test exclusion
   // Use EOSIO-compliant names: 12 chars max, a-z and 1-5 only
   std::vector<account_name> producer_names;

   // Create prода111111aa through prod111111ad (30 producers)
   // Using pattern: prod111111aa, prod111111ab, ..., prod111111az,
   //                prod111111ba, prod111111bb, prod111111bc, prod111111bd
   const std::string root("prod111111");
   const std::vector<std::string> suffixes = {
      "aa", "ab", "ac", "ad", "ae", "af", "ag", "ah", "ai", "aj",  // 10
      "ak", "al", "am", "an", "ao", "ap", "aq", "ar", "as", "at",  // 20
      "au", "av", "aw", "ax", "ay", "az", "ba", "bb", "bc", "bd"   // 30
   };

   for (const auto& suffix : suffixes) {
      producer_names.emplace_back(root + suffix);
   }

   // Names are already sorted since we created them in alphabetical order
   setup_producer_accounts(producer_names);
   int location = 0;
   for (const auto& p: producer_names) {
      BOOST_REQUIRE_EQUAL( success(), regproducer(p, 1, location) );
      produce_blocks(1);
      location++;
   }

   ilog( "=== Created ${count} producers ===" , ("count", producer_names.size()));
   ilog( "First producer: ${first}, Last producer: ${last}",
         ("first", producer_names.front())("last", producer_names.back()) );

   // Step 5: Assign weighted scores to producers
   // We have: prod111111aa, prod111111ab, ..., prod111111bd (30 producers in alphabetical order)
   // Group 1 (indices 0-3): VERY LOW score 0.3x - should be EXCLUDED from all (4 producers: aa-ad)
   // Group 2 (indices 4-7): LOW score 0.8x - may be in standby or excluded (4 producers: ae-ah)
   // Group 3 (indices 8-21): MEDIUM score 1.5x - should be in active slots (14 producers: ai-av)
   // Group 4 (indices 22-29): HIGH score 2.5x - should be in active slots (8 producers: aw-bd)

   ilog( "=== Setting Producer Scores ===" );

   ilog( "Group 1 (indices 0-3, prod111111aa-ad): VERY LOW score 0.3x - should be EXCLUDED" );
   for (size_t i = 0; i < 4; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 300) // 0.3x - VERY LOW
      ));
      ilog( "  ${name}: score=300", ("name", producer_names[i]) );
   }

   ilog( "Group 2 (indices 4-7, prod111111ae-ah): LOW score 0.8x - may be standby or excluded" );
   for (size_t i = 4; i < 8; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 800) // 0.8x - LOW
      ));
      ilog( "  ${name}: score=800", ("name", producer_names[i]) );
   }

   ilog( "Group 3 (indices 8-21, prod111111ai-av): MEDIUM score 1.5x - should be in active" );
   for (size_t i = 8; i < 22; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 1500) // 1.5x - MEDIUM
      ));
   }

   ilog( "Group 4 (indices 22-29, prod111111aw-bd): HIGH score 2.5x - should be in active" );
   for (size_t i = 22; i < 30; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 2500) // 2.5x - HIGH
      ));
   }
   produce_blocks(1);

   // Step 6: All voters vote for ALL producers (equal raw votes)
   produce_block( fc::hours(24) );

   ilog( "=== Voting for all ${count} producers ===" , ("count", producer_names.size()));
   for (const auto& v: voters) {
      BOOST_REQUIRE_EQUAL(success(), vote(v, producer_names));
   }

   // Wait for schedule to update
   produce_blocks(250);

   // Step 7: Verify active producer selection (top 21)
   auto producer_keys = control->head_block_state()->active_schedule.producers;
   BOOST_REQUIRE_EQUAL( 21, producer_keys.size() );

   ilog( "=== Active Producers (Top 21 by weighted votes) ===" );
   std::set<name> active_producers;
   for (size_t i = 0; i < producer_keys.size(); ++i) {
      active_producers.insert(producer_keys[i].producer_name);
      ilog( "${idx}: ${name}", ("idx", i)("name", producer_keys[i].producer_name) );
   }

   // Step 8: Verify standby producer selection
   auto standby_producers = get_standby_table();
   ilog( "=== Standby Producers (Next ${count} by weighted votes) ===" , ("count", standby_producers.size()));

   std::set<name> standby_producer_names;
   for (size_t i = 0; i < standby_producers.size(); ++i) {
      standby_producer_names.insert(standby_producers[i].owner);
      ilog( "${idx}: ${name} (active=${active})",
            ("idx", i)
            ("name", standby_producers[i].owner)
            ("active", standby_producers[i].is_active) );
   }

   // Step 9: Verification - Because guild weight is not effected then all producers has the same weight, Check that it select by sorting location

   // Verify first 21 producers are in ACTIVE (not standby)
   ilog( "=== Verifying HIGH Location Producers (prod111111aw-bd) in Active ===" );
   int high_location_in_active = 0;
   for (size_t i = 0; i < 21; ++i) {
      name producer = producer_names[i];
      bool in_active = active_producers.find(producer) != active_producers.end();

      if (in_active) {
         high_location_in_active++;
         ilog( "✓ ${name} in ACTIVE - CORRECT!", ("name", producer) );
      } else {
         ilog( "✗ ${name} NOT in active - checking standby...", ("name", producer) );
      }
   }
   BOOST_REQUIRE_EQUAL(high_location_in_active, 21);

   // Verify next 5 producers are standby
   ilog( "=== Verifying next five producers is standby ===" );
   int next_five_standby = 0;
   for (size_t i = 21; i < 26; ++i) {
      name producer = producer_names[i];
      bool in_standby = standby_producer_names.find(producer) != standby_producer_names.end();

      if (in_standby) {
         ilog( "✓ ${name} standby - CORRECT!", ("name", producer) );
         next_five_standby++;
      } else {
         ilog( "✗ ${name} standby found in ${where} - WRONG!",
               ("name", producer));
      }
   }
   BOOST_REQUIRE_EQUAL(next_five_standby, 5);

   // Verify remaining is excluded
   ilog( "=== Verifying remaining bp is excluded ===" );
   int excluded_bp = 0;
   for (size_t i = 26; i < 30; ++i) {
      name producer = producer_names[i];
      bool in_standby = standby_producer_names.find(producer) != standby_producer_names.end();
      bool in_active = active_producers.find(producer) != active_producers.end();

      if (!in_standby && !in_active) {
         ilog( "✓ ${name} remaining is excluded - CORRECT!", ("name", producer) );
         excluded_bp++;
      } else {
         ilog( "✗ ${name} remaining is excluded - WRONG!",
               ("name", producer));
      }
   }
   BOOST_REQUIRE_EQUAL(excluded_bp, 4); // All 4 very low score producers excluded

   // Verify standby count matches configuration
   BOOST_REQUIRE_EQUAL(standby_producers.size(), standby_slots);

   // Verify no overlap between active and standby
   ilog( "=== Verifying No Overlap Between Active and Standby ===" );
   int overlap_count = 0;
   for (const auto& sp : standby_producer_names) {
      if (active_producers.find(sp) != active_producers.end()) {
         ilog( "✗ ${name} found in BOTH active and standby!", ("name", sp) );
         overlap_count++;
      }
   }
   BOOST_REQUIRE_EQUAL(overlap_count, 0); // No producer should be in both lists
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_verify_hash_not_match, eosio_weighted_producer_tester) try {
   // Test that weighted voting affects both active (top 21) and standby producer selection
   fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);

   // Step 1: Configure weighted voting system
   const name guilds_contract = GUILDS_OIG;
   const uint32_t scaling_factor = 1000; // 1.0x multiplier
   const uint32_t default_score = 1000; // 1.0x default

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setguildcont"_n, mvo()
      ("contract", guilds_contract)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpscale"_n, mvo()
      ("scaling_factor", scaling_factor)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpdefscore"_n, mvo()
      ("default_score", default_score)
   ));

   produce_blocks(1);

   // set arbitrary guild hash
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "addguildhash"_n, mvo()
      ("hash", "9b6d8a5e1761e682e62f6722b08defc8c6ddbd99f2422bab75f726565b0002fc")
   ));
   produce_blocks(1);

   ilog( "=== Weighted Voting Configuration ===" );
   fc::variant state5 = get_global_state6();
   ilog( "guilds_contract: ${guilds}", ("guilds", state5["guilds_contract"].as<name>()) );
   ilog( "bp_score_scaling_factor: ${scale}", ("scale", state5["bp_score_scaling_factor"].as<uint32_t>()) );
   ilog( "bp_default_score: ${default}", ("default", state5["bp_default_score"].as<uint32_t>()) );
   // Verify guilds hashes empty
   auto hash_list = state5["guilds_code_hashes"].get_array();
   BOOST_REQUIRE_EQUAL(hash_list.size(), 1);
   ilog( "✓ Guilds hash set" );
   BOOST_REQUIRE_EQUAL(state5["enable_weighted_voting"].as<bool>(), true);

   // Step 2: Configure standby system
   const uint32_t standby_slots = 5;
   const uint32_t standby_ratio = 5000; // 0.5 weight

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setsbslot"_n, mvo()
      ("num_slots", standby_slots)
   ));
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setsbratio"_n, mvo()
      ("ratio", standby_ratio)
   ));
   produce_blocks(1);

   ilog( "=== Standby Configuration ===" );
   fc::variant state4 = get_global_state4();
   ilog( "num_standby_slots: ${slots}", ("slots", state4["num_standby_slots"].as<uint32_t>()) );
   ilog( "standby_slot_weight: ${weight}", ("weight", state4["standby_slot_weight"].as<uint32_t>()) );

   // Step 3: Create voters with significant stake
   const asset net = core_sym::from_string("80.0000");
   const asset cpu = core_sym::from_string("80.0000");
   const std::vector<account_name> voters = { "voter1111111"_n, "voter2222222"_n, "voter3333333"_n, "voter4444444"_n };
   for (const auto& v: voters) {
      create_account_with_resources( v, config::system_account_name, core_sym::from_string("1.0000"), false, net, cpu );
      transfer( config::system_account_name, v, core_sym::from_string("100000000.0000"), config::system_account_name );
      BOOST_REQUIRE_EQUAL(success(), stake(v, core_sym::from_string("30000000.0000"), core_sym::from_string("30000000.0000")) );
   }

   // Step 4: Create 30 producers to test both active and standby selection
   // We need: 21 active + 5 standby = 26 slots, plus extras to test exclusion
   // Use EOSIO-compliant names: 12 chars max, a-z and 1-5 only
   std::vector<account_name> producer_names;

   // Create prода111111aa through prod111111ad (30 producers)
   // Using pattern: prod111111aa, prod111111ab, ..., prod111111az,
   //                prod111111ba, prod111111bb, prod111111bc, prod111111bd
   const std::string root("prod111111");
   const std::vector<std::string> suffixes = {
      "aa", "ab", "ac", "ad", "ae", "af", "ag", "ah", "ai", "aj",  // 10
      "ak", "al", "am", "an", "ao", "ap", "aq", "ar", "as", "at",  // 20
      "au", "av", "aw", "ax", "ay", "az", "ba", "bb", "bc", "bd"   // 30
   };

   for (const auto& suffix : suffixes) {
      producer_names.emplace_back(root + suffix);
   }

   // Names are already sorted since we created them in alphabetical order
   setup_producer_accounts(producer_names);
   int location = 0;
   for (const auto& p: producer_names) {
      BOOST_REQUIRE_EQUAL( success(), regproducer(p, 1, location) );
      produce_blocks(1);
      location++;
   }

   ilog( "=== Created ${count} producers ===" , ("count", producer_names.size()));
   ilog( "First producer: ${first}, Last producer: ${last}",
         ("first", producer_names.front())("last", producer_names.back()) );

   // Step 5: Assign weighted scores to producers
   // We have: prod111111aa, prod111111ab, ..., prod111111bd (30 producers in alphabetical order)
   // Group 1 (indices 0-3): VERY LOW score 0.3x - should be EXCLUDED from all (4 producers: aa-ad)
   // Group 2 (indices 4-7): LOW score 0.8x - may be in standby or excluded (4 producers: ae-ah)
   // Group 3 (indices 8-21): MEDIUM score 1.5x - should be in active slots (14 producers: ai-av)
   // Group 4 (indices 22-29): HIGH score 2.5x - should be in active slots (8 producers: aw-bd)

   ilog( "=== Setting Producer Scores ===" );

   ilog( "Group 1 (indices 0-3, prod111111aa-ad): VERY LOW score 0.3x - should be EXCLUDED" );
   for (size_t i = 0; i < 4; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 300) // 0.3x - VERY LOW
      ));
      ilog( "  ${name}: score=300", ("name", producer_names[i]) );
   }

   ilog( "Group 2 (indices 4-7, prod111111ae-ah): LOW score 0.8x - may be standby or excluded" );
   for (size_t i = 4; i < 8; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 800) // 0.8x - LOW
      ));
      ilog( "  ${name}: score=800", ("name", producer_names[i]) );
   }

   ilog( "Group 3 (indices 8-21, prod111111ai-av): MEDIUM score 1.5x - should be in active" );
   for (size_t i = 8; i < 22; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 1500) // 1.5x - MEDIUM
      ));
   }

   ilog( "Group 4 (indices 22-29, prod111111aw-bd): HIGH score 2.5x - should be in active" );
   for (size_t i = 22; i < 30; ++i) {
      BOOST_REQUIRE_EQUAL(success(), push_guild_action(GUILDS_OIG, "insertguild"_n, mvo()
         ("producer", producer_names[i])
         ("score", 2500) // 2.5x - HIGH
      ));
   }
   produce_blocks(1);

   // Step 6: All voters vote for ALL producers (equal raw votes)
   produce_block( fc::hours(24) );

   ilog( "=== Voting for all ${count} producers ===" , ("count", producer_names.size()));
   for (const auto& v: voters) {
      BOOST_REQUIRE_EQUAL(success(), vote(v, producer_names));
   }

   // Wait for schedule to update
   produce_blocks(250);

   // Step 7: Verify active producer selection (top 21)
   auto producer_keys = control->head_block_state()->active_schedule.producers;
   BOOST_REQUIRE_EQUAL( 21, producer_keys.size() );

   ilog( "=== Active Producers (Top 21 by weighted votes) ===" );
   std::set<name> active_producers;
   for (size_t i = 0; i < producer_keys.size(); ++i) {
      active_producers.insert(producer_keys[i].producer_name);
      ilog( "${idx}: ${name}", ("idx", i)("name", producer_keys[i].producer_name) );
   }

   // Step 8: Verify standby producer selection
   auto standby_producers = get_standby_table();
   ilog( "=== Standby Producers (Next ${count} by weighted votes) ===" , ("count", standby_producers.size()));

   std::set<name> standby_producer_names;
   for (size_t i = 0; i < standby_producers.size(); ++i) {
      standby_producer_names.insert(standby_producers[i].owner);
      ilog( "${idx}: ${name} (active=${active})",
            ("idx", i)
            ("name", standby_producers[i].owner)
            ("active", standby_producers[i].is_active) );
   }

   // Step 9: Verification - Because guild weight is not effected then all producers has the same weight, Check that it select by sorting location

   // Verify first 21 producers are in ACTIVE (not standby)
   ilog( "=== Verifying HIGH Location Producers (prod111111aw-bd) in Active ===" );
   int high_location_in_active = 0;
   for (size_t i = 0; i < 21; ++i) {
      name producer = producer_names[i];
      bool in_active = active_producers.find(producer) != active_producers.end();

      if (in_active) {
         high_location_in_active++;
         ilog( "✓ ${name} in ACTIVE - CORRECT!", ("name", producer) );
      } else {
         ilog( "✗ ${name} NOT in active - checking standby...", ("name", producer) );
      }
   }
   BOOST_REQUIRE_EQUAL(high_location_in_active, 21);

   // Verify next 5 producers are standby
   ilog( "=== Verifying next five producers is standby ===" );
   int next_five_standby = 0;
   for (size_t i = 21; i < 26; ++i) {
      name producer = producer_names[i];
      bool in_standby = standby_producer_names.find(producer) != standby_producer_names.end();

      if (in_standby) {
         ilog( "✓ ${name} standby - CORRECT!", ("name", producer) );
         next_five_standby++;
      } else {
         ilog( "✗ ${name} standby found in ${where} - WRONG!",
               ("name", producer));
      }
   }
   BOOST_REQUIRE_EQUAL(next_five_standby, 5);

   // Verify remaining is excluded
   ilog( "=== Verifying remaining bp is excluded ===" );
   int excluded_bp = 0;
   for (size_t i = 26; i < 30; ++i) {
      name producer = producer_names[i];
      bool in_standby = standby_producer_names.find(producer) != standby_producer_names.end();
      bool in_active = active_producers.find(producer) != active_producers.end();

      if (!in_standby && !in_active) {
         ilog( "✓ ${name} remaining is excluded - CORRECT!", ("name", producer) );
         excluded_bp++;
      } else {
         ilog( "✗ ${name} remaining is excluded - WRONG!",
               ("name", producer));
      }
   }
   BOOST_REQUIRE_EQUAL(excluded_bp, 4); // All 4 very low score producers excluded

   // Verify standby count matches configuration
   BOOST_REQUIRE_EQUAL(standby_producers.size(), standby_slots);

   // Verify no overlap between active and standby
   ilog( "=== Verifying No Overlap Between Active and Standby ===" );
   int overlap_count = 0;
   for (const auto& sp : standby_producer_names) {
      if (active_producers.find(sp) != active_producers.end()) {
         ilog( "✗ ${name} found in BOTH active and standby!", ("name", sp) );
         overlap_count++;
      }
   }
   BOOST_REQUIRE_EQUAL(overlap_count, 0); // No producer should be in both lists
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(test_guild_hash_add_remove_operations, eosio_weighted_producer_tester) try {
   // Test addguildhash and rmguildhash operations
   fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);

   ilog( "=== Test: Add/Remove Hash Operations ===" );

   // Step 1: Try to add duplicate hash (should fail)
   std::string test_hash_str = "0101010101010101010101010101010101010101010101010101010101010101";

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "addguildhash"_n, mvo()
      ("hash", test_hash_str)
   ));
   produce_blocks(1);

   fc::variant state5 = get_global_state6();
   auto hash_list = state5["guilds_code_hashes"].get_array();
   BOOST_REQUIRE_EQUAL(hash_list.size(), 1);
   ilog( "✓ First hash added successfully" );

   // Try to add same hash again (should fail)
   BOOST_REQUIRE_EQUAL(wasm_assert_msg("hash already exists in approved list"),
      push_action(config::system_account_name, "addguildhash"_n, mvo()
         ("hash", test_hash_str)
      )
   );
   ilog( "✓ Duplicate hash rejected as expected" );

   // Step 2: Add second hash
   std::string test_hash2_str = "0202020202020202020202020202020202020202020202020202020202020202";

   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "addguildhash"_n, mvo()
      ("hash", test_hash2_str)
   ));
   produce_blocks(1);

   state5 = get_global_state6();
   hash_list = state5["guilds_code_hashes"].get_array();
   BOOST_REQUIRE_EQUAL(hash_list.size(), 2);
   ilog( "✓ Second hash added successfully (total: 2)" );

   // Step 3: Remove first hash
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "rmguildhash"_n, mvo()
      ("hash", test_hash_str)
   ));
   produce_blocks(1);

   state5 = get_global_state6();
   hash_list = state5["guilds_code_hashes"].get_array();
   BOOST_REQUIRE_EQUAL(hash_list.size(), 1);
   ilog( "✓ First hash removed successfully (total: 1)" );

   // Step 4: Try to remove non-existent hash (should fail)
   BOOST_REQUIRE_EQUAL(wasm_assert_msg("hash not found in approved list"),
      push_action(config::system_account_name, "rmguildhash"_n, mvo()
         ("hash", test_hash_str)
      )
   );
   ilog( "✓ Non-existent hash removal rejected as expected" );

   // Step 5: Remove last hash
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "rmguildhash"_n, mvo()
      ("hash", test_hash2_str)
   ));
   produce_blocks(1);

   state5 = get_global_state6();
   hash_list = state5["guilds_code_hashes"].get_array();
   BOOST_REQUIRE_EQUAL(hash_list.size(), 0);
   ilog( "✓ Last hash removed successfully (total: 0)" );

   // Step 6: Verify that only eosio can call these actions
   create_account_with_resources("attacker1111"_n, config::system_account_name, core_sym::from_string("1.0000"), false);

   BOOST_REQUIRE_EQUAL(error("missing authority of eosio"),
      push_action("attacker1111"_n, "addguildhash"_n, mvo()
         ("hash", test_hash_str)
      )
   );
   ilog( "✓ Non-eosio account cannot add hash" );

   BOOST_REQUIRE_EQUAL(error("missing authority of eosio"),
      push_action("attacker1111"_n, "rmguildhash"_n, mvo()
         ("hash", test_hash_str)
      )
   );
   ilog( "✓ Non-eosio account cannot remove hash" );

   BOOST_REQUIRE_EQUAL(error("missing authority of eosio"),
      push_action("attacker1111"_n, "setenablewv"_n, mvo()
         ("enable", false)
      )
   );
   ilog( "✓ Non-eosio account cannot toggle kill switch" );

   ilog( "✓ SUCCESS: All add/remove operations work correctly" );

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
