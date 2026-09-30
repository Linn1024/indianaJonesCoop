from native_probe import Probe, ROOT

p = Probe()
p.run(10)
assert p.core.ij_death_mode() and p.core.ij_menu_visible()
idle = p.save()
p.run(120)
assert p.save() == idle, 'Game must wait for mode confirmation'
p.screenshot(ROOT/'diagnostics/start-mode-menu.png')
p.run(p2=32)
p.run()
assert not p.core.ij_death_mode()
p.run(p2=256)
p.run()
assert not p.core.ij_menu_visible()
p.boot()
assert not p.core.ij_death_mode()
shared = p.save()
p.run(p1=8)
p.run()
p.run(p1=257)
p.run()
for _ in range(5):
    p.run(p1=32)
    p.run()
p.run(p1=256)
p.run()
assert p.core.ij_death_mode(), 'Menu must select individual mode'
p.screenshot(ROOT/'diagnostics/death-mode-menu.png')
p.run(p1=8)
p.run(3)
individual = p.save()

for who in (1, 2):
    for pit in (False, True):
        p.restore(individual)
        for n in (1, 2):
            p.write(0x82, n+1, n)
        if pit:
            p.write(0x7c, 240, who)
        else:
            p.write(0x564, 1, who)
            p.write(0x56b, 2, who)
        passes = p.status()[5]
        p.run(12)
        assert p.read(0x79, who) == 2, (who, pit, p.status())
        assert p.read(0x79, 3-who) == 3
        assert p.read(0x82, who) == who+1
        assert p.read(0xa0) == 0 and p.status()[5] == passes+12
        assert abs(p.read(0x7a)-p.read(0x7a, 2)) <= 8
        assert not p.read(0x564, who)

p.restore(individual)
for n in (1, 2):
    p.write(0x564, 1, n)
    p.write(0x56b, 2, n)
p.run(50)
assert [p.read(0x79, n) for n in (1, 2)] == [2, 2]

for who in (1, 2):
    p.restore(individual)
    p.write(0x79, 1, who)
    p.write(0x564, 1, who)
    p.write(0x56b, 2, who)
    p.run(30)
    assert p.core.ij_player_out(who) and p.read(0x79, who) == 0
    assert p.read(0x79, 3-who) == 3
    assert p.read(0x78)>0, 'The surviving player must still be rendered'
    for controller in (1, 2):
        for paused in (True, False):
            p.run(10, **{'p'+str(controller): 8})
            p.run(2)
            assert bool(p.status()[2]) == paused, (who, controller, p.status())
    p.screenshot(ROOT/f'diagnostics/individual-out-{who}.png')
    state = p.save()
    x = p.read(0x7a, 3-who)
    p.run(15, **{'p'+str(3-who): 128})
    assert p.read(0x7a, 3-who) > x
    future, frame = p.save(), p.frame
    p.restore(state)
    p.run(15, **{'p'+str(3-who): 128})
    assert p.save() == future and p.frame == frame
    p.write(0x79, 1, 3-who)
    p.write(0x564, 1, 3-who)
    p.write(0x56b, 2, 3-who)
    p.run(250)
    assert not p.status()[1] and not p.status()[6]
    assert p.core.ij_player_out(1) and p.core.ij_player_out(2)
    p.screenshot(ROOT/'diagnostics/individual-gameover.png')
    for i in range(1500):
        p.run(p1=8 if i%24 == 1 else 0)
        if p.status()[1]:
            break
    assert p.status()[1] and not p.core.ij_player_out(1) and not p.core.ij_player_out(2)

p.restore(individual)
p.write(0x79, 2)
p.write(0x79, 5, 2)
p.write(0x9a, 0)
p.write(0x9d, 1)
for n in (1, 2):
    for a,v in ((0x7a,144 if n == 2 else 112),(0x7b,0),(0x7c,176),(0x7d,0)):
        p.write(a,v,n)
for i in range(100):
    p.run(p2=16 if 15 <= i < 70 else 0)
assert p.read(0xa0) == 32
assert [p.read(0x79,n) for n in (1,2)] == [2,5]
p.restore(shared)
assert not p.core.ij_death_mode()
p.restore((ROOT/'tests/qs1-hit-hud.state').read_bytes())
assert p.core.ij_death_mode(), 'Older saves without a mode default to Easy'
assert p.read(0x79,2) == p.read(0x79), 'Old shared saves initialize both individual counts'
p.close()
print('PASS: menu, individual deaths/pits, simultaneous deaths, OUT player, survivor movement, both-out game over/continue, replay, area lives, old saves')
