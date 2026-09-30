# Native core development

## Build

Requires Windows x64 and a 64-bit MinGW-w64 toolchain (`gcc`, `g++`, `mingw32-make`).

```powershell
./build.ps1 -ToolchainBin C:/path/to/mingw64/bin
```

You can set `INDIANA_MINGW_BIN` instead. The script also detects the existing toolchain in the neighboring zeroTolerance project on this workstation. The toolchain is a build dependency only. Output: `Indiana-Coop.exe` and `engine/fceumm_libretro.dll`. Close the native app before rebuilding loaded binaries.

Complete modified core source is in `engine/`, based on [libretro FCEUmm](https://github.com/libretro/libretro-fceumm), commit `236ccdfc911e84c60fea6b9d0699c2d440a8de14`. HD-pack and NTSC-filter components are disabled in this build. Its existing notices and GPL license remain included.

## Architecture

Death mode is serialized separately in `IJDM`, preserving the legacy `IJCP` layout. Individual mode swaps RAM `$79` with each player's context and preserves counts through area initialization. The `$9BE6` death hook charges the defeated player, clears their death/attack state, and places them at the partner (using a last grounded position when the partner is also dying). Exhausted players omit sprite generation and player input; the camera/AI follow the survivor. Both exhausted players return to the game's native game-over path. New games default to Easy (individual lives). The startup selector consumes inputs and holds emulation until A/Start confirms. Old saves without `IJDM` migrate to Easy and initialize P2 lives from the old shared count; explicitly saved modes remain unchanged.

`ij_video` adds 40 output lines and preserves the original P1 HUD pixels. P2's band renders the original HUD nametable tiles using CHR bank 72 artwork and a fixed green/white/black palette, then replaces inventory/score/coin tiles with P2's values. It does not sample colors from P1's changing item slots. Matching inventories retain identical pixel-art shapes, with green accents on P2 and unchanged white text. The playfield is copied without scaling or cropping, and Libretro geometry advertises the larger height. Independent score/coins and item fields join the existing player-context address list. `IJIV` saves in-progress inventory transfers across loader resets while retaining the old `IJCP` struct size for older saves. The P2 body marks unused OAM attribute bit 2, letting the PPU choose fixed green/skin/outline colors only for those sprites; native priority and transparency still apply.

Inactive players retain native Start-button history in `$F5/$F7` while gameplay buttons are masked. Clearing held Start history causes a normal held press to repeatedly toggle pause when P1 is OUT. The paused QS1 regression holds Start for ten frames from either controller, checks both pause transitions, and verifies the HUD palette after damage and equipment changes.

Dungeon entry at bank 12 `$83D9` records the entering player's screen position alongside the native saved scroll. Return at `$9C64` restores that position instead of the single-player fixed `(128,160)` spawn. Both players start on that occupied doorway position, without the usual untested 24-pixel offset. Separate `IJDR` serialization retains the entrance through underground saves without changing legacy state layouts. The dungeon regression checks either player entering and leaving, inventory retention and movement after returning.

Point damage replay starts at `$EA65`, the common collision body. Ordinary callers enter at `$EA61` with X/Y coordinates; boss bullets enter directly at `$EA65` with `$00/$01` already populated. Both paths now test both player contexts, retaining the native hit result and damage behavior.

Pause cheats also run in the core for BizHawk/other Libretro hosts: `ij_menu_input` handles NES A+B and consumes menu inputs before emulation, while `ij_menu_draw` overlays a temporary indexed frame before video conversion. The original frame buffer is restored immediately afterwards. UI state closes on state load; cheat values retain their existing save chunks. The standalone launcher consumes its own menu chord first, so the two menus do not open together.

Extra player and collision passes execute without advancing the emulated CPU/PPU/APU budget; original world work remains timed. Native vblank waits remain timed even during a replay. This avoids the every-other-frame slowdown reproduced by the two-enemy slot 1 regression. The Win32 frontend composes each frame in a persistent memory bitmap and presents it with one BitBlt, avoiding a visible black clear before drawing.

Noclip skips player physics/terrain checks while preserving sprite rendering and the shared camera. Its separate `IJNC` save chunk leaves the existing `IJCP` layout unchanged; older native saves load with noclip off. Run `python tools/verify_native_fixes.py` for the reported save, noclip movement, gravity restoration, and save compatibility regression.

`engine/src/indiana_coop.c` owns both player contexts and intercepts selected native CPU instruction boundaries. `x6502.c` invokes the hook **before** opcode fetch. Native subroutines are replayed using core-owned continuation addresses; no trampoline instructions are written into game RAM or cartridge WRAM. Enemies and world updates run once. Nearest-player object context, damage/weapon checks, hats, doors, death recovery and shared camera rules carry over from the Lua prototype.

The core identifies the supported USA cartridge using sizes and combined PRG/CHR CRC32 `35c6f574`. The standalone app additionally validates full-file SHA-256 `a8cec2954f957a88c1628aa6f6cc929babacc38fafa9a509a1ea0e253131d3f3`.

CHR storage expands privately in memory; the original ROM file/header are not rewritten. Banks 128 and 129 alternate so displayed sprite tiles are not overwritten by the next animation frame. Native save chunks `IJCP` and `IJCH` contain player/transient state and both graphics buffers. The struct version is 2; this build targets x64 little-endian. Frontend files add an `IJCOOP2` envelope and write through a temporary file before replacing a slot. FCEUX `.fcN`/`.luasav` files are kept separate; there is no automatic migration.

`launcher.cpp` implements Win32 video, queued stereo audio, XInput and keyboard input, the controls dialog, save slots, and pause cheats. The dialog writes `controls.ini`. Native pause detection lives in the core; cheat-menu display and navigation live in the standalone frontend. Core exports use standard Libretro plus `ij_*` helpers for the frontend and regression tools. A stock NES core runs single-player.

The Aladdin source and research were the architectural reference. NES-specific addresses and mapper/PPU behavior were developed separately. See [legacy research](LEGACY-DEVELOPMENT.md) for ROM reverse-engineering and Lua history.

## Tests

Python 3 with Pillow is needed for headless screenshots:

```powershell
python tools/native_probe.py
python tools/verify_native.py
python tools/verify_native_av.py
python tools/verify_native_inventory.py
python tools/verify_hud_damage.py
python tools/verify_death_modes.py
python tools/verify_last_life_render.py
python tools/verify_pause_hud.py
python tools/verify_dungeon_exit.py
python tools/verify_boss_bullets.py
python tools/verify_level_rescue.py
python tools/verify_script_scene.py
python tools/verify_moving_platform.py
python tools/verify_stone.py
python tools/verify_death_camera.py
python tools/verify_platform_spawn.py
python tools/verify_noclip_lock.py
python tools/verify_falling_camera.py
python tools/verify_boss_transition.py
python tools/verify_artifact_damage.py
python tools/verify_boss_artifact.py
python tools/verify_transition_sprites.py
python tools/verify_level_outfit.py
python tools/verify_motorcycles.py
python tools/verify_sprite_limit.py
python tools/verify_double_pit.py
python tools/verify_stairs.py
python tools/verify_grab_scene.py
python tools/verify_planes.py
python tools/verify_plane_bomb.py
python tools/verify_lift_release.py
```

Tests boot the original cartridge from power-on. Generated `tests/native-mexico.state` is a development artifact, never a launcher dependency. Replay checks compare serialized state, every rendered frame, and audio samples. Combat, hat, death, and door tests include injected positions/object states. Stage probes directly enter initialization; they do not establish campaign completion.

```powershell
Start-Process ./Indiana-Coop.exe -ArgumentList '--test 2800 --save-test --ui-test' -WindowStyle Hidden -Wait
Get-Content diagnostics/native-launcher.txt
```

The frontend check boots, opens and uses cheats, saves/loads, and changes a keyboard binding through the actual Win32 controls dialog. Test saves/settings are isolated under `diagnostics/`; user files are not changed. `native-controls.bmp` and `native-menu.bmp` capture actual controls/menu drawing. This does not test physical key rollover, gamepad hardware, or listening through a sound device.

Older Lua tests remain under `tests/` and use preserved FCEUX. `Play Legacy Lua Co-op.cmd` runs that build; it is not required for native play.

## Remaining work

Full co-op campaign playthrough; bosses, all pickup/weapon variants, moving platforms and vehicles; flight sections; physical controllers; extended real-time audio/visual playtesting. BizHawk boot and core pause cheats have been verified; RetroArch has not. Both frontends receive the expanded HUD output from the core.

Area transitions compare the previous stage (`IJAS`) with the destination before resetting contexts. In Easy mode, an OUT partner returns with one life only when the area changes; the survivor count and both inventories persist. The level cheat queues a native loader jump at the next instruction boundary, skips ending ID 31, and shares the same transition path. Menu display numbers are native stage IDs plus one.

Vertical co-op framing eases toward a 96?128 pixel center band, ignores dying/OUT players, respects native map limits and yields to scripted scenes. P2 collision runs after camera movement, so moving-platform screen coordinates temporarily receive the same scroll delta, then restore before native world updates. `IJPV` preserves this temporary view during mid-pass save states. QS1 death-camera tests verify stable world positions, smooth settling, repeated respawns and the OUT survivor.

Old saves can retain a consumed platform-spawn cursor and an erased active bit from the former per-player platform state. On load, stages 3/5 recover only already-passed nearby platform entries whose inactive base coordinates are invalid/off-screen, using native ROM placement, heights and initial timing. Active platforms and inventories are untouched; QS1 verifies both players landing and subsequent deterministic replay.

Vertical tracking keeps the 2-pixel idle correction but allows up to 12 pixels per frame while a live player is airborne. The falling-camera QS1 regression isolates scrolling from upper-ledge spike damage using invincibility (pit deaths remain enabled), and verifies both players reach the lower floor without losing lives.
