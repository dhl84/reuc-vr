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
- The step `speed:N` of `scripts/drive.py` sets the speed of the game clock. Reach a scene at speed 4, then measure at speed 1.
- `UC_CPU_SAMPLE=all` samples every thread of the process into `cpu-samples-all.txt` in the working folder of the game (`game/cpu_sampler.cpp`). `UC_CPU_SAMPLE_FLAG=<file>` holds the start back until that file exists (`touch:` step). A line is `T<thread id> rva rva ... = count`. The line `T0 0 = N` gives the number of sampling rounds. Resolve the addresses with `uc.map` of the build. `UC_CPU_SAMPLE=guest` does not start in this game: it starts in the frame wait native of `RBUE08`.
- The log line `[sched-cycles]` gives the CPU work of each guest thread in a second, in millions of cycles. The line `[sched] ... run ms` also counts the time that a thread waits while it holds the baton.
- `UC_TEXTURE_MISS_LOG=<file>` writes one line for each miss of the texture cache: address, size, format, and the number of cache entries with that address and with that key.
- `UC_WRITE_TRAP=<report>|<addresses>` (`game/write_trap.h`) works in this game too. It arms at the first retrace after `<report>.go` appears.
- `--game-arg=--fps-limit --game-arg=60` (options of `scripts/drive.py`) runs a chapter with the 60 Hz step (see "The 60 Hz mode"). Without it the Windows build runs the game's own 30 Hz step. The log line `[game-pacing]` gives the frame waits, the retraces and the game time since the start.
- The log line `[perf-dc]` gives, for each second, the small textures that took a GPU image of the pool, the read-backs of the exposure copy and their host time.
- `UC_SMALL_TEXTURES=0` turns the handling of small rewritten textures off (see "The render thread"): the texture cache keeps every version, and the renderer makes a GPU image for each. Use it to compare.
- The step `trace:NAME,N[,EVERY]` of `scripts/drive.py` writes the screen box of every draw of N frames to a file. The boxes of the same draws over the retraces show how the camera and the models move. Two runs can be compared this way (the pictures of two runs are never taken at the same game time).

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

The fixed waits of the third and the last line do not fit every run: in one run the A press on Leon did not take, and the chapter's movie started 7900 retraces later, with the next A press. These steps wait for the movie and for the end of the loading instead: `aim:-0.27,-0.2 wait:60 until:MV_301_10.sfd,A,120,3000 waitlog:EZ0S_r302.arc,12000`. The game opens `EZ0S_r302.arc` when the walk into the village starts, about 6600 retraces after the movie file. The first voice line of the walk (`VO_3015000.brstm`) follows about 300 retraces later; `waitlog` on it gives two runs the same game time within a few retraces.

To play on without the places of the enemies, repeat these steps: `spray:220,2 aim:0.47,-0.03 wait:6 press:A wait:20 shot:NAME`. `spray` shoots 220 times over a grid of points, one shot every 2 input frames. The A press at that place takes "Go Right" when the game shows a choice of ways; in a run without that press the choice waited for more than 25000 retraces. On 15 November 2026 a run of 90 such rounds (146000 retraces, `--seconds 1200 --speed 4`) went through the streets, the market, the spiders, the hut with the wounded man, the choice of ways and the flooded part of the village to the boss of the chapter. The player died at the boss several times; each time the game showed "You are Dead" and the screen "Loading data" and went on from its last checkpoint. No run stopped with an error.

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
- Each frame the update of the light object (8034F238) takes the place of the model and asks the light manager (the object at 80758480, its pointer is at r13 - 9644) about the lights there (virtual `+0x30`, 802D50D4). Then the manager writes the 16 texels (virtual `+0x78`, 802D61D4): one sum of the lights for each texel, as bytes for alpha, red, green and blue.
- The material of the model samples the texture as texture map 1 in its first TEV stage (draw call site 804AD87C).
- The first chapter has about 80 such textures, and nearly every one gets new texels in every frame: 41430 of 41711 misses of the texture cache in 36 s.
- Each miss made a new entry in the texture cache, which kept the old versions (4096 entries at most, each with its GPU image), and a new GPU texture: an image, a view, a staging buffer and two memory allocations, and later the same number of frees.

The fix (`kSmallTextureTexels` in `common/title.h`: 64 texels; 0 for `RBUE08`, which keeps its path):

- `gx::TextureCache`: new texels of such a texture replace the entry with the old texels. The decode runs on the calling thread, not on a worker.
- `VulkanRenderer`: when no submission uses the GPU image of such a texture any more, the image goes to a pool (512 images at most), and the next small texture of that size takes it. The texels go through one staging buffer of the frame slot. A rewrite makes no image, no buffer and no memory allocation. It is one copy of 64 bytes into an image.
- The movie player writes each frame into the same 640x480 texture. Before, the flood of small textures pushed the old frames out of the cache. Without the flood 1 GB of them stayed. `kTextureVersionLimit` (2) drops the older versions of one texture when a third one arrives. A movie frame still makes one GPU image.

The read-back of the exposure copy:

- It never waited for the GPU. The commands of the frame copy the texture to a buffer, and the render thread takes the pixels when the fence of that submission has signalled. The game gets them at its next `GXCopyTex`, one or two frames late.
- The time was in the mean of the pixels. The loop read the mapped buffer one byte at a time, and the buffer was in write-combined memory, the first host-visible memory type of the GPU: 2.9 ms for each copy of 128x64 pixels (render scale 2).
- Now the buffer comes from cached host memory when the device has such a type (`allocate_host_buffer`, `prefer_cached`), and the pixels are copied out once before the mean: 0.015 ms for each copy. The bytes that the game gets are the same.

Numbers (15 November 2026, same PC and settings as above, 30 Hz step; a run is the walk into the village for 900 retraces without input, then three rounds of the spray input; one run each unless a second value is given):

| | before | after |
|---|---|---|
| Presented frames for 60 retraces | 27.4 (26.4 with `UC_SMALL_TEXTURES=0`) | 29.9, 29.5, 29.9 |
| Render thread, ms for a presented frame | 15.6 (20.0) | 1.5, 2.0, 1.7 |
| the same in the walk / in the fights | 28.2 / 12.5 | 1.6 / 1.5 |
| GPU textures made for a frame | 32.6 (44.2) | 0.0 |
| Small textures that took a pooled image, for a frame | - | 36 to 47 |
| Read-back, ms for a copy | 2.9 | 0.015 |

- Memory at the end of such a run: the working set of the process had a peak of 2930 MB before and 1967 MB after, the texture cache held 1022 MB in the movie before and 81 MB at most after, and the GPU memory in use was 873 MB before and 706 MB after.
- The pictures: two runs never show the same game time, so no pair of frames is equal. The closest pair of 14 frames after the first voice line, one run with `UC_SMALL_TEXTURES=0` and one without, has a PSNR of 22.8 dB (neighbours of one run, 6 retraces apart: 19 to 21 dB), and 36.2 dB after both are scaled down to 64x36 (neighbours: 29 to 32 dB). The difference is on the edges of objects, where the camera moved.
- The DirectX renderers have no pool: they still make a texture for each rewrite. The change of the cache holds for them too. They have no read-back and no `GX_TF_I8` conversion.
- Still open: a rewrite is one small upload for each texture (25 to 65 in a frame). One texture for all of them (an atlas) would make it one upload, but it changes how the materials sample.
- Still open: guest frames of 80 to 170 ms, 3 to 10 in a minute, in both modes. The clock of the game counts 4 retraces at most for a frame, so each one makes the game time a few retraces late.

### The 60 Hz mode

- The frame wait of the game (802D8308) is a native now: `hle_DcFrameWait` in `game/game_pacing.cpp` (`project/dc/hle_manual.txt`). With the 60 Hz mode it waits for one retrace whatever the target is. The time step of a chapter is then 1 / 59.94 s, as in the menus, and the game presents 60 frames for 60 retraces.
- The mode is the one of `RBUE08`: a frame rate limit above 30 (`--fps-limit 60`, `graphics.fps_limit` in `config.json`), or `--unlock-60fps`. A new limit while the game runs reaches the native through `game_pacing_set_mode`; that was not tested in this game. The headset build has the limit 60 as its default (`game/uc_main.cpp`), so it gets the mode without a setting. The Windows build without a limit runs the 30 Hz step. A limit above 60 means 60 here: the game has no timer step.
- The target itself stays as the scene set it. The six readers of the target:
  - 80299FFC (in 80299F54, the start of a frame): when a reset is asked for, the retrace count becomes 0 and the next step is target / rate. In the 60 Hz mode that frame steps 2 retraces of game time in 1.
  - 802CB8CC (in 802CB794): a screen saves the target and writes 1 (802CB928); 802CBE90 (in 802CBB3C) writes the saved value back. The screen was not identified.
  - 8031D4D8 and 8031DB3C (in 8031CE54): a number that follows another number. Its step is 0.1 x difference x a factor x (time step x rate / target).
  - 8032CBE4 (in 8032CA94): a model. It gives time step x rate / target, a number of frames, to each of its animation objects (virtual `+0x28`).
  - 80337EB0 (in 80337D3C): a list of animation entries of a model. Each frame counter grows by time step x rate / target.
- So four readers turn the time step into frames of 1/30 s with the target 2. A 60 Hz step is half a frame for them, and what they drive keeps its speed. The test variable `UC_DC_FRAME_TARGET` of the measurement wrote 1 to the target: a 60 Hz step was a whole frame for them, twice the speed. The variable is gone.

Checked on 15 November 2026 (first chapter, speed 1, retrace counts, not wall time):

- Voice lines of the walk without input, one run for each mode: the lines `VO_3015000`, `VO_3010001`, `VO_3017002`, `VO_3017003` and `VO_3017004` started 320, 860, 1262 and 1535 retraces after the first one with the 30 Hz step, and 320, 860, 1262 and 1533 with the 60 Hz step. Five later lines (to 4400 retraces after the first one) started at the same times within the 60 retraces that the log resolves.
- The camera of the walk, with the `trace` step (173 draws followed for 650 retraces after the first voice line, two runs at 30 Hz and one at 60 Hz): from 60 to 530 retraces the boxes of the 60 Hz run were where the 30 Hz runs had them 1 retrace before, which is the precision of the start. Then a guest frame of 133 ms made the 60 Hz run 4 retraces late (see the open item above). The distance of the box centres at the same times (RMS, median of the draws) was 0.006 screen units between the two 30 Hz runs and 0.011 to 0.013 between a 30 Hz run and the 60 Hz run, the late part included.
- The movie of the chapter with its loading, at speed 4: 6671 retraces at 60 Hz, 6610 at 30 Hz.
- Load at 60 Hz: 59.9 frames for 60 retraces in the walk without input (78 s), 59.4 with the spray input (40 s; the seconds below 59 hold a frame capture or a long guest frame). The render thread needs 1.6 to 1.7 ms for a frame, and the guest 14 to 15 million cycles (850 to 900 million each second).
- Not checked: the speed of enemies and of model animations in a fight. Enemies come only after shots, and two runs of the spray input do not fight the same fight. The code of the four readers above says that model animations keep their speed.
- Not done: the pause of the PC port (`game_pacing_set_paused`) does nothing in this game.

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
