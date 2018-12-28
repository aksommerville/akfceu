#include "akfceu_video.h"
#include "akfceu_input_map.h"
#include "fceu/driver.h"
#include "fceu/fceu.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#if USE_macos
  #include "opt/macos/mn_machid.h"
  #include "opt/macos/mn_macaudio.h"
  #include "opt/macos/mn_macwm.h"
  #include "opt/macos/mn_macioc.h"
#else
  #error "The interface layer is currently only for MacOS. Set 'USE_macos=1' or write a new interface."
#endif

/* Globals.
 */

static uint8_t framebuffer[256*240]={0};
static uint8_t palette[768]={0};
static int vmrunning=0;
static uint32_t input_state=0;

/* FCEUD hooks.
 */

void FCEUD_SetPalette(uint8 index, uint8 r, uint8 g, uint8 b) {
  int p=index*3;
  palette[p++]=r;
  palette[p++]=g;
  palette[p]=b;
}

void FCEUD_GetPalette(uint8 i,uint8 *r, uint8 *g, uint8 *b) {
  int p=i*3;
  *r=palette[p++];
  *g=palette[p++];
  *b=palette[p];
}

/* Input callbacks (MacHID).
 */

static int connect_machid(int devid) {
  printf("machid connect %d\n",devid);
  int vid=mn_machid_dev_get_vendor_id(devid);
  int pid=mn_machid_dev_get_product_id(devid);
  struct akfceu_input_map *map=akfceu_input_map_find(vid,pid);
  if (map) {
    printf("...found map for %04x:%04x\n",vid,pid);
  } else {
    printf("No input map for %04x:%04x. Dumping details...\n",vid,pid);
    int btnix=0,srcbtnid,usage,lo,hi,value;
    for (;mn_machid_dev_get_button_info(&srcbtnid,&usage,&lo,&hi,&value,devid,btnix)>=0;btnix++) {
      printf("  %3d: id=0x%08x usage=0x%08x range=(%d..%d) value=%d\n",btnix,srcbtnid,usage,lo,hi,value);
    }
  }
  return 0;
}

static int disconnect_machid(int devid) {
  printf("machid disconnect %d\n",devid);
  return 0;
}

static int button_machid_digested(int btnid,int value,void *userdata) {
  int devid=*(int*)userdata;
  if (!btnid) return 0;
  int mask=btnid<<((devid&1)?0:8);
  if (value) {
    input_state|=mask;
  } else {
    input_state&=~mask;
  }
  return 0;
}

static int button_machid(int devid,int srcbtnid,int value) {
  //printf("machid button %d.%d=%d\n",devid,srcbtnid,value);
  int vid=mn_machid_dev_get_vendor_id(devid);
  int pid=mn_machid_dev_get_product_id(devid);
  const struct akfceu_input_map *map=akfceu_input_map_find(vid,pid);
  if (!map) return 0;
  if (akfceu_input_map_update(map,srcbtnid,value,button_machid_digested,&devid)<0) return -1;
  return 0;
}

/* Window manager callbacks.
 */

static int key_macwm(int keycode,int value) {
  //printf("key %d=%d\n",keycode,value);
  // Static key mapping, all assigned to player one:
  switch (keycode) {
    case 0x00070052: if (value) input_state|=JOY_UP; else input_state&=~JOY_UP; break; // up
    case 0x00070051: if (value) input_state|=JOY_DOWN; else input_state&=~JOY_DOWN; break; // down
    case 0x00070050: if (value) input_state|=JOY_LEFT; else input_state&=~JOY_LEFT; break; // left
    case 0x0007004f: if (value) input_state|=JOY_RIGHT; else input_state&=~JOY_RIGHT; break; // right
    case 0x0007001d: if (value) input_state|=JOY_A; else input_state&=~JOY_A; break; // z
    case 0x0007001b: if (value) input_state|=JOY_B; else input_state&=~JOY_B; break; // x
    case 0x0007002b: if (value) input_state|=JOY_SELECT; else input_state&=~JOY_SELECT; break; // tab
    case 0x00070028: if (value) input_state|=JOY_START; else input_state&=~JOY_START; break; // enter
  }
  return 0;
}

static int resize_macwm(int w,int h) {
  if (akfceu_video_set_output_size(w,h)<0) return -1;
  return 0;
}

/* Load a fresh ROM file and reset the emulator.
 */

static int load_rom_file(const char *path) {
  printf("Load ROM: %s\n",path);

  if (vmrunning) {
    FCEUI_CloseGame();
    vmrunning=0;
  }

  FCEUGI *fceugi=FCEUI_LoadGame(path);
  if (!fceugi) {
    printf("%s: FCEUI_LoadGame failed\n",path);
    return -1;
  }

  /* From fceu/drivers/pc/main.c:
  CurGame=tmp;
  ParseGI(tmp);
  RefreshThrottleFPS();
  if (!DriverInitialize(tmp)) return -1;
  FCEUD_NetworkConnect();
  */

  vmrunning=1;
  printf("Loaded.\n");

  FCEUI_PowerNES();
  printf("Power on.\n");

  FCEUI_SetInput(0,SI_GAMEPAD,&input_state,0);
  FCEUI_SetInput(1,SI_GAMEPAD,&input_state,0);
 
  return 0;
}

/* Audio callback.
 */

// At 44100 Hz, about 734 samples are emitted per update (ie per video frame).
#define AUDIO_BUFFER_SIZE 2048
static int16_t audiobuf[AUDIO_BUFFER_SIZE]={0};
static int audiorp=0;
static int audiowp=0;

static void cb_audio(int16_t *dst,int dstc) {
  while (dstc>0) {
    int cpc=AUDIO_BUFFER_SIZE-audiorp;
    if (cpc>dstc) cpc=dstc;
    memcpy(dst,audiobuf+audiorp,cpc<<1);
    dst+=cpc;
    dstc-=cpc;
    audiorp+=cpc;
    if (audiorp>=AUDIO_BUFFER_SIZE) audiorp=0;
  }
}

static void nix_audio_output() {
  memset(audiobuf,0,sizeof(audiobuf));
}

static int count_available_audio_frames() {
  if (audiowp<=audiorp) return audiorp-audiowp;
  int rp=audiorp+AUDIO_BUFFER_SIZE;
  return rp-audiowp;
}

// It's kind of dirty, but this is where our main timing happens: We let the audio backend drive it.
static int sleep_until_audio_available(int srcc) {
  int panic=100;
  while (panic-->0) {
    if (mn_macaudio_lock()<0) return -1;
    int available=count_available_audio_frames();
    int ok=(available>=srcc);
    if (mn_macaudio_unlock()<0) return -1;
    if (ok) return 0;
    usleep(1000);
  }
  printf("Audio timing panic.\n");
  return -1;
}

static int receive_audio_from_vm(const int32_t *src,int srcc) {
  if (sleep_until_audio_available(srcc)<0) return -1;
  for (;srcc-->0;src++) {
    audiobuf[audiowp++]=*src;
    if (audiowp>=AUDIO_BUFFER_SIZE) audiowp=0;
  }
  return 0;
}

/* Fill framebuffer and palette with some data to ensure everything's hooked up right.
 */

static void make_default_palette() {
  FCEUD_SetPalette(0x00,0x00,0x00,0x00);
  FCEUD_SetPalette(0x01,0xff,0xff,0xff);
  FCEUD_SetPalette(0x02,0xff,0x00,0x00);
  FCEUD_SetPalette(0x03,0x00,0xff,0x00);
  FCEUD_SetPalette(0x04,0x00,0x00,0xff);
  FCEUD_SetPalette(0x05,0xff,0xff,0x00);
  FCEUD_SetPalette(0x06,0xff,0x00,0xff);
  FCEUD_SetPalette(0x07,0x00,0xff,0xff);
  FCEUD_SetPalette(0x08,0xff,0x80,0x00);
  FCEUD_SetPalette(0x09,0xff,0x00,0x80);
  FCEUD_SetPalette(0x0a,0x80,0xff,0x00);
  FCEUD_SetPalette(0x0b,0x00,0xff,0x80);
  FCEUD_SetPalette(0x0c,0x80,0x00,0xff);
  FCEUD_SetPalette(0x0d,0x00,0x80,0xff);
  FCEUD_SetPalette(0x0e,0xff,0x80,0x80);
  FCEUD_SetPalette(0x0f,0x80,0x80,0xff);
  int i=16; for (;i<256;i++) {
    FCEUD_SetPalette(i,i,i,i);
  }
}

static void draw_test_pattern() {
  int x=0,y=0,p=0;
  for (;y<240;y++) {
    for (x=0;x<256;x++,p++) {
      if ((y<16)||(y>=224)) {
        framebuffer[p]=x>>4;
      } else if ((x==0)&&!(y&1)) {
        framebuffer[p]=0x02;
      } else if ((x==255)&&(y&1)) {
        framebuffer[p]=0x04;
      } else {
        int sum=(y-16)+x;
        int index=16+(sum*240)/464;
        framebuffer[p]=index;
      }
    }
  }
  // See script etc/readpixels.py:
  framebuffer[29263]=0xff;
  framebuffer[29269]=0xff;
  framebuffer[29281]=0xff;
  framebuffer[29282]=0xff;
  framebuffer[29283]=0xff;
  framebuffer[29287]=0xff;
  framebuffer[29288]=0xff;
  framebuffer[29291]=0xff;
  framebuffer[29297]=0xff;
  framebuffer[29519]=0xff;
  framebuffer[29520]=0xff;
  framebuffer[29525]=0xff;
  framebuffer[29537]=0xff;
  framebuffer[29540]=0xff;
  framebuffer[29542]=0xff;
  framebuffer[29545]=0xff;
  framebuffer[29547]=0xff;
  framebuffer[29548]=0xff;
  framebuffer[29552]=0xff;
  framebuffer[29553]=0xff;
  framebuffer[29775]=0xff;
  framebuffer[29777]=0xff;
  framebuffer[29781]=0xff;
  framebuffer[29793]=0xff;
  framebuffer[29796]=0xff;
  framebuffer[29798]=0xff;
  framebuffer[29801]=0xff;
  framebuffer[29803]=0xff;
  framebuffer[29805]=0xff;
  framebuffer[29807]=0xff;
  framebuffer[29809]=0xff;
  framebuffer[30031]=0xff;
  framebuffer[30034]=0xff;
  framebuffer[30037]=0xff;
  framebuffer[30040]=0xff;
  framebuffer[30041]=0xff;
  framebuffer[30049]=0xff;
  framebuffer[30050]=0xff;
  framebuffer[30051]=0xff;
  framebuffer[30054]=0xff;
  framebuffer[30057]=0xff;
  framebuffer[30059]=0xff;
  framebuffer[30062]=0xff;
  framebuffer[30065]=0xff;
  framebuffer[30287]=0xff;
  framebuffer[30291]=0xff;
  framebuffer[30293]=0xff;
  framebuffer[30295]=0xff;
  framebuffer[30298]=0xff;
  framebuffer[30305]=0xff;
  framebuffer[30308]=0xff;
  framebuffer[30310]=0xff;
  framebuffer[30313]=0xff;
  framebuffer[30315]=0xff;
  framebuffer[30321]=0xff;
  framebuffer[30543]=0xff;
  framebuffer[30548]=0xff;
  framebuffer[30549]=0xff;
  framebuffer[30551]=0xff;
  framebuffer[30554]=0xff;
  framebuffer[30561]=0xff;
  framebuffer[30564]=0xff;
  framebuffer[30566]=0xff;
  framebuffer[30569]=0xff;
  framebuffer[30571]=0xff;
  framebuffer[30577]=0xff;
  framebuffer[30799]=0xff;
  framebuffer[30805]=0xff;
  framebuffer[30808]=0xff;
  framebuffer[30809]=0xff;
  framebuffer[30817]=0xff;
  framebuffer[30820]=0xff;
  framebuffer[30823]=0xff;
  framebuffer[30824]=0xff;
  framebuffer[30827]=0xff;
  framebuffer[30833]=0xff;
}

/* Find the directory for storing FCEU artifacts (saves, etc), and register it with FCEU.
 */

static int determine_and_register_home_directory() {

  /* 1: Allow the user to set it explicitly via FCEU_HOME.
   */
  const char *FCEU_HOME=getenv("FCEU_HOME");
  if (FCEU_HOME&&FCEU_HOME[0]) {
    FCEUI_SetBaseDirectory((char*)FCEU_HOME);
    return 0;
  }

  /* 2: If there is a HOME in the environment, append "/.fceultra" to it.
   */
  const char *HOME=getenv("HOME");
  if (HOME&&HOME[0]) {
    char path[1024];
    int pathc=snprintf(path,sizeof(path),"%s/.fceultra",HOME);
    if ((pathc>0)&&(pathc<sizeof(path))) {
      FCEUI_SetBaseDirectory(path);
      return 0;
    }
  }

  /* 3: If there is a USER in the environment, use "/home/$USER/.fceultra".
   */
  const char *USER=getenv("USER");
  if (USER&&USER[0]) {
    char path[1024];
    int pathc=snprintf(path,sizeof(path),"/home/%s/.fceultra",USER);
    if ((pathc>0)&&(pathc<sizeof(path))) {
      FCEUI_SetBaseDirectory(path);
      return 0;
    }
  }

  /* 4: Oh whatever, use the working directory.
   * This is almost certainly not what anyone wants.
   */
  FCEUI_SetBaseDirectory(".");
  return 0;
}

/* Initialize.
 */

static int init(const char *rompath) {

  if (akfceu_define_andys_devices()<0) {
    return -1;
  }

  int winw=281*3;
  int winh=228*3;

  struct mn_macwm_delegate macwm_delegate={
    .key=key_macwm,
    .resize=resize_macwm,
    .receive_dragged_file=load_rom_file,
  };
  if (mn_macwm_init(winw,winh,0,"akfceu",&macwm_delegate)<0) {
    return -1;
  }
  mn_macwm_show_cursor(0);

  if (akfceu_video_init()<0) {
    return -1;
  }
  if (
    (akfceu_video_set_output_size(winw,winh)<0)||
    (akfceu_video_set_input_size(256,228)<0) // Dropping 11 rows from the top and 1 from the bottom
  ) {
    return -1;
  }

  struct mn_machid_delegate machid_delegate={
    .connect=connect_machid,
    .disconnect=disconnect_machid,
    .button=button_machid,
  };
  if (mn_machid_init(&machid_delegate)<0) {
    return -1;
  }

  if (mn_macaudio_init(22050,1,cb_audio)<0) {
    return -1;
  }

  if (determine_and_register_home_directory()<0) {
    return -1;
  }

  if (!FCEUI_Initialize()) {
    return -1;
  }

  // Before or after FCEUI_Initialize()?
  FCEUI_SetSoundVolume(100);
  FCEUI_SetSoundQuality(0);
  FCEUI_Sound(22050);
  FCEUI_DisableSpriteLimitation(1);

  if (rompath&&rompath[0]) {
    if (load_rom_file(rompath)<0) return -1;
  } else {
    make_default_palette();
    draw_test_pattern();
  }

  return 0;
}

/* Quit.
 */

static void quit() {
  if (vmrunning) {
    FCEUI_Sound(0);
    FCEUI_CloseGame();
    vmrunning=0;
  }
  akfceu_video_quit();
  mn_macaudio_quit();
  mn_machid_quit();
  mn_macwm_quit();
}

/* Update.
 */

static int update() {
  if (mn_machid_update()<0) {
    return -1;
  }

  uint8_t *real_framebuffer=framebuffer;
  if (vmrunning) {
    uint8 *vmfb=0;
    int32 *vmab=0;
    int32 vmabc=0;
    FCEUI_Emulate(&vmfb,&vmab,&vmabc,0);
    if (vmfb) real_framebuffer=vmfb;
    if (vmabc) {
      if (receive_audio_from_vm(vmab,vmabc)<0) return -1;
    } else {
      usleep(15000);
    }
   
  } else {
    // With no VM running, no one is minding the timing.
    nix_audio_output();
    usleep(16000);
  }

  if (akfceu_video_render(real_framebuffer+256*11,palette)<0) {
    return -1;
  }
 
  if (mn_macwm_swap()<0) {
    return -1;
  }
  return 0;
}

/* Main entry point.
 */

int main(int argc,char **argv) {
  struct mn_ioc_delegate delegate={
    .init=init,
    .quit=quit,
    .update=update,
    .open_file=load_rom_file,
  };
  return mn_macioc_main(argc,argv,&delegate);
}
