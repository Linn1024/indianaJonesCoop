local root=assert(os.getenv('INDIANA_COOP_ROOT'))..'/'
local log=assert(io.open(root..'diagnostics/verification.txt','w'))
log:setvbuf('no')
local function say(s) log:write(s..'\n') end
local function check(ok,s) if not ok then say('FAIL '..s); log:close(); os.exit() end say('PASS '..s) end
local base=savestate.create(root..'tests/mexico-start.fc0')
savestate.load(base)
local coop=dofile(root..'coop.lua')
emu.speedmode('maximum')
local function frame(a,b) joypad.set(1,a or {}); joypad.set(2,b or {}); emu.frameadvance() end
local function fresh()
 savestate.load(base); coop.reset(); coop.enabled=true
 for i=1,3 do frame() end
end
local function signature()
 local s=coop.state(); local t={memory.readbyterange(0,0x800)}
 for a=0,0x600 do if s.p2 and s.p2[a] then t[#t+1]=string.char(s.p2[a]) end end
 return table.concat(t)
end
do
 fresh()
 local start=coop.state()
 for i=1,24 do frame({}, {right=true}) end
 local s=coop.state()
 say(string.format('Movement sample: passes=%d phase=%d p1=%d p2=%d',coop.passes,s.phase,s.p1[0x7a],s.p2[0x7a]))
 check(s.p1[0x7a]==start.p1[0x7a], 'P2 movement leaves idle P1 position unchanged')
 check(s.p2[0x7a]>start.p2[0x7a]+20,'P2 executes native movement')
 local y=s.p2[0x7c]
 for i=1,14 do frame({}, {A=true}) end
 check(coop.state().p2[0x7c]<y-15,'P2 native jump')
 gui.savescreenshotas(root..'diagnostics/two-players.png')
 fresh()
 local p2x=coop.state().p2[0x7a]
 for i=1,20 do frame({right=true},{}) end
 check(coop.state().p2[0x7a]==p2x,'P1 movement leaves idle P2 unchanged')
 local saved=savestate.create(); savestate.save(saved); savestate.persist(saved)
 local extra=coop.export()
 for i=1,90 do frame({right=true,A=i<12},{B=i%25==1,right=true}) end
 local expected=signature()
 savestate.load(saved); coop.import(extra)
 for i=1,90 do frame({right=true,A=i<12},{B=i%25==1,right=true}) end
 check(signature()==expected,'Co-op save restore replays deterministically')
 fresh()
 -- Traverse normally together until the first native guard spawns.
 local found=false
 for i=1,240 do
  frame({right=true},{right=true})
  if memory.readbyte(0x514)==6 then found=true; break end
 end
 check(found,'Native first guard spawns in the shared world')
 local encounter=savestate.create(); savestate.save(encounter); savestate.persist(encounter)
 local encounterExtra=coop.export()
 -- Compare untouched enemy state against a single-player execution, with both
 -- players safely out of contact range. This catches duplicated world updates.
 local timeline={}
 for variant=1,2 do
  savestate.load(encounter); coop.import(encounterExtra); coop.enabled=variant==2
  for i=1,60 do
   memory.writebyte(0x8a,bit.bor(memory.readbyte(0x8a),1)); memory.writebyte(0x9e,0)
   coop.setplayer(1,0x7a,24); coop.setplayer(1,0x7c,176)
   coop.setplayer(2,0x7a,24); coop.setplayer(2,0x7c,176)
   frame()
   local data=memory.readbyterange(0x514,0x50)
   if variant==1 then timeline[i]=data else
    if data~=timeline[i] then
     for j=1,#data do if data:byte(j)~=timeline[i]:byte(j) then say(string.format('DIFF frame=%d addr=%04x native=%d coop=%d',i,0x513+j,timeline[i]:byte(j),data:byte(j))) end end
    end
    if data~=timeline[i] then check(false,'World enemy update matches native frame '..i) end
   end
  end
 end
 say('PASS 60 enemy updates match native execution with a stationary camera')
 coop.enabled=true
 for player=1,2 do
  savestate.load(encounter); coop.import(encounterExtra)
  for i=1,70 do
   for n=1,2 do
    coop.setplayer(n,0x7a,n==player and 128 or 32)
    coop.setplayer(n,0x7c,112); coop.setplayer(n,0x7b,0); coop.setplayer(n,0x7d,0)
    coop.setplayer(n,0x59,0); coop.setplayer(n,0x5a,0); coop.setplayer(n,0x5b,0)
    coop.setplayer(n,0x85,0)
   end
   if memory.readbyte(0x514)==6 then memory.writebyte(0x518,112); memory.writebyte(0x519,154) end
   local input={B=i==2 or i==35}
   frame(player==1 and input or {},player==2 and input or {})
   if memory.readbyte(0x514)~=6 then break end
  end
  check(memory.readbyte(0x514)~=6,'P'..player..' whip defeats a native guard')
 end
 -- Check enemy contact can hurt P2 without hurting distant P1.
 savestate.load(encounter); coop.import(encounterExtra)
 local hurt=false
 for i=1,10 do
  coop.setplayer(1,0x7a,24); coop.setplayer(1,0x7c,176)
  coop.setplayer(2,0x7a,128); coop.setplayer(2,0x7c,112)
  memory.writebyte(0x518,112); memory.writebyte(0x519,128)
  frame()
  local s=coop.state()
  if s.p2[0x86]~=0 or s.p2[0x568]~=0 then hurt=true end
 end
 check(hurt,'Enemy contact reaches P2')
 check(coop.state().p1[0x568]==0,'Distant P1 does not inherit P2 damage')
 for player=1,2 do
  fresh()
  local lives=memory.readbyte(0x79)
  coop.setplayer(player,0x564,1); coop.setplayer(player,0x56b,2)
  for i=1,200 do frame() end
  local s=coop.state()
  check(memory.readbyte(0x79)==lives-1,'P'..player..' death costs one shared life')
  check(s.p2~=nil and s.phase==0 and s.p1[0x564]==0 and s.p2[0x564]==0,'P'..player..' death restores both players through native reload')
 end
 fresh()
 for i=1,35 do frame({}, {right=true}) end
 coop.rejoin(2)
 local s=coop.state()
 check(s.p2[0x7a]==s.p1[0x7a] and s.p2[0x7c]==s.p1[0x7c],'Rejoin places P2 at grounded P1')
 for player=1,2 do
  fresh()
  memory.writebyte(0x9a,0); memory.writebyte(0x9d,1)
  for n=1,2 do
   coop.setplayer(n,0x7a,n==player and 144 or 112)
   coop.setplayer(n,0x7b,0); coop.setplayer(n,0x7c,176); coop.setplayer(n,0x7d,0)
  end
  for i=1,100 do
   local input={up=i>=16 and i<=70}
   frame(player==1 and input or {},player==2 and input or {})
  end
  check(memory.readbyte(0xa0)==32 and coop.state().p2~=nil,'P'..player..' enters the native underground room with the team')
 end
 say('ALL CHECKS PASSED')
end
log:close(); os.exit()
