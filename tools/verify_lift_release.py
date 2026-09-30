"""QS1: the falling lift must update/release its rider, whichever player it is."""
from native_probe import Probe, ROOT

p = Probe()
fixture = (ROOT/'tests/qs1-green-frozen.state').read_bytes()
p.restore(fixture)
fields = (0x79, 0x82, 0x83, 0x84, 0x495, 0x499, 0x49a, 0x49b, 0x49e)
inventory = lambda: [[p.read(a, n) for a in fields] for n in (1, 2)]
items = inventory()
p.run(3)
assert not p.read(0x5da, 2), 'Old stopped-lift save retains the movement lock'
x, y = p.read(0x7a, 2), p.read(0x7c, 2)
p.run(20, p2=64|256)
assert p.read(0x7a, 2) < x and p.read(0x7c, 2) < y
assert inventory() == items
p.screenshot(ROOT/'diagnostics/green-unfrozen.png')

for who in (1, 2):
    p.restore(fixture)
    p.write(0x5da, 0, 3-who)
    p.write(0x5da, 1, who)
    p.write(0x8d, 16, who)
    p.write(0x5c3, 255)
    # Let the original lift code detect the floor and end its descent.
    p.run(3)
    assert p.read(0x5bf) == 5 and p.read(0x5c3) == 0
    assert not p.read(0x5da, who), ('Lift released the wrong player', who)
    saved = p.save()
    p.run(20, **{'p'+str(who): 64|256})
    future, image = p.save(), p.frame
    p.restore(saved)
    p.run(20, **{'p'+str(who): 64|256})
    assert p.save() == future and p.frame == image
p.close()
print('PASS: frozen QS1 resumes movement/jumping, either lift rider released by native landing, inventory and replay')
