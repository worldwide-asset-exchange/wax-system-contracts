# wax-system-contracts [![Build Status](https://travis-ci.org/worldwide-asset-exchange/wax-system-contracts.svg?branch=develop)](https://travis-ci.org/worldwide-asset-exchange/wax-system-contracts)

## The WAX System Contracts

The Worldwide Asset eXchange™ (WAX) is a purpose-built blockchain and protocol token designed to make e-commerce transactions faster, easier, and safer for all participants. The WAX Blockchain mainnet uses Delegated Proof of Stake (DPoS) as its consensus mechanism and is fully backward compatible with EOS. The custom features and incentive mechanisms developed by WAX are designed to optimize the blockchain’s usability in e-commerce, and encourage voting on guilds and proposals.

### Some of the features of the WAX System Contracts include:
1. [Token Swap in conjunction with Genesis Block Member Rewards](https://wax.io/blog/introducing-the-genesis-block-member-program-join-and-receive-daily-token-rewards-for-3-years)
2. [Earning staking rewards](https://wax.io/blog/earn-more-wax-introducing-wax-block-rewards-staking-and-voting-guilds-and-more)
3. [Incentives and mechanics to address voter apathy](https://wax.io/blog/staking-and-voting-on-wax-a-technical-deep-dive)

Dependencies:
* [WAX v1.8.x](https://github.com/worldwide-asset-exchange/wax-blockchain/tree/wax-1.8.1-1.0.0)
* [WAX CDT v1.6.x](https://github.com/worldwide-asset-exchange/wax-cdt/tree/wax-1.6.1-1.0.0)

### Installation Instructions
To build the contracts and the unit tests:
* First, ensure that your __eosio__ is compiled to the core symbol for the EOSIO blockchain that intend to deploy to.
* Second, make sure that you have ```sudo make install```ed __eosio__.
* Then just run the ```build.sh``` in the top directory to build all the contracts and the unit tests for these contracts.

The `main` branch contains the latest state of development; do not use this for production. Refer to the [releases page](https://github.com/eosnetworkfoundation/eos-system-contracts/releases) for current information on releases, pre-releases, and obsolete releases as well as the corresponding tags for those releases.

## Supported Operating Systems

[CDT](https://github.com/AntelopeIO/cdt) is required to build contracts. Any operating systems supported by CDT is sufficient to build the system contracts.

To build and run the tests as well, [Leap](https://github.com/AntelopeIO/leap) is also required as a dependency, which may have its further restrictions on supported operating systems.

## Building

The build guide below will assume you are running Ubuntu 20.04. However, as mentioned above, other operating systems may also be supported.

### Build or install CDT dependency

The CDT dependency is required. This release of the system contracts requires at least version 3.0 of CDT.

The easiest way to satisfy this dependency is to install CDT on your system through a package. Find the release of a compatible version of CDT from its [releases page](https://github.com/AntelopeIO/cdt/releases), download the package file appropriate for your OS from the attached assets, and install the package.

Alternatively, you can build CDT from source. Please refer to the guide in the [CDT README](https://github.com/AntelopeIO/cdt#building-from-source) for instructions on how to do this. If you choose to go with building CDT from source, please keep the path to the build directory in the shell environment variable `CDT_BUILD_PATH` for later use when building the system contracts.

### Optionally build Leap dependency

The Leap dependency is optional. It is only needed if you wish to also build the tests using the `BUILD_TESTS` CMake flag.

Unfortunately, it is not currently possible to satisfy the contract testing dependencies through the Leap packages made available from the [Leap releases page](https://github.com/AntelopeIO/leap/releases). So if you want to build the contract tests, you will first need to build Leap from source.

Please refer to the guide in the [Leap README](https://github.com/AntelopeIO/leap#building-from-source) for instructions on how to do this. If you choose to go with building Leap from source, please keep the path to the build directory in the shell environment variable `LEAP_BUILD_PATH` for later use when building the system contracts.

### Build system contracts

Beyond CDT and optionally Leap (if also building the tests), no additional dependencies are required to build the system contracts.

The instructions below assume you are building the system contracts with tests, have already built Leap from source, and have the CDT dependency installed on your system. For some other configurations, expand the hidden panels placed lower within this section.

For all configurations, you should first `cd` into the directory containing cloned system contracts repository.

Build system contracts with tests using Leap built from source and with installed CDT package:

```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -Dleap_DIR="${LEAP_BUILD_PATH}/lib/cmake/leap" ..
make -j $(nproc)
```

**Note:** `CMAKE_BUILD_TYPE` has no impact on the WASM files generated for the contracts. It only impacts how the test binaries are built. Use `-DCMAKE_BUILD_TYPE=Debug` if you want to create test binaries that you can debug.

<details>
<summary>Build system contracts with tests using Leap and CDT both built from source</summary>

```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -Dcdt_DIR="${CDT_BUILD_PATH}/lib/cmake/cdt" -Dleap_DIR="${LEAP_BUILD_PATH}/lib/cmake/leap" ..
make -j $(nproc)
```
</details>

<details>
<summary>Build system contracts without tests and with CDT build from source</summary>

```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -Dcdt_DIR="${CDT_BUILD_PATH}/lib/cmake/cdt" ..
make -j $(nproc)
```

</details>

#### Supported CMake options

The following is a list of custom CMake options supported in building the system contracts (default values are shown below):

```
-DBUILD_TESTS=OFF                       Do not build the tests

-DSYSTEM_CONFIGURABLE_WASM_LIMITS=ON    Enable use of the CONFIGURABLE_WASM_LIMITS
                                        protocol feature

-DSYSTEM_BLOCKCHAIN_PARAMETERS=ON       Enable use of the BLOCKCHAIN_PARAMETERS
                                        protocol feature
```

### Running tests

Assuming you built with `BUILD_TESTS=ON`, you can run the tests.

```
cd build/tests
ctest -j $(nproc)
```

Alternatively, use the prebuilt contracts development docker image to get up and running with contracts develompment quickly:
1. Start the interactive shell: `make dev-docker-start`
1. Run all the tests: `make test`
1. Run a single test: `make compile && ./build/tests/unit_test --log_level=all --run_test=eosio_system_tests/producer_pay_as_gbm`

## License

[MIT](LICENSE)
