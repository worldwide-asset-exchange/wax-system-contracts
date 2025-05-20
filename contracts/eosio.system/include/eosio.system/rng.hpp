#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/asset.hpp>

using namespace eosio;
using namespace std;

namespace rng {

    static constexpr name RNG_ACCOUNT = name("orng.wax");


    struct [[eosio::table]] treasury
    {
        eosio::asset pool_balance;
    };
    using treas_singleton_type = eosio::singleton<"treasury"_n, treasury>;

    inline treas_singleton_type treas_t = treas_singleton_type(RNG_ACCOUNT, RNG_ACCOUNT.value);

    // Use inline function to prevent duplicate symbols
    inline eosio::asset get_rng_balance() {
        auto itr = treas_t.get();
        return itr.pool_balance;
    }
}