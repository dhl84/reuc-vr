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
- `UC_DC_WEAK=1` holds the health of every enemy at 1 or below. `UC_DC_ROCKET=1` gives the first player the rocket launcher and the linear launcher. `UC_DC_BLOOD=1` gives the good ending of "Operation Javier". The three are test aids and are off by default (see "The rest of the game").
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

On 16 and 17 November 2026 the Windows build was played on from the last boss of "Memories of a Lost City" (script `s10c8`). The first run started from a save of the headset at the checkpoint of that boss. The scripts of "Memories of a Lost City" are `s10c1` to `s10c8`. Those of "Game of Oblivion" are `s20c1` to `s20c7`. Those of "Operation Javier" are `s30c1` to `s30c7`.

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
- `UC_DC_BLOOD=1` is for the last boss of "Operation Javier". It holds the count of Manuela's burns at 0. It also keeps the time for the last hit on the boss open (see "What opens the sixth chapter").
- The three aids are off by default. The constants are in `common/title.h`, and the code is in `game/uc_main.cpp`. The other game does not have them.
- The fourth value of the `bot` step is a list of buttons, one for each row of 7 shots, for example `UP/LEFT/DOWN/-/-/-/-`. The bot presses the button after the row. Up is the handgun, Left the rocket launcher and Down the linear launcher.
- Each pass of the `bot` step over its grid is moved by a part of the grid pitch. After 16 passes the bot has shot at every place in steps of 0.05.
- The step `probe:NAME=ADDR,OFFSET,...,SIZE` names a value behind a chain of pointers. The `bot` step prints each change of it. The table of objects is at 80732B88. The Tyrant was entry 68, so `probe:hp=80732DAC,0x44C,0,4` is its health.

This step wins the fight of `s10c8`: `bot:30000,2,chapterResult_us.arc,UP/LEFT/-/-/-/-/-` with `UC_DC_GOD=1 UC_DC_WEAK=1 UC_DC_ROCKET=1`. The handgun takes the health to 0, and a rocket hits in one of the next 2-second times. In the run with probes the Tyrant died 36000 retraces after the start of the run. A second run reached the result screen of the chapter.

Three later places needed more than the health:

- The puppet theatre of `s20c4` is one object of an enemy class (vtable 806E0800) with six puppets. When a shot kills the boy or the girl, the show starts again. A player shoots the knight, the bear and the two fish in the short times in which the show lets a shot hit them. With `UC_DC_WEAK=1` the boy and the girl keep their health, and the four other puppets have none.
- The Tyrant in the plane of `s20c5` takes no damage in its last part. A rocket starts the scene with the missile (voice line "At this range!?"). The bot passed it with `UP/LEFT/-/-/-/-/-` in 140 shots. With the handgun alone it did not pass in 4 minutes.
- The last boss of `s20c7` needs the linear launcher ("Use the Launcher!"). With the handgun and the rocket launcher the run stood there for 4 minutes. With `UP/LEFT/DOWN/-/-/-/-` the chapter ended.

#### What opens the sixth chapter

The chapters 6 and 7 of "Operation Javier" are the scripts `s30c6` and `s30c7`. Krauser is the first player in them, and Leon is the partner. They are on a fourth piece of the stage screen, which also has the name "Operation Javier".

- The game opens chapters at the result of a chapter (8008F018 to 8008F418). The save keeps a word of flags for each chapter (800CF7B0). Bit 0 says that the chapter is open, and bit 2 says that it was cleared. The bits 0x08 to 0x80 name the difficulty of the clear.
- The clear of the fifth chapter opens the sixth one only with two more conditions. The clear was at difficulty 2 or above (bits 0xF0). The byte at `+12` of the ending block is 1, which is the good ending.
- The ending block is at `+0xCC` of the static object 80733B68, and the save keeps a copy. Its word at `+8` counts the times that Manuela burns the last boss with her blood (80226574). When the boss dies, the game writes the byte: 1 if the count is at most a number of the boss data, else 0 (802223F0).
- The boss (vtable 806E7798) dies from one shot on its head in a time of 4 seconds. Flag 4 of the word at `+0x7FC` marks that time (802262A8). After each such time without that shot Manuela burns the boss once.
- The `bot` step hit the head only after 14 such times in one run and after more than 40 in another. The save then had the bad ending. Its flags of the fifth chapter were 0x225, and the byte was 0. The port stops no flag.
- `s00c1` has no part in this. It is the extra chapter with blocks of tofu as enemies. After the first clear of the fifth chapter the main menu has the button SECRET at the place of NEXT (`_SetSecretChapter` in `zMaster_Root.lub`). The A presses for the result screens pressed it.
- After the fifth and the seventh chapter the game plays the end movie and the staff roll (`MV_0XX_10.sfd`, 260 seconds). Then it shows the result screen, saves and goes to the main menu.

With `UC_DC_BLOOD=1 UC_DC_ROCKET=1` the fifth chapter ended with the good ending after 6090 shots from its checkpoint. The stage screen then showed the fourth piece with the mark NEW, and its chapter screen showed "Chapter 6". After the sixth chapter the chapter screen showed the seventh one.

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
| `s30c5` | Operation Javier, Chapter 5 | "RESULTS" without `UC_DC_WEAK`: the bad ending. Clear time 00:25:38, 2 enemies. Played again from its checkpoint with `UC_DC_BLOOD=1`: the good ending. Clear time 00:13:50. |
| `s30c6` | Operation Javier, Chapter 6 | "RESULTS" with `UC_DC_BLOOD=1 UC_DC_ROCKET=1`. Clear time 00:25:09, 70 enemies. |
| `s30c7` | Operation Javier, Chapter 7 | "RESULTS" with `UC_DC_BLOOD=1 UC_DC_ROCKET=1`, in two runs. Clear time 00:26:17, 2 enemies. The second run showed the staff roll for 2 minutes: nine frames have the title and the names. |
| `s00c1` | The extra chapter with Claire alone and blocks of tofu | "RESULTS" without `UC_DC_WEAK`. Clear time 00:13:29, 79 enemies. The same A presses then pressed SECRET again. |

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
| `s30c5` | Step after `s30c4`, or `save_s30c4_done` | NEXT. From the save: `aim:0.36,0.108`, `aim:0.2,-0.41`, NEXT. From `save_s30c5_cp` the same pieces, then YES. |
| `s30c6` | `save_s30c5_done_good` | `aim:0.47,0.34` (the fourth piece), `aim:0.02,0.05`, NEXT |
| `s30c7` | `save_s30c6_done` | `aim:0.47,0.34`, `aim:0.16,-0.14`, NEXT |
| `s00c1` | The main menu after the fifth chapter | The button SECRET at `aim:0.59,0.237` |

With four pieces of "Operation Javier" the chapters 1 to 3 are at `aim:-0.12,0.29`, `aim:-0.14,-0.15` and `aim:0.04,-0.39`.

The fifth and the seventh chapter end at the main menu, not at the menu with CHRONICLES. Use `aim:0.59,0.237 until:mainMenu_us.arc,A,90,3000` for their result screens, and stop the presses there. The game saves before it loads that menu.

#### Open

- With `UC_DC_WEAK=1` the run stood at the boss of `s30c4` for 4 minutes. The health of the boss was 1 of 1200, and the fight did not go on. Without the aid the bot passed it in 1400 shots with the two launchers. The cause in the boss class (vtable 806E8A90) was not looked for. An aid that makes each loss of health larger is a possible next step.
- Without `UC_DC_BLOOD=1` the bot gets the bad ending of the fifth chapter, and the sixth chapter stays closed.
- The scripts `z10c1` and `z10c2` were not started. The extra chapter `s00c1` was played once.
- With `UC_DC_WEAK=1` the fight against the boss of `s30c6` does not go on either. The headset bot stood there for 10 minutes on 24 November 2026. Without the aid it beat that boss in 5 minutes. The seventh chapter was not played with `UC_DC_WEAK=1`.
- The start of `s30c3` has one frame whose left third is drawn as horizontal streaks. Fourteen frames of the same scene from 24 November 2026 show a waterfall at that wall and no fault (`pv\t_s30c3_flat`). So the streaks are very likely the waterfall, seen from above.
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

On the headset the shot does not go through the pointer. It goes along the ray of the controller (see "The shot along the ray of the controller"). The pointer still places the reticle, and the menus still read it. A trigger pull with the pointer off the picture is still a reload.

The game draws its reticle at 1.25 times the pointer in x and 1.67 times in y, as `RBUE08` does. The pointer of the VR controller thus needs no other scale.

## The headset view

`UC_VR_PREVIEW=1` shows the stereo view of the headset in the window of the PC build, the two eyes side by side. `UC_VR_YAW` and `UC_VR_PITCH` turn the head by degrees. `SBDE08` runs in it. Two things needed work, because `RBUE08` takes them from its game state:

- The layout of the 2D draws. In a menu they are on a flat screen in the world. In a chapter the HUD follows the head. `SBDE08` gives no menu state, so the renderer decides from the frames (`VulkanRenderer::menu_layout`). A frame that drew the world is gameplay. 20 frames in a row without the world are a menu, a movie or a loading screen.
- The reticle. The headset draws it where the controller points, not with the HUD. The laser dot of `SBDE08` comes from the call sites 800BB264 and 800BB4D8. Its rings are panes of the layout library with a square texture at the centre of the dot. The call site of the panes is 804E0228, which draws every HUD pane. `gx::mark_reticle_draws_dc` (`renderer/late_aim.h`) marks them. The list of draws came from the step `draws` of `scripts/drive.py` at two aim points.

Checked in the preview on 15 November 2026: the title screen, the profile screen and the movie are on the flat screen. The chapter is in stereo. With the head turned 25 degrees the HUD and the subtitles stay with the head, and the reticle stays with the world.

Nothing is moved to a later pointer (`late_aim`).

### The shot along the ray of the controller

On a flat screen the game aims from the camera through the pointer. In the headset the hand is not at the camera. The line from the camera through the pointer then passes beside the thing that the laser of the controller touches. With a tracked ray present, the port moves the aim of the game to that ray, as it does for `RBUE08`. A tracked ray is the ray from the muzzle of the VR controller, in the space of the game camera (`runtime/tracked_ray.h`).

The functions of `SBDE08` (the constants are in `common/title.h`):

- 800B8C10 fires a weapon. For each pellet it turns the aim line by a random angle and calls 800B6AE4, which takes a bullet from a pool.
- 800B4E30 is the bullet initializer. A bullet of a player starts at the position of the camera (`+0xD8`) and holds its direction at `+0xE4`. Its trail starts at `+0x54`, which is the origin plus the offset at `+0xF0`.
- 800B5170 moves each bullet in each frame and tests the step with the segment query 800B1EB0.
- 800B151C is the player update. It unprojects the pointer (802974F4) and calls the segment query from the camera to that far point.
- The result goes to the aim data of the player, 988 bytes at 807507A0. The hit point is at `+0x3BC`, the far point at `+0x3C8` and the object under the pointer at `+0x390`.
- 800B2400 gives the aim data to the weapon, to the item pick-up and to the enemies.
- 800C12E8 is the item pick-up. It calls the segment query once more, from the camera to the hit point.
- The camera in use is one of the 32 cameras of the camera system. The pointer to the system is at 807369FC. The index of the camera is the byte at `+0xF4` of the system. The cameras have 268 bytes each from `+0xF8`. A camera starts with its view matrix (804C7FD8), and its position is at `+0x74`.

A trace build with `UC_TRACE_NEW` listed the functions that ran first at two presses of B. Their callers and callees came from the disassembly. The camera was read from the running game: the third row of its matrix is the line from the target of the camera to its position.

The two hooks are in `game/xr_shot.cpp`. `uc::dc_xr_scope` (`game/xr_shot.h`) puts them at the entries 800B1EB0 and 800B4E30 of the translated game:

- At the segment query of the pointer (return address 800B1718), the start becomes the origin of the ray. The far point becomes the point on the ray at the same distance. The hit point, the object under the pointer, the pick-up and the prompts then follow the ray.
- At the segment query of the item pick-up (return address 800C1418), the start becomes the origin of the ray.
- After the bullet initializer (return address 800B6B88, origin argument 0), the bullet starts at the origin of the ray and flies along the ray. The trail offset is 0, so the trail starts at the muzzle. A pellet keeps its angle to the aim line of the game, and that angle is now around the ray.

Without a tracked ray the hooks change nothing. That is the case for the PC with the mouse, for the `bot` step and for `scripts/drive.py`. `UC_XR_TEST_SHOT=ox,oy,oz,dx,dy,dz` gives a fixed ray for a test, and `UC_XR_SHOT_LOG=1` logs each changed query and each changed bullet. The hooks do nothing while the view matrix of the camera is not up to date. The log names that case, and no run showed it.

Checked on the PC on 17 November 2026 in the boss fight of `s10c8`. Each run fired the handgun 15 times at fixed retraces and read the health of the boss after each shot. The boss starts with 1600 and moves, so a part of the shots misses. The first six shots fall in its entrance, where it takes no damage.

| Run | Pointer | Test ray | Health after the 15 shots | Shots that took health |
|---|---|---|---|---|
| A | On the boss | None | 1568 | 6 |
| A again | On the boss | None | 1584 | 4 |
| A with the build of 15 November, which has no hooks | On the boss | None | 1580 | 5 |
| B | On the wall to the upper right of the boss | None | 1600 | 0 |
| C | As B | From the camera through the pointer place of A | 1568 | 6 |
| D | As A | From the camera through the pointer place of B | 1600 | 0 |
| E | As B | From 3 units to the right of the camera and 2 units below it, across the line of A at 24 units | 1568 | 6 |

So the shot hits where the ray goes and not where the pointer is. In run E the log shows each bullet on the ray, 3.6 units from the camera.

Two more runs stopped the game time at the same retrace (`UC_DC_FREEZE_FILE`) and read the hit point and the camera matrix. Without a ray the hit point was on the line from the camera through the pointer, 0.000 units beside it. With the ray of run E the hit point was 0.0000 units beside the ray and 1.30 units beside the line of the pointer. On the screen that is 53 pixels from the pointer, at 640 x 480.

Without a ray the step `bot:420,2` ran three times in the same fight. With the build of 15 November the boss lost 166 of its health. With the new build it lost 178 and 168. With `UC_XR_SHOT_LOG=1` the runs without a ray logged no changed query and no changed bullet.

The item pick-up. One run held the pointer on the wall to the upper right and used a ray through the middle of the screen. It pressed B and A in turn, with `UC_DC_WEAK=1`. The shots took the boss to its state 15. Before that state Ada throws the rocket launcher to the floor. A took it: the count of the launcher in the inventory went from 0 to 4. The log showed 42 pick-up queries along the ray.

The unit test `tests/test_xr_shot.cpp` has the cases of `SBDE08` in a build of that game. It passes in that build and in a build of `RBUE08`.

The reticle. The game draws its reticle at the pointer, in 2D, and not at the hit point. While the game aims at an enemy, the ring of the reticle is red. At other times it is gray. On the headset the pointer is the place where the ray of the controller meets the scene, seen from the game camera (`StereoScene::aim`). The renderer draws the marked reticle at that depth. The reticle is then on the ray, and the shot goes along the same ray. This path was not run for `SBDE08`: the PC has no controller, and the headset was in use.

The stereo preview has no controller. It draws the reticle at the pointer of the game, on a plane 30 units ahead (3 m). One preview run used the ray of run E and put the pointer where that ray crosses the plane. At 8 of 9 shots the frame agreed with the health. Either the reticle was on the boss in both eyes with a red ring, and the shot took health. Or the reticle was on the wall with a gray ring, and the shot took none. So when the pointer comes from the ray, the reticle of the preview sits on the hit point. With any other pointer the preview shows the reticle at the pointer, and the shot still follows the ray. A preview that takes its pointer from `UC_XR_TEST_SHOT` is not written. That change is in `renderer/vulkan_renderer.cpp` and in the input of the PC build.

Open:

- The hooks did not run on a headset. The scale of the tracked ray (game units for each meter) was not checked for this game.
- One enemy class (state function 8021291C, test in 80212C44) makes its own test of the pointer. It unprojects the pointer and marks the parts of the enemy that a player aims at. It still uses the pointer.
- The function 80059C04 tests the line of sight from the camera to the object under the pointer. It was not changed.
- The second player uses the same code with player index 1. It was not run.
- A grenade and the knife were not looked at. Each bullet of a weapon goes through the bullet initializer.

### The first play by a person

On 17 November 2026 a person played the app 0.0.31 for five minutes. The report: bad texture flicker, strange pop-in and an inaccurate aim. No run of the bot showed these faults, because the bot runs kept numbers and no pictures. The stereo preview on the PC showed most of them.

How to look for such faults without the headset:

- `UC_VR_PREVIEW=1` with `UC_VR_YAW` shows both eyes. Compare the two halves of a frame: a thing that is in one eye only is a fault.
- `UC_DC_FREEZE_FILE` holds the game time. Eight frames of the standing scene, one retrace apart, were equal in each test. So nothing flickers by itself with a still head on the PC.
- `UC_VR_FOVEATION=3` shows the coarse shading of the headset in the preview.
- `UC_POST_DUMP=N` with `UC_POST_DUMP_FROM=F` lists the passes and the draws of 12 frames from frame F. The count of presented frames at the start of a chapter is in the `[bench]` lines of the log.
- A flat run of the same streets gives the picture of the game itself. The torn posters on the walls look like a fault, and they are the art of the game.

What was found:

- The water read the wrong part of the picture. The water is a draw of the world (call site 804AD87C) with a half-size copy of the picture as texture 0. Its texgen takes the position through the game camera to the screen. So the coordinate is the place of the pixel in the picture of that one camera. In the headset the copy holds the picture of an eye. The water then showed parts of the scene that lie beside it, each eye had other parts, and they moved with the head. A second call site (8049CF24) does the same in the streets.
- The fix is `gx::screen_lookup` (`renderer/screen_lookup.h`). It finds such a texgen from the matrices of the draw: its three rows are multiples of the x, y and w rows of the clip transform of the camera. The vertex stage of the headset then makes that coordinate from the clip position of the eye (`StereoBlock::lookup` in `smoke.slang`). `UC_VR_NO_SCREEN_LOOKUP=1` keeps the coordinate of the game. In the preview the water is the same in both eyes with it, also with the head turned 35 degrees.
- The wide view of the game was off. `MTXPerspective` (`game/intro.cpp`) widens the view of the game so that it draws what the eyes see. For `SBDE08` it was off, because its body went on at an `RBUE08` address. It is on now (the entry 80383B94 in `project/dc/dc.cfg`). The first chapter has 6 % more draws a frame with it in the preview.
- The coarse shading is visible. At the render scale 2.0 the headset shades 63 % of an eye image in blocks of 4x4 pixels (foveation level 3). The automatic render scale goes down to 1.5. In the preview the edges are steps and the textures are blocks with these values. This is one likely cause of the texture flicker. The section "The see-through draws of the world" gives a second cause. The values were set for the frame rate before a person looked at the picture. The section "The finer picture" gives the new default.
- The aim fault was not found on the PC. The sections "The lens of the aim" and "The aim test on the headset" give the cause and the test without a person. The world scale is right: a person of this game is 18.5 units tall, and the headset takes 10 units for a metre. On the PC a shot along a test ray lands where the ray points, also while the camera moves. `UC_DC_RAY_SHOT=0` turns the shot hooks off for a comparison on the headset.

### The see-through draws of the world

The game draws the see-through things of the world in a packet of their own: dust, smoke, leaves, decals. The packet of a street has about 200 such draws. Its last draw is a 2D quad that draws the picture small into a corner of the frame buffer. The copy of the packet takes that corner (160x85).

The headset renderer has a rule for a small picture that the game renders for a texture: its draws keep the camera of the game. The rule looked at the size of the copy only. So all draws of that packet kept the camera of the game, and each eye showed them at the place of that camera. With the head straight they were near their right place, without depth. With the head turned they were a second picture of roofs, fences and people at a wrong place. This is the pop-in of the first play and a part of its flicker.

The rule now also looks at the viewport of the draw (`title::kSmallCopyByViewport`, `stereo_facts` in `renderer/vulkan_renderer.cpp`). A draw with the viewport of the whole picture is a draw of the world.

Some of those draws read a copy of the picture at the place of the pixel (call site 8049CF24), and `gx::screen_lookup` finds them now. With the wide view the scale of that coordinate was wrong. The game makes the coordinate with the view of its own picture, and the projection of the draw is 1.8 and 2.7 times wider. The fit gave the scales 0.9 and 1.35, and the right scale is 0.5. When the projection of the draw is a wide one, the renderer takes the two factors out (`gx::lookup_without_wide_view`). Without that the lower part of the picture was stripes.

How to see the fault on the PC: run the stereo preview with `UC_VR_PITCH=35` (`pc_ghost.sh`). With the head straight the preview shows nothing wrong. The headset on the desk looks 35 to 40 degrees up, so its stills showed the second picture. Test each change of the headset view with a turned head.

### The lens of the aim

The headset code takes the hit of the controller ray to the pointer of the game through a lens. The lens is the pair of view tangents of the game camera. The 2D reticle of the game comes back through the same lens. For `RBUE08` the renderer reads the lens from the first draw of the scene phase. The frames of `SBDE08` have no scene phase, so the lens stayed at its start value: 60 degrees at 16:9.

The camera of a chapter has 55 degrees. `MTXPerspective` has one caller (804C80E4), with 60 degrees in the menus, 55 degrees in a chapter and 40 degrees for one more camera. So in the app 0.0.31 the reticle was 11 % nearer to the middle than the laser dot. That is 2 degrees at 20 degrees from the middle. In the apps 0.0.33 to 0.0.36 the wide view also made the pointer of the game wrong by the factors 1.8 and 2.7.

Now each world draw of the eyes counts for its projection (`StereoScene::observe_aim`, `title::kAimLensFromWorldDraws`). The projection of most draws of a frame is the lens of the aim. The pointer (`StereoScene::aim`) and the reticle draws (`LG_STEREO_HUD_AIM`) use it. The lens of the screen for the menus and the HUD stays as it was.

The lens of the aim is the wide view. A 2D reticle keeps its place there and becomes 1.8 and 2.7 times too large. The renderer makes it smaller about its centre by the two factors (`stereo_block_scale_about`).

### The aim test on the headset

`quest_aim.sh NAME "space:0.2,-0.2,-0.3,0,0,25,20"` tests the aim without a person. It needs the app 0.0.36 or later. The parts:

- `UC_VR_TEST_AIM=space:x,y,z,yaw,pitch,sweep,period` is a controller pose in the play space: a place in metres and a direction in degrees. The direction swings by the sweep, so the ray goes over the view. The form `head:` follows the head. The real controllers take no part: one that lies on its trigger makes its hand the gun hand.
- `UC_XR_TEST_FEED=<file>`: while that file exists, the pose aims the game although the test driver is connected. The test driver pulls the trigger with the step `wii:B`, which leaves the pointer alone.
- `UC_XR_SHOT_LOG=1` prints the line `[xr-pick] lines nearest` for each frame. It gives the gap between the aim line of the game and the controller ray. The aim line goes from the camera to the pointer, where the reticle is. The shot goes along the ray. The two lines must meet at the hit of the ray.
- `aim_gap.py` and `aim_fit.py` (`q3\build\darkside`) read the log. `aim_fit.py` finds the view tangents that the game uses for its pointer.

| App | Lens of the headset code | Tangents of the game pointer | Gap of the two lines from the camera | Angle of the lines at far hits |
|---|---|---|---|---|
| 0.0.36 | 1.026 x 0.578 | 1.68 x 1.40 | median 10.9 degrees | median 16.0 degrees |
| 0.0.37 | 1.667 x 1.409 | 1.66 x 1.40 | median 0.04 degrees, 90 % below 0.08 degrees | median 0.1 degrees |

In the app 0.0.37 the two lines meet within 2 cm at a hit 11 to 13 metres away. The test does not show where the 2D reticle is in the picture of an eye, and no person held the controllers yet.

### The second play by a person

On 18 November 2026 a person played the app 0.0.37. The report: the water is terrible and very disorienting, and sprites in the distance pop in badly.

The water (call site 804AD87C) had two faults.

- The scale of its lookup still held the factors of the wide view. The first correction compared the scale with the offset of the coordinate. The water has other offsets (0.46 and 0.52) than the draws in the streets, so the correction did not apply. The water read the copy at 1.8 and 2.7 times the right scale. The correction now looks at the projection of the draw: a wide projection has the aspect of the screen times 1.8 / 2.7.
- The water does not read the picture at the place of the pixel. Its texture is the middle half of the frame before (the copy of 320x171 pixels from 160,85). Its coordinate has the scale 0.5 over the whole screen. So the water shows a rough mirror picture of the middle of the screen. In the flat game that picture is calm. In a headset the middle of the picture moves with the head, so the mirror picture swims on the water. Each eye also has another one.

The headset now reads such a map blurred (`title::kSoftScreenLookup`). `gx::lookup_own_place` tells the two kinds of lookup apart: a lookup at the place of the pixel has the scales and offsets 0.5. `soft_lookup_map` in `renderer/vulkan_renderer.cpp` gives the map of the other kind a sampler with a level bias of 5 (`UC_VR_SOFT_LOOKUP_BIAS`). `UC_VR_SOFT_LOOKUP=0` turns it off. In the stereo preview the water then is matte mud with soft ripples, as in the flat picture. The head of that test was 20 degrees to the side and 15 degrees down. Before, the water was glossy with bright stripes.

The wide view now applies only to a projection with the shape of the screen (`title::kWideViewScreenAspectOnly`). The game also calls `MTXPerspective` with 40 degrees and the aspect 1.0, which is not a view of the screen.

The pop-in in the distance: the cause is the view test of the game for the parts of the stage. These were checked first, and none was the cause:

- The eye cull. `UC_VR_CULL_TOGGLE=1` draws everything in each second frame. With a standing scene and the head turned 30 degrees, eight frames in a row were equal. So the cull takes away only what no eye sees.
- Shader compiles on the headset. In the bot run of the first chapter with the app 0.0.37 no pipeline was compiled after the warm-up.
- The depth of the eyes. The eye projection keeps the near and far planes of the game, and the depth format has 24 bits as on the console.
- A test of the game that hides sprites outside its own picture. `UC_SPRITE_HIST=1` counted the place of each world draw in a run of the first chapter, for each call site. Half of the sprites of the call site 8049DEC0 lie outside the wide picture. So the game does not cut the sprites at the edge of its own picture.

The cause: the game hides each part of the stage outside its own view cone.

- The loop at 8032DCE4 (one caller, the tail branch at 8032CD10) goes over the parts of a stage model. It makes a sphere from the box of the part (part data + 68 and + 80) and the matrix of the part.
- The function 80330C60 tests the sphere against the view cone at camera + 640: the near plane, the far plane and four side planes. For a sphere outside, 804D02FC sets the visibility of the part to 0.
- The function 8032F274 builds the cone from the camera of the game: the vertical angle in radians (at r3 + 36), the aspect, and the near and far planes. It takes half of the angle as the half height, without a tangent. So the cone is 9 % narrower than the picture of the console.
- The cone does not come from the projection matrix, so the wide view did not widen it. In the wide view the parts outside the cone are inside the picture.
- A part of the stage is large: a radius near 300 at a distance near 950. So whole houses came and went with each turn of the game camera.

The proof: a trial made the test fail for each sphere in a standing scene. 84 % of the picture changed. The houses and the walkways were gone, and the sky, the water, the enemies and the loose objects stayed. In a run of the first chapter to the water, the test hid a part 8762 times, in four models.

The fix: `DcStagePartsViewTest` (`game/intro.cpp`) takes the place of the loop at 8032DCE4 (`project/dc/hle_manual.txt`, and `entry 8032DCE8` in `project/dc/dc.cfg`). With a wide view it returns at once, and each part stays in the picture. Without a wide view it runs the loop of the game. After the fix a run to the same place had no hide from the test. `UC_DC_PARTS_VIEW_TEST=1` runs the test of the game in the wide view too. On the headset the app 0.0.39 played the first chapter with the bot (run `q39`): 70.7 fps as the mean, 99 % of the seconds at 60 or more, the lowest second 58, no crash. A person did not see the fix yet.

What stays open:

- An earlier trial made the cone itself as wide as the wide view (the input of 8032F274). The click on the character screen then failed, so other code reads the cone too. The fix does not change the cone.
- The view test has a second caller, 80058598. Its object is in `s20c1`, and nobody looked at which object it is. The list below tells what the port does for it.

A later look on 24 November 2026 closed three points:

- The second caller of the view test, 80058598. The function 800547D4 goes over a list of objects and calls 80058440 for each. That function makes a sphere around four points of the object (object + 2852). For a sphere outside the cone it skips the work for the object. `DcViewConeTest` (`game/intro.cpp`) now takes the place of the view test 80330C60 (`entry 80330C64` in `project/dc/dc.cfg`). With a wide view it returns 1 for each sphere, so the object stays in the picture. Without a wide view it runs the test of the game. The first call from another caller than the stage loop prints a line that starts with `[dc] the view test 80330C60`. Two runs of the first chapter with eight hits each did not print it. A headset run of `s20c1` printed it on 24 November 2026 (return to 8005859C).
- The callers 802E68A8 and 802E6B4C. They do not test the camera. The function 802E6868 finds one part by its name and gives it the visibility that its caller asks for. The function 802E69A0 does the same for a part and each part below it. The log of the run `pv/fx1` names the models. The first function changes props that break: `R302_coconuts_S_M`, `R301_rice_baketu_M` and `R301_fukuro_M`. The second changes the models of the menus at the start: `stage`, `chapter1` to `chapter8` and `CharacterA`. It also changes parts of the characters: `PKR1`, `EZS1`, `EZT1` and `EZU1`.
- The second see-through picture in the frame z1 of `pv/fx2`. It is the hit reaction of the game and not a fault of the port. The flat picture shows it too (`pv/hitm1`, the frame z4). A hit puts blood on the screen, and some hits add a see-through second picture or a blur from the middle. In the stereo preview with a moved head each eye shows these on its own picture (`pv/hits2`, `UC_VR_POS=0.2,-0.4,0.3`).

How to repeat the hit frames: `q3\build\reuc-build\pc_hit.sh NAME [YAW] [PITCH]` plays the first chapter with the bot. `UC_DC_FREEZE_ON_HIT=N` (with `UC_DC_GOD=1`) stops the game time at the Nth hit on a player. `UC_DC_FREEZE_HIT_DELAY` is the count of retraces from the hit to the stop (12). With `UC_DC_FREEZE_HIT_HOLD=<retraces>` the time runs again after that many retraces, and the next hit stops it again. `HIT_SHOTS=8` of the script takes a frame at each of eight stops, and `MODE=mono` gives the flat picture.

The check on the headset on 24 November 2026, with the bot in the first chapter and `UC_DC_GOD=1`:

| Run | App | Mean fps | Seconds at 60 or more | Lowest second | Crash reports |
|---|---|---|---|---|---|
| `q40` | 0.0.40 | 68.4 | 84 % | 50 | 0 |
| `q39b` | 0.0.39 | 67.9 | 81 % | 49 | 0 |
| `q39` (19 November) | 0.0.39 | 70.7 | 99 % | 58 | 0 |

The two builds are equal in runs of the same day. Both runs of this day are slower than the run of 19 November in the last minute of the chapter. The median GPU time of a frame in that minute was 11.8 ms (`q40`) and 13.4 ms (`q39b`). It was 10.4 ms on 19 November. On this day another VR app (`com.vrmin.app`) was open on the headset and used about 14 % of a CPU core. That is a likely cause, and it is not proven. Compare two builds only in runs of the same day.

New test aids: `UC_ARG_TRACE=<value>/<value>,<tolerance>` prints each function of the game that is entered with one of the values in f1 or f2. It prints a function once for each caller. It needs a build with `UC_TRACE`, which is the default of the PC build of this game. With `UC_ARG_TRACE_FILE=<file>` the trace starts at the moment that file appears. It found the callers of `MTXLightPerspective` and the view cone. `pc_water.sh` plays to the flooded place and takes frames there (the voice line `VO_3010309.brstm` is its marker, because runs differ in their pace). With `UC_POST_DUMP_FILE=<file>` the draw dump starts at the frame after that file appears. The `[post-class]` lines also give the viewport, the projection scales and whether the draw reads a copy. `UC_SPRITE_HIST=1` prints, each 1200 frames and for each call site, the count of world draws in steps of 0.05 of the half picture. `UC_DC_VIS_LOG=1` (with `UC_ARG_TRACE`) prints each change of the visibility of a model part with its caller. For a hide by the view test it also prints the radius and the distance of the sphere. In `pc_water.sh`, `WATER_END` and `WATER_TAIL` set the steps of the driver at the marker.

### The finer picture

Foveation is coarser shading away from the middle of the view. Level 3 shades each pixel to 25 degrees from the middle, blocks of 2x2 pixels to 36 degrees, and blocks of 4x4 pixels beyond. Level 1 has these limits at 35 and 50 degrees. Blocks of 4x4 then cover 16 % of an eye image at the render scale 2.0, and not 63 %.

From the app 0.0.34 the default of `SBDE08` on the headset is level 1 (`LG_XR_DEFAULT_FOVEATION` in `xr/xr_settings.h`). The settings version 15 moves a saved `foveation high` to that default once. A later choice on the Display page stays. The automatic render scale keeps its three levels (2.0, 1.75 and 1.5).

The numbers are from the first chapter on the headset with the bot. They start at second 20 of the chapter (`quest_look.sh`, `span_fps.py`):

| Foveation | Render scale | Mean fps | Seconds at 60 fps or more | Lowest second |
|---|---|---|---|---|
| Level 3, the old default | automatic | 69.9 | 99 % | 58 fps |
| Off | 2.0 | 57.9 | 54 % | 46 fps |
| Level 1 | 2.0 | 66.3 | 84 % | 52 fps |
| Level 1, the new default | automatic | 69.2 | 99 % | 56 fps |
| Level 1, the app 0.0.37 | automatic | 68.0 | 93 % | 55 fps |

With the automatic render scale the chapter went from 2.0 to 1.75 and to 1.5 in each run, and it stayed at 1.5. The app 0.0.37 draws the see-through draws of the world for each eye, which costs about 1 fps. Its run had no crash.

In the stills of the headset screen the wires and the edges of the roofs are steps of four pixels with level 3. With level 1 they are smooth in most of the view.

In the stills of the app 0.0.37 each person and each house is there once, also with the head 35 to 40 degrees up. A person did not look at the new default yet. Only a person can say that the flicker is gone.

### The wide view and the bot

With the wide view the pointer of the game covers 1.8 times the angle across and 2.7 times the angle down. The items of a menu that is a 3D scene are then nearer to the middle of the pointer range. Divide an old bot position by 1.8 and by 2.7. Leon on the character screen was at `aim:-0.27,-0.2`. He is at `aim:-0.15,-0.074` now. The 2D menus keep their positions. The pieces of `quest_chapter.sh` still have the old positions.

### Two states of the headset that look like faults of the app

- The pictures of the headset screen have no blue. On 17 November 2026 the stills of five runs had no blue at all (20:34 to 20:41 and 21:24 to 21:29). A system dialog in such a still had no blue too, so the app is not the cause. At that time the home program of the headset (Meta Horizon Shell) did not respond. After `adb shell am force-stop com.oculus.vrshell` the home program started again and the blue was back.
- The app does not start. While the home program does not respond, `am start -W` does not return and the app has no process. The same command helps.
- A controller that lies on the desk can hold a button down. On 17 November 2026 the left controller held its trigger, so the left hand was the gun hand in each test without a person.

### The sky of Umbrella Chronicles with a moved head

On 23 November 2026 a person reported a corrupt sky in the second chapter of `RBUE08` (script `s00c2`, the terrace of room `r028`). The report compared it with the water of `SBDE08`. The cause is another one.

- The stereo preview on the PC showed a clean sky, with the screen lookup and without it (`UC_VR_NO_SCREEN_LOOKUP=1`). The sky draws do not read a copy of the picture.
- The head of the preview only turned. `UC_VR_POS=x,y,z` now moves it from its start place, in metres (right, up, back). With `UC_VR_POS=0.2,-0.4,0.3` the sky was a low ceiling of small tiles over the terrace.
- The sky is the first five draws of the frame. They have the projection of the world and a depth test, and they write no depth. So everything after them covers them. The game can then use a small sheet close above its camera.
- The eyes of the headset move with the head from the place where the app started. An eye 40 cm lower is several times farther from that sheet, so it sees many repeats of the cloud texture. An eye above the sheet does not see the sky.

The fix is a draw class of its own, `LG_STEREO_BACKDROP` (`xr/stereo_scene.hpp`). Its eyes turn with the head and stay at the place of the camera of the game. So the sky is at an endless distance. It has no depth for the two eyes, and a moved head does not shift it. The renderer looks for the first world draw of the frame with a depth write. The world draws without a depth write before it get that class (`title::kFarBackdrop`, `renderer/vulkan_renderer.cpp`). With the fix the same run shows the cloud sky of the flat game. `UC_VR_NO_FAR_BACKDROP=1` turns the class off. `UC_POST_DUMP` prints which draws are the backdrop.

The switch is off for `SBDE08`. The sky of that game does not have the fault: the streets of the first chapter show their cloud sky in both eyes with `UC_VR_POS=0.2,-0.4,0.3` (`pv/skyd1`).

The check on the headset, with the fork build 0.1.6-fork7 on 23 November 2026:

- The save of the player had a save point in the second chapter, three rooms before the terrace. `q3\build\reuc-build\uc_quest_continue.py` answers the question "Continue the game from the save point?" and lets the bot play. The app works on a copy of the save.
- The bot played four minutes through the rooms `r026`, `r027`, `r028` and `r029`. The log has no crash report. The frames of the terrace show the cloud sky in both eyes, and `UC_POST_DUMP` names the first five draws as the backdrop.
- The headset lay on the desk, so its head did not move from its start place. The moved head is tested only in the preview on the PC.
- The frame rate without test output: 59.8 fps mean in `r027`, 59.6 in `r028` and 57.0 in `r029`. The session of the player with the build before (0.1.6-fork6) had 58.3, 60.0 and 56.8 in these rooms.

How to repeat it: `q3\build\reuc-build\uc_pc_autoplay.py` plays a chapter of `RBUE08` on the PC with the bot of `scripts/uctest`. It waits 15 minutes for the test pipe, and `scripts/uctest` waits 20 seconds. With `--all-classes` it aims at each enemy kind. It writes the room of each saved frame. The chapter card of the second chapter needs the aim point `--chapter-xy 0.15,-0.2` in the preview. The bot of `scripts/uctest` knows only the zombies of the train. Without `--all-classes` it stops at other enemies.

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
- Frame rate with the render scale 2.0: 69.9 fps in the first chapter, with 60 fps or more in 91 % of the seconds. It was 64.3 fps over the first three chapters and 62 fps over chapters six to eight. The fifth chapter ran at about 45 fps (`s10c4`). The cause is in "The slowdown in a long session" below.
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

The fifth chapter (`s10c4`) with the app 0.0.19: the bot played it to its result screen in 20 minutes at 69.8 fps on average. 95 % of the seconds had 60 fps or more, and no second was below 45 fps. The device made two modules in play. That run was a new session of the app. The 45 fps of the long run came from the length of the session (see "The slowdown in a long session" below).

The route to a chapter of "Memories of a Lost City" from a save, for `scripts/drive_quest.py`, has five steps:

1. Press A on the piece of the chapter.
2. Press A on YES in the checkpoint dialog (`aim:-0.16,0.115`).
3. Press A on Leon (`aim:0.1,0.07`).
4. Press A on NEXT (`aim:0.59,0.237`).
5. Wait for the script with `until:Data/script/s10c,A,120,6000`.

The piece of `s10c3` is at `aim:0.233,-0.237`, that of `s10c4` at `aim:0.033,0.068` and that of `s10c5` at `aim:0.325,0.045`. The piece of `s10c6` is at `aim:0.188,0.26`. If the save holds checkpoint data of another chapter, the dialog comes. YES deletes that data. The reticle in a headset frame is not the pointer of the game. Find the places of a menu in the PC build.

Space warp. With `refresh 90` and `space_warp on` in `vr.cfg` the app 0.0.19 ran the boss scene for three minutes without a fault. The game rendered 45 or 46 frames each second, and the headset reported application space warp at 90 Hz. The GPU took 14.3 ms a frame. Nobody looked at the frames that the headset makes between the frames of the game, so space warp stays off by default.

At each start 74 pipelines fail to compile with `VkResult -13`. The `RBUE08` app has 78 such failures. The log now names them (app 0.0.20). Each one is a small-draw ubershader of the families with one stage or two stages (`uber_merge.h`). The general small-draw ubershader compiles. So the draws lose only the shorter shader of their family. The cause in the driver is not known.

The slowdown in a long session, 17 November 2026. The long run with the app 0.0.20 played eight chapters in 3 h 10 min without a crash. Most chapters ran at 44 to 54 fps. A new session of the app ran the same chapters at about 70 fps. So the frame rate fell with the length of the session, and a short test did not show that.

The cause was in the render thread. Each of the 16 frame slots of `VulkanRenderer` kept a list of textures (`slot.textures`). The list named each texture whose pixels the texture cache of the game still held. For each packet, `render()` walked that list and built a lookup table from it. A frame has about 27 packets. The list grew with the session. After two hours the render thread took 23 ms a frame, against 14 ms in a new session. The renderer now keeps one set of these textures for all slots (`retained_textures_`, app 0.0.23).

The long run with the app 0.0.23 played the same eight chapters in 3 h 14 min without a crash. The table gives the mean frame rate of each chapter in both runs. Both runs had `UC_DC_GOD=1` and `bot:60000,6`. The seconds of the movies count too.

| Chapter | 0.0.20, mean fps | 0.0.23, mean fps | 0.0.23, seconds at 60 fps or more |
|---|---|---|---|
| `s30c1` | 69.5 | 70.6 | 95 % |
| `s10c1` | 50.5 | 66.0 | 78 % |
| `s10c2` | 64.7 | 72.3 | 100 % |
| `s10c3` | 44.4 | 69.8 | 99 % |
| `s10c4` | 47.8 | 72.1 | 100 % |
| `s10c5` | 54.4 | 65.5 | 70 % |
| `s10c6` | 44.1 | 66.0 | 83 % |
| `s10c7` | 49.3 | 71.8 | 100 % |

The app 0.0.23 also has the automatic render scale (see below), and it lowered the scale for most of that run. So the table shows both changes together.

How the cause was found:

- The GPU was not the limit. Under the sustained-high level of the headset the GPU of a Quest 3 runs at 545 MHz. It goes to 599 or 640 MHz only while it is about 85 % busy. The VrApi lines of logcat show the clock and the load (`GPU%`). In the slow state the GPU was 70 % busy at 545 MHz.
- `UC_GPU_LEVEL=boost` and the render scale 1.3 changed nothing in the slow state.
- The line `[perf]` of the log gives the time of the render thread for each second. It showed 23 ms a frame.
- `adb shell simpleperf record --app org.uc.darksidechronicles.vr -e cpu-clock -f 1000 -g --trace-offcpu --duration 10 -o /data/local/tmp/x.data` records all threads for 10 s. The switch `--trace-offcpu` also records the waits of a thread.
- `simpleperf report -i /data/local/tmp/x.data --tids N --sort symbol --children` lists the functions of one thread. The log line `[vulkan-xr] render thread N registered` gives the number of the render thread.
- `llvm-addr2line -i -f -C -e <build folder>/game/libmain.so <address>` of the NDK names the source line of an address.

If a headset run is slow, record such a profile first. Judge the frame rate from a long session, and compare the same moments of the game.

Scenes with many draws, 17 November 2026. With the app 0.0.23 the start of `s10c5` still ran at 50 to 54 fps for eight minutes. A frame has about 1,950 draws there, in 17 packets. The GPU was 65 % busy, and the render thread took 12 ms a frame. So neither was the limit.

A frame goes through three threads. The game thread runs the translated code of the game and writes its GX commands into a queue (`gx::CommandQueue`). The builder thread (`gx-builder`) makes the draws of a packet from the commands. The render thread records the packets for Vulkan. At the end of its frame the game thread asks for the draw-done. When the builder sealed the packet of the frame, the game thread goes on.

A profile of the game thread showed where its frame of 19.7 ms went:

- 8 ms were the code of the game.
- 6.7 ms were the wait for the builder (`gx::CommandQueue::wait_until`). The builder starts with the first draw of the frame and needs about 6 microseconds for a draw.
- 2.5 ms were the sleep of a late frame (`kTimedYieldMs`).
- 3 ms went to the other threads of the game.

The game thread kept the baton during its wait for the builder. The baton is the right to run the code of the game, and one thread holds it at a time. So no other thread of the game ran in that time, and the late frame then slept for them.

The builder now posts the draw-done itself after it sealed the packet (`kDrawDoneFromBuilder` in `common/title.h`, app 0.0.25). The game thread waits for it in `GXWaitDrawDone` without the baton, as on the console. The scheduler counts how long a thread was blocked (`Sched::take_blocked_ticks`). If the game thread was blocked for 2 ms or more in a frame, a late frame does not sleep. `UC_DRAW_DONE_AT_BUILD=1` gives the old way, and `RBUE08` keeps the old way.

The table gives the seconds 60 to 480 after the script of `s10c5` opened. Both runs had `UC_DC_GOD=1` and the step `bot` with 6 retraces.

| App | Mean fps | Median fps | Seconds at 60 fps or more | Baton time of the game thread a frame |
|---|---|---|---|---|
| 0.0.23 | 51.7 | 53 | 2 % | 15.8 ms |
| 0.0.27 | 66.1 | 70 | 81 % | 11.5 ms |

After the change the GPU limits most of that scene. At the render scale 2.0 it ran at 59.5 fps with 15.8 ms of GPU time a frame. The automatic render scale went to 1.5, and the scene then ran at 69 fps. In one minute of the scene the code of the game itself needs 16 ms a frame (15 million guest cycles). The frame rate falls to 52 fps there.

A span of `s10c6` stays at 61 fps with the app 0.0.28. It starts about six minutes into the chapter. A frame has about 970 draws there, in 27 packets. The GPU is 93 % busy at 640 MHz and takes 14.6 ms a frame. What was measured there:

- The render scale does not change the GPU time. The scene packet took 7.9 ms at the scale 2.0 and 6.6 ms at the scale 1.0. `UC_VR_SCALE_SWITCH=<frame>:<scale>,...` changed the scale at fixed frames for this test.
- The line `the vertex pre-pass of all packets` of `UC_GPU_PACKETS=1` gave 0.05 ms a frame for the vertex pre-pass. That line is wrong on this GPU: the pre-pass takes about 1.5 ms (see "The parts of the GPU time of a frame").
- Without the pre-pass (`UC_NO_XFORM=1`) the span ran at 42 fps, with 22.8 ms of GPU time a frame.
- The time of the scene packet follows its draws more than its triangles. A packet with 712 draws and 193,000 indices took 7.3 ms. A packet with 341 draws and 205,000 indices took 3.9 ms.

The cause of this GPU cost of a draw is not found. The piece of `s10c6` on the chapter screen is at `aim:0.188,0.26`, so a run can start in that chapter.

Two traps of these measurements:

- `UC_GPU_PACKETS=1` makes the render thread about 50 % slower in such a scene (18 ms a frame against 12 ms). It computes the screen box of each draw for its statistics. Do not read the time of the render thread or the frame rate from a run with it. The span of `s10c6` ran at 48 fps with it.
- The addresses of the installed `libmain.so` can differ from those of a later build. Keep the `libmain.so` of the build folder for each app that runs on the headset. Before you use `llvm-addr2line`, compare the address of one function in both files with `llvm-nm`.

The automatic render scale (`uc::AutoScale` in `renderer/auto_scale.h`, app 0.0.24). The GPU of the headset stays at 545 MHz while it is less than about 85 % busy. A frame that needs a little more than one refresh at 545 MHz misses each third refresh. The GPU then idles between the frames, so the clock does not rise. The headset therefore gives the picture fewer pixels while the GPU limits the frame rate:

- It has five levels: the chosen scale, and 0.875, 0.75, 0.625 and 0.5 of it. For the scale 2.0 these are 2.0, 1.75, 1.5, 1.25 and 1.0. The fourth level came on 24 November 2026 (see "The fourth level of the automatic render scale"). The fifth came on 25 November 2026 (see "The parts of the GPU time of a frame").
- Each second it reads the frame rate and the GPU time of a frame.
- A slow second has less than 93 % of the refresh rate and a GPU time above 78 % of the period. After two slow seconds it goes one level down.
- It then judges the step. If the step gave no more frames and the GPU time did not fall, the pixels were not the limit. It goes back, and the next try waits longer (60 s at first, 600 s at most).
- A good second is on time, and its GPU time with the pixels of the level above stays below 88 % of the period. After ten good seconds it goes one level up.
- A change of the scale costs one frame of about 50 ms.
- `UC_AUTO_SCALE=0` turns it off. `tests/test_auto_scale.cpp` runs the rule on recorded seconds.

The long run with the app 0.0.31, 17 November 2026. The app has all changes of this section and the automatic render scale. The run had `UC_DC_GOD=1` and `bot:60000,6`, as the runs above. It played the same eight chapters in 2 h 53 min without a crash. The table has the column of the app 0.0.20 again, which is the run before the changes of this section.

| Chapter | Seconds | 0.0.20, mean fps | 0.0.31, mean fps | 0.0.31, seconds at 60 fps or more | 0.0.31, lowest second |
|---|---|---|---|---|---|
| `s30c1` | 1,399 | 69.5 | 71.2 | 97 % | 42 fps |
| `s10c1` | 709 | 50.5 | 67.4 | 83 % | 42 fps |
| `s10c2` | 618 | 64.7 | 72.3 | 100 % | 63 fps |
| `s10c3` | 1,154 | 44.4 | 70.4 | 99 % | 56 fps |
| `s10c4` | 938 | 47.8 | 71.9 | 100 % | 61 fps |
| `s10c5` | 1,554 | 54.4 | 70.6 | 96 % | 37 fps |
| `s10c6` | 1,258 | 44.1 | 70.1 | 90 % | 50 fps |
| `s10c7` | 2,193 | 49.3 | 71.5 | 100 % | 38 fps |

What is left below 60 fps in this run:

- In `s10c1` the minutes with 1,000 to 1,200 draws a frame ran at 62 to 67 fps, with single seconds below 60 fps. No profile of them was recorded.
- In `s10c6` the span with 970 draws a frame ran at about 61 fps (see above).
- In `s10c5` one minute needs 16 ms of game code a frame (see above).

The render scale of this run: the app held the scale 2.0 for 4 to 100 % of a chapter. It held 1.75 or 1.5 for the rest. A first run of this app stopped after two hours in `s10c6`. The PC ran short of memory and ended the script that drives the run. Its chapters had the same frame rates within 1 fps.

The scripts that play the game from the PC leave their log readers behind on Windows (`adb logcat`, and the `tail` of `uc.log`). A reader of an old run then appends lines to the log of that run. Stop these readers at the end of each run.

### The rest of the game on the headset

Until 24 November 2026 the headset played `s30c1` and `s10c1` to `s10c8` only. The other 14 chapters ran on the PC only. This section is the work to play them on the headset.

How to play chapters in a row. `q3\build\reuc-build\quest_chain.sh NAME PC_SAVE "ROUTE" COUNT WEAPONS [ENV_LINE...]` copies a save of the PC to the headset and starts the app with the test server. It goes to the main menu, takes CHRONICLES and follows the route to the weapon screen. Then it plays COUNT chapters one after the other in one session of the app. Between two chapters it uses the step after a chapter from "The menus". After each chapter it pulls the save to `save_<script>_done` and writes one line to `chain.log`. It takes a picture of the headset screen each 60 s.

Three facts that the script needs:

- `adb push` cannot make folders inside the app folder of the headset, and the shell can. So the script pushes the save to `/data/local/tmp` and copies it with `cp -r` in the shell.
- The stage screen, the chapter screen and the character screen are 3D scenes. With the wide view of the headset the pointer covers 1.8 times the angle across and 2.7 times the angle up. Divide the aims of the flat picture by these two numbers. The stereo preview of the PC has the same wide view, so try a route there first.
- The first start after a session with new shader keys folds a module for each new key (see "Keys without a folded module"). After the first three chapters of this section that start took 3 minutes. The driver waits 120 s for the test channels of the app, so start the app once by hand before the run.

Routes that were used, as aims for the headset. Each one follows CHRONICLES at the main menu:

| Script | Save | Steps |
|---|---|---|
| `s30c2` | `save_s10c8_done` | `aim:0.2,0.04` (Operation Javier), `aim:-0.033,-0.1` (the chapter) |
| `s20c1` | `save_s30c2_done` | `aim:0.078,-0.119` (Game of Oblivion), `aim:0.028,0.0` (the chapter), `aim:-0.139,0.0` (the character at the left) |
| `s30c4` | `save_s30c3_done` | `aim:0.2,0.04`, `aim:0.078,0.0` |
| `s30c5` | the save after `s30c4` | `aim:0.2,0.04`, `aim:0.111,-0.152` |
| `s30c6` | `save_s30c5_done_good` | `aim:0.261,0.126` (the fourth piece), `aim:0.011,0.019` |
| `s10c8` | the headset save at the Tyrant (`quest2\soak4_save_b\soak4`) | `aim:-0.056,0.019` (Memories of a Lost City), `aim:-0.084,0.124`, YES at `aim:-0.16,0.115`, with `CHAIN_RESUME=1` |
| `s00c1` | the save after `s30c7` | `CHAIN_MAIN="aim:0.59,0.237 wait:40 press:A wait:300"` (SECRET in place of CHRONICLES), with `CHAIN_RESUME=1` |

The runs had `UC_DC_GOD=1 UC_DC_WEAK=1 UC_DC_ROCKET=1 UC_DC_BLOOD=1` and the weapons `UP/LEFT/DOWN/-/-/-/-`. The first run (`quest2\g3`, app 0.0.40) played `s30c2` and `s20c1` and was stopped in `s20c2` because of the black picture (see below). The second run (`quest2\g4`, app 0.0.41) played six chapters in a row in 1 h 40 min. Its bot then stood in `s20c6` (see "The bot in the wide view"). The third run (`quest2\h2`, app 0.0.42) went on from the checkpoint of `s20c6` and played to the end of `s30c3`. Four more runs of that day (`quest2\h3` to `quest2\h6`, app 0.0.42) played `s30c4` to `s30c7`. They had no `UC_DC_WEAK`, apart from the first part of `s30c6`. No run had a crash. Two runs had the app 0.0.43. The first played the fights at the end of `s10c8` from the checkpoint at the Tyrant (`quest2\h7`). The second played the extra chapter `s00c1` (`quest2\h8`). So each chapter of the game ran on the headset to its result screen.

| Script | App | Seconds | Mean fps | Seconds at 60 fps or more | Lowest second |
|---|---|---|---|---|---|
| `s30c2` | 0.0.41 | 623 | 67.2 | 85 % | 44 fps |
| `s20c1` | 0.0.41 | 794 | 62.6 | 64 % | 42 fps |
| `s20c1` | 0.0.42 | 754 | 65.3 | 78 % | 46 fps |
| `s20c2` | 0.0.41 | 587 | 65.9 | 80 % | 44 fps |
| `s20c3` | 0.0.41 | 1,444 | 66.6 | 84 % | 27 fps |
| `s20c4` | 0.0.41 | 853 | 65.7 | 81 % | 27 fps |
| `s20c5` | 0.0.41 | 720 | 63.0 | 66 % | 23 fps |
| `s20c6`, to the mine room | 0.0.41 | 1,790 | 61.4 | 50 % | 46 fps |
| `s20c6`, from the checkpoint | 0.0.42 | 1,189 | 69.5 | 99 % | 25 fps |
| `s20c7` | 0.0.42 | 1,930 | 58.5 | 42 % | 24 fps |
| `s30c3` | 0.0.42 | 798 | 70.3 | 95 % | 45 fps |
| `s30c4` | 0.0.42 | 1,180 | 63.7 | 72 % | 27 fps |
| `s30c5` | 0.0.42 | 1,206 | 62.4 | 71 % | 43 fps |
| `s30c6`, to the boss | 0.0.42 | 1,821 | 68.1 | 86 % | 30 fps |
| `s30c6`, the boss | 0.0.42 | 267 | 71.2 | 100 % | 50 fps |
| `s30c7` | 0.0.42 | 1,793 | 62.4 | 76 % | 42 fps |
| `s10c8`, from the Tyrant | 0.0.43 | 587 | 62.7 | 66 % | 37 fps |
| `s00c1` | 0.0.43 | 736 | 62.0 | 64 % | 29 fps |
| `s30c4` again | 0.0.48 | 1,189 | 67.0 | 83 % | 37 fps |
| `s30c4` again, with the one-draw blend (off now) | 0.0.50 | 1,203 | 68.4 | 89 % | 4 fps |
| `s20c7` again, with the one-draw blend (off now) | 0.0.50 | 1,139 | 63.6 | 69 % | 24 fps |
| `s20c1` again | 0.0.51 | 707 | 68.0 | 89 % | 48 fps |
| `s20c2` again | 0.0.51 | 548 | 71.2 | 98 % | 53 fps |
| `s20c3` again | 0.0.51 | 1,384 | 70.9 | 98 % | 46 fps |
| `s20c4` again | 0.0.51 | 826 | 70.4 | 96 % | 50 fps |
| `s20c5` again | 0.0.51 | 899 | 69.4 | 96 % | 43 fps |
| `s20c7` again | 0.0.51 | 3,385 | 67.5 | 93 % | 43 fps |
| `s20c1` again | 0.0.53 | 712 | 67.7 | 90 % | 45 fps |
| `s20c2` again | 0.0.53 | 556 | 71.3 | 99 % | 56 fps |
| `s20c3` again | 0.0.53 | 1,401 | 71.3 | 99 % | 46 fps |
| `s20c4` again | 0.0.53 | 837 | 71.2 | 99 % | 44 fps |
| `s20c5` again | 0.0.53 | 786 | 70.3 | 96 % | 44 fps |
| `s20c1` again | 0.0.54 | 749 | 70.1 | 98 % | 52 fps |
| `s30c4` again | 0.0.54 | 1,460 | 65.5 | 73 % | 31 fps |
| `s20c1` again | 0.0.55 | 750 | 69.9 | 97 % | 51 fps |
| `s20c2` again | 0.0.55 | 528 | 71.4 | 99 % | 53 fps |
| `s20c3` again | 0.0.55 | 1,419 | 71.2 | 98 % | 47 fps |
| `s20c4` again | 0.0.55 | 852 | 71.0 | 97 % | 44 fps |
| `s20c5` again | 0.0.55 | 724 | 69.6 | 95 % | 43 fps |
| `s20c1`, battery limits | 0.0.57 | 723 | 68.3 | 89 % | 49 fps |
| `s20c2`, battery limits | 0.0.57 | 541 | 70.4 | 94 % | 47 fps |
| `s20c3`, battery limits | 0.0.57 | 1,382 | 71.1 | 99 % | 49 fps |
| `s20c4`, battery limits | 0.0.57 | 832 | 71.0 | 97 % | 43 fps |
| `s20c5`, battery limits | 0.0.57 | 811 | 69.4 | 95 % | 44 fps |

The five rows with "battery limits" ran with the clock limits of a headset on battery (section "The clock limits of a headset on battery"). All other rows ran on a charger.

The three rows of `s20c7` do not hold the same seconds, because the bot needed a different time for the last boss in each run. The fight took 1,467 s with the app 0.0.42, 675 s with the app 0.0.50 and 2,900 s with the app 0.0.51 (`quest2\b51`). In the fight the mean was 56.1 fps with the app 0.0.42 and 67.2 fps with the app 0.0.51. In the part before the first boss the mean was 62.6 fps and 68.1 fps. The run `quest2\b51` had no GPU hang and no crash. `q3\build\darkside\room_seconds.py RUN\run.log` prints the seconds and the frame rate of each part of a chapter.

The seconds of the movies and of the loading count too. The app 0.0.40 had no folded module for 323 keys of the first chapters. The app 0.0.41 had none for 405 keys of the later ones. Their draws took the ubershader for a time. The keys are in `project/dc/pipeline_keys.quest` now. The app 0.0.42 has 1,359 keys. The app 0.0.43 was built for 1,699 keys, with those of `s20c7` and of the chapters of Operation Javier. Its package still holds the 1,359 keys of 0.0.42 (see the traps of "The parts of the GPU time of a frame"). The headset folds the other keys itself at a start. The list has 1,772 keys after the runs of `s10c8` and `s00c1`. The app 0.0.44 is the first with all of them in its package. With the keys in the build the app starts in 4 to 5 s.

#### The black picture after a change of the render scale

In `s20c1` the pictures of the headset screen were black for 8 minutes, from room R203 to the end of the chapter. They had a few small specks of colour. The game went on, the frame rate was normal, and the log had no fault line. The stereo preview of the PC showed the same rooms without a fault.

The cause. The screen pass of the headset (`XrCompositor::record` in `renderer/xr_compositor.cpp`) keeps a descriptor set for each group of image views that it saw. A descriptor set is the list of pictures that a shader reads. The pass finds a kept set by the handles of the views. When the render scale changes, the renderer destroys the frame buffer (EFB) and makes a new one. The driver can give the new view the handle of a destroyed view. The pass then takes a kept set that still names the destroyed frame buffer, and the eyes show its freed memory. The picture stays black until the next change of the scale gives the view another handle. The automatic render scale changes the scale about ten times in a chapter, so the fault came with it (app 0.0.24). A change of the Render scale in the settings panel goes the same way, in both games.

The fix. `XrCompositor::forget_views` drops the kept sets. The renderer calls it before it destroys the frame buffer (`renderer/vulkan_renderer.cpp`). It also calls it before it destroys the images of a HUD layer or a glow layer. The GPU is idle at these places.

How it was found:

- The black time started at the log line of one change of the scale, from 1.75 to 1.5. It ended at the line of the next change. Eight other changes of that run had no fault.
- `q3\build\reuc-build\quest_scale.sh NAME [COUNT] [STEP]` plays the first chapter and forces a change of the scale each STEP presented frames (`UC_VR_SCALE_SWITCH`, with `UC_AUTO_SCALE=0`). `scalewatch.py` takes pictures of the headset screen all the time, and `scale_report.py` counts the changes after which each picture was dark.
- With the app 0.0.40, 2 of 31 changes with pictures were dark (`quest2\sc2`). One of them stayed dark for 4 s, until the next change.
- With the app 0.0.41, none of 53 changes with pictures was dark (`quest2\sc41a`).
- The same forced changes in the stereo preview of the PC gave no dark frame in 63 changes. The preview does not use `XrCompositor`.

A picture of the headset screen takes 1 to 7 s, so only a part of the changes has a picture. `env.txt` now takes lines of up to 16,383 characters (`game/android_main.cpp`). Before, a line ended after 511 characters, which is 46 changes.

Lesson: the long runs of 16 and 17 November measured the frame rate and the crashes only. A black picture does not show in these numbers. Take pictures of the headset screen in each long run. `blackwatch.py` does that, and it prints a line for each black picture.

#### The fourth level of the automatic render scale

In `s20c1` the automatic render scale stayed at its lowest level (1.5) for the whole chapter. The GPU took 13.3 ms a frame at the median there, and 14.9 ms in the seconds below 60 fps. The GPU time followed the pixels: 18.9 ms at the scale 2.0 and 15.5 ms at 1.75. So the rule has a fourth level since the app 0.0.42: 0.625 of the chosen scale, which is 1.25 for the scale 2.0 (`renderer/auto_scale.h`). With it the chapter had 78 % of its seconds at 60 fps or more, against 64 %. In the minutes at 1.25 the GPU took 11.4 ms at the median.

#### A scene below 60 fps

The last boss of `s20c7` ran at 53 to 55 fps for about 10 minutes (`quest2\h2`). The GPU took 15.5 ms a frame at the scale 1.25, at 640 MHz. A frame has about 1,150 draws and 16 EFB copies there. The step from 1.5 to 1.25 did not make the GPU time shorter, so the pixels are not the limit. Only four new keys came in that fight, so the ubershader is not the cause. This is the same kind of scene as the span of `s10c6` (see "Scenes with many draws"). The cause of the GPU cost of a draw is still not found. The start of `s30c4` is such a scene too: the GPU is 97 % busy at 640 MHz and takes 14.6 ms a frame at the scale 1.25.

One setting was tried on the start of `s30c4`: the limit of the small-draw ubershader (`UC_UBER_PX`, 1,024 guest pixels by default). `q3\build\reuc-build\quest_px.sh NAME [ENV_LINE...]` plays that scene with the bot at the fixed scale 1.25. `q3\build\darkside\window_gpu.py` reads the frame rate and the GPU time from the VrApi lines. Each run had about 155 s after the first minute, with the app 0.0.43:

| Limit | Mean fps | Seconds at 60 fps or more | GPU time of a frame, median | Draws a frame |
|---|---|---|---|---|
| 1,024 (default) | 65.0 | 94 % | 13.5 ms | 783 |
| 4,096 | 61.5 | 65 % | 13.9 ms | 847 |
| 256 | 62.0 | 65 % | 13.7 ms | 854 |

The GPU time is the same within 3 %, and no limit is better than the default. So this limit is not the cause. The runs differ in their draws, because the bot does not play the same way twice.

Where the GPU time goes in that scene. A run with `UC_GPU_PACKETS=1` (`quest2\pk1`, scale 1.25, 46 fps with the switch) gave these times for one frame:

| Packets of a frame | GPU time |
|---|---|
| The main scene packet: 532 draws, 170,000 indices, 19 pipelines, 109 pipeline changes | 7.6 ms |
| The packets with 50 to 199 draws and a copy of 640x342 | 3.0 ms |
| The present | 1.9 ms |
| The packet with 50 to 199 draws and no copy | 1.5 ms |
| All other packets | about 3.7 ms |

A frame has 21 passes on the frame buffer and 21 EFB copies. The passes load and store 68 million attachment pixels a frame. Almost every draw of the main scene packet has destination alpha, and the renderer draws these in one draw each (`fused_destination_alpha`). The small-draw ubershader held 3,125 draws in 514 runs in 2 s, and 10,321 big draws kept their own pipeline. The next subsection takes these costs away one at a time.

A trap of the log. The line `[sched]` gives the time for which each thread of the game held the baton. If a thread blocks and no other thread is ready, that thread runs the idle wait of the scheduler (`Sched::idle_wait`). The wait counts for it. So 400 ms a second for thread 5 in a slow scene is not work. That thread ran only 46 million guest cycles a second there. `q3\build\darkside\thread_times.py` prints these times for each chapter of a log.

#### The parts of the GPU time of a frame

The test aid `UC_PROBE_FILE=<file>` splits the GPU time of a frame (`renderer_probes` in `renderer/graphics_effects.h`, 25 November 2026). A probe is on while the file `<file>.<name>` exists. The renderer looks at the files twice a second. Most probes draw a wrong picture. The line `[probe]` of the log gives the passes, the runs of draws, the pipeline binds and the second alpha draws of 2 s. The line `[probe-uploads]` gives the texture uploads.

| Probe | What it takes away |
|---|---|
| `empty` | Every draw of the passes on the frame buffer. The passes, the copies, the vertex pre-pass and the screen pass stay. |
| `noblend`, `noopaque` | The draws with blending, or the draws without. |
| `tiny` | The fragments. The scissor of each draw is one pixel. |
| `zfail` | The fragment shading. Each draw fails the depth test, and the GPU still walks the triangles in each bin. |
| `alpha2` | The second draw for the destination alpha of a blending draw. With `join2` such a draw can join the texture table and the ubershader. |
| `sort` | Nothing. The runs of a pass go to the GPU in the order of their pipelines. |
| `s100`, `s150`, `s200` | Nothing. They hold the render scale at 1.0, 1.5 or 2.0. |
| `nomip`, `nocopymip` | The generated levels of an uploaded texture, or the levels of an EFB copy. |
| `nodispatch` | The compute dispatch of the vertex pre-pass. |
| `nofxaa`, `nosharp` | The edge smoothing or the sharpening of the screen pass. |
| `nodual` | Nothing. A blending draw with destination alpha is two draws again (see below). The picture stays right. |

Warning: Use `nodispatch` only together with `empty`. Without `empty` the draws read vertices that no dispatch wrote, the GPU device is lost (`VkResult -4`), and the app ends.

The method. `q3\build\reuc-build\quest_probe.sh NAME [SECONDS] [ENV_LINE...]` plays the start of `s30c4` with the bot at the fixed scale 1.25. After 100 s it stops the game time with `UC_DC_FREEZE_FILE`. Then each window of 14 to 20 s has one set of probes (the variable `PROBE_PLAN`). `q3\build\darkside\probe_report.py` prints the frame rate and the GPU time of each window from the VrApi lines. Its column "GPU work" is the GPU time at 640 MHz: the time multiplied by the clock and divided by 640. The windows without a probe gave the same GPU time within 1 %: 13.13, 13.11, 13.19 and 13.09 ms in one run. The standing picture took the same GPU time as the 20 s of play before it.

One standing frame at the scale 1.25 (`quest2\pb2`, 277 runs of draws, 10.3 ms):

| Part of the frame | GPU work |
|---|---|
| No draw at all (`empty`) | 3.9 ms |
| The vertices and the state of the draws (`tiny` less `empty`) | 1.4 ms |
| The walk of the triangles in the bins (`zfail` less `tiny`) | 1.9 ms |
| The fragment shading (the whole frame less `zfail`) | 3.1 ms |

The same frame took 8.2 ms at the scale 1.0 and 15.2 ms at 2.0. So the fragment part follows the pixels, and the other parts follow them little.

What the frame without draws holds (`quest2\pb3`, `quest2\pb6`):

- The compute dispatch of the vertex pre-pass takes 1.45 ms a frame. The frame without draws took 3.6 ms, and 2.2 ms with `nodispatch`. The two scene packets took 1.0 and 0.7 ms with no draw in them.
- The screen pass (the packet `present`) takes 1.9 ms for the two eye images of 1346x1440 pixels. The edge smoothing and the sharpening are off by default, and their probes changed nothing.
- All other passes and copies take about 0.3 ms together.

Other results (`quest2\pb1`, a frame of 13.1 ms with 416 runs, and `quest2\pb4`):

- The second draw for the destination alpha costs 1.3 ms a frame, for 132 such draws. `join2` gave no more.
- The order of the draws is not a cost. With `alpha2`, `sort` cut the pipeline binds from 127 to 68 a frame and saved 0.1 ms.
- The texture uploads are not a cost. A frame uploads 7 to 8 textures of 4x4 or 8x8 texels, with 19 blits for their levels. `nomip` changed nothing.
- The draws with blending cost 5.3 ms of that frame, and the draws without blending cost 4.9 ms.
- `UC_GPU_LEVEL=boost` does not hold the GPU clock (`quest2\pf2`). The level of the GPU stayed 4, and the clock was at 545 MHz for 136 s of that run.

Two changes came from these numbers (app 0.0.48):

- The automatic render scale has a fifth level: 0.5 of the chosen scale, which is 1.0 for the scale 2.0 (`renderer/auto_scale.h`).
- The screen pass fills only a part of the eye images while the picture is smaller than they are (`XrCompositor::set_shown_pixels`). The part has the share of the width of the picture, in blocks of 8 pixels: 848x904 at the scale 1.25 and 680x720 at 1.0. `end_frame` gives the runtime that part as `subImage.imageRect`. The foveation map of the eye images is for the whole images, so a part does not use it. The frame without draws then took 3.1 ms at 1.25 and 2.6 ms at 1.0 (`quest2\pb7`). `UC_XR_FULL_EYES=1` fills the whole images, as before.

The whole chapter `s30c4` with the app 0.0.48 (`quest2\c48`): 1,189 s with a mean of 67.0 fps, 83 % of the seconds at 60 fps or more, and no crash. The lowest second had 37 fps. The app 0.0.42 had 63.7 fps and 72 % in that chapter.

The blending draws with destination alpha in one draw (app 0.0.49, 25 November 2026). This path is off by default since the app 0.0.51, because the GPU hangs with it now and then (see the end of this list):

- The renderer drew such a draw twice: the colour with the computed alpha, then the constant alpha alone. The headset now draws it once with dual-source blending (the device feature `dualSrcBlend`). The fragment stage writes the colour with the constant alpha, and the computed alpha as the second input of the blend. The colour factors read that second input (`VK_BLEND_FACTOR_SRC1_ALPHA`), and the alpha factors are one and zero.
- The key word 67 is 2 for such a pipeline (`pipeline_key` with `dual`). Its modules come from the define `DUAL_ALPHA` of `smoke.slang`. These modules have no `transmit` output, because a module with a second blend input must not write location 1. The build makes four more base modules (`smoke.stereo.dual.frag.spv` and three others). `tools/fold_tev_shaders.py` and the fold on the device take them for a key with that word (variant 4 to 7).
- When the pipeline is compiled, `VulkanRenderer::dual_pipeline` gives it. Until then the draw keeps its two draws, and the key goes to the compile threads and to the key list. So a new key costs no wrong frame. A draw of the HUD layer, of the particle layer or with a logic operation keeps its two draws. The ubershaders have no such form.
- `UC_DUAL_ALPHA=1` turns it on, on the headset and in the stereo preview of the PC. The probe `nodual` turns it off again while the game runs. The PC path compiles, and nobody ran it yet.
- One standing frame, both ways in turn (`quest2\pb9`, scale 1.25): 10.0 ms of GPU work with the one draw, and 11.3 to 11.4 ms with the two draws. The frame had 105 second draws. The pipeline binds fell from 273 to 113 a frame, and the runs from 405 to 390. The pictures of the two ways look the same.
- The first run folded 64 modules on the headset, in 30 s of compile thread time (`quest2\pb8`). The key list has their 51 keys now (1,823 keys, app 0.0.50).
- The whole chapter `s30c4` with the app 0.0.50 (`quest2\c50`): 1,203 s with a mean of 68.4 fps, 89 % of the seconds at 60 fps or more, and no crash. One second had 4 fps: two frames of the game took 375 ms each there, with a read from the disc.
- The chapter `s20c7` with the app 0.0.50 (`quest2\b50`): 1,139 s with a mean of 63.6 fps, 69 % of the seconds at 60 fps or more, and no crash. The app 0.0.42 had 58.5 fps and 42 % there. The last 290 s, with the last boss, had 67.1 fps and 91 %. That boss ran at 53 to 55 fps before.
- The fault. A run of five chapters with the app 0.0.50 ended in `s20c3` after 53 s (`quest2\r50`): the kernel log of the headset had `GPU hang detected` three times in 2 s, and the app ended with `submit frame failed: VkResult -4`. A second run of that chapter ended the same way after 242 s (`quest2\x1`). With `UC_NO_DUAL_ALPHA=1` the chapter ran for 519 s without a fault (`quest2\x2`).
- One GPU hang shows in the log as a frame of 350 to 399 ms (a line `[hitch]`). Three hangs in 2 s end the app. Each run with the one draw has such frames: 10 in `quest2\c50`, 8 in `quest2\b50`, 26 in `quest2\r50`, 7 in `quest2\x1`. No run without it has one (`quest2\c48`, `x2`, `lv5`, `pf1`). So the one second with 4 fps of `quest2\c50` was two such hangs, not a slow read from the disc.
- The hangs are spread over a chapter. They do not follow a change of the render scale at once, and they came at the scales 1.25, 1.5 and 1.75. Without the coarse shading and the foveation map (`UC_NO_VRS=1 UC_NO_FOVEATION=1`) a run still had 7 of them (`quest2\x3`). The standing frames of the probe runs had none. The cause is not found. `q3\build\reuc-build\quest_s20c3.sh NAME [ENV_LINE...]` plays that chapter for a test.
- So the numbers of `quest2\c50` and `quest2\b50` hold for a path that is off now. The gain of the one draw was 1.4 fps in `s30c4` (68.4 against 67.0 fps).
- The app 0.0.51 has the path off. It played `s20c3` for 512 s at 71.3 fps with no such frame and no crash (`quest2\x4`). Then it played the five chapters `s20c1` to `s20c5` in a row in 1 h 19 min (`quest2\r51`). That run had no such frame and no crash. The table of chapter frame rates has its rows.

What limits the slowest seconds now. Three live runs of the start of `s30c4` with the automatic scale reached the scale 1.0 (`quest2\lv5`, `pf1`, `pf2`). They still had 40 to 60 % of their seconds below 60 fps, with a mean of 60 to 62 fps. In the seconds below 60 fps a frame has 890 to 1,280 draws:

- The GPU is 70 to 84 % busy, and its clock falls to 545 or 599 MHz. The headset raises the clock only above about 85 %. So less GPU work gives a lower clock first, and the GPU time of a frame stays near 13 ms.
- A CPU profile with the waits shows the render thread on a core for about 75 % of the time (`q3\build\reuc-build\quest_prof.sh`, `quest2\pf1`). That is about 14 ms a frame at 52 fps. The code of the port takes 35 % of the thread, the Vulkan driver 21 % and `memcpy` 7 %. For 14 % of the time the thread waits for a packet.
- The main thread of the game waits for 55 % of its time, and the builder thread for 57 %.
- So the render thread and the GPU each need 13 to 14 ms for such a frame, and their work overlaps only in part.

Next steps, by expected gain:

- Cut the work of the render thread for each packet. `vkQueueSubmit2`, `vkResetCommandBuffer`, `vkCmdBeginRendering` and the fences took about 15 % of the thread. A frame has 21 packets, and each one is a submission of its own.
- Give the vertex pre-pass no vertices of draws that the cull test removes.

Traps of these measurements:

- While `UC_DC_FREEZE_FILE` is on, the game draws a wall of noise with the text "Loading data" over the scene on the headset. It is a blending overlay: with the probe `noblend` the scene shows. The GPU time of the frame does not change. Nobody looked at this on the PC yet.
- The value `render` of the line `[perf]` holds waits of the render thread. It stayed near 13 ms a frame with the probe `empty`. Take the CPU time of that thread from a profile.
- When the build links `libmain.so` again, it copies the key list and the folded modules into the package (`POST_BUILD` in `game/CMakeLists.txt`). It does not copy them at any other time. A build after a change of the key list alone packs the old keys and modules.

#### The waits of the game thread in slow seconds

`q3\build\darkside\low_cause.py RUN\run.log` sorts the seconds of a run below 60 fps by what the game thread and the render loop did in them. The run of five chapters with the app 0.0.51 (`quest2\r51`) had 197 such seconds of 4,364. The run of `s30c4` with the app 0.0.48 (`quest2\c48`) had 204 of 1,189.

- In `s30c4` the GPU is the limit in all of them. The game thread uses under 500 ms of CPU a second and waits 490 to 600 ms for the renderer. The render loop finds no free GPU slot 300 to 500 times a second.
- In `s20c1` to `s20c5` the game thread is the limit in 81 of the 197 seconds. It uses 600 to 860 ms of CPU a second there, and a present has 1,400 to 1,650 draws.

A CPU profile with the waits gave the parts of the time of the game thread in `s20c1` (`q3\build\reuc-build\quest_prof_s20c1.sh`, `quest2\gp1`, app 0.0.51):

| Part of the time of the game thread | At 340 s | At 545 s |
|---|---|---|
| On a core | 63 % | 59 % |
| The frame wait (`hle_DcFrameWait`) | 16 % | 13 % |
| A more urgent guest thread has the baton (`Sched::safe_point`) | 18 % | 20 % |
| The wait for the renderer | 4 % | 9 % |

The frame wait is `timed_wait` in `game/game_pacing.cpp`. It has two changes:

- The sleep of a late frame. In a late frame the game thread slept 2 ms, so that the guest threads of lower priority got time (`kTimedYieldMs`). Those threads use under 10 ms of CPU a second in the fights (`q3\build\darkside\thread_ms.py RUN\run.log cpu`). When no other thread is ready and no disc read is in flight, the sleep now ends at once (`Sched::donate_ticks`, `Sched::load_begin`). `UC_FULL_YIELD=1` gives the full sleep. The deterministic mode keeps the full sleep.
- The deadline after a late frame. The deadline of a frame is the deadline before it plus one period. When the clock is past that time, the deadline is the clock. So a late frame moves all later deadlines, and a fast frame after it waits for its slot. In `s20c1` the game thread waited 210 ms a second in the frame wait at a mean of 69 fps. The test variable `UC_PACE_SLACK_MS=N` lets the deadline stay behind the clock by N ms. The game then makes up a delay of that size. The variable is off by default.

Results for the first 683 s of `s20c1` (`q3\build\darkside\bins_compare.py CHAPTER RUN\run.log ...`):

| Run | App | Mean fps | Seconds below 60 fps | CPU of the game thread | Frame wait | Frame latency |
|---|---|---|---|---|---|---|
| `quest2\r51` | 0.0.51 | 68.4 | 61 | 663 ms a second | 210 ms a second | not taken |
| `quest2\gp1` | 0.0.51 | 69.0 | 37 | 667 ms a second | 212 ms a second | 20.7 ms |
| `quest2\d52` | 0.0.53 | 69.2 | 46 | 686 ms a second | 185 ms a second | 21.7 ms |
| `quest2\ps14` | 0.0.53 with `UC_PACE_SLACK_MS=14` | 69.8 | 24 | 623 ms a second | 11 ms a second | 27.0 ms |
| `quest2\ps5` | 0.0.53 with `UC_PACE_SLACK_MS=5` | 69.8 | 40 | 670 ms a second | 92 ms a second | 24.5 ms |
| `quest2\r53` | 0.0.53 | 67.8 | 68 | 675 ms a second | 126 ms a second | not taken |

- The new sleep alone changes the frame rate by less than the spread between two runs of the same app. Each of the four chapter runs with it loaded its data and came to its result screen.
- With a slack of 14 ms the chapter had the fewest seconds below 60 fps. The frame latency of the log line `[guest-blocked]` rose from 21 ms to 27 ms. The game then works ahead of the display by up to one period.
- In `s30c4` the slack gave nothing. The run with it had 65.8 fps, and 75 % of its 1,441 s were at 60 fps or more (`quest2\cs14`). The run `quest2\c48` had 67.0 fps and 83 % of 1,189 s. Without the slack the frame wait is 2 to 12 ms a second in the slow seconds of that chapter. The bot needed 252 s more in the run with the slack, so the two runs do not hold the same seconds.
- The app 0.0.53 played the five chapters `s20c1` to `s20c5` in a row with no crash and no GPU hang (`quest2\r53`). The table of chapter frame rates has its rows. Its `s20c1` had 68 seconds below 60 fps, the most of all runs.
- `UC_SCHED_SPIN_US=200` lets a guest thread spin for the baton before it sleeps. It gave nothing: 68.9 fps and 45 seconds below 60 fps with the new sleep (`quest2\sp1`).

Open points:

- A slack of 5 ms gave 40 seconds below 60 fps, with a frame latency of 24.5 ms (`quest2\ps5`). The runs without a slack had 37 to 68 such seconds then. So one run for each value decides nothing.
- `q3\build\reuc-build\ab_slack.sh` played the chapter four times in a row, two times with a slack of 14 ms. In the first 695 s the plays without a slack had 38 and 14 seconds below 60 fps (`quest2\ab_n1`, `quest2\ab_n2`). The plays with the slack had 30 and 48 (`quest2\ab_s1`, `quest2\ab_s2`). The frame latency was 22.8 and 21.4 ms without the slack, and 26.9 and 27.0 ms with it. So the slack gives no gain. It stays off, and `UC_PACE_SLACK_MS` stays a test variable.
- The guest thread of priority 3 (entry 804FDCA8) is the sound thread of the game. The sound frame of the console wakes it each 3 ms (`hle_AXFrame` in `runtime/ax_audio.cpp`). It uses 45 to 56 ms of CPU a second. A profile shows the game thread off the baton for 18 to 20 % of its time. That value is too high, because the profiler makes each thread switch slower. `q3\build\darkside\baton_seconds.py RUN\run.log` takes the values from the log lines. In the slow seconds with a busy game thread, the game thread uses 690 to 720 ms of CPU a second. The other guest threads use 61 to 67 ms. About 1,000 switches a second take 36 to 53 ms of wake time. The game thread waits 72 to 160 ms a second for the renderer.

Traps of these values:

- The value `main loop waited` of the line `[guest-blocked]` is the wait of the render loop of the host. The frame wait of the game is the value `wait` of the line `[game-pacing]`, which is a total.
- The values `run ms` of the line `[sched]` hold the idle time of the scheduler after a thread blocked. The line `[sched-cycles]` has the CPU time of each thread. On Android its unit is the millisecond.

#### The render thread in slow seconds

In the slow seconds of `s20c1` to `s20c5` the game thread waits 72 to 160 ms a second for the renderer. A profile at 640 s of `s20c1` gives the time of the render thread (`quest2\ps14`, app 0.0.53). The thread is on a core for about 93 % of it. So the render thread limits these seconds together with the game thread.

Three scripts in `q3\build\darkside` give the time of a thread by source line, on the PC:

- `pull_prof.sh NAME_AT` copies a profile of `quest_prof_s20c1.sh` from the headset to `D:\reuc-tools\prof`.
- `prof_samples.sh NAME_AT` writes the samples with their call chains as text. It needs the library with symbols in `D:\reuc-tools\prof\symfs`, at the path that the app has on the headset.
- `prof_own.py SAMPLES TID [TOP] [FILTER]` gives each sample to the nearest source line of the port in its call chain. So the time of `memcpy`, of the allocator and of the driver counts for the line that called them.

The parts of the time of the render thread in that profile:

| Part | Share of the thread |
|---|---|
| The cull test of the draws (`vr_cull.h` and the transform of the points) | about 8 % |
| The free of the rendered packet, with its draws and their shared blocks | 5.7 % |
| `vkEndCommandBuffer` and `vkResetFences` | 3.7 % |
| The copy of the texture banks (6,048 bytes each) | 3.6 % |
| `vkQueueSubmit2` | 3.3 % |
| The lookups in the map of pipelines | 3.2 % |
| The copy of the palette blocks (2,064 bytes each) | 3.1 % |
| `vkResetCommandBuffer` | 2.7 % |
| `vkCmdBeginRendering` | 2.5 % |
| `vkUpdateDescriptorSets` | 2.5 % |
| The copy of the draw uniforms | 1.9 % |
| The copy of the indices for the vertex pre-pass | 1.7 % |

Two changes came from these numbers and stayed (app 0.0.55):

- A thread of its own frees the rendered packets (`uc::PacketTrash` in `game/packet_trash.h`). The pools of a packet take a free from any thread. When that thread is behind by 96 packets, the render thread frees the packet itself. The deterministic mode does not use the thread. `UC_NO_PACKET_TRASH=1` turns it off.
- A hash table holds the pipelines of the last keys (`uc::KeyMemo` in `renderer/pipeline_keys.h`). A hit needs no lock and no walk of the map of 70-word keys. A compiled pipeline never changes, so an entry stays right. A test on the PC gave 29 ns for a lookup, against 82 ns in the map. `test_key_memo` in `tests/test_pipeline_keys.cpp` tests the table. It passed in a compile of its own, and no PC build ran with it. `UC_NO_KEY_MEMO=1` turns the table off.

The app 0.0.54 had these two changes and two more, which are gone again (see below). Its results:

- A profile at 640 s of `s20c1` (`quest2\t54a`). The free of the packets took 5.7 % of the render thread before. The hand-over to the new thread takes 0.5 %. The lookups of pipelines took 3.2 % before, and they take about 1.5 %.
- That play of `s20c1` had 9 seconds below 60 fps in its first 703 s. The six plays of the app 0.0.53 without a slack had 14 to 68. In the slow seconds with a busy game thread, the game thread waited 84 ms a second for the renderer. It waited 142 to 184 ms in the two plays before. The play had no crash and no GPU hang, and its screenshots show a right picture.
- The chapter `s30c4` did not gain (`quest2\c54`): 1,460 s with a mean of 65.5 fps and 73 % of the seconds at 60 fps or more. The app 0.0.53 had 65.8 fps and 75 % there (`quest2\cs14`). The GPU limits that chapter. In its slow seconds the render loop found no free GPU slot 340 to 500 times a second. So less work of the render thread does not help there.

Three changes were tried and removed:

- The upload of only the set matrices of a texture bank (app 0.0.54). Its loop took 4.7 % of the render thread, and the whole copies took 3.6 % before. The game has most matrices of a bank set, because position loads and texture loads share the rows.
- A cull test that starts with one point, not with 16 (app 0.0.54). The transform of the points took 4.3 % of the thread, as before. That cost is in the draws that no eye sees, and the test needs all their points.
- A palette block that wrote only its loaded rows (in no app). The game keeps the rows up to 60 loaded, for the same cause.

The app 0.0.55 has only the two changes that stayed. It played the five Oblivion chapters in a row (`quest2\r55`):

- Each chapter loaded and ended. The play had no crash report and no fault frame.
- The chapter `s20c1` had 20 seconds below 60 fps in its first 715 s. The app 0.0.54 had 9 there, and two plays of the app 0.0.53 had 14 and 40. So one play does not prove the gain. The spread from play to play is as large as the gain.
- In the slow seconds with a busy game thread, the game thread waited 137 ms a second for the renderer. That is more than the 84 ms of the app 0.0.54, and less than the 142 to 184 ms before.
- The five chapters had 119 seconds below 60 fps in 4,273 seconds. In 83 of them the game thread used less than 600 ms of CPU. The render loop found no free GPU slot 225 to 413 times a second there. So the GPU limits most of the slow seconds that are left.

#### The clock limits of a headset on battery

A Quest 3 on a charger raises the clock limits of its CPU and its GPU. Meta names this the charging boost (the page "CPU and GPU levels" of the Meta developer documentation). Every play before 26 November 2026 ran on a charger. So every frame rate above this section is the frame rate of a headset on a charger.

| State of the headset | CPU clock | GPU clock, at most |
|---|---|---|
| On a charger | 2.21 GHz | 640 MHz |
| On battery | 1.92 GHz | 545 MHz |
| On battery, the app asks for dynamic resolution | 1.92 GHz | 599 MHz |
| On battery, the app asks for the CPU boost level | 2.36 GHz at most | no change |

- The VrApi lines of logcat show the clocks. A line of a play on a charger has `CPU4/GPU=4/4,2208/640MHz`. The level stays 4, and only the clocks tell the two states apart.
- The test aid. `adb shell setprop debug.crcs.disable_charging_boost 1` turns the charging boost off within 2 s. An empty value turns it on again, and a restart of the headset does that too. `q3\build\reuc-build\quest_battery.sh COMMAND [ARG...]` sets the property, runs the command and clears the property.
- `adb shell dumpsys battery unplug` does not turn the charging boost off (`quest2\b55_bat`).

The test is the start of `s30c4` at the fixed render scale 1.25 (`quest_px.sh`), about 150 s for each play:

| Play | Clock limits | Mean fps | Seconds at 60 fps or more | GPU time of a frame, median |
|---|---|---|---|---|
| `quest2\b55_bat`, app 0.0.55 | charger | 71.1 | 100 % | 11.4 ms |
| `quest2\b55_usb`, app 0.0.55 | charger | 69.5 | 93 % | 10.9 ms |
| `quest2\n56_bat`, app 0.0.56 without the two requests | battery | 60.6 | 56 % | 14.1 ms |
| `quest2\d56_bat`, app 0.0.56 | battery | 64.6 | 91 % | 13.0 ms |

Two changes came from this (app 0.0.56):

- The app turns the dynamic resolution of the runtime on (`XR_META_recommended_layer_resolution`). `XrCompositor::end_frame` asks for the recommended size of the layer in each frame. The renderer does not use the answer, because it has its own automatic render scale. The request opens the GPU level 5: the GPU ran at 599 MHz in `quest2\d56_bat`, and at 545 MHz without the request. `UC_NO_XR_DYNRES=1` leaves the extension off.
- The app asks for the boost level of the CPU in slow seconds (`uc::CpuBoost` in `renderer/cpu_boost.h`, `XrCompositor::set_cpu_boost`). The runtime allows that level for 45 s in a row and for a fifth of the run time. So a boost starts after a second below 93 % of the refresh rate, and it ends after 16 to 40 s. The rule keeps the boost below 18 % of the seconds. A second whose GPU time alone fills the period starts no boost. `tests/test_cpu_boost.cpp` tests the rule. `UC_NO_CPU_BOOST=1` turns it off. The line `[vr] CPU boost on` of the log shows each start.

What the boost did in `quest2\d56_bat`. In one span the frame rate was 50 to 59 fps for the first 5 s of the boost, at 1.92 GHz. Then the clock went to 2.36 GHz, and the frame rate went to 72 fps. In another span the clock stayed at 1.92 GHz for the 8 s of the boost. The runtime raises the clock only while the CPU load is 83 % or more. So the shortest boost holds 16 s since the app 0.0.57.

One setting was not used. The manifest key `com.oculus.trade_cpu_for_gpu_amount` with the value 1 opens the GPU level 6 (640 MHz) together with dynamic resolution. It takes one level from the CPU, and the game thread limits many slow seconds.

The app 0.0.57 played the five Oblivion chapters in a row with the clock limits of battery (`quest2\r57d`, `q3\build\reuc-build\run_bat5.sh NAME`):

- All five chapters ended with a save. The play had no crash report and no fault frame.
- The mean was 70.2 fps. 95.5 % of the 4,289 chapter seconds had 60 fps or more. The chapter table has the five rows.
- `s20c1` and `s20c2` lost the most. They had 89 % and 94 % of the seconds at 60 fps or more, and 97 % and 99 % on a charger. The other three chapters kept their shares.
- 194 seconds were below 60 fps. In 89 of them the game thread used less than 600 ms of CPU. The GPU or the renderer set the limit there.
- In the other 105 slow seconds the game thread was busy. It used 751 ms of CPU a second, and it lost 49 ms to thread switches.
- The game thread was busy in 1,796 of the 4,289 seconds. Its CPU time at 1.92 GHz is the next cost to lower.
- The CPU boost started 43 times. It held 819 of 4,546 s (18 %), and the CPU clock was 2.36 GHz in about 650 of them.
- The GPU ran at 599 MHz in about half of the seconds and at 545 MHz or less in the other half.

A restart of the adb server of the PC cuts a play. The driver then prints `stopped: no reply to input`, and the game runs on with no fault. Godot starts its own adb server, and two plays ended that way (`quest2\r57b`, `quest2\r57c`). If a play stops with that line, look at the creation time of the process `adb.exe` first.

#### The CPU time of the app at the battery clock

The system puts every thread of the app into the cpuset `/immersive`. A cpuset is the list of CPU cores that a group of threads can use. This cpuset holds the cores 3 to 5 only. Core 2 runs the `timewarp` thread of the system.

| Cores | Clock, at most | Clock limit on battery |
|---|---|---|
| 0 and 1 | 2.05 GHz | 1.38 GHz |
| 2 to 5 | 2.36 GHz | 1.92 GHz |

The app has three busy threads: the game thread, the thread `gx-builder` and the render thread. The sound threads use the same three cores. `q3\build\reuc-build\cpusets.sh` and `thread_cores.sh` print these facts, and they change nothing on the headset.

A second play of the app 0.0.57 with the same limits recorded CPU profiles (`quest2\p57`). It had 129 seconds below 60 fps in 4,181 chapter seconds. The play `quest2\r57d` had 194 in 4,289. So two plays of one app differ by about 65 slow seconds, and a smaller gain needs more than one play to show.

The profile tools:

- `q3\build\reuc-build\prof_at.sh NAME "CHAPTER:SECOND:DURATION ..."` records simpleperf profiles at chapter seconds of a running play.
- `q3\build\darkside\slow_times.py RUN/run.log` lists the slow spans of a play by chapter second. It gives the seconds for `prof_at.sh`.
- `q3\build\darkside\prof_get.sh NAME_AT` copies a profile to the PC, and it writes the samples and the thread list.
- `prof_incl.py SAMPLES TID` gives the time of each function together with the functions that it calls.
- `prof_srclines.py SAMPLES TID LEAF_TEXT SYMBOL_FILE` gives the time of one function by source line.
- `pgo_cover.py SAMPLES PROFDATA [TID]` gives the time in translated functions that the optimizer profile never saw run.

What the four profiles of `quest2\p57` show:

- The game thread has a flat profile. 75 % of its time is translated game code. The largest functions are `body_80309F64` (4.7 %), `body_8012D590` (3.7 %), `memcpy` (3 %) and `hle_AXFrame` (2 to 4 %).
- The render thread used 912 ms of CPU a second in the spans that the log names GPU bound. That is about 17 ms a frame at 1.92 GHz, and the period of 72 Hz is 13.9 ms. So the render thread limits those seconds together with the GPU.
- The thread `gx-builder` spends 15 % in `mesh_draw`, 14.5 % in `memcpy` and 14.4 % in `sched_yield`. The yields come from `CommandQueue::loop()` in `runtime/hle/gx_queue.cpp`. It spins 256 times before a timed wait of 500 microseconds.

The largest costs of the render thread in those spans:

| Cost | Share of the thread |
|---|---|
| Driver calls for each packet (`vkUpdateDescriptorSets`, `vkQueueSubmit2`, `vkResetCommandBuffer`, `vkCmdBeginRendering`) | 14.3 % |
| Cull test of each vertex | 7 % |
| Copies of the texture matrix banks | 4.9 % |
| Fills of the matrix palettes | 3.9 % |
| Index rebase for the vertex pre-pass | 2.2 % |
| Setup of `DrawGpu` | 2.1 % |
| Copy of the 1,840 bytes of `DrawUniform` | 1.9 % |

Three changes came from this. They are in the source, and the app 0.0.58 has them.

The matrix writes. A palette block holds the position matrices and the normal matrices of a draw, and a bank holds its texture matrices. The renderer wrote each block and each bank whole. Now `gx::fill_palette` writes only the matrix rows that the draws of the frame use. `gx::used_texture_matrices` finds the texture matrices of a mesh at its upload, and `gx::copy_texture_bank` copies only their rows. A block with more than 10 matrices and a bank with more than 40 are written whole. `UC_FULL_PALETTES=1` and `UC_FULL_BANKS=1` give the old way. `tests/test_gx_transforms.cpp` tests the three functions.

The boost rule. `q3\build\darkside\boost_use.py RUN/run.log` splits the seconds of a play by the boost and by the load of the game thread. In `quest2\r57d` the old rule spent 475 of its 789 boost seconds on seconds with no busy thread. A busy game thread gave 2 slow seconds in 273 seconds with the boost, and 103 slow seconds without it. `load_hist.py` shows that those slow seconds come at 800 ms or more of game thread CPU a second. So the new rule goes by the CPU load:

- The renderer measures the CPU time of the game thread and of the render thread in each second. The line `[vr-load]` of the log has both. `UC_NO_LOAD_LOG=1` turns the line off.
- The load of a second is the larger of two values: the share of the game thread, and the share of the render thread minus 0.15.
- A boost starts after a late second with a load of 0.60 or more.
- A boost also starts before a late second, at a load of 0.80 or more. That start needs room for 40 s in the budget.
- A boost ends after 4 seconds with a load below 0.58, but not before its sixth second. The limits of 40 s and of 18 % of the run time stay.
- A boost that starts within 6 s of the last one holds twice as long, up to 32 s.
- If the load is not known, the old rule applies. `tests/test_cpu_boost.cpp` tests both rules.

The optimizer profile. The headset build takes counts from a recorded play, and the compiler optimizes a function with the count 0 for size. The profile of 16 November 2026 came from the first chapter only. `pgo_cover.py` looked at the game thread time in functions with the count 0. It found 7 % at the end of `s20c1` and 6 % at the start of `s20c4`. The new recording plays all five Oblivion chapters:

1. `q3\build\reuc-build\quest_dc_pgo_build.sh N` builds the instrumented app 0.0.N.
2. `run_pgo5.sh NAME N` installs it on a free headset and plays the five chapters with the clock limits of battery. Then it copies the profile files to the PC.
3. `pgo_merge.sh NAME [OLD_PROFDATA]` makes `project/pgo/dc-quest.profdata` from them.

When the game exits, the instrumented app writes `uc.profraw`. It also writes the profile so far to `uc.profraw.a` and `uc.profraw.b` in turn every 300 s, for a play that ends early. Each of these writes adds one copy to the file. The recording `quest2\g58` ran with the app 0.0.58. All five chapters ended with a save, with no crash report and with a correct picture. The play took 4,956 chapter seconds at 17 to 30 fps, because the counters slow the game. With the new profile, 0.1 % of that game thread time is in functions with the count 0.

A change of the control flow of a function drops its counts. So record the profile after the last change of the hot functions, such as `VulkanRenderer::render()`.

Open on 26 November 2026: the build of the app 0.0.59 with the new profile stopped, because the PC ran out of memory. So no play measured the three changes at the speed of a normal app yet. The headset has the app 0.0.57 again.

#### The bot in the wide view

The `bot` step of `scripts/drive.py` presses A at fixed places for a choice of ways, and it shoots at a grid of five rows. Two places stopped it:

- With the wide view the pointer covers 1.8 times the angle across and 2.7 times the angle up. The bot did not take a way at the choice in `s30c6` (stereo preview). After the step `wide:1.8,2.7` the bot divides its shot places and its prompt places by these factors.
- In the mine room of `s20c6` the game waits for a shot at an enemy with a rifle who stands low in the picture. The grid of the bot ends above him. The bot stood there for 20 minutes on the headset, and it stood there in the flat picture of the PC too. The step `rows:-1,1,-3,3,0,5,-5` gives the grid two more rows. With it the PC bot passed the room after about 3,000 points, and the headset bot passed it too.

`q3\build\reuc-build\quest_chain2.sh` uses both steps. `CHAIN_RESUME=1` is for a route that ends with YES in the checkpoint dialog. With six pieces the piece of `s20c6` is at `aim:-0.061,-0.156`. YES is at `aim:-0.16,0.115` in the wide view too, because the dialog is a 2D layout.

#### Other checks of 24 November 2026

- The fix for the black picture in the long runs. `blackwatch.py` took a picture each 30 s. The runs `quest2\g4` and `quest2\h1` to `quest2\h6` had 83 automatic changes of the scale and 564 pictures. One picture was dark, at a room load, and the next one was right.

- The screens after a death. Without `UC_DC_GOD` and without a shot, the first enemy of the first chapter kills the player. The game shows "You are Dead" and "Continue? Yes / No" (music `menu02.brstm`). Yes is at `aim:0.133,0.407`. A press of A there starts the chapter again from its first movie. This works in the flat picture (`pv\dm1`) and in the stereo preview with a turned and moved head (`pv\ds2`). `q3\build\reuc-build\pc_death.sh NAME mono|stereo` repeats it.
- `q3\build\reuc-build\pc_tour.sh NAME SAVE "ROUTE"` plays the first minutes of a chapter in the stereo preview, with a frame each 10 s. The head is turned and moved (`UC_VR_POS=0.2,-0.3,0.2`). The frames show no fault of the view. They are the whole chapters `s20c1`, `s20c2` and `s20c3` and the first minutes of `s30c4` and `s30c6` (`pv\t_s20c1` and so on). The sky of these chapters is right with the moved head.
- One headset picture at the start of `s30c2` is very bright: the two characters are almost white. The PC shows the same look at that place while a rocket of the bot explodes. It is the flash of the game and not a fault.

A person played the app on 17, 18 and 20 November 2026. The sections "The first play by a person" and "The second play by a person" have the reports. Not done on the headset: a fight against a boss by a person, and a check of the sound by a person. The remote play has no tracked ray, so its shots still follow the pointer.

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
- With the test aids `UC_DC_WEAK=1` and `UC_DC_ROCKET=1` it plays the end of "Memories of a Lost City" and all of "Game of Oblivion". With `UC_DC_BLOOD=1` it also plays the chapters 2 to 7 of "Operation Javier" and the staff roll (see "The rest of the game").
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
- No frame with the squares of the `GX_POINTS` particles was looked at (see "Point draws").
- After the death of the player or of the partner the game shows its game-over screens. The test input does not operate them. In two runs the game went back to its menus.
- In one run the player died at the spiders, and the game went back to its menu instead of a checkpoint (it loaded `mainMenu_us.arc`). The B presses of the test input then started the chapter again. The screens between are not checked.
- `__OSUnlockAllMutex` (8037CF28) is the SDK's code and reads the fields of the guest's mutex, which the natives do not write: a thread that ends with a mutex keeps it.
- Keyboard, mouse, gamepad and VR controller input have one fixed mapping (see "Controls"). It is not tested with real devices.
- The Quest app runs on a Quest 3 (see "On the headset"). A person has not played it with the controllers yet.
- The exposure read-back and the conversion of `GX_TF_I8` copies are in the Vulkan renderer only. The read-back takes the first layer (the left eye of a stereo view).
- The HOME button dims the screen and shows the hand pointer, but the menu itself is not checked.
