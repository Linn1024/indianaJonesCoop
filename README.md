# Indiana Jones Co-op

## Building a fresh checkout

This repository contains source code, not the game ROM, emulator binaries, local shortcuts, or save-state fixtures. Place your own USA ROM at the repository root as `Young Indiana Jones Chronicles, The (USA).nes`. Build with a 64-bit MinGW-w64 toolchain:

```powershell
.\build.ps1 -ToolchainBin C:\path\to\mingw64\bin
```

Then run `Play Indiana Co-op.cmd`. For BizHawk, load `engine/fceumm_libretro.dll` as the Libretro core and open your ROM. The shortcut mentioned below belongs to the original local installation and is not included in Git. Legacy FCEUX runtimes must be installed separately. Python checks require Python, Pillow, and, for disassembly, py65; QS1 regression checks also need their local, untracked fixtures.

## Running

Double-click **Play Indiana Co-op.cmd**, **Indiana-Coop.exe**, or **Play Indiana Co-op in BizHawk.lnk**. The start menu offers **Easy** (selected by default) and **Classic**. Either player can use Up/Down and A/Start to confirm; keyboard P1 uses W/S and Space/Enter. Then press Enter/Start to advance the original intro.

This build runs a modified NES emulator core, following the Aladdin project's approach. Co-op runs inside the core; there is no runtime Lua script. The supplied original USA ROM stays unchanged.

## Controls

| Action | P1 keyboard | P2 keyboard | XInput controller |
| --- | --- | --- | --- |
| Move / climb | WASD | Arrow keys | D-pad / left stick |
| Jump | Space | L | A |
| Whip / use weapon | F | K | X |
| Rejoin grounded partner | Q | O | Y |
| Pause / resume | Enter | | Start |

**Press F2 or choose Game > Controls** to change keyboard and gamepad buttons. Click a binding, then press the replacement key/button. Changes are saved automatically to `controls.ini`. Controller 1 belongs to P1, controller 2 to P2. XInput controller hardware still needs hands-on testing.

F1 shows controls. **F5 saves** and **F8 loads** the selected slot; choose it in the **Save slot** menu. Alt+Enter toggles fullscreen.

## Pause cheats

Pause first, then hold gamepad **A+B** on either controller. Mapped Jump+Whip also works, as does keyboard **A+B+C**. Use Up/Down to choose, Jump to apply, Pause to resume, or Esc to close the cheats. Ordinary pause has no added overlay.

- Invincibility against enemy damage; pits can still kill.
- Set lives to nine (both players in individual mode).
- Give both players hats.
- Rejoin P2 to grounded P1.
- Toggle noclip for both players: move freely through terrain with the D-pad/stick or movement keys. Turn it off in open space to resume gravity.
- **Level:** use Left/Right to choose a level or sublevel, then A / Jump to load it. The ending screen is skipped. Inventories carry over.
- **Mode: Easy / Classic** is also available here if you want to change it mid-game. **Easy is the default on the start menu**: each player has their own lives; dying subtracts only their life and respawns them at their partner's position. At zero they wait with **OUT** on their HUD. The survivor continues. Entering a different level or sublevel rescues the OUT partner with exactly one life; reloading the same area does not. Game over occurs when both are out. **Classic** uses one life pool and a team respawn. Save states retain an explicitly selected mode; older saves without a mode use Easy. Switching to Easy copies the shared count to each player; switching back uses P1's remaining count.

## BizHawk download

To use your installed BizHawk, double-click **Play Indiana Co-op in BizHawk.lnk**. It opens `C:\TEMP2\Bizhawk\EmuHawk.exe` with this project's modified core and its own `bizhawk/indiana.ini` configuration. Change buttons through **Config > Controllers**. Keyboard defaults: P1 WASD / Space / F / Enter; P2 arrows / L / K. BizHawk uses its own save states.

In BizHawk, press gamepad **Start** to pause the game, then **A+B** together to open the core's cheat menu. Use Up/Down to select, A to apply, and Start to resume. Keyboard equivalents: Enter then Space+F for P1; L+K opens it for P2. Use in-game pause, not BizHawk's emulator pause, which stops input processing. All seven cheat options work inside the core; on the Level row, Left/Right changes the destination and A loads it. Restart BizHawk through the shortcut after updating the core or controller profile.

[Download BizHawk from its official releases page](https://github.com/TASEmulators/BizHawk/releases), also available through **Help > Download BizHawk**. This co-op build launches with its supplied modified core and frontend.

## Co-op behavior

Both players use the game's original movement, attacks, terrain collision, and damage logic. They share the world, camera, and stage progress; lives depend on the selected death mode. P1 and P2 have different outfit colors, with no numbers above them. Hat pickups and damage affect only the collecting or damaged player.

Two stacked original-style HUDs show each player's own weapon, hat, item, score, and coins: P1 above, P2 below. Both retain the game's border, portrait, item artwork, and number font; P2's HUD uses green accents with white text. The output is 40 pixels taller (256×264 with default cropping), preserving the full playfield. All inventory pickups, including hats, weapons, temporary items and coins, belong only to the collecting player. Both inventories survive area loading independently; P2 is no longer rebuilt with P1's equipment. Life counts follow the selected death mode. P2's body uses a fixed green palette across areas without recoloring enemies. Brief protection after damage does not count as an inventory item.

In Classic mode, either player's death costs one shared life and restores the team through the native checkpoint/stage reload. In Easy mode, the surviving player and current area keep running. Either player can enter an exit. The two flight sections retain a shared plane: P1 pilots and P2 fires/bombs.

## Saves and the previous build

Native saves are stored in `saves/slot-N.ijstate` and include both players and their graphics buffers. They use a different format from FCEUX Lua saves.

Your previous saves and Lua build remain available through **Play Legacy Lua Co-op.cmd**. They have not been overwritten or converted. See [legacy instructions](LEGACY.md) for that build.

## Verification

Checks pass for cold boot, independent movement/jumping, both players fighting a guard, individual damage, shared-life recovery, hats, underground entry, camera settling, music progression, native pause, cheats, controls-dialog rebinding, and save/load. Deterministic replay compares complete core state, audio, and rendered video. Direct stage probes reach co-op in all 33 platform-stage IDs; they are not campaign playthroughs.

Full two-person campaign traversal, bosses, scripted vehicles/platforms, flight playthroughs, and physical controllers remain to be tested. See [development instructions](DEVELOPMENT.md) and [licenses](THIRD_PARTY_NOTICES.md).
