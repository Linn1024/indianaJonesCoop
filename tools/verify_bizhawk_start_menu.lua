client.unpause()
for i=1,60 do emu.frameadvance() end
client.screenshot('C:/TEMP2/indianaJonesCoop/diagnostics/bizhawk-start-easy.png')
joypad.set({['P2 RetroPad Down']=true})
emu.frameadvance()
joypad.set({['P2 RetroPad Down']=false})
emu.frameadvance()
client.screenshot('C:/TEMP2/indianaJonesCoop/diagnostics/bizhawk-start-classic.png')
client.exit()
