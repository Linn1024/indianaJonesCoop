return function(coop,controls)
 local M={open=false,selected=1}
 local last={}
 local release=false
 local actions={'invincible','lives','hats','rejoin'}
 local function down(keys,key) return keys[key] or keys[string.upper(key)] end
 function M.update(keys,a,b)
  local input={up=a.up or b.up,down=a.down or b.down,
   confirm=a.A or b.A,left=a.left or b.left,right=a.right or b.right,
   close=down(keys,'escape')}
  if not coop.paused then
   if M.open then release=true end
   M.open=false
  else
   local chord=true
   for _,key in ipairs(controls.cheats) do chord=chord and down(keys,key) end
   local opened=chord and not last.chord
   if opened then M.open=true; M.selected=1 end
   input.chord=chord
   if M.open then
    if not opened and input.up and not last.up then M.selected=(M.selected-2)%#actions+1 end
    if not opened and input.down and not last.down then M.selected=M.selected%#actions+1 end
    if not opened and ((input.confirm and not last.confirm) or
       (M.selected==1 and ((input.left and not last.left) or (input.right and not last.right)))) then
     coop.cheat(actions[M.selected])
    end
    if input.close and not last.close then M.open=false; release=true end
    a={start=a.start}; b={start=b.start}
   end
  end
  if release then
   if not input.confirm and not a.B and not b.B then release=false end
   a.A=false; a.B=false; b.A=false; b.B=false
  end
  last=input
  return a,b
 end
 function M.draw()
  if not coop.paused then return end
  if not M.open then
   gui.text(102,91,'PAUSED','white','black')
   gui.text(30,103,table.concat(controls.cheats,' + '):upper()..': cheats','white','black')
   return
  end
  gui.box(19,62,237,168,'#101820f0','#63e7ff')
  gui.text(30,71,'CHEATS - BOTH PLAYERS','#63e7ff','clear')
  local labels={'Invincible: '..(coop.invincible and 'ON' or 'OFF'),'Set shared lives to 9','Give both players hats','Rejoin P2 to P1'}
  for i,label in ipairs(labels) do gui.text(30,78+i*13,(i==M.selected and '> ' or '  ')..label,i==M.selected and '#ffe4a0' or 'white','clear') end
  gui.text(27,146,'Up/Down: choose  Jump: apply','white','clear')
  gui.text(27,157,'Pause: resume  Esc: close','white','clear')
 end
 return M
end
