#include "nes_autoscore.h"
#include "util/akfceu_file.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

/* Metadata registry.
 * Since I don't have any idea of the final shape of this, it's all hard coded.
 * Plan to write this out to a data file.
 */
 
// Sort by cart_md5.
static const struct nes_autoscore_game nes_autoscore_gamev[]={
  {
    .cart_md5={0x16,0xf7,0x8d,0x85,0x61,0x42,0x9f,0xc7,0x5f,0x38,0xd7,0x26,0xed,0xc5,0x7e,0x23},
    .name="Krazy Kreatures",
    .addr=0x0107,
    .addr_shadow=0xffff,
    .format=NES_AUTOSCORE_FORMAT_MIXED66,
    .initial={
      0x00,0x00,0x02,0x40,0x00,0xa3,0xa5,0xa3,0x91,0x9e,0x70,
      0x00,0x50,0x01,0x30,0x00,0x96,0xa2,0x91,0x9e,0xaa,0x70,
      0x00,0x10,0x01,0x00,0x00,0x94,0x91,0xa6,0x95,0x70,0x70,
      0x00,0x71,0x00,0x00,0x00,0x70,0x70,0x70,0x70,0x70,0x70,
      0x00,0x50,0x00,0x00,0x00,0x70,0x70,0x70,0x70,0x70,0x70,
      0x00,0x30,0x00,0x00,0x00,0x70,0x70,0x70,0x70,0x70,0x70,
    },
  },
  {
    .cart_md5={0x2b,0x5c,0x0d,0xae,0x29,0x65,0x06,0x91,0xe6,0x7a,0x08,0xea,0x58,0xf5,0x54,0xe2},
    .name="Gun Nac",
    .addr=0x00e7,
    .addr_shadow=0xffff,
    .format=NES_AUTOSCORE_FORMAT_D7,
    .initial={0x00,0x00,0x08,0x00,0x00,0x00,0x00},
  },
  {
    .cart_md5={0x2f,0xc3,0x53,0xde,0x37,0x36,0xac,0x06,0x14,0xa1,0x0d,0x3d,0x4b,0xff,0x98,0x45},
    .name="Tetris",
    .addr=0x0700,
    .addr_shadow=0xffff,
    .format=NES_AUTOSCORE_FORMAT_MIXED56,
    .initial={
      0x08,0x0f,0x17,0x01,0x12,0x04,0x0f,0x14,
      0x01,0x13,0x01,0x0e,0x0c,0x01,0x0e,0x03,
      0x05,0x2b,0x00,0x00,0x00,0x00,0x00,0x00,
      0x01,0x0c,0x05,0x18,0x2b,0x2b,0x14,0x0f,
      0x0e,0x19,0x2b,0x2b,0x0e,0x09,0x0e,0x14,
      0x05,0x0e,0x00,0x00,0x00,0x00,0x00,0x00,
      0x01,0x00,0x00,0x00,0x75,0x00,0x00,0x50,
    },
  },
  {
    .cart_md5={0x3e,0xb2,0xa5,0x6c,0x3c,0x76,0x56,0xb2,0x1f,0x29,0x94,0x2d,0x24,0xf4,0x2c,0x95},
    .name="Balloon Fight",
    .addr=0x0629,
    .addr_shadow=0x000d,
    .format=NES_AUTOSCORE_FORMAT_D5LEX10,
    .initial={0x00,0x00,0x00,0x01,0x00},
  },
  {
    .cart_md5={0x40,0x08,0x91,0x53,0x66,0x0f,0x09,0x2b,0x5c,0xbb,0x6e,0x20,0x4e,0xfc,0xe1,0xb7},
    .name="Qix",
    .addr=0x0700,
    .addr_shadow=0xffff,
    .format=NES_AUTOSCORE_FORMAT_MIXED82,
    .initial={
      0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,
      0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,
      0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,
      0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,
      0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,
      0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,
      0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,
      0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,
      0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,
      0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,
      0x00,0x00,
    },
  },
  {
    .cart_md5={0x43,0x0c,0x98,0x61,0x31,0x46,0x32,0x70,0x62,0x28,0x4f,0x1d,0xc4,0x7f,0x8f,0xa7},
    .name="Joust",
    .addr=0x0036,
    .addr_shadow=0xffff,
    .format=NES_AUTOSCORE_FORMAT_BCD14X3,
    .initial={0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  },
  {
    .cart_md5={0x46,0x5d,0xc9,0x5c,0x99,0x5e,0x70,0xb3,0xa1,0x61,0xf0,0x6d,0x67,0xc1,0x94,0x2d},
    .name="Goonies",
    .addr=0x07f0,
    .addr_shadow=0xffff,
    .format=NES_AUTOSCORE_FORMAT_BCD3LE,
    .initial={0x00,0x00,0x01},
  },
  {
    .cart_md5={0x49,0x22,0xed,0xd6,0xc7,0xcb,0x26,0x6d,0x4c,0x35,0xd1,0x1c,0xc5,0x60,0xe7,0xcd},
    .name="Jaws",
    .addr=0x0350,
    .addr_shadow=0xffff,
    .format=NES_AUTOSCORE_FORMAT_D7,
    .initial={0x00,0x00,0x01,0x00,0x00,0x00,0x00},
  },
  {
    .cart_md5={0x76,0x7b,0x69,0x6c,0x7a,0x74,0x6d,0x95,0xbc,0x0d,0xa0,0xae,0xcd,0x5f,0x63,0x47},
    .name="Paperboy",
    .addr=0x0773,
    .addr_shadow=0xffff,
    .format=NES_AUTOSCORE_FORMAT_MIXED80,
    .initial={
       0x00,0x01,0x01,0x00,0x00,0x52,0x42,0x20,
       0x00,0x01,0x00,0x09,0x00,0x44,0x41,0x54,
       0x00,0x01,0x00,0x08,0x00,0x54,0x48,0x45,
       0x00,0x01,0x00,0x07,0x00,0x42,0x4f,0x59,
       0x00,0x01,0x00,0x06,0x00,0x42,0x46,0x20,
       0x00,0x01,0x00,0x05,0x00,0x4d,0x45,0x43,
       0x00,0x01,0x00,0x04,0x00,0x43,0x53,0x20,
       0x00,0x01,0x00,0x03,0x00,0x4a,0x45,0x53,
       0x00,0x01,0x00,0x02,0x00,0x50,0x43,0x54,
       0x00,0x01,0x00,0x01,0x00,0x4d,0x41,0x41,
    },
  },
  {
    .cart_md5={0xd1,0xc1,0x36,0x45,0x82,0x95,0x26,0xbc,0x1d,0xec,0xe8,0xb4,0x3f,0x80,0xf9,0x15},
    .name="Kung Fu",
    .addr=0x0511,
    .addr_shadow=0xffff,
    .format=NES_AUTOSCORE_FORMAT_D6,
    .initial={0x00,0x00,0x00,0x00,0x00,0x00},
  },
  {
    .cart_md5={0xe7,0x09,0xcc,0x6d,0xdc,0xfa,0xa4,0x11,0x9d,0xfd,0x1c,0x49,0x94,0xc5,0x4c,0x75},
    .name="Galaxian",
    .addr=0x011d,
    .addr_shadow=0xffff,
    .format=NES_AUTOSCORE_FORMAT_D9,
    .initial={0x10,0x10,0x10,0x10,0x10,0x05,0x00,0x00,0x00},
  },
  {
    .cart_md5={0xe7,0xd7,0x22,0x5d,0xad,0x04,0x4b,0x62,0x4f,0xba,0xd9,0xc9,0xca,0x96,0xe8,0x35},
    .name="Wrecking Crew",
    .addr=0x0081,
    .addr_shadow=0xffff,
    .format=NES_AUTOSCORE_FORMAT_BCD3BE,
    .initial={0x02,0x60,0x00},
  },
  {
    .cart_md5={0xed,0x3d,0xc4,0x46,0x5e,0x40,0x53,0xbd,0xfe,0x42,0xcc,0x33,0xe4,0xe6,0xa9,0x56},
    .name="Yoshi's Cookie",
    .addr=0x03d0,
    .addr_shadow=0xffff,
    .format=NES_AUTOSCORE_FORMAT_D7, // 0xd0..0xd9 per digit
    .initial={0xd0,0xd0,0xd0,0xd0,0xd0,0xd0,0xd0},
  },
};

const struct nes_autoscore_game *nes_autoscore_game_by_md5(const void *md5) {
  if (!md5) return 0;
  int lo=0,hi=sizeof(nes_autoscore_gamev)/sizeof(struct nes_autoscore_game);
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int cmp=memcmp(md5,nes_autoscore_gamev[ck].cart_md5,16);
         if (cmp<0) hi=ck;
    else if (cmp>0) lo=ck+1;
    else return nes_autoscore_gamev+ck;
  }
  return 0;
}

/* Initialize context.
 */

void nes_autoscore_context_init(
  struct nes_autoscore_context *ctx,
  const void *rom_md5
) {
  if (!ctx||!rom_md5) return;
  memset(ctx,0,sizeof(struct nes_autoscore_context));
  if (!(ctx->game=nes_autoscore_game_by_md5(rom_md5))) {
    fprintf(stderr,"Autoscore schema not found (this is fine).\n");
    return;
  }
  
  switch (ctx->game->format) {
    case NES_AUTOSCORE_FORMAT_D5LEX10: ctx->score_size=5; break;
    case NES_AUTOSCORE_FORMAT_BCD14X3: ctx->score_size=7; break;
    case NES_AUTOSCORE_FORMAT_D7: ctx->score_size=7; break;
    case NES_AUTOSCORE_FORMAT_BCD3LE: ctx->score_size=3; break;
    case NES_AUTOSCORE_FORMAT_D6: ctx->score_size=6; break;
    case NES_AUTOSCORE_FORMAT_BCD3BE: ctx->score_size=3; break;
    case NES_AUTOSCORE_FORMAT_D9: ctx->score_size=9; break;
    case NES_AUTOSCORE_FORMAT_MIXED82: ctx->score_size=82; break;
    case NES_AUTOSCORE_FORMAT_MIXED66: ctx->score_size=66; break;
    case NES_AUTOSCORE_FORMAT_MIXED80: ctx->score_size=80; break;
    case NES_AUTOSCORE_FORMAT_MIXED56: ctx->score_size=56; break;
    default: fprintf(stderr,"%s:%d: Unknown score format %d!\n",__FILE__,__LINE__,ctx->game->format);
  }
  
  const void *score=nes_autoscore_db_get_score(rom_md5);
  if (score) {
    memcpy(ctx->score,score,NES_AUTOSCORE_LONGEST_SIZE);
    fprintf(stderr,"Got high score from database.\n");
  } else {
    memcpy(ctx->score,ctx->game->initial,NES_AUTOSCORE_LONGEST_SIZE);
    fprintf(stderr,"Found autoscore schema but no existing record.\n");
  }
}

/* Update context.
 */

void nes_autoscore_context_update(
  struct nes_autoscore_context *ctx,
  void *ram
) {
  if (!ctx->game) return;
  
  // Waiting for initial state?
  if (!ctx->running) {
    if (memcmp(ram+ctx->game->addr,ctx->game->initial,ctx->score_size)) return;
    fprintf(stderr,"Reached initial RAM state for autoscore.\n");
    ctx->running=1;
    memcpy(ram+ctx->game->addr,ctx->score,ctx->score_size);
    if (ctx->game->addr_shadow<=0x800-ctx->score_size) {
      memcpy(ram+ctx->game->addr_shadow,ctx->score,ctx->score_size);
    }
    return;
  }
  
  // Watch for a change.
  if (memcmp(ram+ctx->game->addr,ctx->score,ctx->score_size)) {
    //TODO Allow for "greater only" somehow.
    memcpy(ctx->score,ram+ctx->game->addr,ctx->score_size);
    nes_autoscore_db_set_record(ctx->game->cart_md5,ctx->score,ctx->score_size);
  }
}

/* Database globals.
 */
 
static struct {
  const char *path;
  uint8_t *content;
  int contentc,contenta; // bytes
  int recordc; // contentc/recordlen
  int dirty;
} nes_autoscore_db={
  .path="/home/andy/.romassist/nes-autoscore-db",//XXX must be configurable
};

/* Load database.
 */
 
void nes_autoscore_db_load() {
  if (nes_autoscore_db.content) free(nes_autoscore_db.content);
  nes_autoscore_db.content=0;
  nes_autoscore_db.contentc=0;
  nes_autoscore_db.contenta=0;
  nes_autoscore_db.recordc=0;
  nes_autoscore_db.dirty=0;
  
  if ((nes_autoscore_db.contentc=akfceu_file_read(&nes_autoscore_db.content,nes_autoscore_db.path))<0) {
    nes_autoscore_db.contentc=0;
    fprintf(stderr,"%s: Failed to read high scores. No worries.\n",nes_autoscore_db.path);
    return;
  }
  nes_autoscore_db.contenta=nes_autoscore_db.contentc;
  nes_autoscore_db.recordc=nes_autoscore_db.contentc/NES_AUTOSCORE_RECORD_SIZE;
  fprintf(stderr,"%s: Loaded %d high scores.\n",nes_autoscore_db.path,nes_autoscore_db.recordc);
}

/* Save database.
 */
 
void nes_autoscore_db_save(int force) {
  if (force||nes_autoscore_db.dirty) {
    if (akfceu_file_write(nes_autoscore_db.path,nes_autoscore_db.content,nes_autoscore_db.contentc)<0) {
      fprintf(stderr,"%s: Failed to write high scores.\n",nes_autoscore_db.path);
    } else {
      fprintf(stderr,"%s: Saved high scores.\n",nes_autoscore_db.path);
    }
    nes_autoscore_db.dirty=0;
  }
}

/* Search records by hash.
 */
 
static int nes_autoscore_db_search(const void *q) {
  int lo=0,hi=nes_autoscore_db.recordc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int cmp=memcmp(q,nes_autoscore_db.content+ck*NES_AUTOSCORE_RECORD_SIZE,NES_AUTOSCORE_HASH_SIZE);
         if (cmp<0) hi=ck;
    else if (cmp>0) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Trivial public db accessors.
 */

void nes_autoscore_db_dirty() {
  nes_autoscore_db.dirty=1;
}

int nes_autoscore_db_get_dirty() {
  return nes_autoscore_db.dirty;
}

void *nes_autoscore_db_get_record(const void *md5) {
  int p=nes_autoscore_db_search(md5);
  if (p<0) return 0;
  return nes_autoscore_db.content+p*NES_AUTOSCORE_RECORD_SIZE;
}

/* Insert record.
 */
 
static void *nes_autoscore_db_insert(int p,const void *md5) {
  if ((p<0)||(p>nes_autoscore_db.recordc)) return 0;
  if (nes_autoscore_db.contentc>nes_autoscore_db.contenta-NES_AUTOSCORE_RECORD_SIZE) {
    // Grow by one record only. (We have a single file per launch; you shouldn't ever see more than one add per run).
    int na=nes_autoscore_db.contentc+NES_AUTOSCORE_RECORD_SIZE;
    void *nv=realloc(nes_autoscore_db.content,na);
    if (!nv) return 0;
    nes_autoscore_db.content=nv;
    nes_autoscore_db.contenta=na;
  }
  int bytep=p*NES_AUTOSCORE_RECORD_SIZE;
  uint8_t *record=nes_autoscore_db.content+bytep;
  memmove(record+NES_AUTOSCORE_RECORD_SIZE,record,nes_autoscore_db.contentc-bytep);
  nes_autoscore_db.recordc++;
  nes_autoscore_db.contentc+=NES_AUTOSCORE_RECORD_SIZE;
  memcpy(record,md5,NES_AUTOSCORE_HASH_SIZE);
  return record;
}

/* Add or update record.
 */

void nes_autoscore_db_set_record(const void *md5,const void *score,int scoresize) {
  if (!md5||!score||(scoresize<0)||(scoresize>NES_AUTOSCORE_LONGEST_SIZE)) return;
  void *record=0;
  int p=nes_autoscore_db_search(md5);
  if (p<0) {
    p=-p-1;
    if (!(record=nes_autoscore_db_insert(p,md5))) return;
  } else {
    record=nes_autoscore_db.content+p*NES_AUTOSCORE_RECORD_SIZE;
  }
  memcpy(nes_autoscore_db_score_from_record(record),score,scoresize);
  nes_autoscore_db_now(nes_autoscore_db_timestamp_from_record(record));
  nes_autoscore_db.dirty=1;
}

/* Current timestamp.
 */

void nes_autoscore_db_now(void *dst_8bytes) {
  time_t now=time(0);
  struct tm tm={0};
  localtime_r(&now,&tm);
  struct timeval tv={0};
  gettimeofday(&tv,0);
  int year=1900+tm.tm_year;
  int month=1+tm.tm_mon;
  int day=tm.tm_mday;
  int hour=tm.tm_hour;
  int minute=tm.tm_min;
  int second=tm.tm_sec;
  int centi=(tv.tv_usec/10000)%100;
  uint8_t *DST=dst_8bytes;
  
  #define BCD(dstp,src) DST[dstp]=((((src)/10)%10)<<4)|((src)%10);
  BCD(0,year/100)
  BCD(1,year)
  BCD(2,month)
  BCD(3,day)
  BCD(4,hour)
  BCD(5,minute)
  BCD(6,second)
  BCD(7,centi)
  #undef BCD
}
