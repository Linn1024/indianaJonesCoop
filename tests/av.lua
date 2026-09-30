local root=assert(os.getenv('INDIANA_COOP_ROOT'))..'/'
local log=assert(io.open(root..'diagnostics/av.txt','w')); log:setvbuf('no')
local function check(ok,s)
 if not ok or not s:find('frame ') then log:write((ok and 'PASS ' or 'FAIL ')..s..'\n') end
 if not ok then log:close(); os.exit() end
end
local base=savestate.create(root..'tests/mexico-start.fc0')
local coop=dofile(root..'coop.lua')
emu.speedmode('maximum')
local writes={}
for a=0x4000,0x4015 do
 if a~=0x4014 then
  local address=a
  memory.registerwrite(address,function(_,_,value)
   writes[#writes+1]=string.char(address-0x4000,value)
  end)
 end
end
local function frame(a,b)
 joypad.set(1,a or {}); joypad.set(2,b or {}); emu.frameadvance()
end
local nativeAudio
for variant=1,2 do
 savestate.load(base); coop.reset(); coop.enabled=variant==2; writes={}
 for i=1,600 do frame() end
 local trace=table.concat(writes)
 local f=assert(io.open(root..'diagnostics/audio-'..variant..'.bin','wb')); f:write(trace); f:close()
 log:write('Audio variant '..variant..': '..#writes..' register writes\n')
 if variant==1 then nativeAudio=trace else
  check(#writes>600,'Music engine continues writing audio registers for 600 frames')
  local common=math.min(#trace,#nativeAudio)
  check(common/math.max(#trace,#nativeAudio)>0.98 and trace:sub(1,common)==nativeAudio:sub(1,common),'Music register sequence matches the original game (allowing frame-boundary timing)')
 end
end
savestate.load(base); coop.reset(); coop.enabled=true
for i=1,4 do frame() end
local palette=memory.readbyterange(0xb0,32)
local camera=memory.readbyte(0x9d)*256+memory.readbyte(0x9a)
for i=1,180 do
 frame()
 check(memory.readbyte(0x9d)*256+memory.readbyte(0x9a)==camera,'Idle camera frame '..i)
end
local moved=0
for i=1,85 do
 frame({right=true},{right=true})
 local nextCamera=memory.readbyte(0x9d)*256+memory.readbyte(0x9a)
 local delta=nextCamera-camera
 check(delta>=0 and delta<=2,'Rightward camera stays monotonic and bounded frame '..i)
 moved=moved+delta; camera=nextCamera
end
check(moved>10,'Camera follows the team right')
for i=1,30 do frame() end
camera=memory.readbyte(0x9d)*256+memory.readbyte(0x9a)
for i=1,90 do
 frame()
 check(memory.readbyte(0x9d)*256+memory.readbyte(0x9a)==camera,'Camera settles after movement frame '..i)
end
check(memory.readbyterange(0xb0,32)==palette,'Original world and sprite palette colors remain intact')
local p0,p1=0,0
for a=0x200,0x2fc,4 do
 if memory.readbyte(a)<240 then
  local pal=bit.band(memory.readbyte(a+2),3)
  if pal==0 then p0=p0+1 elseif pal==1 then p1=p1+1 end
 end
end
check(p0>=4 and p1>=4,'Both original-color and alternate-color player sprites are present')
gui.savescreenshotas(root..'diagnostics/av-preview.png'); frame()
log:write('PASS Audio, camera, and palette regression checks completed\n'); log:close(); os.exit()
