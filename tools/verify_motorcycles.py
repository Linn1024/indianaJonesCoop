"""QS1: separate motorcycle pickups, native riding, green graphics and replay."""
from native_probe import Probe, ROOT

p = Probe()
p.restore((ROOT/'tests/qs1-motorcycle.state').read_bytes())
p.run(40)
slots = [i for i in range(0, 80, 16) if p.read(0x514+i) == 0xc0]
assert len(slots) == 2, slots
positions = [p.read(0x519+i) for i in slots]
assert positions[1] - positions[0] == 32
base = p.save()
p.screenshot(ROOT/'diagnostics/motorcycle-pair.png')

def place(who, x):
    for a, v in ((0x7a, x), (0x7c, 176), (0x569, 0), (0x86, 0)):
        p.write(a, v, who)

for who in (1, 2):
    p.restore(base)
    place(who, positions[who-1])
    place(3-who, 40)
    p.run(1, **{'p'+str(who): 256})
    p.run(2)
    assert p.read(0x96, who) and not p.read(0x96, 3-who)
    assert not p.read(0x86, who) and not p.read(0x86, 3-who)

# Jumping onto the other player's bike must not consume it.
for who in (1, 2):
    p.restore(base)
    place(who, positions[2-who])
    place(3-who, 40)
    p.run(1, **{'p'+str(who): 256})
    p.run(2)
    assert not p.read(0x96, who) and not p.read(0x96, 3-who)

p.restore(base)
for who in (1, 2): place(who, positions[who-1])
p.run(1, p1=256, p2=256)
p.run(30)
assert p.read(0x96) and p.read(0x96, 2)
assert all(p.read(0x514+i) != 0xc0 for i in range(0, 80, 16))
p.screenshot(ROOT/'diagnostics/motorcycles-both-riding.png')
before = p.read(0x7a, 2) - p.read(0x7a)
p.run(12, p2=128)
assert p.read(0x7a, 2) - p.read(0x7a) > before
saved = p.save()
p.run(20, p1=128, p2=128)
future, image = p.save(), p.frame
p.restore(saved)
p.run(20, p1=128, p2=128)
assert p.save() == future and p.frame == image
p.close()
print('PASS: two assigned motorcycles, independent mounting/riding, no pickup damage, deterministic replay')
