/* akfceu_input_map.h
 */

#ifndef AKFCEU_INPUT_MAP_H
#define AKFCEU_INPUT_MAP_H

#define AKFCEU_DSTBTNID_HAT 0xffff

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

/* Create maps in the global registry for the devices I own.
 * TODO Store this data in a file somewhere, make it useful for people who aren't me.
 */
int akfceu_define_andys_devices();

#endif
