-- Native two-player context switching for The Young Indiana Jones Chronicles (USA).
-- Loaded by FCEUX. The cartridge CPU runs both players, with one shared world.
local M = {enabled=true, ticks=0, passes=0, paused=false, invincible=false}
local r,w = memory.readbyte,memory.writebyte
local get,set = memory.getregister,memory.setregister
local addresses = {}
local function range(a,b) for i=a,b do addresses[#addresses+1]=i end end
range(0x59,0x5b); range(0x7a,0x89); range(0x8b,0x97)
range(0x564,0x564); range(0x568,0x569); range(0x56b,0x56b)
range(0x56d,0x575); range(0x577,0x5a4); range(0x5d2,0x5db)
range(0x495,0x495)
local function capture()
 local t={} for _,a in ipairs(addresses) do t[a]=r(a) end return t
end
local function install(t) for _,a in ipairs(addresses) do w(a,t[a] or 0) end end
local function bytes(a,n) local t={} for i=0,n-1 do t[i+1]=r(a+i) end return t end
local function put(a,t) for i,v in ipairs(t) do w(a+i-1,v) end end
local function registers() local t={} for _,k in ipairs({'a','x','y','s','p'}) do t[k]=get(k) end return t end
local function copy(t)
 if type(t)~='table' then return t end
 local out={} for k,v in pairs(t) do out[k]=copy(v) end return out
end
local function restore(t) for k,v in pairs(t) do set(k,v) end end
local function native() return r(0xe2)==12 end
local p2,p1,phase,regs,oam,shared,p2oam,p2bank
local damage,attack
local objectP1,objectBoxes,objectRegs,objectEntry,objectPrepared
local mergedChr
local chrBuffers={}
local compositeBank=128
local cameraX,cameraY,cameraAnchor
phase=0
local chr={}
for bank=0,127 do
 chr[bank]={}
 for i=0,1023 do chr[bank][i+1]=rom.readbyte(16+0x20000+bank*1024+i) end
end
local function reset()
 p2=nil; p1=nil; phase=0; M.active=false; p2oam=nil; damage=nil; attack=nil
 objectP1=nil; objectBoxes=nil; objectRegs=nil; objectEntry=nil; objectPrepared=nil
 cameraX=nil; cameraY=nil; cameraAnchor=nil
 M.paused=false
end
local function other() return objectP1 or p2 end
local function saveOther(t) if objectP1 then objectP1=t else p2=t end end
M.reset=reset
M.state=function() return {p1=capture(),p2=p2,phase=phase,ticks=M.ticks,stage=r(0xa0)} end
M.setplayer=function(n,a,v) if n==1 then w(a,v) elseif p2 then p2[a]=v end end
memory.registerexec(0xc1e1,reset)
memory.registerexec(0xc1a0,reset)
-- Native pause loops keep emulating, so keyboard menus can still receive input.
memory.registerexec(0xc0e6,function() if M.active then M.paused=true end end)
memory.registerexec(0xc100,function() M.paused=false end)
-- A hat pickup equips the partner too. Damage remains private to each player.
local function partnerHat(outfit,hat)
 if not M.enabled or not p2 then return end
 local partner=other(); partner[0x83]=outfit; partner[0x495]=hat
end
memory.registerexec(0xe0dc,function() partnerHat(0,1) end)
memory.registerexec(0xe11c,function() partnerHat(2,2) end)
-- The two original flight sections use a single aircraft: P1 pilots, P2 can
-- operate its native gun and bombs. They have a separate CPU update loop.
memory.registerexec(0xfa71,function()
 if not M.enabled then return end
 local stage=r(0xa0)
 if stage==19 or stage==20 then
  w(0xf5,bit.bor(r(0xf5),bit.band(r(0xf6),0xc0)))
  w(0xf7,bit.bor(r(0xf7),bit.band(r(0xf8),0xc0)))
 end
end)
memory.registerexec(0x808d,function()
 if not M.enabled or not native() or phase==2 then return end
 M.active=true; M.ticks=M.ticks+1
 M.paused=false
 if not p2 then
  p2=capture(); p2[0x7a]=math.min(224,r(0x7a)+24)
 end
 phase=1
 if M.invincible then w(0x569,30); p2[0x569]=30 end
end)
-- Test each enemy contact point against both native player hitboxes. The enemy
-- itself still executes once. RTS is prefetched, so a synthetic return re-enters
-- the same native routine for P2 without duplicating the caller's AI update.
local function pushreturn(pc)
 local s=get('s'); local v=pc-1
 w(0x100+s,math.floor(v/256)); w(0x100+(s-1)%256,v%256)
 set('s',(s-2)%256)
end
memory.registerexec(0xea61,function()
 if M.enabled and p2 and phase==0 and not damage then
  damage={player=capture(),entry=registers(),pass=1}
 end
end)
local function damage_end()
 if not damage then return end
 if damage.pass==1 then
  damage.player=capture(); damage.result=registers(); damage.pass=2
  install(other()); restore(damage.entry); pushreturn(0xea61)
 else
  saveOther(capture())
  local hit=get('a')==0 or damage.result.a==0
  install(damage.player); restore(damage.result)
  if hit then set('a',0); set('p',bit.bor(bit.band(get('p'),0x7d),2)) end
  damage=nil
 end
end
memory.registerexec(0xeab2,damage_end)
memory.registerexec(0xeb0b,damage_end)
-- Run the original weapon collision checks twice, including consumable shots.
memory.registerexec(0xed63,function()
 if M.enabled and p2 and phase==0 and not attack then
  attack={player=capture(),pass=1}
 end
end)
memory.registerexec(0xee79,function()
 if not attack then return end
 if attack.pass==1 then
  attack.player=capture(); attack.boxes=bytes(0x5c,0x18); attack.enemy=bytes(0x74,4)
  attack.result=registers(); attack.hit=r(0x73); attack.pass=2
  install(other())
  -- Compute P2 weapon boxes without repeating E4C2's world-object scrolling.
  put(0x6008,{0x20,0x3a,0xeb,0x4c,0x63,0xed})
  pushreturn(0x6008)
 else
  local hit=r(0x73); saveOther(capture())
  install(attack.player); put(0x5c,attack.boxes); w(0x73,math.min(255,attack.hit+hit))
  restore(attack.result); attack=nil
 end
end)
memory.registerexec(0x600b,function()
 if attack and attack.pass==2 then put(0x74,attack.enemy) end
end)
-- Native object AI, pickups and vehicle/platform scripts receive the nearer
-- player. Their state machine still advances once per frame. Contact and weapon
-- checks above test the other player too.
for _,entry in ipairs({0xc53f,0xc556,0xc56d,0xc580,0xc593}) do
 memory.registerexec(entry,function()
  if not M.enabled or not p2 or phase~=0 then return end
  if objectPrepared then objectPrepared=nil; return end
  local slot=get('x'); local x=r(0x519+slot); local y=r(0x518+slot)
  local d1=math.abs(r(0x7a)-x)+math.abs(r(0x7c)-y)
  local d2=math.abs(p2[0x7a]-x)+math.abs(p2[0x7c]-y)
  if d2>=d1 then return end
  objectP1=capture(); objectBoxes=bytes(0x5c,0x18); objectRegs=registers(); objectEntry=entry
  install(p2)
  put(0x6020,{0x20,0x3a,0xeb,0xea})
  set('pc',0x6020)
 end)
end
memory.registerexec(0x6023,function()
 if objectP1 then
  restore(objectRegs); objectPrepared=true; set('pc',objectEntry-1)
 end
end)
for _,pc in ipairs({0xc542,0xc559,0xc570,0xc583,0xc596}) do
 memory.registerexec(pc,function()
  if objectP1 then
   p2=capture(); install(objectP1); put(0x5c,objectBoxes)
   objectP1=nil; objectBoxes=nil; objectRegs=nil; objectEntry=nil
  end
 end)
end
-- Use a stable shared dead zone. The native $7E anchor is a screen coordinate,
-- not the other player's previous X: mixing the two accumulates false scroll.
memory.registerexec(0x9a0c,function()
 if not M.enabled or not native() or not p2 then return end
 if phase==2 then
  w(0x7a,math.max(8,math.min(237,r(0x7a))))
  set('pc',0x9b67) -- same prefetched LDA zp opcode; no second scroll update
 else
  cameraX=r(0x7a); cameraY=r(0x7c); cameraAnchor=r(0x7e)
  local left=math.min(cameraX,p2[0x7a]); local right=math.max(cameraX,p2[0x7a])
  local focus=math.floor((left+right)/2)
  local delta=0
  if focus>144 then delta=math.min(2,focus-144)
  elseif focus<112 then delta=math.max(-2,focus-112) end
  -- Keep the trailing player visible without forcing them through terrain.
  if (delta>0 and left<24) or (delta<0 and right>224) then delta=0 end
  w(0x9e,0); w(0x56c,0); w(0x7e,128); w(0x7a,128+delta)
 end
end)
memory.registerexec(0x8b19,function()
 if not M.enabled or not native() or phase~=1 or not cameraX then return end
 local dx=memory.readbytesigned(0x9e)
 local dy=memory.readbytesigned(0x566)
 w(0x7a,math.max(8,math.min(237,cameraX-dx)))
 w(0x7c,cameraY-dy)
 w(0x7e,cameraAnchor)
 p2[0x7a]=math.max(8,math.min(237,p2[0x7a]-dx))
 p2[0x7c]=(p2[0x7c]-dy)%256
 cameraX=nil
end)
memory.registerexec(0x861d,function()
 if not M.enabled or not native() or not p2 then return end
 if phase==1 then
  p1=capture(); regs=registers(); oam=bytes(0x200,256)
  shared={}
  for _,a in ipairs({0x78,0x9e,0x566,0x56c,0xf5,0xf7,0xec,0x9b}) do shared[a]=r(a) end
  install(p2); w(0xf5,r(0xf6)); w(0xf7,r(0xf8))
  w(0x9e,0); w(0x566,0)
  -- FCEUX fetches the opcode before calling registerexec. Redirect this JSR
  -- through RAM operands, then return to a NOP bridge to restore P1.
  -- $0600-$07FF belongs to the original sound engine. Put all continuation
  -- code in MMC3 cartridge WRAM, never in the game's internal work RAM.
  put(0x6000,{0x20,0x8d,0x80,0xea})
  assert(r(0x6000)==0x20,'Co-op continuation WRAM is not writable')
  phase=2; set('pc',0x6000)
 elseif phase==2 then
  -- Stage-specific world effects advance only on P1's pass.
  set('pc',0x8626)
 end
end)
local function composite()
 local used={}
 for i=1,r(0x78),4 do
  local tile=r(0x200+i)
  if tile%2==1 and tile<64 then used[math.floor(tile/2)]=true end
 end
 local bank=chr[r(0xec)]
 if not bank or not chr[p2bank] then return end
 local merged={} for i=1,1024 do merged[i]=bank[i] end
 local mapping={}
 -- Reuse a distinct native sprite palette, preserving world palettes and fades.
 -- Mexico's palette 1 gives P2 a green outfit and tan skin, versus P1's brown.
 local playerPalette=1
 if r(0xc6)==r(0xc2) then playerPalette=r(0xca)~=r(0xc2) and 2 or 3 end
 local start=r(0x78)
 for i=1,#p2oam,4 do
  if start+4>252 then break end
  local tile=p2oam[i+1]
  if tile%2==1 and tile<64 then
   local source=math.floor(tile/2)
   if not mapping[source] then
    local slot=31 while slot>=0 and used[slot] do slot=slot-1 end
    if slot<0 then break end
    used[slot]=true; mapping[source]=slot
    for j=1,32 do merged[slot*32+j]=chr[p2bank][source*32+j] end
   end
   tile=mapping[source]*2+1
  end
  w(0x200+start,p2oam[i]); w(0x201+start,tile)
  local attr=p2oam[i+2]
  if p2oam[i+1]%2==1 and p2oam[i+1]<64 then
   attr=bit.bor(bit.band(attr,0xfc),playerPalette)
  end
  w(0x202+start,attr); w(0x203+start,p2oam[i+3]); start=start+4
 end
 -- The PPU is still displaying the previous frame while the CPU builds this
 -- one. Never rewrite its live CHR bank: mixed animation tiles break bodies.
 compositeBank=compositeBank==128 and 129 or 128
 mergedChr=merged; chrBuffers[compositeBank]=merged
 for i=1,1024 do rom.writebyte(16+0x20000+compositeBank*1024+i-1,merged[i]) end
 w(0xec,compositeBank); w(0x78,start)
end
memory.registerexec(0x6003,function()
 if M.enabled and p2 and phase==2 then
  p2=capture(); p2bank=r(0xec); p2oam=bytes(0x200,r(0x78))
  install(p1); put(0x200,oam)
  for a,v in pairs(shared) do w(a,v) end
  restore(regs); phase=3; M.passes=M.passes+1; set('pc',0x861c)
 end
end)
memory.registerexec(0xc4ba,function()
 if M.enabled and p2 and phase==3 then
  composite(); phase=0
 end
end)
M.export=function()
 return copy({version=1,p2=p2,p1=p1,phase=phase,regs=regs,oam=oam,shared=shared,
  p2oam=p2oam,p2bank=p2bank,damage=damage,attack=attack,chr=mergedChr,
  chrBuffers=chrBuffers,compositeBank=compositeBank,
  cameraX=cameraX,cameraY=cameraY,cameraAnchor=cameraAnchor,active=M.active,ticks=M.ticks,passes=M.passes,
  objectP1=objectP1,objectBoxes=objectBoxes,objectRegs=objectRegs,objectEntry=objectEntry,objectPrepared=objectPrepared,
  paused=M.paused,invincible=M.invincible})
end
M.import=function(t)
 if not t or t.version~=1 then reset(); return end
 t=copy(t)
 M.paused=t.paused or false; M.invincible=t.invincible or false
 p2=t.p2; p1=t.p1; phase=t.phase; regs=t.regs; oam=t.oam; shared=t.shared
 p2oam=t.p2oam; p2bank=t.p2bank; damage=t.damage; attack=t.attack
 objectP1=t.objectP1; objectBoxes=t.objectBoxes; objectRegs=t.objectRegs; objectEntry=t.objectEntry; objectPrepared=t.objectPrepared
 cameraX=t.cameraX; cameraY=t.cameraY; cameraAnchor=t.cameraAnchor; M.active=t.active; M.ticks=t.ticks; M.passes=t.passes
 mergedChr=t.chr
 chrBuffers=t.chrBuffers or {[128]=mergedChr}; compositeBank=t.compositeBank or 128
 for bank,data in pairs(chrBuffers) do
  for i=1,1024 do rom.writebyte(16+0x20000+bank*1024+i-1,data[i]) end
 end
end
savestate.registersave(function() return M.export() end)
savestate.registerload(function(slot,t) M.import(t) end)
M.rejoin=function(n)
 if not p2 or phase~=0 then return end
 local partner=n==2 and capture() or p2
 if partner[0x87]~=0 or partner[0x564]~=0 then return end
 local target=n==2 and p2 or capture()
 for _,a in ipairs({0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,0x8d,0x8e,0x8f,0x90,0x91,0x92,0x93}) do target[a]=partner[a] end
 for _,a in ipairs({0x59,0x5a,0x5b,0x56d,0x56e,0x56f,0x87,0x88,0x94,0x95,0x96,0x97}) do target[a]=0 end
 target[0x569]=30
 if n==1 then install(target) end
end
M.capture=capture
M.install=install
M.cheat=function(action)
 if not M.active or not p2 or phase~=0 then return false end
 if action=='invincible' then M.invincible=not M.invincible
 elseif action=='lives' then w(0x79,9)
 elseif action=='hats' then w(0x83,0); w(0x495,1); p2[0x83]=0; p2[0x495]=1
 elseif action=='rejoin' then M.rejoin(2)
 else return false end
 return true
end
return M
