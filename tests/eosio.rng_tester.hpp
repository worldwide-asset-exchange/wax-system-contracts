#pragma once
// The RNG test fixture: a system tester with the ORNG contract deployed at orng.wax and its
// code hash allow-listed, plus readers for global5, global.b and the ORNG treasury row.
// Shared by eosio.rng.tests.cpp and eosio.inflation_tests.cpp (WBP-2035).

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
    create_account_with_resources( RNG_CONTRACT, config::system_account_name, core_sym::from_string("100.0000"), false,
                                   core_sym::from_string("800.0000"), core_sym::from_string("800.0000") );
    transfer( config::system_account_name, RNG_CONTRACT, core_sym::from_string("100000000.0000"), config::system_account_name );
    BOOST_REQUIRE_EQUAL(success(), stake(RNG_CONTRACT, core_sym::from_string("30000000.0000"), core_sym::from_string("30000000.0000")) );
    produce_blocks( 2 );
    deploy_orng_contract( RNG_CONTRACT );
    {
      const auto& accnt = control->db().get<account_object,by_name>( RNG_CONTRACT );
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      rng_abi_ser.set_abi(abi, abi_serializer::create_yield_function(abi_serializer_max_time));
    }
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
