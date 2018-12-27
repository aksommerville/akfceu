/* mn_macaudio.h
 * Simple API to produce an audio output stream for MacOS.
 * Link: -framework AudioUnit
 */

#ifndef MN_MACAUDIOH
#define MN_MACAUDIOH

#include <stdint.h>

int mn_macaudio_init(int rate,int chanc,void (*cb)(int16_t *dst,int dstc));
void mn_macaudio_quit();

int mn_macaudio_lock();
int mn_macaudio_unlock();

#endif
