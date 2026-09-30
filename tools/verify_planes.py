"""Two native planes: input, fire, hits, death modes, replay and both stages."""
from native_probe import Probe, ROOT

p = Probe()
p.restore((ROOT/'tests/qs1-plane-hud.state').read_bytes())
p.run(2)
assert p.read(0x60c) == p.read(0x60c, 2) == 2
assert p.read(0x60d) == p.read(0x60d, 2) == 0
raw, width, height, pitch = p.frame
# Equal equipment must use the same original flight icon shapes in both HUDs.
for y in range(13, 29):
    for x in range(64, 112):
        top = raw[y*pitch+x*4:y*pitch+x*4+3]
        bottom = raw[(y+40)*pitch+x*4:(y+40)*pitch+x*4+3]
        assert (top == b'\0\0\0') == (bottom == b'\0\0\0'), 'P2 HUD uses platform-stage icons'
p.screenshot(ROOT/'diagnostics/plane-hud-fixed.png')
p.restore((ROOT/'tests/qs1-planes.state').read_bytes())
p.run(2)
assert p.status()[1] and p.status()[6]
assert p.read(0x36) != p.read(0x36, 2)
base = p.save()
for who in (1, 2):
    p.restore(base)
    old = [(p.read(0x36, n), p.read(0x37, n)) for n in (1, 2)]
    p.run(10, **{'p'+str(who): 128|16|1})
    assert p.read(0x36, who) < old[who-1][0] and p.read(0x37, who) > old[who-1][1]
    assert (p.read(0x36, 3-who), p.read(0x37, 3-who)) == old[2-who]
    assert p.read(0x3a, who) != 240 or p.read(0x3c, who) != 240
    assert p.read(0x3a, 3-who) == p.read(0x3c, 3-who) == 240

    # Contact with the actual plane enemy in QS1 hits only the colliding pilot.
    p.restore(base)
    for n in (1, 2):
        p.write(0x36, p.read(0x611) if n == who else 200, n)
        p.write(0x37, p.read(0x614) if n == who else 20, n)
        p.write(0x38, 0, n)
    p.run(2)
    assert p.read(0x60c, who) == 1 and p.read(0x60c, 3-who) == 2

    p.restore(base)
    for n in (1, 2):
        p.write(0x36, p.read(0x611)-5 if n == who else 200, n)
        p.write(0x37, p.read(0x614)-40 if n == who else 16, n)
    p.run(12, **{'p'+str(who): 1})
    assert p.read(0x61c) == 0, 'Pilot bullets did not damage the native enemy'

    p.restore(base)
    lives = [p.read(0x79, n) for n in (1, 2)]
    p.write(0x163, 1, who)
    p.write(0x36, 254, who)
    p.run(4)
    assert p.read(0x79, who) == lives[who-1]-1 and p.read(0x79, 3-who) == lives[2-who]
    assert not p.read(0x163, who) and p.read(0x60c, who) == 2
    p.write(0x79, 1, who)
    p.write(0x163, 1, who)
    p.write(0x36, 254, who)
    p.run(4)
    assert p.core.ij_player_out(who)
    old_x = p.read(0x37, 3-who)
    p.run(10, **{'p'+str(3-who): 128})
    assert p.read(0x37, 3-who) > old_x

p.restore(base)
p.core.ij_cheat(5)
assert not p.core.ij_death_mode()
p.write(0x163, 1, 2)
p.write(0x36, 254, 2)
p.run(4)
assert [p.read(0x79, n) for n in (1, 2)] == [3, 3]
assert all(p.read(0x37, n) == 48 and p.read(0x60c, n) == 2 for n in (1, 2))

p.restore(base)
p.run(10, p2=128|16|1)
p.screenshot(ROOT/'diagnostics/two-planes.png')
saved = p.save()
p.samples.clear()
p.run(60, p1=1, p2=1)
future, image, audio = p.save(), p.frame, bytes(p.samples)
p.restore(saved)
p.samples.clear()
p.run(60, p1=1, p2=1)
assert p.save() == future and p.frame == image and bytes(p.samples) == audio

p.restore(base)
p.core.ij_level_current()
p.core.ij_cheat(7)
p.core.ij_cheat(6)
p.run(100)
assert p.read(0xa0) == 20 and p.status()[6]
old_x = p.read(0x37, 2)
p.run(10, p2=128|1)
assert p.read(0x37, 2) > old_x
p.close()
print('PASS: two planes in both flight stages, independent controls/shots/hits/lives, OUT survivor, exact video/audio replay')
