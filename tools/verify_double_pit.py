"""QS1: releasing controls after a double fall must not drain all lives."""
from native_probe import Probe, ROOT

p = Probe()
fixture = (ROOT/'tests/qs1-double-pit.state').read_bytes()
p.restore(fixture)
initial = [p.read(0x79, n) for n in (1, 2)]
fields = (0x82, 0x83, 0x84, 0x495, 0x499, 0x49a, 0x49b, 0x49e)
inventory = lambda: [[p.read(a, n) for a in fields] for n in (1, 2)]
items = inventory()
for frame in range(120):
    p.run(p1=128, p2=128)
    if any(p.read(0x79, n) != initial[n-1] for n in (1, 2)): break
else: raise AssertionError('Did not reach the pit')
midfall = p.save()
p.run(660)
assert [p.read(0x79, n) for n in (1, 2)] == [n-1 for n in initial]
assert all(not p.read(0x87, n) and not p.core.ij_player_out(n) for n in (1, 2))
assert inventory() == items
future, image = p.save(), p.frame
p.screenshot(ROOT/'diagnostics/double-pit-fixed.png')
p.restore(midfall)
p.run(660)
assert p.save() == future and p.frame == image

# Legacy saves may already be airborne, without any recorded world anchor.
p.restore(fixture)
for n in (1, 2): p.write(0x7c, 240, n)
p.run(350)
assert p.status()[8] == 9 and p.status()[6]
assert [p.read(0x79, n) for n in (1, 2)] == [n-1 for n in initial]
assert inventory() == items
counts = [p.read(0x79, n) for n in (1, 2)]
p.run(300)
assert [p.read(0x79, n) for n in (1, 2)] == counts
p.close()
print('PASS: double pit costs one life each, stable grounded respawn, old-save entrance fallback, inventory and replay')
