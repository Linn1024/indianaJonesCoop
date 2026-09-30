local root=os.getenv('INDIANA_COOP_ROOT')..'/'
local log=assert(io.open(root..'diagnostics/features.txt','w')); log:setvbuf('no')
local function check(ok,s)
 log:write((ok and 'PASS ' or 'FAIL ')..s..'\n')
 if not ok then log:close(); os.exit() end
end
local coop=dofile(root..'coop.lua')
local base=savestate.create(root..'tests/mexico-start.fc0')
emu.speedmode('maximum')
local function frame(a,b) joypad.set(1,a or {}); joypad.set(2,b or {}); emu.frameadvance() end
local function fresh()
 savestate.load(base); coop.reset(); coop.invincible=false
 for i=1,4 do frame() end
end
-- Exercise the actual native hat pickup for either player and both hat types.
for player=1,2 do
 for variant=1,2 do
  fresh()
  for n=1,2 do
   coop.setplayer(n,0x7a,n==player and 128 or 32)
   coop.setplayer(n,0x7c,176); coop.setplayer(n,0x83,1); coop.setplayer(n,0x495,0)
  end
  for a=0x514,0x523 do memory.writebyte(a,0) end
  memory.writebyte(0x514,variant==1 and 0x4a or 0x4e)
  memory.writebyte(0x518,184); memory.writebyte(0x519,128)
  for i=1,8 do frame() end
  local s=coop.state()
  check(s.p1[0x495]==variant and s.p2[0x495]==variant,
   'P'..player..' native hat pickup '..variant..' equips both players')
 end
end
fresh()
frame({start=true}); for i=1,8 do frame() end
check(coop.paused,'Native Enter/Start pause is detected')
local count=coop.passes
for i=1,8 do frame() end
check(coop.passes==count,'Both player simulations stay frozen during pause')
local menu=dofile(root..'pause-menu.lua')(coop,dofile(root..'controls.lua'))
menu.update({a=true,b=true,c=true},{left=true},{})
check(menu.open,'A+B+C reveals the menu while paused')
check(not coop.invincible,'Opening chord does not also activate a cheat')
menu.update({},{},{})
local a,b=menu.update({space=true},{A=true},{})
check(coop.invincible and not a.A,'Invincibility toggles and menu swallows gameplay input')
menu.update({},{},{})
menu.update({s=true},{down=true},{})
menu.update({},{},{})
menu.update({space=true},{A=true},{})
check(memory.readbyte(0x79)==9,'Menu sets shared lives to nine')
gui.register(function() menu.draw() end)
gui.savescreenshotas(root..'diagnostics/cheat-menu.png'); frame()
frame({start=true}); for i=1,8 do frame() end
menu.update({},{},{})
check(not coop.paused and not menu.open,'Resuming closes the menu')
menu.update({a=true,b=true,c=true},{},{})
check(not menu.open,'Cheat chord cannot open the menu during gameplay')
-- Load the reported slot including its native Lua sidecar, without modifying it.
local path=root..'tests/slot-1-sprites.fc1'
local slot=savestate.create(path)
-- FCEUX 2.6.6's loadscriptdata still expects the old filename metatable field.
debug.getmetatable(slot).filename=path
savestate.load(slot); coop.import(savestate.loadscriptdata(slot))
check(coop.state().p2~=nil,'Reported slot 1 restores both player contexts')
local mapped=memory.readbyte(0xec)
local selection=bit.band(memory.readbyte(0x4a7),7)
memory.registerwrite(0x8000,function(_,_,v) selection=bit.band(v,7) end)
memory.registerwrite(0x8001,function(_,_,v) if selection==2 then mapped=v end end)
local write=rom.writebyte
local unsafe,updates=0,0
rom.writebyte=function(address,value)
 local offset=address-(16+0x20000)
 if offset==128*1024 or offset==129*1024 then
  updates=updates+1
  if offset/1024==mapped then unsafe=unsafe+1 end
 end
 write(address,value)
end
for i=1,100 do frame({right=i<20,A=i<12},{B=i%24==1,A=i>30 and i<45}) end
check(updates>50 and unsafe==0,'Moving sprites never overwrite the currently mapped graphics bank')
gui.savescreenshotas(root..'diagnostics/slot-1-fixed.png'); frame()
local saved=savestate.create(); savestate.save(saved)
local extra=coop.export()
for i=1,15 do frame({left=true},{A=true}) end
savestate.load(saved); coop.import(extra)
local restored=coop.export()
check(restored.chrBuffers[128]~=nil and restored.chrBuffers[129]~=nil,'Save restore retains both graphics buffers')
log:write('PASS Feature regression checks complete\n'); log:close(); os.exit()
