#ifndef FCEU_CART_H
#define FCEU_CART_H

typedef struct {
  /* Set by mapper/board code: */
  void (*Power)(void);
  void (*Reset)(void);
  void (*Close)(void);
  uint8_t *SaveGame[4];     /* Pointers to memory to save/load. */
  uint32_t SaveGameLen[4];  /* How much memory to save/load. */

  /* Set by iNES/UNIF loading code. */
  int mirror;    /* As set in the header or chunk.
                    iNES/UNIF specific.  Intended
                    to help support games like "Karnov"
                    that are not really MMC3 but are
                    set to mapper 4.
                 */
  int battery; /* Presence of an actual battery. */
  uint8_t MD5[16];
  uint32_t CRC32; /* Should be set by the iNES/UNIF loading
                   code, used by mapper/board code, maybe
                   other code in the future.
                */
} CartInfo;

void FCEU_SaveGameSave(CartInfo *LocalHWInfo);
void FCEU_LoadGameSave(CartInfo *LocalHWInfo);

extern uint8_t *Page[32],*VPage[8],*MMC5SPRVPage[8],*MMC5BGVPage[8];

void ResetCartMapping(void);
void SetupCartPRGMapping(int chip, uint8_t *p, uint32_t size, int ram);
void SetupCartCHRMapping(int chip, uint8_t *p, uint32_t size, int ram);
void SetupCartMirroring(int m, int hard, uint8_t *extra);

DECLFR(CartBROB);
DECLFR(CartBR);
DECLFW(CartBW);

extern uint8_t *PRGptr[32];
extern uint8_t *CHRptr[32];

extern uint32_t PRGsize[32];
extern uint32_t CHRsize[32];

extern uint32_t PRGmask2[32];
extern uint32_t PRGmask4[32];
extern uint32_t PRGmask8[32];
extern uint32_t PRGmask16[32];
extern uint32_t PRGmask32[32];

extern uint32_t CHRmask1[32];
extern uint32_t CHRmask2[32];
extern uint32_t CHRmask4[32];
extern uint32_t CHRmask8[32];

void FASTAPASS(2) setprg2(uint32_t A, uint32_t V);
void FASTAPASS(2) setprg4(uint32_t A, uint32_t V);
void FASTAPASS(2) setprg8(uint32_t A, uint32_t V);
void FASTAPASS(2) setprg16(uint32_t A, uint32_t V);
void FASTAPASS(2) setprg32(uint32_t A, uint32_t V);

void FASTAPASS(3) setprg2r(int r, unsigned int A, unsigned int V);
void FASTAPASS(3) setprg4r(int r, unsigned int A, unsigned int V);
void FASTAPASS(3) setprg8r(int r, unsigned int A, unsigned int V);
void FASTAPASS(3) setprg16r(int r, unsigned int A, unsigned int V);
void FASTAPASS(3) setprg32r(int r, unsigned int A, unsigned int V);

void FASTAPASS(3) setchr1r(int r, unsigned int A, unsigned int V);
void FASTAPASS(3) setchr2r(int r, unsigned int A, unsigned int V);
void FASTAPASS(3) setchr4r(int r, unsigned int A, unsigned int V);
void FASTAPASS(2) setchr8r(int r, unsigned int V);

void FASTAPASS(2) setchr1(unsigned int A, unsigned int V);
void FASTAPASS(2) setchr2(unsigned int A, unsigned int V);
void FASTAPASS(2) setchr4(unsigned int A, unsigned int V);
void FASTAPASS(1) setchr8(unsigned int V);

void FASTAPASS(2) setvram4(uint32_t A, uint8_t *p);
void FASTAPASS(1) setvram8(uint8_t *p);

void FASTAPASS(3) setvramb1(uint8_t *p, uint32_t A, uint32_t b);
void FASTAPASS(3) setvramb2(uint8_t *p, uint32_t A, uint32_t b);
void FASTAPASS(3) setvramb4(uint8_t *p, uint32_t A, uint32_t b);
void FASTAPASS(2) setvramb8(uint8_t *p, uint32_t b);

void FASTAPASS(1) setmirror(int t);
void setmirrorw(int a, int b, int c, int d);
void FASTAPASS(3) setntamem(uint8_t *p, int ram, uint32_t b);

#define MI_H 0
#define MI_V 1
#define MI_0 2
#define MI_1 3

#endif
