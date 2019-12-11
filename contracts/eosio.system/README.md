eosio.system
----------

This contract provides multiple functionalities:
- Users can stake tokens for CPU and Network bandwidth, and then vote for producers or delegate their vote to a proxy.
- Producers register in order to be voted for, and can claim per-block and per-vote rewards.
- Users can buy and sell RAM at a market-determined price.
- Users can bid on premium names.
- The WAX Worker Proposal System to vote for and fund projects beneficial to the overall ecosystem
- A resource exchange system (REX) allows token holders to lend their tokens, and users to rent CPU and Network resources in return for a market-determined fee. 

# Actions:
The naming convention is codeaccount::actionname followed by a list of paramters.

## eosio::regproducer producer producer_key url location
   - Indicates that a particular account wishes to become a producer
   - **producer** account registering to be a producer candidate
   - **producer_key** producer account public key
   - **url** producer URL
   - **location** currently unused index

## eosio::voteproducer voter proxy producers
   - **voter** the account doing the voting
   - **proxy** proxy account to whom voter delegates vote
   - **producers** list of producers voted for. A maximum of 30 producers is allowed
   - Voter can vote for a proxy __or__ a list of at most 30 producers. Storage change is billed to `voter`.

## eosio::regproxy proxy is_proxy
   - **proxy** the account registering as voter proxy (or unregistering)
   - **is_proxy** if true, proxy is registered; if false, proxy is unregistered
   - Storage change is billed to `proxy`.
   
## eosio::delegatebw from receiver stake\_net\_quantity stake\_cpu\_quantity transfer
   - **from** account holding tokens to be staked
   - **receiver** account to whose resources staked tokens are added
   - **stake\_net\_quantity** tokens staked for NET bandwidth
   - **stake\_cpu\_quantity** tokens staked for CPU bandwidth
   - **transfer** if true, ownership of staked tokens is transfered to `receiver`
   - All producers `from` account has voted for will have their votes updated immediately.

## eosio::undelegatebw from receiver unstake\_net\_quantity unstake\_cpu\_quantity
   - **from** account whose tokens will be unstaked
   - **receiver** account to whose benefit tokens have been staked
   - **unstake\_net\_quantity** tokens to be unstaked from NET bandwidth
   - **unstake\_cpu\_quantity** tokens to be unstaked from CPU bandwidth
   - Unstaked tokens are transferred to `from` liquid balance via a deferred transaction with a delay of 3 days.
   - If called during the delay period of a previous `undelegatebw` action, pending action is canceled and timer is reset.
   - All producers `from` account has voted for will have their votes updated immediately.
   - Bandwidth and storage for the deferred transaction are billed to `from`.

## eosio::onblock header
   - This special action is triggered when a block is applied by a given producer, and cannot be generated from
     any other source. It is used increment the number of unpaid blocks by a producer and update producer schedule.

## eosio::claimrewards producer
   - **producer** producer account claiming per-block and per-vote rewards

## eosio::setwpsenv

Required authority: `_self`

Description: Sets up the global WPS parameters, which includes vote participation required (in percentage of `total_activated_stake`), expiry time for proposals on vote, and maximum duration of a project. The default values proposed when the WPS is ratified will be 5, 30, and 180, respectively.

## eosio::regproposer

Required authority: Account owner

Description: Register an account as a proposer. All fields required. RAM is billed to the registrant's account. Account is added to the proposers table.

## eosio::editproposer

Required authority: Account owner

Description: Edit proposer info. All fields required.

## eosio::rmvproposer

Required authority: Account owner

Description: Remove account from the proposers table.

## eosio::regproposal

Required authority: Proposer


Description: Register a proposal. Account must be on the proposers table. All fields required. RAM is billed to the proposer's account. Proposal is added to the proposals table. One proposer can register only one proposal at a time.

## eosio::editproposal

Required authority: Proposer


Description: Edit proposal info. All fields required.

## eosio::rmvproposal

Required authority: Proposer

Description: Delete proposal from the proposals table.

## eosio::regcommittee

Required authority: `_self`

Description: Register a committee responsible for a certain category. The account is added to the committees table. RAM is billed to the contract's account. All fields are required. Oversight power is given to the oversight committee. Committees can only be registered using `eosio` permissions.

## eosio::edcommittee

Required authority: `_self`

Description: Edit committee information. All fields required.

## eosio::rmvcommittee

Required authority: `_self`

Description: Remove committee from the committees table.

## eosio::regreviewer

Required authority: Committee

Description: Register account as a reviewer. All fields required. RAM billed to committee account. Reviewer is added to reviewers table, with the committee that the account is associated with.

## eosio::editreviewer

Required authority: Committee

Description: Edit reviewer information. All fields required.

## eosio::rmvreviewer

Required authority: Committee

Description: Remove reviewer from the reviewers table.

## eosio::acceptprop

Required authority: Reviewer

Description: Accept a proposal with PENDING status. Change its status to ON VOTE. All fields required.

## eosio::rejectprop

Required authority: Reviewer

Description: Reject a proposal with PENDING status. Change its status to REJECTED. Move proposal to the rejected proposals table. All fields required.

## eosio::approve

Required authority: Reviewer

Description: Approve funding for proposals with the CHECKED VOTES status. Proposal status changes to APPROVED. All fields required.

## eosio::claimfunds

Required authority: Proposer

Description: Claim funding for a proposal with the APPROVED status. The proposer can claim a portion of the funds for each iteration of the project's duration. When all iterations have been completed, the proposal status changes to COMPLETED. It is then transferred to the completed proposals table. All fields required.

## eosio::rmvreject

Required authority: Reviewer

Description: Clear a proposal on the rejected proposals table when it is no longer needed there.

## eosio::rmvcompleted

Required authority: Reviewer

Description: Clear a proposal on the completed proposals table when it is no longer needed there.

## eosio::voteproposal

Required authority: Account owner

Description: Vote for a proposal. Each account is limited to one vote. Vote weight is determined by the amount of WAX staked. Voting for another proposal will take away the votes of an earlier proposal. All fields required.

## eosio::rejectfund

Required authority: Committee (oversight)

Description: Reject a proposal with APPROVED status being funded. The proposal is transferred to the rejected proposals table. All fiels required.

# Tables

You can find information on the tables directly using `cleos`:
```console
cleos get table eosio eosio <table name>
```

## proposals

Description: Table of ongoing proposals. Indexed by proposer account name and proposal id.

Code: `_self`

Scope: `_self`

## proposers

Description: Table of proposers. Indexed by account name.

Code: `_self`

Scope: `_self`

## reviewers

Description: Table of reviewers. Indexed by account name.

Code: `_self`

Scope: `_self`

## committees

Description: Table of committees. Indexed by account name.

Code: `_self`

Scope: `_self`

## wpsglobal

Description: Table of WPS global environment variables.
