"""QS1 scripted Mexico exit and entering the scene through level selection."""
from native_probe import Probe, ROOT
p=Probe()
fixture=(ROOT/'tests/qs1-script-lock.state').read_bytes()
for buttons in (0,64,128):
    p.restore(fixture)
    for frame in range(900):
        p.run(p2=buttons)
        if p.read(0xa0)==3:break
    else:raise AssertionError('Script camera never reached its destination')
    assert frame==370, 'Scene must retain native single-player timing'
    p.run(120)
    assert p.status()[6] and not p.status()[2]
    saved=p.save();p.run(20,p2=128);future,image=p.save(),p.frame
    p.restore(saved);p.run(20,p2=128)
    assert p.save()==future and p.frame==image
p.close()
p=Probe();p.boot()
fields=(0x79,0x82,0x83,0x84,0x495,0x499,0x49a,0x49b,0x49e)
inventory=lambda:[[p.read(a,n) for a in fields] for n in (1,2)]
expected=inventory()
p.core.ij_level_current();p.core.ij_cheat(7);p.core.ij_cheat(7);p.core.ij_cheat(6)
for frame in range(900):
    p.run()
    if p.read(0xa0)==3:break
else:raise AssertionError('Level cheat trapped players in the scene')
p.run(120)
assert inventory()==expected
p.screenshot(ROOT/'diagnostics/script-cheat-fixed.png')
p.close()
print('PASS: QS1 completes at native timing despite P2 input; level cheat scene exits, preserves inventories and replays')
