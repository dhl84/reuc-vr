"""Runs the steps of scripts/drive.py against the game on the Quest.

The app must be started with a test server: args.txt in its folder holds the two lines "--test-server" and NAME. The channels are
abstract UNIX sockets on the headset (game/test_pipe_posix.cpp); this script forwards them with adb to two TCP ports.

  python -I scripts/drive_quest.py --package org.uc.darksidechronicles.vr --name q1 --out <folder> [--port 47310] steps...

The log of the game (uc.log in the app folder) is copied to <folder>/run.log while the run goes on, so waitlog and until work.
The step shot does not work here (the game would write the picture on the headset): use adb exec-out screencap.
"""
import argparse
import importlib.util
import subprocess
import sys
import threading
import time
from pathlib import Path

DRIVE = Path(__file__).resolve().parent / "drive.py"
spec = importlib.util.spec_from_file_location("drive", DRIVE)
drive = importlib.util.module_from_spec(spec)
sys.path.insert(0, str(DRIVE.parent))
spec.loader.exec_module(drive)
from uctest.transport import PipeTransport  # noqa: E402


def adb(*args, **kw):
    return subprocess.run(["adb", *args], capture_output=True, text=True, **kw)


class App:
    """Stands in for the Popen object of drive.Game: the app on the headset."""
    def __init__(self, package):
        self.package, self.returncode, self._checked, self._alive = package, None, 0.0, True

    def poll(self):
        if self.returncode is None and time.monotonic() - self._checked > 2.0:
            self._checked = time.monotonic()
            if not adb("shell", "pidof", self.package).stdout.strip():
                self.returncode = 1
        return self.returncode

    def wait(self, timeout=None):
        return self.returncode

    def kill(self):
        pass


def quest_init(self, args):
    self.out = Path(args.out).resolve()
    self.out.mkdir(parents=True, exist_ok=True)
    self.name = args.name
    adb("forward", "tcp:%d" % args.port, "localabstract:uc-test-%s-cmd" % args.name)
    adb("forward", "tcp:%d" % (args.port + 1), "localabstract:uc-test-%s-evt" % args.name)
    self.proc = App(args.package)
    deadline = time.monotonic() + args.connect_seconds   # the game opens its channels after the pipeline warm-up
    while "uc-test-%s-cmd" % args.name not in adb("shell", "cat", "/proc/net/unix").stdout:
        if time.monotonic() > deadline:
            raise RuntimeError("the game did not open its test channels")
        time.sleep(1.0)
    # After the channels are open: the app has started its new uc.log by then (a tail that starts earlier reads the log of the last run).
    self.log = open(self.out / "run.log", "wb")
    self.tail = subprocess.Popen(["adb", "exec-out", "tail", "-c", "+1", "-f", "/sdcard/Android/data/%s/files/uc.log" % args.package], stdout=self.log, stderr=subprocess.DEVNULL)
    self.pipe = PipeTransport.tcp(args.name, args.port, connect_seconds=args.connect_seconds)
    self.replies, self.frame, self.cv, self.next_id, self.aim, self.log_pos = {}, {}, threading.Condition(), 1, (0.0, 0.0), 0
    threading.Thread(target=self._read, daemon=True).start()


class OsOnQuest:
    """The os module for drive.py, but a captured frame is pulled from the headset and not moved on this PC (the step shot)."""
    def __getattr__(self, name):
        import os
        return getattr(os, name)

    def replace(self, source, target):
        adb("pull", str(source), str(target))


def main():
    drive.os = OsOnQuest()
    ap = argparse.ArgumentParser()
    ap.add_argument("--package", required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--port", type=int, default=47310)
    known, steps = ap.parse_known_args()
    drive.Game.__init__ = quest_init
    # drive.main reads its own arguments: give it placeholders for the ones that only start a local game.
    sys.argv = [str(DRIVE), "--exe", "quest", "--game", "quest", "--out", known.out, "--connect-seconds", "120"] + steps
    parse = argparse.ArgumentParser.parse_args

    def parse_with_quest(parser, *a, **kw):
        args = parse(parser, *a, **kw)
        args.package, args.name, args.port = known.package, known.name, known.port
        return args
    argparse.ArgumentParser.parse_args = parse_with_quest
    try:
        drive.main()
    finally:
        adb("forward", "--remove", "tcp:%d" % known.port)
        adb("forward", "--remove", "tcp:%d" % (known.port + 1))


if __name__ == "__main__":
    main()
