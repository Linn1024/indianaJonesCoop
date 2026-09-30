"""Dungeon return uses the collector's entrance, not a fixed screen position."""
from native_probe import Probe, ROOT

p = Probe()
# The reported save is already stuck. Move the actors to this map's actual
# doorway, preserving their inventory, then exercise native entrance/exit code.
p.restore((ROOT/'tests/qs1-dungeon-exit.state').read_bytes())
fields = (0x79,0x82,0x83,0x84,0x495,0x499,0x49a,0x49b,0x49e)
inventory = lambda: [[p.read(a,n) for a in fields] for n in (1,2)]
expected = inventory()
for n in (1,2):
    for a,v in ((0x7a,208),(0x7c,160),(0x88,0),(0x87,1),
                (0x5a,0),(0x5b,0),(0x56d,0),(0x56e,0)):
        p.write(a,v,n)
p.run(50)
assert inventory() == expected
assert all(p.read(0x91,n) == 6 for n in (1,2))
outside = p.save()
(ROOT/'diagnostics/qs1-dungeon-repaired-core.state').write_bytes(outside)
for enter in (1,2):
    for leave in (1,2):
        p.restore(outside)
        # Keep the door deliberately off center to reproduce the co-op case.
        p.run(1, **{'p'+str(enter):128})
        p.run(1, **{'p'+str(enter):16})
        p.run(70)
        assert p.read(0xa0) == 33
        inside = p.save()
        # The entry position must survive saving/loading underground.
        p.restore(inside)
        p.write(0x7c,8,leave)
        p.write(0x87,0,leave)
        p.run(90)
        assert p.read(0xa0) == 0
        assert inventory() == expected
        assert all(p.read(0x91,n) == 6 and p.read(0x88,n) == 0 for n in (1,2))
        assert p.read(0x7a) == p.read(0x7a,2)
        assert p.read(0x7c) == p.read(0x7c,2) == 176
        x = p.read(0x7a)
        p.run(20,p1=64,p2=128)
        assert p.read(0x7a) < x and p.read(0x7a,2) > x
        p.screenshot(ROOT/f'diagnostics/dungeon-exit-{enter}-{leave}.png')
p.close()
print('PASS: both entrance/exit players, underground save/load, separate inventories and movement after return')
