-- Exercise the actual frontend with deterministic keyboard events.
local root=assert(os.getenv('INDIANA_COOP_ROOT'))..'/'
local originalFrame=emu.frameadvance
local originalDofile=dofile
local coop,frame,startX
dofile=function(path)
 local result=originalDofile(path)
 if path==root..'coop.lua' then coop=result end
 return result
end
frame=0
input.get=function()
 if not coop or coop.passes<35 then return {enter=frame%24==1,space=frame%12==1} end
 if not startX then startX=coop.state().p2[0x7a] end
 return {right=coop.passes<65,L=coop.passes>=65 and coop.passes<80}
end
emu.frameadvance=function()
 frame=frame+1
 if frame%100==0 then
  local f=assert(io.open(root..'diagnostics/frontend-progress.txt','w'))
  f:write(string.format('frame=%d passes=%d pc=%04x buttons=%d\n',frame,coop.passes,memory.getregister('pc'),memory.readbyte(0xf7))); f:close()
 end
 if coop and coop.passes>=90 then
  local f=assert(io.open(root..'diagnostics/frontend.txt','w'))
  local s=coop.state()
  local ok=startX and s.p2[0x7a]>startX+20 and s.p1[0x7a]<s.p2[0x7a]
  f:write(ok and 'PASS frontend boots and keyboard controls move P2 independently\n' or 'FAIL frontend input\n')
  f:close(); gui.savescreenshotas(root..'diagnostics/playable-preview.png'); originalFrame(); os.exit()
 end
 if frame>3200 then
  local f=assert(io.open(root..'diagnostics/frontend.txt','w')); f:write(string.format('FAIL frontend boot timeout passes=%d stage=%d pc=%04x input=%d\n',coop.passes,memory.readbyte(0xa0),memory.getregister('pc'),memory.readbyte(0xf7))); f:close(); gui.savescreenshotas(root..'diagnostics/frontend-timeout.png'); originalFrame(); os.exit()
 end
 originalFrame()
end
emu.speedmode('maximum')
assert(loadfile(root..'play.lua'))()
