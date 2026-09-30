"""Reported QS1: P1 is OUT, but P2 must remain visible and playable."""
import hashlib
from native_probe import Probe, ROOT

p = Probe()
p.restore((ROOT/'tests/qs1-last-life-freeze.state').read_bytes())
assert p.core.ij_player_out(1) and not p.core.ij_player_out(2)
start = p.status()[5]
audio = []
for n in range(180):
    bank = p.status()[11]
    p.run(p2=128 if n < 60 else 0)
    assert p.status()[5] == start+n+1
    assert p.status()[11] != bank, 'P2 graphics composition stopped'
    count = p.read(0x78)
    assert count > 0, 'Survivor vanished from OAM'
    assert any(p.read(0x202+i)&4 for i in range(0,count,4)), 'Missing P2 body sprites'
    if n%60 == 59:
        audio.append(hashlib.sha256(p.samples).digest())
        p.samples.clear()
assert len(set(audio)) == 3
assert p.core.ij_graphics_errors() == 0
p.screenshot(ROOT/'diagnostics/qs1-out-fixed.png')
state = p.save()
p.run(20,p2=64)
future, image = p.save(), p.frame
p.restore(state)
p.run(20,p2=64)
assert p.save() == future and p.frame == image
p.close()
print('PASS: QS1 survivor renders and updates every frame; music and save replay continue')
