#include "fceu/driver.h"
#include "fceu/fceu.h"
#include <emuhost/emuhost.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

/* Globals.
 */
 
static uint8_t palette[768]={0};
static int palette_dirty=0;
static int vmrunning=0;
static char *current_rom_path=0;
static uint32_t gamepad_state=0;
static int16_t *samplev=0;
static int samplea=0;

/* XXX TEMP dump all audio to a file.
 * Set the path null to disable this.
 */
 
static const char *dump_audio_path=0;//"/home/andy/proj/akfceu/out/dump.s16le.22050";
static int dump_audio_fd=-1;
 
static void dump_audio(const int16_t *src,int srcc) { // srcc in samples

  // Should we open the file? Don't begin recording until there's a signal.
  if (dump_audio_fd<0) {
    if (!dump_audio_path) return;
    const int16_t *v=src;
    int i=srcc,valid=0;
    for (;i-->0;v++) if (*v) { valid=1; break; }
    if (!valid) return;
    // Fails to open, forget it.
    if ((dump_audio_fd=open(dump_audio_path,O_WRONLY|O_CREAT|O_TRUNC,0666))<0) {
      fprintf(stderr,"%s: %m\n",dump_audio_path);
      dump_audio_path=0;
      return;
    }
  }

  srcc<<=1; // ...srcc in bytes
  write(dump_audio_fd,src,srcc);
}

static void dump_audio_finish() {
  if (dump_audio_fd>=0) {
    close(dump_audio_fd);
    dump_audio_fd=-1;
  }
}

/* FCEU palette.
 */

void FCEUD_SetPalette(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
  int p=index*3;
  palette[p++]=r;
  palette[p++]=g;
  palette[p]=b;
  palette_dirty=1;
}

void FCEUD_GetPalette(uint8_t i,uint8_t *r, uint8_t *g, uint8_t *b) {
  int p=i*3;
  *r=palette[p++];
  *g=palette[p++];
  *b=palette[p];
}

/* Other FCEU driver hooks.
 */

FILE *FCEUD_UTF8fopen(const char *fn, const char *mode) {
  return fopen(fn,mode);
}

void FCEUD_PrintError(char *s) {
  fprintf(stderr,"FCEUD_PrintError: %s\n",s);
}

void FCEUD_Message(char *s) {
  fprintf(stderr,"%s",s);
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

  FCEUI_PowerNES();
  
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
 
static int akfceu_main_load_rom(void *userdata,const char *path) {
  if (load_rom_file(path)<0) return -1;
  return 0;
}

/* Input event.
 */
 
static uint8_t akfceu_fceu_input_state_from_emuhost(uint16_t in) {
  return (
    ((in&EH_BUTTON_UP)?JOY_UP:0)|
    ((in&EH_BUTTON_DOWN)?JOY_DOWN:0)|
    ((in&EH_BUTTON_LEFT)?JOY_LEFT:0)|
    ((in&EH_BUTTON_RIGHT)?JOY_RIGHT:0)|
    ((in&EH_BUTTON_A)?JOY_A:0)|
    ((in&EH_BUTTON_B)?JOY_B:0)|
    ((in&EH_BUTTON_AUX2)?JOY_SELECT:0)|
    ((in&EH_BUTTON_AUX1)?JOY_START:0)|
  0);
}

static uint16_t inputstate[4]={0};
 
static int akfceu_main_event(void *userdata,int playerid,int btnid,int value,int state) {
  //fprintf(stderr,"%s %d.%d=%d [0x%04x]\n",__func__,playerid,btnid,value,state);
  if ((playerid>=1)&&(playerid<=4)) {
    if (value) inputstate[playerid-1]|=btnid;
    else inputstate[playerid-1]&=~btnid;
    int shift=(playerid-1)<<3;
    uint32_t mask=~(0xff<<shift);
    uint8_t fceustate=akfceu_fceu_input_state_from_emuhost(inputstate[playerid-1]);
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
  } else {
    fprintf(stderr,"akfceu: no rom was loaded\n");
    return -1;
  }
  
  if (palette_dirty) {
    eh_hi_color_table(palette);
    palette_dirty=0;
  }
  if (eh_hi_frame(vmfb,vmab,vmabc)<0) return -1;
  
  return 0;
}

/* Find the directory for storing FCEU artifacts (saves, etc), and register it with FCEU.
 */

static int determine_and_register_home_directory() {

  const char *scratch=0;
  int scratchc=eh_hi_get_scratch_directory(&scratch);
  
  // Empty is an explicit request for no saving. TODO confirm FCEU is ok with not setting base directory
  if (scratchc<1) return 0;
  
  FCEUI_SetBaseDirectory((char*)scratch);
  return 0;
}

/* Initialize.
 */
 
static int akfceu_main_init() {

  if (determine_and_register_home_directory()<0) return -1;
  
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
  dump_audio_finish();
  if (vmrunning) {
    FCEUI_Sound(0);
    FCEUI_CloseGame();
    FCEUI_Kill();
    vmrunning=0;
  }
}

/* Main entry point.
 */

int main(int argc,char **argv) {
  struct eh_hi_delegate delegate={
    .video_rate=60,
    .video_width=256,
    .video_height=240,
    .video_format=EH_VIDEO_FORMAT_I8,
    .audio_rate=22050,
    .audio_chanc=1,
    .audio_format=EH_AUDIO_FORMAT_S32_LO16,
    .playerc=4,
    .appname="akfceu",
    .userdata=0,
    .init=akfceu_main_init,
    .quit=akfceu_main_quit,
    .update=akfceu_main_update,
    .player_input=akfceu_main_event,
    .reset=akfceu_main_load_rom,
  };
  return eh_hi_main(&delegate,argc,argv);
}
