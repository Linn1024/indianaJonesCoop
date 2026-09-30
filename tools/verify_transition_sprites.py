"""QS1: co-op sprites/HUD disappear during fade/title and return in gameplay."""
from PIL import Image
from native_probe import Probe, ROOT

def picture(p):
    data, w, h, pitch = p.frame
    return Image.frombytes('RGB', (w, h), data, 'raw', 'BGRX', pitch)

p = Probe()
p.restore((ROOT/'tests/qs1-transition-sprites.state').read_bytes())
p.run(31)
assert picture(p).getbbox() is None, 'Extra graphics remain after fade to black'
p.run(90)
title = picture(p)
assert title.getbbox() is not None, 'Native level title disappeared'
assert title.crop((0, 0, 256, 80)).getbbox() is None, 'HUD remains over level title'
saved = p.save()
p.run(120)
future, frame = p.save(), p.frame
assert p.status()[8] == 8 and p.status()[5] > 43545, 'Next level did not start'
hud = picture(p).crop((0, 40, 256, 80))
assert any(g > r and g > b for r, g, b in hud.getdata()), 'Green HUD did not return'
p.restore(saved)
p.run(120)
assert p.save() == future and p.frame == frame, 'Transition save replay differs'
p.close()
print('PASS: QS1 clean fade/title, restored green HUD, next-level gameplay and transition save replay')
