"""The cloned parked motorcycle must emit visible green native sprites."""
from native_probe import Probe, ROOT

p=Probe()
p.restore((ROOT/'tests/qs1-moto-overlap.state').read_bytes())
p.write(0x7a,80,2)  # Saved P2 is falling where the repaired bike will stand.
p.run(40)
slots=[i for i in range(0,80,16) if p.read(0x514+i)==0xc0]
assert len(slots)==2
brown,green=slots
assert abs(p.read(0x519+brown)-p.read(0x519+green))>=32
# Move the player sprite clear so only the parked bike occupies this region.
p.write(0x7a,80,2)
p.write(0x515+green,2)  # Old-save empty facing frame.
p.run(2)
assert p.read(0x515+green)==1
x,y=p.read(0x519+green),p.read(0x518+green)
tiles=[]
for i in range(0,p.read(0x78),4):
    sy,tile,attr,sx=[p.read(0x200+i+j) for j in range(4)]
    if attr&0x1c==4 and x-8<=sx<=x+16 and y<=sy<=y+16:
        tiles.append(tile)
assert len(tiles)==5, 'Missing parked green motorcycle tiles'
p.screenshot(ROOT/'diagnostics/parked-motorcycle-verified.png')
saved=p.save()
p.run(20)
future,image=p.save(),p.frame
p.restore(saved)
p.run(20)
assert p.save()==future and p.frame==image
p.close()
print('PASS: visible five-tile green parked bike beside original; old-save facing repair and replay')
