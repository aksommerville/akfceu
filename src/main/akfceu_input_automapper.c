#include "akfceu_input_automapper.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Cleanup.
 */
 
void akfceu_input_automapper_cleanup(struct akfceu_input_automapper *mapper) {
  if (!mapper) return;
  if (mapper->fieldv) free(mapper->fieldv);
}

/* Search.
 */

static int akfceu_input_automapper_search(const struct akfceu_input_automapper *mapper,int btnid) {
  if (mapper->fieldc<1) return -1;
  if (btnid>mapper->fieldv[mapper->fieldc-1].btnid) return -mapper->fieldc-1;
  int lo=0,hi=mapper->fieldc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (btnid<mapper->fieldv[ck].btnid) hi=ck;
    else if (btnid>mapper->fieldv[ck].btnid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Insert.
 */

static struct akfceu_input_automapper_field *akfceu_input_automapper_insert(
  struct akfceu_input_automapper *mapper,int p,int btnid
) {
  if ((p<0)||(p>mapper->fieldc)) return 0;
  if (p&&(btnid<=mapper->fieldv[p-1].btnid)) return 0;
  if ((p<mapper->fieldc)&&(btnid>=mapper->fieldv[p].btnid)) return 0;

  if (mapper->fieldc>=mapper->fielda) {
    int na=mapper->fielda+16;
    if (na>INT_MAX/sizeof(struct akfceu_input_automapper_field)) return 0;
    void *nv=realloc(mapper->fieldv,sizeof(struct akfceu_input_automapper_field)*na);
    if (!nv) return 0;
    mapper->fieldv=nv;
    mapper->fielda=na;
  }

  struct akfceu_input_automapper_field *field=mapper->fieldv+p;
  memmove(field+1,field,sizeof(struct akfceu_input_automapper_field)*(mapper->fieldc-p));
  mapper->fieldc++;
  memset(field,0,sizeof(struct akfceu_input_automapper_field));
  field->btnid=btnid;

  return field;
}

/* Add button.
 */

int akfceu_input_automapper_add_button(
  struct akfceu_input_automapper *mapper,
  int btnid,int lo,int hi,int rest,int usage
) {
  if (!mapper) return -1;

  int p=akfceu_input_automapper_search(mapper,btnid);
  if (p>=0) {
    // Already have this button. If the new data is identical, just ignore it.
    const struct akfceu_input_automapper_field *field=mapper->fieldv+p;
    if ((lo==field->lo)&&(hi==field->hi)&&(usage==field->usage)) return 0;
    return -1;
  }
  p=-p-1;

  struct akfceu_input_automapper_field *field=akfceu_input_automapper_insert(mapper,p,btnid);
  if (!field) return -1;

  field->lo=lo;
  field->hi=hi;
  field->rest=rest;
  field->usage=usage;
  field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_UNSET;

  return 0;
}

/* Set all targets to UNSET.
 */

static void akfceu_input_automapper_reset_targets(struct akfceu_input_automapper *mapper) {
  struct akfceu_input_automapper_field *field=mapper->fieldv;
  int i=mapper->fieldc;
  for (;i-->0;field++) {
    field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_UNSET;
  }
}

/* Test completion.
 */

static int akfceu_input_automapper_is_complete(const struct akfceu_input_automapper *mapper) {
  int mask=0;
  const struct akfceu_input_automapper_field *field=mapper->fieldv;
  int i=mapper->fieldc;
  for (;i-->0;field++) {
    switch (field->target) {
      case AKFCEU_INPUT_AUTOMAPPER_TARGET_HAT:    mask|=0x0f; break;
      case AKFCEU_INPUT_AUTOMAPPER_TARGET_HORZ:   mask|=0x03; break;
      case AKFCEU_INPUT_AUTOMAPPER_TARGET_VERT:   mask|=0x0c; break;
      case AKFCEU_INPUT_AUTOMAPPER_TARGET_UP:     mask|=0x01; break;
      case AKFCEU_INPUT_AUTOMAPPER_TARGET_DOWN:   mask|=0x02; break;
      case AKFCEU_INPUT_AUTOMAPPER_TARGET_LEFT:   mask|=0x04; break;
      case AKFCEU_INPUT_AUTOMAPPER_TARGET_RIGHT:  mask|=0x08; break;
      case AKFCEU_INPUT_AUTOMAPPER_TARGET_A:      mask|=0x10; break;
      case AKFCEU_INPUT_AUTOMAPPER_TARGET_B:      mask|=0x20; break;
      case AKFCEU_INPUT_AUTOMAPPER_TARGET_SELECT: mask|=0x40; break;
      case AKFCEU_INPUT_AUTOMAPPER_TARGET_START:  mask|=0x80; break;
    }
  }
  return (mask==0xff)?1:0;
}

/* Set target of anything UNSET, if its meaning is perfectly clear.
 */

static void akfceu_input_automapper_assign_unambiguous_fields(struct akfceu_input_automapper *mapper) {
  struct akfceu_input_automapper_field *field=mapper->fieldv;
  int i=mapper->fieldc;
  for (;i-->0;field++) {
    if (field->target!=AKFCEU_INPUT_AUTOMAPPER_TARGET_UNSET) continue;

    /* If (lo..hi) doesn't express a valid range, we won't touch it.
     */
    if (field->lo>=field->hi) {
      field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_IGNORE;
      continue;
    }

    /* If the range allows for exactly 8 values and the resting state is outside them, it's a hat.
     */
    if ((field->lo+7==field->hi)&&((field->rest<field->lo)||(field->rest>field->hi))) {
      field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_HAT;
      continue;
    }

    switch (field->usage) {

      /* 0x0001: Generic Desktop Page. */
      case 0x00010030: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_HORZ; break; // X
      case 0x00010031: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_VERT; break; // Y
      case 0x00010033: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_HORZ; break; // Rx
      case 0x00010034: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_VERT; break; // Ry
      case 0x00010039: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_HAT; break; // Hat switch
      case 0x0001003d: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_START; break; // Start -- wouldn't it be cool if manufacturers actually used this?
      case 0x0001003e: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_SELECT; break; // Select
      case 0x00010040: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_HORZ; break; // Vx
      case 0x00010041: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_VERT; break; // Vy
      case 0x00010043: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_HORZ; break; // Vbrx
      case 0x00010044: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_VERT; break; // Vbry
      case 0x00010084: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_START; break; // System Context Menu
      case 0x00010085: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_START; break; // System Main Menu
      case 0x00010089: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_SELECT; break; // System Select
      case 0x0001008a: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_RIGHT; break; // System Right
      case 0x0001008b: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_LEFT; break; // System Left
      case 0x0001008c: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_UP; break; // System Up
      case 0x0001008d: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_DOWN; break; // System Down
      case 0x00010090: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_UP; break; // D-pad Up
      case 0x00010091: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_DOWN; break; // D-pad Down
      case 0x00010092: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_RIGHT; break; // D-pad Right
      case 0x00010093: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_LEFT; break; // D-pad Left

      /* 0x0005: Game Controls Page. These are, of course, never used by manufacturers. */
      case 0x00050021: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_HORZ; break; // Turn Right/Left
      case 0x00050022: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_VERT; break; // Pitch Forward/Backward
      case 0x00050023: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_HORZ; break; // Roll Right/Left
      case 0x00050024: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_HORZ; break; // Move Right/Left
      case 0x00050025: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_VERT; break; // Move Forward/Backward
      case 0x00050026: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_VERT; break; // Move Up/Down
      case 0x00050027: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_HORZ; break; // Lean Right/Left
      case 0x00050028: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_VERT; break; // Lean Forward/Backward

      /* 0x0007: Keyboard/Keypad Page. */
      case 0x00070004: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_LEFT; break; // A
      case 0x00070007: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_RIGHT; break; // D
      case 0x00070016: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_DOWN; break; // S
      case 0x0007001a: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_UP; break; // W
      case 0x0007001b: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_B; break; // X
      case 0x0007001d: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_A; break; // Z
      case 0x00070028: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_START; break; // Enter
      case 0x0007002b: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_SELECT; break; // Tab
      case 0x00070036: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_B; break; // Comma
      case 0x00070037: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_A; break; // Dot
      case 0x0007004f: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_RIGHT; break; // RightArrow
      case 0x00070050: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_LEFT; break; // LeftArrow
      case 0x00070051: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_DOWN; break; // DownArrow
      case 0x00070052: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_UP; break; // UpArrow

      /* 0x0009: Button Page. We'll consider the first 4 "unambiguous". */
      case 0x00090001: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_A; break;
      case 0x00090002: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_B; break;
      case 0x00090003: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_START; break;
      case 0x00090004: field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_SELECT; break;
      
    }

    /* If we selected a 2-way axis, ensure that it has a range of at least three.
     */
    if ((field->target==AKFCEU_INPUT_AUTOMAPPER_TARGET_HORZ)||(field->target==AKFCEU_INPUT_AUTOMAPPER_TARGET_VERT)) {
      if (field->lo>field->hi-2) {
        field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_UNSET;
      }
    }

    /* Likewise, if we selected HAT, the range must be at least eight.
     */
    if (field->target==AKFCEU_INPUT_AUTOMAPPER_TARGET_HAT) {
      if (field->lo>field->hi-7) {
        field->target=AKFCEU_INPUT_AUTOMAPPER_TARGET_UNSET;
      }
    }
  }
}

/* Anything that looks like a 2-way axis and is still UNSET, assign to one axis or the other.
 */

static int akfceu_input_automapper_select_less_used_axis(const struct akfceu_input_automapper *mapper) {
  int horzc=0,vertc=0,leftc=0,rightc=0,upc=0,downc=0;
  const struct akfceu_input_automapper_field *field=mapper->fieldv;
  int i=mapper->fieldc;
  for (;i-->0;field++) {
    switch (field->target) {
      case AKFCEU_INPUT_AUTOMAPPER_TARGET_HORZ: horzc++; break;
      case AKFCEU_INPUT_AUTOMAPPER_TARGET_VERT: vertc++; break;
      case AKFCEU_INPUT_AUTOMAPPER_TARGET_UP: upc++; break;
      case AKFCEU_INPUT_AUTOMAPPER_TARGET_DOWN: downc++; break;
      case AKFCEU_INPUT_AUTOMAPPER_TARGET_LEFT: leftc++; break;
      case AKFCEU_INPUT_AUTOMAPPER_TARGET_RIGHT: rightc++; break;
    }
  }
  if (upc&&downc) {
    vertc+=(upc<downc)?upc:downc;
  }
  if (leftc&&rightc) {
    horzc+=(leftc<rightc)?leftc:rightc;
  }
  if (vertc<horzc) return AKFCEU_INPUT_AUTOMAPPER_TARGET_VERT;
  return AKFCEU_INPUT_AUTOMAPPER_TARGET_HORZ;
}

static void akfceu_input_automapper_assign_ambiguous_axes(struct akfceu_input_automapper *mapper) {
  struct akfceu_input_automapper_field *field=mapper->fieldv;
  int i=mapper->fieldc;
  for (;i-->0;field++) {
    if (field->target!=AKFCEU_INPUT_AUTOMAPPER_TARGET_UNSET) continue;
    
    if ((field->rest>field->lo)&&(field->rest<field->hi)) {
      field->target=akfceu_input_automapper_select_less_used_axis(mapper);
      
    } else if ((field->lo<0)&&(field->hi>0)) {
      field->target=akfceu_input_automapper_select_less_used_axis(mapper);
    }
  }
}

/* Set target of all 2-state buttons if currently UNSET.
 * Only ambiguous ones reach this point (which should be very likely).
 * So we just assign them willy-nilly: A, B, Start, Select, A, B, ...
 * If no axes or hats were found, we also include (UP,DOWN,LEFT,RIGHT) in the mix.
 */

static void akfceu_input_automapper_count_fields_by_target(int *dst/*13*/,const struct akfceu_input_automapper *mapper) {
  const struct akfceu_input_automapper_field *field=mapper->fieldv;
  int i=mapper->fieldc;
  for (;i-->0;field++) {
    if ((field->target<0)||(field->target>=13)) continue;
    dst[field->target]++;
  }
}

static int akfceu_input_automapper_select_less_used_target(const int *countv/*13*/) {

  /* If the D-pad is not completely covered, assign to it.
   * This decision is probably wrong, but without it the device would be unusable.
   * We will only assign one button to each d-pad target.
   */
  if (!countv[AKFCEU_INPUT_AUTOMAPPER_TARGET_HAT]) {
    if (!countv[AKFCEU_INPUT_AUTOMAPPER_TARGET_VERT]) {
      if (!countv[AKFCEU_INPUT_AUTOMAPPER_TARGET_UP]) return AKFCEU_INPUT_AUTOMAPPER_TARGET_UP;
      if (!countv[AKFCEU_INPUT_AUTOMAPPER_TARGET_DOWN]) return AKFCEU_INPUT_AUTOMAPPER_TARGET_DOWN;
    }
    if (!countv[AKFCEU_INPUT_AUTOMAPPER_TARGET_HORZ]) {
      if (!countv[AKFCEU_INPUT_AUTOMAPPER_TARGET_LEFT]) return AKFCEU_INPUT_AUTOMAPPER_TARGET_LEFT;
      if (!countv[AKFCEU_INPUT_AUTOMAPPER_TARGET_RIGHT]) return AKFCEU_INPUT_AUTOMAPPER_TARGET_RIGHT;
    }
  }

  /* D-pad is covered, so split the rest between A, B, Select, and Start.
   */  
  int ac=countv[AKFCEU_INPUT_AUTOMAPPER_TARGET_A];
  int bc=countv[AKFCEU_INPUT_AUTOMAPPER_TARGET_B];
  int selectc=countv[AKFCEU_INPUT_AUTOMAPPER_TARGET_SELECT];
  int startc=countv[AKFCEU_INPUT_AUTOMAPPER_TARGET_START];
  if ((ac<=bc)&&(ac<=selectc)&&(ac<=startc)) return AKFCEU_INPUT_AUTOMAPPER_TARGET_A;
  if ((bc<=selectc)&&(bc<=startc)) return AKFCEU_INPUT_AUTOMAPPER_TARGET_B;
  if (startc<=selectc) return AKFCEU_INPUT_AUTOMAPPER_TARGET_START;
  return AKFCEU_INPUT_AUTOMAPPER_TARGET_SELECT;
}

static void akfceu_input_automapper_assign_twostate_buttons(struct akfceu_input_automapper *mapper) {

  int count_by_target[13]={0};
  akfceu_input_automapper_count_fields_by_target(count_by_target,mapper);

  struct akfceu_input_automapper_field *field=mapper->fieldv;
  int i=mapper->fieldc;
  for (;i-->0;field++) {
    if (field->target!=AKFCEU_INPUT_AUTOMAPPER_TARGET_UNSET) continue;
    if (field->hi==field->lo+1) {
      field->target=akfceu_input_automapper_select_less_used_target(count_by_target);
      count_by_target[field->target]++;
    }
  }
}

/* One last pass through the fields.
 * Anything we haven't assigned yet, try really hard to put it somewhere.
 * What lands here are probably analogue buttons.
 * This is exactly the same logic as 'twostate', but without the value range check.
 * It's important that we do them in separate passes; that creates a better distribution of the more preferable 2-state buttons.
 */

static void akfceu_input_automapper_last_chance(struct akfceu_input_automapper *mapper) {
  int count_by_target[13]={0};
  akfceu_input_automapper_count_fields_by_target(count_by_target,mapper);
  struct akfceu_input_automapper_field *field=mapper->fieldv;
  int i=mapper->fieldc;
  for (;i-->0;field++) {
    if (field->target!=AKFCEU_INPUT_AUTOMAPPER_TARGET_UNSET) continue;
    field->target=akfceu_input_automapper_select_less_used_target(count_by_target);
    count_by_target[field->target]++;
  }
}

/* Compile.
 */
 
int akfceu_input_automapper_compile(
  struct akfceu_input_automapper *mapper
) {
  if (!mapper) return -1;
  akfceu_input_automapper_reset_targets(mapper);
  akfceu_input_automapper_assign_unambiguous_fields(mapper);
  akfceu_input_automapper_assign_ambiguous_axes(mapper);
  akfceu_input_automapper_assign_twostate_buttons(mapper);
  akfceu_input_automapper_last_chance(mapper);
  return akfceu_input_automapper_is_complete(mapper);
}
