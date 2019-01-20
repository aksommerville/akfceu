/* akfceu_input_automapper.h
 * For unknown input devices.
 * If you can declare the available buttons from a device, send them here and 
 * we'll guess a sensible mapping.
 *
 * - Initialize a new automapper to zero.
 * - Call akfceu_input_automapper_add_button() for each reportable field.
 * - Call akfceu_input_automapper_compile() after all buttons are declared.
 *
 */

#ifndef AKFCEU_INPUT_AUTOMAPPER_H
#define AKFCEU_INPUT_AUTOMAPPER_H

#define AKFCEU_INPUT_AUTOMAPPER_TARGET_UNSET      0 /* Only present before compile. */
#define AKFCEU_INPUT_AUTOMAPPER_TARGET_IGNORE     1 /* Don't use this field. */
#define AKFCEU_INPUT_AUTOMAPPER_TARGET_HAT        2 /* One input becomes the entire D-pad. */
#define AKFCEU_INPUT_AUTOMAPPER_TARGET_HORZ       3 /* One input becomes both LEFT and RIGHT. */
#define AKFCEU_INPUT_AUTOMAPPER_TARGET_VERT       4 /* One input becomes both UP and DOWN. */
#define AKFCEU_INPUT_AUTOMAPPER_TARGET_UP         5 /* 1:1 */
#define AKFCEU_INPUT_AUTOMAPPER_TARGET_DOWN       6 /* 1:1 */
#define AKFCEU_INPUT_AUTOMAPPER_TARGET_LEFT       7 /* 1:1 */
#define AKFCEU_INPUT_AUTOMAPPER_TARGET_RIGHT      8 /* 1:1 */
#define AKFCEU_INPUT_AUTOMAPPER_TARGET_A          9 /* 1:1 */
#define AKFCEU_INPUT_AUTOMAPPER_TARGET_B         10 /* 1:1 */
#define AKFCEU_INPUT_AUTOMAPPER_TARGET_SELECT    11 /* 1:1 */
#define AKFCEU_INPUT_AUTOMAPPER_TARGET_START     12 /* 1:1 */

struct akfceu_input_automapper_field {
  int btnid;
  int lo,hi,rest;
  int usage;
  int target;
};

struct akfceu_input_automapper {
  struct akfceu_input_automapper_field *fieldv;
  int fieldc,fielda;
};

void akfceu_input_automapper_cleanup(struct akfceu_input_automapper *mapper);

int akfceu_input_automapper_add_button(
  struct akfceu_input_automapper *mapper,
  int btnid,     // What are you going to call this button? Must be >=0 and unique.
  int lo,int hi, // The valid range of values, inclusive.
  int rest,      // Resting value, typically either (lo) or (lo+hi)/2.
  int usage      // HID usage page (0xffff0000) and code (0x0000ffff), if you have them.
);

/* Call when all buttons have been declared.
 * We will rewrite (target) of each field, in whatever arrangement seems most correct.
 * Returns >0 if all outputs are reached, 0 if incomplete, or <0 for real errors.
 */
int akfceu_input_automapper_compile(
  struct akfceu_input_automapper *mapper
);

#endif
