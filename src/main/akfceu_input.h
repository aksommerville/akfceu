/* akfceu_input.h
 * Manages the providers and delivers digested input state to FCEU.
 * We do the whole enchilada: Configuration, mapping, maintenance.
 */

#ifndef AKFCEU_INPUT_H
#define AKFCEU_INPUT_H

int akfceu_input_init();
void akfceu_input_quit();
int akfceu_input_update();

int akfceu_input_register_with_fceu();

#endif
