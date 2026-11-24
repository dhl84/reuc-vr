"""Bring-up driver: starts a game build with its test server, sends Wii Remote input at given game times and saves frames.

Usage:
  python -I scripts/drive.py --exe build-dc/game/uc.exe --game <unpacked disc> --out <folder> [--seconds 120] [--speed 1] STEP...

A STEP is one of:
  wait:N            wait N retraces (60 per second of game time)
  press:BUTTONS     press and release Wii Remote buttons: a list of A B plus minus, for example press:A or press:A+B;
                    press:A*30 holds the buttons for 30 input frames (default 4)
  hold:BUTTONS*N    hold the buttons for N input frames and go on with the next step at once
  aim:X,Y           move the pointer (each value from -1 to 1; 0,0 is the centre of the screen)
  wii:BUTTONS[*N]   press buttons of the Wii Remote on the host input path (A B ONE TWO PLUS MINUS HOME UP DOWN LEFT RIGHT Z C) for N
                    input frames (default 6). The steps press and aim write the sample that the game reads; this step goes through
                    the bindings and the action map, as a real remote and the VR controllers do
  wiiaim:X,Y        move the pointer on the host input path
  wiishake[:N]      swing the remote N times (default 8) on the host input path
  shot[:NAME]       save the current frame as a PNG in <folder>
  state             print the input frame counter and the retrace counter
  draws             print every draw of the current frame
  trace:NAME,N[,EVERY]   write the draws of N frames, one every EVERY retraces (default 2), to <folder>/NAME.jsonl: one line for each frame with
                    the retrace count and, for each draw, its call site, vertex count, texture and screen box (-1 to 1); the boxes of
                    the same draws over the retraces show how the camera and the models move
  peek:ADDR,SIZE    print SIZE bytes of guest memory at hexadecimal ADDR
  peekptr:ADDR,OFFSET[,OFFSET...],SIZE   read the pointer at ADDR, then print SIZE bytes at pointer + OFFSET; with more offsets,
                    each offset but the last one reads the next pointer
  probe:NAME=ADDR,OFFSET[,OFFSET...],SIZE   name a value behind a pointer as in peekptr; the bot step prints each change of a named
                    value after each of its rounds (4 bytes also as a float), for example the health and the state of a boss
  waitlog:TEXT[,N]  wait until TEXT appears in the game log after the place of the last waitlog or until step (at most N retraces, default 20000)
  until:TEXT,BUTTONS[,EVERY[,N]]   press BUTTONS every EVERY retraces (default 60) until TEXT appears in the game log (at most N retraces, default 20000)
  dump:NAME         write MEM1 (24 MB) and MEM2 (64 MB) to <folder>/NAME.mem1 and <folder>/NAME.mem2
  waitpeek:ADDR,HEX[,N]   wait until the bytes of guest memory at hexadecimal ADDR are HEX (at most N retraces, default 20000)
  spray:N[,EVERY[,BUTTONS]]   move the pointer over a grid of points of the screen, N points, and press BUTTONS (default B) at each one,
                    one point every EVERY input frames (default 8, 2 at least); shoots at whatever is on the screen when the place of the enemies is not known
  bot:N[,EVERY[,TEXT[,WEAPONS]]]   like spray:N,EVERY (default 2) with B, and after each row of 7 points one more press, in turn: A, WAVE, Z, A, A+B, WAVE, A;
                    each pass over the grid is moved a little, so that 16 passes shoot at every place in steps of 0.05 (small targets);
                    these pass most quick-time prompts of The Darkside Chronicles ("Press the A Button!", "Wave the Wii Remote!"), reload,
                    and take a choice of ways and the prompt Move (the A presses are at the right, in the middle and at the left of the screen); with TEXT the step ends
                    after the round of 35 points in which TEXT appears in the game log (searched from the last waitlog, until or bot match);
                    WEAPONS is a list of buttons with / between them, one for each row in turn, for example UP/LEFT/-/-/-/-/-: the bot
                    presses the button after the row, which changes the weapon (the +Control Pad chooses one of four weapon slots);
                    a - is a row without such a press
  watchptr:ADDR,OFFSET,SIZE,N[,TEXT...]   read the pointer at ADDR, then print the SIZE bytes at pointer + OFFSET each time they change, for
                    N retraces (4 bytes also as a float), and each new line of the game log that holds one of the TEXTs; for example the
                    health of a player of The Darkside Chronicles and the voice lines of a fight (docs/darkside-port.md)
  live:NAME         take steps from the file <folder>/NAME as another program appends lines to it, until a line holds the step "end";
                    for a screen that is not known yet: look at a frame, then send the next steps
  touch:NAME        create the empty file <folder>/NAME (a diagnostic such as UC_PACKET_DUMP=<folder>/packets.txt|go starts when its .go file appears)
  speed:N           set the speed of the game clock to N (1 to 32) from here on: reach a scene at speed 4, then measure at speed 1

The saves of the run go to <folder>/saves (the game never touches other saves). The log of the game goes to <folder>/run.log.
The protocol is docs/testing/test-server.md. The script needs no keyboard or mouse input and does not activate the window.
"""
import argparse
import collections
import json
import os
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from uctest.transport import PipeTransport  # noqa: E402

# Names follow docs/testing/test-server.md (plus 0x1000, minus 0x10). A name of the form 0x.... sends that mask as it is.
BUTTONS = {"A": 0x800, "B": 0x400, "PLUS": 0x1000, "MINUS": 0x10, "LEFT": 0x1, "RIGHT": 0x2, "DOWN": 0x4, "UP": 0x8, "Z": 0x2000, "C": 0x4000,
           "WAVE": 0x10000}   # WAVE: The Darkside Chronicles only, a swing of the Wii Remote (common/title.h kButtonWave)


# The buttons of the host input path (common/input_bindings.h, wii_button codes).
WII_BUTTONS = {"LEFT": 0, "RIGHT": 1, "DOWN": 2, "UP": 3, "PLUS": 4, "TWO": 5, "ONE": 6, "B": 7, "A": 8, "MINUS": 9, "HOME": 10, "Z": 11, "C": 12}


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


def read_chain(game, chain):
    """ADDR,OFFSET[,OFFSET...],SIZE: the pointer at ADDR, then the pointer at each offset but the last one, then SIZE bytes as hex text."""
    addr, *offsets, size = chain.split(",")
    pointer = int(game.command("peek", addr=int(addr, 16), size=4)["hex"], 16)
    for n, offset in enumerate(offsets):
        if not 0x80000000 <= pointer < 0x94000000:
            return "no object"
        at = pointer + int(offset, 0)
        if n == len(offsets) - 1:
            return game.command("peek", addr=at, size=int(size))["hex"]
        pointer = int(game.command("peek", addr=at, size=4)["hex"], 16)
    return "%08X" % pointer


def show_probes(game, probes):
    """Prints each named value (step probe) that changed since the last call."""
    for name, probe in probes.items():
        data = read_chain(game, probe[0])
        if data != probe[1]:
            probe[1] = data
            text = data + (" (float %g)" % struct.unpack(">f", bytes.fromhex(data))[0] if len(data) == 8 and data != "no object" else "")
            print("probe %s: %s at retrace %d" % (name, text, game.retraces()), flush=True)


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
        queue = collections.deque(args.steps)
        live_at = {}
        probes = {}
        while queue:
            step = queue.popleft()
            kind, _, value = step.partition(":")
            if kind == "wait":
                game.wait(int(value))
            elif kind == "press":
                names, _, hold = value.partition("*")
                mask = button_mask(names)
                game.command("input", kpad={"x": game.aim[0], "y": game.aim[1], "buttons": mask}, hold_frames=int(hold or 4))
                print("press %s at retrace %d" % (value, game.retraces()), flush=True)
            elif kind == "wii":   # the host input path: the buttons of a Wii Remote, as a real remote or the VR controllers send them
                names, _, hold = value.partition("*")
                for name in names.split("+"):
                    game.command("input", wii_button={"button": WII_BUTTONS[name.upper()], "down": True}, hold_frames=int(hold or 6))
                print("wii %s at retrace %d" % (value, game.retraces()), flush=True)
            elif kind == "wiiaim":   # the pointer of the remote on the host input path
                x, y = (float(v) for v in value.split(","))
                game.command("input", wii_pointer={"x": x, "y": y})
            elif kind == "wiishake":   # N swings of the remote, to the right and to the left in turn, one every 3 retraces
                for i in range(int(value or 8)):
                    game.command("input", wii_motion={"x": 900 if i % 2 == 0 else -900, "y": 0})
                    game.wait(3)
                print("wiishake %s at retrace %d" % (value or 8, game.retraces()), flush=True)
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
            elif kind == "trace":
                parts = value.split(",")
                count, every = int(parts[1]), int(parts[2]) if len(parts) > 2 else 2
                with open(game.out / (parts[0] + ".jsonl"), "w") as f:
                    for _ in range(count):
                        start = game.retraces()
                        draws = game.command("draws", timeout=60.0).get("draws", [])
                        rows = [[d.get("pass"), d.get("callsite"), d.get("vertices"), d.get("tex"), d.get("bbox")] for d in draws]
                        f.write(json.dumps(dict(retraces=game.retraces(), draws=rows)) + "\n")
                        while game.proc.poll() is None and game.retraces() - start < every:
                            time.sleep(0.002)
                print("trace %s: %d frames to retrace %d" % (parts[0], count, game.retraces()), flush=True)
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
            elif kind == "waitpeek":
                parts = value.split(",")
                want, limit, start, found = parts[1].upper(), int(parts[2]) if len(parts) > 2 else 20000, game.retraces(), False
                while game.proc.poll() is None and game.retraces() - start <= limit:
                    if game.command("peek", addr=int(parts[0], 16), size=len(want) // 2)["hex"] == want:
                        found = True
                        break
                print("waitpeek %s %s: %s at retrace %d" % (parts[0], want, "found" if found else "NOT FOUND", game.retraces()), flush=True)
            elif kind == "spray":   # the inputs carry their input frame, so the rate does not depend on the speed of the run
                parts = value.split(",")
                count, every, mask = int(parts[0]), max(2, int(parts[1]) if len(parts) > 1 else 8), button_mask(parts[2] if len(parts) > 2 else "B")
                points = [(x / 10.0, y / 10.0) for y in (-1, 1, -3, 3, 0) for x in (0, -2, 2, -4, 4, -6, 6)]
                done = 0
                while game.proc.poll() is None and done < count:
                    frame = game.command("state").get("state", {}).get("n", 0) + 4
                    batch = min(40, count - done)   # two commands for each point; the server keeps at most 256 open
                    for i in range(batch):
                        game.aim = points[(done + i) % len(points)]
                        last = i == batch - 1
                        # the press at the new place for one frame, then the release
                        for at, buttons in ((frame + i * every, mask), (frame + i * every + 1, 0)):
                            fields = dict(cmd="input", at_frame=at, kpad={"x": game.aim[0], "y": game.aim[1], "buttons": buttons})
                            if last and not buttons:
                                game.command(timeout=120.0, **fields)   # waits until the game has taken the last release
                            else:
                                with game.cv:
                                    ident = game.next_id
                                    game.next_id += 1
                                game.pipe.send_line(json.dumps(dict(id=ident, **fields)))
                    done += batch
                print("spray %s: %d points to retrace %d" % (value, done, game.retraces()), flush=True)
            elif kind == "watchptr":   # watchptr:ADDR,OFFSET,SIZE,N: prints each change of the bytes at (pointer at ADDR) + OFFSET for N retraces
                addr, offset, size, limit, *texts = value.split(",")
                start, last = game.retraces(), None
                game.log.flush()
                log_at = (game.out / "run.log").stat().st_size
                while game.proc.poll() is None:
                    now = game.retraces()
                    if now - start > int(limit):
                        break
                    if texts:   # the lines of the game log with one of the texts, with the retrace count at which they were seen
                        game.log.flush()
                        with open(game.out / "run.log", "rb") as f:
                            f.seek(log_at)
                            data = f.read()
                        data = data[:data.rfind(b"\n") + 1]
                        log_at += len(data)
                        for line in data.decode("latin-1").splitlines():
                            if any(t in line for t in texts):
                                print("watchptr log at retrace %d (%d after the start of the step): %s" % (now, now - start, line.strip()[:160]), flush=True)
                    pointer = int(game.command("peek", addr=int(addr, 16), size=4)["hex"], 16)
                    data = game.command("peek", addr=pointer + int(offset, 0), size=int(size))["hex"] if 0x80000000 <= pointer < 0x94000000 else "no object"
                    if data != last:
                        text = data
                        if len(data) == 8:
                            text += " (float %g)" % struct.unpack(">f", bytes.fromhex(data))[0]
                        print("watchptr %s+%s: %s between retrace %d and %d (%d after the start of the step)" % (addr, offset, text, now, game.retraces(), now - start), flush=True)
                        last = data
                    time.sleep(0.004)
            elif kind == "bot":   # the spray, and a press of A, the wave, Z or A+B after each row of 7 points (quick-time prompts, the reload)
                parts = value.split(",")
                count, every = int(parts[0]), max(2, int(parts[1]) if len(parts) > 1 else 2)
                points = [(x / 10.0, y / 10.0) for y in (-1, 1, -3, 3, 0) for x in (0, -2, 2, -4, 4, -6, 6)]
                extras = [("A", 3, (0.47, -0.03)), ("WAVE", 12, None), ("Z", 3, None), ("A", 3, (0.01, 0.01)), ("A+B", 3, None), ("WAVE", 12, None), ("A", 3, (-0.47, -0.03))]
                stop, found, done = parts[2] if len(parts) > 2 and parts[2] else None, False, 0
                weapons = parts[3].split("/") if len(parts) > 3 and parts[3] else []
                wide = getattr(game, "wide", (1.0, 1.0))   # the step "wide"
                while game.proc.poll() is None and done < count:
                    if stop and log_has(game, stop):
                        found = True
                        break
                    at = game.command("state").get("state", {}).get("n", 0) + 4
                    events = []   # (input frame, x, y, buttons)
                    batch = min(35, count - done)   # the server keeps at most 256 open commands
                    for i in range(batch):
                        # each pass over the 35 points moves the grid by a part of its pitch: 16 passes cover the screen in steps of 0.05
                        shift = (0.0, 0.1, 0.05, -0.05)
                        turn = (done + i) // len(points)
                        x, y = points[(done + i) % len(points)]
                        game.aim = (round((x + shift[turn % 4]) / wide[0], 3), round((y + shift[(turn // 4) % 4]) / wide[1], 3))
                        events += [(at, game.aim[0], game.aim[1], BUTTONS["B"]), (at + 1, game.aim[0], game.aim[1], 0)]
                        at += every
                        if (done + i) % 7 == 6:
                            names, hold, place = extras[((done + i) // 7) % len(extras)]
                            x, y = (place[0] / wide[0], place[1] / wide[1]) if place else game.aim
                            events += [(at, x, y, button_mask(names)), (at + hold, x, y, 0)]
                            at += hold + 2
                        if weapons and (done + i) % 7 == 6 and weapons[((done + i) // 7) % len(weapons)] != "-":   # a weapon slot
                            events += [(at, game.aim[0], game.aim[1], button_mask(weapons[((done + i) // 7) % len(weapons)])), (at + 3, game.aim[0], game.aim[1], 0)]
                            at += 5
                    for n, (frame, x, y, buttons) in enumerate(events):
                        fields = dict(cmd="input", at_frame=frame, kpad={"x": x, "y": y, "buttons": buttons})
                        if n == len(events) - 1:
                            game.command(timeout=120.0, **fields)   # waits until the game has taken the last release
                        else:
                            with game.cv:
                                ident = game.next_id
                                game.next_id += 1
                            game.pipe.send_line(json.dumps(dict(id=ident, **fields)))
                    done += batch
                    if probes:
                        show_probes(game, probes)
                print("bot %s: %d points to retrace %d%s" % (value, done, game.retraces(), "" if not stop else ", text found" if found else ", text NOT FOUND"), flush=True)
            elif kind == "wide":   # wide:X,Y: the bot divides its places by these factors (the wide view of the headset: 1.8,2.7)
                game.wide = tuple(float(v) for v in value.split(","))
                print("wide %s" % value, flush=True)
            elif kind == "speed":
                game.command("speed", factor=int(value))
                print("speed %s at retrace %d" % (value, game.retraces()), flush=True)
            elif kind == "touch":
                (game.out / value).touch()
                print("touch %s at retrace %d" % (value, game.retraces()), flush=True)
            elif kind == "state":
                st = game.command("state").get("state", {})
                print("state: input frame %s retrace %s" % (st.get("n"), st.get("retraces")), flush=True)
            elif kind == "peekptr":   # peekptr:ADDR,OFFSET[,OFFSET...],SIZE follows the pointers and prints SIZE bytes behind the last one
                print("peekptr %s: %s" % (value, read_chain(game, value)), flush=True)
            elif kind == "probe":
                name, _, chain = value.partition("=")
                probes[name] = [chain, None]
            elif kind == "live":   # steps from a file that another program appends to; the step ends with the line "end"
                path = game.out / value
                while game.proc.poll() is None:
                    text = path.read_text() if path.exists() else ""
                    new = text[live_at.get(value, 0):]
                    cut = new.rfind(chr(10)) + 1   # whole lines only
                    tokens = new[:cut].split()
                    if tokens:
                        live_at[value] = live_at.get(value, 0) + cut
                        if "end" in tokens:
                            tokens = tokens[:tokens.index("end")]
                        else:
                            tokens.append(step)
                        queue.extendleft(reversed(tokens))
                        break
                    time.sleep(0.1)
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
