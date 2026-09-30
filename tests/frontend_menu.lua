local root=os.getenv('INDIANA_COOP_ROOT')..'/'
local log=assert(io.open(root..'diagnostics/frontend_menu.txt','w')); log:setvbuf('no')
local function check(ok,s)
 log:write((ok and 'PASS ' or 'FAIL ')..s..'\n')
 if not ok then log:close(); os.exit() end
end
local originalDofile,advance,draw=dofile,emu.frameadvance,gui.text
local coop,menu
local frame,labels,minY=0,0,255
dofile=function(path)
 local result=originalDofile(path)
 if path==root..'coop.lua' then
  coop=result; savestate.load(savestate.create(root..'tests/mexico-start.fc0')); coop.reset()
 elseif path==root..'controls.lua' then result.p2.A='r'
 elseif path==root..'pause-menu.lua' then
  local constructor=result
  result=function(...) menu=constructor(...); return menu end
 end
 return result
end
gui.text=function(x,y,text,...)
 if text=='1' or text=='2' then labels=labels+1 end
 return draw(x,y,text,...)
end
input.get=function()
 if frame==5 or frame==24 then return {enter=true} end
 if frame==14 then return {a=true,b=true,c=true} end
 if frame==17 or frame==21 then return {space=true} end
 if frame==19 then return {s=true} end
 if frame>=40 and frame<56 then return {r=true} end
 return {}
end
emu.frameadvance=function()
 advance(); frame=frame+1
 if frame==14 then check(coop.paused,'Frontend Enter pauses native gameplay') end
 if frame==15 then check(menu.open and not coop.invincible,'Frontend ABC opens menu without activating a cheat') end
 if frame==18 then check(coop.invincible,'Frontend Space applies selected cheat') end
 if frame==22 then check(memory.readbyte(0x79)==9,'Frontend menu navigation applies lives cheat') end
 if frame==35 then check(not coop.paused and not menu.open,'Frontend Enter resumes and closes menu') end
 if frame>=40 and coop.state().p2 then minY=math.min(minY,coop.state().p2[0x7c]) end
 if frame==75 then
  check(minY<160,'Custom controls.lua mapping drives P2 jump')
  check(labels==0,'Frontend draws no player-number labels')
  log:close(); os.exit()
 end
end
emu.speedmode('maximum')
assert(loadfile(root..'play.lua'))()
