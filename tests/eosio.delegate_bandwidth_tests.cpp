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

BOOST_AUTO_TEST_SUITE(eosio_delegate_bandwidth_tester)

class eosio_delegate_bandwidth_tester : public eosio_system_tester {
public:
    action_result loanram(name lender, name receiver, const uint32_t& bytes, const uint32_t& limit) {
        return push_action(
            lender,
            N(loanram),
            mvo()
            ("lender", lender)
            ("receiver", receiver)
            ("bytes", bytes)
            ("limit", limit)
        );
    }
    
    action_result unloanram(name lender, name receiver, const uint32_t& bytes) {
        return push_action(
             lender,
             N(unloanram),
             mvo()
             ("lender", lender )
             ("receiver", receiver )
             ("bytes", bytes )
        );
    }

    static inline uint128 build_ram_loan_id(account_name lender, account_name receiver) {
      return static_cast<uint128>(lender.value)  | (static_cast<uint128>(receiver.value) << 64);
    }

    fc::variant get_ramloan(const account_name& lender, const account_name& receiver) {
        vector<char> data;
        const auto& db = control->db();
        const auto* t_id = db.find<eosio::chain::table_id_object, eosio::chain::by_code_scope_table>( boost::make_tuple( config::system_account_name, config::system_account_name, N(ramloan) ) );
        if ( !t_id ) {
            return fc::variant();
        }
        //FC_ASSERT( t_id != 0, "object not found" );

        const auto& idx = db.get_index<eosio::chain::key_value_index, eosio::chain::by_scope_primary>();

        auto itr = idx.lower_bound( boost::make_tuple( t_id->id, lender.value, receiver.value ) );
        if ( itr == idx.end() || itr->t_id != t_id->id ) {
            return fc::variant();
        }

        data.resize( itr->value.size() );
        memcpy( data.data(), itr->value.data(), data.size() );
        return abi_ser.binary_to_variant("ram_loan", data, abi_serializer_max_time);;
        // vector<char> data = get_row_by_account(config::system_account_name, config::system_account_name, N(ramloan), build_ram_loan_id(lender, receiver));
        // return data.empty() ? fc::variant() : abi_ser.binary_to_variant("ramloan", data, abi_serializer_max_time);
    }

    fc::variant get_userres(const account_name& act) {
        vector<char> data = get_row_by_account(config::system_account_name, act, N(userres), act);
        return data.empty() ? fc::variant() : abi_ser.binary_to_variant("user_resources", data, abi_serializer_max_time);
    }
};

BOOST_FIXTURE_TEST_CASE(loanram_action, eosio_delegate_bandwidth_tester) try {
    create_account_with_resources(N(lender111111), config::system_account_name, core_sym::from_string("100.0000"), false,
        core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    BOOST_REQUIRE_EQUAL(error("assertion failure with message: cannot lend to self"), loanram(N(lender111111), N(lender111111), 1024, 1024));

    BOOST_REQUIRE_EQUAL(error("assertion failure with message: receiver account does not exist"), loanram(N(lender111111), N(receiver1111), 1024, 1024));

    create_account_with_resources(N(receiver1111), config::system_account_name, core_sym::from_string("1.0000"), false,
        core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    BOOST_REQUIRE_EQUAL(error("assertion failure with message: bytes can not greater than limit"), loanram(N(lender111111), N(receiver1111), 99999, 1024));

    auto lender_res_before = get_userres(N(lender111111));
    uint32_t lender_ram_bytes_before;
    fc::from_variant(lender_res_before["ram_bytes"], lender_ram_bytes_before);
    BOOST_REQUIRE_EQUAL(error("assertion failure with message: overdrawn ram quota"), loanram(N(lender111111), N(receiver1111), lender_ram_bytes_before + 1000, lender_ram_bytes_before + 1000));

    auto receiver_res_before = get_userres(N(receiver1111));
    uint32_t receiver_ram_bytes_before;
    fc::from_variant(receiver_res_before["ram_bytes"], receiver_ram_bytes_before);

    uint32_t ram_lending_quota = 1000;
    BOOST_REQUIRE_EQUAL(success(), loanram(N(lender111111), N(receiver1111), ram_lending_quota, ram_lending_quota + 1000));

    produce_blocks(1);

    auto lender_res_after = get_userres(N(lender111111));
    uint32_t lender_ram_bytes_after;
    fc::from_variant(lender_res_after["ram_bytes"], lender_ram_bytes_after);

    auto receiver_res_after = get_userres(N(receiver1111));
    uint32_t receiver_ram_bytes_after;
    fc::from_variant(receiver_res_after["ram_bytes"], receiver_ram_bytes_after);
    BOOST_REQUIRE_EQUAL(lender_ram_bytes_after, lender_ram_bytes_before - ram_lending_quota);
    BOOST_REQUIRE_EQUAL(receiver_ram_bytes_after, receiver_ram_bytes_before + ram_lending_quota);

    auto ram_loan = get_ramloan(N(lender111111), N(receiver1111));
    BOOST_REQUIRE_EQUAL(ram_loan["bytes"], ram_lending_quota);
    BOOST_REQUIRE_EQUAL(ram_loan["limit"], ram_lending_quota + 1000);

    // already lended but lend more
    BOOST_REQUIRE_EQUAL(error("assertion failure with message: bytes can not greater than limit"), loanram(N(lender111111), N(receiver1111), 1, ram_lending_quota));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(unloanram_action, eosio_delegate_bandwidth_tester) try {
    create_account_with_resources(N(lender111111), config::system_account_name, core_sym::from_string("100.0000"), false,
        core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    BOOST_REQUIRE_EQUAL(error("assertion failure with message: cannot unlend to self"), unloanram(N(lender111111), N(lender111111), 1024));

    BOOST_REQUIRE_EQUAL(error("assertion failure with message: receiver account does not exist"), unloanram(N(lender111111), N(receiver1111), 1024));

    create_account_with_resources(N(receiver1111), config::system_account_name, core_sym::from_string("1.0000"), false,
        core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    BOOST_REQUIRE_EQUAL(error("assertion failure with message: loan does not exist"), unloanram(N(lender111111), N(receiver1111), 1024));


    BOOST_REQUIRE_EQUAL(success(), loanram(N(lender111111), N(receiver1111), 1000, 1000));

    produce_blocks(1);

    auto lender_res_before = get_userres(N(lender111111));
    uint32_t lender_ram_bytes_before;
    fc::from_variant(lender_res_before["ram_bytes"], lender_ram_bytes_before);

    auto receiver_res_before = get_userres(N(receiver1111));
    uint32_t receiver_ram_bytes_before;
    fc::from_variant(receiver_res_before["ram_bytes"], receiver_ram_bytes_before);

    uint32_t ram_unlending_quota = 500;
    BOOST_REQUIRE_EQUAL(success(), unloanram(N(lender111111), N(receiver1111), ram_unlending_quota));

    auto lender_res_after = get_userres(N(lender111111));
    uint32_t lender_ram_bytes_after;
    fc::from_variant(lender_res_after["ram_bytes"], lender_ram_bytes_after);

    auto receiver_res_after = get_userres(N(receiver1111));
    uint32_t receiver_ram_bytes_after;
    fc::from_variant(receiver_res_after["ram_bytes"], receiver_ram_bytes_after);
    BOOST_REQUIRE_EQUAL(lender_ram_bytes_after, lender_ram_bytes_before + 500);
    BOOST_REQUIRE_EQUAL(receiver_ram_bytes_after, receiver_ram_bytes_before - 500);

    auto ram_loan = get_ramloan(N(lender111111), N(receiver1111));
    BOOST_REQUIRE_EQUAL(ram_loan["bytes"], 500);
    BOOST_REQUIRE_EQUAL(ram_loan["limit"], 1000);

    BOOST_REQUIRE_EQUAL(success(), unloanram(N(lender111111), N(receiver1111), ram_unlending_quota));

    ram_loan = get_ramloan(N(lender111111), N(receiver1111));
    BOOST_REQUIRE_EQUAL(ram_loan.is_null(), true);
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
