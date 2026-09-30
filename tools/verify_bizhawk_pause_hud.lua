client.unpause()
savestate.load('C:/TEMP2/Bizhawk/Libretro/State/fceumm_libretro/Young Indiana Jones Chronicles, The (USA).QuickSave1.State',true)
for n=1,2 do
 joypad.set({['P2 RetroPad Start']=false})
 emu.frameadvance()
end
for n=1,10 do
 joypad.set({['P2 RetroPad Start']=true})
 emu.frameadvance()
end
for n=1,60 do
 joypad.set({['P2 RetroPad Start']=false,['P2 RetroPad Right']=true})
 emu.frameadvance()
end
client.screenshot('C:/TEMP2/indianaJonesCoop/diagnostics/bizhawk-qs1-pause-hud-fixed.png')
client.exit()
