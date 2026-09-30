"""Co-op must render all sprites even when a Libretro host defaults to eight."""
from native_probe import Probe, ROOT

def run(option):
    p = Probe({b'fceumm_nospritelimit': option})
    p.restore((ROOT/'tests/qs1-motorcycle.state').read_bytes())
    frames = []
    crowded = False
    for _ in range(90):
        p.run()
        frames.append(p.frame)
        ys = [p.read(0x200+i) for i in range(0, p.read(0x78), 4)]
        crowded |= any(sum(y <= line < y+16 for y in ys) > 8 for line in range(240))
    assert crowded, 'Fixture did not exercise the hardware sprite limit'
    saved = p.save()
    p.run(10)
    after = p.frame
    p.restore(saved)
    p.run(10)
    assert p.frame == after
    p.close()
    return frames

assert run(b'disabled') == run(b'enabled'), 'Host default still hides co-op sprites'
print('PASS: crowded QS1 renders identically with host sprite-limit option disabled/enabled; save replay matches')
