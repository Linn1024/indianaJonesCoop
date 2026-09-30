local root=assert(os.getenv('INDIANA_COOP_ROOT'))..'/'
local log=assert(io.open(root..'diagnostics/campaign.txt','w')); log:setvbuf('no')
local base=savestate.create(root..'tests/mexico-start.fc0')
savestate.load(base)
local coop=dofile(root..'coop.lua')
emu.speedmode('maximum')
for stage=0,35 do
 if stage~=31 then
  savestate.load(base); coop.reset()
  memory.writebyte(0xa0,stage)
  memory.writebyte(0xa3,0); memory.writebyte(0xa4,32); memory.writebyte(0xa5,112); memory.writebyte(0xa6,0)
  memory.setregister('pc',0xc1e1)
  local count=coop.passes
  for i=1,160 do joypad.set(1,{}); joypad.set(2,{}); emu.frameadvance() end
  local s=coop.state()
  log:write(string.format('stage=%02d passes=%d p2=%s phase=%d pc=%04x final=%d\n',stage,coop.passes-count,tostring(s.p2~=nil),s.phase,memory.getregister('pc'),s.stage))
  gui.savescreenshotas(root..'diagnostics/stage-'..stage..'.png')
 end
end
log:close(); os.exit()
