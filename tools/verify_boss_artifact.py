"""The gold boss artifact is a stage-exit query, distinct from ordinary loot."""
from native_probe import Probe, ROOT
p=Probe();fixture=(ROOT/'tests/qs1-artifact-hit.state').read_bytes()
for who in (1,2):
    p.restore(fixture)
    lives=[p.read(0x79,n) for n in (1,2)]
    # Rewind only the saved knockback and place either actor at the artifact.
    for n in (1,2):
        for a,v in ((0x7a,144 if n==who else 205),(0x7c,176),(0x86,0),
                    (0x568,0),(0x569,0),(0x87,0),(0x5a,0),(0x5b,0)):
            p.write(a,v,n)
    for _ in range(120):
        p.run()
        assert not p.read(0x86,2) and not p.read(0x568,2)
        if p.read(0xa0)==8:break
    assert p.read(0xa0)==8, 'Artifact must advance the level for either IJ'
    assert not p.read(0x86,2) and not p.read(0x568,2)
    p.run(350)
    assert [p.read(0x79,n) for n in (1,2)]==lives
    assert all(not p.read(a,n) for n in (1,2) for a in (0x86,0x568,0x564))
    saved=p.save();p.run(20);future,image=p.save(),p.frame
    p.restore(saved);p.run(20)
    assert p.save()==future and p.frame==image
# Repair only the saved erroneous knockback, keeping inventory and lives.
p.restore(fixture);lives=[p.read(0x79,n) for n in (1,2)]
for a,v in ((0x7a,136),(0x7c,176),(0x7b,0),(0x7d,0),(0x86,0),(0x568,0),
            (0x87,0),(0x5a,0),(0x5b,0),(0x56d,0),(0x56e,0)):
    p.write(a,v,2)
(ROOT/'diagnostics/artifact-repaired-core.state').write_bytes(p.save())
p.run(350,p2=128)
assert p.read(0xa0)==8 and [p.read(0x79,n) for n in (1,2)]==lives
p.close()
print('PASS: either IJ collects the gold artifact and exits without damage; repaired QS1 progresses; replay matches')
