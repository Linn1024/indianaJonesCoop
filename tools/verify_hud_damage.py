from native_probe import Probe, ROOT

p = Probe()
p.restore((ROOT/'tests/qs1-hit-hud.state').read_bytes())
# QS1 has no active temporary item. Post-hit protection and cheat protection
# use the same timer as the pickup, but must never create its HUD icon.
for timer in (25, 3, 30, 1, 0):
    p.write(0x569, timer, 2)
    p.run()
    assert p.read(0x4bd, 2) == 0
p.screenshot(ROOT/'diagnostics/qs1-hud-fixed.png')
p.boot()
for n in (1, 2):
    p.write(0x7a, 128 if n == 2 else 32, n)
    p.write(0x7c, 176, n)
for a in range(0x514, 0x564):
    p.write(a, 0)
p.write(0x514, 0x5a)
p.write(0x518, 184)
p.write(0x519, 128)
p.run(8)
assert p.read(0x569, 2) > 30 and p.read(0x4bd, 2) == 0x4c
p.write(0x569, 5, 2)
p.run()
assert p.read(0x4bd, 2) == 0x4c
state = p.save()
p.write(0x569, 1, 2)
p.run()
assert p.read(0x4bd, 2) == 0
p.restore(state)
assert p.read(0x4bd, 2) == 0x4c
p.close()
print('PASS: QS1 post-hit timers show no item; actual pickup shows, expires, and survives save/load')
