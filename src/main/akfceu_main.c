#include "fceu/driver.h"
#include "fceu/fceu.h"
#include "nes_autoscore.h"
#include <emuhost.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

void aks_hack_update();

extern uint8_t RAM[0x800];

/* Globals.
 */
 
#define AUTOSCORE_DELAY_TIME 300 /* frames */
 
static uint8_t palette[768]={0};
static int palette_dirty=0;
static int vmrunning=0;
static char *current_rom_path=0;
static uint32_t gamepad_state=0;
static int16_t *samplev=0;
static int samplea=0;
static struct nes_autoscore_context autoscore={0};
static int autoscore_delay=0;

/* XXX TEMP dump all audio to a file.
 * Set the path null to disable this.
 * TODO: Effect this from emuhost, and make it enablable at runtime.
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
  
  nes_autoscore_context_init(&autoscore,fceugi->MD5);
  
  /* XXX A convenience as I gather autoscore schemas.
  const uint8_t *_=fceugi->MD5;
  fprintf(stderr,
    "    .cart_md5={0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,"
    "0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x},\n",
    _[0],_[1],_[2],_[3],_[4],_[5],_[6],_[7],_[8],_[9],
    _[10],_[11],_[12],_[13],_[14],_[15]
  );
  /**/

  fprintf(stderr,"Launch complete.\n");
  return 0;
}

/* Input state.
 */
 
static uint8_t akfceu_fceu_input_state_from_emuhost(uint16_t in) {
  return (
    ((in&EH_BTN_UP)?JOY_UP:0)|
    ((in&EH_BTN_DOWN)?JOY_DOWN:0)|
    ((in&EH_BTN_LEFT)?JOY_LEFT:0)|
    ((in&EH_BTN_RIGHT)?JOY_RIGHT:0)|
    ((in&EH_BTN_SOUTH)?JOY_A:0)|
    ((in&EH_BTN_WEST)?JOY_B:0)|
    ((in&EH_BTN_AUX2)?JOY_SELECT:0)|
    ((in&EH_BTN_AUX1)?JOY_START:0)|
  0);
}

/* Update.
 */
 
static int akfceu_main_update(int partial) {

  aks_hack_update();
  
  gamepad_state=(
    (akfceu_fceu_input_state_from_emuhost(eh_input_get(1)))|
    (akfceu_fceu_input_state_from_emuhost(eh_input_get(2))<<8)|
    (akfceu_fceu_input_state_from_emuhost(eh_input_get(3))<<16)|
    (akfceu_fceu_input_state_from_emuhost(eh_input_get(4))<<24)
  );
  
  uint8_t *vmfb=0;
  int32_t *vmab=0;
  int32_t vmabc=0;
  if (vmrunning) {
    FCEUI_Emulate(&vmfb,&vmab,&vmabc,0);
  } else {
    fprintf(stderr,"akfceu: no rom was loaded\n");
    return -1;
  }
  
  nes_autoscore_context_update(&autoscore,RAM);
  if (!autoscore_delay--) {
    autoscore_delay=AUTOSCORE_DELAY_TIME;
    nes_autoscore_db_save(0);
  }
  
  if (palette_dirty) {
    eh_ctab_write(0,256,palette);
    palette_dirty=0;
  }
  if (vmfb) {
    eh_video_write(vmfb);
  }
  if (vmabc>0) {
    eh_audio_write(vmab,vmabc);
  }
  
  return 0;
}

/* Find the directory for storing FCEU artifacts (saves, etc), and register it with FCEU.
 */

static int determine_and_register_home_directory() {
  char *scratch=0;
  int scratchc=eh_get_scratch_directory(&scratch);
  
  // This is required! If we don't SetBaseDirectory, fceu tries to use "/" as the scratch root.
  if (scratchc<1) return -1;
  
  FCEUI_SetBaseDirectory((char*)scratch);
  return 0;
}

/* Initialize.
 */
 
static int akfceu_main_load_file(const char *path) {

  if (determine_and_register_home_directory()<0) return -1;
  
  if (!FCEUI_Initialize()) {
    fprintf(stderr,"FCEUI_Initialize failed\n");
    return -1;
  }

  FCEUI_SetSoundVolume(100);
  FCEUI_SetSoundQuality(0);
  FCEUI_Sound(22050);
  FCEUI_DisableSpriteLimitation(1);
  
  nes_autoscore_db_load();
  
  return load_rom_file(path);
}

/* Hard reset.
 */

static int akfceu_main_reset() {
  FCEUI_ResetNES();
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
  if (nes_autoscore_db_get_dirty()) {
    nes_autoscore_db_save(1);
  }
}

/* Extra config. Not using.
 */
 
static int akfceu_main_configure(const char *k,int kc,const char *v,int vc,int vn) {
  return -1;
}

/* Window icon.
 */
 
#define akfceu_window_icon_w 16
#define akfceu_window_icon_h 16

static const uint8_t akfceu_window_icon[]={
#define _ 0,0,0,0,
#define W 255,255,255,255,
#define K 0,0,0,255,
#define Y 128,128,128,255,
#define R 255,0,0,255,
  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
  K K K K K K K K K K K K K K K K
  K W W W W W W W W Y W W W W W K
  K W W W W W W W W Y W W W W W K
  K W W W W W W W W Y W W W W W K
  K W W W W W W W W Y W W W W W K
  K Y Y Y Y Y Y Y Y Y Y Y Y Y Y K
  K Y R R Y Y Y Y Y Y Y Y Y Y Y K
  K Y R R Y Y Y Y Y Y Y Y Y Y Y K
  K Y Y Y Y Y Y Y Y Y Y Y Y Y Y K
  _ K K K K K K K K K K K K K K _
  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
  _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _
#undef _
#undef W
#undef K
#undef Y
#undef R
};

/* Main entry point.
 */

int main(int argc,char **argv) {
  struct eh_delegate delegate={
  
    .name="akfceu",
    .iconrgba=akfceu_window_icon,
    .iconw=akfceu_window_icon_w,
    .iconh=akfceu_window_icon_h,
  
    .video_width=256,
    .video_height=240,
    .video_format=EH_VIDEO_FORMAT_I8,
    // rmask,gmask,bmask: Irrelevant for I8.
    .video_rate=60,
    
    .audio_rate=22050,
    .audio_chanc=1,
    .audio_format=EH_AUDIO_FORMAT_S32N_LO16,
    
    .playerc=4,
    
    .configure=akfceu_main_configure,
    .load_file=akfceu_main_load_file,
    // load_serial: Not necessary; only one "load" is needed.
    .update=akfceu_main_update,
    // generate_pcm: Not using; will deliver audio manually.
    .reset=akfceu_main_reset,
  
  };
  int status=eh_main(argc,argv,&delegate);
  akfceu_main_quit();
  return status;
}
