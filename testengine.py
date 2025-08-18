import argparse
import subprocess
import sys
import os
import time

def run_fastchess(cmd_args):
    print('[INFO] Starting fastchess')
    print('[INFO]', ' '.join(cmd_args))
    subprocess.run(cmd_args, check=True)

def elo_mode(args):
    cmd = ["fastchess"]

    for idx, eng in enumerate(args.engines, start=1):
        name = os.path.splitext(os.path.basename(eng))[0]
        cmd += ["-engine", f'cmd={eng}', f'name={name}']

    cmd += ['-rounds', str(args.num_games), '-repeat', '-each', 'tc=10+0.1']

    # Opening book
    if args.openingbook:
        cmd += [
            "-openings",
            f"file={args.openingbook}",
            "format=pgn",
            "order=random"
        ]

    # Concurrency
    if args.concurrency:
        cmd += ['-concurrency', str(args.concurrency)]

    # Output
    cmd += [
        "-pgnout", f'elo_{time.strftime("%Y%m%d-%H%M%S", time.localtime())}.pgn',
        "-output", "format=fastchess",
        # "-log", "level=err"
    ]

    run_fastchess(cmd)

def sprt_mode(args):
    cmd = ["fastchess"]

    # Two engines only
    if len(args.engines) != 2:
        sys.exit("[ERROR] SPRT mode requires exactly 2 engines.")

    for eng in args.engines:
        name = os.path.splitext(os.path.basename(eng))[0]
        cmd += ["-engine", f"cmd={eng}", f"name={name}"]

    cmd += ["-each", "tc=10+0.1"]  # default

    # Opening book
    if args.openingbook:
        cmd += [
            "-openings",
            f"file={args.openingbook}",
            "format=pgn",
            "order=random"
        ]

    # SPRT parameters
    cmd += [
        "-sprt",
        f"elo0=0",
        f"elo1={args.elohyp}",
        "alpha=0.05",
        "beta=0.05",
        "model=normalized"
    ]

    # Concurrency
    if args.concurrency:
        cmd += ["-concurrency", str(args.concurrency)]

    # Output
    cmd += [
        "-pgnout", f'sprt_{time.strftime("%Y%m%d-%H%M%S", time.localtime())}.pgn',
        "-output", "format=fastchess"
    ]

    run_fastchess(cmd)

def main():
    parser = argparse.ArgumentParser(
        description="Wrapper for Fastchess testing: Elo estimation or SPRT comparison"
    )

    subparsers = parser.add_subparsers(dest="mode", required=True)

    # Elo mode parser
    parser_elo = subparsers.add_parser("elo", help="Elo estimation mode")
    parser_elo.add_argument("-e", "--engines", nargs="+", required=True, help="Engine executables")
    parser_elo.add_argument("-n", "--num-games", type=int, required=True, help="Games per pairing")
    parser_elo.add_argument("--openingbook", help="Opening book (PGN format)")
    parser_elo.add_argument("--concurrency", type=int, help="Number of concurrent games")
    parser_elo.set_defaults(func=elo_mode)

    # SPRT mode parser
    parser_sprt = subparsers.add_parser("sprt", help="SPRT comparison mode")
    parser_sprt.add_argument("-e", "--engines", nargs="+", required=True, help="Two engine executables")
    parser_sprt.add_argument("--elohyp", type=int, required=True, help="Elo improvement hypothesis (elo1)")
    parser_sprt.add_argument("--openingbook", help="Opening book (PGN format)")
    parser_sprt.add_argument("--concurrency", type=int, help="Number of concurrent games")
    parser_sprt.set_defaults(func=sprt_mode)

    args = parser.parse_args()
    args.func(args)

if __name__ == "__main__":
    main()
