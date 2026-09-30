"""Chapter outfit updates without replacing either player's collected items."""
from native_probe import Probe, ROOT

p = Probe()
p.restore((ROOT/'tests/qs1-level-sprite.state').read_bytes())
assert p.read(0xa0) == 8 and p.read(0x45) == 1
assert [p.read(0x83, n) for n in (1, 2)] == [3, 3], 'Old QS1 outfit was not repaired'
fields = (0x82, 0x84, 0x495, 0x499, 0x49a, 0x49b, 0x49e, 0x4c2, 0x79)
items = lambda: [[p.read(a, n) for a in fields] for n in (1, 2)]
inventory = items()
p.run(2)
p.run(p1=8)
p.run(3)
assert not p.status()[2]
assert items() == inventory
p.screenshot(ROOT/'diagnostics/level-sprite-fixed.png')
saved = p.save()
p.run(30, p2=128)
future, image = p.save(), p.frame
p.restore(saved)
p.run(30, p2=128)
assert p.save() == future and p.frame == image

# Hats explicitly select their own body graphics and remain independent.
for n, outfit in ((1, 0), (2, 2)):
    p.write(0x495, n, n)
    p.write(0x83, outfit, n)
p.restore(p.save())
assert [p.read(0x83, n) for n in (1, 2)] == [0, 2]
assert [p.read(0x495, n) for n in (1, 2)] == [1, 2]

# Actual artifact exit goes from Mexico to France, without a fabricated loader.
p.restore((ROOT/'tests/qs1-transition-sprites.state').read_bytes())
p.run(241)
assert p.read(0xa0) == 8 and p.status()[6]
assert [p.read(0x83, n) for n in (1, 2)] == [3, 3]
p.close()
print('PASS: QS1 chapter outfits repaired, native level entry updates both, hats/items retained, replay matches')
