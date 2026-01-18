#!/usr/bin/env python3


"""
CDB File Descriptor Leak Reproducer

This script demonstrates the file descriptor leak in the cdbx library.
It generates a CDB file every  seconds using random data, which will
eventually exhaust file descriptors if the leak exists.

Usage:
    python fd_leak_reproducer.py output.cdb

The script will:
1. Generate random key-value pairs
2. Write them to a CDB file using given method
3. Repeat it couple of times
4. Print file descriptor count to show the leak

"""

import sys
import os
import time
import random
import string
import tempfile

REPEATS = 5
SLEEP_TIME = 0.2

try:
    from cdbx import CDB
except ImportError as e:
    print("ERROR: cdbx module not found. Please install it first.")
    print(f"error: {e}")
    sys.exit(1)


def count_open_fds():
    """Count the number of open file descriptors for this process."""
    try:
        # Works on Linux
        return len(os.listdir(f'/proc/{os.getpid()}/fd'))
    except (FileNotFoundError, PermissionError):
        try:
            # Works on macOS
            import subprocess
            pid = os.getpid()
            #print(f"PID: {pid}")
            result = subprocess.run(
                ['lsof', '-p', str(pid)],
                capture_output=True,
                text=True
            )
            # Count lines minus header
            return len(result.stdout.strip().split('\n')) - 1
        except:
            return -1  # Can't count on this platform


def generate_random_data(num_entries=100):
    """Generate random key-value pairs."""
    data = {}
    for i in range(num_entries):
        # Random key: 8-16 characters
        key_len = random.randint(8, 16)
        key = ''.join(random.choices(string.ascii_letters + string.digits, k=key_len))

        # Random value: 20-100 characters
        val_len = random.randint(20, 100)
        value = ''.join(random.choices(string.ascii_letters + string.digits + ' ', k=val_len))

        data[key.encode()] = value.encode()

    return data


def write_cdb_filename(filename, data, mmap=None):
    """
    Write CDB file using filename.
    """

    maker = CDB.make(filename, mmap=mmap)

    for key, value in data.items():
        maker.add(key, value)

    cdb = maker.commit()
    cdb.close()
    maker.close()

def write_cdb_fd(filename, data, mmap=None):
    """
    Write CDB file using fd.
    """
    with tempfile.TemporaryFile() as fp:
        maker = CDB.make(fp.fileno(), mmap=mmap)

        for key, value in data.items():
            maker.add(key, value)

        cdb = maker.commit()

    maker.close()
    cdb.close()

def write_cdb_fp(filename, data, mmap=None):
    """
    Write CDB file using fp.
    """
    with tempfile.TemporaryFile() as fp:
        maker = CDB.make(fp, mmap=None)

        for key, value in data.items():
            maker.add(key, value)

        cdb = maker.commit()

    maker.close()
    cdb.close()

def main():
    if len(sys.argv) != 4:
        print("Usage: python check.py <output.cdb> <method> <mmap>")
        print("\nThis script will generate a CDB file couple of times,")
        print("demonstrating the file descriptor leak in cdbx.")
        sys.exit(1)

    output_file = sys.argv[1]
    method = sys.argv[2]
    if (sys.argv[3] == "False"):
        _mmap = False
    elif (sys.argv[3] == "True"):
        _mmap = True
    else:
        _mmap = None

    print(f"\nOutput file: {output_file}")
    print(f"Method: {method}")
    print("\nWatch the 'Open FDs' count increase over time.")
    print("-" * 70)

    iteration = 0
    initial_fds = count_open_fds()

    try:
        while iteration < REPEATS:
            iteration += 1

            # Count FDs before
            fds_before = count_open_fds()

            # Generate random data
            data = generate_random_data(num_entries=100)

            # Write CDB file
            if method == "filename":
                write_cdb_filename(output_file, data, mmap=_mmap)
            elif method == "fp":
                write_cdb_fp(output_file, data, mmap=_mmap)
            elif method == "fd":
                write_cdb_fd(output_file, data, mmap=_mmap)
            else:
                print(f"Unknown method: {method}")

            # Count FDs after
            fds_after = count_open_fds()

            # Calculate leak
            if initial_fds > 0 and fds_after > 0:
                leaked = fds_after - initial_fds
                leaked_this_iter = fds_after - fds_before

                print(f"Iteration {iteration:4d} | "
                      f"Open FDs: {fds_after:4d} | "
                      f"Leaked total: {leaked:4d} | "
                      f"Leaked this iteration: {leaked_this_iter:2d}")

            else:
                print(f"Iteration {iteration:4d} | "
                      f"Generated {len(data)} entries | "
                      f"File: {os.path.getsize(output_file)} bytes")
            time.sleep(SLEEP_TIME)

    except Exception as e:
        print(f"\n\nERROR: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
