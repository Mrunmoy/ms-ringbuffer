#!/usr/bin/env python3
"""Build script for ms-ringbuffer.

Usage:
    python build.py              # build only
    python build.py -c           # clean build
    python build.py -t           # build + run tests
    python build.py -e           # build + examples
    python build.py -c -t -e     # clean build + tests + examples
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


def configure(examples=False):
    if not os.path.isfile(os.path.join(BUILD_DIR, "CMakeCache.txt")):
        cmd = ["cmake", "-B", BUILD_DIR, "-DCMAKE_BUILD_TYPE=Release"]
        if examples:
            cmd.append("-DMS_RINGBUFFER_BUILD_EXAMPLES=ON")
        run(cmd, cwd=ROOT)


def build(examples=False):
    configure(examples)
    run(["cmake", "--build", BUILD_DIR, f"-j{os.cpu_count()}"], cwd=ROOT)


def test():
    run(["ctest", "--test-dir", BUILD_DIR, "--output-on-failure"], cwd=ROOT)


def main():
    parser = argparse.ArgumentParser(description="Build ms-ringbuffer")
    parser.add_argument("-c", "--clean", action="store_true", help="clean before building")
    parser.add_argument("-t", "--test", action="store_true", help="run tests after building")
    parser.add_argument("-e", "--examples", action="store_true", help="build examples")
    args = parser.parse_args()

    if args.clean:
        clean()

    build(examples=args.examples)

    if args.test:
        test()


if __name__ == "__main__":
    main()
