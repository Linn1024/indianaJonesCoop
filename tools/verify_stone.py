"""QS1 stone must finish one animation and launch both riders together."""
from native_probe import Probe, ROOT
p=Probe();fixture=(ROOT/'tests/qs1-stone.state').read_bytes()
for trigger in (1,2):
    p.restore(fixture)
    p.write(0x7a,109,trigger);p.write(0x7a,121,3-trigger)
    fields=(0x79,0x82,0x83,0x84,0x495,0x499,0x49a,0x49b,0x49e)
    inventory=lambda:[[p.read(a,n) for a in fields] for n in (1,2)]
    before=inventory()
    p.run(20)
    assert p.read(0x571)==p.read(0x571,2)==20
    saved=p.save()
    for frame in range(30):
        p.run()
        if p.read(0x5b)==11 or p.read(0x5b,2)==11:
            assert p.read(0x5b)==p.read(0x5b,2)==11
            assert p.read(0x87)==p.read(0x87,2)==1
            assert p.read(0x7c)==p.read(0x7c,2)
            assert p.read(0x42)==0
            break
    else:raise AssertionError('Stone never launched both riders')
    assert inventory()==before
    future,image=p.save(),p.frame
    p.restore(saved);p.run(frame+1)
    assert p.save()==future and p.frame==image
    p.screenshot(ROOT/f'diagnostics/stone-both-{trigger}.png')
# A distant partner is not a rider and must not receive the launch velocity.
p.restore(fixture);p.write(0x7a,32);p.run(41)
assert p.read(0x5b,2)==11 and p.read(0x5b)!=11
p.close()
print('PASS: either rider triggers stone; both riders launch together; distant partner excluded; inventory and mid-animation save replay preserved')
