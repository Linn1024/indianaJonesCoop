/* Native Indiana Jones player-context replay. GPL-2.0-or-later.
 * Hooks run BEFORE opcode fetch. Continuations are core-owned PC traps, with
 * no injected instructions and no executable data in game or cartridge RAM.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "fceu.h"
#include "cart.h"
#include "ines.h"
#include "state.h"
#include "x6502.h"
#include "indiana_coop.h"
#include "crc32.h"
#include "ppu.h"

typedef struct { uint8_t v[0x600]; } Player;
typedef struct { uint8_t a,x,y,s,p,pi; } Registers;
typedef struct {
 uint32_t version,ticks,passes,graphics_errors;
 uint8_t enabled,active,paused,invincible,has_p2,phase,bank;
 uint8_t camera,x,y,anchor;
 Player p1,p2,damage_player,attack_player,object_player;
 Registers regs,damage_entry,damage_result,attack_result,object_regs;
 uint8_t oam[256],p2oam[256],p2length,p2bank,shared[8];
 uint8_t damage,attack,attack_boxes[24],attack_enemy[4],attack_hit;
 uint8_t object_active,object_prepared,object_boxes[24];
 uint16_t object_entry;
} Coop;
static Coop c;
typedef struct {uint8_t zp[10],stats[4],local[16];} Plane;
static struct {
 uint8_t active,hit,draw_start,input[2],scratch[54],result[54];
 uint16_t hit_entry;
 Plane p1,p2;
 Registers regs,entry_regs,result_regs;
} flight;
typedef struct {uint8_t individual,out[2],lives[2],pending,safe_valid[2];Player safe[2];} DeathRules;
static DeathRules deaths;
static struct {uint16_t scroll_x[2];uint8_t scroll_y[2],valid[2],grounded[2];} safe_ground;
static uint8_t graphics[256*1024];
static uint16_t addresses[160];
static unsigned address_count;
int ij_loaded;
int ij_extra_cycles;
static uint8_t noclip;
static uint8_t transition_hidden;
int ij_transition_hidden(void){return transition_hidden;}
static unsigned menu_open,menu_selected,menu_previous;
static unsigned start_menu,start_selection;
static uint8_t area_stage=255;
static unsigned menu_level,warp_pending;
static const uint16_t inventory_addresses[]={0x82,0x83,0x84,0x495,0x499,0x49a,0x49b,0x49e,0x4bf,0x4c2};
typedef struct {uint8_t pending,items[2][10];} InventoryTransfer;
static InventoryTransfer inventory_transfer;
typedef struct {uint8_t valid,stage,x,y,pending;} DoorReturn;
static DoorReturn door_return;
static struct {uint16_t x;uint8_t active,stage,y;} cave_exit;
typedef struct {uint8_t active,owner,partner;} Stone;
static Stone stone;
static struct {uint8_t active,y[4],x[4];} platform_view;
static struct {uint8_t active;Player original;} lift_context;
static uint8_t grabbed_player;
static struct {uint8_t active,start;} grab_drawing;
static struct {uint8_t owner[5],pending[5],draw_active,draw_start;} motorcycles;
static int clamp(int n,int lo,int hi);
static void motorcycle_pairs(void){
 /* France's C0 pickup becomes a pair. Keep native object slots/physics and
  * retry when slots are full rather than replacing an enemy. */
 unsigned i,j;
 if(!c.has_p2||RAM[0x45]!=1)return;
 for(i=0;i<5;i++)if(RAM[0x514+16*i]!=0xc0)motorcycles.owner[i]=motorcycles.pending[i]=0;
 for(i=0;i<5;i++)if(RAM[0x514+16*i]==0xc0&&!motorcycles.owner[i]){
  motorcycles.owner[i]=1;motorcycles.pending[i]=!c.p2.v[0x96];
 }
 for(i=0;i<5;i++)if(motorcycles.pending[i]){
  for(j=5;j>0;j--)if(!RAM[0x514+16*(j-1)])break;
  if(!j)break;
  j--;memcpy(RAM+0x514+16*j,RAM+0x514+16*i,16);
  RAM[0x515+16*j]=1;RAM[0x517+16*j]=0;RAM[0x51d+16*j]=1;
  RAM[0x519+16*j]=clamp(RAM[0x519+16*i]+(RAM[0x519+16*i]<=192?32:-40),16,224);
  motorcycles.owner[j]=2;motorcycles.pending[i]=0;
 }
 /* Copies made before native pickup initialization can retain facing 2,
  * whose parked-bike frame is empty. Repair these in existing saves too. */
 for(j=0;j<5;j++)if(motorcycles.owner[j]==2&&RAM[0x514+16*j]==0xc0){
  RAM[0x515+16*j]=1;
  for(i=0;i<5;i++)if(motorcycles.owner[i]==1&&RAM[0x514+16*i]==0xc0&&
     abs(RAM[0x519+16*j]-RAM[0x519+16*i])<32)
   RAM[0x519+16*j]=clamp(RAM[0x519+16*i]+(RAM[0x519+16*i]<=192?32:-40),16,224);
 }
}
static void motorcycle_graphics(unsigned bank){
 /* A parked bike uses object CHR, unlike the mounted player's body bank.
  * Copy its tiles into unused merged slots so enemies can retain their CHR. */
 unsigned i,j,slot;uint8_t used[32]={0};
 if(bank>=128||c.bank<128)return;
 for(i=0;i<RAM[0x78];i+=4){unsigned t=RAM[0x201+i];if((t&1)&&t<64)used[t/2]=1;}
 for(i=motorcycles.draw_start;i<RAM[0x78];i+=4){
  unsigned tile=RAM[0x201+i];
  if(!(tile&1))continue;
  for(slot=0;slot<32&&used[slot];slot++);
  if(slot==32)break;
  used[slot]=1;
  for(j=0;j<32;j++)graphics[c.bank*1024+slot*32+j]=graphics[bank*1024+(tile&63)/2*32+j];
  RAM[0x201+i]=slot*2+1;RAM[0x202+i]=(RAM[0x202+i]&0xe0)|4;
 }
}
static void preserve_inventory(void){
 unsigned p,i;
 if(!c.has_p2||inventory_transfer.pending)return;
 for(p=0;p<2;p++)for(i=0;i<10;i++){
  unsigned a=inventory_addresses[i];
  inventory_transfer.items[p][i]=c.phase==2?(p?RAM[a]:c.p1.v[a]):
    (p?c.p2.v[a]:(c.object_active?c.object_player.v[a]:RAM[a]));
 }
 inventory_transfer.pending=1;
}
static const uint16_t shared_addresses[]={0x78,0x9e,0x566,0x56c,0xf5,0xf7,0xec,0x9b};
static void transition_whip(void){
 /* These native story exits explicitly equip the whip. Apply the same
  * weapon change to both inventories, including an already queued transfer. */
 preserve_inventory();
 if(RAM[0xa0]==19||RAM[0xa0]==20)return;
 if(inventory_transfer.pending)inventory_transfer.items[0][0]=inventory_transfer.items[1][0]=1;
 RAM[0x82]=c.p1.v[0x82]=c.p2.v[0x82]=1;
}
static void range(unsigned a,unsigned b){while(a<=b)addresses[address_count++]=(uint16_t)a++;}
static void capture(Player *p){unsigned i;for(i=0;i<address_count;i++)if(addresses[i]!=0x79||deaths.individual)p->v[addresses[i]]=RAM[addresses[i]];}
static void install(const Player *p){unsigned i;for(i=0;i<address_count;i++)if(addresses[i]!=0x79||deaths.individual)RAM[addresses[i]]=p->v[addresses[i]];}
static Registers regs(void){Registers r={X.A,X.X,X.Y,X.S,X.P,X.mooPI};return r;}
static void restore(Registers r){X.A=r.a;X.X=r.x;X.Y=r.y;X.S=r.s;X.P=r.p;X.mooPI=r.pi;}
static Player *other(void){return c.object_active?&c.object_player:&c.p2;}
static int scripted_scene(void){return (RAM[0xa0]==2&&RAM[0x132]&&RAM[0x13f])||(RAM[0xa0]==7&&RAM[0x42]==1)||(RAM[0xa0]==18&&RAM[0x514]==0x6a&&RAM[0x517]==255);}
static int clamp(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
static int stone_contact(const Player *p,int width){
 int x=p?p->v[0x7a]:RAM[0x7a],y=p?p->v[0x7c]:RAM[0x7c];
 return !(p?(p->v[0x87]|p->v[0x568]|p->v[0x56b]):(RAM[0x87]|RAM[0x568]|RAM[0x56b]))&&
  x>RAM[0x519]-16&&x<=RAM[0x519]+width&&y+31<RAM[0x518]+4&&y+39>=RAM[0x518]+4;
}
static void pushreturn(unsigned pc){unsigned v=pc-1;RAM[0x100+X.S--]=v>>8;RAM[0x100+X.S--]=v;}
static uint16_t popreturn(void){unsigned lo=RAM[0x100+(uint8_t)++X.S];unsigned hi=RAM[0x100+(uint8_t)++X.S];return (uint16_t)((hi<<8)+lo+1);}
IJ_API unsigned ij_death_mode(void){return deaths.individual;}
IJ_API unsigned ij_player_out(unsigned player){return player>=1&&player<=2?deaths.out[player-1]:0;}
static uint16_t ghost(void){
 RAM[0x7a]=8;RAM[0x7c]=32;RAM[0x7b]=RAM[0x7d]=0;
 RAM[0x564]=RAM[0x568]=RAM[0x56b]=RAM[0x86]=RAM[0x95]=RAM[0x96]=0;
 RAM[0x569]=30;RAM[0xf5]&=0x10;RAM[0xf7]&=0x10; /* Keep Start's held edge through an OUT player's pass. */
 return 0x8614;
}
static void prepare_area(void);
static int grounded_player(const Player *p){
 return !p->v[0x564]&&!p->v[0x56b]&&!p->v[0x87]&&p->v[0x7c]>=32&&p->v[0x7c]<=208;
}
static void release_finished_lift(void){
 unsigned surface=RAM[0x8d],slot;
 if(!RAM[0x5da])return;
 /* $5DA holds a rider while a falling lift moves. Its landing callback may
  * run in the other player's context; the shared timer is authoritative. */
 if(surface>=16&&surface<20){
  slot=surface-16;
  if((RAM[0x580]&(1<<slot))&&!(RAM[0x5bf+slot]==5&&!RAM[0x5c3+slot]))return;
 }
 RAM[0x5da]=0;
}
static int safe_respawn(Player *p,unsigned who){
 int x,y;
 if(!deaths.safe_valid[who]||!safe_ground.valid[who])return 0;
 *p=deaths.safe[who];
 x=p->v[0x7a]+safe_ground.scroll_x[who]-(RAM[0x9a]+256*RAM[0x9d]);
 y=p->v[0x7c]+safe_ground.scroll_y[who]-RAM[0x9b];
 if(x<8||x>237||y<32||y>208)return 0;
 p->v[0x7a]=x;p->v[0x7c]=y;return 1;
}
static uint16_t individual_death(void){
 unsigned who=c.phase==2?1:0,i;
 unsigned ret=RAM[0x100+(uint8_t)(X.S+1)]|(RAM[0x100+(uint8_t)(X.S+2)]<<8);
 Player own,partner=who?c.p1:c.p2;
 capture(&own);
 if(RAM[0x79])RAM[0x79]--;
 if(!RAM[0x79])deaths.out[who]=1;
 deaths.lives[who]=RAM[0x79];own.v[0x79]=RAM[0x79];
 if(deaths.out[0]&&deaths.out[1]){
  RAM[0x79]=0;c.has_p2=c.phase=c.active=c.camera=c.damage=c.attack=c.object_active=0;
  return 0x9c0b;
 }
 if(!who&&c.camera){
  int dx=(int8_t)RAM[0x9e],dy=(int8_t)RAM[0x566];
  c.p2.v[0x7a]=clamp(c.p2.v[0x7a]-dx,8,237);c.p2.v[0x7c]-=dy;
  partner=c.p2;c.camera=0;
 }
 if(deaths.out[1-who]||!grounded_player(&partner)){
  if(!safe_respawn(&partner,who)&&!safe_respawn(&partner,1-who)){
   /* An old airborne save or a scrolled-off ledge has no usable anchor.
    * Reload the area entrance once, retaining lives and inventory. */
   Player *teammate=who?&c.p1:&c.p2;
   if(!deaths.out[1-who]&&(teammate->v[0x7c]>=224||teammate->v[0x564]||teammate->v[0x56b])){
    if(teammate->v[0x79])teammate->v[0x79]--;
    if(!teammate->v[0x79])deaths.out[1-who]=1;
   }
   if(deaths.out[0]&&deaths.out[1]){
    RAM[0x79]=0;c.has_p2=c.phase=c.active=c.camera=c.damage=c.attack=c.object_active=0;
    return 0x9c0b;
   }
   if(who){capture(&c.p2);install(&c.p1);}
   c.phase=0;prepare_area();
   RAM[0xa3]=RAM[0xa6]=RAM[0x9a]=0;RAM[0xa4]=32;RAM[0xa5]=112;
   return 0xc1e1;
  }
 }
 /* Keep the defeated player's inventory, reset motion/attacks, and place
  * them on their partner's occupied position rather than an untested offset. */
 for(i=0;i<10;i++)partner.v[inventory_addresses[i]]=own.v[inventory_addresses[i]];
 partner.v[0x4bd]=own.v[0x4bd];
 partner.v[0x79]=own.v[0x79];install(&partner);
 RAM[0x59]=RAM[0x5a]=RAM[0x5b]=RAM[0x86]=RAM[0x94]=RAM[0x95]=RAM[0x96]=0;
 RAM[0x564]=RAM[0x568]=RAM[0x56b]=RAM[0x5da]=RAM[0x5db]=0;
 RAM[0x56d]=RAM[0x56e]=RAM[0x56f]=RAM[0x570]=RAM[0x57f]=0;
 RAM[0x569]=30;
 if(deaths.out[who])ghost();
 return ret==0x8616?popreturn():0x8614;
}
static void cancel_stone_lock(void);
static void finish_coop_camera(void){
 int dx,dy;
 if(c.phase!=1||c.camera!=1)return;
 dx=(int8_t)RAM[0x9e];dy=(int8_t)RAM[0x566];
 RAM[0x7a]=clamp(c.x-dx,8,237);RAM[0x7c]=c.y-dy;RAM[0x7e]=c.anchor;
 c.p2.v[0x7a]=clamp(c.p2.v[0x7a]-dx,8,237);c.p2.v[0x7c]-=dy;c.camera=0;
}
static uint16_t coop_vertical_boundary(void){
 /* A viewport edge is not an area exit while there is map below it.
  * Native P1 reaches this check only after scrolling has been considered;
  * P2's replay must enforce the same limit explicitly. */
 if(RAM[0x7c]>=224&&RAM[0x565]&&!(RAM[0x8a]&2)&&RAM[0x9b]<175)
  return deaths.individual?individual_death():0x9be6;
 return 0x9b67;
}
static uint16_t fly(void){
 unsigned input=RAM[0xf7];
 cancel_stone_lock();
 RAM[0x7a]=clamp(RAM[0x7a]+((input&1)?3:0)-((input&2)?3:0),8,237);
 RAM[0x7c]=clamp(RAM[0x7c]+((input&4)?3:0)-((input&8)?3:0),32,208);
 RAM[0x7b]=RAM[0x7d]=RAM[0x59]=RAM[0x5a]=RAM[0x5b]=0;
 RAM[0x86]=RAM[0x87]=RAM[0x88]=RAM[0x94]=RAM[0x95]=RAM[0x96]=0;
 RAM[0x564]=RAM[0x568]=RAM[0x56b]=0;RAM[0x569]=30;
 return 0x8614; /* Camera and original sprites, without gravity/terrain tests. */
}
static void cancel_stone_lock(void){
 unsigned i;
 if(RAM[0x42]!=39)return;
 RAM[0x42]=RAM[0x43]=0;
 RAM[0x571]=c.p2.v[0x571]=255;
 RAM[0x88]&=~2;c.p2.v[0x88]&=~2;
 for(i=0;i<0x50;i+=16)if(RAM[0x514+i]==0x20||RAM[0x514+i]==0x24)RAM[0x517+i]=255;
 memset(&stone,0,sizeof(stone));
}
static void reset_players(void){
 c.has_p2=c.phase=c.active=c.paused=c.damage=c.attack=c.object_active=c.object_prepared=c.camera=0;
}
static void level_outfits(void){
 /* $83 combines hat graphics (0/2) with the chapter's hatless outfit (1/3).
  * Preserve collected hats, but do not carry a hatless body across chapters.
  * Native initialization/death selects the same outfit using $45. */
 unsigned outfit=RAM[0x45]?3:1;
 if(!RAM[0x495]&&(RAM[0x83]==1||RAM[0x83]==3))RAM[0x83]=outfit;
 if(!c.p2.v[0x495]&&(c.p2.v[0x83]==1||c.p2.v[0x83]==3))c.p2.v[0x83]=outfit;
}
static void prepare_area(void){
 unsigned i;
 if(cave_exit.stage!=RAM[0xa0])cave_exit.active=0;
 memset(&lift_context,0,sizeof(lift_context));
 memset(&flight,0,sizeof(flight));
 grabbed_player=0;
 memset(&grab_drawing,0,sizeof(grab_drawing));
 memset(&safe_ground,0,sizeof(safe_ground));
 memset(&motorcycles,0,sizeof(motorcycles));
 transition_hidden=1;
 platform_view.active=0;
 memset(&stone,0,sizeof(stone));
 if(deaths.individual&&c.has_p2){
  deaths.lives[0]=c.phase==2?c.p1.v[0x79]:RAM[0x79];
  deaths.lives[1]=c.phase==2?RAM[0x79]:c.p2.v[0x79];
  if(area_stage!=255&&area_stage!=RAM[0xa0]&&!(deaths.out[0]&&deaths.out[1]))
   for(i=0;i<2;i++)if(deaths.out[i]){deaths.out[i]=0;deaths.lives[i]=1;}
  deaths.pending=1;
 }
 deaths.safe_valid[0]=deaths.safe_valid[1]=0;
 preserve_inventory();reset_players();area_stage=RAM[0xa0];
}
void ij_close(void){ij_loaded=ij_extra_cycles=0;noclip=0;memset(&c,0,sizeof(c));}
void ij_prepare_load(void){memset(&cave_exit,0,sizeof(cave_exit));memset(&lift_context,0,sizeof(lift_context));memset(&flight,0,sizeof(flight));memset(&grab_drawing,0,sizeof(grab_drawing));grabbed_player=0;memset(&safe_ground,0,sizeof(safe_ground));memset(&motorcycles,0,sizeof(motorcycles));transition_hidden=0;memset(&platform_view,0,sizeof(platform_view));memset(&stone,0,sizeof(stone));area_stage=255;noclip=0;ij_extra_cycles=0;menu_open=menu_selected=menu_previous=0;start_menu=0;memset(&door_return,0,sizeof(door_return));memset(&inventory_transfer,0,sizeof(inventory_transfer));memset(&deaths,0,sizeof(deaths));deaths.individual=2;}
void ij_finish_load(void){
 if(c.has_p2&&c.phase==0&&!transition_hidden)level_outfits();
 /* Old noclip saves may have cancelled the animation but retained its lock. */
 if(c.has_p2&&RAM[0x42]==39&&!(RAM[0x88]&2)&&!(c.p2.v[0x88]&2))cancel_stone_lock();
 warp_pending=0;menu_level=RAM[0xa0];if(area_stage==255)area_stage=RAM[0xa0];
 if(deaths.individual==2){
  deaths.individual=1;c.p2.v[0x79]=RAM[0x79];
  capture(&deaths.safe[0]);deaths.safe[1]=c.p2;
  deaths.safe_valid[0]=deaths.safe_valid[1]=1;
 }
 /* Older player-context saves could erase a platform's active bit while
  * leaving the object-spawn cursor past it. Recover only nearby, already
  * passed platform records whose inactive coordinates are off-screen. */
 if(ij_loaded&&c.has_p2&&c.phase==0&&(RAM[0xa0]==3||RAM[0xa0]==5)){
  unsigned table=RAM[0x4a5]|(RAM[0x4a6]<<8),record;
  int scroll=RAM[0x9a]+256*RAM[0x9d];
  if(table>=0x8000&&table<0xc000)for(record=0;record<255&&table+record+5<=0xc000;record+=5){
   const uint8_t *entry=ROM+8*8192+table-0x8000+record;
   unsigned kind=entry[4],slot;int x;
   if(entry[0]&128)break;
   if(RAM[0xa0]==3){if(kind<0xa0||kind>0xa6||(kind&1))continue;slot=(kind-0xa0)/2;}
   else{if(kind<0xaa||kind>0xb0||(kind&1))continue;slot=(kind-0xaa)/2;}
   x=entry[0]*256+entry[1]-16-scroll;
   if(x<0||x>240||(RAM[0x580]&(1<<slot))||RAM[0x5bb+slot]<254)continue;
   RAM[0x5ab+slot]=x;RAM[0x5bb+slot]=0;
   RAM[0x581+slot]=RAM[0x5b7+slot]=0;
   RAM[0x5af+slot]=ROM[12*8192+(RAM[0xa0]==3?0x2d20:0x2d72)+slot];
   RAM[0x5bf+slot]=RAM[0xa0]==3?1:0;
   RAM[0x5c3+slot]=ROM[12*8192+(RAM[0xa0]==3?0x2b7d:0x2b6d)+slot];
   RAM[0x580]|=1<<slot;
  }
 }
}
IJ_API unsigned ij_noclip(void){return noclip;}
int ij_extra_work(uint16_t pc){
 /* Extra player/collision work executes on the host, outside the cartridge's
  * original CPU budget. Keep PPU, mapper IRQ, and APU time at native speed.
  * A native vblank wait must remain timed if a scripted transition reaches it.
  */
 if(!ij_loaded||!c.enabled||(pc>=0xf955&&pc<=0xf95d))return 0;
 return c.phase==2||flight.hit==2||c.damage==2||c.attack==2||c.object_prepared;
}
void ij_init(void){
 memset(&cave_exit,0,sizeof(cave_exit));
 memset(&lift_context,0,sizeof(lift_context));
 memset(&flight,0,sizeof(flight));
 grabbed_player=0;
 memset(&grab_drawing,0,sizeof(grab_drawing));
 memset(&safe_ground,0,sizeof(safe_ground));
 memset(&motorcycles,0,sizeof(motorcycles));
 transition_hidden=0;
 menu_open=menu_selected=menu_previous=0;
 start_menu=1;start_selection=0;
 memset(&platform_view,0,sizeof(platform_view));area_stage=255;menu_level=warp_pending=0;memset(&stone,0,sizeof(stone));
 memset(&inventory_transfer,0,sizeof(inventory_transfer));
 memset(&door_return,0,sizeof(door_return));
 memset(&deaths,0,sizeof(deaths));
 deaths.individual=1;
 /* Only the supported USA cartridge's native player code is hooked. */
 memset(&c,0,sizeof(c)); c.version=2;c.enabled=1;c.bank=128;noclip=0;ij_extra_cycles=0;
 ij_loaded=(ROM_size==8 && VROM_size==16 && ROM && VROM);
 if(ij_loaded)ij_loaded=CalcCRC32(CalcCRC32(0,ROM,128*1024),VROM,128*1024)==0x35c6f574u;
 if(!ij_loaded)return;
 memcpy(graphics,VROM,128*1024);memset(graphics+128*1024,0,128*1024);
 SetupCartCHRMapping(0,graphics,sizeof(graphics),0);
 address_count=0;
 range(0x59,0x5b);range(0x7a,0x89);range(0x8b,0x97);
 range(0x564,0x564);range(0x568,0x569);range(0x56b,0x56b);
 range(0x56d,0x575);range(0x577,0x57f);range(0x585,0x5a4);range(0x5d2,0x5db);range(0x495,0x495);
 /* $580-$584 are the shared moving-platform mask and vertical offsets.
  * Swapping them with player contexts hides platforms from the other IJ
  * and rolls back their movement when object dispatch restores P1. */
 range(0x499,0x49b);range(0x49e,0x49e);range(0x4bf,0x4bf);range(0x4c2,0x4c2);
 range(0x4bd,0x4bd); /* Native temporary-item HUD latch, separate from hit protection. */
 range(0x79,0x79);
 AddExState(&c,sizeof(c),0,"IJCP");
 AddExState(graphics+128*1024,2048,0,"IJCH");
 AddExState(&noclip,1,0,"IJNC");
 AddExState(&inventory_transfer,sizeof(inventory_transfer),0,"IJIV");
 AddExState(&deaths,sizeof(deaths),0,"IJDM");
 AddExState(&door_return,sizeof(door_return),0,"IJDR");
 AddExState(&cave_exit,sizeof(cave_exit),0,"IJCE");
 AddExState(&stone,sizeof(stone),0,"IJSN");
 AddExState(&platform_view,sizeof(platform_view),0,"IJPV");
 AddExState(&transition_hidden,1,0,"IJVH");
 AddExState(&area_stage,1,0,"IJAS");
 AddExState(&motorcycles,sizeof(motorcycles),0,"IJMC");
 AddExState(&safe_ground,sizeof(safe_ground),0,"IJSG");
 AddExState(&grabbed_player,1,0,"IJGP");
 AddExState(&grab_drawing,sizeof(grab_drawing),0,"IJGD");
 AddExState(&flight,sizeof(flight),0,"IJFL");
 AddExState(&lift_context,sizeof(lift_context),0,"IJLC");
}
static void composite(void){
 uint8_t used[32]={0},mapping[32];unsigned i,j,start=RAM[0x78],base_bank=RAM[0xec];
 uint8_t *merged;int slot;
 /* An OUT P1 skips the native sprite routine which normally selects a fresh
  * source bank. EC therefore still names last frame's merged bank. P2's
  * native sprite pass supplies the valid source while P1 is absent. */
 if(deaths.individual&&deaths.out[0])base_bank=c.p2bank;
 if(base_bank>=128||c.p2bank>=128)return;
 memset(mapping,255,sizeof(mapping));
 for(i=0;i<start;i+=4){unsigned t=RAM[0x201+i];if((t&1)&&t<64)used[t/2]=1;}
 c.bank=c.bank==128?129:128;merged=graphics+c.bank*1024;
 if(VPage[4]+0x1000==merged)c.graphics_errors++;
 memcpy(merged,graphics+base_bank*1024,1024);
 for(i=0;i<c.p2length && start+4<=252;i+=4){
  unsigned tile=c.p2oam[i+1],attr=c.p2oam[i+2];
  if((tile&1)&&tile<64){
   unsigned source=tile/2;
   if(mapping[source]==255){
    for(slot=31;slot>=0&&used[slot];slot--);
    if(slot<0)break;
    used[slot]=1;mapping[source]=slot;
    for(j=0;j<32;j++)merged[slot*32+j]=graphics[c.p2bank*1024+source*32+j];
   }
   tile=mapping[source]*2+1;attr=(attr&0xe0)|4; /* Exact unused-bit pattern tags P2 body pixels. */
  }
  RAM[0x200+start]=c.p2oam[i];RAM[0x201+start]=tile;
  RAM[0x202+start]=attr;RAM[0x203+start]=c.p2oam[i+3];start+=4;
 }
 RAM[0xec]=c.bank;RAM[0x78]=start;
}
#include "indiana_flight.inc"
uint16_t ij_instruction(uint16_t pc){
 unsigned i; int dx,dy,left,right,focus,delta;
 if(!ij_loaded||!c.enabled)return pc;
 if(c.has_p2&&pc>=0x8000&&pc<0xc000){
  unsigned bank=RAM[pc<0xa000?0xe2:0xe3],offset=pc&0x1fff;
  /* Every switchable-bank LDA $04BF is a native enemy time-stop check.
   * Evaluate the shared effect without copying the collector's timer into
   * the other inventory. Adjust its result after the load so its CPU cycles
   * remain native. Fixed-bank countdown/HUD reads remain per-player. */
  if(bank<16&&offset>=3&&ROM[bank*8192+offset-3]==0xad&&
     ROM[bank*8192+offset-2]==0xbf&&ROM[bank*8192+offset-1]==4){
   unsigned timer=other()->v[0x4bf];
   X.A=RAM[0x4bf]>timer?RAM[0x4bf]:timer;
   X.P=(X.P&0x7d)|(X.A?0:2)|(X.A&128);
  }
 }
 if((RAM[0xa0]==19||RAM[0xa0]==20)&&RAM[0xe2]==10){uint16_t next=flight_instruction(pc);if(next!=pc)return next;}
 if(warp_pending){
  warp_pending=0;RAM[0xa0]=menu_level;RAM[0xa3]=RAM[0xa6]=RAM[0x9a]=0;
  RAM[0xa4]=32;RAM[0xa5]=112;RAM[0x567]=0;
  memset(&door_return,0,sizeof(door_return));menu_open=0;
  prepare_area();return 0xc1e1;
 }
 switch(pc){
 case 0xae36:case 0xae64:
  if(RAM[0xe2]==12&&c.has_p2&&!lift_context.active&&X.X<4){
   Player *partner=c.phase==2?&c.p1:other();unsigned surface=16+X.X;
   int own=pc==0xae64?RAM[0x5da]:(RAM[0x8d]==surface&&RAM[0x8e]==surface&&RAM[0x8f]==surface&&!RAM[0x87]&&!RAM[0x88]);
   int theirs=pc==0xae64?(partner->v[0x5da]&&partner->v[0x8d]==surface):
    (partner->v[0x8d]==surface&&partner->v[0x8e]==surface&&partner->v[0x8f]==surface&&!partner->v[0x87]&&!partner->v[0x88]);
   if(!own&&theirs){capture(&lift_context.original);install(partner);lift_context.active=1;}
  }break;
 case 0xaa93:
  if(RAM[0xe2]==12&&lift_context.active){
   capture(c.phase==2?&c.p1:other());install(&lift_context.original);lift_context.active=0;
  }break;
 case 0xad07:if(RAM[0xe2]==10&&RAM[0xa0]==18)grabbed_player=c.object_active?2:1;break;
 case 0xae31:if(RAM[0xe2]==10&&RAM[0xa0]==18)grabbed_player=0;break;
 case 0xc4e3:
  motorcycle_pairs();
  if(cave_exit.active&&RAM[0xa0]==cave_exit.stage){
   for(i=0;i<80;i+=16)if(RAM[0x514+i]==0xba){
    int x=RAM[0x9d]*256+RAM[0x9a]+RAM[0x519+i]-(int8_t)RAM[0x9e];
    int y=RAM[0x518+i]-(int8_t)RAM[0x566]+RAM[0x9b];
    if(abs(x-cave_exit.x)<=24&&abs(y-cave_exit.y)<=32){
     /* Reloading the outside map recreates its entrance boulder. Treat
      * just that obstacle as cleared, using the native spawn bitmap so
      * scrolling away and back cannot recreate it again. */
     unsigned source=RAM[0x483+i];
     if(source<64)RAM[(i<32?0x504:0x50c)+source/8]|=1<<(source&7);
     RAM[0x514+i]=0;cave_exit.active=0;
    }
   }
  }
  break;
 case 0xa2d5:if(RAM[0xe2]==0&&RAM[0x45]==1&&c.has_p2&&X.X<80&&motorcycles.owner[X.X/16])return 0xa2e3;break;
 case 0xc59f:
  grab_drawing.active=grabbed_player==2&&RAM[0xa0]==18&&X.X<32&&RAM[0x514+X.X]==0x6a&&
   ((X.X==0&&RAM[0x517]>=12&&RAM[0x517]<=22)||(X.X==16&&RAM[0x527]==24))&&
   RAM[0x519+X.X]<241&&!RAM[0x51c+X.X]&&RAM[0x518+X.X]>=16;
  grab_drawing.start=RAM[0x78];
  motorcycles.draw_active=c.has_p2&&X.X<80&&motorcycles.owner[X.X/16]==2&&RAM[0x514+X.X]==0xc0&&RAM[0x519+X.X]<241&&!RAM[0x51c+X.X]&&RAM[0x518+X.X]>=16;
  motorcycles.draw_start=RAM[0x78];break;
 /* C641 also returns each emitted byte; remap only after the whole object. */
 case 0xc638:
  if(motorcycles.draw_active)motorcycles.draw_active=2;
  if(grab_drawing.active)grab_drawing.active=2;break;
 case 0xc641:
  /* The native grab combines blue enemy tiles and a yellow player in one
   * object. Only palette-zero victim tiles receive the P2 color tag. */
  if(grab_drawing.active==2){
   for(i=grab_drawing.start;i<RAM[0x78];i+=4)if(!(RAM[0x202+i]&3))RAM[0x202+i]=(RAM[0x202+i]&0xe0)|4;
   grab_drawing.active=0;
  }
  if(motorcycles.draw_active==2){motorcycle_graphics(X.A);motorcycles.draw_active=0;}break;
 case 0xde51:{
  /* Level cheats and old saves can leave the story cursor ahead of the map.
   * Resume the chapter belonging to this destination, not a later ending.
   * Capture players before the story code replaces equipment and registers. */
  static const uint8_t stages[]={0,3,7,8,9,11,17,18,19,20,21,26,29,30,31};
  /* 42 is the ending terminator, not an entry point: final completion
   * must begin at 39 and let the native interpreter advance to it. */
  static const uint8_t cursors[]={0,5,9,12,19,24,26,27,30,32,33,36,37,39,39};
  for(i=0;i<sizeof(stages);i++)if(RAM[0xa0]==stages[i]){RAM[0x47]=cursors[i];break;}
  if(c.has_p2){
   preserve_inventory();
   if(c.phase==2){capture(&c.p2);install(&c.p1);}
   c.phase=c.camera=0;
  }
  transition_hidden=1;break;
 }
 case 0xde8c:
  /* The story-skip flag must not bypass the last playable area. The real
   * ending still runs through the cartridge's stage-31 completion path. */
  if(RAM[0xa0]==30)return 0xde81;
  break;
 case 0xa00d:case 0xa014:case 0xa022:
  if(RAM[0xe2]==2)transition_hidden=1;break; /* Native fade, including old mid-fade saves. */
 case 0x9c67:
  /* The water/boss transition owns one world camera and lock. Running it
   * again as grounded P2 can freeze an airborne P1 before he can land. */
  if(RAM[0xe2]==12&&RAM[0xa0]==7&&c.phase==2&&!(deaths.individual&&deaths.out[0]))return popreturn();
  break;
 case 0xa7f5:
  if(RAM[0xe2]==2&&c.has_p2){
   stone.active=1;stone.owner=c.object_active?1:0;
   stone.partner=!deaths.out[1-stone.owner]&&stone_contact(other(),32);
  }break;
 case 0xa812:
  if(RAM[0xe2]==2&&stone.active&&stone.partner){Player *p=other();p->v[0x571]=39;p->v[0x88]|=2;p->v[0x7c]+=8;}break;
 case 0xa836:
  if(RAM[0xe2]==2&&stone.active&&stone.partner)other()->v[0x7c]-=8;break;
 case 0xa869:
  if(RAM[0xe2]==2&&stone.active){
   if(stone.partner){Player *p=other();p->v[0x5b]=11;p->v[0x5a]=0;p->v[0x87]=1;}
   memset(&stone,0,sizeof(stone));
  }break;
 case 0xc1a0:{unsigned mode=deaths.individual;memset(&deaths,0,sizeof(deaths));deaths.individual=mode;memset(&inventory_transfer,0,sizeof(inventory_transfer));memset(&door_return,0,sizeof(door_return));reset_players();break;}
 case 0x83d9:
  if(RAM[0xe2]==12&&c.has_p2){door_return.valid=1;door_return.stage=RAM[0xa0];door_return.x=RAM[0x7a];door_return.y=RAM[0x7c];}break;
 case 0x9c64:
  if(RAM[0xe2]==12&&c.has_p2){
   door_return.pending=1;
   if(door_return.valid&&door_return.stage==RAM[0xa0]){RAM[0xa4]=door_return.x;RAM[0xa5]=door_return.y;}
   cave_exit.active=1;cave_exit.stage=RAM[0xa0];
   cave_exit.x=(RAM[0xa3]-1)*256+RAM[0x9a]+RAM[0xa4];
   cave_exit.y=RAM[0xa5]+RAM[0xa6];
  }break;
 case 0xc1e1:prepare_area();break;
 case 0x9b88:case 0x9bd9:
  /* Vertical exits run inside P1's camera pass, where $7A is temporarily
   * the camera target. Store the actual player's X as the next entrance. */
  if(RAM[0xe2]==12&&c.has_p2){
   door_return.pending=1; /* Vertical exits also require a shared landing. */
   if(c.phase==1&&c.camera==1)X.A=clamp(c.x-(int8_t)RAM[0x9e],8,237);
  }
  break;
 case 0x9be6:if(RAM[0xe2]==12&&deaths.individual&&c.has_p2)return individual_death();break;
 case 0x8000:
  if(RAM[0xe2]==12&&deaths.individual&&deaths.out[c.phase==2?1:0]&&(c.phase==1||c.phase==2)){RAM[0x78]=0;return 0xc426;}break; /* Native RTS: let the 861D hook run next. */
 case 0xea11:if(c.has_p2){transition_whip();return 0xea1d;}break;
 case 0xe649:if(c.has_p2){transition_whip();return 0xc1ba;}break;
 case 0xa69e:
  if(RAM[0xe2]==0&&(c.has_p2||inventory_transfer.pending))transition_whip();
  break;
 case 0xe7e3:preserve_inventory();break; /* Story sequence temporarily equips a sword/hat. */
 case 0xc15c:if(c.has_p2&&!(RAM[0xf0]&3)&&c.p2.v[0x4bf])c.p2.v[0x4bf]--;break;
 case 0xc0e6:if(c.active)c.paused=1;break;
 case 0xdba3:
  /* The generic HUD's weapon slot shares flight's bomb tiles. Its normal
   * inventory value (whip=1) must never impersonate equipped plane bombs. */
  if(flight.active&&(RAM[0xa0]==19||RAM[0xa0]==20)){X.A=RAM[0x60d]?1:0;X.Y=0;}
  break;
 case 0xdbda:case 0xdbff:
  /* Native flight equipment updates own the other two armor slots. */
  if(flight.active&&(RAM[0xa0]==19||RAM[0xa0]==20))return popreturn();
  break;
 case 0xe18b:RAM[0x4bd]=0x4a;break;
 case 0xe1a8:RAM[0x4bd]=0x4c;break;
 case 0xc100:c.paused=0;break;
 case 0xfa71:
  break;
 case 0x808d:
  if(RAM[0xe2]!=12)break;
  release_finished_lift();
  if(c.phase==2){if(deaths.individual&&deaths.out[1])return ghost();if(noclip)return fly();break;}
  c.active=1;c.paused=0;c.ticks++;area_stage=RAM[0xa0];transition_hidden=0;
  if(!c.has_p2){
   if(deaths.out[0]&&deaths.out[1]){deaths.out[0]=deaths.out[1]=0;deaths.pending=0;deaths.safe_valid[0]=deaths.safe_valid[1]=0;}
   /* Area exits provide one valid landing/ladder coordinate. The opening
    * separation would put P2 beside a narrow ladder and back over the exit. */
   capture(&c.p2);if(!door_return.pending)c.p2.v[0x7a]=clamp(RAM[0x7a]+24,0,224);c.has_p2=1;
   door_return.pending=0;
   if(deaths.pending){RAM[0x79]=deaths.lives[0];c.p2.v[0x79]=deaths.lives[1];deaths.pending=0;}
   if(inventory_transfer.pending){
   for(i=0;i<10;i++){unsigned a=inventory_addresses[i];RAM[a]=inventory_transfer.items[0][i];c.p2.v[a]=inventory_transfer.items[1][i];}
   inventory_transfer.pending=0;
   /* Force the native HUD cache to refresh after the area loader. */
    memset(RAM+0x4b4,255,8);
   }
   level_outfits();
  }
  c.phase=1;if(c.invincible){RAM[0x569]=30;c.p2.v[0x569]=30;}
  if(deaths.individual&&deaths.out[0])return ghost();
  if(noclip)return fly();break;
 /* EA61 accepts X/Y; boss projectiles call EA65 with $00/$01 already set.
  * Replay their common body, which leaves those point coordinates intact. */
 case 0xea65:
  /* Pickups borrow the point-hit routine and restore $86 for the selected
   * collector at DF04. Replaying that query on the partner leaves a real
   * damage flag behind. Object dispatch already supplies the collector. */
  {unsigned ret=RAM[0x100+(uint8_t)(X.S+1)]|(RAM[0x100+(uint8_t)(X.S+2)]<<8);
   if((ret>=0xdf85&&ret<=0xdfaa)||ret==0xdefd)break;
   /* Vehicle mounting is a contact query for its assigned rider. */
   if(RAM[0xe2]==0&&ret>=0xa364&&ret<=0xa3db)break;
   if(RAM[0xe2]==0){
    unsigned caller=RAM[0x100+(uint8_t)(X.S+3)]|(RAM[0x100+(uint8_t)(X.S+4)]<<8);
    /* The bottom row of the mounting rectangle tail-calls E42F. Its
     * nested EA61 calls return into that shared helper, and its last
     * point returns straight to A32C. These are still pickup queries. */
    if(ret==0xa32b||((ret==0xe432||ret==0xe43e||ret==0xe44a||ret==0xe456)&&caller==0xa32b))break;
   }
  }
  if(c.has_p2&&c.phase==0&&!c.damage){capture(&c.damage_player);c.damage_entry=regs();c.damage=1;}break;
 case 0xeab2:case 0xeb0b:
  if(c.damage==1){capture(&c.damage_player);c.damage_result=regs();c.damage=2;
   install(other());restore(c.damage_entry);return 0xea65;}
  if(c.damage==2){int hit=X.A==0||c.damage_result.a==0;
   unsigned ret=RAM[0x100+(uint8_t)(X.S+1)]|(RAM[0x100+(uint8_t)(X.S+2)]<<8);
   int artifact=RAM[0xe2]==10&&ret==0xa03e;
   /* The boss artifact reads $86 as a collection result, not A. Return the
    * combined contact to its caller without leaving damage on P2. */
   if(artifact)RAM[0x86]=other()->v[0x86];
   capture(other());install(&c.damage_player);
   restore(c.damage_result);if(hit){X.A=0;X.P=(X.P&0x7d)|2;if(artifact)RAM[0x86]=255;}c.damage=0;}break;
 case 0xed63:
  if(c.has_p2&&c.phase==0&&!c.attack){capture(&c.attack_player);c.attack=1;}break;
 case 0xee79:
  if(c.attack==1){capture(&c.attack_player);memcpy(c.attack_boxes,RAM+0x5c,24);memcpy(c.attack_enemy,RAM+0x74,4);
   c.attack_result=regs();c.attack_hit=RAM[0x73];c.attack=2;install(other());pushreturn(0x600b);return 0xeb3a;}
  if(c.attack==2){unsigned hit=RAM[0x73];capture(other());install(&c.attack_player);memcpy(RAM+0x5c,c.attack_boxes,24);
   RAM[0x73]=clamp(c.attack_hit+hit,0,255);restore(c.attack_result);c.attack=0;}break;
 case 0x600b:if(c.attack==2){memcpy(RAM+0x74,c.attack_enemy,4);return 0xed63;}break;
 case 0xc53f:case 0xc556:case 0xc56d:case 0xc580:case 0xc593:
  if(!c.has_p2||c.phase!=0)break;
  if(c.object_prepared){c.object_prepared=0;break;}
  {int x=RAM[0x519+X.X],y=RAM[0x518+X.X];
   unsigned kind=RAM[0x514+X.X];int choose=-1;
   if(kind==0x6a&&grabbed_player&&RAM[0xa0]==18&&
      ((X.X==0&&RAM[0x517]>=12&&RAM[0x517]<=22)||(X.X==16&&RAM[0x527]==24)))choose=grabbed_player-1;
   if(kind==0xc0&&RAM[0x45]==1&&X.X<80&&motorcycles.owner[X.X/16])choose=motorcycles.owner[X.X/16]-1;
   if(kind==0xc0&&RAM[0x45]!=1){
    /* Tanks remain world objects while occupied. $95 encodes slot+1 on
     * entry, then slot|$80 while driving. Keep their update on the driver
     * even when the other player becomes closer to last frame's position. */
    unsigned a=RAM[0x95],b=c.p2.v[0x95];
    if(a&&((a&128)?(a&127):a-1)==X.X)choose=0;
    else if(b&&((b&128)?(b&127):b-1)==X.X)choose=1;
   }
   if(kind==0x20||kind==0x24){
    if(stone.active)choose=stone.owner;
    else if(kind==0x20&&RAM[0x517+X.X]==1){
     if(!deaths.out[0]&&stone_contact(0,1))choose=0;
     else if(!deaths.out[1]&&stone_contact(&c.p2,1))choose=1;
    }
   }
   if(deaths.individual&&deaths.out[1])break;
   if(choose==0)break;
   if(choose<0&&!(deaths.individual&&deaths.out[0])&&abs(c.p2.v[0x7a]-x)+abs(c.p2.v[0x7c]-y)>=abs(RAM[0x7a]-x)+abs(RAM[0x7c]-y))break;
   capture(&c.object_player);memcpy(c.object_boxes,RAM+0x5c,24);c.object_regs=regs();c.object_entry=pc;c.object_active=1;
   install(&c.p2);c.object_prepared=1;pushreturn(0x6023);return 0xeb3a;}
 case 0x6023:
  if(c.object_active){restore(c.object_regs);c.object_prepared=0;return c.object_entry;}break;
 case 0xc542:case 0xc559:case 0xc570:case 0xc583:case 0xc596:
  if(c.object_active){capture(&c.p2);install(&c.object_player);memcpy(RAM+0x5c,c.object_boxes,24);c.object_active=0;}break;
 case 0x9a0c:
  if(RAM[0xe2]!=12||!c.has_p2)break;
  if(c.phase==2){RAM[0x7a]=clamp(RAM[0x7a],8,237);return noclip||(deaths.individual&&deaths.out[1])?popreturn():coop_vertical_boundary();}
  c.camera=1;c.x=RAM[0x7a];c.y=RAM[0x7c];c.anchor=RAM[0x7e];
  if(scripted_scene()){c.camera=2;break;} /* Native scene deliberately moves its camera anchor. */
  left=c.x<c.p2.v[0x7a]?c.x:c.p2.v[0x7a];right=c.x>c.p2.v[0x7a]?c.x:c.p2.v[0x7a];focus=(left+right)/2;
  if(deaths.individual&&deaths.out[0])left=right=focus=c.p2.v[0x7a];
  if(deaths.individual&&deaths.out[1])left=right=focus=c.x;
  delta=focus>144?clamp(focus-144,0,2):focus<112?clamp(focus-112,-2,0):0;
  if((delta>0&&left<24)||(delta<0&&right>224))delta=0;
  /* A respawn/area loader can leave the native vertical anchor at the HUD.
   * Frame both live players instead of retaining that single-player anchor. */
  if(RAM[0x565]&&!(RAM[0x8a]&2)){
   int top=c.y<c.p2.v[0x7c]?c.y:c.p2.v[0x7c];
   int bottom=c.y>c.p2.v[0x7c]?c.y:c.p2.v[0x7c];
   int middle=(top+bottom)/2,vertical,speed=2;
   int absent1=(deaths.individual&&deaths.out[0])||RAM[0x564]||RAM[0x56b];
   int absent2=(deaths.individual&&deaths.out[1])||c.p2.v[0x564]||c.p2.v[0x56b];
   if(absent1)top=bottom=middle=c.p2.v[0x7c];
   if(absent2)top=bottom=middle=c.y;
   /* Falling speed exceeds the gentle idle recentering rate. Track airborne
    * motion promptly so a valid descent cannot outrun the viewport. */
   if((!absent1&&RAM[0x87])||(!absent2&&c.p2.v[0x87]))speed=12;
   vertical=middle>128?clamp(middle-128,0,speed):middle<96?clamp(middle-96,-speed,0):0;
   if((vertical>0&&top<48)||(vertical<0&&bottom>200))vertical=0;
   if(absent1&&absent2)vertical=0;
   RAM[0x7c]=middle;
   RAM[0x7f]=clamp(RAM[0x7c]-vertical,0,255);
  }
  RAM[0x9e]=RAM[0x56c]=0;RAM[0x7e]=128;RAM[0x7a]=128+delta;break;
 case 0x9b67:
  if(RAM[0xe2]!=12||!c.has_p2)break;
  /* Camera anchors are temporary. Native exits and corpse removal must
   * inspect the actual player, including when the camera follows a survivor. */
  finish_coop_camera();
  if(noclip||(deaths.individual&&deaths.out[c.phase==2?1:0]))return popreturn();
  return coop_vertical_boundary();
 case 0x8b19:
  if((noclip||(deaths.individual&&deaths.out[1]))&&c.phase==2)return popreturn();
  if(RAM[0xe2]!=12||c.phase!=1||!c.camera)break;
  if(c.camera==2){
   c.p2.v[0x7a]=clamp(c.p2.v[0x7a]-(int8_t)RAM[0x9e],8,237);
   c.p2.v[0x7c]-=(int8_t)RAM[0x566];c.camera=0;break;
  }
  finish_coop_camera();
  if(noclip||(deaths.individual&&deaths.out[0]))return popreturn();break;
 case 0x861d:
  if(RAM[0xe2]!=12||!c.has_p2)break;
  if(c.phase==1){capture(&c.p1);c.regs=regs();memcpy(c.oam,RAM+0x200,256);
   for(i=0;i<8;i++)c.shared[i]=RAM[shared_addresses[i]];
   /* P2 runs after the camera moved, before native platform drawing updates
    * their screen coordinates. Match collision surfaces to P2's view. */
   platform_view.active=1;
   for(i=0;i<4;i++){
    platform_view.y[i]=RAM[0x5b3+i];platform_view.x[i]=RAM[0x5c7+i];
    if(RAM[0x5b3+i]<240){RAM[0x5b3+i]-=(int8_t)RAM[0x566];RAM[0x5c7+i]-=(int8_t)RAM[0x9e];}
   }
   install(&c.p2);if(!scripted_scene()){RAM[0xf5]=RAM[0xf6];RAM[0xf7]=RAM[0xf8];}RAM[0x9e]=RAM[0x566]=0;
   c.phase=2;release_finished_lift();pushreturn(0x6003);return deaths.individual&&deaths.out[1]?ghost():noclip?fly():0x808d;}
  if(c.phase==2)return 0x8626;
  break;
 case 0x6003:
  if(c.has_p2&&c.phase==2){capture(&c.p2);c.p2bank=RAM[0xec];c.p2length=RAM[0x78];memcpy(c.p2oam,RAM+0x200,c.p2length);
   if(platform_view.active){for(i=0;i<4;i++){RAM[0x5b3+i]=platform_view.y[i];RAM[0x5c7+i]=platform_view.x[i];}platform_view.active=0;}
   install(&c.p1);memcpy(RAM+0x200,c.oam,256);for(i=0;i<8;i++)RAM[shared_addresses[i]]=c.shared[i];
   restore(c.regs);c.phase=3;c.passes++;return 0x861d;}break;
 case 0xc4ba:if(c.has_p2&&c.phase==3){
  if(deaths.individual){
   deaths.lives[0]=RAM[0x79];deaths.lives[1]=c.p2.v[0x79];
   for(i=0;i<2;i++){
    Player current={{0}};if(i)current=c.p2;else capture(&current);
    if(!noclip&&!deaths.out[i]&&grounded_player(&current)){
     if(safe_ground.grounded[i]<8)safe_ground.grounded[i]++;
     if(safe_ground.grounded[i]>=8){
      deaths.safe[i]=current;deaths.safe_valid[i]=safe_ground.valid[i]=1;
      safe_ground.scroll_x[i]=RAM[0x9a]+256*RAM[0x9d];safe_ground.scroll_y[i]=RAM[0x9b];
     }
    }else safe_ground.grounded[i]=0;
   }
  }
  if(c.p2.v[0x4bf]>1)c.p2.v[0x4bd]=0x4a;
  else if(c.p2.v[0x569]>30)c.p2.v[0x4bd]=0x4c;
  if((c.p2.v[0x4bd]==0x4a&&c.p2.v[0x4bf]<=1)||
     (c.p2.v[0x4bd]==0x4c&&c.p2.v[0x569]<=1))c.p2.v[0x4bd]=0;
  composite();c.phase=0;
 }break;
 }
 return pc;
}
IJ_API void ij_enable(unsigned enabled){c.enabled=!!enabled;}
IJ_API unsigned ij_read(unsigned player,unsigned address){
 if(address>=0x800)return 0;
 if(player==2&&flight.active){
  if(address>=0x36&&address<=0x3f)return flight.p2.zp[address-0x36];
  if(address>=0x160&&address<=0x163)return flight.p2.stats[address-0x160];
  if(address>=0x600&&address<=0x60f)return flight.p2.local[address-0x600];
 }
 return player==2?(address<0x600&&c.has_p2?c.p2.v[address]:0):RAM[address];
}
IJ_API void ij_write(unsigned player,unsigned address,unsigned value){
 if(address>=0x800)return;
 if(player==2&&flight.active){
  if(address>=0x36&&address<=0x3f){flight.p2.zp[address-0x36]=value;return;}
  if(address>=0x160&&address<=0x163){flight.p2.stats[address-0x160]=value;return;}
  if(address>=0x600&&address<=0x60f){flight.p2.local[address-0x600]=value;return;}
 }
 if(player==2){if(address<0x600&&c.has_p2)c.p2.v[address]=value;}else RAM[address]=value;
}
IJ_API void ij_status(unsigned *out){
 if(!out)return;
 out[0]=ij_loaded;out[1]=c.active;out[2]=c.paused;out[3]=c.phase;out[4]=c.ticks;out[5]=c.passes;
 out[6]=c.has_p2;out[7]=c.invincible;out[8]=RAM[0xa0];out[9]=RAM[0x79];out[10]=X.PC;out[11]=c.bank;
}
IJ_API void ij_rejoin(unsigned player){
 Player target,partner;unsigned i;
 static const uint16_t position[]={0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,0x8d,0x8e,0x8f,0x90,0x91,0x92,0x93};
 static const uint16_t clear[]={0x59,0x5a,0x5b,0x56d,0x56e,0x56f,0x87,0x88,0x94,0x95,0x96,0x97};
 if(!c.has_p2||c.phase!=0)return;
 if(player==2){capture(&partner);target=c.p2;}else{partner=c.p2;capture(&target);}
 if(partner.v[0x87]||partner.v[0x564])return;
 for(i=0;i<sizeof(position)/sizeof(*position);i++)target.v[position[i]]=partner.v[position[i]];
 for(i=0;i<sizeof(clear)/sizeof(*clear);i++)target.v[clear[i]]=0;
 target.v[0x569]=30;if(player==2)c.p2=target;else install(&target);
}
IJ_API int ij_cheat(unsigned action){
 if(!c.active||!c.has_p2||c.phase!=0)return 0;
 switch(action){
 case 0:c.invincible=!c.invincible;break;
 case 1:RAM[0x79]=9;if(deaths.individual){c.p2.v[0x79]=9;deaths.out[0]=deaths.out[1]=0;}break;
 case 2:RAM[0x83]=c.p2.v[0x83]=0;RAM[0x495]=c.p2.v[0x495]=1;break;
 case 3:ij_rejoin(2);break;
 case 4:
  cancel_stone_lock();noclip=!noclip;
  if(!noclip){
   RAM[0x87]=c.p2.v[0x87]=1;
   RAM[0x5a]=RAM[0x5b]=c.p2.v[0x5a]=c.p2.v[0x5b]=0;
   RAM[0x5da]=RAM[0x5db]=c.p2.v[0x5da]=c.p2.v[0x5db]=0;
  }break;
 case 5:
  deaths.individual=!deaths.individual;deaths.out[0]=deaths.out[1]=0;
  c.p2.v[0x79]=RAM[0x79];capture(&deaths.safe[0]);deaths.safe[1]=c.p2;
  deaths.safe_valid[0]=deaths.safe_valid[1]=1;
  {unsigned n;for(n=0;n<2;n++){
   safe_ground.valid[n]=grounded_player(&deaths.safe[n]);safe_ground.grounded[n]=0;
   safe_ground.scroll_x[n]=RAM[0x9a]+256*RAM[0x9d];safe_ground.scroll_y[n]=RAM[0x9b];
  }}break;
 case 6:warp_pending=1;break;
 case 7:do{menu_level=(menu_level+1)%36;}while(menu_level==31);break;
 case 8:do{menu_level=(menu_level+35)%36;}while(menu_level==31);break;
 default:return 0;
 }return 1;
}
IJ_API unsigned ij_level_choice(void){return menu_level;}
IJ_API void ij_level_current(void){menu_level=RAM[0xa0];}
IJ_API void ij_test_pc(unsigned pc){X.PC=pc;reset_players();}
IJ_API unsigned ij_graphics_errors(void){return c.graphics_errors;}

/* Frontend-independent pause cheats. NES A/B are bits 0/1 after mapping. */
uint32_t ij_menu_input(uint32_t pads){
 unsigned p1=pads&255,p2=(pads>>8)&255,held=p1|p2,edge=held&~menu_previous;
 unsigned chord=(p1&3)==3||(p2&3)==3;
 menu_previous=held;
 if(ij_loaded&&start_menu){
  if(edge&(16|32))start_selection^=1;
  deaths.individual=start_selection==0;
  if(edge&(1|8)){start_menu=0;return 0;}
  return 0;
 }
 if(!ij_loaded||!c.enabled||!c.active){menu_open=0;return pads;}
 pads|=p2&8; /* Either player's Start operates the game's pause. */
 if(!c.paused){menu_open=0;return pads;}
 if(chord&&(edge&3)){menu_open=1;menu_selected=0;ij_level_current();}
 else if(menu_open){
  if(edge&16)menu_selected=(menu_selected+6)%7;
  if(edge&32)menu_selected=(menu_selected+1)%7;
  if(menu_selected==6){if(edge&64)ij_cheat(8);if(edge&128)ij_cheat(7);}
  if(edge&1)ij_cheat(menu_selected);
 }
 if(menu_open){
  if(edge&8)menu_open=0;
  return pads&8;
 }
 return pads;
}
IJ_API int ij_menu_visible(void){return ij_loaded&&(start_menu||(c.paused&&menu_open));}
int ij_start_menu_visible(void){return ij_loaded&&start_menu;}
/* Original compact 5x7 uppercase bitmap font, rows packed in five bits. */
static const uint8_t menu_font[26][7]={
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}};
static void menu_text(uint8_t *pixels,unsigned x,unsigned y,const char *text,unsigned color){
 while(*text){unsigned row,col;char ch=*text++;
  const uint8_t *glyph=ch>='A'&&ch<='Z'?menu_font[ch-'A']:0;
  static const uint8_t digits[10][7]={{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14}};
  if(ch>='0'&&ch<='9')glyph=digits[ch-'0'];
  if(glyph)for(row=0;row<7;row++)for(col=0;col<5;col++)if(glyph[row]&(16>>col))pixels[(y+row)*256+x+col]=color|128;
  x+=6;
 }
}
void ij_menu_draw(uint8_t *pixels){
 unsigned y,i;char level[24];const char *items[]={c.invincible?"INVINCIBLE ON":"INVINCIBLE OFF","LIVES 9","HATS BOTH","REJOIN P2",noclip?"NOCLIP ON":"NOCLIP OFF",deaths.individual?"MODE EASY":"MODE CLASSIC",level};
 sprintf(level,"LEVEL %02u",menu_level+1);
 if(!ij_menu_visible())return;
 if(start_menu){
  memset(pixels,0x8f,256*240);
  menu_text(pixels,74,43,"COOP GAME MODE",0x2c);
  menu_text(pixels,38,66,"EASY",start_selection==0?0x28:0x30);
  menu_text(pixels,38,80,"SEPARATE LIVES",0x30);
  menu_text(pixels,38,93,"RESPAWN NEAR PARTNER",0x30);
  menu_text(pixels,38,119,"CLASSIC",start_selection==1?0x28:0x30);
  menu_text(pixels,38,133,"SHARED LIVES",0x30);
  menu_text(pixels,38,146,"BOTH RESPAWN AT CHECKPOINT",0x30);
  for(y=67+start_selection*53;y<72+start_selection*53;y++)memset(pixels+y*256+26,0xa8,4);
  menu_text(pixels,62,178,"UP DOWN SELECT",0x30);
  menu_text(pixels,44,194,"A OR START CONTINUE",0x30);
  return;
 }
 for(y=48;y<224;y++)memset(pixels+y*256+20,0x8f,216);
 menu_text(pixels,92,58,"CHEATS",0x2c);
 for(i=0;i<7;i++){
  menu_text(pixels,40,76+i*16,items[i],i==menu_selected?0x28:0x30);
  if(i==menu_selected)for(y=77+i*16;y<82+i*16;y++)memset(pixels+y*256+29,0xa8,4);
 }
 menu_text(pixels,32,190,"LEFT RIGHT CHANGE LEVEL",0x30);
 menu_text(pixels,32,202,"UP DOWN SELECT   A APPLY",0x30);
 menu_text(pixels,68,214,"START RESUME",0x30);
}

/* Reuse the cartridge HUD, including its border, portrait and typography. */
static unsigned hud_pixel(unsigned tile,unsigned x,unsigned y){
 /* Flight replaces the HUD pattern bank: the same tile IDs are aircraft
  * equipment there, rather than the guns/items used by platform stages. */
 const uint8_t *g=graphics+(flight.active?76:72)*1024+tile*16;
 return ((g[y]>>(7-x))&1)|(((g[y+8]>>(7-x))&1)<<1);
}
static unsigned hud_weapon(unsigned weapon,unsigned sub){
 unsigned tile=weapon==5?5+sub:weapon;
 if(tile==8)tile=16;
 return tile<=16?0x20+2*tile:0x20;
}
static void hud_tile(uint32_t *out,unsigned width,unsigned x,unsigned y,unsigned tile,const uint32_t *colors){
 unsigned r,k;
 for(r=0;r<8;r++)for(k=0;k<8;k++)out[(y+r)*width+x+k]=colors[hud_pixel(tile,k,r)];
}
static void hud_icon(uint32_t *out,unsigned width,unsigned x,unsigned y,unsigned tile,const uint32_t *colors){
 unsigned tx,ty;
 for(ty=0;ty<2;ty++)for(tx=0;tx<2;tx++)hud_tile(out,width,x+tx*8,y+ty*8,tile+ty*16+tx,colors);
}
static void hud_out(uint32_t *out,unsigned width,unsigned x,unsigned y){
 unsigned r,k,n;const char *text="OUT";
 for(r=0;r<16;r++)for(k=0;k<24;k++)out[(y+r)*width+x+k]=0;
 for(n=0;n<3;n++)for(r=0;r<7;r++)for(k=0;k<5;k++)
  if(menu_font[text[n]-'A'][r]&(16>>k))out[(y+4+r)*width+x+2+n*6+k]=0xffffff;
}
const uint32_t *ij_video(const uint32_t *pixels,unsigned width,unsigned height,unsigned stride,unsigned crop_left,unsigned crop_top){
 static uint32_t out[256*280];unsigned row,i,x,y;int show=c.has_p2&&!transition_hidden;
 const uint32_t colors[4]={0,0xffffff,0xa8edb0,0x42be68};
 const uint8_t *v=c.p2.v;
 if(!ij_loaded||width>256||height>240)return pixels;
 memset(out,0,sizeof(out));
 for(row=0;row<height;row++)memcpy(out+(row+(row>=40&&show?40:0))*width,pixels+row*stride,width*4);
 if(!show)return out;
 for(row=0;row<40;row++)memcpy(out+(row+40)*width,pixels+row*stride,width*4);
 /* Read original HUD tiles directly, with P2's own fixed four-color palette.
  * Sampling colors from P1's mutable item slots fails when damage clears a
  * slot or a cached tile hasn't reached the PPU yet. */
 x=64-crop_left;y=21-crop_top;
 for(row=25;row<29;row++)for(i=2;i<30;i++)
  hud_tile(out,width,i*8-crop_left,13+(row-25)*8-crop_top+40,vnapage[2][row*32+i],colors);
 y+=40;
 if(deaths.individual)hud_tile(out,width,48-crop_left,y+8,(v[0x79]%10)+1,colors);
 hud_icon(out,width,x,y,hud_weapon(v[0x82],v[0x84]),colors);
 hud_icon(out,width,80-crop_left,y,v[0x495]==1?0x42:v[0x495]==2?0x44:0x20,colors);
 hud_icon(out,width,96-crop_left,y,v[0x4bd]==0x4a||v[0x4bd]==0x4c?v[0x4bd]:v[0x4c2]==1?0x46:v[0x4c2]==2?0x48:0x20,colors);
 if(flight.active){
  hud_icon(out,width,x,y,0x20+2*flight.p2.local[13],colors);
  hud_icon(out,width,x+16,y,flight.p2.local[12]?0x24:0x20,colors);
  hud_icon(out,width,x+32,y,flight.p2.local[12]==2?0x24:0x20,colors);
 }
 /* The native score is five decimal tiles followed by a static zero. */
 {
  unsigned score=v[0x499]|(v[0x49a]<<8)|(v[0x49b]<<16),divisor=10000;
  for(i=0;i<5;i++,divisor/=10)hud_tile(out,width,128-crop_left+i*8,y+8,(score/divisor)%10+1,colors);
 }
 hud_tile(out,width,208-crop_left,y+8,v[0x49e]>=10?v[0x49e]/10+1:0,colors);
 hud_tile(out,width,216-crop_left,y+8,v[0x49e]%10+1,colors);
 if(deaths.individual&&deaths.out[0])hud_out(out,width,32-crop_left,y-40);
 if(deaths.individual&&deaths.out[1])hud_out(out,width,32-crop_left,y);
 return out;
}
