"""Recover the already-missed platform in QS1 without changing inventory."""
from native_probe import Probe, ROOT
p=Probe();p.restore((ROOT/'tests/qs1-platform-spawn.state').read_bytes())
fields=(0x79,0x82,0x83,0x84,0x495,0x499,0x49a,0x49b,0x49e)
inventory=lambda:[[p.read(a,n) for a in fields] for n in (1,2)]
before=inventory()
p.run(3)
assert p.read(0x580)&2, 'Missing next platform was not recovered'
assert 0<p.read(0x5c8)<240 and p.read(0x5b4)<240
p.screenshot(ROOT/'diagnostics/platform-spawn-fixed.png')
assert inventory()==before
base=p.save()
for who in (1,2):
    p.restore(base)
    for a,v in ((0x7a,p.read(0x5c8)+16),(0x7c,p.read(0x5b4)-45),
                (0x87,1),(0x5a,0),(0x5b,1),(0x56d,0),(0x56e,0)):
        p.write(a,v,who)
    for frame in range(60):
        p.run()
        if p.read(0x8d,who)==17 and not p.read(0x87,who):break
    else:raise AssertionError(('Recovered platform has no collision',who))
    saved=p.save();p.run(20);future,image=p.save(),p.frame
    p.restore(saved);p.run(20)
    assert p.save()==future and p.frame==image
p.close()
print('PASS: QS1 next platform restored from native placement; both players can land; inventory and replay preserved')
