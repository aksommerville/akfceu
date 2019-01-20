/* akfceu_input_map.h
 */

#ifndef AKFCEU_INPUT_MAP_H
#define AKFCEU_INPUT_MAP_H

#define AKFCEU_DSTBTNID_HAT  0xffff
#define AKFCEU_DSTBTNID_HORZ 0xfffe
#define AKFCEU_DSTBTNID_VERT 0xfffd

struct akfceu_button_map {
  int srcbtnid;
  int srclo,srchi;
  int dstbtnid;
};

struct akfceu_input_map {
  int vid,pid; // read only
  struct akfceu_button_map *buttonv;
  int buttonc,buttona;
};

/* We keep a global registry of maps.
 * 'find' returns an exact match if existing.
 * 'new' asserts that it not exist, and creates it fresh.
 * In both cases, the map is globally registered and a WEAK reference is returned.
 */
struct akfceu_input_map *akfceu_input_map_find(int vid,int pid);
struct akfceu_input_map *akfceu_input_map_new(int vid,int pid);

/* Duplicate srcbtnid are permitted (eg for 2-way axes).
 * Searching finds the lowest index always.
 */
int akfceu_input_map_search_srcbtnid(const struct akfceu_input_map *map,int srcbtnid);
int akfceu_input_map_add_button(struct akfceu_input_map *map,int p,int srcbtnid,int srclo,int srchi,int dstbtnid);
int akfceu_input_map_add_hat(struct akfceu_input_map *map,int p,int srcbtnid,int validlo,int validhi); // ugh, why do these things exist?

/* Examine maps and return the affected output state for a given input state change.
 */
int akfceu_input_map_update(
  const struct akfceu_input_map *map,
  int srcbtnid,int srcvalue,
  int (*cb)(int dstbtnid,int value,void *userdata),
  void *userdata
);

/* Nonzero if this map can output all 8 buttons.
 */
int akfceu_input_map_is_suitable_for_gamepad(const struct akfceu_input_map *map);

/* Read a config file (FCEUMKF_CONFIG) and register any maps declared in it.
 * Quietly do nothing if the file can't be opened.
 * We read the file line-wise, discarding anything after '#', spurious space, and empty lines.
 *
 * Unknown data is quietly ignored until we reach a device block:
 *   device VID PID
 *
 * Within the device block, each line is a single mapping:
 *   SRCBTNID DSTBTNNAME LO HI
 * DSTBTNNAME is one of: UP DOWN LEFT RIGHT A B START SELECT
 * May also be an aggregate: HORZ VERT DPAD
 * (LO,HI) may be omitted for standard buttons, for a range of 1..INT_MAX.
 * Thresholds are required for aggregate outputs.

 * Device block ends with a line reading exactly:
 *   end device
 */
int akfceu_input_map_load_configuration(const char *path);

int akfceu_input_map_dstbtnid_eval(const char *src,int srcc);

#endif
