/* Native flight player passes; scrolling/enemy updates remain single-pass. */
static void plane_capture(Plane *p){memcpy(p->zp,RAM+0x36,10);memcpy(p->stats,RAM+0x160,4);memcpy(p->local,RAM+0x600,16);}
static void plane_install(const Plane *p){memcpy(RAM+0x36,p->zp,10);memcpy(RAM+0x160,p->stats,4);memcpy(RAM+0x600,p->local,16);}
static uint16_t flight_finish(void){
 unsigned i;
 if(c.phase!=2){
  capture(&c.p1);plane_capture(&flight.p1);flight.regs=regs();
  flight.input[0]=RAM[0xf5];flight.input[1]=RAM[0xf7];flight.draw_start=RAM[0x21];
  install(&c.p2);plane_install(&flight.p2);
  if(!deaths.individual)RAM[0x79]=c.p1.v[0x79];
  RAM[0xf5]=RAM[0xf6];RAM[0xf7]=RAM[0xf8];c.phase=2;
  if(!(deaths.individual&&deaths.out[1]))return 0x8043;
 }
 for(i=flight.draw_start;i!=RAM[0x21];i=(i+(int8_t)RAM[0x22])&255)RAM[0x202+i]=(RAM[0x202+i]&0xe0)|4;
 capture(&c.p2);plane_capture(&flight.p2);install(&c.p1);plane_install(&flight.p1);
 RAM[0xf5]=flight.input[0];RAM[0xf7]=flight.input[1];restore(flight.regs);
 c.phase=0;c.passes++;return 0x805f;
}
static uint16_t flight_instruction(uint16_t pc){
 unsigned i;
 switch(pc){
 case 0x8007:
  if(!flight.active){
   capture(&c.p2);plane_capture(&flight.p2);
   flight.p2.zp[0]=clamp(RAM[0x36]+(RAM[0x36]>128?-40:40),54,208);
   flight.p2.stats[3]=flight.p2.local[9]=0;flight.p2.local[12]=2;
   flight.p2.zp[2]=32;flight.p2.zp[4]=flight.p2.zp[6]=flight.p2.zp[8]=240;
   if(deaths.pending){RAM[0x79]=deaths.lives[0];c.p2.v[0x79]=deaths.lives[1];deaths.pending=0;}
   if(inventory_transfer.pending){
    for(i=0;i<10;i++){unsigned a=inventory_addresses[i];RAM[a]=inventory_transfer.items[0][i];c.p2.v[a]=inventory_transfer.items[1][i];}
    inventory_transfer.pending=0;
   }
   flight.active=c.has_p2=1;transition_hidden=0;
  }
  c.active=1;c.paused=0;c.ticks++;c.phase=0;break;
 case 0x8043:
  if(c.phase!=2)c.phase=1;
  if(deaths.individual&&deaths.out[c.phase==2?1:0])return flight_finish();break;
 case 0x805f:if(flight.active)return flight_finish();break;
 case 0x8fa4:
  /* Flight supply pickups bypass the enemy hit loop. Run their native
   * award/removal once, in the context of the plane touching the item. */
  if(flight.active&&!flight.hit){
   int first=!deaths.out[0]&&!RAM[0x163]&&
    (uint8_t)(RAM[0x2b]-RAM[0x37]+16)<40&&(uint8_t)(RAM[0x29]-RAM[0x36]+30)<40;
   int second=!deaths.out[1]&&!flight.p2.stats[3]&&
    (uint8_t)(RAM[0x2b]-flight.p2.zp[1]+16)<40&&(uint8_t)(RAM[0x29]-flight.p2.zp[0]+30)<40;
   if(!first&&!second)return 0x8ff5;
   if(!first){capture(&c.p1);plane_capture(&flight.p1);install(&c.p2);plane_install(&flight.p2);flight.hit=3;}
  }break;
 case 0x871e:case 0x9684:
  if(flight.active&&!flight.hit){
   flight.hit=1;flight.hit_entry=pc;flight.entry_regs=regs();memcpy(flight.scratch,RAM,54);
  }break;
 case 0x8794:case 0x96c7:
  if(flight.hit==3){
   capture(&c.p2);plane_capture(&flight.p2);install(&c.p1);plane_install(&flight.p1);flight.hit=0;
   break;
  }
  if(flight.hit==1){
   capture(&c.p1);plane_capture(&flight.p1);flight.result_regs=regs();memcpy(flight.result,RAM,54);
   install(&c.p2);plane_install(&flight.p2);memcpy(RAM,flight.scratch,54);restore(flight.entry_regs);
   flight.hit=2;return flight.hit_entry;
  }
  if(flight.hit==2){
   uint8_t lo=RAM[0x25],hi=RAM[0x26];
   capture(&c.p2);plane_capture(&flight.p2);install(&c.p1);plane_install(&flight.p1);
   memcpy(RAM,flight.result,54);RAM[0x25]=lo;RAM[0x26]=hi;restore(flight.result_regs);flight.hit=0;
  }break;
 case 0x88f3:
  if(c.invincible||deaths.out[(flight.hit==2||c.phase==2)?1:0])return popreturn();break;
 case 0x8426:
  if(flight.active&&(flight.hit>=2||c.phase==2))return popreturn();break;
 case 0x80b2:case 0x80ca:
  if(flight.active){
   unsigned who=c.phase==2?1:0;
   if(RAM[0x79])RAM[0x79]--;
   if(deaths.individual&&!RAM[0x79])deaths.out[who]=1;
   if((deaths.individual&&deaths.out[0]&&deaths.out[1])||(!deaths.individual&&!RAM[0x79])){
    reset_players();flight.active=0;return 0xf162;
   }
   RAM[0x163]=RAM[0x609]=0;RAM[0x60c]=2;RAM[0x38]=60;
   RAM[0x36]=who?clamp(flight.p1.zp[0]+32,54,208):120;RAM[0x37]=48;
   if(!deaths.individual){
    Plane *partner=who?&flight.p1:&flight.p2;
    c.p2.v[0x79]=c.p1.v[0x79]=RAM[0x79];
    partner->stats[3]=partner->local[9]=0;partner->local[12]=2;
    partner->zp[0]=who?120:160;partner->zp[1]=48;partner->zp[2]=60;
    partner->zp[4]=partner->zp[6]=partner->zp[8]=240;
   }
   return flight_finish();
  }break;
 }
 return pc;
}
