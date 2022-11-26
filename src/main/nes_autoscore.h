/* nes_autoscore.h
 * A system for persisting high scores in NES games that don't supply their own battery backup.
 * This necessarily involves some highly specific knowledge of each game.
 */
 
#ifndef NES_AUTOSCORE_H
#define NES_AUTOSCORE_H

#include <stdint.h>

/* Per-game metadata, and a registry of those.
 ************************************************/

/* Format of persistable data in memory.
 * D5LEX10: Balloon Fight
 *   5 bytes little-endian, each represents one digit, and multiply by ten.
 *   Range: 0..999990
 * BCD14X3: Joust
 *   3 bytes BCD, with 3 zero bytes between each, total 7 bytes.
 *   Range: 0..999999
 * D7: Gun Nac
 *   7 bytes, one digit each.
 *   Range: 0..9999999
 * BCD3LE: Goonies
 *   3 bytes, little-endian BCD
 *   Range: 0..999999
 * D6: Kung Fu
 *   6 bytes, one digit each.
 *   Range: 0..999999
 * BCD3BE: Wrecking Crew
 *   3 bytes, big-endian BCD
 *   Range: 0..999999
 * D9: Galaxian (leading zeroes are 0x10)
 *   9 bytes, one digit each.
 *   Range: 0..999999999
 * MIXED82: Qix
 *   4 bytes header + 6*10 bytes ASCII (names) + 6*u24le
 * MIXED66: Krazy Kreatures
 *   6*11 bytes. 3-byte little-endian BCD score, 2 unknown, 6 like ascii +0x50 for name.
 * MIXED80: Paperboy
 *   10*8 bytes: 5 bytes big-endian digits, LSD dropped + 3 bytes initials (ascii?)
 * MIXED56: Tetris
 *   3*6 names + 30 mystery bytes + 3*3 bytes big-endian BCD
 */
#define NES_AUTOSCORE_FORMAT_D5LEX10  1
#define NES_AUTOSCORE_FORMAT_BCD14X3  2
#define NES_AUTOSCORE_FORMAT_D7       3
#define NES_AUTOSCORE_FORMAT_BCD3LE   4
#define NES_AUTOSCORE_FORMAT_D6       5
#define NES_AUTOSCORE_FORMAT_BCD3BE   6
#define NES_AUTOSCORE_FORMAT_D9       7
#define NES_AUTOSCORE_FORMAT_MIXED82  8
#define NES_AUTOSCORE_FORMAT_MIXED66  9
#define NES_AUTOSCORE_FORMAT_MIXED80 10
#define NES_AUTOSCORE_FORMAT_MIXED56 11

// Currently could be 82. Padding to 8%16 aligns records to 16 bytes, helpful for reading a hex dump.
#define NES_AUTOSCORE_LONGEST_SIZE 88

#define NES_AUTOSCORE_GAME_NAME_LIMIT 16
 
struct nes_autoscore_game {
  uint8_t cart_md5[16];
  char name[NES_AUTOSCORE_GAME_NAME_LIMIT]; // Commentary only. Beware, might not be terminated.
  uint16_t addr;
  uint16_t addr_shadow; // Write it here too, if <0x800
  uint8_t format;
  uint8_t initial[NES_AUTOSCORE_LONGEST_SIZE]; // Wait for it to reach this state before using.
};

const struct nes_autoscore_game *nes_autoscore_game_by_md5(const void *md5);

/* Live context.
 ******************************************************/
 
struct nes_autoscore_context {
  const struct nes_autoscore_game *game;
  uint8_t score[NES_AUTOSCORE_LONGEST_SIZE];
  int running; // Nonzero if the initial state has been matched.
  int score_size; // 0..NES_AUTOSCORE_LONGEST_SIZE; Derived from game->format.
};

void nes_autoscore_context_init(
  struct nes_autoscore_context *ctx,
  const void *rom_md5
);

void nes_autoscore_context_update(
  struct nes_autoscore_context *ctx,
  void *ram /* 2048 bytes. "RAM" in fceu. */
);

/* Database of scores.
 * We'll store these separate from the metadata.
 ********************************************************/

#define NES_AUTOSCORE_HASH_SIZE 16 /* md5 */
#define NES_AUTOSCORE_TIMESTAMP_SIZE 8 /* YYYYMMDDHHMMSSCC in BCD */
#define NES_AUTOSCORE_RECORD_SIZE ( \
  NES_AUTOSCORE_HASH_SIZE+ \
  NES_AUTOSCORE_TIMESTAMP_SIZE+ \
  NES_AUTOSCORE_LONGEST_SIZE \
)

/* Load once at startup.
 * Save as often as you like. It quickly noops if not dirty.
 * Dirty after changing a record.
 */
void nes_autoscore_db_load();
void nes_autoscore_db_save(int force);
void nes_autoscore_db_dirty();
int nes_autoscore_db_get_dirty();
void *nes_autoscore_db_get_record(const void *md5);
void nes_autoscore_db_set_record(const void *md5,const void *score,int scoresize); // add or update

static inline void *nes_autoscore_db_hash_from_record(void *record) {
  return record;
}
static inline void *nes_autoscore_db_timestamp_from_record(void *record) {
  if (!record) return 0;
  return (uint8_t*)record+NES_AUTOSCORE_HASH_SIZE;
}
static inline void *nes_autoscore_db_score_from_record(void *record) {
  if (!record) return 0;
  return (uint8_t*)record+NES_AUTOSCORE_HASH_SIZE+NES_AUTOSCORE_TIMESTAMP_SIZE;
}
static void *nes_autoscore_db_get_score(const void *md5) {
  return nes_autoscore_db_score_from_record(nes_autoscore_db_get_record(md5));
}

// Current timestamp.
void nes_autoscore_db_now(void *dst_8bytes);

#endif
