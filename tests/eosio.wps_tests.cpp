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

#include "eosio.system_tester.hpp"

using namespace eosio_system;

BOOST_AUTO_TEST_SUITE(eosio_wps_tests)

class eosio_wps_tester : public eosio_system_tester {
public:
    action_result regcommittee(name sender, name committeeman, const string& category, bool is_oversight) {
        return push_action(
            sender,
            "regcommittee"_n,
            mvo()
            ("committeeman", committeeman)
            ("category", category)
            ("is_oversight", is_oversight)
        );
    }

    action_result edcommittee(name sender, name committeeman, const string& category, bool is_oversight) {
        return push_action(
             sender,
             "edcommittee"_n,
             mvo()
             ("committeeman", committeeman )
             ("category", category )
             ("is_oversight", is_oversight )
        );
    }

    action_result rmvcommittee(name sender, name committeeman) {
        return push_action(
             sender,
             "rmvcommittee"_n,
             mvo()
             ("committeeman", committeeman)
        );
    }

    action_result regreviewer(name sender, name committee, name reviewer, const string& first_name, const string& last_name) {
        return push_action(
                sender,
                "regreviewer"_n,
                mvo()
                ("committee", committee)
                ("reviewer", reviewer)
                ("first_name", first_name)
                ("last_name", last_name)

        );
    }

    action_result editreviewer(name sender, name committee, name reviewer, const string& first_name, const string& last_name) {
        return push_action(
                sender,
                "editreviewer"_n,
                mvo()
                ("committee", committee)
                ("reviewer", reviewer)
                ("first_name", first_name)
                ("last_name", last_name)

        );
    }

    action_result rmvreviewer(name sender, name committee, name reviewer) {
        return push_action(
                sender,
                "rmvreviewer"_n,
                mvo()
                        ("committee", committee)
                        ("reviewer", reviewer)

        );
    }

    action_result regproposer(name sender,
                              name account,
                              const string& first_name,
                              const string& last_name,
                              const string& img_url,
                              const string& bio,
                              const string& country,
                              const string& telegram,
                              const string& website,
                              const string& linkedin) {
        return push_action(
                sender,
                "regproposer"_n,
                mvo()
                        ("account", account)
                        ("first_name", first_name)
                        ("last_name", last_name)
                        ("img_url", img_url)
                        ("bio", bio)
                        ("country", country)
                        ("telegram", telegram)
                        ("website", website)
                        ("linkedin", linkedin)
        );
    }

    action_result editproposer(name sender,
                              name account,
                              const string& first_name,
                              const string& last_name,
                              const string& img_url,
                              const string& bio,
                              const string& country,
                              const string& telegram,
                              const string& website,
                              const string& linkedin) {
        return push_action(
                sender,
                "editproposer"_n,
                mvo()
                        ("account", account)
                        ("first_name", first_name)
                        ("last_name", last_name)
                        ("img_url", img_url)
                        ("bio", bio)
                        ("country", country)
                        ("telegram", telegram)
                        ("website", website)
                        ("linkedin", linkedin)
        );
    }

    action_result rmvproposer(name sender,
                              name account) {
        return push_action(
                sender,
                "rmvproposer"_n,
                mvo()
                        ("account", account)
        );
    }

    action_result regproposal(name sender,
                              name proposer,
                              name committee,
                              uint16_t subcategory,
                              const string& title,
                              const string& summary,
                              const string& project_img_url,
                              const string& description,
                              const string& roadmap,
                              uint64_t duration,
                              const vector<string>& members,
                              const asset& funding_goal,
                              uint32_t total_iterations) {
        return push_action(
                sender,
                "regproposal"_n,
                mvo()
                        ("proposer", proposer)
                        ("committee", committee)
                        ("subcategory", subcategory)
                        ("title", title)
                        ("summary", summary)
                        ("project_img_url", project_img_url)
                        ("description", description)
                        ("roadmap", roadmap)
                        ("duration", duration)
                        ("members", members)
                        ("funding_goal", funding_goal)
                        ("total_iterations", total_iterations)
        );
    }

    action_result editproposal(name sender,
                              name proposer,
                              name committee,
                              uint16_t subcategory,
                              const string& title,
                              const string& summary,
                              const string& project_img_url,
                              const string& description,
                              const string& roadmap,
                              uint64_t duration,
                              const vector<string>& members,
                              const asset& funding_goal,
                              uint32_t total_iterations) {
        return push_action(
                sender,
                "editproposal"_n,
                mvo()
                        ("proposer", proposer)
                        ("committee", committee)
                        ("subcategory", subcategory)
                        ("title", title)
                        ("summary", summary)
                        ("project_img_url", project_img_url)
                        ("description", description)
                        ("roadmap", roadmap)
                        ("duration", duration)
                        ("members", members)
                        ("funding_goal", funding_goal)
                        ("total_iterations", total_iterations)
        );
    }

    action_result setwpsenv(name sender, uint32_t total_voting_percent, uint32_t duration_of_voting,
                            uint32_t max_duration_of_funding, uint32_t total_iteration_of_funding) {
        return push_action(
                sender,
                "setwpsenv"_n,
                mvo()
                        ("total_voting_percent", total_voting_percent)
                        ("duration_of_voting", duration_of_voting)
                        ("max_duration_of_funding", max_duration_of_funding)
                        ("total_iteration_of_funding", total_iteration_of_funding)
        );
    }

    action_result acceptprop(name sender, name reviewer, name proposer) {
        return push_action(
                sender,
                "acceptprop"_n,
                mvo()
                        ("reviewer", reviewer)
                        ("proposer", proposer)

        );
    }

    action_result rejectprop(name sender, name reviewer, name proposer, const string& reason) {
        return push_action(
                sender,
                "rejectprop"_n,
                mvo()
                        ("reviewer", reviewer)
                        ("proposer", proposer)
                        ("reason", reason)

        );
    }

    action_result rejectfund(name sender, name committeeman, name proposer, const string& reason) {
        return push_action(
                sender,
                "rejectfund"_n,
                mvo()
                        ("committeeman", committeeman)
                        ("proposer", proposer)
                        ("reason", reason)

        );
    }

    action_result voteproposal(name sender, name voter_name, const std::vector<name>& proposals) {
        return push_action(
                sender,
                "voteproposal"_n,
                mvo()
                        ("voter_name", voter_name)
                        ("proposals", proposals)
        );
    }

    action_result approve(name sender, name reviewer, name proposer) {
        return push_action(
                sender,
                "approve"_n,
                mvo()
                        ("reviewer", reviewer)
                        ("proposer", proposer)
        );
    }

    action_result claimfunds(name sender, name account) {
        return push_action(
                sender,
                "claimfunds"_n,
                mvo()
                        ("account", account)
        );
    }

    action_result regproducer( const account_name& acnt, int params_fixture = 1 ) {
        action_result r = push_action( acnt, "regproducer"_n, mvo()
                ("producer",  acnt )
                ("producer_key", get_public_key( acnt, "active" ) )
                ("url", "" )
                ("location", 0 )
        );
        BOOST_REQUIRE_EQUAL( success(), r);
        return r;
    }

    action_result vote( const account_name& voter, const std::vector<account_name>& producers, const account_name& proxy = name(0) ) {
        return push_action(voter, "voteproducer"_n, mvo()
                ("voter",     voter)
                ("proxy",     proxy)
                ("producers", producers));
    }

    action_result cleanvotes(name sender, name reviewer, name proposer, uint64_t begin, uint64_t end) {
        return push_action(
                sender,
                "cleanvotes"_n,
                mvo()
                        ("reviewer", reviewer)
                        ("proposer", proposer)
                        ("begin", begin)
                        ("end", end)
        );
    }

    fc::variant get_committee(const account_name& act) {
        vector<char> data = get_row_by_account(config::system_account_name, config::system_account_name, "committees"_n, act);
        return data.empty() ? fc::variant() : abi_ser.binary_to_variant("committee", data, abi_serializer_max_time);
    }

    fc::variant get_reviewer(const account_name& act) {
        vector<char> data = get_row_by_account(config::system_account_name, config::system_account_name, "reviewers"_n, act);
        return data.empty() ? fc::variant() : abi_ser.binary_to_variant("reviewer", data, abi_serializer_max_time);
    }

    fc::variant get_proposer(const account_name& act) {
        vector<char> data = get_row_by_account(config::system_account_name, config::system_account_name, "proposers"_n, act);
        return data.empty() ? fc::variant() : abi_ser.binary_to_variant("proposer", data, abi_serializer_max_time);
    }

    fc::variant get_proposal(const account_name& act) {
        vector<char> data = get_row_by_account(config::system_account_name, config::system_account_name, "proposals"_n, act);
        return data.empty() ? fc::variant() : abi_ser.binary_to_variant("proposal", data, abi_serializer_max_time);
    }

    fc::variant get_wpsvoter(const account_name& act) {
        vector<char> data = get_row_by_account(config::system_account_name, config::system_account_name, "wpsvoters"_n, act);
        return data.empty() ? fc::variant() : abi_ser.binary_to_variant("wps_voter", data, abi_serializer_max_time);
    }
};

BOOST_FIXTURE_TEST_CASE(wpsenv_set, eosio_wps_tester) try {
    create_account_with_resources("committee111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    BOOST_REQUIRE_EQUAL(error("missing authority of eosio"), setwpsenv("committee111"_n, 5, 30, 500, 6));
    BOOST_REQUIRE_EQUAL(success(), setwpsenv(config::system_account_name, 5, 30, 500, 6));
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(committee_reg_edit_rmv, eosio_wps_tester) try {

    create_account_with_resources("committee111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    setwpsenv(config::system_account_name, 5, 30, 500, 6);

    BOOST_REQUIRE_EQUAL(error("missing authority of eosio"), regcommittee("committee111"_n, "committee111"_n, "categoryX", true));
    BOOST_REQUIRE_EQUAL(success(), regcommittee(config::system_account_name, "committee111"_n, "categoryX", true));
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("This account has already been registered as a committee"), regcommittee(config::system_account_name, "committee111"_n, "categoryX", true));


    produce_blocks(1);
    auto committee = get_committee("committee111"_n);
    BOOST_REQUIRE_EQUAL(committee["category"], "categoryX");

    BOOST_REQUIRE_EQUAL(error("missing authority of eosio"), edcommittee("committee111"_n, "committee111"_n, "categoryZ", true));
    edcommittee(config::system_account_name, "committee111"_n, "categoryY", true);

    produce_blocks(1);

    committee = get_committee("committee111"_n);
    BOOST_REQUIRE_EQUAL(committee["category"], "categoryY");

    BOOST_REQUIRE_EQUAL(error("missing authority of eosio"), rmvcommittee("committee111"_n, "committee111"_n));
    BOOST_REQUIRE_EQUAL(success(), rmvcommittee(config::system_account_name, "committee111"_n));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(reviewer_reg_edit_rmv, eosio_wps_tester) try {

    create_account_with_resources("committee111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("reviewer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    setwpsenv(config::system_account_name, 5, 30, 500, 6);
    regcommittee(config::system_account_name, "committee111"_n, "categoryX", true);

    BOOST_REQUIRE_EQUAL(error("missing authority of committee111"),
        regreviewer("reviewer1111"_n, "committee111"_n, "reviewer1111"_n, "bob", "bob"));

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("Account not found in committee table"),
            regreviewer("reviewer1111"_n, "reviewer1111"_n, "reviewer1111"_n, "bob", "bob"));

    BOOST_REQUIRE_EQUAL(success(), regreviewer("committee111"_n, "committee111"_n, "reviewer1111"_n, "bob", "bob"));

    produce_blocks(1);

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("This account has already been registered as a reviewer"),
            regreviewer("committee111"_n, "committee111"_n, "reviewer1111"_n, "bob", "bob"));

    auto reviewer = get_reviewer("reviewer1111"_n);
    BOOST_REQUIRE_EQUAL(reviewer["last_name"], "bob");

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("Account not found in committee table"),
        editreviewer("reviewer1111"_n, "reviewer1111"_n, "reviewer1111"_n, "bob", "bob"));

    editreviewer("committee111"_n, "committee111"_n, "reviewer1111"_n, "bob", "smith");

    produce_blocks(1);

    reviewer = get_reviewer("reviewer1111"_n);
    BOOST_REQUIRE_EQUAL(reviewer["last_name"], "smith");

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("Account not found in committee table"),
        rmvreviewer("reviewer1111"_n, "reviewer1111"_n, "reviewer1111"_n));

    BOOST_REQUIRE_EQUAL(success(), rmvreviewer("committee111"_n, "committee111"_n, "reviewer1111"_n));

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(proposer_reg_edit_rmv, eosio_wps_tester) try {

create_account_with_resources("proposer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
create_account_with_resources("randomuser11"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

setwpsenv(config::system_account_name, 5, 30, 500, 6);

BOOST_REQUIRE_EQUAL(error("missing authority of proposer1111"),
        regproposer("randomuser11"_n, "proposer1111"_n, "user", "one", "img_url", "bio", "country", "telegram", "website", "linkedin"));

BOOST_REQUIRE_EQUAL(success(),
        regproposer("proposer1111"_n, "proposer1111"_n, "user", "one", "img_url", "bio", "country", "telegram", "website", "linkedin"));

BOOST_REQUIRE_EQUAL(wasm_assert_msg("This account has already been registered as a proposer"),
        regproposer("proposer1111"_n, "proposer1111"_n, "user", "one", "img_url", "bio", "country", "telegram", "website", "linkedin"));

produce_blocks(1);

auto proposer = get_proposer("proposer1111"_n);

BOOST_REQUIRE_EQUAL(proposer["first_name"], "user");

editproposer("proposer1111"_n, "proposer1111"_n, "proposer", "one", "img_url", "bio", "country", "telegram", "website", "linkedin");

produce_blocks(1);

proposer = get_proposer("proposer1111"_n);

BOOST_REQUIRE_EQUAL(proposer["first_name"], "proposer");

BOOST_REQUIRE_EQUAL(wasm_assert_msg("Account not found in proposer table"),
    rmvproposer("randomuser11"_n, "randomuser11"_n));

BOOST_REQUIRE_EQUAL(success(), rmvproposer("proposer1111"_n, "proposer1111"_n));

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(proposal_reg_edit_rmv, eosio_wps_tester) try {

    create_account_with_resources("committee111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("reviewer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("proposer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("randomuser11"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

setwpsenv(config::system_account_name, 5, 30, 500, 6);
regcommittee(config::system_account_name, "committee111"_n, "categoryX", true);
regreviewer("committee111"_n, "committee111"_n, "reviewer1111"_n, "bob", "bob");
regproposer("proposer1111"_n, "proposer1111"_n, "user", "one", "img_url", "bio", "country", "telegram", "website", "linkedin");

BOOST_REQUIRE_EQUAL(error("missing authority of proposer1111"),
        regproposal("randomuser11"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
                    "description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 3));

BOOST_REQUIRE_EQUAL(success(),
        regproposal("proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
                    "description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 3));

produce_blocks(1);

BOOST_REQUIRE_EQUAL(wasm_assert_msg("This account has already registered a proposal"),
        regproposal("proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
                    "description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 3));

auto proposal = get_proposal("proposer1111"_n);

BOOST_REQUIRE_EQUAL(proposal["title"], "title");

BOOST_REQUIRE_EQUAL(success(), editproposal("proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "First proposal", "summary", "project_img_url",
"description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 3));

produce_blocks(1);

proposal = get_proposal("proposer1111"_n);

BOOST_REQUIRE_EQUAL(proposal["title"], "First proposal");

} FC_LOG_AND_RETHROW()


// WCAP-SYS-2026-003 (WBP-1989). editproposal must enforce the same input floors and
// ceilings as regproposal. Before the fix it required only duration > 0, so a PENDING
// proposal registered at 30 days could be edited down to 1 and its whole funding goal
// claimed within 24 hours of approval - collapsing the instalment schedule claimfunds
// enforces. Both actions now validate through one helper so they cannot drift again.
BOOST_FIXTURE_TEST_CASE(proposal_edit_enforces_shared_floors, eosio_wps_tester) try {

    create_account_with_resources("committee111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("proposer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("proposer2222"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    setwpsenv(config::system_account_name, 5, 30, 500, 6);
    regcommittee(config::system_account_name, "committee111"_n, "categoryX", true);
    regproposer("proposer1111"_n, "proposer1111"_n, "user", "one", "img_url", "bio", "country", "telegram", "website", "linkedin");
    regproposer("proposer2222"_n, "proposer2222"_n, "user", "two", "img_url", "bio", "country", "telegram", "website", "linkedin");

    // Registered exactly at the floor: accepted.
    BOOST_REQUIRE_EQUAL(success(),
        regproposal("proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
                    "description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 3));
    produce_blocks(1);

    // The defect: editing the PENDING proposal below the floor was accepted. It must be
    // refused with the same message regproposal uses.
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("duration should be at least 30 days"),
        editproposal("proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
                     "description", "roadmap", 1, {"user"}, core_sym::from_string("9000.0000"), 3));

    // The register side still holds the floor too - both paths, one helper.
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("duration should be at least 30 days"),
        regproposal("proposer2222"_n, "proposer2222"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
                    "description", "roadmap", 29, {"user"}, core_sym::from_string("9000.0000"), 3));

    // The ceiling (wpsenv.max_duration_of_funding = 500) is enforced on edit with the one
    // unified message; editproposal previously said "duration maximum exceeded".
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("this proposal is over the maximum duration"),
        editproposal("proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
                     "description", "roadmap", 501, {"user"}, core_sym::from_string("9000.0000"), 3));

    // Editing at the floor is fine, and the stored value is what was asked for.
    BOOST_REQUIRE_EQUAL(success(),
        editproposal("proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
                     "description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 3));
    produce_blocks(1);
    BOOST_REQUIRE_EQUAL(30u, get_proposal("proposer1111"_n)["duration"].as_uint64());

    // The iterations floor is 1 on BOTH actions - the value c6c2942 (2020) chose for
    // register and that editproposal had never picked up. Two instalments is now legal
    // on edit, as it always was on register; zero is not, on either.
    BOOST_REQUIRE_EQUAL(success(),
        editproposal("proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
                     "description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 2));
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("total number of iterations must be at least 1"),
        editproposal("proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
                     "description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 0));
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("total number of iterations must be at least 1"),
        regproposal("proposer2222"_n, "proposer2222"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
                    "description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 0));

    // setwpsenv may not set a ceiling that undercuts the floor.
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("max_duration_of_funding must be at least 30 days, the proposal duration floor"),
        setwpsenv(config::system_account_name, 5, 30, 29, 6));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(reviewer_accept_reject, eosio_wps_tester) try {

create_account_with_resources("committee111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
create_account_with_resources("reviewer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
create_account_with_resources("proposer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
create_account_with_resources("proposer2222"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
create_account_with_resources("randomuser11"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

setwpsenv(config::system_account_name, 5, 30, 500, 6);
regcommittee(config::system_account_name, "committee111"_n, "categoryX", true);
regreviewer("committee111"_n, "committee111"_n, "reviewer1111"_n, "bob", "bob");
regproposer("proposer1111"_n, "proposer1111"_n, "user", "one", "img_url", "bio", "country", "telegram", "website", "linkedin");
regproposer("proposer2222"_n, "proposer2222"_n, "user", "two", "img_url", "bio", "country", "telegram", "website", "linkedin");
regproposal("proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
"description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 3);
regproposal("proposer2222"_n, "proposer2222"_n, "committee111"_n, 1, "title", "summary", "project_img_url", "description", "roadmap", 30, {"user 2"}, core_sym::from_string("9000.0000"), 3);

BOOST_REQUIRE_EQUAL(error("missing authority of reviewer1111"),
        acceptprop("proposer1111"_n, "reviewer1111"_n, "proposer1111"_n));

BOOST_REQUIRE_EQUAL(wasm_assert_msg("Account not found in reviewers table"),
        acceptprop("proposer1111"_n, "proposer1111"_n, "proposer1111"_n));

BOOST_REQUIRE_EQUAL(error("missing authority of reviewer1111"),
        rejectprop("proposer1111"_n, "reviewer1111"_n, "proposer1111"_n, "reason"));

BOOST_REQUIRE_EQUAL(wasm_assert_msg("Account not found in reviewers table"),
        rejectprop("proposer1111"_n, "proposer1111"_n, "proposer1111"_n, "reason"));

BOOST_REQUIRE_EQUAL(success(),
        rejectprop("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n, "reason"));

produce_blocks(1);

auto proposal = get_proposal("proposer1111"_n);

BOOST_REQUIRE_EQUAL(proposal["status"], 2);

produce_blocks(1);

BOOST_REQUIRE_EQUAL(success(), acceptprop("reviewer1111"_n, "reviewer1111"_n, "proposer2222"_n));

produce_blocks(1);

proposal = get_proposal("proposer2222"_n);

BOOST_REQUIRE_EQUAL(proposal["status"], 3);

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(proposal_vote_claim, eosio_wps_tester) try {

    create_account_with_resources("committee111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("reviewer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("proposer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("randomuser11"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    cross_15_percent_threshold();

    setwpsenv(config::system_account_name, 35, 30, 500, 6);
    regcommittee(config::system_account_name, "committee111"_n, "categoryX", true);
    regreviewer("committee111"_n, "committee111"_n, "reviewer1111"_n, "bob", "bob");
    regproposer("proposer1111"_n, "proposer1111"_n, "user", "one", "img_url", "bio", "country", "telegram", "website", "linkedin");
    regproposal("proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
    "description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 3);
    acceptprop("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n);


    create_account_with_resources("smallvoter11"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    create_account_with_resources("bigvoter1111"_n, config::system_account_name, core_sym::from_string("10000.0000"), false,
core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    issue_and_transfer( "smallvoter11", core_sym::from_string("1000.0000"),  config::system_account_name );
    BOOST_REQUIRE_EQUAL( success(), stake( "smallvoter11", core_sym::from_string("100.0002"), core_sym::from_string("50.0001") ) );

    issue_and_transfer( "bigvoter1111", core_sym::from_string("100000000.0000"),  config::system_account_name );
    BOOST_REQUIRE_EQUAL( success(), stake( "bigvoter1111", core_sym::from_string("50000000.0000"), core_sym::from_string("50000000.0000") ) );

    // Make a producer account to create an appropriate producer_vote_weight
    create_account_with_resources("prod11111111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    // prod11111111 registers to be a producer
    BOOST_REQUIRE_EQUAL( success(), regproducer( "prod11111111"_n, 1) );

    produce_blocks(1);

    // bigvoter1111 votes for prod11111111
    BOOST_REQUIRE_EQUAL( success(), vote( "bigvoter1111"_n, { "prod11111111"_n } ) );

    produce_block(fc::days(10));
    BOOST_REQUIRE_EQUAL( success(), push_action("prod11111111"_n, "claimrewards"_n, mvo()("owner", "prod11111111")) );

    BOOST_REQUIRE_EQUAL(error("missing authority of smallvoter11"), voteproposal("proposer1111"_n, "smallvoter11"_n, {"proposer1111"_n}));

    BOOST_REQUIRE_EQUAL(success(), voteproposal("smallvoter11"_n, "smallvoter11"_n, {"proposer1111"_n}));

    produce_blocks(1);

    auto proposal = get_proposal("proposer1111"_n);

    BOOST_REQUIRE_EQUAL(proposal["status"], 3);

    BOOST_REQUIRE_EQUAL(success(), voteproposal("bigvoter1111"_n, "bigvoter1111"_n, {"proposer1111"_n}));

    produce_blocks(1);

    proposal = get_proposal("proposer1111"_n);

    BOOST_REQUIRE_EQUAL(proposal["status"], 4);

    BOOST_REQUIRE_EQUAL(error("missing authority of reviewer1111"),
        approve("proposer1111"_n, "reviewer1111"_n, "proposer1111"_n));

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("Account not found in reviewers table"),
        approve("proposer1111"_n, "proposer1111"_n, "proposer1111"_n));

    BOOST_REQUIRE_EQUAL(success(),
        approve("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n));

    produce_blocks(1);

    proposal = get_proposal("proposer1111"_n);

    BOOST_REQUIRE_EQUAL(proposal["status"], 5);

    produce_blocks(1);

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("Please wait until the end of this interval to claim funding"), claimfunds("proposer1111"_n, "proposer1111"_n));

    produce_block( fc::days(10) );

    BOOST_REQUIRE_EQUAL(error("missing authority of proposer1111"), claimfunds("randomuser11"_n, "proposer1111"_n));

    BOOST_REQUIRE_EQUAL(success(), claimfunds("proposer1111"_n, "proposer1111"_n));

    produce_blocks(1);

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("Please wait until the end of this interval to claim funding"), claimfunds("proposer1111"_n, "proposer1111"_n));

    produce_block( fc::days(10) );

    BOOST_REQUIRE_EQUAL(success(), claimfunds("proposer1111"_n, "proposer1111"_n));

    produce_blocks(1);

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("Please wait until the end of this interval to claim funding"), claimfunds("proposer1111"_n, "proposer1111"_n));

    produce_block( fc::days(10) );

    BOOST_REQUIRE_EQUAL(success(), claimfunds("proposer1111"_n, "proposer1111"_n));

    produce_blocks(1);

    proposal = get_proposal("proposer1111"_n);

    BOOST_REQUIRE_EQUAL(proposal["status"], 6);

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("Proposal::status is not PROPOSAL_STATUS::APPROVED"), claimfunds("proposer1111"_n, "proposer1111"_n));

    // WBP-2001: a COMPLETED row can be removed by a reviewer of its committee, and only once.
    BOOST_REQUIRE_EQUAL(success(), push_action("reviewer1111"_n, "rmvcompleted"_n, mvo()("reviewer", "reviewer1111")("proposer", "proposer1111")));
    BOOST_REQUIRE(get_proposal("proposer1111"_n).is_null());
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("Proposal not found in completed proposals table"),
        push_action("reviewer1111"_n, "rmvcompleted"_n, mvo()("reviewer", "reviewer1111")("proposer", "proposer1111")));

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE(proposal_reject_fund, eosio_wps_tester) try {

    create_account_with_resources("committee111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("reviewer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("proposer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("randomuser11"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    cross_15_percent_threshold();

    setwpsenv(config::system_account_name, 5, 30, 500, 6);
    regcommittee(config::system_account_name, "committee111"_n, "categoryX", true);
    regreviewer("committee111"_n, "committee111"_n, "reviewer1111"_n, "bob", "bob");
    regproposer("proposer1111"_n, "proposer1111"_n, "user", "one", "img_url", "bio", "country", "telegram", "website", "linkedin");
    regproposal("proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
    "description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 3);
    acceptprop("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n);

    create_account_with_resources("bigvoter1111"_n, config::system_account_name, core_sym::from_string("10000.0000"), false, core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    issue_and_transfer( "bigvoter1111", core_sym::from_string("100000000.0000"),  config::system_account_name );
    BOOST_REQUIRE_EQUAL( success(), stake( "bigvoter1111", core_sym::from_string("50000000.0000"), core_sym::from_string("50000000.0000") ) );

    // Make a producer account to create an appropriate producer_vote_weight
    create_account_with_resources("prod11111111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    // prod11111111 registers to be a producer
    BOOST_REQUIRE_EQUAL( success(), regproducer( "prod11111111"_n, 1) );

    produce_blocks(1);

    // bigvoter1111 votes for prod11111111
    BOOST_REQUIRE_EQUAL( success(), vote( "bigvoter1111"_n, { "prod11111111"_n } ) );

    produce_block(fc::days(10));
    BOOST_REQUIRE_EQUAL( success(), push_action("prod11111111"_n, "claimrewards"_n, mvo()("owner", "prod11111111")) );

    BOOST_REQUIRE_EQUAL(success(), voteproposal("bigvoter1111"_n, "bigvoter1111"_n, {"proposer1111"_n}));

    produce_blocks(1);

    BOOST_REQUIRE_EQUAL(success(),
        approve("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n));

    produce_block( fc::days(11) );

    BOOST_REQUIRE_EQUAL(success(), claimfunds("proposer1111"_n, "proposer1111"_n));

    BOOST_REQUIRE_EQUAL(error("missing authority of committee111"), rejectfund("proposer1111"_n, "committee111"_n, "proposer1111"_n, "reason"));
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("Proposal creator does not exist"), rejectfund("committee111"_n, "committee111"_n, "dne"_n, "reason"));
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("Proposal not found in proposal table"), rejectfund("committee111"_n, "committee111"_n, "committee111"_n, "reason"));
    BOOST_REQUIRE_EQUAL(wasm_assert_msg("Account not found in committee table"), rejectfund("proposer1111"_n, "proposer1111"_n, "proposer1111"_n, "reason"));
    BOOST_REQUIRE_EQUAL(success(), rejectfund("committee111"_n, "committee111"_n, "proposer1111"_n, "reason"));

    produce_block( fc::days(10) );

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("Proposal::status is not PROPOSAL_STATUS::APPROVED"), claimfunds("proposer1111"_n, "proposer1111"_n));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(proposal_vote_increase_stake, eosio_wps_tester) try {

    create_account_with_resources("committee111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("reviewer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("proposer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("randomuser11"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    cross_15_percent_threshold();

    setwpsenv(config::system_account_name, 35, 30, 500, 6);
    regcommittee(config::system_account_name, "committee111"_n, "categoryX", true);
    regreviewer("committee111"_n, "committee111"_n, "reviewer1111"_n, "bob", "bob");
    regproposer("proposer1111"_n, "proposer1111"_n, "user", "one", "img_url", "bio", "country", "telegram", "website", "linkedin");
    regproposal("proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
    "description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 3);
    acceptprop("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n);

    create_account_with_resources("bigvoter1111"_n, config::system_account_name, core_sym::from_string("10000.0000"), false,
core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    issue_and_transfer( "bigvoter1111", core_sym::from_string("100000000.0000"),  config::system_account_name );
    BOOST_REQUIRE_EQUAL( success(), stake( "bigvoter1111", core_sym::from_string("50000.0000"), core_sym::from_string("50000.0000") ) );

    produce_blocks(1);

    create_account_with_resources("prodvoter111"_n, config::system_account_name, core_sym::from_string("10000.0000"), false, core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    issue_and_transfer( "prodvoter111", core_sym::from_string("100000000.0000"),  config::system_account_name );
    BOOST_REQUIRE_EQUAL( success(), stake( "prodvoter111", core_sym::from_string("50000000.0000"), core_sym::from_string("50000000.0000") ) );

    produce_blocks(1);

    // Make a producer account to create an appropriate producer_vote_weight
    create_account_with_resources("prod11111111"_n, config::system_account_name, core_sym::from_string("100.0000"), false, core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    // prod11111111 registers to be a producer
    BOOST_REQUIRE_EQUAL( success(), regproducer( "prod11111111"_n, 1) );

    produce_blocks(1);

    // prodvoter111 votes for prod11111111
    BOOST_REQUIRE_EQUAL( success(), vote( "prodvoter111"_n, { "prod11111111"_n } ) );

    produce_blocks(1);

    BOOST_REQUIRE_EQUAL(success(), voteproposal("bigvoter1111"_n, "bigvoter1111"_n, {"proposer1111"_n}));

    produce_blocks(1);

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("Proposal::status is not PROPOSAL_STATUS::FINISHED_VOTING"),
        approve("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n));

    BOOST_REQUIRE_EQUAL( success(), stake( "bigvoter1111", core_sym::from_string("49950000.0000"), core_sym::from_string("49950000.0000") ) );

    produce_blocks(100);

    BOOST_REQUIRE_EQUAL(success(), approve("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(proposal_vote_decrease_stake, eosio_wps_tester) try {

    create_account_with_resources("committee111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("reviewer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("proposer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("randomuser11"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    cross_15_percent_threshold();

    setwpsenv(config::system_account_name, 30, 30, 500, 6);
    regcommittee(config::system_account_name, "committee111"_n, "categoryX", true);
    regreviewer("committee111"_n, "committee111"_n, "reviewer1111"_n, "bob", "bob");
    regproposer("proposer1111"_n, "proposer1111"_n, "user", "one", "img_url", "bio", "country", "telegram", "website", "linkedin");
    regproposal("proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
    "description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 3);
    acceptprop("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n);

    create_account_with_resources("bigvoter1111"_n, config::system_account_name, core_sym::from_string("10000.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    issue_and_transfer( "bigvoter1111", core_sym::from_string("100000000.0000"),  config::system_account_name );
    BOOST_REQUIRE_EQUAL( success(), stake( "bigvoter1111", core_sym::from_string("3700000.0000"), core_sym::from_string("3700000.0000") ) );

    produce_blocks(1);

    create_account_with_resources("prodvoter111"_n, config::system_account_name, core_sym::from_string("10000.0000"), false, core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    issue_and_transfer( "prodvoter111", core_sym::from_string("100000000.0000"),  config::system_account_name );
    BOOST_REQUIRE_EQUAL( success(), stake( "prodvoter111", core_sym::from_string("50000000.0000"), core_sym::from_string("50000000.0000") ) );

    produce_blocks(1);

    // Make a producer account to create an appropriate producer_vote_weight
    create_account_with_resources("prod11111111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    // prod11111111 registers to be a producer
    BOOST_REQUIRE_EQUAL( success(), regproducer( "prod11111111"_n, 1) );

    produce_blocks(1);

    // prodvoter111 votes for prod11111111
    BOOST_REQUIRE_EQUAL( success(), vote( "prodvoter111"_n, { "prod11111111"_n } ) );

    produce_blocks(1);

    // bigvoter1111 votes for prod11111111
    BOOST_REQUIRE_EQUAL( success(), vote( "bigvoter1111"_n, { "prod11111111"_n } ) );

    produce_blocks(1);

    BOOST_REQUIRE_EQUAL(success(), voteproposal("bigvoter1111"_n, "bigvoter1111"_n, {"proposer1111"_n}));

    produce_blocks(100);

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("Proposal::status is not PROPOSAL_STATUS::FINISHED_VOTING"),
        approve("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n));

    produce_blocks(100);

    BOOST_REQUIRE_EQUAL( success(), unstake( "bigvoter1111", "bigvoter1111", core_sym::from_string("3700000.0000"), core_sym::from_string("3700000.0000") ) );

    produce_blocks(1);

    BOOST_REQUIRE_EQUAL( success(), stake( "bigvoter1111", core_sym::from_string("1.0000"), core_sym::from_string("1.0000") ) );

    produce_blocks(1);

    BOOST_REQUIRE_EQUAL(wasm_assert_msg("Proposal::status is not PROPOSAL_STATUS::FINISHED_VOTING"),
        approve("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n));

    BOOST_REQUIRE_EQUAL( success(), stake( "bigvoter1111", core_sym::from_string("45000000.0000"), core_sym::from_string("45000000.0000") ) );

    produce_blocks(100);

    BOOST_REQUIRE_EQUAL(success(),
        approve("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(proposal_stake_unstake_repetition, eosio_wps_tester) try {

    create_account_with_resources("committee111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("reviewer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("proposer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("randomuser11"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    cross_15_percent_threshold();

    setwpsenv(config::system_account_name, 30, 30, 500, 6);
    regcommittee(config::system_account_name, "committee111"_n, "categoryX", true);
    regreviewer("committee111"_n, "committee111"_n, "reviewer1111"_n, "bob", "bob");
    regproposer("proposer1111"_n, "proposer1111"_n, "user", "one", "img_url", "bio", "country", "telegram", "website", "linkedin");
    regproposal("proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
    "description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 3);
    acceptprop("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n);

    create_account_with_resources("bigvoter1111"_n, config::system_account_name, core_sym::from_string("10000.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    issue_and_transfer( "bigvoter1111", core_sym::from_string("100000000.0000"),  config::system_account_name );
    BOOST_REQUIRE_EQUAL( success(), stake( "bigvoter1111", core_sym::from_string("50000000.0000"), core_sym::from_string("50000000.0000") ) );

    produce_blocks(100);

    create_account_with_resources("prodvoter111"_n, config::system_account_name, core_sym::from_string("10000.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    issue_and_transfer( "prodvoter111", core_sym::from_string("100000000.0000"),  config::system_account_name );
    BOOST_REQUIRE_EQUAL( success(), stake( "prodvoter111", core_sym::from_string("50000000.0000"), core_sym::from_string("50000000.0000") ) );

    produce_blocks(100);

    // Make a producer account to create an appropriate producer_vote_weight
    create_account_with_resources("prod11111111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    // prod11111111 registers to be a producer
    BOOST_REQUIRE_EQUAL( success(), regproducer( "prod11111111"_n, 1) );

    produce_blocks(100);

    // prodvoter111 votes for prod11111111
    BOOST_REQUIRE_EQUAL( success(), vote( "prodvoter111"_n, { "prod11111111"_n } ) );

    produce_blocks(100);

    // bigvoter1111 votes for prod11111111
    BOOST_REQUIRE_EQUAL( success(), vote( "bigvoter1111"_n, { "prod11111111"_n } ) );

    produce_blocks(100);

    // Unstake and stake repeatedly
    BOOST_REQUIRE_EQUAL( success(), unstake( "bigvoter1111", "bigvoter1111", core_sym::from_string("50000000.0000"), core_sym::from_string("50000000.0000") ) );

    produce_block( fc::days(4) );

    BOOST_REQUIRE_EQUAL( success(), stake( "bigvoter1111", core_sym::from_string("15000000.0000"), core_sym::from_string("15000000.0000") ) );

    produce_blocks(100);

    BOOST_REQUIRE_EQUAL( success(), unstake( "bigvoter1111", "bigvoter1111", core_sym::from_string("15000000.0000"), core_sym::from_string("15000000.0000") ) );

    produce_block( fc::days(4) );

    BOOST_REQUIRE_EQUAL( success(), stake( "bigvoter1111", core_sym::from_string("15000000.0000"), core_sym::from_string("15000000.0000") ) );

    produce_blocks(100);

    BOOST_REQUIRE_EQUAL( success(), unstake( "bigvoter1111", "bigvoter1111", core_sym::from_string("15000000.0000"), core_sym::from_string("15000000.0000") ) );

    produce_block( fc::days(4) );

    BOOST_REQUIRE_EQUAL( success(), stake( "bigvoter1111", core_sym::from_string("15000000.0000"), core_sym::from_string("15000000.0000") ) );

    produce_blocks(100);

    BOOST_REQUIRE_EQUAL( success(), unstake( "bigvoter1111", "bigvoter1111", core_sym::from_string("15000000.0000"), core_sym::from_string("15000000.0000") ) );

    produce_block( fc::days(4) );

    BOOST_REQUIRE_EQUAL( success(), stake( "bigvoter1111", core_sym::from_string("34000000.0000"), core_sym::from_string("34000000.0000") ) );

    produce_blocks(100);

    BOOST_REQUIRE_EQUAL(success(), voteproposal("bigvoter1111"_n, "bigvoter1111"_n, {"proposer1111"_n}));

    produce_blocks(100);

    BOOST_REQUIRE_EQUAL(success(),
            approve("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n));

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(proposal_cleanvotes, eosio_wps_tester) try {

    create_account_with_resources("committee111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("reviewer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10000.0000"));
    create_account_with_resources("proposer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    cross_15_percent_threshold();

    setwpsenv(config::system_account_name, 35, 30, 500, 6);
    regcommittee(config::system_account_name, "committee111"_n, "categoryX", true);
    regreviewer("committee111"_n, "committee111"_n, "reviewer1111"_n, "bob", "bob");
    regproposer("proposer1111"_n, "proposer1111"_n, "user", "one", "img_url", "bio", "country", "telegram", "website", "linkedin");
    regproposal("proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
    "description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 3);
    acceptprop("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n);


    create_account_with_resources("voter1111111"_n, config::system_account_name, core_sym::from_string("10000.0000"), false, core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("voter2222222"_n, config::system_account_name, core_sym::from_string("10000.0000"), false, core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
    create_account_with_resources("voter3333333"_n, config::system_account_name, core_sym::from_string("10000.0000"), false, core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    issue_and_transfer( "voter1111111", core_sym::from_string("100000000.0000"),  config::system_account_name );
    BOOST_REQUIRE_EQUAL( success(), stake( "voter1111111", core_sym::from_string("50000000.0000"), core_sym::from_string("50000000.0000") ) );

    issue_and_transfer( "voter2222222", core_sym::from_string("100000000.0000"),  config::system_account_name );
    BOOST_REQUIRE_EQUAL( success(), stake( "voter2222222", core_sym::from_string("50000000.0000"), core_sym::from_string("50000000.0000") ) );

    issue_and_transfer( "voter3333333", core_sym::from_string("100000000.0000"),  config::system_account_name );
    BOOST_REQUIRE_EQUAL( success(), stake( "voter3333333", core_sym::from_string("50000000.0000"), core_sym::from_string("50000000.0000") ) );

    // Make a producer account to create an appropriate producer_vote_weight
    create_account_with_resources("prod11111111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
    core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

    // prod11111111 registers to be a producer
    BOOST_REQUIRE_EQUAL( success(), regproducer( "prod11111111"_n, 1) );

    BOOST_REQUIRE_EQUAL(success(), voteproposal("voter1111111"_n, "voter1111111"_n, {"proposer1111"_n}));
    BOOST_REQUIRE_EQUAL(success(), voteproposal("voter2222222"_n, "voter2222222"_n, {"proposer1111"_n}));
    BOOST_REQUIRE_EQUAL(success(), voteproposal("voter3333333"_n, "voter3333333"_n, {"proposer1111"_n}));


    auto voter1111111 = get_wpsvoter("voter1111111"_n);
    auto voter2222222 = get_wpsvoter("voter2222222"_n);
    auto voter3333333 = get_wpsvoter("voter3333333"_n);

    BOOST_REQUIRE_EQUAL(voter1111111["proposals"].size(), 1);
    BOOST_REQUIRE_EQUAL(voter2222222["proposals"].size(), 1);
    BOOST_REQUIRE_EQUAL(voter3333333["proposals"].size(), 0);

    produce_blocks(1);

    // WBP-2014: votes can only be cleaned once the proposal no longer takes them.
    BOOST_REQUIRE_EQUAL(success(), rejectprop("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n, "reason"));
    BOOST_REQUIRE_EQUAL(error("missing authority of reviewer1111"), cleanvotes("proposer1111"_n, "reviewer1111"_n, "proposer1111"_n, 0, 1));
    BOOST_REQUIRE_EQUAL(success(), cleanvotes("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n, 0, 1));

    voter1111111 = get_wpsvoter("voter1111111"_n);
    voter2222222 = get_wpsvoter("voter2222222"_n);
    voter3333333 = get_wpsvoter("voter3333333"_n);

    BOOST_REQUIRE_EQUAL(voter1111111["proposals"].size(), 0);
    BOOST_REQUIRE_EQUAL(voter2222222["proposals"].size(), 1);
    BOOST_REQUIRE_EQUAL(voter3333333["proposals"].size(), 0);

    produce_blocks(1);

    BOOST_REQUIRE_EQUAL(success(), cleanvotes("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n, 1, 2));

    voter1111111 = get_wpsvoter("voter1111111"_n);
    voter2222222 = get_wpsvoter("voter2222222"_n);
    voter3333333 = get_wpsvoter("voter3333333"_n);

    BOOST_REQUIRE_EQUAL(voter1111111["proposals"].size(), 0);
    BOOST_REQUIRE_EQUAL(voter2222222["proposals"].size(), 0);
    BOOST_REQUIRE_EQUAL(voter3333333["proposals"].size(), 0);

    produce_blocks(1);

    BOOST_REQUIRE_EQUAL(success(), cleanvotes("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n, 0, 2));

    voter1111111 = get_wpsvoter("voter1111111"_n);
    voter2222222 = get_wpsvoter("voter2222222"_n);
    voter3333333 = get_wpsvoter("voter3333333"_n);

    BOOST_REQUIRE_EQUAL(voter1111111["proposals"].size(), 0);
    BOOST_REQUIRE_EQUAL(voter2222222["proposals"].size(), 0);
    BOOST_REQUIRE_EQUAL(voter3333333["proposals"].size(), 0);

    produce_blocks(1);

    BOOST_REQUIRE_EQUAL(success(), cleanvotes("reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n, 2, 3));

    voter1111111 = get_wpsvoter("voter1111111"_n);
    voter2222222 = get_wpsvoter("voter2222222"_n);
    voter3333333 = get_wpsvoter("voter3333333"_n);

    BOOST_REQUIRE_EQUAL(voter1111111["proposals"].size(), 0);
    BOOST_REQUIRE_EQUAL(voter2222222["proposals"].size(), 0);
    BOOST_REQUIRE_EQUAL(voter3333333["proposals"].size(), 0);
} FC_LOG_AND_RETHROW()

// WBP-2001: rmvreject, rmvcompleted and setwpsstate had no test reference in the public
// suite. rmvreject and setwpsstate are exercised on guard and effect here; rmvcompleted's
// guard here and its effect at the end of proposal_vote_claim, the one test that reaches
// COMPLETED.
BOOST_FIXTURE_TEST_CASE( wps_terminal_row_removal_and_state, eosio_wps_tester ) try {
   const name eosio = config::system_account_name;
   for( const auto& a : { "committee111"_n, "reviewer1111"_n, "reviewer2222"_n, "proposer1111"_n } )
      create_account_with_resources( a, eosio, core_sym::from_string("100.0000"), false,
                                     core_sym::from_string("10.0000"), core_sym::from_string("10.0000") );
   BOOST_REQUIRE_EQUAL( success(), setwpsenv( eosio, 35, 30, 365, 3 ) );
   regcommittee( eosio, "committee111"_n, "categoryX", true );
   regreviewer( "committee111"_n, "committee111"_n, "reviewer1111"_n, "bob", "bob" );
   regproposer( "proposer1111"_n, "proposer1111"_n, "user", "one", "img_url", "bio", "country", "telegram", "website", "linkedin" );
   BOOST_REQUIRE_EQUAL( success(), regproposal( "proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
                                                 "description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 3 ) );

   // A PENDING proposal is neither rejected nor completed, so neither removal applies.
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("Proposal::status is not PROPOSAL_STATUS::REJECTED"),
                        push_action( "reviewer1111"_n, "rmvreject"_n, mvo()("reviewer", "reviewer1111")("proposer", "proposer1111") ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("Proposal::status is not PROPOSAL_STATUS::COMPLETED"),
                        push_action( "reviewer1111"_n, "rmvcompleted"_n, mvo()("reviewer", "reviewer1111")("proposer", "proposer1111") ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("Account not found in reviewers table"),
                        push_action( "reviewer2222"_n, "rmvreject"_n, mvo()("reviewer", "reviewer2222")("proposer", "proposer1111") ) );

   // Reject it, then remove the rejected row; the proposer's slot is free again.
   BOOST_REQUIRE_EQUAL( success(), rejectprop( "reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n, "reason" ) );
   BOOST_REQUIRE_EQUAL( 2, get_proposal( "proposer1111"_n )["status"].as<int>() );
   BOOST_REQUIRE_EQUAL( error("missing authority of reviewer1111"),
                        push_action( "proposer1111"_n, "rmvreject"_n, mvo()("reviewer", "reviewer1111")("proposer", "proposer1111") ) );
   BOOST_REQUIRE_EQUAL( success(), push_action( "reviewer1111"_n, "rmvreject"_n, mvo()("reviewer", "reviewer1111")("proposer", "proposer1111") ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("Proposal not found in rejected proposal table"),
                        push_action( "reviewer1111"_n, "rmvreject"_n, mvo()("reviewer", "reviewer1111")("proposer", "proposer1111") ) );
   BOOST_REQUIRE_EQUAL( success(), regproposal( "proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
                                                 "description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 3 ) );

   // setwpsstate: msig only, positive only.
   BOOST_REQUIRE_EQUAL( error("missing authority of eosio"), push_action( "proposer1111"_n, "setwpsstate"_n, mvo()("total_stake", 1.0) ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("total_stake should be more 0"), push_action( eosio, "setwpsstate"_n, mvo()("total_stake", 0.0) ) );
   BOOST_REQUIRE_EQUAL( success(), push_action( eosio, "setwpsstate"_n, mvo()("total_stake", 1000000.0) ) );
   {
      vector<char> data = get_row_by_account( eosio, eosio, "wpsstate"_n, "wpsstate"_n );
      BOOST_REQUIRE( !data.empty() );
      const auto state = abi_ser.binary_to_variant( "wps_global_state", data, abi_serializer::create_yield_function( abi_serializer_max_time ) );
      BOOST_REQUIRE_EQUAL( 1000000.0, state["total_stake"].as_double() );
   }
} FC_LOG_AND_RETHROW()

// ---------------------------------------------------------------------------
// WCAP-SYS-2026-016 (WBP-2014). cleanvotes edits a voter's list and nothing else - not the
// proposal's tally, not the voter's stored weight. On a proposal still taking votes that
// let the voter re-vote and be counted twice (and never withdraw), and any reviewer of any
// committee could run it against any proposal. It is now confined to rejected or completed
// proposals and to reviewers of the proposal's committee.
// ---------------------------------------------------------------------------
BOOST_FIXTURE_TEST_CASE( wcap_016_cleanvotes_cannot_touch_a_live_proposal, eosio_wps_tester ) try {
   const name eosio = config::system_account_name;
   for( const auto& a : { "committee111"_n, "committee222"_n, "reviewer1111"_n, "reviewer2222"_n, "proposer1111"_n } )
      create_account_with_resources( a, eosio, core_sym::from_string("100.0000"), false, core_sym::from_string("10.0000"), core_sym::from_string("10.0000") );
   cross_15_percent_threshold();
   BOOST_REQUIRE_EQUAL( success(), setwpsenv( eosio, 35, 30, 365, 3 ) );
   regcommittee( eosio, "committee111"_n, "categoryX", false );
   regcommittee( eosio, "committee222"_n, "categoryY", false );
   regreviewer( "committee111"_n, "committee111"_n, "reviewer1111"_n, "bob", "bob" );
   regreviewer( "committee222"_n, "committee222"_n, "reviewer2222"_n, "eve", "eve" );
   regproposer( "proposer1111"_n, "proposer1111"_n, "user", "one", "img_url", "bio", "country", "telegram", "website", "linkedin" );
   BOOST_REQUIRE_EQUAL( success(), regproposal( "proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
                                                 "description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 3 ) );
   BOOST_REQUIRE_EQUAL( success(), acceptprop( "reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n ) );

   // wpsstate.total_stake counts only stake still delegated, and cross_15_percent_threshold
   // unstakes in the same transaction it stakes, so a large non-voting staker keeps the
   // voter below the 35% finish line and the proposal ON_VOTE.
   for( const auto& v : { "voter1111111"_n, "whale1111111"_n } ) {
      create_account_with_resources( v, eosio, core_sym::from_string("10000.0000"), false, core_sym::from_string("10.0000"), core_sym::from_string("10.0000") );
      issue_and_transfer( v, core_sym::from_string("2000000.0000"), eosio );
   }
   BOOST_REQUIRE_EQUAL( success(), stake( "whale1111111", core_sym::from_string("500000.0000"), core_sym::from_string("500000.0000") ) );
   BOOST_REQUIRE_EQUAL( success(), stake( "voter1111111", core_sym::from_string("100000.0000"), core_sym::from_string("100000.0000") ) );
   BOOST_REQUIRE_EQUAL( success(), voteproposal( "voter1111111"_n, "voter1111111"_n, { "proposer1111"_n } ) );
   BOOST_REQUIRE_EQUAL( 3, get_proposal( "proposer1111"_n )["status"].as<int>() );   // ON_VOTE
   const double one_vote = get_proposal( "proposer1111"_n )["total_votes"].as_double();
   BOOST_REQUIRE_GT( one_vote, 0 );

   // Live proposal: refused. Before the fix this succeeded, and the re-vote below doubled the tally.
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("votes cannot be cleaned while the proposal is taking votes"),
                        cleanvotes( "reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n, 0, 1 ) );
   BOOST_REQUIRE_EQUAL( success(), voteproposal( "voter1111111"_n, "voter1111111"_n, { "proposer1111"_n } ) );
   BOOST_REQUIRE_EQUAL( one_vote, get_proposal( "proposer1111"_n )["total_votes"].as_double() );
   // And the voter can still withdraw.
   BOOST_REQUIRE_EQUAL( success(), voteproposal( "voter1111111"_n, "voter1111111"_n, {} ) );
   BOOST_REQUIRE_EQUAL( 0, get_proposal( "proposer1111"_n )["total_votes"].as_double() );
   BOOST_REQUIRE_EQUAL( success(), voteproposal( "voter1111111"_n, "voter1111111"_n, { "proposer1111"_n } ) );

   // Rejected: cleanable, but only by a reviewer of the proposal's committee.
   BOOST_REQUIRE_EQUAL( success(), rejectprop( "reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n, "reason" ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("Reviewer is not part of this proposal's responsible committee"),
                        cleanvotes( "reviewer2222"_n, "reviewer2222"_n, "proposer1111"_n, 0, 1 ) );
   BOOST_REQUIRE_EQUAL( success(), cleanvotes( "reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n, 0, 1 ) );
   BOOST_REQUIRE_EQUAL( 0u, get_wpsvoter( "voter1111111"_n )["proposals"].size() );

   // Removed: the row is gone, nothing is left to protect, any reviewer may tidy the lists -
   // and a range past the end of the voter table cleans what exists instead of aborting.
   BOOST_REQUIRE_EQUAL( success(), push_action( "reviewer1111"_n, "rmvreject"_n, mvo()("reviewer", "reviewer1111")("proposer", "proposer1111") ) );
   BOOST_REQUIRE_EQUAL( success(), voteproposal( "whale1111111"_n, "whale1111111"_n, {} ) );   // a second voter row, list empty
   BOOST_REQUIRE_EQUAL( success(), cleanvotes( "reviewer2222"_n, "reviewer2222"_n, "proposer1111"_n, 0, 50 ) );
} FC_LOG_AND_RETHROW()

// The same root cause in the other direction: entries a voter's list carries for a proposal
// that never tallied them (pending, finished, rejected, or a same-named successor) used to
// be subtracted on the voter's next action from whichever row carried the name by then.
BOOST_FIXTURE_TEST_CASE( wcap_016_untallied_votes_are_never_subtracted, eosio_wps_tester ) try {
   const name eosio = config::system_account_name;
   for( const auto& a : { "committee111"_n, "reviewer1111"_n, "proposer1111"_n } )
      create_account_with_resources( a, eosio, core_sym::from_string("100.0000"), false, core_sym::from_string("10.0000"), core_sym::from_string("10.0000") );
   cross_15_percent_threshold();
   BOOST_REQUIRE_EQUAL( success(), setwpsenv( eosio, 35, 30, 365, 3 ) );
   regcommittee( eosio, "committee111"_n, "categoryX", false );
   regreviewer( "committee111"_n, "committee111"_n, "reviewer1111"_n, "bob", "bob" );
   regproposer( "proposer1111"_n, "proposer1111"_n, "user", "one", "img_url", "bio", "country", "telegram", "website", "linkedin" );
   for( const auto& v : { "voter1111111"_n, "voter2222222"_n, "whale1111111"_n } ) {
      create_account_with_resources( v, eosio, core_sym::from_string("10000.0000"), false, core_sym::from_string("10.0000"), core_sym::from_string("10.0000") );
      issue_and_transfer( v, core_sym::from_string("2000000.0000"), eosio );
   }
   BOOST_REQUIRE_EQUAL( success(), stake( "whale1111111", core_sym::from_string("500000.0000"), core_sym::from_string("500000.0000") ) );
   BOOST_REQUIRE_EQUAL( success(), stake( "voter1111111", core_sym::from_string("100000.0000"), core_sym::from_string("100000.0000") ) );
   BOOST_REQUIRE_EQUAL( success(), stake( "voter2222222", core_sym::from_string("100000.0000"), core_sym::from_string("100000.0000") ) );
   auto regprop = [&]() {
      return regproposal( "proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
                          "description", "roadmap", 30, {"user"}, core_sym::from_string("9000.0000"), 3 );
   };

   // Phantom pre-vote: voter1 votes while PENDING (nothing tallied), the proposal goes live,
   // voter2's vote is the whole tally, and voter1's withdrawal must not subtract anything.
   BOOST_REQUIRE_EQUAL( success(), regprop() );
   BOOST_REQUIRE_EQUAL( success(), voteproposal( "voter1111111"_n, "voter1111111"_n, { "proposer1111"_n } ) );
   BOOST_REQUIRE_EQUAL( 0u, get_wpsvoter( "voter1111111"_n )["proposals"].size() );   // not tallied, not stored
   BOOST_REQUIRE_EQUAL( success(), acceptprop( "reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n ) );
   BOOST_REQUIRE_EQUAL( success(), voteproposal( "voter2222222"_n, "voter2222222"_n, { "proposer1111"_n } ) );
   const double tally = get_proposal( "proposer1111"_n )["total_votes"].as_double();
   BOOST_REQUIRE_GT( tally, 0 );
   BOOST_REQUIRE_EQUAL( success(), voteproposal( "voter1111111"_n, "voter1111111"_n, {} ) );
   BOOST_REQUIRE_EQUAL( tally, get_proposal( "proposer1111"_n )["total_votes"].as_double() );

   // Successor: voter2's tallied entry survives rejection and removal; the proposer registers
   // again under the same name. While the successor is PENDING the committee's reviewer can
   // clean the stale entry; once it is live, voter2 withdrawing or re-voting cannot bleed it.
   BOOST_REQUIRE_EQUAL( success(), rejectprop( "reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n, "reason" ) );
   BOOST_REQUIRE_EQUAL( success(), push_action( "reviewer1111"_n, "rmvreject"_n, mvo()("reviewer", "reviewer1111")("proposer", "proposer1111") ) );
   BOOST_REQUIRE_EQUAL( success(), regprop() );
   BOOST_REQUIRE_EQUAL( 1u, get_wpsvoter( "voter2222222"_n )["proposals"].size() );
   BOOST_REQUIRE_EQUAL( success(), cleanvotes( "reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n, 0, 50 ) );
   BOOST_REQUIRE_EQUAL( 0u, get_wpsvoter( "voter2222222"_n )["proposals"].size() );
   BOOST_REQUIRE_EQUAL( success(), acceptprop( "reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n ) );
   BOOST_REQUIRE_EQUAL( success(), voteproposal( "voter1111111"_n, "voter1111111"_n, { "proposer1111"_n } ) );
   const double successor_tally = get_proposal( "proposer1111"_n )["total_votes"].as_double();
   BOOST_REQUIRE_GT( successor_tally, 0 );
   BOOST_REQUIRE_EQUAL( success(), voteproposal( "voter2222222"_n, "voter2222222"_n, {} ) );
   BOOST_REQUIRE_EQUAL( successor_tally, get_proposal( "proposer1111"_n )["total_votes"].as_double() );
} FC_LOG_AND_RETHROW()

// WCAP-SYS-2026-011 (WBP-2008). regproposal and editproposal validated funding_goal only
// as is_valid() and amount > 0, which says nothing about which symbol it is in. A proposal
// denominated in a foreign token, or in the core token at the wrong precision, was stored,
// reviewed, voted on and approvable, and failed only at claimfunds - the eosio.token
// transfer from eosio.saving throws for a symbol it holds no balance of - leaving the
// proposal APPROVED and unclaimable, removable only by the committee (rejectfund, then
// rmvreject), never by the proposer. Both actions now refuse a funding_goal that is not
// denominated in the core symbol at the core precision.
BOOST_FIXTURE_TEST_CASE( wcap_011_funding_goal_must_be_core_denominated, eosio_wps_tester ) try {
   create_account_with_resources("committee111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
                                 core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
   create_account_with_resources("proposer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
                                 core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

   setwpsenv(config::system_account_name, 5, 30, 500, 6);
   regcommittee(config::system_account_name, "committee111"_n, "categoryX", true);
   regproposer("proposer1111"_n, "proposer1111"_n, "user", "one", "img_url", "bio", "country", "telegram", "website", "linkedin");

   const name proposer = "proposer1111"_n;
   const char* refused = "funding goal must be denominated in the core symbol at its precision";
   auto reg = [&]( const asset& goal ) {
      return regproposal( proposer, proposer, "committee111"_n, 1, "title", "summary", "project_img_url",
                          "description", "roadmap", 30, {"user"}, goal, 3 );
   };
   auto edit = [&]( const asset& goal ) {
      return editproposal( proposer, proposer, "committee111"_n, 1, "edited title", "summary", "project_img_url",
                           "description", "roadmap", 30, {"user"}, goal, 3 );
   };

   // All well-formed assets (is_valid() holds): a foreign symbol, and the core symbol code at
   // a precision one above and one below the core's - each a different symbol to eosio.token.
   const asset foreign_symbol   = asset::from_string("9000.0000 XYZ");
   const asset precision_above  = asset( 9000 * 10 * 10000, symbol( CORE_SYM_PRECISION + 1, CORE_SYM_NAME ) );
   const asset precision_below  = asset( 9000 * 10000 / 10, symbol( CORE_SYM_PRECISION - 1, CORE_SYM_NAME ) );
   BOOST_REQUIRE_EQUAL( "9000.00000 " CORE_SYM_NAME, precision_above.to_string() );
   BOOST_REQUIRE_EQUAL( "9000.000 "   CORE_SYM_NAME, precision_below.to_string() );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg(refused), reg( foreign_symbol ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg(refused), reg( precision_above ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg(refused), reg( precision_below ) );
   BOOST_REQUIRE( get_proposal( proposer ).is_null() );

   BOOST_REQUIRE_EQUAL( success(), reg( core_sym::from_string("9000.0000") ) );
   produce_blocks(1);

   BOOST_REQUIRE_EQUAL( wasm_assert_msg(refused), edit( foreign_symbol ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg(refused), edit( precision_above ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg(refused), edit( precision_below ) );
   auto proposal = get_proposal( proposer );
   BOOST_REQUIRE_EQUAL( core_sym::from_string("9000.0000"), proposal["funding_goal"].as<asset>() );
   BOOST_REQUIRE_EQUAL( proposal["title"], "title" );

   // A core-denominated goal still edits normally.
   BOOST_REQUIRE_EQUAL( success(), edit( core_sym::from_string("8000.0000") ) );
   produce_blocks(1);
   proposal = get_proposal( proposer );
   BOOST_REQUIRE_EQUAL( core_sym::from_string("8000.0000"), proposal["funding_goal"].as<asset>() );
   BOOST_REQUIRE_EQUAL( proposal["title"], "edited title" );
} FC_LOG_AND_RETHROW()

// WCAP-SYS-2026-015, degenerate case (WBP-2008 review). claimfunds pays
// funding_goal / total_iterations per instalment with truncating division, and eosio.token
// refuses a zero transfer. A core-denominated goal smaller than total_iterations minimum
// units passed every entry check and reached the same approved-but-unclaimable state as
// WCAP-SYS-2026-011 through arithmetic instead of the symbol. Both actions now require at
// least one minimum unit per instalment. (The rounding remainder of larger goals is the
// open part of 015 and is not changed here.)
BOOST_FIXTURE_TEST_CASE( wcap_015_goal_must_cover_every_instalment, eosio_wps_tester ) try {
   create_account_with_resources("committee111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
                                 core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));
   create_account_with_resources("proposer1111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
                                 core_sym::from_string("10.0000"), core_sym::from_string("10.0000"));

   setwpsenv(config::system_account_name, 5, 30, 500, 6);
   regcommittee(config::system_account_name, "committee111"_n, "categoryX", true);
   regproposer("proposer1111"_n, "proposer1111"_n, "user", "one", "img_url", "bio", "country", "telegram", "website", "linkedin");

   const name proposer = "proposer1111"_n;
   const char* refused = "funding goal must be at least one minimum unit per iteration";
   auto reg = [&]( const asset& goal, uint32_t iterations ) {
      return regproposal( proposer, proposer, "committee111"_n, 1, "title", "summary", "project_img_url",
                          "description", "roadmap", 30, {"user"}, goal, iterations );
   };
   auto edit = [&]( const asset& goal, uint32_t iterations ) {
      return editproposal( proposer, proposer, "committee111"_n, 1, "title", "summary", "project_img_url",
                           "description", "roadmap", 30, {"user"}, goal, iterations );
   };

   // 50 minimum units over 99 instalments truncates to a zero instalment.
   BOOST_REQUIRE_EQUAL( wasm_assert_msg(refused), reg( core_sym::from_string("0.0050"), 99 ) );
   BOOST_REQUIRE( get_proposal( proposer ).is_null() );
   // Exactly one unit per instalment is the floor.
   BOOST_REQUIRE_EQUAL( success(), reg( core_sym::from_string("0.0099"), 99 ) );
   produce_blocks(1);

   BOOST_REQUIRE_EQUAL( wasm_assert_msg(refused), edit( core_sym::from_string("0.0098"), 99 ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg(refused), edit( core_sym::from_string("0.0050"), 51 ) );
   BOOST_REQUIRE_EQUAL( 99u, get_proposal( proposer )["total_iterations"].as<uint32_t>() );
   BOOST_REQUIRE_EQUAL( success(), edit( core_sym::from_string("0.0050"), 50 ) );
   produce_blocks(1);
   BOOST_REQUIRE_EQUAL( core_sym::from_string("0.0050"), get_proposal( proposer )["funding_goal"].as<asset>() );
} FC_LOG_AND_RETHROW()

// ---------------------------------------------------------------------------
// WCAP-SYS-2026-010 (WBP-2007). claimfunds computed the instalment cadence in 32 bits:
// `duration * seconds_per_day` narrowed into a uint32 (wraps at 49,711 days) and the
// instalment offset was added to a 32-bit time_point_sec (wraps past 2^32), so a long but
// permitted duration made instalments claimable early - the WCAP-SYS-2026-003 exploit class
// reached through the ceiling instead of the floor.
// ---------------------------------------------------------------------------
namespace {
   // Committee, reviewer, proposer, one proposal with the given duration and instalment
   // count, voted through and approved. Mirrors proposal_vote_claim's setup.
   void approve_proposal_with( eosio_wps_tester& t, uint64_t duration_days, uint32_t iterations, uint32_t max_duration_days ) {
      for( const auto& a : { "committee111"_n, "reviewer1111"_n, "proposer1111"_n } )
         t.create_account_with_resources( a, config::system_account_name, core_sym::from_string("100.0000"), false,
                                          core_sym::from_string("10.0000"), core_sym::from_string("10.0000") );
      t.cross_15_percent_threshold();
      BOOST_REQUIRE_EQUAL( t.success(), t.setwpsenv( config::system_account_name, 35, 30, max_duration_days, 6 ) );
      t.regcommittee( config::system_account_name, "committee111"_n, "categoryX", true );
      t.regreviewer( "committee111"_n, "committee111"_n, "reviewer1111"_n, "bob", "bob" );
      t.regproposer( "proposer1111"_n, "proposer1111"_n, "user", "one", "img_url", "bio", "country", "telegram", "website", "linkedin" );
      BOOST_REQUIRE_EQUAL( t.success(), t.regproposal( "proposer1111"_n, "proposer1111"_n, "committee111"_n, 1, "title", "summary", "project_img_url",
                                                        "description", "roadmap", duration_days, {"user"}, core_sym::from_string("9000.0000"), iterations ) );
      BOOST_REQUIRE_EQUAL( t.success(), t.acceptprop( "reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n ) );

      t.create_account_with_resources( "bigvoter1111"_n, config::system_account_name, core_sym::from_string("10000.0000"), false,
                                       core_sym::from_string("10.0000"), core_sym::from_string("10.0000") );
      t.issue_and_transfer( "bigvoter1111", core_sym::from_string("100000000.0000"), config::system_account_name );
      BOOST_REQUIRE_EQUAL( t.success(), t.stake( "bigvoter1111", core_sym::from_string("50000000.0000"), core_sym::from_string("50000000.0000") ) );
      t.create_account_with_resources( "prod11111111"_n, config::system_account_name, core_sym::from_string("100.0000"), false,
                                       core_sym::from_string("10.0000"), core_sym::from_string("10.0000") );
      BOOST_REQUIRE_EQUAL( t.success(), t.regproducer( "prod11111111"_n, 1 ) );
      t.produce_blocks(1);
      BOOST_REQUIRE_EQUAL( t.success(), t.vote( "bigvoter1111"_n, { "prod11111111"_n } ) );
      t.produce_block( fc::days(10) );
      BOOST_REQUIRE_EQUAL( t.success(), t.push_action( "prod11111111"_n, "claimrewards"_n, mvo()("owner", "prod11111111") ) );
      BOOST_REQUIRE_EQUAL( t.success(), t.voteproposal( "bigvoter1111"_n, "bigvoter1111"_n, { "proposer1111"_n } ) );
      t.produce_blocks(1);
      BOOST_REQUIRE_EQUAL( 4, t.get_proposal( "proposer1111"_n )["status"].as<int>() );   // FINISHED_VOTING
      BOOST_REQUIRE_EQUAL( t.success(), t.approve( "reviewer1111"_n, "reviewer1111"_n, "proposer1111"_n ) );
      t.produce_blocks(1);
      BOOST_REQUIRE_EQUAL( 5, t.get_proposal( "proposer1111"_n )["status"].as<int>() );   // APPROVED
   }
}

BOOST_FIXTURE_TEST_CASE( wcap_010_setwpsenv_caps_funding_duration, eosio_wps_tester ) try {
   // The ceiling is a century; 49,711 days - where the old 32-bit product wrapped - is refused.
   BOOST_REQUIRE_EQUAL( success(), setwpsenv( config::system_account_name, 35, 30, 36500, 6 ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("max_duration_of_funding cannot exceed 36500 days"),
                        setwpsenv( config::system_account_name, 35, 30, 36501, 6 ) );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("max_duration_of_funding cannot exceed 36500 days"),
                        setwpsenv( config::system_account_name, 35, 30, 49711, 6 ) );
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( wcap_010_claim_cadence_does_not_wrap, eosio_wps_tester ) try {
   // A single-instalment proposal at the maximum permitted duration: the one instalment is
   // due after a century. In 32-bit arithmetic fund_start_time + 36,500 days wrapped past
   // 2^32 to a date in 1983, so the whole goal was claimable at approval. (The due date
   // itself lies beyond the 32-bit block-timestamp range, so this case can only assert the
   // refusal; the control below claims a representable instalment.)
   approve_proposal_with( *this, 36500, 1, 36500 );
   produce_blocks(2);
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("Please wait until the end of this interval to claim funding"),
                        claimfunds( "proposer1111"_n, "proposer1111"_n ) );
   BOOST_REQUIRE_EQUAL( 5, get_proposal( "proposer1111"_n )["status"].as<int>() );   // still APPROVED, nothing paid
} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( wcap_010_claim_cadence_control_long_duration, eosio_wps_tester ) try {
   // Control on the 64-bit path: the same century-long proposal in four instalments has its
   // first one due after 25 years - representable, and paid exactly then.
   approve_proposal_with( *this, 36500, 4, 36500 );
   produce_blocks(2);
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("Please wait until the end of this interval to claim funding"),
                        claimfunds( "proposer1111"_n, "proposer1111"_n ) );
   produce_block( fc::days(36500 / 4 - 1) );
   produce_blocks(1);
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("Please wait until the end of this interval to claim funding"),
                        claimfunds( "proposer1111"_n, "proposer1111"_n ) );
   produce_block( fc::days(2) );
   produce_blocks(1);
   BOOST_REQUIRE_EQUAL( success(), claimfunds( "proposer1111"_n, "proposer1111"_n ) );
   produce_blocks(1);
   const auto proposal = get_proposal( "proposer1111"_n );
   BOOST_REQUIRE_EQUAL( 5, proposal["status"].as<int>() );                       // APPROVED, three instalments left
   BOOST_REQUIRE_EQUAL( 2, proposal["iteration_of_funding"].as<int>() );
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
