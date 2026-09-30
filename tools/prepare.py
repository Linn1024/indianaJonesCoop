"""Build a private CHR-expanded working cartridge; never modify the input ROM."""
from pathlib import Path
import hashlib

ROOT = Path(__file__).resolve().parents[1]
source = ROOT / 'Young Indiana Jones Chronicles, The (USA).nes'
data = bytearray(source.read_bytes())
assert hashlib.sha256(data).hexdigest() == 'a8cec2954f957a88c1628aa6f6cc929babacc38fafa9a509a1ea0e253131d3f3', 'Unsupported ROM revision'
assert data[:4] == b'NES\x1a' and data[4:6] == bytes([8, 16]), 'Unsupported cartridge layout'
print('Source SHA256:', hashlib.sha256(data).hexdigest())
data[5] = 32
data.extend(bytes(128 * 1024))
(ROOT / 'runtime' / 'Indiana-Coop.nes').write_bytes(data)
print('Prepared runtime/Indiana-Coop.nes')
