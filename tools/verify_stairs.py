"""QS1: either player can climb to the next screen without dropping the partner."""
from native_probe import Probe, ROOT

p = Probe()
fixture = (ROOT/'tests/qs1-stairs.state').read_bytes()
for who in (1, 2):
    p.restore(fixture)
    if who == 1:
        for a in (0x59, 0x5a, 0x5b, 0x7a, 0x7b, 0x7c, 0x7d, 0x87, 0x88):
            first, second = p.read(a), p.read(a, 2)
            p.write(a, second)
            p.write(a, first, 2)
    fields = (0x79, 0x82, 0x83, 0x84, 0x495, 0x499, 0x49a, 0x49b, 0x49e)
    inventory = lambda: [[p.read(a, n) for a in fields] for n in (1, 2)]
    items = inventory()
    for _ in range(120):
        p.run(**{'p'+str(who): 16})
        if p.read(0xa0) == 14: break
    else: raise AssertionError(('Did not reach upstairs', who))
    transition = p.save()
    for _ in range(200):
        p.run(**{'p'+str(who): 16})
        assert p.read(0xa0) == 14, ('Partner fell into previous screen', who)
    assert inventory() == items
    assert p.read(0x7a) == p.read(0x7a, 2) == 65
    future, image = p.save(), p.frame
    p.restore(transition)
    p.run(200, **{'p'+str(who): 16})
    assert p.save() == future and p.frame == image
    p.run(120)
    assert p.read(0xa0) == 14 and inventory() == items
    p.screenshot(ROOT/f'diagnostics/stairs-fixed-{who}.png')
p.close()
print('PASS: either IJ climbs upstairs, partner stays on ladder, inventories/lives retained, transition replay matches')
