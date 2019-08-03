#include <eosio/crypto.hpp>
#include <eosio/datastream.hpp>
#include <eosio/eosio.hpp>
#include <eosio/multi_index.hpp>
#include <eosio/privileged.hpp>
#include <eosio/serialize.hpp>
#include <eosio/singleton.hpp>

#include <eosio.system/eosio.system.hpp>
#include <eosio.token/eosio.token.hpp>

namespace eosiosystem {

    using eosio::const_mem_fun;
    using eosio::current_time_point;
    using eosio::indexed_by;
    using eosio::singleton;
    using std::string;
    using std::vector;
    using std::set;

    void system_contract::regproposer(
            name account,
            const string& first_name,
            const string& last_name,
            const string& img_url,
            const string& bio,
            const string& country,
            const string& telegram,
            const string& website,
            const string& linkedin) {

        // authority of the user's account is required
        require_auth(account);

        //verify that the inputs are not too short
        check(first_name.size() > 0, "first name should be more than 0 characters long");
        check(last_name.size() > 0, "last name should be more than 0 characters long");
        check(img_url.size() > 0, "not a valid image URL");
        check(bio.size() > 0, "bio should be more than 0 characters long");
        check(country.size() > 0, "country name should be more than 0 characters long");
        check(telegram.size() > 4, "not a valid Telegram username");
        check(website.size() > 0, "not a valid website URL");
        check(linkedin.size() > 0, "not a valid linkedin URL");

        //verify that the inputs aren't too long
        check(first_name.size() < 128, "first name should be shorter than 128 characters.");
        check(last_name.size() < 128, "last name should be shorter than 128 characters.");
        check(img_url.size() < 128, "image URL should be shorter than 128 characters.");
        check(bio.size() < 256, "description should be shorter than 256 characters.");
        check(country.size() < 64, "country name should be shorter than 64 characters.");
        check(telegram.size() < 64, "telegram username should be shorter than 64 characters.");
        check(website.size() < 128, "website URL should be shorter than 128 characters.");
        check(linkedin.size() < 128, "linked URL should be shorter than 128 characters.");

        auto itr = _proposers.find(account.value);
        // verify that the account doesn't already exist in the table
        check(itr == _proposers.end(), "This account has already been registered as a proposer");

        // add to the table
        // storage is billed to the contract account
        _proposers.emplace(account, [&](auto& proposer){
            proposer.account = account;
            proposer.first_name = first_name;
            proposer.last_name = last_name;
            proposer.img_url = img_url;
            proposer.bio = bio;
            proposer.country = country;
            proposer.telegram = telegram;
            proposer.website = website;
            proposer.linkedin = linkedin;
            proposer.last_claim_time = 0;
        });
    }

    void system_contract::editproposer(name account,
                                    const string& first_name,
                                    const string& last_name,
                                    const string& img_url,
                                    const string& bio,
                                    const string& country,
                                    const string& telegram,
                                    const string& website,
                                    const string& linkedin) {
        // authority of the user's account is required
        require_auth(account);

        //verify that the inputs are not too short
        check(first_name.size() > 0, "first name should be more than 0 characters long");
        check(last_name.size() > 0, "last name should be more than 0 characters long");
        check(img_url.size() > 0, "not a valid image URL");
        check(bio.size() > 0, "bio should be more than 0 characters long");
        check(country.size() > 0, "country name should be more than 0 characters long");
        check(telegram.size() > 4, "not a valid Telegram username");
        check(website.size() > 0, "not a valid website URL");
        check(linkedin.size() > 0, "not a valid linkedin URL");

        //verify that the inputs aren't too long
        check(first_name.size() < 128, "first name should be shorter than 128 characters.");
        check(last_name.size() < 128, "last name should be shorter than 128 characters.");
        check(img_url.size() < 128, "image URL should be shorter than 128 characters.");
        check(bio.size() < 256, "description should be shorter than 256 characters.");
        check(country.size() < 64, "country name should be shorter than 64 characters.");
        check(telegram.size() < 64, "telegram username should be shorter than 64 characters.");
        check(website.size() < 128, "website URL should be shorter than 128 characters.");
        check(linkedin.size() < 128, "linked URL should be shorter than 128 characters.");

        auto itr = _proposers.find(account.value);
        // verify that the account doesn't already exist in the table
        check(itr != _proposers.end(), "Account not found in proposer table");

        // modify value in the table
        _proposers.modify(itr, same_payer, [&](auto& proposer) {
            proposer.account = account;
            proposer.first_name = first_name;
            proposer.last_name = last_name;
            proposer.img_url = img_url;
            proposer.bio = bio;
            proposer.country = country;
            proposer.telegram = telegram;
            proposer.website = website;
            proposer.linkedin = linkedin;
        });
    }

    void system_contract::rmvproposer(name account) {
        // needs authority of the proposers's account
        require_auth(account);

        // verify that the account already exists in the proposer table
        auto itr = _proposers.find(account.value);
        check(itr != _proposers.end(), "Account not found in proposer table");

        _proposers.erase(itr);
    }

    void system_contract::claimfunds(name account, uint64_t proposal_id){

    }

    void system_contract::regproposal(
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
            const asset& funding_goal
    ){

    }

    void system_contract::editproposal(
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
            const asset& funding_goal
    ){

    }

    void system_contract::rmvproposal(name proposer){

    }

    void system_contract::regreviewer(name committee, name reviewer, const string& first_name, const string& last_name){

    }

    void system_contract::editreviewer(name committee, name reviewer, const string& first_name, const string& last_name){

    }

    void system_contract::rmvreviewer(name committee, const name reviewer){

    }

    void system_contract::acceptprop(name reviewer, uint64_t proposal_id){

    }

    void system_contract::rejectprop(name reviewer, uint64_t proposal_id, const string& reason){

    }

    void system_contract::approve(name reviewer, uint64_t proposal_id){

    }

    void system_contract::rmvreject(name reviewer, uint64_t proposal_id){

    }

    void system_contract::rmvcompleted(name reviewer, uint64_t proposal_id){

    }

    void system_contract::setwpsenv(
            uint32_t total_voting_percent, uint32_t duration_of_voting,
            uint32_t max_duration_of_funding, uint32_t total_iteration_of_funding
    ){

    }

    void system_contract::regcommittee(name committeeman, const string& category, bool is_oversight){

    }

    void system_contract::edcommittee(name committeeman, const string& category, bool is_oversight){

    }

    void system_contract::rmvcommittee(name committeeman){

    }

    void system_contract::rejectfund(name committeeman, uint64_t proposal_id, const string& reason){

    }

    void system_contract::checkexpire(name watchman, uint64_t proposal_id){
        
    }

} /// namespace eosiosystem

//
// Created by Jae Chung on 2/8/2019.
//

