"""QS1: both falling players must reach the lower floor before pit checks."""
from native_probe import Probe, ROOT
p=Probe();p.restore((ROOT/'tests/qs1-falling-camera.state').read_bytes())
p.run(10,p1=8);p.run(2)
# Isolate camera/pit behavior from the spikes on the upper ledge. This cheat
# blocks contact damage, but deliberately does not block off-screen deaths.
assert p.core.ij_cheat(0)
lives=[p.read(0x79,n) for n in (1,2)]
fast_scroll=False
for frame in range(160):
    previous=p.read(0x9b)
    p.run(p1=128,p2=128)
    fast_scroll |= p.read(0x9b)-previous>2
    assert [p.read(0x79,n) for n in (1,2)]==lives, 'Camera-induced pit death'
    assert all(p.read(0x7c,n)<224 for n in (1,2))
assert fast_scroll and p.read(0x9b)==175
assert all(p.read(0x7c,n)==177 and not p.read(0x87,n) for n in (1,2))
p.screenshot(ROOT/'diagnostics/fall-camera-fixed.png')
saved=p.save();p.run(20);future,image=p.save(),p.frame
p.restore(saved);p.run(20)
assert p.save()==future and p.frame==image
p.close()
print('PASS: both players descend to the lower floor without camera/pit deaths; map limit and replay preserved')
