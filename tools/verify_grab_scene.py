"""Train boss intro and owner-specific grab/throw sprites and release."""
from native_probe import Probe, ROOT

p = Probe()
fixture = (ROOT/'tests/qs1-grab-scene.state').read_bytes()
for who in (1, 2):
    p.restore(fixture)
    p.run(120)
    assert p.read(0x9f) == 0 and p.read(0x517) != 255, 'Opening scroll is stuck'
    for frame in range(400):
        p.run(**{'p'+str(who): 128})
        if p.read(0x57f, who) == 8 and p.read(0x517) == 12: break
    else: raise AssertionError(('Boss did not grab player', who))
    p.run(3)
    assert p.read(0x57f, who) and not p.read(0x57f, 3-who)
    victim = [p.read(0x202+i) for i in range(0, p.read(0x78), 4)
              if p.read(0x201+i) >= 192 and not (p.read(0x202+i) & 3)]
    assert victim and all(bool(a & 4) == (who == 2) for a in victim), (who, victim)
    assert any((p.read(0x202+i)&3) == 1 and not (p.read(0x202+i)&4)
               for i in range(0, p.read(0x78), 4)), 'Enemy was recolored'
    # Native flying boxes set unused bits $0C; these are not P2's $04 tag.
    boxes = [i for i in range(0, p.read(0x78), 4)
             if p.read(0x201+i) == 0x69 and (p.read(0x202+i)&0x1c) == 0x0c]
    assert boxes, 'No thrown boxes exercised'
    raw, width, height, pitch = p.frame
    for i in boxes:
        x, y = p.read(0x203+i), p.read(0x200+i)+33
        gold = 0
        for row in range(max(0, y-3), min(height, y+19)):
            for col in range(max(0, x-3), min(width, x+11)):
                b, g, r = raw[row*pitch+col*4:row*pitch+col*4+3]
                gold += r > 150 and r > g*1.15 and g > b
        assert gold, ('Thrown box lost its native gold palette', who, x, y)
    p.screenshot(ROOT/f'diagnostics/grab-fixed-player-{who}.png')
    saved = p.save()
    for frame in range(150):
        p.run()
        assert not p.read(0x57f, 3-who), 'Grab switched to the partner'
        if not p.read(0x57f, who) and p.read(0x524) == 0: break
    else: raise AssertionError(('Grabbed player never released', who))
    assert p.read(0x7a, who) > 8 and p.read(0xa0) == 18
    future, image = p.save(), p.frame
    p.restore(saved)
    p.run(frame+1)
    assert p.save() == future and p.frame == image
p.close()
print('PASS: QS1 intro, either player grabbed/released, correct victim color, enemy and thrown boxes unchanged, replay')
