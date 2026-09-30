"""QS1: held Start must toggle once with P1 OUT; P2 HUD stays green."""
from PIL import Image
from native_probe import Probe, ROOT

p = Probe()
fixture = (ROOT/'tests/qs1-freeze-hud.state').read_bytes()
for who in (1, 2):
    p.restore(fixture)
    p.run(2)
    assert p.core.ij_player_out(1) and p.status()[2]
    for paused in (False, True, False):
        p.run(10, **{'p'+str(who): 8})
        p.run(2)
        assert bool(p.status()[2]) == paused, (who, p.status())
        passes = p.status()[5]
        p.run(8)
        assert (p.status()[5] == passes) == paused
    p.run(60, p2=128)
    assert p.status()[5] > passes
    raw, w, h, pitch = p.frame
    im = Image.frombytes('RGB', (w,h), raw, 'raw', 'BGRX', pitch)
    colors = set(im.crop((0,40,w,80)).getdata())
    assert colors <= {(0,0,0), (255,255,255), (168,237,176), (66,190,104)}, colors
    p.screenshot(ROOT/f'diagnostics/qs1-pause-hud-fixed-{who}.png')

# Damage, missing equipment and native HUD cache values must not supply
# the second HUD's palette. Use the previously reported damage save too.
p.restore((ROOT/'tests/qs1-hit-hud.state').read_bytes())
for weapon in (0, 1, 2, 4):
    p.write(0x82, weapon)
    p.write(0x495, 0)
    p.write(0x569, 25)
    p.run(4)
    raw, w, h, pitch = p.frame
    im = Image.frombytes('RGB', (w,h), raw, 'raw', 'BGRX', pitch)
    colors = set(im.crop((0,40,w,80)).getdata())
    assert colors <= {(0,0,0), (255,255,255), (168,237,176), (66,190,104)}, colors
state = p.save()
p.run(20, p2=64)
future, image = p.save(), p.frame
p.restore(state)
p.run(20, p2=64)
assert p.save() == future and p.frame == image
p.close()
print('PASS: held Start toggles once from either pad with P1 OUT; green HUD survives damage and weapon changes; replay matches')
