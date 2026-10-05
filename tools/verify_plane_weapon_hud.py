"""A carried whip cannot appear as a plane bomb or supply plane equipment."""
from native_probe import Probe, ROOT

p=Probe()
p.restore((ROOT/'tests/qs1-plane-hud.state').read_bytes())
if p.status()[2]:
    p.run(p1=8)
p.run(2)
def slot(row):
    raw,w,h,pitch=p.frame
    return [raw[y*pitch+x*4:y*pitch+x*4+3]!=b'\0\0\0'
            for y in range(13+row,29+row) for x in range(64,80)]
empty=slot(40)
for who in (1,2):
    p.write(0x82,1,who)
    p.write(0x60d,0,who)
    p.write(0x3e,240,who)
p.write(0x4ba,255)  # Force the original HUD to refresh the weapon slot.
p.run(5)
assert slot(0)==slot(40)==empty, 'Whip produced a false bomb icon'
p.run(5,p1=256,p2=256)
assert p.read(0x3e)==p.read(0x3e,2)==240
# The same HUD refresh must still display actual equipped bombs.
p.write(0x60d,1)
p.run(5)
assert slot(0)!=empty and slot(40)==empty
p.run(1,p1=256)
p.run(2)
assert p.read(0x3e)!=240, 'Real equipped bomb no longer works'
saved=p.save()
p.run(20)
future,image=p.save(),p.frame
p.restore(saved)
p.run(20)
assert p.save()==future and p.frame==image
p.close()
print('PASS: carried whips produce no bomb icons or bombs; actual plane equipment displays and fires; replay')
