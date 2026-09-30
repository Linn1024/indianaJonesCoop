"""Actual pickup/door paths, independent HUDs, fixed P2 colors and replay."""
from native_probe import Probe, ROOT

p = Probe()
p.boot()
raw, width, height, pitch = p.frame
top, bottom = raw[:40*pitch], raw[40*pitch:80*pitch]
assert all((top[i:i+3] == b'\0\0\0') == (bottom[i:i+3] == b'\0\0\0')
           for i in range(0, len(top), 4)), 'The green HUD must retain the original pixel-art shapes'
assert top != bottom, 'P2 HUD must have its own green tint'
base = p.save()
def pickup(who, kind):
    for n in (1, 2):
        p.write(0x7a, 128 if n == who else 32, n)
        p.write(0x7c, 176, n)
    for a in range(0x514, 0x564):
        p.write(a, 0)
    p.write(0x514, kind)
    p.write(0x518, 184)
    p.write(0x519, 128)
    p.run(8)

# Temporary protection is inventory too: it must not reach the partner.
for who in (1, 2):
    p.restore(base)
    pickup(who, 0x5a)
    assert p.read(0x4bd, who) == 0x4c and p.read(0x569, who) > 30
    assert p.read(0x4bd, 3-who) == 0 and p.read(0x569, 3-who) == 0
p.restore(base)
pickup(1, 0x36)
assert [p.read(0x82, n) for n in (1, 2)] == [2, 1]
pickup(2, 0x3e)
assert [p.read(0x82, n) for n in (1, 2)] == [2, 4]
pickup(2, 0x26)
assert [p.read(0x49e, n) for n in (1, 2)] == [0, 3]
for n in (1, 2):
    p.write(0x499, 11*n, n)
    p.write(0x495, n, n)
    p.write(0x83, 0 if n == 1 else 2, n)
fields = (0x82, 0x83, 0x84, 0x495, 0x499, 0x49a, 0x49b, 0x49e)
inventory = lambda: [[p.read(a, n) for a in fields] for n in (1, 2)]
expected = inventory()
saved = p.save()
for who in (1, 2):
    p.restore(saved)
    p.write(0x9a, 0)
    p.write(0x9d, 1)
    for n in (1, 2):
        for a, v in ((0x7a, 144 if n == who else 112), (0x7b, 0),
                     (0x7c, 176), (0x7d, 0)):
            p.write(a, v, n)
    for i in range(100):
        p.run(**{'p'+str(who): 16 if 15 <= i < 70 else 0})
    assert p.read(0xa0) == 32 and p.status()[6]
    assert inventory() == expected, (who, inventory(), expected)
    assert p.frame[1:3] == (256, 264), p.frame[1:3]
    p.screenshot(ROOT/f'diagnostics/inventory-door-{who}.png')
state = p.save()
p.run(15)
future, image = p.save(), p.frame
p.restore(state)
p.run(15)
assert p.save() == future and p.frame == image

# Cross a native vertical area boundary with either player as the trigger.
for who in (1, 2):
    p.restore(base)
    for a, v in ((0xa0, 13), (0xa3, 0), (0xa4, 32), (0xa5, 112), (0xa6, 0)):
        p.write(a, v)
    p.core.ij_test_pc(0xc1e1)
    p.run(90)
    for n in (1, 2):
        p.write(0x82, 2*n, n)
    p.write(0x7c, 225, who)
    p.write(0x564, 0, who)
    p.run(90)
    assert p.read(0xa0) == 12
    assert [p.read(0x82, n) for n in (1, 2)] == [2, 4]

# Change the game's palette while keeping simulation identical. Green body
# pixels must keep exactly the same RGB even as surrounding art changes.
p.restore(base)
for n, x in ((1, 40), (2, 176)):
    p.write(0x7a, x, n)
    p.write(0x7c, 176, n)
p.run(4)
state = p.save()
p.run(3)
before = p.frame[0]
p.restore(state)
for a in range(0xc0, 0xd0):
    p.write(a, 0x21)
p.run(3)
after = p.frame[0]
green = []
for y in range(225, 250):
    for x in range(168, 215):
        offset = (y*256+x)*4
        b, g, r = before[offset:offset+3]
        if g > 100 and g > r*1.4 and g > b*1.4:
            green.append(offset)
assert len(green) > 30
assert all(before[i:i+4] == after[i:i+4] for i in green)
assert before != after
p.close()
print('PASS: independent pickups/coins, both door triggers retain inventory, 256x264 HUD, fixed P2 green, replay')
