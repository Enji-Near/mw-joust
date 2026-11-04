#!/usr/bin/env python3
"""Records the motor bench test to a timestamped log, surviving USB drops.

The board can only count while it runs; this adds the wall-clock time for each reset.

Run:      python3 tools/bench_logger.py [--port /dev/ttyACMx]
Command:  echo g > logs/cmd      (the logger owns the port, so commands go through this FIFO)
Summary:  python3 tools/bench_logger.py --summary logs/run-XXXX.log

Stdout carries only what needs attention: BOOT/EVT lines, lost contact, hourly progress.
"""
import argparse
import datetime as dt
import errno
import os
import re
import sys
import time

import serial
import serial.tools.list_ports

# Genuine Mega 2560s and the CH340 chip on most clones. Anything else: use --port.
BOARD_VID_PIDS = {(0x2341, 0x0042), (0x2341, 0x0010), (0x2A03, 0x0042), (0x1A86, 0x7523)}
BAUD = 115200
STALE_SECONDS = 30  # One cycle is ~3 s, so 30 s of silence during a test means trouble.
PROGRESS_SECONDS = 3600
LOG_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "logs")
CYC_RE = re.compile(r"CYC session=(\d+) total=(\d+)")


def now_iso():
    return dt.datetime.now().isoformat(timespec="seconds")


def emit(text):
    print(f"{now_iso()} {text}", flush=True)


def find_port():
    for p in serial.tools.list_ports.comports():
        if (p.vid, p.pid) in BOARD_VID_PIDS:
            return p.device
    return None


def open_port(path):
    ser = serial.Serial()
    ser.port = path
    ser.baudrate = BAUD
    ser.timeout = 0.2
    # Reduces, but can't eliminate, the DTR pulse that resets a Mega when the port opens.
    # Only the capacitor on RESET stops it fully (see README).
    ser.dtr = False
    ser.rts = False
    ser.open()
    return ser


class Tally:
    """Adds up cycles across resets from each session's last reported count."""

    def __init__(self):
        self.finished_sessions = []
        self.session = 0
        self.boots = []

    def feed(self, stamp, line):
        m = CYC_RE.search(line)
        if m:
            self.session = int(m.group(1))
        elif line.startswith("BOOT"):
            self.finished_sessions.append(self.session)
            self.session = 0
            self.boots.append((stamp, line))
        elif "TEST_START" in line:
            # `g` restarts the board's session counter without a reset.
            self.finished_sessions.append(self.session)
            self.session = 0

    @property
    def total(self):
        return sum(self.finished_sessions) + self.session

    def report(self):
        sessions = [s for s in self.finished_sessions if s] + ([self.session] if self.session else [])
        lines = [f"Total cycles: {self.total}", f"Sessions: {sessions}", f"Boots: {len(self.boots)}"]
        lines += [f"  {stamp}  {line}" for stamp, line in self.boots]
        return "\n".join(lines)


def summarize(path):
    tally = Tally()
    with open(path, encoding="utf8", errors="replace") as f:
        for raw in f:
            stamp, _, line = raw.rstrip("\n").partition(" ")
            tally.feed(stamp, line)
    print(tally.report())


def open_command_fifo(path):
    try:
        os.mkfifo(path)
    except FileExistsError:
        pass
    # O_RDWR keeps the FIFO open with no writer, so reads don't report EOF between commands.
    return os.open(path, os.O_RDWR | os.O_NONBLOCK)


def read_commands(fd):
    try:
        return os.read(fd, 256)
    except OSError as e:
        if e.errno == errno.EAGAIN:
            return b""
        raise


def run(port_arg, log_path):
    os.makedirs(os.path.dirname(log_path), exist_ok=True)
    fifo = open_command_fifo(os.path.join(os.path.dirname(log_path), "cmd"))
    tally = Tally()
    ser = None
    last_rx = time.time()
    last_progress = time.time()
    stale_reported = False
    active = False
    buf = b""
    emit(f"Logging to {log_path}")
    with open(log_path, "a", encoding="utf8", buffering=1) as log:
        while True:
            if ser is None:
                path = port_arg or find_port()
                try:
                    if not path:
                        raise OSError("no Mega found")
                    ser = open_port(path)
                    emit(f"CONNECTED {path}")
                    log.write(f"{now_iso()} # CONNECTED {path}\n")
                    last_rx = time.time()
                except OSError:
                    time.sleep(2)
                    continue
            try:
                chunk = ser.read(256)
                cmd = read_commands(fifo)
                if cmd:
                    ser.write(cmd)
                    log.write(f"{now_iso()} # SENT {cmd.decode(errors='replace').strip()}\n")
            except (OSError, serial.SerialException) as e:
                emit(f"DISCONNECTED {e}")
                log.write(f"{now_iso()} # DISCONNECTED\n")
                ser.close()
                ser = None
                buf = b""
                continue

            buf += chunk
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = raw.decode(errors="replace").strip()
                if not line:
                    continue
                stamp = now_iso()
                log.write(f"{stamp} {line}\n")
                last_rx = time.time()
                stale_reported = False
                tally.feed(stamp, line)
                if line.startswith("CYC"):
                    active = True
                else:
                    if "STOPPED" in line:
                        active = False
                    emit(line)

            t = time.time()
            if active and not stale_reported and t - last_rx > STALE_SECONDS:
                emit(f"STALE no data for {STALE_SECONDS}s, last total {tally.total}")
                log.write(f"{now_iso()} # STALE\n")
                stale_reported = True
            if active and t - last_progress > PROGRESS_SECONDS:
                emit(f"PROGRESS cycles={tally.total} boots={len(tally.boots)}")
                last_progress = t


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", help="serial port (default: find the Mega by USB ID)")
    ap.add_argument("--log", help="log file (default: logs/run-<timestamp>.log)")
    ap.add_argument("--summary", metavar="LOG", help="print totals from a log and exit")
    args = ap.parse_args()
    if args.summary:
        summarize(args.summary)
        return
    log_path = args.log or os.path.join(LOG_DIR, f"run-{dt.datetime.now():%Y%m%d-%H%M}.log")
    try:
        run(args.port, log_path)
    except KeyboardInterrupt:
        sys.exit(0)


if __name__ == "__main__":
    main()
