"""Cheat level selection and one-life rescue on area changes."""
from native_probe import Probe, ROOT
p=Probe();p.boot();base=p.save()
def exhaust(who):
    p.write(0x79,1,who);p.write(0x564,1,who);p.write(0x56b,2,who);p.run(30)
    assert p.core.ij_player_out(who)
def rescued(who):
    assert not p.core.ij_player_out(who)
    assert p.read(0x79,who)==1
    assert p.read(0x79,3-who)==3
    assert p.status()[6]
for who in (1,2):
    p.restore(base);exhaust(who)
    p.run(p1=8);p.run();p.run(p1=257);p.run()
    for _ in range(6):p.run(p1=32);p.run()
    p.run(p1=128);p.run()
    assert p.core.ij_level_choice()==1
    p.screenshot(ROOT/'diagnostics/level-cheat-menu.png')
    p.run(p1=256);p.run(100)
    assert p.read(0xa0)==1
    rescued(who)
    assert not p.status()[2]
# Natural door entry and exit both count as a new area.
for who in (1,2):
    p.restore(base);exhaust(who)
    p.write(0x9a,0);p.write(0x9d,1)
    for a,v in ((0x7a,144),(0x7b,0),(0x7c,176),(0x7d,0)):
        p.write(a,v,3-who)
    for i in range(100):p.run(**{'p'+str(3-who):16 if 15<=i<70 else 0})
    assert p.read(0xa0)==32
    rescued(who)
    exhaust(who)
    saved=p.save();p.restore(saved)
    p.write(0x7c,8,3-who);p.write(0x87,0,3-who);p.run(90)
    assert p.read(0xa0)==0
    rescued(who)
# Reloading the same area is not a rescue.
p.restore(base);exhaust(2)
p.core.ij_level_current();assert p.core.ij_cheat(6);p.run(100)
assert p.core.ij_player_out(2) and p.read(0x79,2)==0
# Selection skips the non-playable ending and wraps in both directions.
p.core.ij_level_current();p.core.ij_cheat(8)
assert p.core.ij_level_choice()==35
p.core.ij_cheat(7);assert p.core.ij_level_choice()==0
for _ in range(31):p.core.ij_cheat(7)
assert p.core.ij_level_choice()==32
p.close()
print('PASS: gamepad level menu, both-player rescue on level/door entry/exit, saved OUT state, same-area exclusion and selector wrap')
