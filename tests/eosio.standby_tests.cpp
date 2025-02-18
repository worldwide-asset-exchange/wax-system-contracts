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
  bool            is_active = true;
};

FC_REFLECT(standby_producer_state, (owner)(standby_share)(last_standby_share_update)(is_active))

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
};


BOOST_AUTO_TEST_SUITE(eosio_standby_tests)

BOOST_FIXTURE_TEST_CASE(standby_config_tests, eosio_standby_tester ) try {
  const auto& gs4 = get_global_state4();
  BOOST_TEST_REQUIRE( 0 == gs4["standby_pay_ratio"].as_double() );
  BOOST_TEST_REQUIRE( 0 == gs4["num_standby_slots"].as_uint64() );
  BOOST_TEST_REQUIRE( 0 == gs4["total_standy_share"].as_double() );

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


BOOST_AUTO_TEST_SUITE_END()
