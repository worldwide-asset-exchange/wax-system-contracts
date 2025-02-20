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


// bool within_error(int64_t a, int64_t b, int64_t err) { return std::abs(a - b) <= err; };
// bool within_one(int64_t a, int64_t b) { return within_error(a, b, 1); }
// bool within_and_gte(int64_t a, int64_t b, int64_t w) { return a - b <= w && a >= b; }

struct standby_producer_state {
  name            owner;
  double          standby_share = 0;
  time_point      last_standby_share_update;
  time_point      last_claim_time;
  bool            is_active = true;
};

FC_REFLECT(standby_producer_state, (owner)(standby_share)(last_standby_share_update)(last_claim_time)(is_active))

struct standby_disallow_state {
  name            owner;
};

FC_REFLECT(standby_disallow_state, (owner))


using namespace eosio_system;

struct eosio_standby_tester : eosio_system_tester {
  eosio_standby_tester() {  }



  standby_producer_state get_standby_producer_state(name acc) {
   vector<char> data = get_row_by_account(config::system_account_name, config::system_account_name, "standbys"_n, acc);
    return fc::raw::unpack<standby_producer_state>(data);
  }

  standby_disallow_state get_standby_disallow_state(name acc) {
    vector<char> data = get_row_by_account(config::system_account_name, config::system_account_name, "stdbdisallow"_n, acc);
    return fc::raw::unpack<standby_disallow_state>(data);
  }

  fc::variant get_global_state4() {
    vector<char> data = get_row_by_account( config::system_account_name, config::system_account_name, "global4"_n, "global4"_n );
    return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "eosio_global_state4", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
  }

  std::vector<standby_producer_state> get_stanby_table()
  {
    std::vector<standby_producer_state> result;

    const auto* table_id_itr = control->db().find<eosio::chain::table_id_object, eosio::chain::by_code_scope_table>(
      boost::make_tuple(eosio::chain::config::system_account_name, eosio::chain::config::system_account_name, "standbys"_n));

    const auto& idx = control->db().get_index<eosio::chain::key_value_index, eosio::chain::by_scope_primary>();
    auto table_id = table_id_itr->id;

    standby_producer_state r;

    auto lower = idx.lower_bound(boost::make_tuple(table_id, 0));
    for (auto itr = lower; itr != idx.end(); ++itr){
         fc::datastream<const char*> ds(itr->value.data(), itr->value.size());
         fc::raw::unpack(ds, r);
         result.push_back(r);
    }
    return result;
  }

  vector<name> active_and_vote_producers_and_standbys() {
    //stake more than 15% of total EOS supply to activate chain
    const asset net = core_sym::from_string("80.0000");
    const asset cpu = core_sym::from_string("80.0000");
    const std::vector<account_name> voters = { "producvotera"_n, "producvoterb"_n, "producvoterc"_n, "producvoterd"_n };
    for (const auto& v: voters) {
       create_account_with_resources( v, config::system_account_name, core_sym::from_string("1.0000"), false, net, cpu );
       transfer( config::system_account_name, v, core_sym::from_string("100000000.0000"), config::system_account_name );
       BOOST_REQUIRE_EQUAL(success(), stake(v, core_sym::from_string("30000000.0000"), core_sym::from_string("30000000.0000")) );
    }
 
    // create accounts {defproducera, defproducerb, ..., defproducerz, abcproducera, ..., defproducern} and register as producers
    std::vector<account_name> producer_names;
    {
       producer_names.reserve('z' - 'a' + 1);
       {
          const std::string root("defproducer");
          for ( char c = 'a'; c <= 'z'; ++c ) {
             producer_names.emplace_back(root + std::string(1, c));
          }
       }
       {
          const std::string root("abcproducer");
          for ( char c = 'a'; c <= 'n'; ++c ) {
             producer_names.emplace_back(root + std::string(1, c));
          }
       }
       setup_producer_accounts(producer_names);
       for (const auto& p: producer_names) {
          BOOST_REQUIRE_EQUAL( success(), regproducer(p) );
          produce_blocks(1);
          ilog( "------ get pro----------" );
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


BOOST_AUTO_TEST_SUITE(eosio_standby_tests)

BOOST_FIXTURE_TEST_CASE(standby_config_tests, eosio_standby_tester ) try {
  const auto& gs4 = get_global_state4();
  BOOST_TEST_REQUIRE( 0 == gs4["standby_pay_ratio"].as_double() );
  BOOST_TEST_REQUIRE( 0 == gs4["num_standby_slots"].as_uint64() );
  BOOST_TEST_REQUIRE( 0 == gs4["total_standby_share"].as_double() );

  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "setstdbratio"_n, mvo()("ratio", 0.5) )
  );

  const auto& gs42 = get_global_state4();
  BOOST_TEST_REQUIRE( 0.5 == gs42["standby_pay_ratio"].as_double() );


  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "setstdbslot"_n, mvo()("num_slots", 5) )
  );

  const auto& gs43 = get_global_state4();
  BOOST_TEST_REQUIRE( 5 == gs43["num_standby_slots"].as_uint64() );

} 
FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(standby_disallow_tests, eosio_standby_tester ) try {
  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "addstdbblock"_n, mvo()("account", alice) )
  );

  const auto& sps = get_standby_disallow_state(alice);
  BOOST_TEST_REQUIRE( alice == sps.owner );
} 
FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(standby_list, eosio_standby_tester ) try {
  
  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "setstdbratio"_n, mvo()("ratio", 0.5) )
  );

  BOOST_REQUIRE_EQUAL( 
    success(), push_action( config::system_account_name, "setstdbslot"_n, mvo()("num_slots", 5) )
  );

  auto producer_names = active_and_vote_producers_and_standbys();
  fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);

  ilog( "------ get producers----------" );
  wdump((producer_names));

  auto producer_keys = control->head_block_state()->active_schedule.producers;

  wdump((producer_keys));

  const auto& gs4 = get_global_state4();
  BOOST_TEST_REQUIRE( 0.5 == gs4["standby_pay_ratio"].as_double() );
  BOOST_TEST_REQUIRE( 5 == gs4["num_standby_slots"].as_uint64() );
  BOOST_TEST_REQUIRE( 0 == gs4["total_standby_share"].as_double() );

  auto standby_producers = get_stanby_table();
  wdump((standby_producers));
  BOOST_TEST_REQUIRE( 5 == standby_producers.size() );
  BOOST_TEST_REQUIRE( name("defproducerv") == standby_producers[0].owner );
  BOOST_TEST_REQUIRE( name("defproducerw") == standby_producers[1].owner );
  BOOST_TEST_REQUIRE( name("defproducerx") == standby_producers[2].owner );
  BOOST_TEST_REQUIRE( name("defproducery") == standby_producers[3].owner );
  BOOST_TEST_REQUIRE( name("defproducerz") == standby_producers[4].owner );
  fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::off);

}
FC_LOG_AND_RETHROW()



BOOST_AUTO_TEST_SUITE_END()
