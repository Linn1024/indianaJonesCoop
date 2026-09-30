# Young Indiana Jones Co-op

Local two-player co-op built from the supplied **The Young Indiana Jones Chronicles (USA)** NES cartridge, following the player-context approach in `../aladdinGameCoop/RESEARCH.md`.

This document describes the preserved Lua build. Double-click **Play Legacy Lua Co-op.cmd**. Press **Enter** to start or advance the original intro. The FCEUX emulator and Lua scripts are included.

After updating from the version with frozen music, close the old emulator and start a fresh game through the launcher. Old save states can retain the corrupted sound-engine memory. P1 wears brown/red; P2 wears green in the opening stage. The shared camera follows the team with a small dead zone.

| Action | Player 1 | Player 2 |
| --- | --- | --- |
| Move / climb / enter doors | WASD | Arrow keys |
| Jump | Space | L |
| Whip / use weapon | F | K |
| Rejoin grounded partner | Q | O |
| Start / pause | Enter | |
| Save / load current slot | F5 / F8 | |
| Show controls | F1 | |
| Fullscreen | Alt+Enter | |

The launcher uses the native Windows build of FCEUX. Keyboard controls work directly; gamepads can be assigned under **Config > Input** to ports 1 and 2. The native keyboard mappings are blank by default so they do not interfere with the co-op keyboard layout. Physical gamepads have not been tested.

**Change keyboard buttons:** edit [controls.lua](controls.lua), change the quoted key names for `p1` or `p2`, and restart the launcher. `A` means jump, `B` means whip/use weapon, and `start` means pause. For example, `A='r'` binds jump to R. F1 shows the current movement/action bindings. Gamepad buttons are changed in **Config > Input > Configure** for each port.

**Cheat menu:** pause with Enter, then hold the keyboard letters **A+B+C** together. Choose with Up/Down (or W/S), apply with either player's jump button, and press Enter to resume. Esc closes the menu while keeping the game paused. Cheats include invincibility against enemy damage (pits can still kill), nine shared lives, hats for both players, and rejoining P2 to grounded P1. The chord is editable in `controls.lua`.

Both players use the original movement, animation, terrain collisions, weapons and damage logic. They share the world, camera, lives and level progress. Their outfit colors identify the players; there are no numbers above their heads. Either player's hat pickup equips both players; subsequent damage and hat loss remain individual. Either player's death costs one shared life and restarts the team through the native checkpoint/level reload. Doors and exits move the team together. Rejoin only works when the destination partner is grounded and alive.

The two aircraft sections use one shared plane: P1 pilots; P2 operates the gun with K and bombs with L, when available. P1 can also fire.

## Build status

This is a playable initial co-op build, **not a fully playtested campaign release**. Automated checks cover independent movement and jumping, both players whipping a native guard, P2 taking damage independently, shared-life death recovery, rejoining, underground entry, deterministic save replay, and an enemy-update comparison against native execution. Cold boot works without a saved game. Direct stage-load probes reach the co-op loop in all 33 tested platforming/underground stage IDs; these are initialization checks, not completed levels.

Boss fights, every collectible/weapon, moving platforms and scripted events still need campaign playtesting. The flight input bridge is implemented, but the flight sections have not been played through. Hardware controllers have not been tested. Keep the Lua script running: its state is part of co-op saves. Use this build's saves with this build.

The original ROM is left unchanged. The launcher validates its SHA-256 and uses a private copy with extra CHR storage for independent player sprites. This is an emulator-assisted conversion, not a cartridge ROM hack that runs by itself on a normal NES.

See [DEVELOPMENT.md](DEVELOPMENT.md) for architecture, reproduction and tests, and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for emulator attribution.

If startup fails, the CMD now stays open with an error. Startup details are saved in `diagnostics/launch.txt` and `diagnostics/launch-stderr.txt`.
