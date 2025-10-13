#include <boost/test/unit_test.hpp>
#include <cstdlib>
#include <eosio/chain/contract_table_objects.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/global_property_object.hpp>
#include <eosio/chain/resource_limits.hpp>
#include <eosio/chain/wast_to_wasm.hpp>
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

  fc::variant get_global_state5() {
    vector<char> data = get_row_by_account( config::system_account_name, config::system_account_name, "global5"_n, "global5"_n );
    return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "eosio_global_state5", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
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
   fc::variant initial_state = get_global_state5();

   // Test 1: Set guilds contract name
   const name new_guilds_contract = GUILDS_OIG;
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setguildcont"_n, mvo()
      ("contract", new_guilds_contract)
   ));
   produce_blocks(1);

   // Verify guilds contract was set
   fc::variant state_after_guild = get_global_state5();
   BOOST_REQUIRE_EQUAL(state_after_guild["guilds_contract"].as<name>(), new_guilds_contract);

   // Test 2: Set BP score scaling factor
   const uint32_t new_scaling_factor = 2000; // 2.0x multiplier
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpscale"_n, mvo()
      ("scaling_factor", new_scaling_factor)
   ));
   produce_blocks(1);

   // Verify scaling factor was set
   fc::variant state_after_scale = get_global_state5();
   BOOST_REQUIRE_EQUAL(state_after_scale["bp_score_scaling_factor"].as<uint32_t>(), new_scaling_factor);

   // Test 3: Set default BP score
   const uint32_t new_default_score = 1500; // 1.5x default
   BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setbpdefscore"_n, mvo()
      ("default_score", new_default_score)
   ));
   produce_blocks(1);

   // Verify default score was set
   fc::variant final_state = get_global_state5();
   BOOST_REQUIRE_EQUAL(final_state["bp_default_score"].as<uint32_t>(), new_default_score);

   // Verify all settings persist together
   BOOST_REQUIRE_EQUAL(final_state["guilds_contract"].as<name>(), new_guilds_contract);
   BOOST_REQUIRE_EQUAL(final_state["bp_score_scaling_factor"].as<uint32_t>(), new_scaling_factor);
   BOOST_REQUIRE_EQUAL(final_state["bp_default_score"].as<uint32_t>(), new_default_score);

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
   fc::variant state = get_global_state5();
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
   fc::variant final_state = get_global_state5();
   BOOST_REQUIRE_EQUAL(final_state["guilds_contract"].as<name>(), guilds_contract);
   BOOST_REQUIRE_EQUAL(final_state["bp_score_scaling_factor"].as<uint32_t>(), scaling_factor);
   BOOST_REQUIRE_EQUAL(final_state["bp_default_score"].as<uint32_t>(), default_score);

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
