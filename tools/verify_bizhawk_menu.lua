client.speedmode(800)
client.unpause()
local function step(a,b)
 joypad.set({['P1 RetroPad Start']=a=='start',['P1 RetroPad A']=a=='ab',['P1 RetroPad B']=a=='ab',
 ['P2 RetroPad Down']=b=='down',['P2 RetroPad A']=b=='a'})
 emu.frameadvance()
end
for n=0,2399 do
 joypad.set({['P1 RetroPad Start']=n%24==1,['P1 RetroPad A']=n%12==1})
 emu.frameadvance()
end
step();step('start');step();step('ab');step()
client.screenshot('C:/TEMP2/indianaJonesCoop/diagnostics/bizhawk-cheats.png')
for n=1,4 do step(nil,'down');step() end
step(nil,'a');step()
client.screenshot('C:/TEMP2/indianaJonesCoop/diagnostics/bizhawk-noclip.png')
client.exit()
