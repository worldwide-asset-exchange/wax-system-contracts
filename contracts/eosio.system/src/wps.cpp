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
            proposer.last_claim_time = time_point_sec();
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

    void system_contract::claimfunds(name account, name proposer){
        // needs authority of the proposer account
        require_auth(account);
        check(is_account(proposer), "Proposal creator does not exist");

        // verify that the account already exists in the proposer table
        auto itr = _proposers.find(account.value);
        check(itr != _proposers.end(), "Account not found in proposer table");

        //auto idx_index = _proposals.get_index<"idx"_n>();
        auto itr_proposal = _proposals.find(proposer.value);
        check(itr_proposal != _proposals.end(), "Proposal not found in proposal table");

        time_point_sec current_time = current_time_point();
        auto wps_env = _wps_env.get();
        auto& proposal = (*itr_proposal);

        check(proposal.status == PROPOSAL_STATUS::APPROVED, "Proposal::status is not PROPOSAL_STATUS::APPROVED");
        check(proposal.iteration_of_funding < wps_env.total_iteration_of_funding, "all funds for this proposal have already been claimed");

        uint32_t funding_duration_seconds = proposal.duration * seconds_per_day;
        uint32_t seconds_per_claim_interval = funding_duration_seconds / wps_env.total_iteration_of_funding;
        time_point_sec start_funding_round = proposal.fund_start_time +
                (uint32_t) (proposal.iteration_of_funding * seconds_per_claim_interval);

        check(current_time > start_funding_round, "It has not been 30 days since last claim");

        asset transfer_amount = proposal.funding_goal / wps_env.total_iteration_of_funding;

        //inline action transfer, send funds to proposer
        eosio::action(
                eosio::permission_level{get_self() , "active"_n },
                "eosio.token"_n, "transfer"_n,
                std::make_tuple( get_self(), account, transfer_amount, std::string("Your worker proposal has been approved."))
        ).send();

        _proposers.modify(itr, same_payer, [&](auto& _proposer){
            _proposer.last_claim_time = current_time;
        });

        _proposals.modify(itr_proposal, same_payer, [&](auto& _proposal){
            _proposal.iteration_of_funding += 1;
        });

        if(proposal.iteration_of_funding >= wps_env.total_iteration_of_funding){
            _proposals.modify(itr_proposal, same_payer, [&](auto& _proposal){
                _proposal.status = PROPOSAL_STATUS::COMPLETED;
            });
        }
        //change state based on count
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
        // authority of the user's account is required
        require_auth(proposer);

        // verify that the committee account exists
        check(is_account(committee), "committee account doesn't exist");

        //verify that the inputs are not too short
        //subcategory is not required
        //eosio_assert(subcategory > 0, "subcategory should be an integer greater than 0");
        check(title.size() > 0, "title should be more than 0 characters long");
        check(summary.size() > 0, "summary should be more than 0 characters long");
        check(project_img_url.size() > 0, "URL should be more than 0 characters long");
        check(description.size() > 0, "description should be more than 0 characters long");
        check(roadmap.size() > 0, "roadmap should be more than 0 characters long");
        check(duration > 0, "duration should be longer than 0 days");
        check(members.size() > 0, "member should be more than 0");

        auto env = _wps_env.get();

        //verify that the inputs aren't too long
        check(subcategory < 10, "invalid sub-category");
        check(title.size() < 256, "title should be shorter than 256 characters.");
        check(summary.size() < 400, "subtitle should be shorter than 256 characters.");
        check(project_img_url.size() < 128, "URL should be shorter than 128 characters.");
        check(description.size() < 5000, "description should be shorter than 1024 characters.");
        check(roadmap.size() < 2000, "financial_roadmap should be shorter than 256 characters.");
        check(duration <= env.max_duration_of_funding, "duration can be at most 180 days");
        check(members.size() < 50, "members should be shorter than 50 characters.");
        check(funding_goal.is_valid(), "invalid quantity" );
        check(funding_goal.amount > 0, "must request positive amount" );

        check(funding_goal.symbol == eosio::symbol("EOS", 4), "symbol precision mismatch");

        auto itr = _proposers.find(proposer.value);
        // verify that the account is a registered proposer
        check(itr != _proposers.end(), "This account is not a registered proposer");

        auto proposal_itr = _proposals.find(proposer.value);
        // verify that the account doesn't already exist in the table
        check(proposal_itr == _proposals.end(), "This account has already registered a proposal");

        auto committee_itr = _committees.find(committee.value);
        // verify that the committee is on committee table
        check(committee_itr != _committees.end(), "Account not found in committee table");

        // add to the table
        // storage is billed to the contract account
        _proposals.emplace(proposer, [&](auto& proposal) {
            proposal.proposer = proposer;
            proposal.committee = committee;
            proposal.category = (*committee_itr).category;
            proposal.subcategory = subcategory;
            proposal.title = title;
            proposal.summary = summary;
            proposal.project_img_url = project_img_url;
            proposal.description = description;
            proposal.roadmap = roadmap;
            proposal.duration = duration;
            proposal.members = members;
            proposal.funding_goal = funding_goal;
            proposal.id = _proposers.available_primary_key();
            proposal.status = PROPOSAL_STATUS::PENDING; 		//initialize status to pending
            proposal.total_votes = 0;
            proposal.vote_start_time = time_point_sec();
            proposal.fund_start_time = time_point_sec();
            proposal.iteration_of_funding = 0;
        });
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
        // authority of the user's account is required
        require_auth(proposer);

        // verify that the committee account exists
        check(is_account(committee), "committee account doesn't exist");

        //verify that the inputs are not too short
        check(title.size() > 0, "title should be more than 0 characters long");
        check(summary.size() > 0, "summary should be more than 0 characters long");
        check(project_img_url.size() > 0, "URL should be more than 0 characters long");
        check(description.size() > 0, "description should be more than 0 characters long");
        check(roadmap.size() > 0, "roadmap should be more than 0 characters long");
        check(duration > 0, "duration should be longer than 0 days");
        check(members.size() > 0, "member should be more than 0");

        auto env = _wps_env.get();

        //verify that the inputs aren't too long
        check(subcategory < 10, "invalid sub-category");
        check(title.size() < 256, "title should be shorter than 256 characters.");
        check(summary.size() < 400, "subtitle should be shorter than 256 characters.");
        check(project_img_url.size() < 128, "URL should be shorter than 128 characters.");
        check(description.size() < 5000, "description should be shorter than 1024 characters.");
        check(roadmap.size() < 2000, "financial_roadmap should be shorter than 256 characters.");
        check(duration <= env.max_duration_of_funding, "duration can be at most 180 days");
        check(members.size() < 50, "members should be shorter than 50 characters.");
        check(funding_goal.is_valid(), "invalid quantity" );
        check(funding_goal.amount > 0, "must request positive amount" );

        check(funding_goal.symbol == asset().symbol, "symbol precision mismatch" );

        auto itr = _proposers.find(proposer.value);
        // verify that the account is a registered proposer
        check(itr != _proposers.end(), "This account is not a registered proposer");

        auto proposal_itr = _proposals.find(proposer.value);
        // verify that the account already exists in the proposals table
        check(proposal_itr != _proposals.end(), "Account not found in proposal table");

        check((*proposal_itr).status == PROPOSAL_STATUS::PENDING, "Proposal::status is not PROPOSAL_STATUS::PENDING");

        auto committee_itr = _committees.find(committee.value);
        // verify that the committee is on committee table
        check(committee_itr != _committees.end(), "Account not found in committee table");

        // modify value in the table
        _proposals.modify(proposal_itr, same_payer, [&](auto& proposal){
            proposal.proposer = proposer;
            proposal.committee = committee;
            proposal.category = (*committee_itr).category;
            proposal.subcategory = subcategory;
            proposal.title = title;
            proposal.summary = summary;
            proposal.project_img_url = project_img_url;
            proposal.description = description;
            proposal.roadmap = roadmap;
            proposal.duration = duration;
            proposal.members = members;
            proposal.funding_goal = funding_goal;
        });
    }

    void system_contract::rmvproposal(name proposer){
        // needs authority of the proposers's account
        require_auth(proposer);

        // verify that the account already exists in the proposer table
        auto itr = _proposals.find(proposer.value);
        check(itr != _proposals.end(), "Account not found in proposal table");

        _proposals.erase(itr);
    }

    void system_contract::regreviewer(name committee, name reviewer, const string& first_name, const string& last_name){
        //Require permission of committee account
        require_auth(committee);

        //verify that the account exists
        check(is_account(reviewer), "The reviewer account does not exist");

        //verify that the inputs are not too short
        check(first_name.size() > 0, "first name should be more than 0 characters long");
        check(last_name.size() > 0, "last name should be more than 0 characters long");

        //verify that the inputs are not too long
        check(first_name.size() < 128, "first name should be shorter than 128 characters.");
        check(last_name.size() < 128, "last name should be shorter than 128 characters.");

        auto itr = _committees.find(committee.value);
        // verify that the committee is on committee table
        check(itr != _committees.end(), "Account not found in committee table");

        auto reviewer_itr = _reviewers.find(reviewer.value);
        // verify that the account doesn't already exist in the table
        check(reviewer_itr == _reviewers.end(), "This account has already been registered as a reviewer");

        //add to the table
        _reviewers.emplace(committee, [&](auto& _reviewer){
            _reviewer.account = reviewer;
            _reviewer.first_name = first_name;
            _reviewer.last_name = last_name;
            _reviewer.committee = committee;
        });
    }

    void system_contract::editreviewer(name committee, name reviewer, const string& first_name, const string& last_name){
        //Require permission of committee account
        require_auth(committee);

        //verify that the account exists
        check(is_account(reviewer), "The reviewer account does not exist");

        //verify that the inputs are not too short
        check(first_name.size() > 0, "first name should be more than 0 characters long");
        check(last_name.size() > 0, "last name should be more than 0 characters long");

        //verify that the inputs are not too long
        check(first_name.size() < 128, "first name should be shorter than 128 characters.");
        check(last_name.size() < 128, "last name should be shorter than 128 characters.");

        auto committee_itr = _committees.find(committee.value);
        // verify that the committee is on committee table
        check(committee_itr != _committees.end(), "Account not found in committee table");

        auto itr = _reviewers.find(reviewer.value);
        // verify that the account already exists in the table
        check(itr != _reviewers.end(), "Account not found in reviewers table");
        // verify that the reviewer is part of the committee
        check((*itr).committee == committee, "The given reviewer is not part of this committee");

        //add to the table
        _reviewers.modify(itr, same_payer, [&](auto& _reviewer){
            _reviewer.account = reviewer;
            _reviewer.first_name = first_name;
            _reviewer.last_name = last_name;
            _reviewer.committee = committee;
        });
    }

    void system_contract::rmvreviewer(name committee, name reviewer){
        // needs authority of the committee account
        require_auth(committee);

        //verify that the account exists
        check(is_account(reviewer), "The account does not exist");

        auto committee_itr = _committees.find(committee.value);
        // verify that the committee is on committee table
        check(committee_itr != _committees.end(), "Account not found in committee table");

        auto itr = _reviewers.find(reviewer.value);
        // verify that the account already exists in the reviewers table
        check(itr != _reviewers.end(), "Account not found in reviewers table");
        // verify that the reviewer is part of the committee
        check((*itr).committee == committee, "The given reviewer is not part of this committee");

        _reviewers.erase(itr);
    }

    void system_contract::acceptprop(name reviewer, name proposer){
        require_auth(reviewer);

        check(is_account(proposer), "Proposal creator does not exist");

        auto itr = _reviewers.find(reviewer.value);
        check(itr != _reviewers.end(), "Account not found in reviewers table");

        auto itr_proposal = _proposals.find(proposer.value);
        check(itr_proposal != _proposals.end(), "Proposal not found in proposal table");
        check((*itr_proposal).status == PROPOSAL_STATUS::PENDING, "Proposal::status is not proposal_status::PENDING");
        check((*itr_proposal).committee == (*itr).committee, "Reviewer is not part of this proposal's responsible committee");

        _proposals.modify(itr_proposal, same_payer, [&](auto& proposal){
            proposal.vote_start_time = current_time_point();
            proposal.status = PROPOSAL_STATUS::ON_VOTE;
        });
    }

    void system_contract::rejectprop(name reviewer, name proposer, const string& reason){
        require_auth(reviewer);

        check(is_account(proposer), "Proposal creator does not exist");

        check(reason.size() > 0, "must provide a brief reason");
        check(reason.size() < 256, "reason is too long");

        auto itr = _reviewers.find(reviewer.value);
        check(itr != _reviewers.end(), "Account not found in reviewers table");

        auto itr_proposal = _proposals.find(proposer.value);
        check(itr_proposal != _proposals.end(), "Proposal not found in proposal table");
        check((*itr_proposal).committee == (*itr).committee, "Reviewer is not part of this proposal's responsible committee");
        check(((*itr_proposal).status == PROPOSAL_STATUS::PENDING) || ((*itr_proposal).status == PROPOSAL_STATUS::ON_VOTE), "invalid proposal status");

        _proposals.modify(itr_proposal, (*itr_proposal).proposer, [&](auto& proposal){
            proposal.status = PROPOSAL_STATUS::REJECTED;
        });
    }

    void system_contract::approve(name reviewer, name proposer){
        require_auth(reviewer);

        check(is_account(proposer), "Proposal creator does not exist");

        auto itr = _reviewers.find(reviewer.value);
        check(itr != _reviewers.end(), "Account not found in reviewers table");

        auto itr_proposal = _proposals.find(proposer.value);
        check(itr_proposal != _proposals.end(), "Proposal not found in proposal table");
        check((*itr_proposal).committee==(*itr).committee, "Reviewer is not part of this proposal's responsible committee");
        check((*itr_proposal).status == PROPOSAL_STATUS::FINISHED_VOTING, "Proposal::status is not PROPOSAL_STATUS::FINISHED_VOTING");

        _proposals.modify(itr_proposal, same_payer, [&](auto& proposal){
            proposal.fund_start_time = current_time_point();
            proposal.status = PROPOSAL_STATUS::APPROVED;
        });
    }

    void system_contract::rmvreject(name reviewer, name proposer){
        require_auth(reviewer);

        check(is_account(proposer), "Proposal creator does not exist");

        auto itr = _reviewers.find(reviewer.value);
        check(itr != _reviewers.end(), "Account not found in reviewers table");

        auto itr_proposal = _proposals.find(proposer.value);
        check(itr_proposal != _proposals.end(), "Proposal not found in rejected proposal table");
        check((*itr_proposal).status == PROPOSAL_STATUS::REJECTED, "Proposal::status is not PROPOSAL_STATUS::REJECTED");
        check((*itr_proposal).committee==(*itr).committee, "Reviewer is not part of this proposal's responsible committee");

        _proposals.erase(itr_proposal);
    }

    void system_contract::rmvcompleted(name reviewer, name proposer){
        require_auth(reviewer);

        check(is_account(proposer), "Proposal creator does not exist");

        auto itr = _reviewers.find(reviewer.value);
        check(itr != _reviewers.end(), "Account not found in reviewers table");

        auto itr_proposal = _proposals.find(proposer.value);
        check(itr_proposal != _proposals.end(), "Proposal not found in completed proposals table");
        check((*itr_proposal).status == PROPOSAL_STATUS::COMPLETED, "Proposal::status is not PROPOSAL_STATUS::COMPLETED");
        check((*itr_proposal).committee==(*itr).committee, "Reviewer is not part of this proposal's responsible committee");

        _proposals.erase(itr_proposal);
    }

    void system_contract::setwpsenv(
            uint32_t total_voting_percent, uint32_t duration_of_voting,
            uint32_t max_duration_of_funding, uint32_t total_iteration_of_funding
    ){
        //registration of committee requires contract account permissions
        require_auth(get_self());

        check(total_voting_percent >= 5, "total_voting_percent should be more than equal 5 long");
        check(duration_of_voting > 0, "duration_of_voting should be more than 0");
        check(max_duration_of_funding > 0, "max_duration_of_funding should be more than 0");
        check(total_iteration_of_funding > 0, "total_iteration_of_funding should be more than 0");

        wps_env env = wps_env();

        env.total_voting_percent = total_voting_percent;
        env.duration_of_voting = duration_of_voting;
        env.max_duration_of_funding = max_duration_of_funding;
        env.total_iteration_of_funding = total_iteration_of_funding;

        _wps_env.set( env, _self );
    }

    void system_contract::regcommittee(name committeeman, const string& category, bool is_oversight){
        //registration of committee requires contract account permissions
        require_auth(get_self());

        //verify that the committee account exists
        check(is_account(committeeman), "committeeman account doesn't exist");

        //verify that the size of the category string is not too long/short
        check(category.size() > 0, "category should be more than 0 characters long");
        check(category.size() < 64, "category should be less than 64 characters long");

        auto itr = _committees.find(committeeman.value);
        // verify that the account doesn't already exist in the table
        check(itr == _committees.end(), "This account has already been registered as a committee");

        //add to the table
        _committees.emplace(_self, [&](auto& committee){
            committee.committeeman = committeeman;
            committee.category = category;
            committee.is_oversight = is_oversight;
        });
    }

    void system_contract::edcommittee(name committeeman, const string& category, bool is_oversight){
        //editing committee info requires contract account permissions
        require_auth(get_self());

        //verify that the committee account exists
        check(is_account(committeeman), "committee account doesn't exist");

        //verify that the size of the category string is not too long/short
        check(category.size() > 0, "category should be more than 0 characters long");
        check(category.size() < 64, "category should be less than 64 characters long");

        auto itr = _committees.find(committeeman.value);
        // verify that the account doesn't already exist in the table
        check(itr != _committees.end(), "Account not found in committee table");

        //add to the table
        _committees.modify(itr, same_payer, [&](auto& committee){
            committee.committeeman = committeeman;
            committee.category = category;
            committee.is_oversight = is_oversight;
        });
    }

    void system_contract::rmvcommittee(name committeeman){
        require_auth(_self);

        check(is_account(committeeman), "committeeman account doesn't exist");

        auto itr = _committees.find(committeeman.value);

        check(itr != _committees.end(), "Account not found in committee table");
        _committees.erase(itr);
    }

    void system_contract::rejectfund(name committeeman, name proposer, const string& reason){
        require_auth(committeeman);

        check(is_account(proposer), "Proposal creator does not exist");

        check(reason.size() > 0, "must provide a brief reason");
        check(reason.size() < 256, "reason is too long");

        auto itr = _committees.find(committeeman.value);
        // verify that the committee is on committee table
        check(itr != _committees.end(), "Account not found in committee table");

        auto itr_proposal = _proposals.find(proposer.value);
        check(itr_proposal != _proposals.end(), "Proposal not found in proposal table");

        check((*itr_proposal).committee == (*itr).committeeman || (*itr).is_oversight, "Committee is not associated with this proposal");
        check((*itr_proposal).status == PROPOSAL_STATUS::APPROVED, "Proposal::status is not PROPOSAL_STATUS::APPROVED");

        _proposals.modify(itr_proposal, same_payer, [&](auto& _proposal){
            _proposal.status = PROPOSAL_STATUS::REJECTED;
        });
    }

    void system_contract::checkexpire(name proposer){
        check(is_account(proposer), "Proposal creator does not exist");

        auto itr_proposal = _proposals.find(proposer.value);
        check(itr_proposal != _proposals.end(), "Proposal not found in proposal table");
        check((*itr_proposal).status == PROPOSAL_STATUS::ON_VOTE, "Proposal::status is not PROPOSAL_STATUS::ON_VOTE");

        time_point_sec current_time = current_time_point();
        auto env = _wps_env.get();
        uint32_t duration_of_voting = env.duration_of_voting * seconds_per_day;

        if(time_point_sec(time_point(current_time - (*itr_proposal).vote_start_time)) > time_point_sec(duration_of_voting)){
            _proposals.modify(itr_proposal, same_payer, [&](auto& _proposal){
                _proposal.status = PROPOSAL_STATUS::REJECTED;
            });
        }
    }

    void system_contract::voteproposal( const name& voter_name, const name& proxy, const std::vector<name>& proposals ) {
        require_auth( voter_name );
        vote_stake_updater( voter_name );
        update_wps_votes( voter_name, proxy, proposals, true );
    }

    void system_contract::update_wps_votes( const name& voter_name, const name& proxy, const std::vector<name>& proposals, bool voting){
        //validate input
        if ( proxy ) {
            check( proposals.size() == 0, "cannot vote for proposals and proxy at same time" );
            check( voter_name != proxy, "cannot proxy to self" );
        } else {
            check( proposals.size() <= 5, "attempt to vote for too many proposals" );
            for( size_t i = 1; i < proposals.size(); ++i ) {
                check( proposals[i-1] != proposals[i], "proposal votes must be unique" );
            }
        }

        auto voter = _voters.find( voter_name.value );
        check( voter != _voters.end(), "user must stake before they can vote" ); /// staking creates voter object
        check( !proxy || !voter->is_proxy, "account registered as a proxy is not allowed to use a proxy" );

        /**
         * The first time someone votes we calculate and set last_vote_weight, since they cannot unstake until
         * after total_activated_stake hits threshold, we can use last_vote_weight to determine that this is
         * their first vote and should consider their stake activated.
         */
        if( voter->last_vote_weight <= 0.0 ) {
            _gstate.total_activated_stake += voter->staked;
            if( _gstate.total_activated_stake >= min_activated_stake && _gstate.thresh_activated_stake_time == time_point() ) {
                _gstate.thresh_activated_stake_time = current_time_point();
            }
        }

        auto new_vote_weight = stake2vote( voter->staked );
        if( voter->is_proxy ) {
            new_vote_weight += voter->proxied_vote_weight;
        }

        std::map<name, std::pair<double, bool /*new*/> > proposal_deltas;
        if ( voter->last_vote_weight > 0 ) {
            if( voter->proxy ) {
                auto old_proxy = _voters.find( voter->proxy.value );
                check( old_proxy != _voters.end(), "old proxy not found" ); //data corruption
                _voters.modify( old_proxy, same_payer, [&]( auto& vp ) {
                    vp.proxied_vote_weight -= voter->last_vote_weight;
                });
                propagate_weight_change( *old_proxy );
            } else {
                for( const auto& p : voter->proposals ) {
                    auto& d = proposal_deltas[p];
                    d.first -= voter->last_vote_weight;
                    d.second = false;
                }
            }
        }

        if( proxy ) {
            auto new_proxy = _voters.find( proxy.value );
            check( new_proxy != _voters.end(), "invalid proxy specified" ); //if ( !voting ) { data corruption } else { wrong vote }
            check( !voting || new_proxy->is_proxy, "proxy not found" );
            if ( new_vote_weight >= 0 ) {
                _voters.modify( new_proxy, same_payer, [&]( auto& vp ) {
                    vp.proxied_vote_weight += new_vote_weight;
                });
                propagate_weight_change( *new_proxy );
            }
        } else {
            if( new_vote_weight >= 0 ) {
                for( const auto& p : proposals ) {
                    auto& d = proposal_deltas[p];
                    d.first += new_vote_weight;
                    d.second = true;
                }
            }
        }

        const auto ct = current_time_point();
        for( const auto& pd : proposal_deltas ) {
            auto pitr = _proposals.find( pd.first.value );
            if( pitr != _proposals.end() ) {
                /*if( voting && (*pitr).status != PROPOSAL_STATUS::ON_VOTE && pd.second.second ) {
                    check( false, ( pitr->owner.to_string() + "'s proposal is not currently on vote" ).data() );
                }*/
                check( (*pitr).status != PROPOSAL_STATUS::ON_VOTE, ( pitr->proposer.to_string() + "'s proposal is not currently on vote" ).data() );
                //double init_total_votes = pitr->total_votes;
                _proposals.modify( pitr, same_payer, [&]( auto& p ) {
                    p.total_votes += pd.second.first;
                    if ( p.total_votes < 0 ) { // floating point arithmetics can give small negative numbers
                        p.total_votes = 0;
                    }
                    //_gstate.total_producer_vote_weight += pd.second.first;
                    //check( p.total_votes >= 0, "something bad happened" );
                });

            } else {
                if( pd.second.second ) {
                    check( false, ( pd.first.to_string() + "does not have an existing proposal" ).data() );
                }
            }
        }

        _voters.modify( voter, same_payer, [&]( auto& av ) {
            av.last_vote_weight = new_vote_weight;
            av.proposals = proposals;
            av.proxy     = proxy;
        });
        //update_voter_votepay_share(voter);
    }

} /// namespace eosiosystem

//
// Created by Jae Chung on 2/8/2019.
//

