
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

inline const auto RNG_CONTRACT = "orng.wax"_n;

struct rng_treasury_data {
  uint64_t pool_balance;
};

FC_REFLECT(rng_treasury_data, (pool_balance))

// struct rng_treasury_data2 {
//   asset asset_balance;
// };

// FC_REFLECT(rng_treasury_data2, (asset_balance))


// bool within_error(int64_t a, int64_t b, int64_t err) { return std::abs(a - b) <= err; };
// bool within_one(int64_t a, int64_t b) { return within_error(a, b, 1); }
// bool within_and_gte(int64_t a, int64_t b, int64_t w) { return a - b <= w && a >= b; }

using namespace eosio_system;

struct eosio_rng_tester : eosio_system_tester {
  
  abi_serializer rng_abi_ser;
  eosio_rng_tester() { 
    const asset net = core_sym::from_string("800.0000");
    const asset cpu = core_sym::from_string("800.0000");
    const std::vector<account_name> accounts = { RNG_CONTRACT,  };
    for (const auto& v: accounts) {
      create_account_with_resources( v, config::system_account_name, core_sym::from_string("100.0000"), false, net, cpu );
      transfer( config::system_account_name, v, core_sym::from_string("100000000.0000"), config::system_account_name );
      BOOST_REQUIRE_EQUAL(success(), stake(v, core_sym::from_string("30000000.0000"), core_sym::from_string("30000000.0000")) );
    }

    produce_blocks( 2 );

    // fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);


    set_code( RNG_CONTRACT, contracts::util::rng_wasm() );
    set_abi( RNG_CONTRACT, contracts::util::rng_abi().data() );

    {
      const auto& accnt = control->db().get<account_object,by_name>( RNG_CONTRACT );
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      rng_abi_ser.set_abi(abi, abi_serializer::create_yield_function(abi_serializer_max_time));
    }

    // Get the code hash of the deployed ORNG contract
    // Compute hash from the WASM code that was deployed
    auto wasm = contracts::util::rng_wasm();
    auto hash_result = fc::sha256::hash(reinterpret_cast<const char*>(wasm.data()), wasm.size());
    ilog( "ORNG contract hash: ${hash}", ("hash", hash_result.str()) );
    // Add the correct hash to approved list
    base_tester::push_action(config::system_account_name, "addornghash"_n, config::system_account_name, mvo()
       ("hash", hash_result.str())
    );

     base_tester::push_action(config::system_account_name, updateauth::get_name(), RNG_CONTRACT, mvo()
      ("account", RNG_CONTRACT.to_string())
      ("permission", name(config::active_name).to_string())
      ("parent", name(config::owner_name).to_string())
      ("auth",  authority(1, {key_weight{get_public_key(RNG_CONTRACT, "active" ), 1}}, {
            // permission_level_weight{{config::system_account_name, config::eosio_code_name}, 1},
            permission_level_weight{{RNG_CONTRACT, config::eosio_code_name}, 1}
        }
      ))
    );
  }
  
  fc::variant get_global_state5() {
    vector<char> data = get_row_by_account( config::system_account_name, config::system_account_name, "global5"_n, "global5"_n );
    return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "eosio_global_state5", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
  }

  fc::variant get_global_state7() {
    vector<char> data = get_row_by_account( config::system_account_name, config::system_account_name, "global.b"_n, "global.b"_n );
    return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "eosio_global_state7", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
  }

  rng_treasury_data get_treasury_balance() {
    vector<char> data = get_row_by_account(  RNG_CONTRACT, RNG_CONTRACT, "treasury"_n, "treasury"_n);
    return data.empty() ? rng_treasury_data() : fc::raw::unpack<rng_treasury_data>(data);
  }

};

BOOST_AUTO_TEST_SUITE(eosio_rng_tests)

BOOST_FIXTURE_TEST_CASE(rng_config_tests, eosio_rng_tester ) try {
    // fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug)    ;
    ilog("Starting rng config tests");


    BOOST_REQUIRE_EQUAL( 
        success(), push_action( config::system_account_name, "setrngrate"_n, mvo()("rng_rate", 5000)("max_pool_rng", 1000000000))
    );

    auto global_state5 = get_global_state5();
      wdump((global_state5));
    BOOST_REQUIRE_EQUAL(global_state5["rng_rate"].as<uint64_t>(), 5000);
    BOOST_REQUIRE_EQUAL(global_state5["max_pool_rng"].as<uint64_t>(), 1000000000);

    auto treasury_balance = get_treasury_balance();
    wdump((treasury_balance));



    // fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::off);
} 
FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(rng_deposit, eosio_rng_tester, * boost::unit_test::tolerance(1e-10)) try {
  // fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug)    ;
  const uint64_t rng_rate = 5000;
  const uint64_t max_pool_rng = 1000000000;

  BOOST_REQUIRE_EQUAL( 
      success(), push_action( config::system_account_name, "setrngrate"_n, mvo()("rng_rate", rng_rate)("max_pool_rng", max_pool_rng))
  );


   const double continuous_rate = 0.04879;
   const double usecs_per_year  = 52 * 7 * 24 * 3600 * 1000000ll;
   const double secs_per_year   = 52 * 7 * 24 * 3600;

   const asset large_asset = core_sym::from_string("80.0000");
   create_account_with_resources( "defproducera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );
   create_account_with_resources( "defproducerb"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );
   create_account_with_resources( "defproducerc"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );

   create_account_with_resources( "producvotera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );
   create_account_with_resources( "producvoterb"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );

   BOOST_REQUIRE_EQUAL(success(), regproducer("defproducera"_n));
   produce_block(fc::hours(24));
   auto prod = get_producer_info( "defproducera"_n );
   BOOST_REQUIRE_EQUAL("defproducera", prod["owner"].as_string());
   BOOST_REQUIRE_EQUAL(0, prod["total_votes"].as_double());

   transfer( config::system_account_name, "producvotera", core_sym::from_string("400000000.0000"), config::system_account_name);
   BOOST_REQUIRE_EQUAL(success(), stake("producvotera", core_sym::from_string("100000000.0000"), core_sym::from_string("100000000.0000")));
   BOOST_REQUIRE_EQUAL(success(), vote( "producvotera"_n, { "defproducera"_n }));
   // defproducera is the only active producer
   // produce enough blocks so new schedule kicks in and defproducera produces some blocks
   {
      produce_blocks(50);

      const auto     initial_global_state      = get_global_state();
      const uint64_t initial_claim_time        = microseconds_since_epoch_of_iso_string( initial_global_state["last_pervote_bucket_fill"] );
      const int64_t  initial_pervote_bucket    = initial_global_state["pervote_bucket"].as<int64_t>();
      const int64_t  initial_perblock_bucket   = initial_global_state["perblock_bucket"].as<int64_t>();
      const int64_t  initial_savings           = get_balance("eosio.saving"_n).get_amount();
      const uint32_t initial_tot_unpaid_blocks = initial_global_state["total_unpaid_blocks"].as<uint32_t>();

      prod = get_producer_info("defproducera");
      const uint32_t unpaid_blocks = prod["unpaid_blocks"].as<uint32_t>();
      BOOST_REQUIRE(1 < unpaid_blocks);

      BOOST_REQUIRE_EQUAL(initial_tot_unpaid_blocks, unpaid_blocks);

      const asset initial_supply  = get_token_supply();
      const asset initial_balance = get_balance("defproducera"_n);

      auto treasury_balance_before = get_treasury_balance();
      wdump((treasury_balance_before));

      BOOST_REQUIRE_EQUAL(success(), push_action("defproducera"_n, "claimrewards"_n, mvo()("owner", "defproducera")));

      const auto     global_state      = get_global_state();
      const uint64_t claim_time        = microseconds_since_epoch_of_iso_string( global_state["last_pervote_bucket_fill"] );
      const int64_t  pervote_bucket    = global_state["pervote_bucket"].as<int64_t>();
      const int64_t  perblock_bucket   = global_state["perblock_bucket"].as<int64_t>();
      const int64_t  savings           = get_balance("eosio.saving"_n).get_amount();
      const uint32_t tot_unpaid_blocks = global_state["total_unpaid_blocks"].as<uint32_t>();

      prod = get_producer_info("defproducera");
      BOOST_REQUIRE_EQUAL(1, prod["unpaid_blocks"].as<uint32_t>());
      BOOST_REQUIRE_EQUAL(1, tot_unpaid_blocks);
      const asset supply  = get_token_supply();
      const asset balance = get_balance("defproducera"_n);

      BOOST_REQUIRE_EQUAL(claim_time, microseconds_since_epoch_of_iso_string( prod["last_claim_time"] ));

      auto usecs_between_fills = claim_time - initial_claim_time;
      int32_t secs_between_fills = usecs_between_fills/1000000;
      uint64_t new_tokens_org = (initial_supply.get_amount() * double(secs_between_fills) * continuous_rate) / secs_per_year;
      uint64_t new_tokens = new_tokens_org * (10000 - rng_rate) / 10000;

      BOOST_REQUIRE_EQUAL(0, initial_savings);
      BOOST_REQUIRE_EQUAL(0, initial_perblock_bucket);
      BOOST_REQUIRE_EQUAL(0, initial_pervote_bucket);

      auto treasury_balance = get_treasury_balance();
      wdump((treasury_balance));
      wdump((new_tokens_org));
      wdump((new_tokens));

      BOOST_REQUIRE_EQUAL(new_tokens_org, supply.get_amount() - initial_supply.get_amount());
      BOOST_REQUIRE_EQUAL(int64_t(new_tokens - (new_tokens / 5) * 3), savings - initial_savings);
      BOOST_REQUIRE_EQUAL(int64_t(new_tokens / 5), balance.get_amount() - initial_balance.get_amount());

      int64_t from_perblock_bucket_org = int64_t( initial_supply.get_amount() * double(secs_between_fills) * (continuous_rate / 5.) / secs_per_year ) ;
      int64_t from_perblock_bucket = from_perblock_bucket_org * (10000 - rng_rate) / 10000;
      int64_t from_pervote_bucket  = 0;


      if (from_pervote_bucket >= 100 * 10000) {
         BOOST_REQUIRE_EQUAL(from_perblock_bucket + from_pervote_bucket, balance.get_amount() - initial_balance.get_amount());
         BOOST_REQUIRE_EQUAL(0, pervote_bucket);
      } else {
         BOOST_REQUIRE_EQUAL(from_perblock_bucket, balance.get_amount() - initial_balance.get_amount());
         BOOST_REQUIRE_EQUAL(from_pervote_bucket, pervote_bucket);
      }
   }

  // fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::off);
} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(rng_deposit_max_balance, eosio_rng_tester, * boost::unit_test::tolerance(1e-10)) try {
  fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug)    ;
  const uint64_t rng_rate = 5000;
  const uint64_t max_pool_rng = 100000; //small value

  BOOST_REQUIRE_EQUAL( 
      success(), push_action( config::system_account_name, "setrngrate"_n, mvo()("rng_rate", rng_rate)("max_pool_rng", max_pool_rng))
  );


   const double continuous_rate = 0.04879;
   const double usecs_per_year  = 52 * 7 * 24 * 3600 * 1000000ll;
   const double secs_per_year   = 52 * 7 * 24 * 3600;

   const asset large_asset = core_sym::from_string("80.0000");
   create_account_with_resources( "defproducera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );
   create_account_with_resources( "defproducerb"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );
   create_account_with_resources( "defproducerc"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );

   create_account_with_resources( "producvotera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );
   create_account_with_resources( "producvoterb"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );

   BOOST_REQUIRE_EQUAL(success(), regproducer("defproducera"_n));
   produce_block(fc::hours(24));
   auto prod = get_producer_info( "defproducera"_n );
   BOOST_REQUIRE_EQUAL("defproducera", prod["owner"].as_string());
   BOOST_REQUIRE_EQUAL(0, prod["total_votes"].as_double());

   transfer( config::system_account_name, "producvotera", core_sym::from_string("400000000.0000"), config::system_account_name);
   BOOST_REQUIRE_EQUAL(success(), stake("producvotera", core_sym::from_string("100000000.0000"), core_sym::from_string("100000000.0000")));
   BOOST_REQUIRE_EQUAL(success(), vote( "producvotera"_n, { "defproducera"_n }));
   // defproducera is the only active producer
   // produce enough blocks so new schedule kicks in and defproducera produces some blocks
   {
      produce_blocks(50);

      const auto     initial_global_state      = get_global_state();
      const uint64_t initial_claim_time        = microseconds_since_epoch_of_iso_string( initial_global_state["last_pervote_bucket_fill"] );
      const int64_t  initial_pervote_bucket    = initial_global_state["pervote_bucket"].as<int64_t>();
      const int64_t  initial_perblock_bucket   = initial_global_state["perblock_bucket"].as<int64_t>();
      const int64_t  initial_savings           = get_balance("eosio.saving"_n).get_amount();
      const uint32_t initial_tot_unpaid_blocks = initial_global_state["total_unpaid_blocks"].as<uint32_t>();

      prod = get_producer_info("defproducera");
      const uint32_t unpaid_blocks = prod["unpaid_blocks"].as<uint32_t>();
      BOOST_REQUIRE(1 < unpaid_blocks);

      BOOST_REQUIRE_EQUAL(initial_tot_unpaid_blocks, unpaid_blocks);

      const asset initial_supply  = get_token_supply();
      const asset initial_balance = get_balance("defproducera"_n);

      auto treasury_balance_before = get_treasury_balance();
      wdump((treasury_balance_before));

      BOOST_REQUIRE_EQUAL(success(), push_action("defproducera"_n, "claimrewards"_n, mvo()("owner", "defproducera")));

      const auto     global_state      = get_global_state();
      const uint64_t claim_time        = microseconds_since_epoch_of_iso_string( global_state["last_pervote_bucket_fill"] );
      const int64_t  pervote_bucket    = global_state["pervote_bucket"].as<int64_t>();
      const int64_t  perblock_bucket   = global_state["perblock_bucket"].as<int64_t>();
      const int64_t  savings           = get_balance("eosio.saving"_n).get_amount();
      const uint32_t tot_unpaid_blocks = global_state["total_unpaid_blocks"].as<uint32_t>();

      prod = get_producer_info("defproducera");
      BOOST_REQUIRE_EQUAL(1, prod["unpaid_blocks"].as<uint32_t>());
      BOOST_REQUIRE_EQUAL(1, tot_unpaid_blocks);
      const asset supply  = get_token_supply();
      const asset balance = get_balance("defproducera"_n);

      BOOST_REQUIRE_EQUAL(claim_time, microseconds_since_epoch_of_iso_string( prod["last_claim_time"] ));

      auto usecs_between_fills = claim_time - initial_claim_time;
      int32_t secs_between_fills = usecs_between_fills/1000000;
      uint64_t new_tokens_org = (initial_supply.get_amount() * double(secs_between_fills) * continuous_rate) / secs_per_year;
      uint64_t expected_rng_deposit = new_tokens_org * rng_rate / 10000;
      uint64_t rng_deposit = min(expected_rng_deposit, max_pool_rng);
      ilog("expected_rng_deposit:                        ${x}", ("x", expected_rng_deposit));
      ilog("rng_deposit:                        ${x}", ("x", rng_deposit));

      uint64_t new_tokens = new_tokens_org  - rng_deposit;

      BOOST_REQUIRE_EQUAL(0, initial_savings);
      BOOST_REQUIRE_EQUAL(0, initial_perblock_bucket);
      BOOST_REQUIRE_EQUAL(0, initial_pervote_bucket);

      auto treasury_balance = get_treasury_balance();
      wdump((treasury_balance));
      wdump((new_tokens_org));
      wdump((new_tokens));
      
      // deposit max pool rng
      BOOST_REQUIRE_EQUAL(max_pool_rng, treasury_balance.pool_balance);
      BOOST_REQUIRE_EQUAL(true, rng_deposit < expected_rng_deposit);

      BOOST_REQUIRE_EQUAL(new_tokens_org, supply.get_amount() - initial_supply.get_amount());
      BOOST_REQUIRE_EQUAL(int64_t(new_tokens - (new_tokens / 5) * 3), savings - initial_savings);
      BOOST_REQUIRE_EQUAL(int64_t(new_tokens / 5), balance.get_amount() - initial_balance.get_amount());

      int64_t from_perblock_bucket_org = int64_t( initial_supply.get_amount() * double(secs_between_fills) * (continuous_rate) / secs_per_year ) ;
      int64_t from_perblock_bucket = (from_perblock_bucket_org - rng_deposit) / 5;
      int64_t from_pervote_bucket  = 0;


      if (from_pervote_bucket >= 100 * 10000) {
         BOOST_REQUIRE_EQUAL(from_perblock_bucket + from_pervote_bucket, balance.get_amount() - initial_balance.get_amount());
         BOOST_REQUIRE_EQUAL(0, pervote_bucket);
      } else {
         BOOST_REQUIRE_EQUAL(from_perblock_bucket, balance.get_amount() - initial_balance.get_amount());
         BOOST_REQUIRE_EQUAL(from_pervote_bucket, pervote_bucket);
      }
   }

  fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::off);
} FC_LOG_AND_RETHROW()


// Edge Case Tests

BOOST_FIXTURE_TEST_CASE(rng_rate_zero, eosio_rng_tester, * boost::unit_test::tolerance(1e-10)) try {
  ilog("Testing rng_rate = 0 (no deposits should occur)");

  const uint64_t rng_rate = 0;  // Zero rate - should disable RNG deposits
  const uint64_t max_pool_rng = 1000000000;

  BOOST_REQUIRE_EQUAL(
      success(), push_action( config::system_account_name, "setrngrate"_n, mvo()("rng_rate", rng_rate)("max_pool_rng", max_pool_rng))
  );

  const double continuous_rate = 0.04879;
  const double secs_per_year   = 52 * 7 * 24 * 3600;

  const asset large_asset = core_sym::from_string("80.0000");
  create_account_with_resources( "defproducera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );
  create_account_with_resources( "producvotera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );

  BOOST_REQUIRE_EQUAL(success(), regproducer("defproducera"_n));
  produce_block(fc::hours(24));

  transfer( config::system_account_name, "producvotera", core_sym::from_string("400000000.0000"), config::system_account_name);
  BOOST_REQUIRE_EQUAL(success(), stake("producvotera", core_sym::from_string("100000000.0000"), core_sym::from_string("100000000.0000")));
  BOOST_REQUIRE_EQUAL(success(), vote( "producvotera"_n, { "defproducera"_n }));

  produce_blocks(50);

  const auto     initial_global_state      = get_global_state();
  const uint64_t initial_claim_time        = microseconds_since_epoch_of_iso_string( initial_global_state["last_pervote_bucket_fill"] );
  const asset    initial_supply            = get_token_supply();
  const auto     initial_treasury_balance  = get_treasury_balance();

  BOOST_REQUIRE_EQUAL(success(), push_action("defproducera"_n, "claimrewards"_n, mvo()("owner", "defproducera")));

  const auto     global_state      = get_global_state();
  const uint64_t claim_time        = microseconds_since_epoch_of_iso_string( global_state["last_pervote_bucket_fill"] );
  const asset    supply            = get_token_supply();
  const auto     treasury_balance  = get_treasury_balance();

  auto usecs_between_fills = claim_time - initial_claim_time;
  int32_t secs_between_fills = usecs_between_fills/1000000;
  uint64_t expected_inflation = (initial_supply.get_amount() * double(secs_between_fills) * continuous_rate) / secs_per_year;

  // Verify inflation occurred
  BOOST_REQUIRE_EQUAL(expected_inflation, supply.get_amount() - initial_supply.get_amount());

  // Verify NO RNG deposit occurred
  BOOST_REQUIRE_EQUAL(initial_treasury_balance.pool_balance, treasury_balance.pool_balance);
  BOOST_REQUIRE_EQUAL(0, treasury_balance.pool_balance);

  ilog("✓ rng_rate = 0 correctly prevents RNG deposits");
} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(max_pool_rng_zero, eosio_rng_tester, * boost::unit_test::tolerance(1e-10)) try {
  ilog("Testing max_pool_rng = 0 (should disable deposits)");

  const uint64_t rng_rate = 5000;  // 50% rate
  const uint64_t max_pool_rng = 0; // Zero max pool - should disable deposits

  BOOST_REQUIRE_EQUAL(
      success(), push_action( config::system_account_name, "setrngrate"_n, mvo()("rng_rate", rng_rate)("max_pool_rng", max_pool_rng))
  );

  const double continuous_rate = 0.04879;
  const double secs_per_year   = 52 * 7 * 24 * 3600;

  const asset large_asset = core_sym::from_string("80.0000");
  create_account_with_resources( "defproducera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );
  create_account_with_resources( "producvotera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );

  BOOST_REQUIRE_EQUAL(success(), regproducer("defproducera"_n));
  produce_block(fc::hours(24));

  transfer( config::system_account_name, "producvotera", core_sym::from_string("400000000.0000"), config::system_account_name);
  BOOST_REQUIRE_EQUAL(success(), stake("producvotera", core_sym::from_string("100000000.0000"), core_sym::from_string("100000000.0000")));
  BOOST_REQUIRE_EQUAL(success(), vote( "producvotera"_n, { "defproducera"_n }));

  produce_blocks(50);

  const auto     initial_global_state      = get_global_state();
  const uint64_t initial_claim_time        = microseconds_since_epoch_of_iso_string( initial_global_state["last_pervote_bucket_fill"] );
  const asset    initial_supply            = get_token_supply();
  const auto     initial_treasury_balance  = get_treasury_balance();

  BOOST_REQUIRE_EQUAL(success(), push_action("defproducera"_n, "claimrewards"_n, mvo()("owner", "defproducera")));

  const auto     global_state      = get_global_state();
  const uint64_t claim_time        = microseconds_since_epoch_of_iso_string( global_state["last_pervote_bucket_fill"] );
  const asset    supply            = get_token_supply();
  const auto     treasury_balance  = get_treasury_balance();

  auto usecs_between_fills = claim_time - initial_claim_time;
  int32_t secs_between_fills = usecs_between_fills/1000000;
  uint64_t expected_inflation = (initial_supply.get_amount() * double(secs_between_fills) * continuous_rate) / secs_per_year;

  // Verify inflation occurred
  BOOST_REQUIRE_EQUAL(expected_inflation, supply.get_amount() - initial_supply.get_amount());

  // Verify NO RNG deposit occurred (max_pool_rng = 0 means always at max)
  BOOST_REQUIRE_EQUAL(initial_treasury_balance.pool_balance, treasury_balance.pool_balance);
  BOOST_REQUIRE_EQUAL(0, treasury_balance.pool_balance);

  ilog("✓ max_pool_rng = 0 correctly disables RNG deposits");
} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(rng_rate_maximum, eosio_rng_tester, * boost::unit_test::tolerance(1e-10)) try {
  ilog("Testing rng_rate = 9999 (maximum valid value - 99.99% to RNG)");

  const uint64_t rng_rate = 9999;  // Maximum valid rate (99.99%)
  const uint64_t max_pool_rng = 100000000000;  // Very large max

  BOOST_REQUIRE_EQUAL(
      success(), push_action( config::system_account_name, "setrngrate"_n, mvo()("rng_rate", rng_rate)("max_pool_rng", max_pool_rng))
  );

  const double continuous_rate = 0.04879;
  const double secs_per_year   = 52 * 7 * 24 * 3600;

  const asset large_asset = core_sym::from_string("80.0000");
  create_account_with_resources( "defproducera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );
  create_account_with_resources( "producvotera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );

  BOOST_REQUIRE_EQUAL(success(), regproducer("defproducera"_n));
  produce_block(fc::hours(24));

  transfer( config::system_account_name, "producvotera", core_sym::from_string("400000000.0000"), config::system_account_name);
  BOOST_REQUIRE_EQUAL(success(), stake("producvotera", core_sym::from_string("100000000.0000"), core_sym::from_string("100000000.0000")));
  BOOST_REQUIRE_EQUAL(success(), vote( "producvotera"_n, { "defproducera"_n }));

  produce_blocks(50);

  const auto     initial_global_state      = get_global_state();
  const uint64_t initial_claim_time        = microseconds_since_epoch_of_iso_string( initial_global_state["last_pervote_bucket_fill"] );
  const int64_t  initial_savings           = get_balance("eosio.saving"_n).get_amount();
  const asset    initial_supply            = get_token_supply();
  const asset    initial_balance           = get_balance("defproducera"_n);

  BOOST_REQUIRE_EQUAL(success(), push_action("defproducera"_n, "claimrewards"_n, mvo()("owner", "defproducera")));

  const auto     global_state      = get_global_state();
  const uint64_t claim_time        = microseconds_since_epoch_of_iso_string( global_state["last_pervote_bucket_fill"] );
  const int64_t  savings           = get_balance("eosio.saving"_n).get_amount();
  const asset    supply            = get_token_supply();
  const asset    balance           = get_balance("defproducera"_n);
  const auto     treasury_balance  = get_treasury_balance();

  auto usecs_between_fills = claim_time - initial_claim_time;
  int32_t secs_between_fills = usecs_between_fills/1000000;
  uint64_t total_inflation = (initial_supply.get_amount() * double(secs_between_fills) * continuous_rate) / secs_per_year;
  uint64_t expected_rng_deposit = total_inflation * rng_rate / 10000;
  uint64_t remaining_for_producers = total_inflation - expected_rng_deposit;

  // Verify total inflation
  BOOST_REQUIRE_EQUAL(total_inflation, supply.get_amount() - initial_supply.get_amount());

  // Verify RNG got 99.99% of inflation
  BOOST_REQUIRE_EQUAL(expected_rng_deposit, treasury_balance.pool_balance);

  // Verify producers only got 0.01% / 5 of total inflation
  uint64_t expected_producer_pay = remaining_for_producers / 5;
  BOOST_REQUIRE_EQUAL(expected_producer_pay, balance.get_amount() - initial_balance.get_amount());

  // Verify savings got 2/5 of the remaining (not the RNG portion)
  uint64_t expected_savings = remaining_for_producers - (remaining_for_producers / 5) * 3;
  BOOST_REQUIRE_EQUAL(expected_savings, savings - initial_savings);

  ilog("✓ rng_rate = 9999 correctly allocates 99.99%% to RNG");
} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(treasury_exceeds_max_pool, eosio_rng_tester, * boost::unit_test::tolerance(1e-10)) try {
  ilog("Testing treasury_balance > max_pool_rng (underflow prevention)");

  const uint64_t rng_rate = 5000;
  const uint64_t initial_max_pool_rng = 500000;  // Initial max

  BOOST_REQUIRE_EQUAL(
      success(), push_action( config::system_account_name, "setrngrate"_n, mvo()("rng_rate", rng_rate)("max_pool_rng", initial_max_pool_rng))
  );

  const double continuous_rate = 0.04879;
  const double secs_per_year   = 52 * 7 * 24 * 3600;

  const asset large_asset = core_sym::from_string("80.0000");
  create_account_with_resources( "defproducera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );
  create_account_with_resources( "producvotera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );

  BOOST_REQUIRE_EQUAL(success(), regproducer("defproducera"_n));
  produce_block(fc::hours(24));

  transfer( config::system_account_name, "producvotera", core_sym::from_string("400000000.0000"), config::system_account_name);
  BOOST_REQUIRE_EQUAL(success(), stake("producvotera", core_sym::from_string("100000000.0000"), core_sym::from_string("100000000.0000")));
  BOOST_REQUIRE_EQUAL(success(), vote( "producvotera"_n, { "defproducera"_n }));

  // First claim to fill up the treasury to the initial max
  produce_blocks(200);
  BOOST_REQUIRE_EQUAL(success(), push_action("defproducera"_n, "claimrewards"_n, mvo()("owner", "defproducera")));

  auto treasury_balance_after_first = get_treasury_balance();
  ilog("Treasury after first claim: ${b}", ("b", treasury_balance_after_first.pool_balance));
  BOOST_REQUIRE_EQUAL(initial_max_pool_rng, treasury_balance_after_first.pool_balance);

  // Now LOWER the max_pool_rng to below the current treasury balance
  // This simulates the scenario where treasury_balance > max_pool_rng
  const uint64_t lowered_max_pool_rng = 100000;  // Much lower than current treasury (500000)
  BOOST_REQUIRE_EQUAL(
      success(), push_action( config::system_account_name, "setrngrate"_n, mvo()("rng_rate", rng_rate)("max_pool_rng", lowered_max_pool_rng))
  );

  ilog("Lowered max_pool_rng to ${m}, treasury is ${t}",
       ("m", lowered_max_pool_rng)("t", treasury_balance_after_first.pool_balance));
  BOOST_REQUIRE(treasury_balance_after_first.pool_balance > lowered_max_pool_rng);

  const auto     initial_global_state      = get_global_state();
  const uint64_t initial_claim_time        = microseconds_since_epoch_of_iso_string( initial_global_state["last_pervote_bucket_fill"] );
  const asset    initial_supply            = get_token_supply();

  // Now claim again - should NOT deposit to RNG since treasury > max_pool_rng
  produce_blocks(50);
  produce_block(fc::hours(24));
  BOOST_REQUIRE_EQUAL(success(), push_action("defproducera"_n, "claimrewards"_n, mvo()("owner", "defproducera")));

  const auto     global_state      = get_global_state();
  const uint64_t claim_time        = microseconds_since_epoch_of_iso_string( global_state["last_pervote_bucket_fill"] );
  const asset    supply            = get_token_supply();
  const auto     treasury_balance_final  = get_treasury_balance();

  auto usecs_between_fills = claim_time - initial_claim_time;
  int32_t secs_between_fills = usecs_between_fills/1000000;
  uint64_t total_inflation = (initial_supply.get_amount() * double(secs_between_fills) * continuous_rate) / secs_per_year;

  // Verify inflation occurred
  BOOST_REQUIRE_EQUAL(total_inflation, supply.get_amount() - initial_supply.get_amount());

  // Verify NO additional RNG deposit occurred (treasury was already over max)
  BOOST_REQUIRE_EQUAL(treasury_balance_after_first.pool_balance, treasury_balance_final.pool_balance);

  // This proves the underflow prevention works - no deposit when treasury > max_pool_rng
  ilog("✓ No deposit when treasury (${t}) exceeds max_pool_rng (${m})",
       ("t", treasury_balance_final.pool_balance)("m", lowered_max_pool_rng));

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(rng_rate_boundary_10000, eosio_rng_tester) try {
  ilog("Testing rng_rate = 10000 (should fail - at boundary)");

  // Should fail because rate must be < 10000
  BOOST_REQUIRE_EQUAL(
      wasm_assert_msg("rng_rate must be between 0 and 10000"),
      push_action( config::system_account_name, "setrngrate"_n, mvo()("rng_rate", 10000)("max_pool_rng", 1000000))
  );

  ilog("✓ rng_rate = 10000 correctly rejected");
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(orng_hash_validation_wrong_hash, eosio_rng_tester, * boost::unit_test::tolerance(1e-10)) try {
  ilog("Testing ORNG hash validation with wrong hash (should block deposits)");

  const uint64_t rng_rate = 5000;
  const uint64_t max_pool_rng = 1000000000;

  BOOST_REQUIRE_EQUAL(
      success(), push_action( config::system_account_name, "setrngrate"_n, mvo()("rng_rate", rng_rate)("max_pool_rng", max_pool_rng))
  );

  // Replace the correct hash (from constructor) with a WRONG hash
  std::vector<std::string> wrong_hashes = {
      "0000000000000000000000000000000000000000000000000000000000000001"
  };
  BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setornghash"_n, mvo()
     ("hashes", wrong_hashes)
  ));

  // Verify hash was added
  auto global_state7 = get_global_state7();
  BOOST_REQUIRE(!global_state7.is_null());
  auto hashes = global_state7["orng_code_hashes"].get_array();
  BOOST_REQUIRE_EQUAL(1, hashes.size());

  const double continuous_rate = 0.04879;
  const double secs_per_year   = 52 * 7 * 24 * 3600;

  const asset large_asset = core_sym::from_string("80.0000");
  create_account_with_resources( "defproducera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );
  create_account_with_resources( "producvotera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );

  BOOST_REQUIRE_EQUAL(success(), regproducer("defproducera"_n));
  produce_block(fc::hours(24));

  transfer( config::system_account_name, "producvotera", core_sym::from_string("400000000.0000"), config::system_account_name);
  BOOST_REQUIRE_EQUAL(success(), stake("producvotera", core_sym::from_string("100000000.0000"), core_sym::from_string("100000000.0000")));
  BOOST_REQUIRE_EQUAL(success(), vote( "producvotera"_n, { "defproducera"_n }));

  produce_blocks(50);

  auto initial_treasury_balance = get_treasury_balance();
  BOOST_REQUIRE_EQUAL(success(), push_action("defproducera"_n, "claimrewards"_n, mvo()("owner", "defproducera")));
  auto final_treasury_balance = get_treasury_balance();

  // With wrong hash, deposits should NOT occur
  BOOST_REQUIRE_EQUAL(initial_treasury_balance.pool_balance, final_treasury_balance.pool_balance);

  ilog("✓ Wrong hash blocks deposits (security validated)");
} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(orng_hash_validation_no_approved_hash, eosio_rng_tester, * boost::unit_test::tolerance(1e-10)) try {
  ilog("Testing ORNG deposit blocked when contract hash not in approved list");

  const uint64_t rng_rate = 5000;
  const uint64_t max_pool_rng = 1000000000;

  BOOST_REQUIRE_EQUAL(
      success(), push_action( config::system_account_name, "setrngrate"_n, mvo()("rng_rate", rng_rate)("max_pool_rng", max_pool_rng))
  );

  // Clear all approved hashes - the deployed ORNG contract hash won't match anything
  std::vector<std::string> empty_hashes;
  BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setornghash"_n, mvo()
     ("hashes", empty_hashes)
  ));

  // Verify the approved hash list is empty
  auto global_state7 = get_global_state7();
  BOOST_REQUIRE(!global_state7.is_null());
  auto hashes = global_state7["orng_code_hashes"].get_array();
  BOOST_REQUIRE_EQUAL(0, hashes.size());

  const double continuous_rate = 0.04879;
  const double secs_per_year   = 52 * 7 * 24 * 3600;

  const asset large_asset = core_sym::from_string("80.0000");
  create_account_with_resources( "defproducera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );
  create_account_with_resources( "producvotera"_n, config::system_account_name, core_sym::from_string("1.0000"), false, large_asset, large_asset );

  BOOST_REQUIRE_EQUAL(success(), regproducer("defproducera"_n));
  produce_block(fc::hours(24));

  transfer( config::system_account_name, "producvotera", core_sym::from_string("400000000.0000"), config::system_account_name);
  BOOST_REQUIRE_EQUAL(success(), stake("producvotera", core_sym::from_string("100000000.0000"), core_sym::from_string("100000000.0000")));
  BOOST_REQUIRE_EQUAL(success(), vote( "producvotera"_n, { "defproducera"_n }));

  produce_blocks(50);

  const auto     initial_global_state      = get_global_state();
  const uint64_t initial_claim_time        = microseconds_since_epoch_of_iso_string( initial_global_state["last_pervote_bucket_fill"] );
  const asset    initial_supply            = get_token_supply();
  auto initial_treasury_balance = get_treasury_balance();

  ilog("Initial treasury balance: ${b}", ("b", initial_treasury_balance.pool_balance));

  BOOST_REQUIRE_EQUAL(success(), push_action("defproducera"_n, "claimrewards"_n, mvo()("owner", "defproducera")));

  const auto     global_state      = get_global_state();
  const uint64_t claim_time        = microseconds_since_epoch_of_iso_string( global_state["last_pervote_bucket_fill"] );
  const asset    supply            = get_token_supply();
  auto final_treasury_balance = get_treasury_balance();

  auto usecs_between_fills = claim_time - initial_claim_time;
  int32_t secs_between_fills = usecs_between_fills/1000000;
  uint64_t expected_inflation = (initial_supply.get_amount() * double(secs_between_fills) * continuous_rate) / secs_per_year;

  // Verify inflation occurred normally
  BOOST_REQUIRE_EQUAL(expected_inflation, supply.get_amount() - initial_supply.get_amount());

  // Verify NO RNG deposit occurred (contract hash not in approved list)
  BOOST_REQUIRE_EQUAL(initial_treasury_balance.pool_balance, final_treasury_balance.pool_balance);
  BOOST_REQUIRE_EQUAL(0, final_treasury_balance.pool_balance);

  ilog("Final treasury balance: ${b}", ("b", final_treasury_balance.pool_balance));
  ilog("✓ Deposits blocked when contract hash not in approved list");
} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(orng_hash_management_actions, eosio_rng_tester) try {
  ilog("Testing ORNG hash management actions (add/remove/set)");

  // Test addornghash
  BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "addornghash"_n, mvo()
     ("hash", "0000000000000000000000000000000000000000000000000000000000000001")
  ));

  auto global_state7 = get_global_state7();
  auto hashes = global_state7["orng_code_hashes"].get_array();
  BOOST_REQUIRE_EQUAL(2, hashes.size());

  // Test adding duplicate (should fail)
  BOOST_REQUIRE_EQUAL(
      wasm_assert_msg("hash already exists in approved list"),
      push_action(config::system_account_name, "addornghash"_n, mvo()
         ("hash", "0000000000000000000000000000000000000000000000000000000000000001")
      )
  );

  // Test adding second hash
  BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "addornghash"_n, mvo()
     ("hash", "0000000000000000000000000000000000000000000000000000000000000002")
  ));

  global_state7 = get_global_state7();
  hashes = global_state7["orng_code_hashes"].get_array();
  BOOST_REQUIRE_EQUAL(3, hashes.size());

  // Test rmornghash
  BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "rmornghash"_n, mvo()
     ("hash", "0000000000000000000000000000000000000000000000000000000000000001")
  ));

  global_state7 = get_global_state7();
  hashes = global_state7["orng_code_hashes"].get_array();
  BOOST_REQUIRE_EQUAL(2, hashes.size());

  // Test removing non-existent hash (should fail)
  BOOST_REQUIRE_EQUAL(
      wasm_assert_msg("hash not found in approved list"),
      push_action(config::system_account_name, "rmornghash"_n, mvo()
         ("hash", "0000000000000000000000000000000000000000000000000000000000000099")
      )
  );

  // Test setornghash (replace entire list)
  std::vector<std::string> new_hashes = {
      "0000000000000000000000000000000000000000000000000000000000000003",
      "0000000000000000000000000000000000000000000000000000000000000004",
      "0000000000000000000000000000000000000000000000000000000000000005"
  };
  BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setornghash"_n, mvo()
     ("hashes", new_hashes)
  ));

  global_state7 = get_global_state7();
  hashes = global_state7["orng_code_hashes"].get_array();
  BOOST_REQUIRE_EQUAL(3, hashes.size());

  // Test setting empty list
  std::vector<std::string> empty_hashes;
  BOOST_REQUIRE_EQUAL(success(), push_action(config::system_account_name, "setornghash"_n, mvo()
     ("hashes", empty_hashes)
  ));

  global_state7 = get_global_state7();
  hashes = global_state7["orng_code_hashes"].get_array();
  BOOST_REQUIRE_EQUAL(0, hashes.size());

  ilog("✓ All ORNG hash management actions work correctly");
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()