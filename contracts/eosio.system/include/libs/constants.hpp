// "Copyright [2022] <alcor exchange>"
#ifndef INCLUDE_CONSTANTS_HPP_
#define INCLUDE_CONSTANTS_HPP_

// Allow conversation for uint128 builtin
#define WIDE_INTEGER_HAS_LIMB_TYPE_UINT64

#include <math/wide_integer/uintwide_t.h>
#include <map>
#include <limits>
#include <eosio/eosio.hpp>

using uint256 = math::wide_integer::uint256_t;
using int256 = math::wide_integer::int256_t;

// const uint64_t UINT64_MAX = std::numeric_limits<uint64_t>::max();
const uint128_t UINT128_MAX = std::numeric_limits<uint128_t>::max();
const uint256 UINT256_MAX = std::numeric_limits<uint256>::max();

const uint128_t Q64 = uint128_t(uint256("0x10000000000000000"));
const uint8_t FIXED_POINT_64 = 64;

const uint8_t FEE_PROTOCOLS[] = {0, 4, 10}; // it must be 1/4 (FEE_PROTOCOLS = 4 ) Or 1/10 (FEE_PROTOCOLS = 10) of swap fee.
const uint32_t BAR_FEE = 1000000;

static std::map<uint32_t, int32_t> FeeAmount_TickSpacing = {{500, 10}, {3000, 60}, {10000, 200}};
const uint64_t SYSTEM_INDEX = 0;
#endif  // INCLUDE_CONSTANTS_HPP_
