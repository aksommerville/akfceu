#ifndef _FCEUH

#include "driver.h"
#include "git.h"

#define JOY_A      0x01
#define JOY_B      0x02
#define JOY_SELECT 0x04
#define JOY_START  0x08
#define JOY_UP     0x10
#define JOY_DOWN   0x20
#define JOY_LEFT   0x40
#define JOY_RIGHT  0x80

extern int fceuindbg;
void ResetGameLoaded(void);

#define DECLFR(x) uint8_t FP_FASTAPASS(1) x (uint32_t A)
#define DECLFW(x) void FP_FASTAPASS(2) x (uint32_t A, uint8_t V)

void FCEU_MemoryRand(uint8_t *ptr, uint32_t size);
void FASTAPASS(3) SetReadHandler(int32_t start, int32_t end, readfunc func);
void FASTAPASS(3) SetWriteHandler(int32_t start, int32_t end, writefunc func);
writefunc FASTAPASS(1) GetWriteHandler(int32_t a);
readfunc FASTAPASS(1) GetReadHandler(int32_t a);

int AllocGenieRW(void);
void FlushGenieRW(void);

void FCEU_ResetVidSys(void);

void ResetMapping(void);
void ResetNES(void);
void PowerNES(void);

extern uint64_t timestampbase;
extern uint32_t MMC5HackVROMMask;
extern uint8_t *MMC5HackExNTARAMPtr;
extern int MMC5Hack;
extern uint8_t *MMC5HackVROMPTR;
extern uint8_t MMC5HackCHRMode;
extern uint8_t MMC5HackSPMode;
extern uint8_t MMC5HackSPScroll;
extern uint8_t MMC5HackSPPage;

extern uint8_t RAM[0x800];
extern uint8_t GameMemBlock[131072];

extern readfunc ARead[0x10000];
extern writefunc BWrite[0x10000];

extern void (*GameInterface)(int h);
extern void (*GameStateRestore)(int version);

#define GI_RESETM2  1
#define GI_POWER  2
#define GI_CLOSE  3

extern FCEUGI *FCEUGameInfo;
extern int GameAttributes;

extern uint8_t PAL;

typedef struct {
  int PAL;
  int NetworkPlay;
  int SoundVolume;
  int GameGenie;

  /* Current first and last rendered scanlines. */
  int FirstSLine;
  int LastSLine;

  /* Driver code(user)-specified first and last rendered scanlines.
     Usr*SLine[0] is for NTSC, Usr*SLine[1] is for PAL.
   */
  int UsrFirstSLine[2];
  int UsrLastSLine[2];
  int SnapName;
  uint32_t SndRate;
  int soundq;
  int lowpass;
} FCEUS;

extern FCEUS FSettings;

void FCEU_PrintError(char *format, ...);
void FCEU_printf(char *format, ...);
void FCEU_DispMessage(char *format, ...);

void SetNESDeemph(uint8_t d, int force);
void DrawTextTrans(uint8_t *dest, uint32_t width, uint8_t *textmsg, uint8_t fgcolor);
void FCEU_PutImage(void);
#ifdef FRAMESKIP
void FCEU_PutImageDummy(void);
#endif

extern uint8_t Exit;
extern uint8_t pale;
extern uint8_t vsdip;

#define _FCEUH
#endif
