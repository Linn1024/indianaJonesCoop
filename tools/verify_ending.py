"""QS1: final completion reaches a responsive ending, then returns to title."""
from native_probe import Probe, ROOT

p = Probe()
p.restore((ROOT/'tests/qs1-ending-freeze.state').read_bytes())
for frame in range(500):
    p.run()
    assert p.status()[10] >= 0x8000, 'Ending interpreter jumped outside cartridge code'
assert p.read(0xa0) == 31 and p.read(0x47) == 42
assert len(set(p.frame[0])) > 4, 'Final screen is blank'
saved = p.save()
p.run(20)
future, image = p.save(), p.frame
p.restore(saved)
p.run(20)
assert p.save() == future and p.frame == image
for who in (1, 2):
    p.restore(saved)
    p.run(1, **{'p'+str(who): 8})
    p.run(150)
    assert p.read(0xa0) == 0 and p.read(0xe2) == 2, 'Start did not leave the ending'
    assert p.status()[10] >= 0x8000
p.close()
print('PASS: final sequence reaches The End, replay matches, either player can return to title with Start')
