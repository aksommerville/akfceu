#include "akfceu_video.h"
#include "akfceu_input_map.h"
#include "akfceu_input.h"
#include "fceu/driver.h"
#include "fceu/fceu.h"
#include <emuhost/emuhost.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

/* Globals.
 */
 
static uint8_t palette[768]={0};
static int vmrunning=0;
static char *current_rom_path=0;
static uint32_t gamepad_state=0;
static int16_t *samplev=0;
static int samplea=0;

/* FCEU palette.
 * TODO do we really need to cache the palette here? Either eliminate it (if GetPalette unnecessary), or provide a getter from emuhost.
 */

void FCEUD_SetPalette(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
  int p=index*3;
  palette[p++]=r;
  palette[p++]=g;
  palette[p]=b;
  uint8_t tmp[3]={r,g,b};
  emuhost_wm_set_palette(tmp,index,1);
}

void FCEUD_GetPalette(uint8_t i,uint8_t *r, uint8_t *g, uint8_t *b) {
  int p=i*3;
  *r=palette[p++];
  *g=palette[p++];
  *b=palette[p];
}

/* Audio sample buffer.
 */
 
static int akfceu_samplev_require(int c) {
  if (c<=samplea) return 0;
  int na=(c+1024)&~1023;
  void *nv=realloc(samplev,na<<1);
  if (!nv) return -1;
  samplev=nv;
  samplea=na;
  return 0;
}

/* Load a fresh ROM file and reset the emulator.
 */

static int load_rom_file(const char *path) {
  fprintf(stderr,"Load ROM: %s\n",path);

  if (vmrunning) {
    FCEUI_CloseGame();
    vmrunning=0;
  }

  FCEUGI *fceugi=FCEUI_LoadGame(path);
  if (!fceugi) {
    fprintf(stderr,"%s: FCEUI_LoadGame failed\n",path);
    return -1;
  }

  vmrunning=1;
  fprintf(stderr,"Loaded.\n");

  FCEUI_PowerNES();
  fprintf(stderr,"Power on.\n");
  
  FCEUI_DisableFourScore(0);
  FCEUI_SetInput(0,SI_GAMEPAD,&gamepad_state,0);
  FCEUI_SetInput(1,SI_GAMEPAD,&gamepad_state,0);

  if (current_rom_path) free(current_rom_path);
  if (!(current_rom_path=strdup(path))) return -1;

  fprintf(stderr,"Launch complete.\n");
  return 0;
}

/* Logging, accessible to emuhost.
 */
 
static void akfceu_main_log(int level,const char *msg,int msgc) {
  printf("%s: %.*s\n",__func__,msgc,msg);
}

/* Load rom.
 */
 
static int akfceu_main_load_rom(const char *path,const void *src,int srcc) {
  fprintf(stderr,"%s '%s', %d bytes\n",__func__,path,srcc);
  //TODO Can we provide the serial to FCEU? As is we are reading the file twice.
  if (load_rom_file(path)<0) return -1;
  return 0;
}

/* Input event.
 */
 
static uint8_t akfceu_fceu_input_state_from_emuhost(uint16_t in) {
  return (
    ((in&EMUHOST_BTNID_UP)?JOY_UP:0)|
    ((in&EMUHOST_BTNID_DOWN)?JOY_DOWN:0)|
    ((in&EMUHOST_BTNID_LEFT)?JOY_LEFT:0)|
    ((in&EMUHOST_BTNID_RIGHT)?JOY_RIGHT:0)|
    ((in&EMUHOST_BTNID_PRIMARY)?JOY_A:0)|
    ((in&EMUHOST_BTNID_SECONDARY)?JOY_B:0)|
    ((in&EMUHOST_BTNID_AUX2)?JOY_SELECT:0)|
    ((in&EMUHOST_BTNID_AUX1)?JOY_START:0)|
  0);
}
 
static int akfceu_main_event(int playerid,int btnid,int value,int state) {
  //fprintf(stderr,"%s %d.%d=%d [0x%04x]\n",__func__,playerid,btnid,value,state);
  if ((playerid>=1)&&(playerid<=4)) {
    int shift=(playerid-1)<<3;
    uint32_t mask=~(0xff<<shift);
    uint8_t fceustate=akfceu_fceu_input_state_from_emuhost(state);
    gamepad_state=(gamepad_state&mask)|(fceustate<<shift);
  }
  return 0;
}

/* Update.
 */
 
static int akfceu_main_update() {
  
  uint8_t *vmfb=0;
  int32_t *vmab=0;
  int32_t vmabc=0;
  if (vmrunning) {
    FCEUI_Emulate(&vmfb,&vmab,&vmabc,0);
  }
  
  if (akfceu_samplev_require(vmabc)<0) return -1;
  
  if (vmabc>0) {
    // VM is running and we got audio from it -- update emuhost normally.
    const int32_t *src=vmab; // Seriously, fceu, why 32-bit? TODO Can we change that?
    int16_t *dst=samplev;
    int i=vmabc; for (;i-->0;src++,dst++) *dst=*src;
    if (emuhost_app_provide_frame(vmfb,samplev,vmabc)<0) return -1;
  } else {
    // Not running or not producing audio. Send a little silence to keep things running steady.
    if (akfceu_samplev_require(1024)<0) return -1;
    memset(samplev,0,1024<<1);
    if (emuhost_app_provide_frame(vmfb,samplev,1024)<0) return -1;
  }
  
  return 0;
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
 
static int akfceu_main_init() {

  if (determine_and_register_home_directory()<0) return -1;
  
  if (1) { //XXX initialize color table
    int i=0; for (;i<256;i++) {
      uint8_t rgb[3]={i,i,i};
      emuhost_wm_set_palette(rgb,i,1);
    }
  }
  
  if (!FCEUI_Initialize()) {
    fprintf(stderr,"FCEUI_Initialize failed\n");
    return -1;
  }

  FCEUI_SetSoundVolume(100);
  FCEUI_SetSoundQuality(0);
  FCEUI_Sound(22050);
  FCEUI_DisableSpriteLimitation(1);
  
  return 0;
}

/* Quit.
 */
 
static void akfceu_main_quit() {
  if (vmrunning) {
    FCEUI_Sound(0);
    FCEUI_CloseGame();
    FCEUI_Kill();
    vmrunning=0;
  }
}

/* Receive command-line option.
 */
 
static int akfceu_main_option(const char *k,int kc,const char *v,int vc) {
  fprintf(stderr,"%s: '%.*s' = '%.*s'\n",__func__,kc,k,vc,v);
  return 0;
}

/* Main entry point.
 */

int main(int argc,char **argv) {
  struct emuhost_app_delegate appdelegate={
  
    .fbw=256,
    .fbh=240,
    .fbformat=EMUHOST_TEXFMT_I8,
    .fullscreen=0,
    .window_title="akfceu",
    .audiorate=22050,
    .audiochanc=1,
    .romassist_port=0,//8111,
    .appname="akfceu",
    .platform="nes",
  
    .option=akfceu_main_option,
    .log=akfceu_main_log,
    .load_rom=akfceu_main_load_rom,
    .event=akfceu_main_event,
    .init=akfceu_main_init,
    .quit=akfceu_main_quit,
    .update=akfceu_main_update,
    
  };
  return emuhost_app_main(argc,argv,&appdelegate);
}
