# Development

## Architecture

The Aladdin reference's central technique is retained: execute native player logic with two private player contexts, simulate the shared world once, and give native object logic the nearer player's context. This implementation uses FCEUX Lua CPU hooks instead of rebuilding a Genesis emulator.

`coop.lua` contains the conversion; `play.lua` handles keyboard input and help, `controls.lua` exposes key bindings, and `pause-menu.lua` implements the pause cheat menu. Both characters are NES OAM sprites, with outfit colors identifying them.

The production runtime is now `runtime/fceux-win64/fceux64.exe`, the official native Windows 2.6.6 build. The previously bundled Qt build crashed inside the real `input.get()` call. The mocked frontend test did not detect that defect. `tests/run.ps1 -Suite raw_frontend` now runs 120 frames through the actual keyboard/controller APIs, and `play.ps1` checks a per-launch readiness marker written only after actual input sampling and an emulation frame succeed.

Native Windows FCEUX uses `-lua` and `-no8lim 1`. Its defaults enable both controller ports, reserve the co-op keyboard keys, and bind F5/F8 to native save/load. `tools/configure_windows_runtime.py` reproduces those defaults from the native runtime's generated configuration; `runtime/windows-defaults.cfg` is the first-launch template. The older Qt runtime is retained for development comparison and is no longer launched.

The supported original ROM SHA-256 is `a8cec2954f957a88c1628aa6f6cc929babacc38fafa9a509a1ea0e253131d3f3`. It has 128 KiB PRG and 128 KiB CHR, mapper 4. `play.ps1` validates the source and creates a 256 KiB CHR working copy. PRG remains unchanged. CHR banks 128 and 129 alternate between the displayed and next sprite frames. Each contains P1's current sprite bank plus P2's native tiles in unused slots. Rewriting a single shared bank while the PPU displays it caused mixed body graphics; both buffers are now serialized. Legacy slot-1 metadata remains supported. The emulator's sprite limit is disabled for co-op.

Important hooks:

| CPU address | Role |
| --- | --- |
| Bank 12 `$808D` | Native player simulation entry |
| Bank 12 `$861D` | P1 sprite complete; run P2 and restore P1 |
| `$C4BA` | Merge P2 OAM before shared object work |
| Bank 12 `$9A0C` | Single shared camera update |
| `$EA61`, `$EAB2`, `$EB0B` | Native damage contact against both players |
| `$ED63`, `$EE79` | Native weapon checks against both players |
| `$C53F` and following object dispatch calls | Nearest-player object context |
| `$C1E1` | Native stage/death reload; reset secondary context |
| `$FA71` | Flight-section gunner input bridge |

FCEUX calls execution hooks **after fetching the opcode**. Redirects must preserve that opcode's semantics. Small cartridge WRAM bridges at `$6000`, `$6008` and `$6020` provide JSR/RTS continuation points. They are installed immediately before use. Internal RAM `$0600-$07FF` belongs to the sound engine; the previous bridges there corrupted music. Banked hooks check the mapped PRG bank. CPU registers, stack depth, inputs, camera deltas and player state are restored after the secondary pass. Save metadata includes in-flight contexts and the generated CHR bank, since ROM edits are not ordinary emulator save RAM.

The horizontal camera uses the players' midpoint with a 112–144 pixel dead zone and a maximum two-pixel step. Its temporary native anchor is restored after scrolling, and unused scroll backlog is cleared. P2 body sprites use a different existing sprite palette, preserving world colors and native fades.

Native hat pickup hooks `$E0DC` and `$E11C` equip the partner in the correct player context, including when the nearer-player object dispatch has installed P2. Native pause entry `$C0E6` and exit `$C100` control menu availability. Invincibility refreshes each player's native damage immunity; it does not disable pit deaths.

Source references: [FCEUX Lua API](https://fceux.com/web/help/LuaFunctionsList.html) and [CPU hook implementation](https://github.com/TASEmulators/fceux/blob/master/src/x6502.cpp). Existing read-only analysis and the initial Mexico fixture came from `../indianaJones`; the Godot remake is not used at runtime.

## Verification

Run `powershell -NoProfile -ExecutionPolicy Bypass -File tests/run.ps1`.

`-Suite features` checks both hat types for either collector, native pause/menu behavior, and the supplied sprite-bug save (copied to `tests/slot-1-sprites.fc1` with its Lua sidecar). Its mapper-write trace ensures moving players never rewrite the currently displayed CHR bank; reverting to a single buffer makes that assertion fail. `-Suite frontend_menu` exercises keyboard menu navigation through `play.lua`, confirms there are no number labels, and verifies a changed jump binding. User save slots are not overwritten.

Run `tests/run.ps1 -Suite av` for audio/camera/palette regressions. It compares 600 idle frames' APU write sequences against single-player execution (allowing a small end-of-frame timing difference), checks stationary and rightward camera motion and settling, and checks palette preservation and alternate sprite attributes. `diagnostics/av-preview.png` captures the two outfits. These checks cover the opening Mexico scene, not every stage's camera or palette behavior.

The checks use `tests/mexico-start.fc0`, a local development fixture copied from the neighboring project's captured original-ROM state. It is not used by the launcher. The report is written to `diagnostics/verification.txt`. Tests include injected player/enemy positions and deaths as well as ordinary input-driven movement and native enemy spawning; they do not constitute a normal campaign playthrough.

`tests/boot.lua` checks startup without a fixture; `tests/campaign.lua` probes direct native stage loading. Set `INDIANA_COOP_ROOT` to this folder and run native Windows FCEUX with `-lua` and the working cartridge. Run emulator tests sequentially because the bundled emulator shares its configuration file.

Alternatively use `tests/run.ps1 -Suite boot`, `-Suite campaign`, or `-Suite frontend`. The frontend test boots `play.lua` and feeds its keyboard mapping deterministic events. It confirms independent P2 keyboard movement; it does not test physical keyboard rollover or controller hardware.

`tools/prepare.py` regenerates the private working ROM for development. Python is optional for playing. `tools/disassemble.py` requires `py65` and prints mapped CPU dumps. Local disassemblies, screenshots and test output are in `diagnostics/`.

## Remaining verification

Complete two-person campaign traversal, all weapons/pickups, boss logic, scripted vehicles/platforms, all object targeting families, flight playthroughs, physical controllers and save/load through the emulator UI remain to be checked. Native execution is retained, but that alone does not establish that every single-player assumption has been adapted correctly.
