"""QS1 flight supplies award only the touching pilot, including P2."""
from native_probe import Probe, ROOT

p = Probe()
fixture = (ROOT/'tests/qs1-plane-bomb.state').read_bytes()
p.restore(fixture)
p.run(2)
p.run(p1=8)
p.run(2)
assert not p.status()[2]
base = p.save()
# Reproduce with ordinary controls: move the green plane up into the item.
p.run(15, p2=16)
assert p.read(0x60d, 2) == 1 and p.read(0x60d) == 0 and p.read(0x620) == 0
p.run(p2=256)
assert p.read(0x3e, 2) < 240, 'Collected bomb equipment cannot fire'
p.screenshot(ROOT/'diagnostics/plane-bomb-fixed.png')

for who in (1, 2):
    for equipped in (False, True):
        p.restore(base)
        for n in (1, 2):
            p.write(0x36, p.read(0x621)+15 if n == who else 190, n)
            p.write(0x37, p.read(0x624) if n == who else 20, n)
            p.write(0x163, 0, n)
            p.write(0x38, 60, n)
            p.write(0x60c, 1, n)
            p.write(0x60d, int(equipped), n)
        p.run(3)
        assert p.read(0x620) == 0
        if equipped:
            assert p.read(0x60c, who) == 2 and p.read(0x60c, 3-who) == 1
        else:
            assert p.read(0x60d, who) == 1 and p.read(0x60d, 3-who) == 0
        saved = p.save()
        p.run(20, **{'p'+str(who): 256})
        future, image = p.save(), p.frame
        p.restore(saved)
        p.run(20, **{'p'+str(who): 256})
        assert p.save() == future and p.frame == image
p.close()
print('PASS: QS1 green pickup and bombing, either collector, independent equipment/repair, replay')
