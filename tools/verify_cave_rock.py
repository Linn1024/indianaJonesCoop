"""QS1: returning from the cave keeps the entrance boulder cleared."""
from native_probe import Probe, ROOT

p = Probe()
base = (ROOT/'tests/qs1-cave-rock.state').read_bytes()
fields = (0x79,0x82,0x83,0x84,0x495,0x499,0x49a,0x49b,0x49e)
inventory = lambda: [[p.read(a,n) for a in fields] for n in (1,2)]
for who in (1,2):
    p.restore(base)
    expected = inventory()
    flags = [p.read(0x504+i) for i in range(16)]
    p.run(p1=8)
    p.run()
    p.run(220, **{'p'+str(who):16})
    assert p.read(0xa0) == 1 and p.status()[6]
    assert inventory() == expected
    flags[1] |= 4  # Native spawn ID 10 is this entrance's boulder.
    assert [p.read(0x504+i) for i in range(16)] == flags, 'Unrelated spawn flags changed'
    assert not any(p.read(0x514+i)==0xba and p.read(0x483+i)==10 for i in range(0,80,16))
    saved = p.save()
    p.run(60,p1=128,p2=128)
    future, image = p.save(), p.frame
    p.restore(saved)
    p.run(60,p1=128,p2=128)
    assert p.save() == future and p.frame == image
    p.run(60,p1=64,p2=64)
    assert p.read(0x505)&4
    assert not any(p.read(0x514+i)==0xba and p.read(0x483+i)==10 for i in range(0,80,16))
p.close()
print('PASS: either player exits the cave without the entrance boulder; other spawn flags, inventory and replay preserved')
