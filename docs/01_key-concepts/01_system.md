---
content_title: System contracts, system accounts, privileged accounts
link_text: System contracts, system accounts, privileged accounts
---

At the genesis of an EOSIO-based blockchain, there is only one account present, `eosio` account, which is the main `system account`. There are other `system account`s, created by `eosio` account, which control specific actions of the `system contract`s [mentioned in previous section](../#system-contracts-defined-in-eosio.contracts). __Note__ the terms `system contract` and `system account`. `Privileged accounts` are accounts which can execute a transaction while skipping the standard authorization check. To ensure that this is not a security hole, the permission authority over these accounts is granted to `eosio.prods` system account.

As you just learned the relation between a `system account` and a `system contract`, it is also important to remember that not all system accounts contain a system contract, but each system account has important roles in the blockchain functionality, as follows:

|Account|Privileged|Has contract|Description|
|---|---|---|---|
|eosio|Yes|It contains the `eosio.system` contract|The main system account on an EOSIO-based blockchain.|
|eosio.msig|Yes|It contains the `eosio.msig` contract|Allows the signing of a multi-sig transaction proposal for later execution if all required parties sign the proposal before the expiration time.|
|eosio.wrap|Yes|It contains the `eosio.wrap` contract.|Simplifies block producer superuser actions by making them more readable and easier to audit.|
|eosio.token|No|It contains the `eosio.token` contract.|Defines the structures and actions allowing users to create, issue, and manage tokens on EOSIO-based blockchains.|
|eosio.names|No|No|The account which is holding funds from namespace auctions.|
|eosio.bpay|No|No|The account that pays block producers and standbys for blocks produced. On WAX it receives the `producers_bps` share of post-RNG inflation (default 3/10), set by `setinflation`; see the [architecture overview](../02_wax-extensions/01_architecture-overview.md#3-producer-pay-and-the-inflation-split).|
|eosio.prods|No|No|The account representing the union of all current active block producers permissions.|
|eosio.ram|No|No|The account that keeps track of the SYS balances based on users actions of buying or selling RAM.|
|eosio.ramfee|No|No|The account that keeps track of the fees collected from users RAM trading actions: 0.5% from the value of each trade goes into this account.|
|eosio.saving|No|No|The account which holds the `savings_bps` share of post-RNG inflation (default 3/10 on WAX), set by `setinflation`.|
|eosio.stake|No|No|The account that keeps track of all SYS tokens which have been staked for NET or CPU bandwidth.|
|eosio.vpay|No|No|Upstream per-vote producer pay account. Unused on WAX: there is no per-vote producer pay; voters are paid from `eosio.voters` (the `voters_bps` share, default 4/10).|
|eosio.rex|No|No|The account that keeps track of fees and balances resulted from REX related actions execution.|
