// Native Windows frontend for Indiana Co-op. GPL-2.0-or-later.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <xinput.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include "engine/src/drivers/libretro/libretro-common/include/libretro.h"

struct Core {
 HMODULE dll;
 void (*init)(),(*run)(),(*deinit)(),(*unload_game)(),(*reset)();
 void (*set_environment)(retro_environment_t);void (*set_video_refresh)(retro_video_refresh_t);
 void (*set_audio_sample)(retro_audio_sample_t);void (*set_audio_sample_batch)(retro_audio_sample_batch_t);
 void (*set_input_poll)(retro_input_poll_t);void (*set_input_state)(retro_input_state_t);
 void (*set_controller_port_device)(unsigned,unsigned);
 bool (*load_game)(const retro_game_info*);void (*get_system_av_info)(retro_system_av_info*);
 size_t (*serialize_size)();bool (*serialize)(void*,size_t);bool (*unserialize)(const void*,size_t);
 void (*status)(unsigned*),(*rejoin)(unsigned);int (*cheat)(unsigned);
 unsigned (*noclip)(),(*death_mode)(),(*level_choice)();void (*level_current)();
} core{};
static std::map<std::string,std::string> options{{"fceumm_nospritelimit","enabled"},{"fceumm_region","NTSC"}};
static std::vector<uint32_t> pixels;
static std::vector<int16_t> sound;
static unsigned width=256,height=224,format=1,pads[2]{},state[12]{};
static HWND windowHandle,controlsWindow;
static bool running=true,help=false,cheatOpen=false,wasChord=false,releaseActions=false;
static unsigned previousActions=0,previousRejoin=0,selectedCheat=0,slot=1;
static std::string notice;
static DWORD noticeUntil=0;
static int testFrames=0,frameNumber=0;
static unsigned audioDropped=0;
static bool testScenario=false;
static bool uiTest=false,testMenuOpened=false,testMenuApplied=false,testBindings=false;
static int testMenuStart=-1;
static const char *bindingPath(){return testFrames?".\\diagnostics\\native-controls-test.ini":".\\controls.ini";}
static HWAVEOUT audioDevice;
static WAVEHDR headers[12]{};
static std::vector<int16_t> audioBuffers[12];
static unsigned audioSlot=0;
typedef DWORD (WINAPI *GetPad)(DWORD,XINPUT_STATE*);
static GetPad getPad=nullptr;
static const char *actionNames[]={"Up","Down","Left","Right","Jump","Whip / weapon","Pause","Rejoin"};
static const char *actionIds[]={"up","down","left","right","jump","whip","pause","rejoin"};
static const unsigned retroIds[]={4,5,6,7,8,0,3,10};
static int keys[2][8]={{'W','S','A','D',VK_SPACE,'F',VK_RETURN,'Q'},{VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,'L','K',0,'O'}};
static unsigned gamepadKeys[2][8]={{1,2,4,8,XINPUT_GAMEPAD_A,XINPUT_GAMEPAD_X,XINPUT_GAMEPAD_START,XINPUT_GAMEPAD_Y},
                                 {1,2,4,8,XINPUT_GAMEPAD_A,XINPUT_GAMEPAD_X,XINPUT_GAMEPAD_START,XINPUT_GAMEPAD_Y}};
static int capturePlayer=-1,captureAction=-1,captureKind=0;
static unsigned previousPad[2]{};
static void message(const std::string& s){notice=s;noticeUntil=GetTickCount()+3500;}
static void snapshotWindow(HWND window,const char*path){
 RECT r;GetClientRect(window,&r);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=r.right;info.bmiHeader.biHeight=-r.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
 void*data=nullptr;HDC dc=CreateCompatibleDC(nullptr);HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&data,nullptr,0);HGDIOBJ old=SelectObject(dc,bitmap);
 SendMessage(window,WM_PRINT,(WPARAM)dc,PRF_CLIENT|PRF_CHILDREN|PRF_ERASEBKGND);
 BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);header.bfSize=header.bfOffBits+r.right*r.bottom*4;
 FILE*f=fopen(path,"wb");if(f){fwrite(&header,sizeof(header),1,f);fwrite(&info.bmiHeader,sizeof(info.bmiHeader),1,f);fwrite(data,4,r.right*r.bottom,f);fclose(f);}
 SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
}
static bool keyDown(int key){return key && (GetAsyncKeyState(key)&0x8000);}
static std::string keyName(int key){
 if(!key)return "Unbound";
 char name[64]{};unsigned scan=MapVirtualKeyA(key,MAPVK_VK_TO_VSC);
 if(key>=VK_PRIOR&&key<=VK_DOWN)scan|=0x100;
 if(GetKeyNameTextA(scan<<16,name,sizeof(name)))return name;
 return std::to_string(key);
}
static std::string padName(unsigned button){
 const unsigned ids[]={1,2,4,8,16,32,64,128,256,512,4096,8192,16384,32768};
 const char *names[]={"D-pad Up","D-pad Down","D-pad Left","D-pad Right","Start","Back","Left stick","Right stick","LB","RB","A","B","X","Y"};
 for(unsigned i=0;i<14;i++)if(button==ids[i])return names[i];return "Unbound";
}
static void loadBindings(){
 for(int p=0;p<2;p++)for(int a=0;a<8;a++){
  std::string section="Player"+std::to_string(p+1);
  keys[p][a]=GetPrivateProfileIntA(section.c_str(),actionIds[a],keys[p][a],bindingPath());
  section+="Gamepad";
  gamepadKeys[p][a]=GetPrivateProfileIntA(section.c_str(),actionIds[a],gamepadKeys[p][a],bindingPath());
 }
}
static void saveBinding(){
 std::string section="Player"+std::to_string(capturePlayer+1)+(captureKind?"Gamepad":"");
 unsigned value=captureKind?gamepadKeys[capturePlayer][captureAction]:keys[capturePlayer][captureAction];
 WritePrivateProfileStringA(section.c_str(),actionIds[captureAction],std::to_string(value).c_str(),bindingPath());
 SetWindowTextA(GetDlgItem(controlsWindow,100+capturePlayer*100+captureKind*20+captureAction),
                (captureKind?padName(value):keyName(value)).c_str());
 capturePlayer=captureAction=-1;
 SetWindowTextA(GetDlgItem(controlsWindow,1),"Click a binding, then press a key or a button on that player's controller. Esc cancels.");
}
static LRESULT CALLBACK controlsProc(HWND w,UINT m,WPARAM a,LPARAM b){
 switch(m){
 case WM_CREATE:
  CreateWindowA("STATIC","Click a binding, then press a key or a button on that player's controller. Esc cancels.",WS_CHILD|WS_VISIBLE,12,12,756,30,w,(HMENU)1,nullptr,nullptr);
  for(int p=0;p<2;p++){
   int x=12+p*384;
   CreateWindowA("STATIC",p?"PLAYER 2      Keyboard          Gamepad":"PLAYER 1      Keyboard          Gamepad",WS_CHILD|WS_VISIBLE,x,48,365,24,w,nullptr,nullptr,nullptr);
   for(int n=0;n<8;n++){
    CreateWindowA("STATIC",actionNames[n],WS_CHILD|WS_VISIBLE,x,83+n*31,137,25,w,nullptr,nullptr,nullptr);
    CreateWindowA("BUTTON",keyName(keys[p][n]).c_str(),WS_CHILD|WS_VISIBLE,x+138,78+n*31,103,27,w,(HMENU)(INT_PTR)(100+p*100+n),nullptr,nullptr);
    CreateWindowA("BUTTON",padName(gamepadKeys[p][n]).c_str(),WS_CHILD|WS_VISIBLE,x+246,78+n*31,119,27,w,(HMENU)(INT_PTR)(120+p*100+n),nullptr,nullptr);
   }
  }
  CreateWindowA("STATIC","Saved to controls.ini. While paused: gamepad A+B opens cheats; keyboard A+B+C also works.",WS_CHILD|WS_VISIBLE,12,335,750,38,w,nullptr,nullptr,nullptr);
  EnumChildWindows(w,[](HWND child,LPARAM)->BOOL{SendMessage(child,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);return TRUE;},0);
  SetTimer(w,1,20,nullptr);return 0;
 case WM_COMMAND:{
  int id=LOWORD(a);if(id<100||id>=229)break;
  capturePlayer=(id-100)/100;int action=(id-100)%100;captureKind=action>=20;captureAction=action%20;
  if(captureAction>=8||capturePlayer>1){capturePlayer=-1;break;}
  SetWindowTextA(GetDlgItem(w,1),captureKind?"Press the desired button on this player's controller. Esc cancels.":"Press the desired keyboard key. Delete clears the binding. Esc cancels.");
  SetFocus(w);return 0;}
 case WM_KEYDOWN:
  if(a==VK_ESCAPE){capturePlayer=-1;SetWindowTextA(GetDlgItem(w,1),"Binding cancelled. Close this window to return to the game.");return 0;}
  if(a==VK_F1||a==VK_F2||a==VK_F5||a==VK_F8){SetWindowTextA(GetDlgItem(w,1),"F1, F2, F5 and F8 are reserved. Press another key.");return 0;}
  if(capturePlayer>=0&&!captureKind){keys[capturePlayer][captureAction]=a==VK_DELETE?0:(int)a;saveBinding();}return 0;
 case WM_TIMER:
  if(getPad)for(int p=0;p<2;p++){
   XINPUT_STATE s{};unsigned buttons=getPad(p,&s)==ERROR_SUCCESS?s.Gamepad.wButtons:0;
   unsigned edge=buttons&~previousPad[p];previousPad[p]=buttons;
   if(capturePlayer==p&&captureKind&&edge){gamepadKeys[p][captureAction]=edge&(~edge+1);saveBinding();}
  }return 0;
 case WM_CLOSE:DestroyWindow(w);return 0;
 case WM_DESTROY:KillTimer(w,1);controlsWindow=nullptr;capturePlayer=-1;EnableWindow(windowHandle,TRUE);SetForegroundWindow(windowHandle);return 0;
 }return DefWindowProcA(w,m,a,b);
}
static void openControls(){
 if(controlsWindow){SetForegroundWindow(controlsWindow);return;}
 if(audioDevice)waveOutReset(audioDevice);
 controlsWindow=CreateWindowA("IndianaControls","Indiana Co-op - Controls",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|(testFrames?0:WS_VISIBLE),
 CW_USEDEFAULT,CW_USEDEFAULT,790,446,windowHandle,nullptr,GetModuleHandle(nullptr),nullptr);
 EnableWindow(windowHandle,FALSE);
}
static std::vector<unsigned char> readFile(const char *path){
 FILE *f=fopen(path,"rb");if(!f)return {};fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
 if(n<=0||n>4*1024*1024){fclose(f);return {};}
 std::vector<unsigned char> v(n);bool ok=fread(v.data(),1,v.size(),f)==v.size();fclose(f);return ok?v:std::vector<unsigned char>{};
}
static bool correctRom(const std::vector<unsigned char>& data){
 if(data.size()!=262160)return false;
 BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;unsigned char digest[32];DWORD n=0,returned=0;
 if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
 NTSTATUS result=BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,(PUCHAR)&n,sizeof(n),&returned,0);
 std::vector<unsigned char> object(n);
 if(result>=0)result=BCryptCreateHash(alg,&hash,object.data(),n,nullptr,0,0);
 if(result>=0)result=BCryptHashData(hash,(PUCHAR)data.data(),(ULONG)data.size(),0);
 if(result>=0)result=BCryptFinishHash(hash,digest,32,0);
 if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(alg,0);if(result<0)return false;
 char hex[65];for(int i=0;i<32;i++)sprintf(hex+2*i,"%02x",digest[i]);
 return !strcmp(hex,"a8cec2954f957a88c1628aa6f6cc929babacc38fafa9a509a1ea0e253131d3f3");
}
static bool saveSlot(){
 CreateDirectoryA("saves",nullptr);
 std::string path=(testFrames?"diagnostics\\native-test-slot-":"saves\\slot-")+std::to_string(slot)+".ijstate",temp=path+".tmp";
 std::vector<unsigned char> data(core.serialize_size());if(!core.serialize(data.data(),data.size())){message("Save failed");return false;}
 FILE *f=fopen(temp.c_str(),"wb");if(!f){message("Cannot write save file");return false;}
 bool ok=fwrite("IJCOOP2\0",1,8,f)==8&&fwrite(data.data(),1,data.size(),f)==data.size();ok=fclose(f)==0&&ok;
 if(ok)ok=MoveFileExA(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
 message(ok?"Saved slot "+std::to_string(slot):"Save failed");return ok;
}
static bool loadSlot(){
 std::string path=(testFrames?"diagnostics\\native-test-slot-":"saves\\slot-")+std::to_string(slot)+".ijstate";auto data=readFile(path.c_str());
 bool ok=data.size()>8&&!memcmp(data.data(),"IJCOOP2\0",8)&&core.unserialize(data.data()+8,data.size()-8);
 if(ok){core.status(state);cheatOpen=false;releaseActions=true;previousActions=0;if(audioDevice)waveOutReset(audioDevice);}
 message(ok?"Loaded slot "+std::to_string(slot):"No compatible save in slot "+std::to_string(slot));return ok;
}
static bool environment(unsigned cmd,void *data){
 switch(cmd){
 case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:*(const char**)data=".";return true;
 case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:format=*(unsigned*)data;return format==RETRO_PIXEL_FORMAT_XRGB8888;
 case RETRO_ENVIRONMENT_GET_VARIABLE:{auto*v=(retro_variable*)data;auto i=options.find(v->key);v->value=i==options.end()?nullptr:i->second.c_str();return v->value!=nullptr;}
 case RETRO_ENVIRONMENT_SET_VARIABLES:
  for(auto*v=(retro_variable*)data;v->key;v++){std::string s=v->value;auto n=s.find(';');if(n==s.npos)continue;s=s.substr(n+1);while(!s.empty()&&s[0]==' ')s.erase(0,1);options.emplace(v->key,s.substr(0,s.find('|')));}return true;
 case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:*(bool*)data=false;return true;
 case RETRO_ENVIRONMENT_GET_CAN_DUPE:*(bool*)data=true;return true;
 case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:return true;
 case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:*(unsigned*)data=0;return true;
 case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:case RETRO_ENVIRONMENT_SET_GEOMETRY:return true;
 default:return false;
 }
}
static void video(const void *data,unsigned w,unsigned h,size_t pitch){
 if(!data)return;width=w;height=h;pixels.resize(w*h);for(unsigned y=0;y<h;y++)memcpy(pixels.data()+y*w,(const char*)data+y*pitch,w*4);
}
static size_t audioBatch(const int16_t*p,size_t n){sound.insert(sound.end(),p,p+n*2);return n;}
static void audioSample(int16_t l,int16_t r){sound.push_back(l);sound.push_back(r);}
static void inputPoll(){}
static int16_t inputState(unsigned p,unsigned d,unsigned i,unsigned b){if(p>1||d!=RETRO_DEVICE_JOYPAD)return 0;return b==RETRO_DEVICE_ID_JOYPAD_MASK?pads[p]:(pads[p]>>b)&1;}
static void readInputs(){
 pads[0]=pads[1]=0;
 bool focused=GetForegroundWindow()==windowHandle,gamepadChord=false;
 if(focused&&!testFrames)for(int p=0;p<2;p++){
  XINPUT_STATE s{};unsigned buttons=getPad&&getPad(p,&s)==ERROR_SUCCESS?s.Gamepad.wButtons:0;
  gamepadChord|=(buttons&(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_B))==(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_B);
  for(int a=0;a<8;a++)if(keyDown(keys[p][a])||(buttons&gamepadKeys[p][a]))pads[p]|=1u<<retroIds[a];
  if(s.Gamepad.sThumbLX< -14000)pads[p]|=1<<6;if(s.Gamepad.sThumbLX>14000)pads[p]|=1<<7;
  if(s.Gamepad.sThumbLY< -14000)pads[p]|=1<<5;if(s.Gamepad.sThumbLY>14000)pads[p]|=1<<4;
 }
 if(testFrames){
  if(!state[1])pads[0]=(frameNumber%24==1?8:0)|(frameNumber%12==1?256:0);
  else if(state[5]<120)pads[1]=128;
  if(testScenario&&state[5]>130&&testMenuStart<0)testMenuStart=frameNumber;
  if(testMenuStart>=0){int step=frameNumber-testMenuStart;pads[0]=pads[1]=0;
   if(step==0||step==37)pads[0]=8;
   if(step==14)pads[0]=0x101;
   if(step==17||step==21)pads[0]=256;
   if(step==19)pads[0]=32;
   if(step==25||step==27||step==29)pads[0]=32;
   if(step==31)pads[0]=256;
  }
 }
 // Either controller can operate the native P1 pause button.
 pads[0]|=pads[1]&8;
 unsigned actions=pads[0]|pads[1],edge=actions&~previousActions;previousActions=actions;
 bool chord=state[2]&&(gamepadChord||(focused&&keyDown('A')&&keyDown('B')&&keyDown('C'))||((pads[0]&0x101)==0x101)||((pads[1]&0x101)==0x101));
 bool opened=chord&&!wasChord;wasChord=chord;
 if(!state[2]){if(cheatOpen)releaseActions=true;cheatOpen=false;}
 else if(opened){cheatOpen=true;selectedCheat=0;core.level_current();}
 if(testMenuStart>=0&&frameNumber-testMenuStart==15)testMenuOpened=cheatOpen&&!state[7];
 if(testMenuStart>=0&&frameNumber-testMenuStart==33)testMenuApplied=state[7]&&state[9]==9&&core.noclip();
 if(cheatOpen){
  if(!opened){
   if(edge&16)selectedCheat=(selectedCheat+6)%7;if(edge&32)selectedCheat=(selectedCheat+1)%7;
   if(selectedCheat==6){if(edge&64)core.cheat(8);if(edge&128)core.cheat(7);}
   if(edge&256)core.cheat(selectedCheat);
  }
  pads[0]&=8;pads[1]=0;
 }
 unsigned rejoin=((pads[0]>>10)&1)|((pads[1]>>9)&2),rejoinEdge=rejoin&~previousRejoin;previousRejoin=rejoin;
 if(!state[2]){if(rejoinEdge&1)core.rejoin(1);if(rejoinEdge&2)core.rejoin(2);}
 if(releaseActions){if(!(actions&0x101))releaseActions=false;pads[0]&=~0x101;pads[1]&=~0x101;}
}
static void outputAudio(){
 if(!audioDevice||sound.empty())return;auto &h=headers[audioSlot];
 if(h.dwFlags&WHDR_PREPARED){if(!(h.dwFlags&WHDR_DONE)){audioDropped++;return;}waveOutUnprepareHeader(audioDevice,&h,sizeof(h));}
 audioBuffers[audioSlot]=sound;h={};h.lpData=(LPSTR)audioBuffers[audioSlot].data();h.dwBufferLength=(DWORD)sound.size()*2;
 waveOutPrepareHeader(audioDevice,&h,sizeof(h));waveOutWrite(audioDevice,&h,sizeof(h));audioSlot=(audioSlot+1)%12;
}
static void drawText(HDC dc,int x,int y,const std::string& s,COLORREF color=RGB(240,240,240)){
 SetTextColor(dc,color);TextOutA(dc,x,y,s.c_str(),(int)s.size());
}
static void paintFrame(HDC dc){
 RECT r;GetClientRect(windowHandle,&r);FillRect(dc,&r,(HBRUSH)GetStockObject(BLACK_BRUSH));SetBkMode(dc,TRANSPARENT);
 int scale=std::max(1,(int)std::min((r.right-16)/(int)width,(r.bottom-42)/(int)height));int w=width*scale,h=height*scale,x=(r.right-w)/2,y=(r.bottom-30-h)/2;
 if(!pixels.empty()){
  BITMAPINFO b{};b.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);b.bmiHeader.biWidth=width;b.bmiHeader.biHeight=-(int)height;b.bmiHeader.biPlanes=1;b.bmiHeader.biBitCount=32;
  SetStretchBltMode(dc,COLORONCOLOR);StretchDIBits(dc,x,y,w,h,0,0,width,height,pixels.data(),&b,DIB_RGB_COLORS,SRCCOPY);
 }
 if(help||(state[2]&&cheatOpen)){
  RECT box{r.right/2-220,r.bottom/2-150,r.right/2+220,r.bottom/2+150};HBRUSH bg=CreateSolidBrush(RGB(16,24,32));FillRect(dc,&box,bg);DeleteObject(bg);
  int tx=box.left+20,ty=box.top+16;
  if(help){
   drawText(dc,tx,ty,"INDIANA CO-OP - CONTROLS",RGB(100,225,255));
   for(int p=0;p<2;p++){
    drawText(dc,tx,ty+30+p*52,"P"+std::to_string(p+1)+": "+keyName(keys[p][0])+" / "+keyName(keys[p][1])+" / "+keyName(keys[p][2])+" / "+keyName(keys[p][3]));
    drawText(dc,tx,ty+52+p*52,"Jump: "+keyName(keys[p][4])+"    Whip: "+keyName(keys[p][5]));
   }
   drawText(dc,tx,ty+141,"F2: change buttons    F5/F8: save/load");drawText(dc,tx,ty+165,"Paused cheats: gamepad A+B / keyboard A+B+C");drawText(dc,tx,ty+186,"F1: close help");
  }else if(cheatOpen){
   drawText(dc,tx,ty,"CHEATS",RGB(100,225,255));
   const std::string items[]={std::string("Invincible: ")+(state[7]?"ON":"OFF"),"Set lives to 9","Give both players hats","Rejoin P2 to grounded P1",std::string("Noclip: ")+(core.noclip()?"ON":"OFF"),std::string("Mode: ")+(core.death_mode()?"Easy":"Classic"),"Level: "+std::to_string(core.level_choice()+1)};
   for(unsigned i=0;i<7;i++)drawText(dc,tx,ty+32+i*28,(i==selectedCheat?"> ":"  ")+items[i],i==selectedCheat?RGB(255,225,160):RGB(235,235,235));
   drawText(dc,tx,ty+238,"Arrows: choose / change level    Jump: apply");drawText(dc,tx,ty+262,"Pause: resume    Esc: close cheats");
  }
 }
 drawText(dc,12,r.bottom-24,GetTickCount()<noticeUntil?notice:"F1 Help   F2 Controls   F5 Save   F8 Load   Slot "+std::to_string(slot));
}
// Compose the entire frame off-screen, then present it in one operation.
static void paint(HDC target){
 struct Buffer {
  HDC dc=nullptr;HBITMAP bitmap=nullptr;HGDIOBJ original=nullptr;int w=0,h=0;
  ~Buffer(){if(dc){SelectObject(dc,original);DeleteObject(bitmap);DeleteDC(dc);}}
 };
 static Buffer buffer;
 RECT r;GetClientRect(windowHandle,&r);if(r.right<=0||r.bottom<=0)return;
 if(!buffer.dc){buffer.dc=CreateCompatibleDC(target);buffer.original=GetCurrentObject(buffer.dc,OBJ_BITMAP);}
 if(buffer.w!=r.right||buffer.h!=r.bottom){
  HBITMAP next=CreateCompatibleBitmap(target,r.right,r.bottom);if(!next)return;
  SelectObject(buffer.dc,next);if(buffer.bitmap)DeleteObject(buffer.bitmap);
  buffer.bitmap=next;buffer.w=r.right;buffer.h=r.bottom;
 }
 paintFrame(buffer.dc);BitBlt(target,0,0,r.right,r.bottom,buffer.dc,0,0,SRCCOPY);
}
static void fullscreen(){
 static bool full=false;static RECT old{};full=!full;
 if(full){GetWindowRect(windowHandle,&old);SetWindowLongPtr(windowHandle,GWL_STYLE,WS_POPUP|WS_VISIBLE);MONITORINFO mi{sizeof(mi)};GetMonitorInfo(MonitorFromWindow(windowHandle,MONITOR_DEFAULTTONEAREST),&mi);SetWindowPos(windowHandle,HWND_TOP,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right-mi.rcMonitor.left,mi.rcMonitor.bottom-mi.rcMonitor.top,SWP_FRAMECHANGED);}
 else{SetWindowLongPtr(windowHandle,GWL_STYLE,WS_OVERLAPPEDWINDOW|WS_VISIBLE);SetWindowPos(windowHandle,nullptr,old.left,old.top,old.right-old.left,old.bottom-old.top,SWP_FRAMECHANGED|SWP_NOZORDER);}
}
static LRESULT CALLBACK windowProc(HWND w,UINT m,WPARAM a,LPARAM b){
 switch(m){
 case WM_DESTROY:running=false;return 0;
 case WM_ERASEBKGND:return 1;
 case WM_SYSKEYDOWN:if(a==VK_RETURN){fullscreen();return 0;}break;
 case WM_KEYDOWN:
  if(b&(1L<<30))return 0;
  if(a==VK_F1)help=!help;if(a==VK_F2)openControls();if(a==VK_F5)saveSlot();if(a==VK_F8)loadSlot();
  if(a==VK_ESCAPE){cheatOpen=false;help=false;releaseActions=true;}return 0;
 case WM_COMMAND:
  if(a==1)openControls();else if(a==2)saveSlot();else if(a==3)loadSlot();else if(a==4)DestroyWindow(w);
  else if(a==5){core.reset();cheatOpen=false;message("Restarted cartridge");}
  else if(a==6)ShellExecuteA(w,"open","https://github.com/TASEmulators/BizHawk/releases",nullptr,nullptr,SW_SHOWNORMAL);
  else if(a>=101&&a<=110){slot=(a-100)%10;message("Selected slot "+std::to_string(slot));CheckMenuRadioItem(GetMenu(w),101,110,(UINT)a,MF_BYCOMMAND);}return 0;
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);paint(dc);EndPaint(w,&ps);return 0;}
 case WM_PRINTCLIENT:paint((HDC)a);return 0;
 }return DefWindowProcA(w,m,a,b);
}
static int fail(const char*s){fprintf(stderr,"%s\n",s);if(!testFrames)MessageBoxA(nullptr,s,"Indiana Co-op",MB_ICONERROR);return 1;}
int main(int argc,char**argv){
 char path[MAX_PATH];GetModuleFileNameA(nullptr,path,MAX_PATH);char*slash=strrchr(path,'\\');if(slash){*slash=0;SetCurrentDirectoryA(path);}
 for(int i=1;i<argc;i++){if(!strcmp(argv[i],"--test")&&i+1<argc)testFrames=atoi(argv[++i]);if(!strcmp(argv[i],"--save-test"))testScenario=true;if(!strcmp(argv[i],"--ui-test"))uiTest=true;}
 CreateDirectoryA("diagnostics",nullptr);loadBindings();
 auto rom=readFile("Young Indiana Jones Chronicles, The (USA).nes");if(!correctRom(rom))return fail("The supported Young Indiana Jones Chronicles USA ROM is missing or changed.");
 core.dll=LoadLibraryA("engine\\fceumm_libretro.dll");if(!core.dll)return fail("Cannot load engine\\fceumm_libretro.dll. Rebuild or restore the native core.");
#define LOAD(n) core.n=(decltype(core.n))GetProcAddress(core.dll,"retro_" #n);if(!core.n)return fail("Missing core export: " #n);
 LOAD(init);LOAD(run);LOAD(deinit);LOAD(unload_game);LOAD(reset);LOAD(set_environment);LOAD(set_video_refresh);LOAD(set_audio_sample);LOAD(set_audio_sample_batch);LOAD(set_input_poll);LOAD(set_input_state);LOAD(set_controller_port_device);LOAD(load_game);LOAD(get_system_av_info);LOAD(serialize_size);LOAD(serialize);LOAD(unserialize);
#undef LOAD
#define LOAD(n) core.n=(decltype(core.n))GetProcAddress(core.dll,"ij_" #n);if(!core.n)return fail("Missing co-op export: " #n);
 LOAD(status);LOAD(cheat);LOAD(rejoin);LOAD(noclip);LOAD(death_mode);LOAD(level_choice);LOAD(level_current);
#undef LOAD
 core.set_environment(environment);core.set_video_refresh(video);core.set_audio_sample(audioSample);core.set_audio_sample_batch(audioBatch);core.set_input_poll(inputPoll);core.set_input_state(inputState);core.init();
 retro_game_info game{"Young Indiana Jones Chronicles, The (USA).nes",rom.data(),rom.size(),nullptr};if(!core.load_game(&game))return fail("Cartridge load failed.");
 for(int p=0;p<2;p++)core.set_controller_port_device(p,RETRO_DEVICE_JOYPAD);
 core.status(state);if(!state[0])return fail("The co-op core did not recognize this cartridge.");
 retro_system_av_info av{};core.get_system_av_info(&av);
 if(!testFrames||uiTest){
  SetProcessDPIAware();WNDCLASSA wc{};wc.hInstance=GetModuleHandle(nullptr);wc.lpfnWndProc=windowProc;wc.lpszClassName="IndianaCoop";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassA(&wc);wc.lpfnWndProc=controlsProc;wc.lpszClassName="IndianaControls";wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);RegisterClassA(&wc);
  HMENU menu=CreateMenu(),gameMenu=CreatePopupMenu(),saveMenu=CreatePopupMenu();
  AppendMenuA(gameMenu,MF_STRING,1,"Controls...\tF2");AppendMenuA(gameMenu,MF_STRING,2,"Save state\tF5");AppendMenuA(gameMenu,MF_STRING,3,"Load state\tF8");AppendMenuA(gameMenu,MF_STRING,5,"Restart cartridge");AppendMenuA(gameMenu,MF_STRING,4,"Exit");
  for(int s=1;s<=10;s++)AppendMenuA(saveMenu,MF_STRING|(s==1?MF_CHECKED:0),100+s,("Slot "+std::to_string(s%10)).c_str());
  AppendMenuA(menu,MF_POPUP,(UINT_PTR)gameMenu,"Game");AppendMenuA(menu,MF_POPUP,(UINT_PTR)saveMenu,"Save slot");
  HMENU helpMenu=CreatePopupMenu();AppendMenuA(helpMenu,MF_STRING,6,"Download BizHawk...");AppendMenuA(menu,MF_POPUP,(UINT_PTR)helpMenu,"Help");
  windowHandle=CreateWindowA("IndianaCoop","Indiana Jones - Native Co-op",WS_OVERLAPPEDWINDOW|(testFrames?0:WS_VISIBLE),CW_USEDEFAULT,CW_USEDEFAULT,810,930,nullptr,menu,GetModuleHandle(nullptr),nullptr);
  HMODULE xi=LoadLibraryA("xinput1_4.dll");if(!xi)xi=LoadLibraryA("xinput9_1_0.dll");if(xi)getPad=(GetPad)GetProcAddress(xi,"XInputGetState");
  WAVEFORMATEX wf{};wf.wFormatTag=WAVE_FORMAT_PCM;wf.nChannels=2;wf.nSamplesPerSec=(DWORD)av.timing.sample_rate;wf.wBitsPerSample=16;wf.nBlockAlign=4;wf.nAvgBytesPerSec=wf.nSamplesPerSec*4;
  if(!testFrames){if(waveOutOpen(&audioDevice,WAVE_MAPPER,&wf,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR)message("Audio device unavailable");timeBeginPeriod(1);}
  if(uiTest){
   openControls();SendMessage(controlsWindow,WM_COMMAND,204,0);SendMessage(controlsWindow,WM_KEYDOWN,'R',0);
   testBindings=keys[1][4]=='R'&&GetPrivateProfileIntA("Player2","jump",0,bindingPath())=='R';
   snapshotWindow(controlsWindow,"diagnostics/native-controls.bmp");
   SendMessage(controlsWindow,WM_CLOSE,0,0);
  }
 }
 LARGE_INTEGER freq,now;QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&now);double deadline=double(now.QuadPart)/freq.QuadPart;
 bool saved=false,loaded=false;
 while(running&&(!testFrames||frameNumber<testFrames)){
  MSG msg;while(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessage(&msg);}if(!running)break;
  if(controlsWindow){Sleep(15);QueryPerformanceCounter(&now);deadline=double(now.QuadPart)/freq.QuadPart;continue;}
  readInputs();sound.clear();core.run();core.status(state);
  if(uiTest&&testMenuStart>=0&&frameNumber-testMenuStart==33)snapshotWindow(windowHandle,"diagnostics/native-menu.bmp");
  if(uiTest&&testMenuStart>=0&&frameNumber-testMenuStart==10)snapshotWindow(windowHandle,"diagnostics/native-pause.bmp");
  if(frameNumber==0&&!testFrames){const char*session=getenv("INDIANA_COOP_SESSION");if(session){FILE*f=fopen("diagnostics/native-ready.txt","w");if(f){fputs(session,f);fclose(f);}}}
  if(testScenario&&state[5]>60&&!saved){slot=0;saved=saveSlot();}
  if(testScenario&&state[5]>100&&!loaded){loaded=loadSlot();}
  if(!testFrames){outputAudio();InvalidateRect(windowHandle,nullptr,FALSE);UpdateWindow(windowHandle);
   deadline+=1.0/av.timing.fps;QueryPerformanceCounter(&now);double remaining=deadline-double(now.QuadPart)/freq.QuadPart;
   if(remaining>0)Sleep((DWORD)(remaining*1000));else if(remaining< -0.2)deadline=double(now.QuadPart)/freq.QuadPart;
  }frameNumber++;
 }
 if(testFrames){
  FILE*f=fopen("diagnostics/native-launcher.txt","w");if(f){fprintf(f,"frames=%d passes=%u stage=%u p2=%u save=%d load=%d menu=%d cheats=%d bindings=%d audio=%zu\n",frameNumber,state[5],state[8],state[6],saved,loaded,testMenuOpened,testMenuApplied,testBindings,sound.size());fclose(f);}
  BITMAPFILEHEADER bf{};BITMAPINFOHEADER bi{};bi.biSize=sizeof(bi);bi.biWidth=width;bi.biHeight=-(int)height;bi.biPlanes=1;bi.biBitCount=32;bi.biSizeImage=(DWORD)pixels.size()*4;bf.bfType=0x4d42;bf.bfOffBits=sizeof(bf)+sizeof(bi);bf.bfSize=bf.bfOffBits+bi.biSizeImage;
  f=fopen("diagnostics/native-launcher.bmp","wb");if(f){fwrite(&bf,sizeof(bf),1,f);fwrite(&bi,sizeof(bi),1,f);fwrite(pixels.data(),4,pixels.size(),f);fclose(f);}
 }
 if(audioDevice){waveOutReset(audioDevice);for(auto&h:headers)if(h.dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(audioDevice,&h,sizeof(h));waveOutClose(audioDevice);}
 if(!testFrames)timeEndPeriod(1);core.unload_game();core.deinit();FreeLibrary(core.dll);
 return testFrames&&(!state[6]||(testScenario&&(!saved||!loaded||!testMenuOpened||!testMenuApplied))||(uiTest&&!testBindings))?1:0;
}
