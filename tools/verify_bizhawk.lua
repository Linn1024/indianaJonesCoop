client.speedmode(400)
client.unpause()
for n=0,2399 do
  joypad.set({['P1 RetroPad Start']=n%24==1,['P1 RetroPad A']=n%12==1,
    ['P2 RetroPad Right']=n>1900 and n<1950})
  emu.frameadvance()
end
client.screenshot('C:/TEMP2/indianaJonesCoop/diagnostics/bizhawk-coop.png')
local f=io.open('C:/TEMP2/indianaJonesCoop/diagnostics/bizhawk-verified.txt','w')
f:write(emu.getsystemid()..' frames='..emu.framecount());f:close()
client.exit()
