local root=assert(os.getenv('INDIANA_COOP_ROOT'),'Launch using Play Indiana Co-op.cmd')..'/'
local coop=dofile(root..'coop.lua')
local controls=dofile(root..'controls.lua')
local menu=dofile(root..'pause-menu.lua')(coop,controls)
local previous={}
local help=300
local startupPending=true
local function down(keys,key) return keys[key] or keys[string.upper(key)] end
local function pad(keys,n,map)
 -- get() includes the previous Lua override in the Qt build and would latch
 -- released keys. getimmediate() reads the physical controller instead.
 local result=joypad.getimmediate(n) or {}
 for button,key in pairs(map) do result[button]=result[button] or down(keys,key) or false end
 return result
end
gui.register(function()
 local stage=memory.readbyte(0xa0)
 if stage==19 or stage==20 then
  gui.text(9,219,'FLIGHT: P1 pilots / P2 '..controls.p2.B..' gun, '..controls.p2.A..' bomb','#63e7ff','black')
 elseif coop.active then
  if help>0 then gui.text(9,219,'F1 help  '..controls.rejoin1..'/'..controls.rejoin2..' rejoin  F5/F8 save/load','white','black') end
 else
  gui.text(8,205,'INDIANA JONES - LOCAL CO-OP','#63e7ff','black')
  gui.text(8,215,'Enter: start / skip intro   F1: controls','white','black')
 end
 if help>240 then
  gui.box(15,63,240,160,'#101820e0','#63e7ff')
  gui.text(23,72,'TWO EXPLORERS. ONE ADVENTURE.','#63e7ff','clear')
  gui.text(23,86,'P1: '..table.concat({controls.p1.up,controls.p1.left,controls.p1.down,controls.p1.right},'/'),'white','clear')
  gui.text(23,98,'Jump: '..controls.p1.A..'  Whip: '..controls.p1.B,'white','clear')
  gui.text(23,110,'P2: '..table.concat({controls.p2.up,controls.p2.left,controls.p2.down,controls.p2.right},'/'),'white','clear')
  gui.text(23,122,'Jump: '..controls.p2.A..'  Whip: '..controls.p2.B,'white','clear')
  gui.text(23,134,'Edit keys in controls.lua','white','clear')
  gui.text(23,146,'F5 save  F8 load   F1 close help','white','clear')
 end
 menu.draw()
end)
while true do
 local keys=input.get()
 local a=pad(keys,1,controls.p1)
 local b=pad(keys,2,controls.p2)
 if not coop.paused then
  if down(keys,controls.rejoin1) and not down(previous,controls.rejoin1) then coop.rejoin(1) end
  if down(keys,controls.rejoin2) and not down(previous,controls.rejoin2) then coop.rejoin(2) end
 end
 a,b=menu.update(keys,a,b)
 if keys.F1 and not previous.F1 then help=help>240 and 0 or 1000000 end
 joypad.set(1,a); joypad.set(2,b)
 previous=keys
 if help>0 then help=help-1 end
 emu.frameadvance()
 if startupPending then
  local session=os.getenv('INDIANA_COOP_SESSION')
  if session then
   local file=assert(io.open(root..'diagnostics/launch-ready.txt','w'))
   file:write(session); file:close()
  end
  startupPending=false
 end
end
