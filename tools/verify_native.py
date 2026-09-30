"""Behavioral regression suite for the modified NES core."""
from native_probe import Probe, ROOT
import hashlib

p=Probe()
def check(ok, text):
    assert ok, text
    print('PASS', text, flush=True)
p.boot(); base=p.save()
check(p.status()[5]>35, 'Cold boot runs co-op without Lua or a startup save')
def fresh(): p.restore(base); p.run(3)
for player in (1,2):
    fresh(); before=[p.read(0x7a,n) for n in (1,2)]
    p.run(24, **{'p'+str(player):128})
    check(p.read(0x7a,player)>before[player-1]+20 and p.read(0x7a,3-player)==before[2-player], f'P{player} moves independently')
    y=p.read(0x7c,player);p.run(14,**{'p'+str(player):256})
    check(p.read(0x7c,player)<y-15, f'P{player} native jump')
fresh(); saved=p.save()
def sequence():
    p.samples.clear()
    hashes=[]
    for i in range(90):
        p.run(p1=128|(256 if i<12 else 0),p2=128|(1 if i%25==1 else 0))
        hashes.append(hashlib.sha256(p.frame[0]).digest())
    return p.save(), bytes(p.samples), hashes
expected=sequence();p.restore(saved)
check(sequence()==expected, 'Save replay reproduces full core state, rendered video, and audio exactly')
check(p.core.ij_graphics_errors()==0,'Moving players never rewrite the currently mapped sprite buffer')
for who in (1,2):
    for hat in (1,2):
        fresh()
        for n in (1,2):
            for a,v in ((0x7a,128 if n==who else 32),(0x7c,176),(0x83,1),(0x495,0)):
                p.write(a,v,n)
        for a in range(0x514,0x524):p.write(a,0)
        p.write(0x514,0x4a if hat==1 else 0x4e);p.write(0x518,184);p.write(0x519,128)
        p.run(8)
        check(p.read(0x495,who)==hat and p.read(0x495,3-who)==0
              and p.read(0x83,who)==(0 if hat==1 else 2) and p.read(0x83,3-who)==1,
              f'P{who} hat pickup {hat} equips only the collector')
fresh()
for i in range(260):
    p.run(p1=128,p2=128)
    if p.read(0x514)==6:break
check(p.read(0x514)==6,'Native guard spawns in shared world')
encounter=p.save()
for who in (1,2):
    p.restore(encounter)
    for i in range(70):
        for n in (1,2):
            for a,v in ((0x7a,128 if n==who else 32),(0x7c,112),(0x7b,0),(0x7d,0),(0x59,0),(0x5a,0),(0x5b,0),(0x85,0)):
                p.write(a,v,n)
        if p.read(0x514)==6:p.write(0x518,112);p.write(0x519,154)
        p.run(**{'p'+str(who):1 if i in (1,34) else 0})
        if p.read(0x514)!=6:break
    check(p.read(0x514)!=6,f'P{who} whip defeats native guard')
p.restore(encounter);hurt=False
for i in range(10):
    p.write(0x7a,24);p.write(0x7c,176);p.write(0x7a,128,2);p.write(0x7c,112,2)
    p.write(0x518,112);p.write(0x519,128);p.run()
    hurt |= bool(p.read(0x86,2) or p.read(0x568,2))
    assert p.read(0x4bd,2) != 0x4c, 'Enemy damage must not award an hourglass'
check(hurt and p.read(0x568)==0,'Enemy contact damages P2 independently')
for who in (1,2):
    fresh()
    if p.core.ij_death_mode():p.core.ij_cheat(5)
    lives=p.read(0x79);p.write(0x564,1,who);p.write(0x56b,2,who);p.run(200)
    check(p.read(0x79)==lives-1 and not p.read(0x564) and not p.read(0x564,2), f'P{who} death costs one shared life and restores the team')
fresh();p.run(1,p1=8);p.run(8);ticks=p.status()[5];p.run(20)
check(p.status()[2] and p.status()[5]==ticks,'Native pause freezes both players')
check(p.core.ij_cheat(0) and p.core.ij_cheat(1) and p.read(0x79)==9,'Native cheat actions apply')
p.run(1,p1=8);p.run(8)
check(not p.status()[2],'Native pause resumes')
for who in (1,2):
    fresh();p.write(0x9a,0);p.write(0x9d,1)
    for n in (1,2):
        for a,v in ((0x7a,144 if n==who else 112),(0x7b,0),(0x7c,176),(0x7d,0)):p.write(a,v,n)
    for i in range(100):p.run(**{'p'+str(who):16 if 15<=i<70 else 0})
    check(p.read(0xa0)==32 and p.status()[6],f'P{who} enters underground with team')
fresh();p.run(85,p1=128,p2=128);p.run(30);camera=p.read(0x9d)*256+p.read(0x9a)
for i in range(90):
    p.run(); assert camera==p.read(0x9d)*256+p.read(0x9a),'Idle camera drift'
check(camera>10,'Shared camera follows movement and settles')
p.screenshot(ROOT/'diagnostics/native-verified.png')
p.close()
print('ALL NATIVE CHECKS PASSED',flush=True)
