
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


bool within_error(int64_t a, int64_t b, int64_t err) { return std::abs(a - b) <= err; };
bool within_one(int64_t a, int64_t b) { return within_error(a, b, 1); }
bool within_and_gte(int64_t a, int64_t b, int64_t w) { return a - b <= w && a >= b; }

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

  rng_treasury_data get_treasury_balance() {
    vector<char> data = get_row_by_account(  RNG_CONTRACT, RNG_CONTRACT, "treasury"_n, "treasury"_n);
    return data.empty() ? rng_treasury_data() : fc::raw::unpack<rng_treasury_data>(data);
  }

};


BOOST_AUTO_TEST_SUITE(eosio_rng_tests)

BOOST_FIXTURE_TEST_CASE(rng_config_tests, eosio_rng_tester ) try {
    fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug)    ;
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



    fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::off);
} 
FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(rng_pay, eosio_rng_tester, * boost::unit_test::tolerance(1e-10)) try {
  fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug)    ;
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

  fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::off);
} FC_LOG_AND_RETHROW()



BOOST_AUTO_TEST_SUITE_END()