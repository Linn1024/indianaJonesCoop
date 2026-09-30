-- Keep the real input.get and joypad.getimmediate calls: a mocked keyboard
-- cannot catch a crash in the emulator's input implementation.
local root=assert(os.getenv('INDIANA_COOP_ROOT'))..'/'
local originalFrame=emu.frameadvance
local frames=0
emu.frameadvance=function()
 originalFrame()
 frames=frames+1
 if frames==120 then
  local f=assert(io.open(root..'diagnostics/raw_frontend.txt','w'))
  f:write('PASS 120 real frontend frames with unmodified keyboard and controller APIs\n')
  f:close(); os.exit()
 end
end
assert(loadfile(root..'play.lua'))()
