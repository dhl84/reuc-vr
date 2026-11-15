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

- `scripts/drive.py` starts a build with its test server, sends Wii Remote input at given game times, saves frames and reads guest memory. It needs no keyboard and no mouse. The steps `waitlog`, `until` and `waitpeek` wait for a text in the game log or for a value in guest memory, because the retrace count of a screen is not the same in every run. The step `dump` writes MEM1 and MEM2 to files. The step `spray` moves the pointer over a grid of points and shoots at each one. The step `touch` makes a file, which starts a diagnostic that waits for one.
- `UC_PACKET_DUMP=<file>|go|<frames>` lists every render packet of some frames: its draws with call site, blend and textures, and its EFB copy. The list starts when `<file>.go` appears (`touch:packets.txt.go` in `scripts/drive.py`). `UC_PACKET_DUMP_TEV=1` adds the TEV stages, the colour registers and constants, the channel controls and the lights of each draw.
- `UC_CAPTURE_PASS_GO=<file>` saves the frame buffer after each render packet of one frame as `pass-*.png` in the capture folder. It starts with the first packet after a present once the file exists (`touch:pass.go`).
- `UC_TRACE_NEW=<retrace>` (trace build) prints each guest function the first time it runs after that retrace count.
- `UC_DUMP_THREADS=1` prints the state, the wait key and the registers of each guest thread once per second.
- `UC_CAPTURE_RETRACES=300,900` saves the frame at those retrace counts as PNG files (`--capture-dir <dir>`).
- `--log-dvd` prints each disc file that the game opens and reads.

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

To play on without the places of the enemies, repeat these steps: `spray:220,2 aim:0.47,-0.03 wait:6 press:A wait:20 shot:NAME`. `spray` shoots 220 times over a grid of points, one shot every 2 input frames. The A press at that place takes "Go Right" when the game shows a choice of ways; in a run without that press the choice waited for more than 25000 retraces. On 15 November 2026 a run of 90 such rounds (146000 retraces, `--seconds 1200 --speed 4`) went through the streets, the market, the spiders, the hut with the wounded man, the choice of ways and the flooded part of the village to the boss of the chapter. The player died at the boss several times; each time the game showed "You are Dead" and the screen "Loading data" and went on from its last checkpoint. No run stopped with an error.

## State

On 15 November 2026 the Windows build of `SBDE08` does this:

- It runs the SDK start-up, the save start-up, and the start of NW4R, GX, VI, AI, AX and KPAD.
- It loads its files from the unpacked disc.
- It draws the strap screen, the Zapper screen, and the Capcom and CRIWARE logos.
- It shows the dialog "Save game data not found. Create data?", takes A from the test input, and writes `banner.bin` and `buc2_0.dat` to the save folder.
- It plays the intro movie (`MV_000_10.sfd`, decoded by the game's own code) and reaches the title screen.
- It shows the profile screen, the difficulty screen, the main menu and the character screen, and takes the pointer and A there.
- It plays the two movies of the first chapter, loads the chapter and runs it to its boss: the walk through the village, the first enemy, the HUD, shots that kill the enemy, the reload, the subtitles, the scenes in the streets and the market, the spiders, the scene with the wounded man, the choice of ways, the creatures in the water, the screen "You are Dead" and the return to the last checkpoint.
- It reads the music and voice streams (`Data/sound/stream/*.brstm`). The scenes of the chapter wait for their voice lines, so the chapter did not go on after the first enemy before.
- The exposure of the picture follows the brightness of the scene: the game reads the picture back from the GPU.

Found on the way (each is a constant in `common/title.h` or a note in the code):

- Three math natives named `RBUE08` data directly. `PSMTXConcat` gave wrong matrices to every layout pane under a moved parent, so the profile screen drew no slots.
- The arena of MEM1 ends at the file table of the disc (0x817D8320) and the arena of MEM2 at 0x935E0000 (IOS55). With the `RBUE08` values the game ran out of memory when it loaded the chapter.
- `OSJoinThread` does not wait for a detached thread.
- NW4R material display lists write a part of the constant selectors of a stage pair.
- The game sets its exposure from the picture. Each frame it copies the middle of the frame buffer to a `GX_TF_I8` texture, draws that texture into a 32x16 corner, copies the corner to a second `GX_TF_I8` texture (8036AD98), and reads 66 bytes of that texture from memory (8036AE74). It takes the two conversions of the hardware away from their mean (`Y = 16 + 0.8588 x colour`, twice) and gives the result to its tone mapping. A native EFB copy stays on the GPU, so the texture in memory was all zero, the game saw a black picture and chose the largest exposure: the daylight scene was white. The Vulkan renderer now reads such a small intensity copy back, and the next `GXCopyTex` call writes it to the guest's texture, one or two frames late (`kEfbCopyReadbackTexels` in `common/title.h`). The game records these copies in a display list, so the request goes through `GXCallDisplayList` too.
- An EFB copy to `GX_TF_I8` holds `Y = 16 + 0.257 R + 0.504 G + 0.098 B` on the hardware, and the game's formulas expect that. The Vulkan renderer converts the copy on the GPU (`renderer/intensity_copy_pass.cpp`). Before, it read the green channel.
- The exposure is a gain of `0.15 / brightness`, held between 0.75 and 1.25 (8036AFC4). After it the game draws its colour filter with six TEV stages (call site 802A0B04): 30 % less colour, a gain of 1 + (0.62, 0.56, 0.41) for red, green and blue in the first chapter, 8 % less blue, and a contrast curve `x (0.70 x + 0.30)`. The filter comes from the game's data. With the smallest exposure a part of the scene that is brighter than about 0.83 before the filter comes out as pale yellow (255, 255, about 210) without detail. Ground in direct sun does that: its material is twice the texture times the light (TEV scale 2), and the sun has the colour 255. No reference picture of the console was compared.
- NW4R reads its stream files under a mutex that it takes with `OSTryLockMutex` and gives back with `OSUnlockMutex`. `OSUnlockMutex` is a native that keeps the state of a mutex outside the guest, and `OSTryLockMutex` was the SDK's code, which works on the fields of the guest's mutex. The first reader kept the mutex for ever, every later reader slept, and its read ended with "cancelled" when the game stopped the sound. `OSTryLockMutex` is a native now (`project/dc/hle_manual.txt`).

Not done yet:

- The game draws no shadows: an EFB copy to `GX_TF_Z16` gives the farthest depth in every texel (there is no depth copy yet), and a copy that names lines below 480 takes the 480 lines of the native colour buffer. EFB copies to `GX_CTF_R8` give the red channel. Vulkan renderer only.
- Sound is not checked. The runs so far were muted. The streams are read; nobody has listened to them.
- The chapter is not played to its end. The test input shoots at a grid of points and does not win against the boss.
- In one run the player died at the spiders and the game went back to its menu instead of a checkpoint (it loaded `mainMenu_us.arc`); the B presses of the test input then started the chapter again. The screens between are not checked.
- `__OSUnlockAllMutex` (8037CF28) is the SDK's code and reads the fields of the guest's mutex, which the natives do not write: a thread that ends with a mutex keeps it.
- Keyboard, mouse, gamepad and VR controller input. The PC action layer reads `RBUE08` game state, so it is off, and only the Wii Remote sample of the test server reaches the game.
- The Quest build, the VR view and the VR aim.
- The exposure read-back and the conversion of `GX_TF_I8` copies are in the Vulkan renderer only. The read-back takes the first layer (the left eye of a stereo view).
- The HOME button dims the screen and shows the hand pointer, but the menu itself is not checked.
