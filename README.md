# wax-system-contracts [![Build Status](https://travis-ci.org/worldwide-asset-exchange/wax-system-contracts.svg?branch=develop)](https://travis-ci.org/worldwide-asset-exchange/wax-system-contracts)

## The WAX System Contracts

The Worldwide Asset eXchange™ (WAX) is a purpose-built blockchain and protocol token designed to make e-commerce transactions faster, easier, and safer for all participants. The WAX Blockchain mainnet uses Delegated Proof of Stake (DPoS) as its consensus mechanism and is fully backward compatible with EOS. The custom features and incentive mechanisms developed by WAX are designed to optimize the blockchain’s usability in e-commerce, and encourage voting on guilds and proposals.

### Some of the features of the WAX System Contracts include:
1. [Token Swap in conjunction with Genesis Block Member Rewards](https://wax.io/blog/introducing-the-genesis-block-member-program-join-and-receive-daily-token-rewards-for-3-years)
2. [Earning staking rewards](https://wax.io/blog/earn-more-wax-introducing-wax-block-rewards-staking-and-voting-guilds-and-more)
3. [Incentives and mechanics to address voter apathy](https://wax.io/blog/staking-and-voting-on-wax-a-technical-deep-dive)

The design of the EOSIO blockchain calls for a number of smart contracts that are run at a privileged permission level in order to support functions such as block producer registration and voting, token staking for CPU and network bandwidth, RAM purchasing, multi-sig, etc.  These smart contracts are referred to as the bios, system, msig, wrap (formerly known as sudo) and token contracts.

This repository contains examples of these privileged contracts that are useful when deploying, managing, and/or using an EOSIO blockchain.  They are provided for reference purposes:

   * [eosio.bios](./contracts/eosio.bios)
   * [eosio.system](./contracts/eosio.system)
   * [eosio.msig](./contracts/eosio.msig)
   * [eosio.wrap](./contracts/eosio.wrap)

The following unprivileged contract(s) are also part of the system.
   * [eosio.token](./contracts/eosio.token)

Dependencies:
* [WAX v1.8.x](https://github.com/worldwide-asset-exchange/wax-blockchain/tree/wax-1.8.1-1.0.0)
* [WAX CDT v1.6.x](https://github.com/worldwide-asset-exchange/wax-cdt/tree/wax-1.6.1-1.0.0)

### Installation Instructions
To build the contracts and the unit tests:
* First, ensure that your __eosio__ is compiled to the core symbol for the EOSIO blockchain that intend to deploy to.
* Second, make sure that you have ```sudo make install```ed __eosio__.
* Then just run the ```build.sh``` in the top directory to build all the contracts and the unit tests for these contracts.

After build:
* The unit tests executable is placed in the _build/tests_ and is named __unit_test__.
* The contracts are built into a _bin/\<contract name\>_ folder in their respective directories.
* Finally, simply use __cleos__ to _set contract_ by pointing to the previously mentioned directory.

Run interactive tests:
1. Start the interactive shell: `make dev-docker-start`
1. Run all the tests: `make test`
1. Run a single test: `make compile && ./build/tests/unit_test --log_level=all --run_test=eosio_wps_tests/committee_reg_edit_rmv`

### License
[MIT](https://github.com/worldwide-asset-exchange/wax-eos-contracts/blob/master/LICENSE)

