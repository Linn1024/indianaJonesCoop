"""QS2: either player's clock freezes the shared enemies for its duration."""
from native_probe import Probe, ROOT

p = Probe()
base = (ROOT/'tests/qs2-freeze-time.state').read_bytes()
enemy = lambda: bytes(p.read(0x514+i) for i in range(16))
for who in (1,2):
    p.restore(base)
    if who == 1:
        p.write(0x7a,49)
        p.write(0x7a,80,2)
    p.run(23, **{'p'+str(who):64})
    timer = p.read(0x4bf,who)
    assert 140 < timer <= 150 and not p.read(0x4bf,3-who)
    frozen = enemy()
    assert frozen[0] == 0x6e
    for frame in range(80):
        p.run()
        assert enemy() == frozen, 'Enemy ignores the collector clock'
    assert p.read(0x4bf,who) == timer-20, 'Clock countdown runs at the wrong speed'
    saved = p.save()
    p.run(20)
    future, image = p.save(), p.frame
    p.restore(saved)
    p.run(20)
    assert p.save() == future and p.frame == image
    p.run(p1=8)
    paused = p.read(0x4bf,who)
    p.run(30)
    assert p.read(0x4bf,who) == paused
    p.run(p1=8)
    p.run()
    p.write(0x4bf,1,who)
    p.run(40)
    assert not p.read(0x4bf,who) and enemy() != frozen, 'Enemy remains frozen after expiry'

# A shorter clock expiring must not cancel the other player's active clock.
p.restore(base)
p.run(23,p2=64)
p.write(0x4bf,1)
p.write(0x4bf,20,2)
frozen = enemy()
p.run(60)
assert not p.read(0x4bf) and p.read(0x4bf,2)==5 and enemy()==frozen
p.run(40)
assert not p.read(0x4bf,2) and enemy()!=frozen
p.close()
print('PASS: native pickup by either IJ freezes enemies; separate timers, pause, expiry, overlapping clocks and replay')
