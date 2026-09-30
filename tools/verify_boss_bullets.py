"""QS1 boss projectile must damage either player without hitting the partner."""
from native_probe import Probe, ROOT

p = Probe()
fixture = (ROOT/'tests/qs1-boss-bullets.state').read_bytes()
for who in (1,2):
    p.restore(fixture)
    p.run(10,p1=8)
    p.run(2)
    assert not p.status()[2]
    # Keep the target in the actual bullet path and the partner above it.
    # Bullets are spawned and moved by the boss's unmodified native code.
    for frame in range(700):
        for n in (1,2):
            for a,v in ((0x7a,99 if n==who else 8),(0x7c,144 if n==who else 32),
                        (0x564,0),(0x568,0),(0x569,0),(0x56b,0),(0x86,0)):
                p.write(a,v,n)
        p.run()
        if p.read(0x86,who):
            assert not p.read(0x86,3-who), 'Bullet damaged the distant partner'
            break
    else:
        raise AssertionError(f'Boss bullets never hit P{who}')
    saved = p.save()
    p.run(2)
    assert p.read(0x568,who) > 0, 'Hit did not trigger native damage'
    assert not p.read(0x568,3-who)
    p.screenshot(ROOT/f'diagnostics/boss-bullet-hit-{who}.png')
    p.run(15)
    future, image = p.save(), p.frame
    p.restore(saved)
    p.run(17)
    assert p.save() == future and p.frame == image
p.close()
print('PASS: native boss bullets damage either IJ independently; hit replay matches')
