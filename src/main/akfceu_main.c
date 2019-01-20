#include "akfceu_video.h"
#include "akfceu_input_map.h"
#include "akfceu_input.h"
#include "fceu/driver.h"
#include "fceu/fceu.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#if USE_macos
  #include "opt/macos/mn_macaudio.h"
  #include "opt/macos/mn_macwm.h"
  #include "opt/macos/mn_macioc.h"
#else
  #error "The interface layer is currently only for MacOS. Set 'USE_macos=1' or write a new interface."
#endif

#if USE_romassist
  #include "opt/romassist/ra_client.h"
  static romassist_t romassist=0;
#endif

/* Globals.
 */

static uint8_t framebuffer[256*240]={0};
static uint8_t palette[768]={0};
static int vmrunning=0;
static char *current_rom_path=0;

/* FCEUD hooks.
 */

void FCEUD_SetPalette(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
  int p=index*3;
  palette[p++]=r;
  palette[p++]=g;
  palette[p]=b;
}

void FCEUD_GetPalette(uint8_t i,uint8_t *r, uint8_t *g, uint8_t *b) {
  int p=i*3;
  *r=palette[p++];
  *g=palette[p++];
  *b=palette[p];
}

/* Window manager callbacks.
 */

static int key_macwm(int keycode,int value) {
  //printf("key %d=%d\n",keycode,value);
  // Static key mapping, all assigned to player one:
  switch (keycode) {
  #if 0//XXX must manage via akfceu_input
    case 0x00070052: if (value) input_state|=JOY_UP; else input_state&=~JOY_UP; break; // up
    case 0x00070051: if (value) input_state|=JOY_DOWN; else input_state&=~JOY_DOWN; break; // down
    case 0x00070050: if (value) input_state|=JOY_LEFT; else input_state&=~JOY_LEFT; break; // left
    case 0x0007004f: if (value) input_state|=JOY_RIGHT; else input_state&=~JOY_RIGHT; break; // right
    case 0x0007001d: if (value) input_state|=JOY_A; else input_state&=~JOY_A; break; // z
    case 0x0007001b: if (value) input_state|=JOY_B; else input_state&=~JOY_B; break; // x
    case 0x0007002b: if (value) input_state|=JOY_SELECT; else input_state&=~JOY_SELECT; break; // tab
    case 0x00070028: if (value) input_state|=JOY_START; else input_state&=~JOY_START; break; // enter
  #endif
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

  vmrunning=1;
  printf("Loaded.\n");

  FCEUI_PowerNES();
  printf("Power on.\n");
  
  // 20190106: Had a mysterious freeze here, launching a game via Romassist after several restarts.
  // This is right after adding Romassist Restart.
  // I don't know what caused it, and can't reproduce it. Leaving new logging in place for next time.

  if (akfceu_input_register_with_fceu()<0) return -1;

  #if USE_romassist
    if (romassist) {
      printf("Notifying Romassist of launch...\n");
      if (romassist_launched_file(romassist,path)<0) {
        romassist_del(romassist);
        romassist=0;
      }
    }
  #endif

  if (current_rom_path) free(current_rom_path);
  if (!(current_rom_path=strdup(path))) return -1;

  printf("Launch complete.\n");
  return 0;
}

static int restart_vm() {
  if (!current_rom_path) return 0;

  if (vmrunning) {
    FCEUI_CloseGame();
    vmrunning=0;
  }

  FCEUGI *fceugi=FCEUI_LoadGame(current_rom_path);
  if (!fceugi) {
    printf("%s: FCEUI_LoadGame failed\n",current_rom_path);
    return -1;
  }

  vmrunning=1;
  printf("Loaded.\n");

  FCEUI_PowerNES();
  printf("Power on.\n");

  if (akfceu_input_register_with_fceu()<0) return -1;

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
  int i=0; for (;i<256;i++) {
    FCEUD_SetPalette(i,i,i,i);
  }
}

static void draw_test_pattern() {
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

/* Callbacks from Romassist.
 */
#if USE_romassist

static int akfceu_romassist_launch_file(romassist_t client,const char *path,int pathlen) {
  if (load_rom_file(path)<0) {
    return -1;
  }
  return 0;
}

static int akfceu_romassist_command(romassist_t client,const char *src,int srcc) {

  if ((srcc==4)&&!memcmp(src,"quit",4)) {
    printf("akfceu: Quitting due to request from Romassist.\n");
    mn_macioc_quit();
    return 0;
  }

  if ((srcc==7)&&!memcmp(src,"restart",7)) {
    printf("akfceu: Restart per Romassist.\n");
    return restart_vm();
  }

  printf("akfceu: Unexpected command from Romassist: '%.*s'\n",srcc,src);
  return 0;
}

#endif

/* Initialize.
 */

static int init(const char *rompath) {

  if (determine_and_register_home_directory()<0) {
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

  if (akfceu_input_init()<0) {
    return -1;
  }

  if (mn_macaudio_init(22050,1,cb_audio)<0) {
    return -1;
  }

  if (!FCEUI_Initialize()) {
    return -1;
  }

  FCEUI_SetSoundVolume(100);
  FCEUI_SetSoundQuality(0);
  FCEUI_Sound(22050);
  FCEUI_DisableSpriteLimitation(1);

  #if USE_romassist
    struct romassist_delegate romassist_delegate={
      .client_name="akfceu",
      .platforms="nes",
      .launch_file=akfceu_romassist_launch_file,
      .command=akfceu_romassist_command,
    };
    if (romassist=romassist_new(0,0,&romassist_delegate)) {
      printf("Connected to Romassist server.\n");
    } else {
      printf("Failed to connect to Romassist.\n");
    }
  #endif

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
  akfceu_input_quit();
  mn_macaudio_quit();
  mn_macwm_quit();
  #if USE_romassist
    romassist_del(romassist);
    romassist=0;
  #endif
}

/* Update.
 */

static int update() {

  #if USE_romassist
    if (romassist) {
      if (romassist_update(romassist)<0) {
        printf("Error updating Romassist client. Dropping.\n");
        romassist_del(romassist);
        romassist=0;
      }
    }
  #endif

  if (akfceu_input_update()<0) {
    return -1;
  }

  uint8_t *real_framebuffer=framebuffer;
  if (vmrunning) {
    uint8_t *vmfb=0;
    int32_t *vmab=0;
    int32_t vmabc=0;
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
