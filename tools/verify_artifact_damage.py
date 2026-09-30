"""Artifact contact must not leak a damage flag to the other player."""
from native_probe import Probe, ROOT
p=Probe();fixture=(ROOT/'tests/qs1-artifact-damage.state').read_bytes()
for who in (1,2):
    p.restore(fixture)
    p.write(0x7a,189,who);p.write(0x7a,190,3-who)
    lives=[p.read(0x79,n) for n in (1,2)]
    for n in (1,2):p.write(0x495,1,n);p.write(0x83,0,n)
    for frame in range(15):
        p.run()
        assert all(not p.read(a,n) for n in (1,2) for a in (0x86,0x568,0x564,0x56b))
    assert not p.read(0x554), 'Artifact was not collected'
    assert [p.read(0x79,n) for n in (1,2)]==lives
    assert all(p.read(0x495,n)==1 for n in (1,2))
    saved=p.save();p.run(20);future,image=p.save(),p.frame
    p.restore(saved);p.run(20)
    assert p.save()==future and p.frame==image
p.close()
print('PASS: either IJ collects QS1 artifact with both overlapping; no damage, hat loss or life loss; replay matches')
