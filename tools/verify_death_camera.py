"""QS1 camera settles away from the HUD and remains stable after respawns."""
from native_probe import Probe, ROOT
p=Probe();p.restore((ROOT/'tests/qs1-death-camera.state').read_bytes())
world_y=[p.read(0x7c,n)+p.read(0x9b) for n in (1,2)]
for frame in range(90):
    scroll=p.read(0x9b);p.run()
    assert abs(p.read(0x9b)-scroll)<=2
    assert [p.read(0x7c,n)+p.read(0x9b) for n in (1,2)]==world_y
assert all(96<=p.read(0x7c,n)<=128 for n in (1,2))
scroll=p.read(0x9b);p.run(90);assert p.read(0x9b)==scroll
p.screenshot(ROOT/'diagnostics/death-camera-fixed.png')
base=p.save()
for who in (1,2):
    p.restore(base);p.write(0x79,9,who)
    for _ in range(3):
        p.write(0x564,1,who);p.write(0x56b,2,who);p.run(60)
        assert p.read(0x9b)==scroll
        assert 96<=p.read(0x7c,3-who)<=128
    p.write(0x79,1,who);p.write(0x564,1,who);p.write(0x56b,2,who);p.run(60)
    assert p.core.ij_player_out(who)
    assert p.read(0x9b)==scroll
    saved=p.save();p.run(20);future,image=p.save(),p.frame
    p.restore(saved);p.run(20)
    assert p.save()==future and p.frame==image
p.close()
print('PASS: QS1 recenters smoothly without moving actors in the world; repeated deaths and OUT partner leave survivor framed; replay matches')
