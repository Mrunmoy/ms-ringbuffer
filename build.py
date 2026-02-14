#!/usr/bin/env python3
"""
Build script for ms-ringbuffer.

Usage:
  python build.py                 # build only
  python build.py -c              # clean build
  python build.py -t              # build + run tests
  python build.py -e              # build + examples
  python build.py -b              # build + benchmarks
  python build.py -b --run-size   # build + benchmarks + run size harness
  python build.py -c -t -e -b     # clean build + tests + examples + benchmarks
"""

import argparse
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.join(ROOT, "build")


def run(cmd, **kwargs):
    print(f">>> {' '.join(cmd)}")
    result = subprocess.run(cmd, **kwargs)
    if result.returncode != 0:
        sys.exit(result.returncode)


def clean():
    if os.path.isdir(BUILD_DIR):
        print(f"Removing {BUILD_DIR}")
        shutil.rmtree(BUILD_DIR)


def configure(examples=False, benchmarks=False):
    os.makedirs(BUILD_DIR, exist_ok=True)

    cmd = [
        "cmake",
        "-B",
        BUILD_DIR,
        "-DCMAKE_BUILD_TYPE=Release",
    ]

    if examples:
        cmd.append("-DMS_RINGBUFFER_BUILD_EXAMPLES=ON")
    else:
        cmd.append("-DMS_RINGBUFFER_BUILD_EXAMPLES=OFF")

    if benchmarks:
        cmd.append("-DMS_RINGBUFFER_BUILD_BENCHMARKS=ON")
    else:
        cmd.append("-DMS_RINGBUFFER_BUILD_BENCHMARKS=OFF")

    run(cmd, cwd=ROOT)


def build(examples=False, benchmarks=False):
    configure(examples=examples, benchmarks=benchmarks)
    run(["cmake", "--build", BUILD_DIR, f"-j{os.cpu_count()}"], cwd=ROOT)


def test():
    run(["ctest", "--test-dir", BUILD_DIR, "--output-on-failure"], cwd=ROOT)


def run_size():
    exe = os.path.join(BUILD_DIR, "bench", "ms_ringbuffer_size")
    if os.name == "nt":
        exe += ".exe"

    run([exe], cwd=ROOT)

def size_report():
    run(["cmake", "--build", BUILD_DIR, "--target", "ms_ringbuffer_size_report"], cwd=ROOT)

    report_path = os.path.join(BUILD_DIR, "bench", "size_report.txt")
    if os.path.isfile(report_path):
        print("\n--- size_report.txt ---")
        with open(report_path, "r", encoding="utf-8") as f:
            print(f.read())
    else:
        print(f"error: size report not found at: {report_path}")
        sys.exit(1)

def run_bench(bench_time_s="0.05s", repetitions=1, out_json=None):
    exe = os.path.join(BUILD_DIR, "bench", "ms_ringbuffer_bench")
    if os.name == "nt":
        exe += ".exe"

    cmd = [
        exe,
        f"--benchmark_min_time={bench_time_s}",
        f"--benchmark_repetitions={repetitions}",
    ]

    if out_json:
        cmd.append("--benchmark_format=json")
        cmd.append(f"--benchmark_out={out_json}")
        cmd.append("--benchmark_out_format=json")

        # Suppress JSON dump to console
        with open(os.devnull, "w") as devnull:
            run(cmd, cwd=ROOT, stdout=devnull)
    else:
        run(cmd, cwd=ROOT)

def collect_metrics(bench_time_s="0.2s", repetitions=1):
    # 1) sizeof report
    exe = os.path.join(BUILD_DIR, "bench", "ms_ringbuffer_size")
    if os.name == "nt":
        exe += ".exe"

    sizeof_path = os.path.join(BUILD_DIR, "bench", "sizeof_report.txt")
    print(f">>> {exe} > {sizeof_path}")
    with open(sizeof_path, "w", encoding="utf-8") as f:
        result = subprocess.run([exe], cwd=ROOT, stdout=f)
        if result.returncode != 0:
            sys.exit(result.returncode)

    # 2) size segments report (text/data/bss)
    size_report()

    # 3) benchmarks json
    results_path = os.path.join(BUILD_DIR, "bench", "results.json")
    run_bench(bench_time_s=bench_time_s, repetitions=repetitions, out_json=results_path)

    print("\nWrote:")
    print(f"  {sizeof_path}")
    print(f"  {os.path.join(BUILD_DIR, 'bench', 'size_report.txt')}")
    print(f"  {os.path.join(BUILD_DIR, 'bench', 'ms_ringbuffer_size.map')}")
    print(f"  {results_path}")


def main():
    parser = argparse.ArgumentParser(description="Build ms-ringbuffer")
    parser.add_argument("-c", "--clean", action="store_true", help="clean before building")
    parser.add_argument("-t", "--test", action="store_true", help="run tests after building")
    parser.add_argument("-e", "--examples", action="store_true", help="build examples")
    parser.add_argument("-b", "--benchmarks", action="store_true", help="build benchmarks")
    parser.add_argument("--run-size", action="store_true", help="run size harness (requires -b)")
    parser.add_argument("--size-report", action="store_true", help="generate and print code/ram size report (requires -b)")
    parser.add_argument("--run-bench", action="store_true", help="run benchmarks after building (requires -b)")
    parser.add_argument("--bench-time", default="0.05s", help="google-benchmark min time (use seconds suffix, e.g. 0.05s, 0.2s, 1s)")
    parser.add_argument("--bench-reps", type=int, default=1, help="google-benchmark repetitions")
    parser.add_argument("--bench-out", default="", help="write benchmark json to this path (requires --run-bench)")
    parser.add_argument("--collect-metrics", action="store_true", help="write sizeof_report.txt + size_report.txt (+ map) (requires -b)")

    args = parser.parse_args()

    if args.clean:
        clean()

    build(examples=args.examples, benchmarks=args.benchmarks)

    if args.test:
        test()

    if args.run_size:
        if not args.benchmarks:
            print("error: --run-size requires -b/--benchmarks")
            sys.exit(2)
        run_size()

    if args.size_report:
        if not args.benchmarks:
            print("error: --size-report requires -b/--benchmarks")
            sys.exit(2)
        size_report()

    if args.run_bench:
        if not args.benchmarks:
            print("error: --run-bench requires -b/--benchmarks")
            sys.exit(2)

        out_json = args.bench_out if args.bench_out else None
        run_bench(bench_time_s=args.bench_time, repetitions=args.bench_reps, out_json=out_json)

    if args.collect_metrics:
        if not args.benchmarks:
            print("error: --collect-metrics requires -b/--benchmarks")
            sys.exit(2)
        collect_metrics(bench_time_s=args.bench_time, repetitions=args.bench_reps)

if __name__ == "__main__":
    main()