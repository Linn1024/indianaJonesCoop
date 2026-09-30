"""QS1: grounded P2 must not freeze airborne P1 during the boss reveal."""
from native_probe import Probe, ROOT
p=Probe();p.restore((ROOT/'tests/qs1-boss-bug.state').read_bytes())
assert p.read(0x42)==1 and p.read(0x87) and not p.read(0x87,2)
activated=False
for frame in range(150):
    p.run()
    activated |= p.read(0x517)!=255
    if p.read(0x9b)==0 and p.read(0x42)==0 and activated:break
else:raise AssertionError('Boss transition is still locked')
assert p.status()[6]
saved=p.save();p.run(90);future,image=p.save(),p.frame
p.restore(saved);p.run(90)
assert p.save()==future and p.frame==image
p.screenshot(ROOT/'diagnostics/boss-transition-fixed.png')
p.close()
print('PASS: QS1 airborne player lands, scripted scroll finishes, boss activates, and save replay matches')
