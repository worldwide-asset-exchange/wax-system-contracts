// "Copyright [2022] <alcor exchange>"
#ifndef INCLUDE_HELP_HPP_
#define INCLUDE_HELP_HPP_

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

#include "tick_math.hpp"

static const char* charmap = "0123456789";


static std::string uint128ToString(const uint128_t& value)
{
    std::string result;
    result.reserve( 40 ); // max. 40 digits possible ( uint64_t has 20) 
    uint128_t helper = value;

    do {
        result += charmap[ helper % 10 ];
        helper /= 10;
    } while ( helper );
    std::reverse( result.begin(), result.end() );
    return result;
}

static void assertTicks(int32_t tickLower, int32_t tickUpper) {
  eosio::check(tickLower < tickUpper, "TickLower must smaller then tickUpper");
  eosio::check(tickLower >= TickMath::MIN_TICK, "Invalid tickLower");
  eosio::check(tickUpper <= TickMath::MAX_TICK, "Invalid tickUpper");
}

static void assertTokens(eosio::extended_asset tokenA, eosio::extended_asset tokenB) {
  eosio::check(tokenA.contract.value <= tokenB.contract.value, "Contracts must be sorted");
  if (tokenA.contract.value == tokenB.contract.value) {
    eosio::check(tokenA.quantity.symbol.code().raw() < tokenB.quantity.symbol.code().raw(),
                 "Symbols must be unique and sorted");
  }
}

#endif  // INCLUDE_HELP_HPP_
