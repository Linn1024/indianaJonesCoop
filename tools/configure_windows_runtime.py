"""One-time defaults for the bundled FCEUX 2.6.6 native Windows build."""
from pathlib import Path
import base64
import struct

root = Path(__file__).resolve().parents[1]
path = root / 'runtime/fceux-win64/fceux.cfg'
lines = path.read_text().splitlines()
reserved = {0x11, 0x1e, 0x1f, 0x20, 0x21, 0x25, 0x26, 0x18, 0x10,
            0x39, 0x1c, 0xc8, 0xd0, 0xcb, 0xcd, 0x3b, 0x3f, 0x42}
for i, line in enumerate(lines):
    if line.startswith('"InputType" '):
        lines[i] = '"InputType" base64:' + base64.b64encode(struct.pack('<iii', 1, 1, 0)).decode()
    elif line.startswith('GamePadConfig_V2 '):
        prefix, data = line.split('base64:')
        lines[i] = prefix + 'base64:' + base64.b64encode(bytes(len(base64.b64decode(data)))).decode()
    elif line.startswith('FCEUD_CommandMapping_V2 '):
        prefix, data = line.split('base64:')
        data = bytearray(base64.b64decode(data))
        # ButtConfig is 92 bytes in this release; ButtonNum[0] at 8, NumC at 24.
        assert len(data) % 92 == 0
        for offset in range(0, len(data), 92):
            if struct.unpack_from('<I', data, offset + 8)[0] in reserved:
                data[offset:offset+92] = bytes(92)
        # EMUCMD_SAVE_STATE=26, EMUCMD_LOAD_STATE=38 (v2.6.6 src/input.h).
        for command, scan in ((26, 0x3f), (38, 0x42)):
            offset = command * 92
            data[offset:offset+92] = bytes(92)
            struct.pack_into('<I', data, offset+8, scan)
            struct.pack_into('<I', data, offset+24, 1)
        lines[i] = prefix + 'base64:' + base64.b64encode(data).decode()
    elif line.startswith(('winsizemulx ', 'winsizemuly ')):
        lines[i] = line.split(' ')[0] + ' base64:' + base64.b64encode(struct.pack('<d', 3.0)).decode()
path.write_text('\n'.join(lines) + '\n')
(root / 'runtime/windows-defaults.cfg').write_bytes(path.read_bytes())
