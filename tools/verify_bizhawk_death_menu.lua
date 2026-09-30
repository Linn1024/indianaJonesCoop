client.speedmode(800)
client.unpause()
local function step(a)
 joypad.set({['P1 RetroPad Start']=a=='start',['P1 RetroPad A']=a=='ab' or a=='a',
 ['P1 RetroPad B']=a=='ab',['P1 RetroPad Down']=a=='down'})
 emu.frameadvance()
end
for n=0,2399 do
 joypad.set({['P1 RetroPad Start']=n%24==1,['P1 RetroPad A']=n%12==1})
 emu.frameadvance()
end
step();step('start');step();step('ab');step()
for n=1,5 do step('down');step() end
step('a');step()
client.screenshot('C:/TEMP2/indianaJonesCoop/diagnostics/bizhawk-death-menu.png')
client.exit()
