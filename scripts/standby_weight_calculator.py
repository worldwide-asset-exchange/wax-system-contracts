#!/usr/bin/env python3
"""
WAX Standby Weight Calculator

Calculates the standby_slot_weight needed to achieve a target active producer payout.

Example usage:
    $ python3 standby_weight_calculator.py --producers 20 --standbys 7 --target 53
"""


def calculate_standby_weight(
    wax_price_usd: float,
    num_active_producers: int,
    num_standby_slots: int,
    target_producer_pay_usd_per_day: float,
    total_supply_wax: float = 4_600_000_000,
    continuous_rate: float = 0.04879,
    pay_split_scale: int = 10000
) -> dict:
    """
    Calculate the standby_slot_weight needed to achieve a target active producer payout.

    Args:
        wax_price_usd: Current WAX price in USD
        num_active_producers: Number of active producer slots (A)
        num_standby_slots: Number of standby slots (S)
        target_producer_pay_usd_per_day: Target USD payout per active producer per day
        total_supply_wax: Total WAX supply (default 4.6B)
        continuous_rate: Inflation rate (default 0.04879 = ~5% annual)
        pay_split_scale: PAY_SPLIT_SCALE constant (default 10000)

    Returns:
        dict with calculation results
    """
    A = num_active_producers
    S = num_standby_slots

    # Calculate daily block pay pool (B) in WAX
    # 20% of daily inflation goes to block pay
    daily_inflation_wax = continuous_rate * total_supply_wax / 365.25
    B = daily_inflation_wax / 5  # 20% to block pay

    # Convert target to WAX
    P_active_target = target_producer_pay_usd_per_day / wax_price_usd

    # Calculate W
    # From: P_active = B * 10000 / (A * 10000 + W * S)
    # Solving for W: W = 10000 * (B - P_active * A) / (P_active * S)

    numerator = pay_split_scale * (B - P_active_target * A)
    denominator = P_active_target * S

    if denominator == 0:
        W = None
        feasible = False
    else:
        W = numerator / denominator
        feasible = 0 <= W <= pay_split_scale

    # Calculate actual payouts for the computed W (clamped to valid range)
    W_clamped = max(0, min(pay_split_scale, W)) if W is not None else 0
    total_weight = (A * pay_split_scale) + (W_clamped * S)

    actual_per_active_wax = B * pay_split_scale / total_weight
    actual_per_standby_wax = B * W_clamped / total_weight

    # Calculate min/max achievable payouts
    # W = 0: standbys get nothing, max to producers
    max_per_active_wax = B / A
    # W = 10000: equal split
    min_per_active_wax = B / (A + S)

    return {
        "inputs": {
            "wax_price_usd": wax_price_usd,
            "num_active_producers": A,
            "num_standby_slots": S,
            "target_producer_pay_usd_per_day": target_producer_pay_usd_per_day,
            "total_supply_wax": total_supply_wax,
        },
        "daily_block_pay_pool_wax": B,
        "target_per_active_wax": P_active_target,
        "calculated_W": W,
        "W_clamped": W_clamped,
        "feasible": feasible,
        "actual_per_active_wax": actual_per_active_wax,
        "actual_per_active_usd": actual_per_active_wax * wax_price_usd,
        "actual_per_standby_wax": actual_per_standby_wax,
        "actual_per_standby_usd": actual_per_standby_wax * wax_price_usd,
        "achievable_range": {
            "min_per_active_wax": min_per_active_wax,
            "min_per_active_usd": min_per_active_wax * wax_price_usd,
            "max_per_active_wax": max_per_active_wax,
            "max_per_active_usd": max_per_active_wax * wax_price_usd,
        }
    }


def print_result(result: dict) -> None:
    """Pretty print the calculation results."""
    print("=== Standby Weight Calculator ===\n")
    print(f"Inputs:")
    print(f"  WAX price: ${result['inputs']['wax_price_usd']}")
    print(f"  Active producers: {result['inputs']['num_active_producers']}")
    print(f"  Standby slots: {result['inputs']['num_standby_slots']}")
    print(f"  Target producer pay: ${result['inputs']['target_producer_pay_usd_per_day']}/day")
    print(f"  Total WAX supply: {result['inputs']['total_supply_wax']:,.0f}")

    print(f"\nDaily block pay pool: {result['daily_block_pay_pool_wax']:,.0f} WAX")
    print(f"Target per active producer: {result['target_per_active_wax']:,.0f} WAX (${result['inputs']['target_producer_pay_usd_per_day']}/day)")

    print(f"\nCalculated W: {result['calculated_W']:,.0f}")
    print(f"Feasible (0 <= W <= 10000): {result['feasible']}")

    print(f"\nAchievable range for active producers:")
    print(f"  Min (W=10000): {result['achievable_range']['min_per_active_wax']:,.0f} WAX (${result['achievable_range']['min_per_active_usd']:.2f}/day)")
    print(f"  Max (W=0):     {result['achievable_range']['max_per_active_wax']:,.0f} WAX (${result['achievable_range']['max_per_active_usd']:.2f}/day)")

    if result['feasible']:
        print(f"\n>>> Set standby_slot_weight to: {int(result['calculated_W'])}")
        print(f"    This gives standbys: {result['actual_per_standby_wax']:,.0f} WAX (${result['actual_per_standby_usd']:.2f}/day)")
    else:
        print(f"\n>>> Target ${result['inputs']['target_producer_pay_usd_per_day']}/day is outside achievable range!")
        print(f"    With W clamped to {int(result['W_clamped'])}:")
        print(f"    Active producers get: {result['actual_per_active_wax']:,.0f} WAX (${result['actual_per_active_usd']:.2f}/day)")
        print(f"    Standbys get: {result['actual_per_standby_wax']:,.0f} WAX (${result['actual_per_standby_usd']:.2f}/day)")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Calculate WAX standby_slot_weight for target producer payout")
    parser.add_argument("--price", type=float, default=0.008, help="WAX price in USD (default: 0.008)")
    parser.add_argument("--producers", "-p", type=int, default=9, help="Number of active producers (default: 9)")
    parser.add_argument("--standbys", "-s", type=int, default=6, help="Number of standby slots (default: 6)")
    parser.add_argument("--target", "-t", type=float, default=53, help="Target USD/day per active producer (default: 53)")
    parser.add_argument("--supply", type=float, default=4_600_000_000, help="Total WAX supply (default: 4.8B)")

    args = parser.parse_args()

    result = calculate_standby_weight(
        wax_price_usd=args.price,
        num_active_producers=args.producers,
        num_standby_slots=args.standbys,
        target_producer_pay_usd_per_day=args.target,
        total_supply_wax=args.supply,
    )

    print_result(result)

    # Also show some alternative scenarios
    print("\n" + "=" * 50)
    print("Alternative scenarios:")
    print("=" * 50)

    print("\nVarying WAX price (same target $53/day):")
    for price in [0.005, 0.008, 0.01, 0.02, 0.04, 0.05]:
        r = calculate_standby_weight(price, args.producers, args.standbys, args.target, args.supply)
        status = f"W={int(r['calculated_W'])}" if r['feasible'] else f"W={r['calculated_W']:,.0f} (infeasible)"
        print(f"  ${price:.3f}: {status}")

    print(f"\nVarying slot configuration (WAX ${args.price}, target ${args.target}/day):")
    for a, s in [(9, 6), (12, 9), (15, 10), (21, 15), (21, 21)]:
        r = calculate_standby_weight(args.price, a, s, args.target, args.supply)
        status = f"W={int(r['calculated_W'])}" if r['feasible'] else f"infeasible (W={r['calculated_W']:,.0f})"
        print(f"  {a} active + {s} standby: {status}")
