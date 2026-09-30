"""QS1: viewport falls must not skip an area or wrap a dying body."""
from native_probe import Probe, ROOT

p = Probe()
base = (ROOT/'tests/qs1-left-fall.state').read_bytes()
for who in (1, 2):
    p.restore(base)
    lives = [p.read(0x79, n) for n in (1, 2)]
    died = False
    for frame in range(90):
        p.run(p1=64 if who == 1 else 0, p2=64 if who == 2 else 0)
        assert p.read(0xa0) == 22, 'Viewport bottom incorrectly triggered an area exit'
        assert not (p.read(0x564, who) and p.read(0x7c, who) < 32), 'Corpse wrapped through the top'
        died |= p.read(0x79, who) == lives[who-1]-1
        assert p.read(0x79, 3-who) == lives[2-who]
    assert died and p.read(0x79, who) == lives[who-1]-1
    assert not p.read(0x564, who)
    saved = p.save()
    p.run(20)
    future, image = p.save(), p.frame
    p.restore(saved)
    p.run(20)
    assert p.save() == future and p.frame == image

    # The bottom of the actual map must still permit either player to exit.
    p.restore(base)
    p.write(0x9b, 175)
    for n in (1, 2):
        p.write(0x7c, 224 if n == who else 200, n)
        p.write(0x87, 1, n)
        p.write(0x564, 0, n)
        p.write(0x56b, 0, n)
    p.run(5)
    assert p.read(0xa0) == 23, 'Legitimate bottom exit blocked'
p.close()
print('PASS: either player falls without skipping areas or wrapping corpses; own life charged, real bottom exits and replay preserved')
