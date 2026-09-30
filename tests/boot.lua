local root=assert(os.getenv('INDIANA_COOP_ROOT'))..'/'
local coop=dofile(root..'coop.lua')
emu.speedmode('maximum')
for i=1,2600 do
 joypad.set(1,{start=i%24==1,A=i%12==1}); joypad.set(2,{})
 emu.frameadvance()
 if coop.passes>30 then break end
end
local f=assert(io.open(root..'diagnostics/boot.txt','w'))
f:write('passes='..coop.passes..' stage='..memory.readbyte(0xa0)..'\n')
f:write(coop.passes>30 and 'PASS cold boot into co-op without a savestate\n' or 'FAIL cold boot\n')
f:close(); gui.savescreenshotas(root..'diagnostics/cold-boot.png'); emu.frameadvance(); os.exit()
