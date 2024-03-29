#ifndef FCEU_DRIVER_H
#define FCEU_DRIVER_H

#include <stdio.h>

#include "types.h"
#include "git.h"
#include "debug.h"

FILE *FCEUD_UTF8fopen(const char *fn, const char *mode);

/* This makes me feel dirty for some reason. */
void FCEU_printf(char *format, ...);
#define FCEUI_printf FCEU_printf

/* Video interface */
void FCEUD_SetPalette(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void FCEUD_GetPalette(uint8_t i,uint8_t *r, uint8_t *g, uint8_t *b);

/* Displays an error.  Can block or not. */
void FCEUD_PrintError(char *s);
void FCEUD_Message(char *s);

void FCEUI_ResetNES(void);
void FCEUI_PowerNES(void);

void FCEUI_NTSCSELHUE(void);
void FCEUI_NTSCSELTINT(void);
void FCEUI_NTSCDEC(void);
void FCEUI_NTSCINC(void);
void FCEUI_GetNTSCTH(int *tint, int *hue);
void FCEUI_SetNTSCTH(int n, int tint, int hue);

void FCEUI_SetInput(int port, int type, void *ptr, int attrib);
void FCEUI_SetInputFC(int type, void *ptr, int attrib);
void FCEUI_DisableFourScore(int s);

#define SI_NONE      0
#define SI_GAMEPAD   1
#define SI_ZAPPER    2
#define SI_POWERPADA  3
#define SI_POWERPADB  4
#define SI_ARKANOID   5

#define SIFC_NONE  0
#define SIFC_ARKANOID  1
#define SIFC_SHADOW  2
#define SIFC_4PLAYER    3
#define SIFC_FKB  4
#define SIFC_HYPERSHOT  5
#define SIFC_MAHJONG  6
#define SIFC_QUIZKING  7
#define SIFC_FTRAINERA  8
#define SIFC_FTRAINERB  9
#define SIFC_OEKAKIDS  10
#define SIFC_BWORLD  11
#define SIFC_TOPRIDER  12

#define SIS_NONE  0
#define SIS_DATACH  1
#define SIS_NWC    2
#define SIS_VSUNISYSTEM  3

/* New interface functions */

/* 0 to order screen snapshots numerically(0.png), 1 to order them file base-numerically(smb3-0.png). */
void FCEUI_SetSnapName(int a);

/* 0 to keep 8-sprites limitation, 1 to remove it */
void FCEUI_DisableSpriteLimitation(int a);

/* -1 = no change, 0 = show, 1 = hide, 2 = internal toggle */
void FCEUI_SetRenderDisable(int sprites, int bg);

/* name=path and file to load.  returns 0 on failure, 1 on success */
FCEUGI *FCEUI_LoadGame(const char *name);

/* allocates memory.  0 on failure, 1 on success. */
int FCEUI_Initialize(void);

/* Emulates a frame. */
void FCEUI_Emulate(uint8_t **, int32_t **, int32_t *, int);

/* Closes currently loaded game */
void FCEUI_CloseGame(void);

/* Deallocates all allocated memory.  Call after FCEUI_Emulate() returns. */
void FCEUI_Kill(void);

/* Set video system a=0 NTSC, a=1 PAL */
void FCEUI_SetVidSystem(int a);

/* Convenience function; returns currently emulated video system(0=NTSC, 1=PAL).  */
int FCEUI_GetCurrentVidSystem(int *slstart, int *slend);

#ifdef FRAMESKIP
/* Should be called from FCEUD_BlitScreen().  Specifies how many frames
   to skip until FCEUD_BlitScreen() is called.  FCEUD_BlitScreenDummy()
   will be called instead of FCEUD_BlitScreen() when when a frame is skipped.
*/
void FCEUI_FrameSkip(int x);
#endif

/* First and last scanlines to render, for ntsc and pal emulation. */
void FCEUI_SetRenderedLines(int ntscf, int ntscl, int palf, int pall);

/* Sets the base directory(save states, snapshots, etc. are saved in directories
   below this directory. */
void FCEUI_SetBaseDirectory(const char *dir);

/* Tells FCE Ultra to copy the palette data pointed to by pal and use it.
   Data pointed to by pal needs to be 64*3 bytes in length.
*/
void FCEUI_SetPaletteArray(uint8_t *pal);

/* Sets up sound code to render sound at the specified rate, in samples
   per second.  Only sample rates of 44100, 48000, and 96000 are currently
   supported.
   If "Rate" equals 0, sound is disabled.
*/
void FCEUI_Sound(int Rate);
void FCEUI_SetSoundVolume(uint32_t volume);
void FCEUI_SetSoundQuality(int quality);

void FCEUI_SelectState(int);

/* "fname" overrides the default save state filename code if non-NULL. */
void FCEUI_SaveState(char *fname);
void FCEUI_LoadState(char *fname);

int32_t FCEUI_GetDesiredFPS(void);

#define FCEUIOD_STATE   0
#define FCEUIOD_SNAPS   1
#define FCEUIOD_NV      2
#define FCEUIOD_MISC    4

#define FCEUIOD__COUNT  6

void FCEUI_MemDump(uint16_t a, int32_t len, void (*callb)(uint16_t a, uint8_t v));
uint8_t FCEUI_MemSafePeek(uint16_t A);
void FCEUI_MemPoke(uint16_t a, uint8_t v, int hl);
void FCEUI_NMI(void);
void FCEUI_IRQ(void);
uint16_t FCEUI_Disassemble(void *XA, uint16_t a, char *stringo);
void FCEUI_GetIVectors(uint16_t *reset, uint16_t *irq, uint16_t *nmi);

uint32_t FCEUI_CRC32(uint32_t crc, uint8_t *buf, uint32_t len);

void FCEUI_ToggleTileView(void);
void FCEUI_SetLowPass(int q);

void FCEUI_VSUniToggleDIPView(void);
void FCEUI_VSUniToggleDIP(int w);
uint8_t FCEUI_VSUniGetDIPs(void);
void FCEUI_VSUniSetDIP(int w, int state);
void FCEUI_VSUniCoin(void);

int FCEUI_FDSInsert(int oride);
int FCEUI_FDSEject(void);
void FCEUI_FDSSelect(void);

int FCEUI_DatachSet(const uint8_t *rcode);

#endif
