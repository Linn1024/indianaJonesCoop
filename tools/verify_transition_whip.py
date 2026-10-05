"""Original story transitions restore the whip for both player inventories."""
from native_probe import Probe

p=Probe()
p.boot()
base=p.save()
fields=(0x79,0x83,0x84,0x495,0x499,0x49a,0x49b,0x49e,0x4c2)
inventory=lambda:[[p.read(a,n) for a in fields] for n in (1,2)]
for weapons in ((0,0),(3,5)):
    p.restore(base)
    for n,weapon in enumerate(weapons,1):
        p.write(0x82,weapon,n)
        p.write(0x4c2,n,n)
    expected=inventory()
    p.write(0xe8,1)  # Native story-skip setting; still executes equipment resets.
    p.core.ij_level_current()
    p.core.ij_cheat(7)
    p.core.ij_cheat(7)
    p.core.ij_cheat(6)
    for frame in range(900):
        p.run()
        if p.read(0xa0)==3:
            break
    else:
        raise AssertionError('Story transition did not complete')
    # Let the title/story screen finish; inventories are restored when the
    # destination's gameplay starts, not while native story RAM is in use.
    for frame in range(3000):
        p.run(p1=8 if frame%30==0 else 0)
        state=p.save()
        if state[state.index(b'IJIV')+8]==0:
            break
    else:
        raise AssertionError('Destination inventory was never restored')
    assert p.read(0x82)==p.read(0x82,2)==1, 'Whip reset lost during inventory transfer'
    assert inventory()==expected, 'Other inventory changed during whip reset'
    saved=p.save()
    p.run(20)
    future,image=p.save(),p.frame
    p.restore(saved)
    p.run(20)
    assert p.save()==future and p.frame==image
p.close()
print('PASS: missing or equipped weapons become whips for both players at the native story reset; other inventory and replay preserved')
