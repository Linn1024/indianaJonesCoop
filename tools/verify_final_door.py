"""QS1: late-game door must reach the remaining area, not the ending."""
from native_probe import Probe, ROOT

p = Probe()
base = (ROOT/'tests/qs1-final-door.state').read_bytes()
fields = (0x79, 0x82, 0x83, 0x84, 0x495, 0x499, 0x49a, 0x49b, 0x49e)
inventory = lambda: [[p.read(a, n) for a in fields] for n in (1, 2)]
for who in (1, 2):
    p.restore(base)
    if who == 1:
        # Put P1 in the same native door contact/physics state as saved P2.
        addresses = list(range(0x59,0x5c))+list(range(0x7a,0x82))+list(range(0x85,0x8a))+list(range(0x8b,0x98))
        addresses += [0x564,0x568,0x569,0x56b]+list(range(0x56d,0x576))+list(range(0x577,0x580))+list(range(0x585,0x5a5))+list(range(0x5d2,0x5dc))
        for a in addresses:
            p.write(a, p.read(a, 2))
    expected = inventory()
    p.run(2, **{'p'+str(who): 16})
    p.run(140)
    assert p.read(0xa0) == 30 and p.status()[6], 'Door skipped the remaining playable area'
    assert inventory() == expected, 'Transition changed lives or inventory'
    saved = p.save()
    p.run(20, p1=128, p2=64)
    future, image = p.save(), p.frame
    p.restore(saved)
    p.run(20, p1=128, p2=64)
    assert p.save() == future and p.frame == image
p.close()
print('PASS: either IJ enters the late-game door and reaches the remaining area; inventory, lives and replay preserved')
