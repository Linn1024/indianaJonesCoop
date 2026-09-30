# Third-party components

The native build uses a modified [FCEUmm Libretro core](https://github.com/libretro/libretro-fceumm), based on commit `236ccdfc911e84c60fea6b9d0699c2d440a8de14`. Complete corresponding source, original copyright notices, and GPL license are included in `engine/`; see `engine/Copying`. Indiana-specific native additions and the standalone frontend are GPL-2.0-or-later; see `LICENSE`.

The Aladdin co-op project's core/frontend design and ROM research served as implementation references. This NES port uses its own CPU hooks and graphics handling.

`runtime/fceux-win64` is the preserved, unmodified Windows FCEUX 2.6.6 runtime, downloaded from the project's official release: https://github.com/TASEmulators/fceux/releases/download/v2.6.6/fceux-2.6.6-win64.zip . It is used only by the legacy launcher/tests.

`runtime/fceux` is the older unmodified FCEUX Windows Qt runtime copied from the neighboring Indiana Jones project's tools directory, retained for development comparison. FCEUX is GPL-2.0-or-later; its copyright and license remain with its contributors.

- Project and source: https://github.com/TASEmulators/fceux
- FCEUX website: https://fceux.com/
- License text: `runtime/FCEUX-COPYING.txt`

The runtime also contains its Qt, SDL, FFmpeg and related shared-library dependencies from that existing installation. Their licenses remain with their respective authors; this project does not claim ownership of those libraries. Source projects: https://www.qt.io/, https://libsdl.org/, https://ffmpeg.org/.

The original game ROM, artwork, music and trademarks are not authored by this project. The launcher uses the user's local cartridge file. Do not include the original or generated ROM, extracted assets, or development save-state fixtures when distributing the co-op source.
