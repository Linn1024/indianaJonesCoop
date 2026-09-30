"""Regression for the reported enemy save and both-player noclip."""
from collections import Counter
from native_probe import Probe, ROOT

p = Probe()
old = (ROOT/'tests/native-two-enemies.ijstate').read_bytes()[8:]
p.restore(old)
cadence = Counter()
for _ in range(600):
    before = p.status()[5]
    p.run()
    cadence[p.status()[5] - before] += 1
assert cadence == {1: 600}, cadence
assert p.core.ij_graphics_errors() == 0
p.boot()
assert p.core.ij_cheat(4) and p.core.ij_noclip()
y = [p.read(0x7c, j) for j in (1, 2)]
p.run(10, p1=16, p2=32)
assert p.read(0x7c) == y[0]-30
assert p.read(0x7c, 2) == y[1]+30  # Through the solid ground below spawn.
held = [p.read(0x7c, j) for j in (1, 2)]
p.run(20)
assert [p.read(0x7c, j) for j in (1, 2)] == held
saved = p.save()
p.run(12, p1=128, p2=64)
replay = p.save()
p.restore(saved)
p.run(12, p1=128, p2=64)
assert p.save() == replay
p.restore(saved)
assert p.core.ij_cheat(4) and not p.core.ij_noclip()
p.run(12)
assert p.read(0x7c) > held[0], 'Gravity did not resume'
p.restore(saved)
assert p.core.ij_noclip()
p.restore(old)
assert not p.core.ij_noclip() and p.status()[7] == 1
p.run(p1=8)
p.run(2)
assert p.status()[2] and not p.core.ij_menu_visible()
p.run(p2=257)
p.run()
assert p.core.ij_menu_visible()
data, width, height, pitch = p.frame
colors = {data[row*pitch+col*4:row*pitch+col*4+3]
          for row in range(55, 80) for col in range(25, 180)}
assert len(colors) >= 3, 'Menu background and text must have distinct colors'
for _ in range(4):
    p.run(p2=32)
    p.run()
p.run(p2=256)
p.run()
assert p.core.ij_noclip()
p.run(p2=8)
p.run()
assert not p.status()[2] and not p.core.ij_menu_visible()
p.close()
print('PASS: slot 1 updates 600/600 frames; both-player noclip, gravity, replay, old saves')
