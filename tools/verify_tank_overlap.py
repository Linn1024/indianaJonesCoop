"""QS1: a driven tank must follow its owner through the other player."""
from native_probe import Probe, ROOT

p = Probe()
base = (ROOT/'tests/qs1-tank-overlap.state').read_bytes()
for who in (1, 2):
    p.restore(base)
    if who == 2:
        p.write(0x7a, 151)
        p.write(0x7a, 73, 2)
    buttons = 'p'+str(who)
    p.run(20, **{buttons: 128})
    p.run(1, **{buttons: 256})
    p.run(30)
    assert p.read(0x95, who) == 128, 'Native tank mounting failed'
    assert p.read(0x7a, who) < p.read(0x7a, 3-who)
    for frame in range(110):
        p.run(**{buttons: 128})
        assert p.read(0x95, who) == 128
        assert not p.read(0x95, 3-who)
        assert p.read(0x51a) == 2, 'Occupied tank changed to its parked sprite'
        assert p.read(0x519) == p.read(0x7a, who), 'Tank left behind at the other IJ'
        assert p.read(0x518) == p.read(0x7c, who)
    assert p.read(0x7a, who) > p.read(0x7a, 3-who), 'Players did not cross'
    saved = p.save()
    p.run(20, **{buttons: 64})
    future, image = p.save(), p.frame
    p.restore(saved)
    p.run(20, **{buttons: 64})
    assert p.save() == future and p.frame == image
    p.run(1, **{buttons: 256})
    p.run(5)
    assert not p.read(0x95, who), 'Driver cannot leave the tank'
p.close()
print('PASS: either driver crosses the other IJ without leaving a tank sprite; dismount and replay preserved')
