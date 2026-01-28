#!/usr/bin/env python3
"""
WAX Standby Weight Calculator - By Pay Ratio

Calculates the standby_slot_weight needed to achieve a target pay ratio
between active producers and standbys.

Ratio M:N means producers get M parts for every N parts standbys get.
Examples:
  - 2:1 means producers get twice what standbys get
  - 1:1 means equal pay
  - 4:1 means producers get 4x what standbys get

    $ python3 standby_weight_by_ratio.py --ratio 8:1  --producers 20 --standbys 7
"""


def calculate_standby_weight_by_ratio(
    producer_ratio: float,
    standby_ratio: float,
    wax_price_usd: float,
    num_active_producers: int,
    num_standby_slots: int,
    total_supply_wax: float = 4_600_000_000,
    continuous_rate: float = 0.04879,
    pay_split_scale: int = 10000
) -> dict:
    """
    Calculate the standby_slot_weight for a target pay ratio.

    The ratio P_active : P_standby = M : N implies:
        W = PAY_SPLIT_SCALE * N / M

    Args:
        producer_ratio: M in the ratio M:N (producer portion)
        standby_ratio: N in the ratio M:N (standby portion)
        wax_price_usd: Current WAX price in USD
        num_active_producers: Number of active producer slots (A)
        num_standby_slots: Number of standby slots (S)
        total_supply_wax: Total WAX supply (default 4.8B)
        continuous_rate: Inflation rate (default 0.04879 = ~5% annual)
        pay_split_scale: PAY_SPLIT_SCALE constant (default 10000)

    Returns:
        dict with calculation results
    """
    M = producer_ratio
    N = standby_ratio
    A = num_active_producers
    S = num_standby_slots

    # Calculate W from ratio
    # P_active / P_standby = 10000 / W = M / N
    # Therefore: W = 10000 * N / M
    W = pay_split_scale * N / M

    feasible = 0 <= W <= pay_split_scale

    # Calculate daily block pay pool (B) in WAX
    daily_inflation_wax = continuous_rate * total_supply_wax / 365.25
    B = daily_inflation_wax / 5  # 20% to block pay

    # Calculate actual payouts
    W_clamped = max(0, min(pay_split_scale, W))
    total_weight = (A * pay_split_scale) + (W_clamped * S)

    per_active_wax = B * pay_split_scale / total_weight
    per_standby_wax = B * W_clamped / total_weight

    # Total going to each group
    total_to_producers_wax = per_active_wax * A
    total_to_standbys_wax = per_standby_wax * S

    # Percentage split
    pct_to_producers = (total_to_producers_wax / B) * 100
    pct_to_standbys = (total_to_standbys_wax / B) * 100

    return {
        "inputs": {
            "ratio": f"{M}:{N}",
            "producer_ratio": M,
            "standby_ratio": N,
            "wax_price_usd": wax_price_usd,
            "num_active_producers": A,
            "num_standby_slots": S,
            "total_supply_wax": total_supply_wax,
        },
        "calculated_W": W,
        "W_clamped": W_clamped,
        "W_integer": int(round(W_clamped)),
        "feasible": feasible,
        "daily_block_pay_pool_wax": B,
        "per_active_wax": per_active_wax,
        "per_active_usd": per_active_wax * wax_price_usd,
        "per_active_usd_monthly": per_active_wax * wax_price_usd * 30,
        "per_standby_wax": per_standby_wax,
        "per_standby_usd": per_standby_wax * wax_price_usd,
        "per_standby_usd_monthly": per_standby_wax * wax_price_usd * 30,
        "total_to_producers_wax": total_to_producers_wax,
        "total_to_standbys_wax": total_to_standbys_wax,
        "pct_to_producers": pct_to_producers,
        "pct_to_standbys": pct_to_standbys,
        "actual_ratio": per_active_wax / per_standby_wax if per_standby_wax > 0 else float('inf'),
    }


def print_result(result: dict) -> None:
    """Pretty print the calculation results."""
    print("=== Standby Weight Calculator (By Ratio) ===\n")
    print(f"Inputs:")
    print(f"  Target ratio (producer:standby): {result['inputs']['ratio']}")
    print(f"  WAX price: ${result['inputs']['wax_price_usd']}")
    print(f"  Active producers: {result['inputs']['num_active_producers']}")
    print(f"  Standby slots: {result['inputs']['num_standby_slots']}")
    print(f"  Total WAX supply: {result['inputs']['total_supply_wax']:,.0f}")

    print(f"\n>>> standby_slot_weight (W): {result['W_integer']}")
    print(f"    Feasible (0 <= W <= 10000): {result['feasible']}")

    print(f"\nDaily block pay pool: {result['daily_block_pay_pool_wax']:,.0f} WAX")

    print(f"\nPer-slot payouts:")
    print(f"  Active producer:  {result['per_active_wax']:,.0f} WAX/day = ${result['per_active_usd']:.2f}/day = ${result['per_active_usd_monthly']:.2f}/month")
    print(f"  Standby:          {result['per_standby_wax']:,.0f} WAX/day = ${result['per_standby_usd']:.2f}/day = ${result['per_standby_usd_monthly']:.2f}/month")
    print(f"  Actual ratio:     {result['actual_ratio']:.2f}:1")

    print(f"\nPool distribution:")
    print(f"  To {result['inputs']['num_active_producers']} producers: {result['total_to_producers_wax']:,.0f} WAX/day ({result['pct_to_producers']:.1f}%)")
    print(f"  To {result['inputs']['num_standby_slots']} standbys:  {result['total_to_standbys_wax']:,.0f} WAX/day ({result['pct_to_standbys']:.1f}%)")


def print_ratio_table(wax_price: float, num_producers: int, num_standbys: int, supply: float) -> None:
    """Print a table of common ratios and their resulting payouts."""
    print("\n" + "=" * 80)
    print("Common ratio scenarios:")
    print("=" * 80)
    print(f"{'Ratio':<10} {'W':<8} {'Producer/day':<20} {'Standby/day':<20} {'Producer/mo':<15}")
    print("-" * 80)

    ratios = [
        (1, 1),    # Equal
        (1.5, 1),  # 1.5:1
        (2, 1),    # 2:1
        (2.5, 1),  # 2.5:1
        (3, 1),    # 3:1
        (4, 1),    # 4:1
        (5, 1),    # 5:1
        (10, 1),   # 10:1
    ]

    for m, n in ratios:
        r = calculate_standby_weight_by_ratio(m, n, wax_price, num_producers, num_standbys, supply)
        ratio_str = f"{m}:{n}"
        w_str = str(r['W_integer'])
        prod_str = f"{r['per_active_wax']:,.0f} WAX (${r['per_active_usd']:.2f})"
        sb_str = f"{r['per_standby_wax']:,.0f} WAX (${r['per_standby_usd']:.2f})"
        mo_str = f"${r['per_active_usd_monthly']:.0f}"
        print(f"{ratio_str:<10} {w_str:<8} {prod_str:<20} {sb_str:<20} {mo_str:<15}")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(
        description="Calculate WAX standby_slot_weight for a target producer:standby pay ratio",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # 2:1 ratio (producers get twice what standbys get)
  python3 standby_weight_by_ratio.py --ratio 2:1

  # Custom ratio with specific parameters
  python3 standby_weight_by_ratio.py --ratio 3:1 --price 0.01 --producers 21 --standbys 21

  # Show table of common ratios
  python3 standby_weight_by_ratio.py --table
        """
    )
    parser.add_argument("--ratio", "-r", type=str, default="2:1",
                        help="Pay ratio M:N where producers get M for every N standbys get (default: 2:1)")
    parser.add_argument("--price", type=float, default=0.008,
                        help="WAX price in USD (default: 0.008)")
    parser.add_argument("--producers", "-p", type=int, default=9,
                        help="Number of active producers (default: 9)")
    parser.add_argument("--standbys", "-s", type=int, default=6,
                        help="Number of standby slots (default: 6)")
    parser.add_argument("--supply", type=float, default=4_600_000_000,
                        help="Total WAX supply (default: 4.8B)")
    parser.add_argument("--table", "-t", action="store_true",
                        help="Show table of common ratios")

    args = parser.parse_args()

    # Parse ratio
    try:
        m, n = map(float, args.ratio.split(":"))
    except ValueError:
        print(f"Error: Invalid ratio format '{args.ratio}'. Use M:N format (e.g., 2:1)")
        exit(1)

    result = calculate_standby_weight_by_ratio(
        producer_ratio=m,
        standby_ratio=n,
        wax_price_usd=args.price,
        num_active_producers=args.producers,
        num_standby_slots=args.standbys,
        total_supply_wax=args.supply,
    )

    print_result(result)

    if args.table:
        print_ratio_table(args.price, args.producers, args.standbys, args.supply)
    else:
        # Always show a brief ratio reference
        print("\n" + "-" * 50)
        print("Quick reference (use --table for full table):")
        print("-" * 50)
        for m_ref, n_ref in [(1, 1), (2, 1), (4, 1)]:
            r = calculate_standby_weight_by_ratio(m_ref, n_ref, args.price, args.producers, args.standbys, args.supply)
            print(f"  {m_ref}:{n_ref} ratio → W={r['W_integer']}, producer=${r['per_active_usd']:.2f}/day, standby=${r['per_standby_usd']:.2f}/day")
