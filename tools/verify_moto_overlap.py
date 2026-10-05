"""QS1: mounting a motorcycle must not damage an overlapping partner."""
from native_probe import Probe, ROOT

p=Probe()
p.restore((ROOT/'tests/qs1-moto-overlap.state').read_bytes())
p.write(0x7a,80,2)  # Keep the falling player away until the overlap setup.
p.run(40)
base=p.save()
fields=(0x79,0x82,0x83,0x495)
inventory=lambda:[[p.read(a,n) for a in fields] for n in (1,2)]
for who in (1,2):
    p.restore(base)
    slots=[i for i in range(0,80,16) if p.read(0x514+i)==0xc0]
    assert len(slots)==2
    # Slot zero is P1's bike; the cloned pickup belongs to P2.
    slot=slots[who-1]
    x,y=p.read(0x519+slot),p.read(0x518+slot)
    for n in (1,2):
        for a,v in ((0x7a,x),(0x7c,y),(0x569,0),(0x86,0),(0x87,0),(0x88,0)):
            p.write(a,v,n)
    expected=inventory()
    p.run(1,**{'p'+str(who):256})
    for frame in range(12):
        p.run()
        assert inventory()==expected, 'Mounting stripped a partner weapon or hat'
        assert not p.read(0x86,3-who) and not p.read(0x568,3-who)
    assert p.read(0x96,who)==slot+1 and not p.read(0x96,3-who)
    saved=p.save()
    p.run(20,**{'p'+str(who):128})
    future,image=p.save(),p.frame
    p.restore(saved)
    p.run(20,**{'p'+str(who):128})
    assert p.save()==future and p.frame==image
p.close()
print('PASS: either rider mounts over the partner without damage, lost weapons or hats; independent ownership and replay')
