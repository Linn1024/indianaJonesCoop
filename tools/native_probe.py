"""Headless tests of the compiled core; no Lua or emulator UI."""
import ctypes as C
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]

class Game(C.Structure):
    _fields_ = [('path', C.c_char_p), ('data', C.c_void_p), ('size', C.c_size_t), ('meta', C.c_char_p)]

class Variable(C.Structure):
    _fields_ = [('key', C.c_char_p), ('value', C.c_char_p)]

class Probe:
    def __init__(self, options=None):
        self.core = C.CDLL(str(ROOT/'engine/fceumm_libretro.dll'))
        self.buttons = [0, 0]
        self.options = {b'fceumm_nospritelimit': b'enabled', b'fceumm_region': b'NTSC'}
        if options: self.options.update(options)
        self.root_bytes = str(ROOT).encode()
        self.frame = None
        self.samples = bytearray()
        self.format = 0
        def env(cmd, data):
            if cmd in (9, 31):
                C.cast(data, C.POINTER(C.c_char_p))[0] = self.root_bytes
                return True
            if cmd == 10:
                self.format = C.cast(data, C.POINTER(C.c_int))[0]
                return True
            if cmd == 16:
                arr = C.cast(data, C.POINTER(Variable)); i = 0
                while arr[i].key:
                    self.options.setdefault(arr[i].key, arr[i].value.split(b'; ', 1)[-1].split(b'|')[0]); i += 1
                return True
            if cmd == 15:
                var = C.cast(data, C.POINTER(Variable)).contents
                var.value = self.options.get(var.key)
                return bool(var.value)
            if cmd in (3, 17):
                C.cast(data, C.POINTER(C.c_bool))[0] = cmd == 3
                return True
            if cmd == 52:
                C.cast(data, C.POINTER(C.c_uint))[0] = 0
                return True
            return cmd in (11, 35, 37)
        def video(data, w, h, pitch):
            if data: self.frame = (C.string_at(data, h*pitch), w, h, pitch)
        def audio(data, n):
            self.samples.extend(C.string_at(data, n*4)); return n
        signatures = {
            'environment': (C.CFUNCTYPE(C.c_bool, C.c_uint, C.c_void_p), env),
            'video_refresh': (C.CFUNCTYPE(None, C.c_void_p, C.c_uint, C.c_uint, C.c_size_t), video),
            'audio_sample_batch': (C.CFUNCTYPE(C.c_size_t, C.c_void_p, C.c_size_t), audio),
            'audio_sample': (C.CFUNCTYPE(None, C.c_int16, C.c_int16), lambda l,r: None),
            'input_poll': (C.CFUNCTYPE(None), lambda: None),
            'input_state': (C.CFUNCTYPE(C.c_int16, C.c_uint, C.c_uint, C.c_uint, C.c_uint),
                            lambda p,d,i,b: int(p<2 and bool(self.buttons[p] & (1<<b)))),
        }
        self.callbacks = []
        for name, (sig, fn) in signatures.items():
            cb = sig(fn); self.callbacks.append(cb); getattr(self.core, 'retro_set_'+name)(cb)
        self.core.retro_init()
        path = ROOT/'Young Indiana Jones Chronicles, The (USA).nes'
        data = path.read_bytes(); self.rom = C.create_string_buffer(data)
        game = Game(str(path).encode(), C.addressof(self.rom), len(data), None)
        self.core.retro_load_game.argtypes = [C.POINTER(Game)]
        self.core.retro_load_game.restype = C.c_bool
        assert self.core.retro_load_game(C.byref(game))
        for p in (0,1): self.core.retro_set_controller_port_device(p, 1)
        self.core.retro_serialize_size.restype = C.c_size_t
        for name in ('retro_serialize', 'retro_unserialize'):
            fn = getattr(self.core, name); fn.argtypes = [C.c_void_p, C.c_size_t]; fn.restype = C.c_bool
        self.core.ij_read.restype = C.c_uint
        assert self.status()[0], 'Core did not recognize cartridge'

    def run(self, n=1, p1=0, p2=0):
        self.buttons[:] = p1, p2
        for _ in range(n): self.core.retro_run()

    def status(self):
        data = (C.c_uint*12)(); self.core.ij_status(data); return list(data)

    def read(self, addr, player=1): return self.core.ij_read(player, addr)
    def write(self, addr, value, player=1): self.core.ij_write(player, addr, value)
    def save(self):
        n = self.core.retro_serialize_size(); buf = C.create_string_buffer(n)
        assert self.core.retro_serialize(buf, n); return buf.raw
    def restore(self, data):
        buf = C.create_string_buffer(data); assert self.core.retro_unserialize(buf, len(data))
    def boot(self):
        for frame in range(2800):
            self.run(p1=(8 if frame%24==1 else 0) | (256 if frame%12==1 else 0))
            if self.status()[5]>35:
                self.run(90); return
        raise AssertionError(('cold boot', self.status()))
    def screenshot(self, path):
        data,w,h,pitch = self.frame
        assert self.format == 1
        Image.frombytes('RGB', (w,h), data, 'raw', 'BGRX', pitch).save(path)
    def close(self):
        self.core.retro_unload_game(); self.core.retro_deinit()

if __name__ == '__main__':
    p = Probe(); p.boot(); print('Booted:', p.status(), flush=True)
    (ROOT/'tests/native-mexico.state').write_bytes(p.save())
    p.screenshot(ROOT/'diagnostics/native-boot.png')
    p.close()
