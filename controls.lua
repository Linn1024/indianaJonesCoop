-- Keyboard bindings. Edit the quoted key names, then restart the launcher.
-- Key names: a-z, 0-9, space, enter, up, down, left, right, shift, control.
-- F1 is help, F5/F8 save/load. Avoid assigning those or emulator shortcuts.
-- Gamepads: FCEUX > Config > Input > Configure for ports 1 and 2.
return {
 p1={up='w',down='s',left='a',right='d',A='space',B='f',start='enter'},
 p2={up='up',down='down',left='left',right='right',A='l',B='k'},
 rejoin1='q', rejoin2='o',
 -- Hold all three keyboard keys after pausing to reveal the cheat menu.
 cheats={'a','b','c'},
}
