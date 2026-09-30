client.unpause()
savestate.load('C:/TEMP2/Bizhawk/Libretro/State/fceumm_libretro/Young Indiana Jones Chronicles, The (USA).QuickSave1.State',true)
for n=1,20 do
 joypad.set({['P1 RetroPad Left']=true,['P2 RetroPad Right']=true})
 emu.frameadvance()
end
client.screenshot('C:/TEMP2/indianaJonesCoop/diagnostics/bizhawk-dungeon-repaired.png')
client.exit()
