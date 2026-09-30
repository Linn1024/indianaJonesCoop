"""QS1 stale stone lock and cancelling noclip during a stone animation."""
from native_probe import Probe, ROOT
p=Probe();p.restore((ROOT/'tests/qs1-noclip-off.state').read_bytes())
assert not p.core.ij_noclip() and not p.read(0x42)
y=[p.read(0x7c,n)+p.read(0x9b) for n in (1,2)]
p.run(10,p1=8);p.run(30)
assert not p.status()[2]
assert all(p.read(0x7c,n)+p.read(0x9b)>y[n-1] for n in (1,2))
for trigger in (1,2):
    p.restore((ROOT/'tests/qs1-stone.state').read_bytes())
    p.write(0x7a,109,trigger);p.write(0x7a,121,3-trigger)
    p.run(5);assert p.read(0x42)==39
    assert p.core.ij_cheat(4) and p.core.ij_noclip()
    assert not p.read(0x42)
    p.run(12,p1=16,p2=16)
    assert p.core.ij_cheat(4) and not p.core.ij_noclip()
    assert not p.read(0x42)
    y=[p.read(0x7c,n)+p.read(0x9b) for n in (1,2)]
    saved=p.save();p.run(12)
    assert all(p.read(0x7c,n)+p.read(0x9b)>y[n-1] for n in (1,2))
    future,image=p.save(),p.frame
    p.restore(saved);p.run(12)
    assert p.save()==future and p.frame==image
p.close()
print('PASS: QS1 stale lock repaired; noclip cancels either-player stone animation; switching off restores gravity; replay matches')
