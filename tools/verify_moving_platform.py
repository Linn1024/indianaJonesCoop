"""QS1: native moving platforms are shared world state, not player state."""
from native_probe import Probe, ROOT
p=Probe();fixture=(ROOT/'tests/qs1-moving-platform.state').read_bytes()
# Real input scrolls a second platform into the scene. Previously nearest-
# player object dispatch spawned it in P2's context, then erased its active
# flag when restoring P1, leaving visible artwork with no collision surface.
p.restore(fixture)
for frame in range(60):p.run(p2=128|(256 if frame<20 else 0))
assert p.read(0x580)&2, 'New moving platform lost its shared active flag'
for a,v in ((0x7a,p.read(0x5c8)+5),(0x7c,p.read(0x5b4)-45),
            (0x87,1),(0x5a,0),(0x5b,1),(0x56d,0),(0x56e,0)):
    p.write(a,v,2)
for frame in range(40):
    p.run()
    if p.read(0x8d,2)==0x11 and not p.read(0x87,2):break
else:raise AssertionError('Green IJ fell through the newly spawned platform')
for riders in ((1,),(2,),(1,2)):
    p.restore(fixture);p.run(2)
    assert p.read(0x580)&1
    for who in riders:
        for a,v in ((0x7a,p.read(0x5c7)+5),(0x7c,p.read(0x5b3)-45),
                    (0x87,1),(0x5a,0),(0x5b,1)):
            p.write(a,v,who)
    for frame in range(40):
        p.run()
        if all(p.read(0x8d,n)&0xfc==0x10 and not p.read(0x87,n) for n in riders):break
    else:raise AssertionError(('Missed moving platform',riders))
    height=p.read(0x7c,riders[0])
    for frame in range(12):
        p.run()
        for who in riders:
            assert not p.read(0x87,who)
            assert abs(p.read(0x7c,who)-(p.read(0x5b3)-30))<=2
    assert p.read(0x7c,riders[0])!=height, 'Platform did not carry rider'
    p.screenshot(ROOT/f'diagnostics/platform-riders-{len(riders)}-{riders[0]}.png')
    saved=p.save();p.run(20);future,image=p.save(),p.frame
    p.restore(saved);p.run(20)
    assert p.save()==future and p.frame==image
p.close()
print('PASS: either IJ and both together land on and ride the moving platform; save replay matches')
