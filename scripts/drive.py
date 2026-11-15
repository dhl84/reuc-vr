"""Bring-up driver: starts a game build with its test server, sends Wii Remote input at given game times and saves frames.

Usage:
  python -I scripts/drive.py --exe build-dc/game/uc.exe --game <unpacked disc> --out <folder> [--seconds 120] [--speed 1] STEP...

A STEP is one of:
  wait:N            wait N retraces (60 per second of game time)
  press:BUTTONS     press and release Wii Remote buttons: a list of A B plus minus, for example press:A or press:A+B;
                    press:A*30 holds the buttons for 30 input frames (default 4)
  hold:BUTTONS*N    hold the buttons for N input frames and go on with the next step at once
  aim:X,Y           move the pointer (each value from -1 to 1; 0,0 is the centre of the screen)
  shot[:NAME]       save the current frame as a PNG in <folder>
  state             print the input frame counter and the retrace counter
  draws             print every draw of the current frame
  peek:ADDR,SIZE    print SIZE bytes of guest memory at hexadecimal ADDR
  peekptr:ADDR,OFFSET,SIZE   read the pointer at ADDR, then print SIZE bytes at pointer + OFFSET
  waitlog:TEXT[,N]  wait until TEXT appears in the game log after the place of the last waitlog or until step (at most N retraces, default 20000)
  until:TEXT,BUTTONS[,EVERY[,N]]   press BUTTONS every EVERY retraces (default 60) until TEXT appears in the game log (at most N retraces, default 20000)
  dump:NAME         write MEM1 (24 MB) and MEM2 (64 MB) to <folder>/NAME.mem1 and <folder>/NAME.mem2

The saves of the run go to <folder>/saves (the game never touches other saves). The log of the game goes to <folder>/run.log.
The protocol is docs/testing/test-server.md. The script needs no keyboard or mouse input and does not activate the window.
"""
import argparse
import json
import os
import subprocess
import sys
import threading
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from uctest.transport import PipeTransport  # noqa: E402

# Names follow docs/testing/test-server.md (plus 0x1000, minus 0x10). A name of the form 0x.... sends that mask as it is.
BUTTONS = {"A": 0x800, "B": 0x400, "PLUS": 0x1000, "MINUS": 0x10}


def button_mask(names):
    mask = 0
    for name in names.split("+"):
        mask |= int(name, 16) if name.lower().startswith("0x") else BUTTONS[name.upper()]
    return mask


class Game:
    def __init__(self, args):
        self.out = Path(args.out).resolve()
        (self.out / "saves").mkdir(parents=True, exist_ok=True)
        self.name = "drive%d" % os.getpid()
        cmd = [str(Path(args.exe).resolve()), "--game", str(Path(args.game).resolve()), "--test-server", self.name, "--save-root", str(self.out / "saves"),
               "--capture-dir", str(self.out), "--width", "1280", "--height", "720", "--mute", "--seconds", str(args.seconds),
               "--speed", str(args.speed)] + args.game_arg
        self.log = open(self.out / "run.log", "wb")
        self.proc = subprocess.Popen(cmd, stderr=self.log, stdout=subprocess.DEVNULL)
        self.pipe = PipeTransport(self.name, connect_seconds=args.connect_seconds, alive=lambda: self.proc.poll() is None)
        self.replies = {}
        self.frame = {}
        self.cv = threading.Condition()
        self.next_id = 1
        self.aim = (0.0, 0.0)
        self.log_pos = 0
        threading.Thread(target=self._read, daemon=True).start()

    def _read(self):
        while True:
            line = self.pipe.read_line()
            if line is None:
                with self.cv:
                    self.frame["closed"] = True
                    self.cv.notify_all()
                return
            try:
                msg = json.loads(line)
            except ValueError:
                continue
            with self.cv:
                if "id" in msg:
                    self.replies[msg["id"]] = msg
                elif msg.get("event") == "frame" or "retraces" in msg:
                    self.frame.update(msg)
                self.cv.notify_all()

    def command(self, cmd, timeout=30.0, **fields):
        with self.cv:
            ident = self.next_id
            self.next_id += 1
        self.pipe.send_line(json.dumps(dict(id=ident, cmd=cmd, **fields)))
        deadline = time.monotonic() + timeout
        with self.cv:
            while ident not in self.replies:
                if self.frame.get("closed") or time.monotonic() > deadline:
                    raise RuntimeError("no reply to %s" % cmd)
                self.cv.wait(0.2)
            reply = self.replies.pop(ident)
        if "error" in reply:
            raise RuntimeError("%s: %s" % (cmd, reply["error"]))
        return reply

    def retraces(self):
        return self.command("state").get("state", {}).get("retraces", 0)

    def wait(self, ticks):
        target = self.retraces() + ticks
        while self.proc.poll() is None:
            if self.retraces() >= target:
                return
            time.sleep(0.05)
        raise RuntimeError("the game ended (exit code %s)" % self.proc.returncode)


def log_has(game, text):
    """True when the game log holds the text after the place of the last match. The next search starts after the match."""
    game.log.flush()
    with open(game.out / "run.log", "rb") as f:
        f.seek(game.log_pos)
        data = f.read()
    at = data.find(text.encode())
    if at < 0:
        game.log_pos += max(0, len(data) - len(text))
        return False
    game.log_pos += at + len(text)
    return True


def wait_log(game, text, limit, press=None, every=60):
    start = game.retraces()
    last = None
    while game.proc.poll() is None:
        if log_has(game, text):
            return True
        now = game.retraces()
        if now - start > limit:
            return False
        if press is not None and (last is None or now - last >= every):
            game.command("input", kpad={"x": game.aim[0], "y": game.aim[1], "buttons": press}, hold_frames=4)
            last = now
        time.sleep(0.02)
    raise RuntimeError("the game ended (exit code %s)" % game.proc.returncode)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", required=True)
    ap.add_argument("--game", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--seconds", type=int, default=120)
    ap.add_argument("--speed", type=int, default=1)
    ap.add_argument("--connect-seconds", type=float, default=90.0)
    ap.add_argument("--game-arg", action="append", default=[])
    ap.add_argument("steps", nargs="*")
    args = ap.parse_args()
    game = Game(args)
    shots = 0
    try:
        for step in args.steps:
            kind, _, value = step.partition(":")
            if kind == "wait":
                game.wait(int(value))
            elif kind == "press":
                names, _, hold = value.partition("*")
                mask = button_mask(names)
                game.command("input", kpad={"x": game.aim[0], "y": game.aim[1], "buttons": mask}, hold_frames=int(hold or 4))
                print("press %s at retrace %d" % (value, game.retraces()), flush=True)
            elif kind == "aim":
                x, y = (float(v) for v in value.split(","))
                game.aim = (x, y)
                game.command("input", kpad={"x": x, "y": y, "buttons": 0})
            elif kind == "shot":
                path = Path(game.command("capture", timeout=60.0)["path"])
                shots += 1
                target = game.out / ("%02d-%s.png" % (shots, value or "frame"))
                for _ in range(100):
                    if path.exists():
                        break
                    time.sleep(0.05)
                os.replace(path, target)
                print("shot %s at retrace %d" % (target.name, game.retraces()), flush=True)
            elif kind == "peek":
                addr, size = value.split(",")
                print("peek %s: %s" % (addr, game.command("peek", addr=int(addr, 16), size=int(size))["hex"]), flush=True)
            elif kind == "hold":   # like press, but the script goes on while the buttons are down
                names, _, hold = value.partition("*")
                mask = button_mask(names)
                with game.cv:
                    ident = game.next_id
                    game.next_id += 1
                game.pipe.send_line(json.dumps(dict(id=ident, cmd="input", kpad={"x": game.aim[0], "y": game.aim[1], "buttons": mask}, hold_frames=int(hold or 60))))
                print("hold %s" % value, flush=True)
            elif kind == "draws":   # every draw of the frame: pass, call site, vertex count, texture size, screen box (0..100 percent)
                reply = game.command("draws", timeout=60.0)
                draws = reply.get("draws", [])
                print("draws: %d" % len(draws), flush=True)
                for d in draws:
                    box = d.get("bbox", [0, 0, 0, 0])
                    pct = [round((v + 1) * 50) for v in box]
                    print("  pass %s site %s vertices %s tex %s box x %d..%d y %d..%d" % (d.get("pass"), d.get("callsite"), d.get("vertices"), d.get("tex"), pct[0], pct[2], pct[1], pct[3]), flush=True)
            elif kind == "waitlog":
                text, _, limit = value.partition(",")
                found = wait_log(game, text, int(limit or 20000))
                print("waitlog %s: %s at retrace %d" % (text, "found" if found else "NOT FOUND", game.retraces()), flush=True)
            elif kind == "until":
                parts = value.split(",")
                found = wait_log(game, parts[0], int(parts[3]) if len(parts) > 3 else 20000, button_mask(parts[1]), int(parts[2]) if len(parts) > 2 else 60)
                print("until %s: %s at retrace %d" % (parts[0], "found" if found else "NOT FOUND", game.retraces()), flush=True)
            elif kind == "dump":
                for suffix, base, size in ((".mem1", 0x80000000, 24 << 20), (".mem2", 0x90000000, 64 << 20)):
                    with open(game.out / (value + suffix), "wb") as f:
                        for at in range(0, size, 262144):
                            f.write(bytes.fromhex(game.command("peek", timeout=60.0, addr=base + at, size=262144)["hex"]))
                print("dump %s at retrace %d" % (value, game.retraces()), flush=True)
            elif kind == "state":
                st = game.command("state").get("state", {})
                print("state: input frame %s retrace %s" % (st.get("n"), st.get("retraces")), flush=True)
            elif kind == "peekptr":   # peekptr:ADDR,OFFSET,SIZE reads the pointer at ADDR and prints SIZE bytes at pointer + OFFSET
                addr, offset, size = value.split(",")
                pointer = int(game.command("peek", addr=int(addr, 16), size=4)["hex"], 16)
                print("peekptr %s -> %08X+%s: %s" % (addr, pointer, offset, game.command("peek", addr=pointer + int(offset, 0), size=int(size))["hex"]), flush=True)
            else:
                raise SystemExit("unknown step %r" % step)
        game.command("quit")
    except RuntimeError as error:
        print("stopped: %s" % error, flush=True)
    try:
        game.proc.wait(timeout=30)
    except subprocess.TimeoutExpired:
        game.proc.kill()
    game.log.close()
    print("exit code %s" % game.proc.returncode, flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
