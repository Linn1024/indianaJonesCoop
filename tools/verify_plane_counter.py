"""Either pilot contributes once to the shared flight objective."""
from native_probe import Probe, ROOT

p=Probe()
p.restore((ROOT/'tests/qs1-planes.state').read_bytes())
p.run(2)
base=p.save()
for who in (1,2):
    p.restore(base)
    p.write(0x49e,10)
    p.write(0x49e,10,2)
    for n in (1,2):
        p.write(0x36,p.read(0x611)-5 if n==who else 200,n)
        p.write(0x37,p.read(0x614)-40 if n==who else 16,n)
    p.run(12,**{'p'+str(who):1})
    assert p.read(0x49e)==p.read(0x49e,2)==9, 'Kill was lost or counted twice'
    p.run(20)
    assert p.read(0x49e)==p.read(0x49e,2)==9
    saved=p.save()
    p.run(20)
    future,image=p.save(),p.frame
    p.restore(saved)
    p.run(20)
    assert p.save()==future and p.frame==image
p.boot()
p.write(0x49e,24)
p.write(0x49e,39,2)
p.core.ij_level_current()
while p.core.ij_level_choice()!=19:
    p.core.ij_cheat(7)
p.core.ij_cheat(6)
p.run(100)
assert p.read(0xa0)==19
assert p.read(0x49e)==p.read(0x49e,2)==10, 'Coin inventory replaced the flight objective'
assert p.read(0x162)==24 and p.read(0x162,2)==39
p.close()
print('PASS: either pilot kill counts once for both HUDs; native objective initialization, saved coins and replay')
