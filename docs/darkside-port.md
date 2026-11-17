# The Darkside Chronicles (SBDE08) bring-up

This page describes the work to run Resident Evil: The Darkside Chronicles (disc ID `SBDE08`, North American release) on this code base. The code base is a port of Resident Evil: The Umbrella Chronicles (`RBUE08`). The two games come from the same developer and share much of their engine and middleware.

The work is not finished. The section "State" says what runs.

## Credits

This work builds on [solleo95/reuc-recomp](https://github.com/solleo95/reuc-recomp) (the PC port) and [lialan/reuc-vr](https://github.com/lialan/reuc-vr) (the macOS and Quest 3 port). The licence is the same: GPL-3.0. The repository contains no game code and no game data. You need your own disc image.

## Method

The port replaces Wii SDK functions with native code. The native code names functions and data of `RBUE08` by address. A second game needs the same functions and data at its own addresses. The tools below find them.

1. `tools/dolmatch.py` compares the two `main.dol` files. It finds functions that are the same code after it masks relocations. Then it follows calls from matched functions, and it fills short runs between matched neighbours. It also records pairs of small-data displacements and data addresses that appear at the same place of same-code functions.
2. `tools/sigmatch` names SDK functions with the signature database of Dolphin (`totaldb.dsy`).
3. `tools/carry_project.py` writes `project/dc` from the reference project and the two outputs above. `project/dc/hle_status.txt` lists each replaced function as on or off, with the reason.
4. `tools/title_map.py` writes `common/title_map_dc.inc`. Native code names an `RBUE08` address as `UC_ADDR(0x...)` and a displacement as `UC_SDA13(n)` or `UC_SDA2(n)` (`common/title.h`). An `RBUE08` build gets the value itself. An `SBDE08` build gets the value of that game at compile time. A value that the table does not have stops the compile.
5. `tools/find_entries.py` and `tools/find_data_entries.py` find function entries that the analyzer does not see. `project/dc/dc.cfg` holds them.

Values that a person found by hand are in `project/dc/hle_manual.txt`, `project/dc/title_manual.txt` and `project/dc/symbols_manual.txt`, each with its evidence.

Natives that hold `RBUE08` game logic stay off (`project/dc/hle_exclude.txt`). Examples are the 60 fps pacing hooks, the generated leaf functions and the PC control hooks.

## Build (Windows)

1. Build the `RBUE08` port first as [building.md](building.md) describes. This step also builds the translator and the tools.
2. Unpack your own `SBDE08` image with Wit into a folder, so that `<folder>\game\DATA\sys\main.dol` exists.
3. Set `DC_INSTALL_DIR` to that folder, in `local.env` or in the shell.
4. Run `scripts\bringup-dc.bat`. The script translates the game into `generated-dc\`, builds `build-dc\`, and runs the game for 20 seconds.

Never publish `generated-dc\`. It is derived from the game's code.

To make the matcher outputs again, run these commands from the repository root:

```
python -I tools/dolmatch.py --ref <RBUE08 main.dol> --new <SBDE08 main.dol> --project project --out <match dir>
build\tools\sigmatch\sigmatch.exe --dol <SBDE08 main.dol> --dsy third_party\signatures\totaldb.dsy --required project\hle_required.txt --min-words 3 --out <sigmatch dir>
python -I tools/carry_project.py --project project --match <match dir> --sigmatch <sigmatch dir>\names.txt --out project/dc
python -I tools/title_map.py --match <match dir> --manual project/dc/title_manual.txt --out common/title_map_dc.inc runtime game renderer modding xr common
```

## Bring-up aids

- `scripts/drive.py` starts a build with its test server, sends Wii Remote input at given game times, saves frames and reads guest memory. It needs no keyboard and no mouse. The retrace count of a screen is not the same in every run. The steps `waitlog`, `until` and `waitpeek` thus wait for a text in the game log or for a value in guest memory. The step `dump` writes MEM1 and MEM2 to files. The step `spray` moves the pointer over a grid of points and shoots at each one. The step `touch` makes a file, which starts a diagnostic that waits for one.
- `UC_PACKET_DUMP=<file>|go|<frames>` lists every render packet of some frames: its draws with call site, blend and textures, and its EFB copy. When `<file>.go` appears, the list starts (`touch:packets.txt.go` in `scripts/drive.py`). `UC_PACKET_DUMP_TEV=1` adds the TEV stages, the colour registers and constants, the channel controls and the lights of each draw. It also adds the texgen of each texture coordinate that a stage uses: the control word and the two matrices. A draw with 32 vertices at most gets a list of its vertices.
- `UC_CAPTURE_PASS_GO=<file>` saves the frame buffer after each render packet of one frame as `pass-*.png` in the capture folder. It starts with the first packet after a present once the file exists (`touch:pass.go`). The renderer draws the 3D part of a frame into its scene targets, not into the frame buffer. For a packet of that part the file shows the scene target. A packet that clears after its copy shows the cleared picture.
- `UC_TRACE_NEW=<retrace>` (trace build) prints each guest function the first time it runs after that retrace count.
- `UC_DUMP_THREADS=1` prints the state, the wait key and the registers of each guest thread once per second.
- `UC_CAPTURE_RETRACES=300,900` saves the frame at those retrace counts as PNG files (`--capture-dir <dir>`).
- `--log-dvd` prints each disc file that the game opens and reads.
- The step `speed:N` of `scripts/drive.py` sets the speed of the game clock. Reach a scene at speed 4, then measure at speed 1.
- `UC_CPU_SAMPLE=all` samples every thread of the process into `cpu-samples-all.txt` in the working folder of the game (`game/cpu_sampler.cpp`). `UC_CPU_SAMPLE_FLAG=<file>` holds the start back until that file exists (`touch:` step). A line is `T<thread id> rva rva ... = count`. The line `T0 0 = N` gives the number of sampling rounds. Resolve the addresses with `uc.map` of the build. `UC_CPU_SAMPLE=guest` does not start in this game: it starts in the frame wait native of `RBUE08`.
- The log line `[sched-cycles]` gives the CPU work of each guest thread in a second, in millions of cycles. The line `[sched] ... run ms` also counts the time that a thread waits while it holds the baton.
- `UC_TEXTURE_MISS_LOG=<file>` writes one line for each miss of the texture cache: address, size, format, and the number of cache entries with that address and with that key.
- `UC_WRITE_TRAP=<report>|<addresses>` (`game/write_trap.h`) works in this game too. It arms at the first retrace after `<report>.go` appears.
- `--game-arg=--fps-limit --game-arg=60` (options of `scripts/drive.py`) runs a chapter with the 60 Hz step (see "The 60 Hz mode"). Without it the Windows build runs the game's own 30 Hz step. The log line `[game-pacing]` gives the frame waits, the retraces and the game time since the start.
- The log line `[perf-dc]` gives three numbers for each second. They are the small textures that took a GPU image of the pool, the read-backs of the exposure copy and their host time.
- `UC_SMALL_TEXTURES=0` turns the handling of small rewritten textures off (see "The render thread"): the texture cache keeps every version, and the renderer makes a GPU image for each. Use it to compare.
- The step `trace:NAME,N[,EVERY]` of `scripts/drive.py` writes the screen box of every draw of N frames to a file. The boxes of the same draws over the retraces show how the camera and the models move. Two runs can be compared this way (the pictures of two runs are never taken at the same game time).
- `UC_DC_GOD=1` keeps the health of the two players and of the partner full (see "Play-through"). It is a test aid and is off by default.
- `UC_DC_WEAK=1` holds the health of every enemy at 1 or below. `UC_DC_ROCKET=1` gives the first player the rocket launcher and the linear launcher. Both are test aids and are off by default (see "The rest of the game").
- The step `bot:N[,EVERY[,TEXT]]` of `scripts/drive.py` is the spray with one more press after each row of points: A at the right of the screen, the wave, Z, A in the middle, A+B, the wave, A at the left. It passes the quick-time prompts, reloads, takes a choice of ways and the prompt "Move". With TEXT, the step ends after the game log holds that text. An example is `chapterResult_us.arc`, which the game opens for the result screen of a chapter.
- The step `live:NAME` takes its steps from a file while the game runs. Another program appends lines to the file. Use it for a screen that is not known yet: look at a frame, then send the next steps.
- The step `watchptr:ADDR,OFFSET,SIZE,N[,TEXT...]` reads a pointer and watches the value behind it. It prints each change of the value with the retrace count. It also prints each new log line that holds one of the texts.
- `UC_AUDIO_DUMP=<prefix>` writes every block of mixed sound to `<prefix>.<pid>`. `scripts/audio_check.py` turns the file into a WAV file. It prints the level, the clipping and the spectrum of a WAV file. It also finds a decoded stream file of the game in a WAV file (see "Sound").

The game ignores A, B and Z as "OK" while the pointer is exactly at the centre of the screen (0, 0). Move the pointer first, for example `aim:-0.2,0.1`.

A menu button takes A only while the pointer is on it. The pointer of `aim:X,Y` is at about (320 + 400 X, 180 + 295 Y) of a 640x360 frame.

These steps go from the start of the game to the first enemy of the first chapter (15 November 2026):

```
aim:-0.2,0.1 until:savebanner_us.arc,A,60 until:menu01.brstm,A,60 wait:400 press:A wait:500
aim:-0.58,-0.18 wait:60 press:A wait:300 aim:-0.31,-0.15 wait:60 press:A wait:300 aim:0,-0.1 wait:60 press:A wait:400 press:A
until:MV_301_05.sfd,A,120,2000 wait:200 press:A wait:500 aim:-0.27,-0.2 wait:60 press:A wait:7600
aim:0.05,0.0 wait:300 press:A wait:250 until:NEVERMATCH,B,10,300
```

The first line creates the save and passes the intro movie and the title screen. The second line takes New Game, File 01, Normal and Chronicles. The third line passes the chapter's first movie and takes Leon. The last line waits for the second movie and the loading, then shoots (B) at the first enemy.

The fixed waits of the third and the last line do not fit every run: in one run the A press on Leon did not take, and the chapter's movie started 7900 retraces later, with the next A press. These steps wait for the movie and for the end of the loading instead: `aim:-0.27,-0.2 wait:60 until:MV_301_10.sfd,A,120,3000 waitlog:EZ0S_r302.arc,12000`. When the walk into the village starts, the game opens `EZ0S_r302.arc`. That is about 6600 retraces after the movie file. The first voice line of the walk (`VO_3015000.brstm`) follows about 300 retraces later. `waitlog` on it gives two runs the same game time within a few retraces.

To play on without the places of the enemies, repeat these steps: `spray:220,2 aim:0.47,-0.03 wait:6 press:A wait:20 shot:NAME`. `spray` shoots 220 times over a grid of points, one shot every 2 input frames. When the game shows a choice of ways, the A press at that place takes "Go Right". In a run without that press the choice waited for more than 25000 retraces. On 15 November 2026 a run of 90 such rounds reached the boss of the chapter (146000 retraces, `--seconds 1200 --speed 4`). Its way was the streets, the market, the spiders, the hut of the wounded man, the choice of ways and the flooded village. The player died at the boss several times. Each time the game showed "You are Dead" and the screen "Loading data", then went on from its last checkpoint. No run stopped with an error.

## Play-through

On 15 November 2026 the Windows build was played from the first chapter on. The runs used the test aid `UC_DC_GOD=1`, the step `bot` of `scripts/drive.py`, and speed 4 or speed 6.

### The health and the test aid

- The health of a player is the float at `+0x974` of the player object. 50 is full and 0 is dead. This is the "LEON" bar of the HUD.
- The setter 800AC7EC holds the value between 0 and 50. A hit subtracts its damage (800ACA74). A herb adds to the value (800AC86C).
- The player manager is the object at 80736378. Its getter is 80046EFC. It keeps one handle for each of the two players at `+0xAC`. A handle is an id and an index into the object table at 80732B88. An entry of the table is an id and a pointer. The function 80046DC8 finds a player this way.
- The partner is not a player object. This is the "KRAUSER" or "CLAIRE" bar. The object at 80735500 keeps four health values at `+0xF8` and their maxima at `+0xE8`. Its getter is 80049CC8. The HUD reads the value at 800B17C0.
- `UC_DC_GOD=1` writes the full value to the two players and to the partner at every retrace (`game/uc_main.cpp`, constants in `common/title.h`). A hit takes less than the full health, so one write for each retrace is sufficient. The game still plays its hit reactions.
- How it was found: the layout names `lifebar1p` and `gauge_1p` lead to the HUD code. The HUD update 800E9604 gets the float `+0x974` of the player at 800AD174. Two memory dumps, one before the damage and one after the death, showed 50 and 0 there.

Checked with frames: the "LEON" bar stays full while zombies hold the player and during the boss fight. The "KRAUSER" bar stays full for 16 rounds of the `bot` step in the first chapter. In a run without the partner's part of the aid it was at a quarter after the same number of rounds. Without the aid the same fight takes the health from 50 to 0 in steps of 5 (see "The 60 Hz mode").

### The steps from one chapter to the next

The spray moves the pointer and presses in the same frame. In a menu such a press takes a button only by chance. These steps operate the menus:

- After the last scene of a chapter the game opens `chapterResult_us.arc` and shows the screen "RESULTS". The button NEXT is at `aim:0.59,0.237`. More screens follow, one for each new weapon and archive item ("You got Sub Machinegun."). `aim:0.59,0.237 until:NEVERMATCH,A,90,1800` presses NEXT on all of them.
- Then a menu shows CHRONICLES, ARCHIVES, CUSTOMIZE and MAIN MENU. `aim:0,-0.29 wait:60 press:A` takes CHRONICLES. At this press the game writes the save file. Before it, the save file holds only the last checkpoint of the chapter.
- The next screen shows the weapons of the two characters. `aim:0.59,0.237 wait:60 press:A` takes NEXT, and the next chapter loads. A new scenario shows the character screen first: `aim:0.25,0.0 wait:60 press:A` takes the character at the right.
- One chapter in a loop: `speed:4 wait:1500 press:A wait:600 speed:6 bot:20000,2,chapterResult_us.arc`, then the three steps above.

The next steps go from the start of the game to a saved chapter. They need a save folder that has a file: copy the folder `saves` of an earlier run into the output folder of the new run.

```
aim:-0.2,0.1 until:menu01.brstm,A,60,6000 wait:300 speed:1 press:A wait:240
aim:-0.16,-0.15 wait:40 press:A wait:240 aim:-0.21,0.26 wait:40 press:A wait:300 aim:0,-0.12 wait:40 press:A wait:300
aim:-0.1,0.05 wait:60 press:A wait:300 aim:0.06,0.22 wait:120 press:A wait:240 aim:-0.16,0.115 wait:60 press:A wait:900
```

The first line goes to the profile screen. The second line takes FILE 01, YES and CHRONICLES. The third line takes the left scenario of the stage screen and the chapter piece at the bottom. Then it answers YES to "Do you want to resume the game from this checkpoint?". The chapter piece is at a different place for each chapter.

### What ran

| Chapter | Result |
|---|---|
| Operation Javier, Chapter 1 | Played from its start to the screen "RESULTS": the village, the flooded part and the boss in the church. Clear time 00:21:43, 42 enemies. |
| Memories of a Lost City, Chapter 1 | Played to "RESULTS": the streets of the city and the gun shop. |
| Memories of a Lost City, Chapter 2 | Played to "RESULTS": the police station. Clear time 00:09:45, 25 enemies. |
| Memories of a Lost City, Chapter 3 | Played to "RESULTS". Clear time 00:15:43, 34 enemies. |
| Memories of a Lost City, Chapter 4 | Played to "RESULTS" in the second run: the sewer with the spiders. The first run went back to the menus (see below). |
| Memories of a Lost City, Chapter 5 | Played to "RESULTS". Clear time 00:23:35, 92 enemies. |
| Memories of a Lost City, Chapter 6 | Played to "RESULTS" after the change for the point draw (see below). Before the change the run stopped 170 s into the chapter. |
| Memories of a Lost City, Chapter 7 | Played to "RESULTS". Clear time 00:32:53, 84 enemies. |
| Memories of a Lost City, Chapter 8 | Played for 36 minutes of game time, to the fight with the boss on the train. The run ended there at its time limit of 440 s, without an error. |

The clear time is the time of the game clock. A chapter of 15 minutes takes about 3 minutes at speed 6.

After each chapter the game showed its result screens and the screens for new weapons and archive items. It wrote the save file, and the next run loaded it: the profile screen showed the picture of the file, and the game asked "Do you want to resume the game from this checkpoint?". The chapter screen showed one piece for each open chapter.

One run stopped with an error: the point draw in the sixth chapter of "Memories of a Lost City". The log of each run was searched for the words "unsupported", "invalid", "exception", "fatal" and "assert".

### Faults found and fixed

- The piece under the pointer on the chapter screen and on the character screen was a dark square. The logo of the title screen was black. The cause was the native `GXSetBlendMode`. The SDK function writes the subtract bit of the blend word at every call: it sets the bit for `GX_BM_SUBTRACT` and clears it for every other mode (803A7F98). The native kept the bit after it was set. After the first subtract draw, every draw that set its blend through this function was subtracted from the picture. The glow of the piece is an additive quad with an indirect texture (call site 804A0A20). The effect sprites (call site 8049CD48) use the same path. The bits that the native replaces are now a constant of each game (`kBlendModeBits` in `common/title.h`). `RBUE08` has the same SDK code, but it keeps its old value until one of its scenes with a subtract draw is checked.
- The effect library draws one kind of particle as `GX_POINTS`, one point for each `GXBegin` (call site 804A1D10, in a display list). The native renderers have no point primitive. The draw stopped the game with "unsupported native draw topology" in the sixth chapter of "Memories of a Lost City". The draw was dropped at first. Now each point becomes a square of two triangles (see "Point draws").
- The test aid did not hold the partner's health at first. The partner's bar went down in the first chapter. In the fourth chapter of "Memories of a Lost City" one run opened `menu02.brstm` and went back to the menus. When the player is dead, the game opens this file: the three runs without the aid opened it after the health was 0, and no other run opened it. The players had the aid in that run, so the death of the partner is the probable cause. No frame shows it. The aid writes the partner's health now. Two more runs of that chapter reached "RESULTS", the first of them still without the write. The step `bot:N,2,menu02.brstm` ends at a game over.

### Not faults

- The log line `[save] call from 803C8934 failed: -105` comes at each save. `ISFS_CreateFile` answers "the file exists", and the game then writes the file. The save file loads in the next run.
- With the game clock at speed 6 a chapter presents 75 to 235 frames for 360 retraces. At speed 1 the second chapter holds 30 frames for 60 retraces with 2.5 to 3 ms of render time for a frame.

### The rest of the game

On 16 November 2026 the Windows build was played on from the last boss of "Memories of a Lost City" (script `s10c8`). The first run started from a save of the headset at the checkpoint of that boss. The scripts of "Memories of a Lost City" are `s10c1` to `s10c8`. Those of "Game of Oblivion" are `s20c1` to `s20c7`. Those of "Operation Javier" are `s30c1` to `s30c7`.

#### What the boss of `s10c8` needs

The boss is the Tyrant on the platform of the train (class with the vtable 806E4398). The `bot` step took its health down again and again and never won.

- The Tyrant has 16 states (`+0x5D0` of its object). Its function after a hit is 801D34EC. In the states up to 13 the function holds the health at 10 % of the largest health. In state 14 the health goes to 0 once, and the Tyrant gets half of its health back.
- State 15 is the last one. When the health is 0 there, the Tyrant kneels and a voice line says "Leon, now!". For 2 seconds the byte `+0x685` is 1. A hit of the rocket launcher in that time kills the Tyrant (801D352C). After that time the Tyrant gets a quarter of its health back.
- Outside that time the Tyrant takes no damage from a rocket in state 15 (801D35BC). It knocks the rocket away (801DA994).
- Before state 15 Ada throws a rocket launcher ("Use this!"). It lies on the floor for about 2 seconds with the prompt "Pick Up". The player takes it with A while the pointer is on it. The `bot` step pressed A in the middle of the screen only once in 49 shots.
- A weapon that the player picks up gets the first free one of the four weapon slots (800CDF98). The save has four full slots: the handgun, the submachine gun, the hand grenade and the bow gun. The launcher then gets no slot, and the +Control Pad cannot choose it.

The kind of a hit is the weapon number of the item table at 806D7038: 0 is the handgun, 3 the magnum, 5 the rocket launcher and 7 the linear launcher. An enemy keeps the kind of its last hit at `+0x50C`.

Checked: the states, the health and the byte `+0x685` were read from the running game with the step `probe`. The inventory was read before the pick-up. One run pressed A at the launcher every 5 retraces, and the four slots did not change. The inventory after that pick-up was not read.

#### The test aids

- `UC_DC_WEAK=1` holds the health of every enemy at 1 or below, so one hit takes it to 0. An enemy is an object whose class comes from the enemy base class: its vtable has the function 8005D408 at `+0x10`. The two partner classes are left out. The game still runs the phases of a boss. With this aid the Tyrant reaches its state 15 after 21000 retraces. Without it the state came after 36000 retraces.
- `UC_DC_ROCKET=1` gives the first player two weapons while a chapter runs: the rocket launcher in the left weapon slot and the linear launcher in the lower slot. The weapons that were in those slots have no slot then. The game itself gives the rocket launcher at the result screen of `s10c8`.
- Both aids are off by default. The constants are in `common/title.h`, and the code is in `game/uc_main.cpp`. The other game does not have them.
- The fourth value of the `bot` step is a list of buttons, one for each row of 7 shots, for example `UP/LEFT/DOWN/-/-/-/-`. The bot presses the button after the row. Up is the handgun, Left the rocket launcher and Down the linear launcher.
- Each pass of the `bot` step over its grid is moved by a part of the grid pitch. After 16 passes the bot has shot at every place in steps of 0.05.
- The step `probe:NAME=ADDR,OFFSET,...,SIZE` names a value behind a chain of pointers. The `bot` step prints each change of it. The table of objects is at 80732B88. The Tyrant was entry 68, so `probe:hp=80732DAC,0x44C,0,4` is its health.

This step wins the fight of `s10c8`: `bot:30000,2,chapterResult_us.arc,UP/LEFT/-/-/-/-/-` with `UC_DC_GOD=1 UC_DC_WEAK=1 UC_DC_ROCKET=1`. The handgun takes the health to 0, and a rocket hits in one of the next 2-second times. In the run with probes the Tyrant died 36000 retraces after the start of the run. A second run reached the result screen of the chapter.

Three later places needed more than the health:

- The puppet theatre of `s20c4` is one object of an enemy class (vtable 806E0800) with six puppets. When a shot kills the boy or the girl, the show starts again. A player shoots the knight, the bear and the two fish in the short times in which the show lets a shot hit them. With `UC_DC_WEAK=1` the boy and the girl keep their health, and the four other puppets have none.
- The Tyrant in the plane of `s20c5` takes no damage in its last part. A rocket starts the scene with the missile (voice line "At this range!?"). The bot passed it with `UP/LEFT/-/-/-/-/-` in 140 shots. With the handgun alone it did not pass in 4 minutes.
- The last boss of `s20c7` needs the linear launcher ("Use the Launcher!"). With the handgun and the rocket launcher the run stood there for 4 minutes. With `UP/LEFT/DOWN/-/-/-/-` the chapter ended.

#### What ran

The runs used `UC_DC_GOD=1 UC_DC_WEAK=1 UC_DC_ROCKET=1`, speed 4 to 6 and the `bot` step. The table names the runs without `UC_DC_WEAK`.

| Script | Chapter | Result |
|---|---|---|
| `s10c8` | Memories of a Lost City, Chapter 8 | "RESULTS" from the checkpoint at the Tyrant: the Tyrant, the movie, the train with the last boss. Clear time 00:14:15. The game gave the rocket launcher. |
| `s30c2` | Operation Javier, Chapter 2 | "RESULTS". Clear time 00:11:47, 73 enemies. |
| `s20c1` | Game of Oblivion, Chapter 1 | "RESULTS". Clear time 00:11:16, 320 enemies. |
| `s20c2` | Game of Oblivion, Chapter 2 | "RESULTS". Clear time 00:09:19, 88 enemies. |
| `s20c3` | Game of Oblivion, Chapter 3 | "RESULTS" after the change for the EFB copy (see below). Clear time 00:22:45, 63 enemies. |
| `s20c4` | Game of Oblivion, Chapter 4 | "RESULTS" after the puppet rule and the change for the palette. The chapter was played in parts from its checkpoints. Clear time 00:17:55. |
| `s20c5` | Game of Oblivion, Chapter 5 | "RESULTS". The last part was played again from its checkpoint with the rocket launcher. Clear time 00:41:15. |
| `s20c6` | Game of Oblivion, Chapter 6 | "RESULTS". Clear time 00:33:35, 65 enemies. |
| `s20c7` | Game of Oblivion, Chapter 7 | "RESULTS". The last part was played again from its checkpoint with the linear launcher. Clear time 00:18:28, 409 enemies. |
| `s30c3` | Operation Javier, Chapter 3 | "RESULTS". Clear time 00:12:42, 195 enemies. |
| `s30c4` | Operation Javier, Chapter 4 | "RESULTS". The boss in the dam was played again from its checkpoint without `UC_DC_WEAK`. Clear time 00:16:15, 79 enemies. |
| `s30c5` | Operation Javier, Chapter 5 | "RESULTS" without `UC_DC_WEAK`. Clear time 00:25:38, 2 enemies. |
| `s00c1` | A chapter with Claire alone, which the game starts after `s30c5` | "RESULTS" without `UC_DC_WEAK`. Clear time 00:13:29, 79 enemies. After its result screens the game opened `s00c1` again. |

After each chapter the game showed its result screens and the next weapon screen, and it wrote the save file. The log of each run was searched for "guest trap", "fatal", "unsupported", "exception" and "assert".

#### Faults found and fixed

- The last boss of `s10c8` calls five functions of one instruction through pointers (8022C0C0 and 8022C9E0 to 8022C9F0). Its constructor builds their addresses through its stack frame, so the analyzer did not find them. The game stopped with "indirect call or jump to an address that is not a known function entry". The dispatcher now runs such a function: `blr` returns, and a branch goes to its target (`runtime/dispatch.cpp`). The log names each address once. `tools/find_entries.py` now follows a `lis` value through the stack frame and lists the six entries. They are not in `project/dc/dc.cfg` yet: a new entry makes a new translation of the whole game.
- In room R224 of `s20c3` the game binds a texture of 320x171 at an address where an EFB copy of 80x42 was before. The renderer stopped with "unsupported native EFB texture view". A texture that does not fit the older copy now comes from the guest's memory (`runtime/hle/gx.cpp`).
- In room R213 of `s20c4` the game binds a `GX_TF_C8` texture of 64x64 with a texture object from `GXInitTexObj`, which has no palette. The decoder stopped with "GX palette index exceeds loaded palette". Such a texture now gets the palette that its texture map had before, as on the hardware (`kPaletteLenient` in `common/title.h`). `RBUE08` keeps its errors.

#### The menus

A save after a finished chapter is in the scratch folder of the run (`save_<script>_done`). These steps go from the title to the main menu of file 01: `aim:-0.2,0.1 until:menu01.brstm,A,60,6000 wait:300 press:A wait:240 aim:-0.16,-0.15 wait:40 press:A wait:240 aim:-0.21,0.26 wait:40 press:A wait:300`. Then `aim:0,-0.12 wait:40 press:A wait:300` takes CHRONICLES.

- The stage screen has three pieces after `s30c2`: "Memories of a Lost City" at `aim:-0.1,0.05`, "Game of Oblivion" at `aim:0.14,-0.32` and "Operation Javier" at `aim:0.36,0.108`.
- When a new chapter opens, the pieces of the chapter screen move. Hold the pointer on a piece for 60 retraces: the screen then shows "Chapter N".
- A new chapter of a scenario that was played before goes straight to the weapon screen. NEXT is at `aim:0.59,0.237`. A chapter with checkpoint data asks "Do you want to resume the game from this checkpoint?", and YES is at `aim:-0.16,0.115`. The game then goes on without the weapon screen.
- A piece of another chapter asks to delete the checkpoint data of the chapter in progress. YES at the same place deletes it.
- The step after a chapter is the same for every chapter: NEXT on the result screens (`aim:0.59,0.237 until:NEVERMATCH,A,90,1500`), CHRONICLES (`aim:0,-0.29 wait:60 press:A wait:500`), NEXT on the weapon screen, then `until:Data/script/s,A,120,3000`. A new scenario shows the character screen first: `aim:-0.25,0.0 wait:60 press:A wait:400` takes the character at the left.

These routes were used. Each one follows CHRONICLES at the main menu. "Step after" is the step after a chapter from the list above.

| Script | Start | Steps |
|---|---|---|
| `s10c8` | The save of the headset at the boss | `aim:-0.1,0.05` (scenario), `aim:-0.151,0.336` (chapter), YES |
| `s30c2` | Step after `s10c8` | NEXT on the weapon screen. From `save_s10c8_done` the pieces are at `aim:0.36,0.108` and `aim:-0.06,-0.27` (read from a frame, not pressed). |
| `s20c1` | Step after `s30c2` | The character at the left, then NEXT. From `save_s30c2_done` the pieces are at `aim:0.14,-0.32` and `aim:0.05,0.0` (read from a frame, not pressed). |
| `s20c2` | `save_s20c1_done` | `aim:0.14,-0.32`, `aim:-0.21,0.2`, NEXT |
| `s20c3` | `save_s20c2_done` | `aim:0.14,-0.32`, `aim:-0.45,-0.19`, NEXT |
| `s20c4` | `save_s20c4_cp` (four pieces) | `aim:0.14,-0.32`, `aim:0.1,-0.3`, NEXT |
| `s20c5` | `save_s20c5_cp` (five pieces, checkpoint) | `aim:0.14,-0.32`, `aim:-0.26,-0.34`, YES |
| `s20c6` | Step after `s20c5` | NEXT. With seven pieces the chapter is at `aim:-0.11,-0.42`. |
| `s20c7` | `save_s20c7_cp` (seven pieces, checkpoint) | `aim:0.14,-0.32`, `aim:0.26,-0.1`, YES |
| `s30c3` | Step after `s20c7` | NEXT |
| `s30c4` | `save_s30c4_cp` (four pieces, checkpoint) | `aim:0.36,0.108`, `aim:0.14,0.0`, YES |
| `s30c5` | Step after `s30c4` | NEXT |
| `s00c1` | Step after `s30c5` | The game starts it during the NEXT presses of the result screens. |

With four pieces of "Operation Javier" the chapters 1 to 3 are at `aim:-0.12,0.29`, `aim:-0.14,-0.15` and `aim:0.04,-0.39`.

#### Open

- With `UC_DC_WEAK=1` the run stood at the boss of `s30c4` for 4 minutes. The health of the boss was 1 of 1200, and the fight did not go on. Without the aid the bot passed it in 1400 shots with the two launchers. The cause in the boss class (vtable 806E8A90) was not looked for. An aid that makes each loss of health larger is a possible next step.
- `s30c6` and `s30c7` were not played. After `s30c5` the game starts `s00c1`. The save after `s00c1` shows five chapter pieces of "Operation Javier" and no sixth one. The step that opens the sixth chapter was not found.
- The start of `s30c3` has one frame whose left third is drawn as horizontal streaks. The cause was not looked for.
- The six new entries are not in `project/dc/dc.cfg`.
- The pictures of the two renderer changes were not compared with a console. Three frames of room R213 after the change show the corridor and its enemies without a fault. No frame of room R224 was looked at.
- With `UC_DC_ROCKET=1` the bow gun and the hand grenade have no weapon slot, and the save keeps that.
- The plain `bot` step with the handgun alone does not pass `s10c8`, `s20c5` and `s20c7`.

## Frame pacing and load

Measured on 15 November 2026 with a build without the function trace (`-DUC_TRACE=OFF -DUC_THINLTO=ON`), Vulkan, a 1280x720 window, Ryzen 7 9800X3D and RTX 5090. The runs were driven with `scripts/drive.py` and measured at speed 1.

The clock of the game:

- The clock is the object at 80736D10. Its getter is 8004714C.
- `+0x2C` is the rate (59.94). `+0x34` is the frame target in retraces. `+0x30` is the number of retraces that the last frame took (4 at most).
- `+8` is the time step in seconds: `+0x30` divided by the rate. `+0x0C` to `+0x28` are eight time scales. `+4` is the time step times these scales.
- The frame wait is 802D8308, called at the end of a frame (8029A4DC). It calls `VIWaitForRetrace` until the retrace count of the clock reaches the target. The count is the word at r13 - 9640, and the retrace callback 802D8484 adds 1 to it. The function is the counterpart of 801581E0 of `RBUE08`.
- At its start a scene sets the target (80047788, called with 1 or 2). The menus, the logos and the first movies run with 1. The scene of a chapter (call site 80045ED4) sets 2. The movie of the chapter, the loading and the gameplay then present 30 frames for 60 retraces. The time step is 2 / 59.94 s.
- Most of the code reads the time step: 111 places read `+4` directly, and the getter has 449 call sites. 6 places read the target (see "The 60 Hz mode").

Load in the first chapter before the fixes of the next section (streets and market, the test input shoots at a grid):

- The guest needs 21 to 25 million CPU cycles for a presented frame, 18 of them on the main thread. The sound thread needs 130 to 165 million each second. A frame has 800 to 1000 draws.
- `RBUE08` needs 19 million cycles and 1090 draws for a frame in its train chapter at 30 frames per second on the same PC.
- The PC does not hold 30 frames per second in 16 to 55 % of the seconds of a run. The guest is not the cause: its main thread runs for 0.2 s of each second.
- The render thread is the cause. It needs 10 to 25 ms for a presented frame (`RBUE08`: 2 ms). The game then waits in `GXDrawDone` and takes 3 or 4 retraces for a frame.
- The renderer makes 14 to 110 textures for each frame (`RBUE08`: 0). The game writes new pixels into about 120 textures of 4x4 and 8x8 texels (RGBA8) every frame. Each change is a miss of the texture cache, a new entry there and a new texture on the GPU. A movie frame (640x480) has the same effect once per frame.
- `VulkanRenderer::collect` takes about 3 ms of each frame for the mean of the exposure copy that the game reads back.
- The guest code has no hot spot: 1323 functions have samples, and the first 20 hold 24 % of them. 70 % of the samples are in the SDK and the middleware (addresses from 80378000), 30 % in the code of the game. Functions whose `RBUE08` counterpart has a native that is off here hold 5 %.

### The render thread

The small textures:

- The game lights each model with a texture of 4x4 texels (8x8 for some), RGBA8. The light object of a model (vtable 806F22B4, 0x480 bytes) holds the 64 bytes of texels at `+0x40`.
- Each frame the update of the light object (8034F238) takes the place of the model. It asks the light manager about the lights there (virtual `+0x30`, 802D50D4). The manager is the object at 80758480, and its pointer is at r13 - 9644. Then the manager writes the 16 texels (virtual `+0x78`, 802D61D4): one sum of the lights for each texel, as bytes for alpha, red, green and blue.
- The material of the model samples the texture as texture map 1 in its first TEV stage (draw call site 804AD87C).
- The first chapter has about 80 such textures, and nearly every one gets new texels in every frame: 41430 of 41711 misses of the texture cache in 36 s.
- Each miss made a new entry in the texture cache, which kept the old versions (4096 entries at most, each with its GPU image). Each miss also made a new GPU texture: an image, a view, a staging buffer and two memory allocations. The same number of frees followed later.

The fix (`kSmallTextureTexels` in `common/title.h` is 64 texels, and it is 0 for `RBUE08`, which keeps its path):

- `gx::TextureCache`: new texels of such a texture replace the entry with the old texels. The decode runs on the calling thread, not on a worker.
- `VulkanRenderer`: when no submission uses the GPU image of such a texture any more, the image goes to a pool (512 images at most). The next small texture of that size takes it. The texels go through one staging buffer of the frame slot. A rewrite makes no image, no buffer and no memory allocation. It is one copy of 64 bytes into an image.
- The movie player writes each frame into the same 640x480 texture. Before, the flood of small textures pushed the old frames out of the cache. Without the flood 1 GB of them stayed. When a third version of one texture arrives, `kTextureVersionLimit` (2) drops the older versions. A movie frame still makes one GPU image.

The read-back of the exposure copy:

- It never waited for the GPU. The commands of the frame copy the texture to a buffer. When the fence of that submission signals, the render thread takes the pixels. The game gets them at its next `GXCopyTex`, one or two frames late.
- The time was in the mean of the pixels. The loop read the mapped buffer one byte at a time. The buffer was in write-combined memory, the first host-visible memory type of the GPU. Each copy of 128x64 pixels took 2.9 ms (render scale 2).
- If the device offers cached host memory, the buffer now comes from that type (`allocate_host_buffer`, `prefer_cached`). The renderer copies the pixels out once before the mean. Each copy takes 0.015 ms. The bytes that the game gets are the same.

The numbers are of 15 November 2026, with the same PC and settings as above and the 30 Hz step. A run is the walk into the village for 900 retraces without input, then three rounds of the spray input. A cell with more than one value gives one value for each run.

| | before | after |
|---|---|---|
| Presented frames for 60 retraces | 27.4 (26.4 with `UC_SMALL_TEXTURES=0`) | 29.9, 29.5, 29.9 |
| Render thread, ms for a presented frame | 15.6 (20.0) | 1.5, 2.0, 1.7 |
| the same in the walk / in the fights | 28.2 / 12.5 | 1.6 / 1.5 |
| GPU textures made for a frame | 32.6 (44.2) | 0.0 |
| Small textures that took a pooled image, for a frame | - | 36 to 47 |
| Read-back, ms for a copy | 2.9 | 0.015 |

- Memory at the end of such a run: the working set of the process had a peak of 2930 MB before and 1967 MB after. The texture cache held 1022 MB in the movie before and 81 MB at most after. The GPU memory in use was 873 MB before and 706 MB after.
- The pictures: two runs never show the same game time, so no pair of frames is equal. The test took 14 frames after the first voice line from one run with `UC_SMALL_TEXTURES=0` and from one run without. The closest pair has a PSNR of 22.8 dB. Neighbours of one run, 6 retraces apart, have 19 to 21 dB. Scaled down to 64x36, the pair has 36.2 dB, and the neighbours have 29 to 32 dB. The difference is on the edges of objects, where the camera moved.
- The DirectX renderers have no pool: they still make a texture for each rewrite. The change of the cache holds for them too. They have no read-back and no `GX_TF_I8` conversion.
- Still open: a rewrite is one small upload for each texture (25 to 65 in a frame). One texture for all of them (an atlas) makes it one upload, but it changes how the materials sample.
- Still open: guest frames of 80 to 170 ms, 3 to 10 in a minute, in both modes. The clock of the game counts 4 retraces at most for a frame, so each one makes the game time a few retraces late.

### The 60 Hz mode

- The frame wait of the game (802D8308) is a native now: `hle_DcFrameWait` in `game/game_pacing.cpp` (`project/dc/hle_manual.txt`). With the 60 Hz mode it waits for one retrace whatever the target is. The time step of a chapter is then 1 / 59.94 s, as in the menus, and the game presents 60 frames for 60 retraces.
- The mode is the one of `RBUE08`: a frame rate limit above 30 (`--fps-limit 60`, `graphics.fps_limit` in `config.json`), or `--unlock-60fps`. A new limit while the game runs reaches the native through `game_pacing_set_mode`. That path was not tested in this game. The headset build has the limit 60 as its default (`game/uc_main.cpp`), so it gets the mode without a setting. The Windows build without a limit runs the 30 Hz step. A limit above 60 means 60 here: the game has no timer step.
- The target itself stays as the scene set it. The six readers of the target:
  - 80299FFC (in 80299F54, the start of a frame): when a reset is asked for, the retrace count becomes 0 and the next step is target / rate. In the 60 Hz mode that frame steps 2 retraces of game time in 1.
  - 802CB8CC (in 802CB794): a screen saves the target and writes 1 (802CB928). The code at 802CBE90 (in 802CBB3C) writes the saved value back. The screen was not identified.
  - 8031D4D8 and 8031DB3C (in 8031CE54): a number that follows another number. Its step is 0.1 x difference x a factor x (time step x rate / target).
  - 8032CBE4 (in 8032CA94): a model. It gives time step x rate / target, a number of frames, to each of its animation objects (virtual `+0x28`).
  - 80337EB0 (in 80337D3C): a list of animation entries of a model. Each frame counter grows by time step x rate / target.
- So four readers turn the time step into frames of 1/30 s with the target 2. A 60 Hz step is half a frame for them, and what they drive keeps its speed. The test variable `UC_DC_FRAME_TARGET` of the measurement wrote 1 to the target: a 60 Hz step was a whole frame for them, twice the speed. The variable is gone.

Checked on 15 November 2026 (first chapter, speed 1, retrace counts, not wall time):

- Voice lines of the walk without input, one run for each mode: the test took the lines `VO_3015000`, `VO_3010001`, `VO_3017002`, `VO_3017003` and `VO_3017004`. With the 30 Hz step the last four started 320, 860, 1262 and 1535 retraces after the first one. With the 60 Hz step they started 320, 860, 1262 and 1533 retraces after it. Five later lines (to 4400 retraces after the first one) started at the same times within the 60 retraces that the log resolves.
- The `trace` step compared the camera of the walk. It followed 173 draws for 650 retraces after the first voice line, in two runs at 30 Hz and one at 60 Hz. From 60 to 530 retraces the boxes of the 60 Hz run were where the 30 Hz runs had them 1 retrace before. That is the precision of the start. Then a guest frame of 133 ms made the 60 Hz run 4 retraces late (see the open item above). The distance of the box centres at the same times was 0.006 screen units between the two 30 Hz runs (RMS, median of the draws). It was 0.011 to 0.013 between a 30 Hz run and the 60 Hz run, the late part included.
- The movie of the chapter with its loading, at speed 4: 6671 retraces at 60 Hz, 6610 at 30 Hz.
- Load at 60 Hz: 59.9 frames for 60 retraces in the walk without input (78 s), and 59.4 with the spray input (40 s). The seconds below 59 hold a frame capture or a long guest frame. The render thread needs 1.6 to 1.7 ms for a frame, and the guest 14 to 15 million cycles (850 to 900 million each second).
- The speed of the enemies was checked on 15 November 2026: one run for each mode, speed 2, no shot, no test aid. After the walk the zombies of the first fight hit the player ten times. Each hit takes 5 of the 50 health (see "Play-through"). The step `watchptr` gave the retrace of each hit. The table counts the retraces from the first voice line of the walk (`VO_3015000.brstm`).

| | 30 Hz step | 60 Hz step |
|---|---|---|
| Last voice line before the hits (`VO_3015018.brstm`) | 6715 | 6718 |
| Hits 1 to 10 | 7007, 7403, 7819, 8282, 8719, 9137, 9471, 9808, 10313, 10759 | 7001, 7425, 7799, 8238, 8578, 8949, 9315, 9769, 10115, 10602 |
| Mean time between two hits | 417 | 400 |

- The first hit comes at the same time in the two modes. The ten hits take 3752 retraces with the 30 Hz step and 3601 with the 60 Hz step. Enemies at twice the speed need about half of that time. After the first hit the two runs do not fight the same fight, because the zombies choose their attacks. The difference of 4 % is not more than that.
- Not checked: the speed of model animations that are not part of a fight. The code of the four readers above says that they keep their speed.
- Not done: the pause of the PC port (`game_pacing_set_paused`) does nothing in this game.

## Shadows

How the game makes the shadows of its characters (16 November 2026, from the code of the shadow object at 8033B69C and from packet dumps):

1. The game sets a viewport of 512x512 pixels at the top left of the frame buffer. The buffer of the console has 640x528 pixels.
2. It draws a white quad over the viewport (call site 8033B7C8). Then it draws each caster with a black material colour, seen from the light through an orthographic projection (call site 804AD87C, one TEV stage). A white line strip closes the edge (8033D364).
3. It copies the picture to a `GX_TF_Z16` texture of 512x512 texels (8033B9B8). This copy does not clear.
4. It copies the picture at half size to a `GX_CTF_R8` texture of 256x256 texels (8033BBA0). This copy does not clear.
5. It draws seven quads over the picture (call site 8033BCF8). Each quad adds the half-size texture times 0.5 to the red channel (blend one plus one, vertex colour 128, 0, 0). The quads are 448, 384, 320 and 256 pixels wide. Each one is moved by 0.5 to 4 pixels. A texel of a caster stays black only where no quad adds white. The result is a caster with a soft edge and without its rim.
6. It copies the picture to a second `GX_CTF_R8` texture of 512x512 texels and clears (8033C528).
7. The materials of the ground take that texture as texture map 2 (call site 804AD87C, four TEV stages). Texture coordinate 2 comes from the position, through the view matrix and a post matrix of the light. Stage 2 gives `K3.a x (1 - texture)`. Stage 3 mixes the colour with `K3.rgb` by that amount. `K3` is (0, 0.039, 0.157, 0.882) for some materials and (0, 0, 0, 0.502) for others.

A second kind of material reads the `GX_TF_Z16` texture as `GX_TF_IA8`, together with a ramp texture (8033DF24). Byte 12 of the shadow data of the material selects it. No draw of the dumped frames of the first chapter does that. The port still gives the farthest depth for that texture.

The fault: the native frame buffer has 480 lines. A copy of 512 lines took the 480 lines that exist and stretched them over the texture. A line of the picture was then at 512/480 of its place in the texture. The seven quads of step 5 read the half-size texture at the place that the console has. So they read 6 % of the line number away from the pixel that they added to: 4 lines at line 65, 12 lines at line 200. They added white to the casters, and the casters became white.

The fix (`gx::TextureCopy::lines`, `VulkanRenderer::record_texture_copy`): the lines that exist keep their place in the texture, and the rows below them repeat the last line. The game's own filter then works as on the console. `RBUE08` never names such lines (`kEfbCopyLines` is 480 there).

Checked on 16 November 2026 (first chapter, Vulkan, 1280x720 window):

- Pass captures of the shadow packets: the caster pass shows the white picture with one black caster (Krauser, 283 texels at line 65). A test switch kept the picture of step 5. The old copy left a black core of about 5x10 texels inside a red rim.
- A test switch dropped the quads of step 5. That run showed sharp shadows at the feet of the first zombie and beside its body. So the materials of the ground read the texture correctly.
- After the fix the body of the first zombie has a soft dark patch on the ground beside it. The two zombies of the next street have one at their feet.
- A script that does steps 4 to 6 on the captured caster picture gives 104 of 283 caster texels below 0.5 for the new copy. For this caster at line 65 the old copy gives 96. The game's filter takes the rim of a caster away on the console too.

Not checked:

- The old build was not captured at the places of the frames above. That its filter made the casters of later lines white follows from the offsets, not from a frame.
- No picture of the console was compared. The size, the darkness and the direction of the shadows were not checked.

## Point draws

The effect library (NW4R `ef`) draws one kind of particle as `GX_POINTS`. Each `GXBegin` has one point. The call site is 804A1D10, inside a recorded display list. Before the draw it calls `GXSetPointSize` (803A4C68) with the size and the texture offset 5.

- `GXSetPointSize` keeps the size in the word at `+0x7C` of the SDK state, in bits 8 to 15. The unit is a sixth of a pixel of the frame buffer. Bits 19 to 21 are the texture offset. Checked in the code of the function.
- `GXEnableTexOffsets` (803A4C9C) keeps the switch of each texture coordinate in bit 19 of the word at `+0x108 + 4 x coordinate`.
- The hardware draws a point as a square of that size around the projected vertex. The size on the screen is the same at every distance. With the texture offset 5 the corners get the texture coordinates 0 to 1, so the texture covers the square.

The port makes the square where the draw packets are built (`expand_points` in `runtime/hle/gx.cpp`, `kPointQuads` in `common/title.h`). At the start of a draw it reads the two words from the SDK state. For a draw of a display list it reads them at the time of the recording. Each point becomes four vertices in the plane that faces the screen, and the draw becomes a quad draw without culling. So every renderer gets triangles. `RBUE08` has no point draws and keeps its path.

Checked on 16 November 2026: the sixth chapter of "Memories of a Lost City" ran for 22000 retraces from its start without an error. The run went through the place of the first point draw. The log line of the first point draw gives size 6 (one pixel), texture offset 5 and texture coordinate 0. Not checked: a frame with the squares. The eight frames after the first point draw had no draw from that call site, so the particles live for a short time.

## The post-processing chain

How the game finishes its picture, and what the port does with it (16 November 2026). The facts come from packet dumps with `UC_PACKET_DUMP_TEV=1`, from pass captures, and from the code of the game. A packet is the draws between two EFB copies. Two chapters were dumped in the stereo preview. One is the first chapter of Operation Javier. The other is the fourth chapter of "Memories of a Lost City" (script `s10c4`).

### The effect objects

- Each effect is an object of one base class. The byte at `+2` is its switch. The word at `+4` is its vtable. When the switch is 0, the draw function of the base class (802D8D44) returns. If not, it calls the draw function of the effect (vtable `+0x18`). The virtual functions 802D8E60 and 802D8E78 write 1 and 0 to the switch.
- The manager of the effects is the static object at 8075B010. Its getter is 80323304. It holds three sets of effects. It draws the set at `+64`. Two more sets are at `+2824` and at `+4336`. A set has nine effects (803254A8 builds one). The glow is at `+220` of a set. Its vtable is 806EE568 and its draw function is 802A5E30. This was read from memory in a run.
- The other draw functions are named by the order of their code, not by a look at memory: the soft focus 802A6E78, the colour filter 802DCD08, the exposure 8036AFC4 with its measure 8036ABD0, and a colour gradient 8036B6C4. What the effects with the draw functions 802A0040, 802D1064 and 80310AE4 draw was not looked at.
- Four helpers draw the quads. 802A0978 draws a quad with a texture (call site 802A0B04). 802A0C40 does the same with a moved texture (802A0DBC). 802A0EF8 draws a quad of one colour (802A100C). 802A1E40 draws a quad with a texture and puts a part of the picture back (802A1F8C).
- Every effect works in the top left corner of the frame buffer, on top of the finished scene. It draws a small picture there, copies the corner to a texture, and draws the next step with that texture. Before an effect uses the corner, it copies the corner of the scene to a texture. After its last step it draws that texture back.
- The game copies most of its temporary pictures to one address (904163E0). A draw takes the newest copy of an address at the time of the draw.

### The packets of a frame

The viewport of each quad is 640x342 pixels at the top left. The sizes are those of the game. "Corner" is the top left part of the frame buffer of that size.

| Packet | Draws | Copy | What it is |
|---|---|---|---|
| Shadow 1 | the casters | 512x480 to a 512x512 `GX_TF_Z16` texture | See "Shadows". No draw of the two chapters samples this copy. |
| Shadow 2 | none | 512x480 to 256x256 | |
| Shadow 3 | seven quads | 512x480 to 512x512, with a clear | The shadow map. |
| Scene 1 | the solid part of the scene (150 to 600 draws) | 640x342 to 320x171 | The copy is for effects that bend the picture. Nothing samples it in the dumped frames. |
| Scene 2 | the see-through part (15 to 70 draws) | 640x342 to 320x171, the same address | In the first chapter ten effect sprites of the next packet sample it (call site 8049CF24). In `s10c4` nothing samples it in most frames. |
| Effects | particles (call sites 8049CD48, 8049DEC0, 8049E780) and sprites that bend the picture (8049CF24, 804A0A20) | 640x342 to 320x171, the same address | Only in some frames, a fight for example. Nothing samples the copy. In the other frames these draws are in the next packet. |
| Gradient | one quad over the picture with four vertex colours and alpha 20, added (8036BDF0) | corner 160x85 to 160x85 | First chapter only. The copy is the corner of the picture, kept for "Exposure 3". |
| Glow 1 | none | corner 320x171 to 320x171 | Glow only (`s10c4`): the corner of the scene, kept for "Glow 11". |
| Glow 2 | none | 640x342 to 320x171 | The scene at half size. |
| Glow 3 | a black quad, then the half-size scene through a TEV compare with the level 220 of 255: only bright pixels pass | corner 320x171 to 320x171 | The bright pass. |
| Glow 4 | the bright pass at quarter size | corner 160x85 to 80x42 | |
| Glow 5 to 7 | one quad each: the last copy, moved by 0.016 of its width or height, added with alpha 0.5 | corner 160x85 to 80x42 | Each quad spreads the glow to one side. |
| Glow 8 | the fourth such quad (up), then the result of the last frame added with alpha 0.15 | corner 160x85 to 160x85 | The afterglow. The next frame samples this copy. |
| Glow 9 | none | corner 160x85 to 80x42 | |
| Glow 10 | that copy over 320x171 | corner 320x171 to 160x85 | Smooths the glow. |
| Glow 11 | the corner of "Glow 1" back, then the glow added to the whole picture (one plus one) | corner 160x85 to 160x85 | The copy is the corner of the picture, kept for "Exposure 3". |
| Exposure 1 | none | the middle 320x171 (from 160,85) to 320x171 | |
| Exposure 2 | that copy into 32x16 pixels | corner 32x16 to a 32x16 `GX_TF_I8` texture | The game reads this texture from memory (see "State"). |
| Exposure 3 | the corner back | 640x342 to 640x342 | The whole picture. |
| Exposure 4 | the whole picture again with the gain: times a constant (gain below 1) or added to itself with a constant alpha (gain above 1) | corner 160x85 to 160x85 | The copy is the corner for the soft focus. |
| Soft focus 1 | none | 640x342 to 320x171 | |
| Soft focus 2 | that copy at quarter size | corner 160x85 to 80x42 | |
| Soft focus 3 | eight quads of that copy, moved by 0.002 to 0.016, each mixed in with alpha 0.5 | corner 160x85 to 80x42 | The blur. |
| Soft focus 4 | the corner back, then the blur mixed over the whole picture with alpha 0.3 | 640x342 to 640x342 | |
| Last | the colour filter: the whole picture through five or six TEV stages (call site 802A0B04). Then the HUD and the reticle. | none | The present follows. |

- The first chapter has no glow. Its frame has 17 packets and 14 copies, or 18 and 15 in a fight. A frame of `s10c4` has 27 or 28 packets and 24 or 25 copies.
- The glow shows on bright things: a lit EXIT sign, lit walls. In many scenes of `s10c4` it adds nothing (see "Checked").

### What a packet costs on the headset

The log of a run on the headset on 16 November 2026 has the GPU time of each kind of packet. The run was the first chapter with render scale 2.25. The frame buffer had 1514x2160 pixels for each eye.

- A packet with one quad in the 160x85 corner and a copy to 80x42 took 0.11 ms. With eight quads it took 0.18 ms.
- The copy of the picture at half size, with no draw, took 0.19 to 0.21 ms. The copy of the middle of the picture for the exposure took 0.36 ms.
- "Exposure 3" took 0.33 to 0.35 ms. "Exposure 4" took 0.43 to 0.45 ms. "Soft focus 4" took 0.66 to 0.75 ms. Each of them draws or copies the whole picture.
- The second scene packet took 1.2 to 1.4 ms for about 50 draws. Most of that is the cost of one more pass on the whole frame buffer.
- The chain of the first chapter took about 2.8 ms a frame. The glow of `s10c4` is ten more packets. Its cost was not measured as a part. `s10c4` ran at about 45 frames a second, the first chapter at about 70.

### What the port does

- **Copies that no draw can sample.** Sometimes the game copies to the same address two times. If no draw between the two copies sampled the first one, the first copy is dead. The builder of the packets finds such copies (`runtime/hle/gx.cpp`, `seal_or_hold`). The constant is `kDropDeadCopies` in `common/title.h`. `UC_KEEP_DEAD_COPIES=1` makes every copy.
- A dead copy gets the mark `dead` (`gx::TextureCopy`), and the renderer copies no pixels for it. The packet keeps its draws and stays a packet with a copy. A packet without draws loses its dead copy and goes to the renderer empty, because the game thread counted it.
- `UC_JOIN_DEAD_COPIES=1` also moves draws. When the next packet copies the same part of the frame buffer, the draws of the packet with the dead copy go to that packet. The two scene packets then become one pass. This is off: see "Checked".
- **The depth copy of the shadows.** The texture of a `GX_TF_Z16` copy gives the farthest depth for each texel (see "Shadows"). The renderer copies no pixels for it.
- **The chain in a proxy.** A packet of the chain has a copy inside the top left 320x180 pixels. All its draws are flat and inside that corner. The renderer draws such a packet in the bloom proxy and makes the copy from there. The proxy is a pair of images for the corner only: 740x720 pixels for each eye in the preview. It has the pixel density of the frame buffer, so each step of the chain has the picture that it had before. The large frame buffer of the two eyes is not the target of these passes. The constant is `kCornerChainProxy`. `UC_NO_BLOOM_PROXY=1` turns the proxy off.
- When a packet reads what the chain did not draw, the renderer first copies the corner of the frame buffer into the proxy. An opaque first quad of a packet makes its part of the corner. All the chains of the two chapters start with such a quad, so the corner is never copied in.
- The game puts the corner of the picture back with an opaque quad of its own (call site 802A1F8C or 802A0B04). Sometimes the first draw of the next packet is such a quad over all that the chain drew. Then the proxy is not copied back.
- `UC_CHAIN_SCALE=1` to `4` gives the proxy that number of points for each guest pixel, and the copies of the chain that size. This is for tests. With 1 or 2 the steps of the chain are softer, and small bright things lose their glow.
- **The glow switch.** With the bloom switch of the port off, the game draws no glow. The switch is `graphics.bloom` in `config.json`. On the headset it is `glow` in `vr.cfg`, or Glow in the settings panel. The port then holds the switch byte of the three glow effects at 0, one in each set (`kDcGlowEffects`). The two later sets alone are not sufficient: the game then still draws the glow. `UC_DC_GLOW=0` or `1` decides instead of the switch. The glow is on by default.
- The empty draw function 8029FFE0 in the vtable of the glow effect is no way to turn the glow off. The base class then still prepares the draw, and the effects after the glow draw at twice their size.

### Test aids

- `UC_CHAIN_AB=<file>`: while the file exists, the port makes every copy and draws the chain in the frame buffer, as before. `<file>.off` ends that. Five more rounds use `<file>.2` with `<file>.2.off`, and so on. The step `touch` of `scripts/drive.py` makes the files. On the headset, `adb shell touch` does it.
- `UC_DC_GLOW_FILE=<file>`: the glow is off while the file exists, with the same rounds.
- `UC_DC_FREEZE_FILE=<file>`: while the file exists, the eight time scales of the game's clock are 0. The camera and the models stand still, and the game still draws each frame. Two frames of the same scene, 30 retraces apart, are then equal pixel for pixel in most scenes. So one run can compare two settings on the same picture.
- `UC_GPU_PACKETS=1` prints three more lines. `[gpu-ops]` gives the passes and the copies of a frame. `[chain-proxy]` gives the packets in the proxy. `[dead-copies]` gives the copies that were not made.
- `UC_CAPTURE_PASS_MAX=40` lets the pass capture save 40 passes of a frame. The packet dump prints the viewport and the scissor of each draw, and the guest address of each copy.

### Checked

On 16 November 2026, on the PC, in the stereo preview (a frame buffer of 1479x1920 pixels for each eye).

The method. `UC_DC_FREEZE_FILE` stops the game time in a scene. `UC_CHAIN_AB` then shows the same scene the old way and the new way in one run. The frames are compared pixel for pixel. When two frames of the same setting are equal, the scene is static.

- `s10c4`, three static scenes, the default settings against the old way. The largest difference of a pixel was 5 of 255 levels. 0.003 % of the pixels at most differed by more than 2 levels.
- First chapter, two static scenes (the walk and a fight). The largest difference was 3 levels. 0.003 % of the pixels at most differed by more than 2 levels. In a third scene some pixels still moved. There the old way differed from the new way less than two frames of the new way differed from each other.
- Runs that also joined packets (as `UC_JOIN_DEAD_COPIES=1` does now) had the same result in five of six scenes. In one fight scene of the first chapter the marks on a wall were brighter: 0.38 % of the pixels differed by more than 16 levels. The cause is not found. So the port does not join packets by default.
- A run that only dropped dead copies (`UC_NO_BLOOM_PROXY=1`) showed no difference above the noise of its two scenes.
- The proxy with fewer points changes the picture. With `UC_CHAIN_SCALE=1` the mean difference to the old way was 1.7 to 2.0 levels in two scenes. A lit EXIT sign lost its glow: its pixels differed by up to 138 levels. With `UC_CHAIN_SCALE=2` the mean difference was 0.6 to 1.3 levels and the largest 33 to 190. The bright pass of the glow then reads the half-size scene with too few samples.
- The glow switch, in a scene of `s10c4` with a lit EXIT sign. With the glow off the sign lost its halo. 8.8 % of the pixels differed by more than 2 levels, and 1.3 % by more than 16. A part of that is a slightly sharper top left quarter of the picture: with the glow, the game copies that corner and draws it back one more time. In a second scene 3.1 % of the pixels differed by more than 2 levels.
- With the glow off, a frame of `s10c4` has 18 packets, as a frame of the first chapter. The game played on with the glow off for about 4000 retraces.
- In 270 retraces the game wrote its 1 to the switch byte of the drawn effect two times. The port writes the 0 again at the next retrace, so a frame with glow can still come.
- While the game time stood still, the glow did not come back after the switch was on again. With the time running it came back.
- The log line `[chain-proxy]` of each run: the corner was copied in 0 times and copied out 0 times. The game's own quad put it back three times a frame in `s10c4` and two times in the first chapter.

The work of the GPU for a frame, from the line `[gpu-ops]` and from packet dumps:

| | Passes on the frame buffer | Passes on the proxy | Copies |
|---|---|---|---|
| `s10c4`, before | 19 or 20 | 0 | 24 or 25 |
| `s10c4`, now | 9 or 10 | 10 | 21 |
| `s10c4`, glow off | 8 or 9 | 3 | 11 |
| `s10c4`, with `UC_JOIN_DEAD_COPIES=1` | 8 | 10 | 21 |
| First chapter, before | 12 or 13 | 0 | 14 or 15 |
| First chapter, now | 9 or 10 | 3 | 12 |

Not checked, and open:

- The GPU time on the headset. The proxy keeps the same number of passes and copies. It only moves ten or three passes from the large frame buffer to a small target. Only the headset can tell what that saves.
- One run of `s10c4` stopped: the renderer took no packet any more, 45 s after the last switch of a test file. The cause is not found. The log line `[render-refused]` was added after it and names the reason the next time.
- The corner copies of the game stay. Each effect copies the corner of the picture before it works and draws it back after. With the proxy the corner of the frame buffer does not change, so these copies and quads do nothing. A rule that drops them needs to know that no later draw samples the copy.
- A packet with a small copy keeps the camera of the game for its perspective draws. It does not get the camera of each eye (`small_copy` in `stereo_facts`, from the code). In most frames the effect sprites of the game are in such a packet. The packet "Effects" of the first chapter ends with the 160x85 copy of the exposure. In a frame with a third scene copy the sprites are in their own packet and get the cameras of the eyes. This was so before this work. It is why the port does not move draws from one kind of packet to another.
- The shadow map packets are drawn at the size of the game. Their quads still get the coarse shading rate that fits the scale of the frame buffer (from the code, not checked with a frame).

## Sound

No person listened to the game yet. On 15 November 2026 the sound was checked with numbers.

How the port makes sound:

- The game's sound library (NW4R) fills the parameter blocks of the SDK's voices. The native `hle_AXFrame` (`runtime/ax_audio.cpp`) runs every 3 ms of game time. It calls the game's voice callbacks and mixes the voices into 96 stereo frames: 32000 Hz, 16 bit.
- The mixer decodes PCM and ADPCM samples and changes the rate with the nearest sample. It applies the envelope and the left and right volume of each voice. It has no auxiliary buses (reverb), no filters and no surround channel. These are shortcuts of the `RBUE08` port.
- `AudioOutput::write` (`game/audio_output.cpp`) gives each block to an SDL audio stream. `--mute` sets the gain of that stream to 0.
- `UC_AUDIO_DUMP=<prefix>` writes each block to a file before the stream gets it. The dump works with `--mute` and at each speed of the game clock, because the samples are game time.

The check (`scripts/audio_check.py`, one run from the start of the game to the menu after the first chapter, 2153 s of sound):

| Part of the run | RMS, dBFS | Peak, dBFS | Samples at full scale | Spectral flatness |
|---|---|---|---|---|
| Before the intro movie (20 s) | silence, all samples 0 | - | 0 % | - |
| Intro movie, title and menus | -22.0 | 0.0 | 0.000 % | 0.07 |
| Walk into the village, voices | -18.5 | 0.0 | 0.002 % | 0.06 |
| Fights of the first chapter | -10.8 to -12.7 | 0.0 | 0.2 to 0.8 % | 0.18 to 0.27 |
| Boss fight | -7.6 to -9.9 | 0.0 | 2.6 to 4.6 % | 0.11 to 0.26 |
| Menu after the chapter | -21.1 | -4.3 | 0.000 % | 0.00 |

A flatness of 1 is white noise, and a value near 0 is tones. The centre of the spectrum is between 380 and 1570 Hz. 0 to 4 % of the energy is above 8 kHz.

Comparison with the stream files of the game, decoded with ffmpeg (`ffmpeg -i menu01.brstm menu01.wav`):

- Title music (`menu01.brstm`, stereo, 32000 Hz). The capture holds it from 29.907 s. The log shows the open of the file at retrace 1807, which is 30.1 s. The music fades in for 2 s. After that, each window of one second has a correlation of 1.000 with the file in each channel. The gain is 0.394. The correlation with the other channel of the file is 0.18 to 0.44, so left and right are not exchanged. The file played 2 % faster or slower gives 0.10 to 0.11. A file of 44100 Hz or 48000 Hz played as 32000 Hz gives 0.04 to 0.05.
- Voice line (`VO_3015000.brstm`, mono). The correlation is 0.814 at 203.166 s, with a gain of 0.585, the same in the two channels. The music of the scene plays at the same time. At wrong rates the value is below 0.12.
- Boss music (`s30c12.brstm`). The correlation is 0.38 with a gain of 0.733, below the gunfire.
- Gunfire has no stream file. In ten seconds of a fight the level of 10 ms windows goes from -25.7 to -6.1 dBFS, with a rhythm of 4.8 Hz.
- The mean step between two samples was measured inside the blocks and across the block edges. The step across an edge is 0.92 to 1.08 times the step inside a block. The blocks join without clicks.

Result: the output is the sound of the game at the right sample rate, in stereo, without gaps between blocks.

Still open:

- In the fights a part of the samples is at full scale. The share is 0.2 to 0.8 % in the first fights and 2.6 to 4.6 % in the boss fight of the first chapter. It is 4.5 to 7.2 % for ten minutes of the fifth chapter of "Memories of a Lost City". In these parts the music is 17 to 19 dB below full scale, and the other sounds are 8 to 10 dB below. One shot of the handgun has a peak of 0.6 to 0.8 of full scale. The test input pulls the trigger many times in a second, so the shots add up. A player fires less. No recording of a console was compared.
- Two parts of the hardware that change the level of the sum were checked in the code of the game. The compressor of the 2009 SDK is off. The game calls its switch with 0 at 802A38B0 (the setter is 803951FC). The flag at r13 - 8184 is 0 in a memory dump of a chapter. The master volume at r13 - 8194 is 0x8000, which is 1. The native mixer has no compressor and no master volume, and for this game it needs none. It clamps the sum of the voices to 16 bits, as the hardware does.
- The quality of the sound was not judged: reverb, filters and the 4-tap rate change are not there.
- The sound of the movies (`.sfd`) was not compared with a reference. Its level and spectrum are in the second row of the table.

## Controls

The message file of the game (`Data/message/mes_us.txtres`) describes its controls. B fires. A wave of the Wii Remote or of the Nunchuk reloads. If the magazine is empty, B also reloads. A held with a wave swings the knife. Waves shake off an enemy that holds the player. Each direction of the +Control Pad holds one weapon or herb. Plus pauses.

A host has no accelerometer. The input sample carries a wave as one more button bit above the 16 bits of the Wii Remote (`common/title.h`, `kButtonWave`, 0x10000). While the bit is set, `KPADRead` reports a remote that swings from side to side: 3 g to the left and to the right in turn, with the matching `acc_value` and `acc_speed`. The step `press:WAVE*12` of `scripts/drive.py` sends it. In the first fight the ammunition ring showed 11 of 16 rounds after five shots and the same 90 retraces later. After the wave it showed 16 rounds.

The PC action layer of `RBUE08` reads the game state of that game. For `SBDE08` one fixed mapping takes its place (`game/game_input.cpp`):

- Interact, OK and the knife action press A. Fire presses B. Back presses minus. Pause presses plus.
- Reload, the knife action and a shake of the mouse or of the VR controller send the wave for 12 reads.
- Next weapon and previous weapon press the next direction of the +Control Pad, in the order up, right, down, left.

On the headset the trigger is fire. A trigger pull off the picture or the grip is reload. The lower face button is A, the upper face button is the next weapon, and a click of the stick is pause. The game takes B as OK in its menus, so the trigger is also OK there. This mapping is not tested on a headset yet.

The game draws its reticle at 1.25 times the pointer in x and 1.67 times in y, as `RBUE08` does. The pointer of the VR controller thus needs no other scale.

## The headset view

`UC_VR_PREVIEW=1` shows the stereo view of the headset in the window of the PC build, the two eyes side by side. `UC_VR_YAW` and `UC_VR_PITCH` turn the head by degrees. `SBDE08` runs in it. Two things needed work, because `RBUE08` takes them from its game state:

- The layout of the 2D draws. In a menu they are on a flat screen in the world. In a chapter the HUD follows the head. `SBDE08` gives no menu state, so the renderer decides from the frames (`VulkanRenderer::menu_layout`). A frame that drew the world is gameplay. 20 frames in a row without the world are a menu, a movie or a loading screen.
- The reticle. The headset draws it where the controller points, not with the HUD. The laser dot of `SBDE08` comes from the call sites 800BB264 and 800BB4D8. Its rings are panes of the layout library with a square texture at the centre of the dot. The call site of the panes is 804E0228, which draws every HUD pane. `gx::mark_reticle_draws_dc` (`renderer/late_aim.h`) marks them. The list of draws came from the step `draws` of `scripts/drive.py` at two aim points.

Checked in the preview on 15 November 2026: the title screen, the profile screen and the movie are on the flat screen. The chapter is in stereo. With the head turned 25 degrees the HUD and the subtitles stay with the head, and the reticle stays with the world.

Not done for the headset: the shot follows the pointer of the game and not the ray of the controller. The hooks of `game/xr_shot.cpp` name `RBUE08` functions. Nothing is moved to a later pointer (`late_aim`).

## On the headset

First runs on a Quest 3, 15 November 2026. The game was played from the PC, with nobody in the headset.

How to play it from the PC. Put a file `args.txt` in the folder of the app (`/sdcard/Android/data/org.uc.darksidechronicles.vr/files`). Its lines are `--test-server`, a name, `--save-root` and a folder inside the app folder. Start the app. Then run `python -I scripts/drive_quest.py --package org.uc.darksidechronicles.vr --name NAME --out <folder> steps...`. The script forwards the test channels of the app with adb and runs the steps of `scripts/drive.py`. The step `shot` pulls the frame from the headset. Hold the menu presses long (`press:A*40`): at 72 Hz the test server applied a short press and its release between two reads of the game. Delete `args.txt` after the tests, or the app starts in test mode.

The crash of the shader compiler. On the Quest a pipeline key without a folded TEV module goes through the driver's own specialization of the large TEV shader. The Adreno compiler fails there with `VkResult -13`, or it crashes (`libllvm-qgl.so+0x6425A0`). `RBUE08` ships a key list and folded modules for it. `SBDE08` had none, and the app crashed in its first minute, twice. Two changes fix it:

- Each game has its own key lists (`UC_KEY_DIR`: `project/dc/pipeline_keys.seed` and `pipeline_keys.quest` for this game). `tools/merge_pipeline_keys.py` adds the keys that a run recorded in `data/pipeline_keys.bin`. The seed list has the 1,418 keys of the PC play-through. The build folds 6,776 modules from the lists.
- The compile guard (`renderer/compile_guard.h`) marks a compile in which the driver crashed. The next start does not compile that pipeline again.

With both, the app played from the start through the menus, both movies and the walk into the first fight. It did so in three runs (14, 6 and 6 minutes), and no run crashed.

The frame rate. With native eye images (2064x2208) at 90 Hz the menus ran at 60 fps and the first fight at 30 to 35 fps. The GPU took 23 ms a frame. The preset "72 Hz, no space warp" has eye images of 1514x1620, the render scale 2.25 and high foveation. With it the first fight ran at 66 fps on average, and 104 of 110 seconds were at 60 fps or more. The GPU took 11 to 15 ms a frame there. This preset is now the default of the app (`xr/xr_settings.h`). The step of the game at 72 Hz is the timer step of `hle_DcFrameWait`.

Second day, 16 November 2026. Three faults showed only on the headset:

- The input was read too seldom. After its first packet the main loop drew up to 64 more packets before it read the input again. A frame of this game is about 6 packets, so with a busy render thread one pass of the loop drew about 10 frames. The loop read the VR controllers 5 to 8 times a second (`[profile] loops` in the log). When a packet presents its frame, the drain now stops. After the change the loop ran 72 times a second or more.
- The chapter stopped at "Loading data" after its first fight. With the timer step a late frame did not wait, so the main thread of the game never blocked. The loading thread of the game got 20 to 34 ms of CPU time a second. A late frame now sleeps for 2 ms (`kTimedYieldMs`). The PC did not show this fault, because there the frame wait blocks on the retrace.
- The test clock speed above 1 (`speed:N`) is of no use on the headset: the timer step takes real time.

With these changes the app (0.0.9) played the first chapter of Operation Javier from the start to "RESULTS" in 24 minutes. The run had the test aid `UC_DC_GOD=1` in `env.txt` and the step `bot`. The app went on into the next chapter. No crash. Over that chapter the frame rate was 57.8 fps on average. It was 60 fps or more in 50 % of the seconds and less than 45 fps in 11 %. The bot fires all the time, which is more load than a player makes. The GPU took 14.6 ms a frame at the median and 18.6 ms at the 95th percentile, against 13.9 ms for 72 Hz. The game makes about 16 EFB copies a frame (`copies 960` a second in `[profile]`). It draws the scene in three or four passes with a copy of the whole picture between them.

The frame rate work of that day. The test is the first chapter from its first voice line to `VO_3015203.brstm` (about six minutes) with `bot:2400,12`, which is six shots a second. The step `bot:N,2` fires 36 shots a second and loads the game thread more than a player can.

| Build | Change | Mean fps | Seconds at 60 fps or more | Seconds below 45 fps |
|---|---|---|---|---|
| 0.0.10 | the shadow map packets at 512x480 pixels | 57.6 | 44 % | 26 % |
| 0.0.10 | render scale 2.0 instead of 2.25 | 58.4 | 48 % | 21 % |
| 0.0.11 | profile-guided optimization | 58.7 | 47 % | 19 % |
| 0.0.12 | no `RBUE08` state probe | 65.2 | 72 % | 1 % |
| 0.0.12 | render scale 2.0 | 67.2 | 80 % | 0 % |

How the causes were found:

- `UC_GPU_PACKETS=1` in `env.txt` prints the GPU time of each kind of packet. In the first fight the scene took 12.2 ms a frame and the shadow map packets 3.4 ms. The post-processing chain took 2.4 ms and the present 2.2 ms. The shadow map packets were drawn at the scale of the frame buffer (1211x2160 pixels for each eye). They are now drawn at one pixel for each guest pixel (`kNativeCopyWidth`), which takes 1.2 ms and gives sharper shadows.
- `simpleperf record --app org.uc.darksidechronicles.vr -e cpu-clock -g` on the headset showed that a third of the CPU time of the game thread was in `input_probe_snapshot`. It reads `RBUE08` game state at fixed addresses for each `KPADRead`. In this game the reads fail, and each failed read throws. The probe now returns nothing for another game.
- The profile for the optimizer is recorded on the headset. `UC_QUEST_PGO=generate scripts/build-quest.sh` builds an instrumented app. A run with `--seconds 900` in `args.txt` ends the game after that time, and the game then writes `uc.profraw` into the app folder. `llvm-profdata merge` of the NDK makes `project/pgo/dc-quest.profdata` from it. The folder `project/pgo` is not in the repository.
- The boost levels of the CPU and the GPU (`UC_CPU_BOOST=1`, `UC_GPU_LEVEL=boost`) changed nothing in this test.

The app now starts with the render scale 2.0. At that scale the test ran at the full 72 fps in 64 % of the seconds. The GPU took 10.6 ms a frame at the median and 16.9 ms at the 95th percentile.

The long run of that day. The app played the first eight chapters to their result screens on the headset, and the ninth to its boss. That was about four and a half hours of play with `UC_DC_GOD=1` and `bot:60000,6`. The bot does not win against that boss.

- One crash, after two hours, at the load of the sixth chapter: the Adreno shader compiler again, for a key that had no folded module. The compile guard needs one crash for each such key. On Android the renderer now does not compile a specialized pipeline without a folded module (`folded_only_` in `renderer/vulkan_renderer.cpp`). Its draws keep the ubershader. `UC_UNFOLDED_PIPELINES=1` gives the old way. The next three and a half chapters ran without a crash.
- The keys that the run recorded are in `project/dc/pipeline_keys.quest` (625 keys).
- Frame rate with the render scale 2.0: 69.9 fps in the first chapter, with 60 fps or more in 91 % of the seconds. It was 64.3 fps over the first three chapters and 62 fps over chapters six to eight. The fifth chapter ran at about 45 fps (`s10c4`, the glow chain).
- The slowest scene of the run was the boss of the ninth chapter: 30 to 35 fps. The scene packet took 12 ms of GPU time and the last packet of the frame 11.4 ms. The cause was not the geometry (see "Keys without a folded module" below).
- The proxy of the post-processing chain (see "The post-processing chain") was measured there with the live switch `UC_CHAIN_AB`: 34.8 fps without it, 30.3 and 31.1 fps with it. It is off (`kCornerChainProxy`). The copies that no draw samples are still not made. The glow switch changed nothing in that scene.

Keys without a folded module. The boss scene was slow because six of its pipeline keys had no folded TEV module in the build. On Android such a key keeps the ubershader (`folded_only_`). The ubershader is slow for a draw that covers the screen. Here the draws were panes of the HUD and parts of the room.

- How it was found. The last packet of the frame holds the colour filter and the HUD. It took 11.4 ms in this scene and 2.0 ms in the first fight, with fewer pixels. At render scale 1.4 it took 5.8 ms, so the cost was in the pixels. The count `keys without a folded module` in the log was 24.
- The build with the six keys in `project/dc/pipeline_keys.quest` (631 keys, app 0.0.17) runs the scene at 57 to 61 fps. The scene packet takes 6.6 ms and the last packet 2.5 ms.
- `UC_GPU_PACKETS=1` now prints the line `[uber-fallback]` each 2 s. It lists the draws that took the ubershader for lack of their own pipeline, by call site. If a scene is slow on the headset, read this line and the count of keys without a module first.

The headset now makes the missing module itself (`renderer/tev_fold.cpp`, app 0.0.18). Before, a new key waited for a later build that listed it.

- A compile thread runs the passes of `tools/fold_tev_shaders.py` with the optimizer of SPIRV-Tools. The library is the release of the build's `spirv-opt` (`vulkan-sdk-1.4.363.0`). CMake fetches its source (`UC_DEVICE_FOLD`, on for Android).
- The render thread never folds. Its draw takes the ubershader until the module and the pipeline are ready.
- One fold runs at a time. A fold takes about 0.5 s on a Quest 3 and about 0.1 s on the PC.
- The module stays in `data/folded-<hash>` of the app folder for the next starts. The hash is that of the four TEV modules of the build, so a new build starts a new folder.
- Checked on the PC: `UC_FOLDED_ONLY=1` makes the stereo preview compile as the headset does. The preview then made every module at run time (1,203 modules) and drew the boss scene as before. 39 of these modules were compared with the output of the tool for the same keys. Each pair was equal, byte for byte.
- Checked on the headset: `UC_DEVICE_FOLD_TEST=16` in `env.txt` leaves the build's modules of one material in 16 unused. The app made 155 modules at its start (79 s instead of 4 s, once). It then ran the boss scene at 57 to 61 fps without a crash, and the frame was right.
- `UC_NO_DEVICE_FOLD=1` turns the fold off.

The fifth chapter (`s10c4`) with the app 0.0.19: the bot played it to its result screen in 20 minutes at 69.8 fps on average. 95 % of the seconds had 60 fps or more, and no second was below 45 fps. The device made two modules in play. The 45 fps of the long run came from keys without a module, not from the glow chain.

The route to a chapter of "Memories of a Lost City" from a save, for `scripts/drive_quest.py`, has five steps:

1. Press A on the piece of the chapter.
2. Press A on YES in the checkpoint dialog (`aim:-0.16,0.115`).
3. Press A on Leon (`aim:0.1,0.07`).
4. Press A on NEXT (`aim:0.59,0.237`).
5. Wait for the script with `until:Data/script/s10c,A,120,6000`.

The piece of `s10c3` is at `aim:0.233,-0.237`, that of `s10c4` at `aim:0.033,0.068` and that of `s10c5` at `aim:0.325,0.045`. If the save holds checkpoint data of another chapter, the dialog comes. YES deletes that data. The reticle in a headset frame is not the pointer of the game. Find the places of a menu in the PC build.

Space warp. With `refresh 90` and `space_warp on` in `vr.cfg` the app 0.0.19 ran the boss scene for three minutes without a fault. The game rendered 45 or 46 frames each second, and the headset reported application space warp at 90 Hz. The GPU took 14.3 ms a frame. Nobody looked at the frames that the headset makes between the frames of the game, so space warp stays off by default.

At each start 74 pipelines fail to compile with `VkResult -13`. The `RBUE08` app has 78 such failures. The log now names them (app 0.0.20). Each one is a small-draw ubershader of the families with one stage or two stages (`uber_merge.h`). The general small-draw ubershader compiles. So the draws lose only the shorter shader of their family. The cause in the driver is not known.

Not done on the headset: play by a person with the controllers, and the sound.

## State

On 15 November 2026 the Windows build of `SBDE08` does this:

- It runs the SDK start-up, the save start-up, and the start of NW4R, GX, VI, AI, AX and KPAD.
- It loads its files from the unpacked disc.
- It draws the strap screen, the Zapper screen, and the Capcom and CRIWARE logos.
- It shows the dialog "Save game data not found. Create data?", takes A from the test input, and writes `banner.bin` and `buc2_0.dat` to the save folder.
- It plays the intro movie (`MV_000_10.sfd`, decoded by the game's own code) and reaches the title screen.
- It shows the profile screen, the difficulty screen, the main menu and the character screen, and takes the pointer and A there.
- It plays the two movies of the first chapter, loads the chapter and runs it to its boss. The chapter has the walk through the village, the first enemy, the HUD, shots that kill the enemy, the reload and the subtitles. It has the scenes in the streets and the market, the spiders, the scene with the wounded man and the choice of ways. It has the creatures in the water, the screen "You are Dead" and the return to the last checkpoint.
- It reads the music and voice streams (`Data/sound/stream/*.brstm`). The scenes of the chapter wait for their voice lines, so the chapter did not go on after the first enemy before.
- The exposure of the picture follows the brightness of the scene: the game reads the picture back from the GPU.
- With the test aid `UC_DC_GOD=1` it plays the first chapter to its end: the boss, the result screens and the save. It loads that save in the next run. It plays the first seven chapters of "Memories of a Lost City" and most of the eighth (see "Play-through").
- With the test aids `UC_DC_WEAK=1` and `UC_DC_ROCKET=1` it plays the end of "Memories of a Lost City" and all of "Game of Oblivion". It also plays the chapters 2 to 5 of "Operation Javier" (see "The rest of the game").
- Its mixed sound is the sound of the game. The title music is equal to the stream file, sample for sample, at the right rate and in stereo (see "Sound").

Found on the way (each is a constant in `common/title.h` or a note in the code):

- Three math natives named `RBUE08` data directly. `PSMTXConcat` gave wrong matrices to every layout pane under a moved parent, so the profile screen drew no slots.
- The arena of MEM1 ends at the file table of the disc (0x817D8320) and the arena of MEM2 at 0x935E0000 (IOS55). With the `RBUE08` values the game ran out of memory at the load of the chapter.
- `OSJoinThread` does not wait for a detached thread.
- NW4R material display lists write a part of the constant selectors of a stage pair.
- The game sets its exposure from the picture. Each frame it copies the middle of the frame buffer to a `GX_TF_I8` texture and draws that texture into a 32x16 corner. It copies the corner to a second `GX_TF_I8` texture (8036AD98) and reads 66 bytes of that texture from memory (8036AE74). It takes the two conversions of the hardware away from their mean (`Y = 16 + 0.8588 x colour`, twice) and gives the result to its tone mapping. A native EFB copy stays on the GPU, so the texture in memory was all zero. The game saw a black picture and chose the largest exposure, and the daylight scene was white. The Vulkan renderer now reads such a small intensity copy back (`kEfbCopyReadbackTexels` in `common/title.h`). The next `GXCopyTex` call writes it to the guest's texture, one or two frames late. The game records these copies in a display list, so the request goes through `GXCallDisplayList` too.
- An EFB copy to `GX_TF_I8` holds `Y = 16 + 0.257 R + 0.504 G + 0.098 B` on the hardware, and the game's formulas expect that. The Vulkan renderer converts the copy on the GPU (`renderer/intensity_copy_pass.cpp`). Before, it read the green channel.
- The exposure is a gain of `0.15 / brightness`, held between 0.75 and 1.25 (8036AFC4). After it the game draws its colour filter with six TEV stages (call site 802A0B04). The filter takes 30 % of the colour away. Its gain is 1 + (0.62, 0.56, 0.41) for red, green and blue in the first chapter. It takes 8 % of the blue away, and its contrast curve is `x (0.70 x + 0.30)`. The filter comes from the game's data. With the smallest exposure a part of the scene that is brighter than about 0.83 before the filter loses its detail. It comes out as pale yellow (255, 255, about 210). Ground in direct sun does that: its material is twice the texture times the light (TEV scale 2), and the sun has the colour 255. No reference picture of the console was compared.
- NW4R reads its stream files under a mutex that it takes with `OSTryLockMutex` and gives back with `OSUnlockMutex`. `OSUnlockMutex` is a native that keeps the state of a mutex outside the guest. `OSTryLockMutex` was the SDK's code, which works on the fields of the guest's mutex. The first reader kept the mutex for ever, and every later reader slept. When the game stopped the sound, the read of such a reader ended with "cancelled". `OSTryLockMutex` is a native now (`project/dc/hle_manual.txt`).
- The native `GXSetBlendMode` kept the subtract bit of the blend word after the first subtract draw. Later draws that set their blend through this function were subtracted from the picture (see "Play-through").
- The effect library draws one kind of particle as `GX_POINTS`. The native renderers have no point primitive, and such a draw stopped the game. Each point is a square of two triangles now (`kPointQuads` in `common/title.h`, see "Point draws").
- A copy of frame buffer lines that the native buffer does not have was stretched over its texture. The filter of the game's shadows then read the wrong lines, and the shadows were white (see "Shadows").

Not done yet:

- Materials with the depth flag of the shadow data get no shadow: an EFB copy to `GX_TF_Z16` gives the farthest depth in every texel. No draw of the first chapter has that flag (see "Shadows"). The casters of the last 32 lines of the shadow picture are not drawn. EFB copies to `GX_CTF_R8` give the red channel. Vulkan renderer only.
- No person listened to the sound. The numbers are in "Sound". In long fights 2.6 to 7.2 % of the samples are at full scale.
- The chapters were played with the test aid `UC_DC_GOD=1` only. Play without the aid was not tested after the first fight.
- The chapters 6 and 7 of "Operation Javier" were not played (see "The rest of the game", "Open").
- No frame with the squares of the `GX_POINTS` particles was looked at (see "Point draws").
- After the death of the player or of the partner the game shows its game-over screens. The test input does not operate them. In two runs the game went back to its menus.
- In one run the player died at the spiders, and the game went back to its menu instead of a checkpoint (it loaded `mainMenu_us.arc`). The B presses of the test input then started the chapter again. The screens between are not checked.
- `__OSUnlockAllMutex` (8037CF28) is the SDK's code and reads the fields of the guest's mutex, which the natives do not write: a thread that ends with a mutex keeps it.
- Keyboard, mouse, gamepad and VR controller input have one fixed mapping (see "Controls"). It is not tested with real devices.
- The Quest app runs on a Quest 3 (see "On the headset"). A person has not played it with the controllers yet.
- The exposure read-back and the conversion of `GX_TF_I8` copies are in the Vulkan renderer only. The read-back takes the first layer (the left eye of a stereo view).
- The HOME button dims the screen and shows the hand pointer, but the menu itself is not checked.
