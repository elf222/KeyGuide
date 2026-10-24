#!/usr/bin/env python3
# scripts/measure_idle.py
"""Compare macOS process CPU time and wakeups without sampling or root access."""

import argparse
import ctypes
import json
import math
import sys
import time


class ProcessUsage(ctypes.Structure):
    # rusage_info_v2 from sys/resource.h. CPU counters use Mach absolute-time ticks.
    _fields_ = [("uuid", ctypes.c_uint8 * 16)] + [
        (name, ctypes.c_uint64)
        for name in (
            "user_time", "system_time", "idle_wakeups", "interrupt_wakeups",
            "pageins", "wired_size", "resident_size", "physical_footprint",
            "start_time", "exit_time", "child_user_time", "child_system_time",
            "child_idle_wakeups", "child_interrupt_wakeups", "child_pageins",
            "child_elapsed_time", "disk_read", "disk_written",
        )
    ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pids", type=int, nargs="+")
    parser.add_argument("--seconds", type=float, default=20)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("This measurement tool requires macOS; the application remains portable.")
    if not math.isfinite(args.seconds) or args.seconds <= 0:
        parser.error("--seconds must be positive and finite")

    library = ctypes.CDLL("/usr/lib/libproc.dylib", use_errno=True)
    library.proc_pid_rusage.argtypes = (ctypes.c_int, ctypes.c_int, ctypes.c_void_p)
    library.proc_pid_rusage.restype = ctypes.c_int

    class Timebase(ctypes.Structure):
        _fields_ = [("numerator", ctypes.c_uint32), ("denominator", ctypes.c_uint32)]

    system = ctypes.CDLL("/usr/lib/libSystem.B.dylib")
    system.mach_timebase_info.argtypes = (ctypes.POINTER(Timebase),)
    system.mach_timebase_info.restype = ctypes.c_int
    timebase = Timebase()
    if system.mach_timebase_info(ctypes.byref(timebase)) or not timebase.denominator:
        raise RuntimeError("Cannot read the Mach timebase")

    def read_usage(pid):
        usage = ProcessUsage()
        if library.proc_pid_rusage(pid, 2, ctypes.byref(usage)):
            raise OSError(ctypes.get_errno(), f"Cannot read process {pid}")
        return usage

    before = {pid: read_usage(pid) for pid in args.pids}
    start = time.monotonic()
    time.sleep(args.seconds)
    elapsed = time.monotonic() - start
    measurements = []
    for pid, old in before.items():
        new = read_usage(pid)
        if new.uuid[:] != old.uuid[:] or new.start_time != old.start_time:
            raise RuntimeError(f"Process {pid} restarted during measurement")
        cpu_ticks = new.user_time + new.system_time - old.user_time - old.system_time
        cpu_seconds = cpu_ticks * timebase.numerator / timebase.denominator / 1e9
        measurements.append({
            "pid": pid,
            "elapsed_seconds": round(elapsed, 3),
            "cpu_percent_one_core": round(100 * cpu_seconds / elapsed, 3),
            "idle_wakeups_per_second": round((new.idle_wakeups - old.idle_wakeups) / elapsed, 3),
            "interrupt_wakeups_per_second": round(
                (new.interrupt_wakeups - old.interrupt_wakeups) / elapsed, 3
            ),
            "physical_memory_mib": round(new.physical_footprint / 2**20, 2),
        })
    if args.json:
        print(json.dumps(measurements, indent=2))
    else:
        print("PID       CPU %       Idle wakeups/s   Interrupt wakeups/s   Memory MiB")
        for row in measurements:
            print(f"{row['pid']:<9} {row['cpu_percent_one_core']:<11.3f} "
                  f"{row['idle_wakeups_per_second']:<16.3f} "
                  f"{row['interrupt_wakeups_per_second']:<21.3f} "
                  f"{row['physical_memory_mib']:.2f}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError) as error:
        sys.exit(str(error))
