#include "mn_macwm_internal.h"
#include <stdio.h>

struct mn_macwm mn_macwm={0};

/* Init.
 */

int mn_macwm_init(
  int w,int h,
  int fullscreen,
  const char *title,
  const struct mn_macwm_delegate *delegate
) {
  if (mn_macwm.window) {
    return -1;
  }
  memset(&mn_macwm,0,sizeof(struct mn_macwm));

  if (delegate) memcpy(&mn_macwm.delegate,delegate,sizeof(struct mn_macwm_delegate));

  if (!(mn_macwm.window=[MNWindow newWithWidth:w height:h title:title fullscreen:fullscreen])) {
    return -1;
  }

  return 0;
}

/* Quit.
 */

void mn_macwm_quit() {
  [mn_macwm.window release];

  memset(&mn_macwm,0,sizeof(struct mn_macwm));
}

/* Abort.
 */
 
void mn_macwm_abort(const char *fmt,...) {
  if (fmt&&fmt[0]) {
    va_list vargs;
    va_start(vargs,fmt);
    char msg[256];
    int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
    if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
    fprintf(stderr,"macwm:FATAL: %.*s\n",msgc,msg);
  }
  [NSApplication.sharedApplication terminate:nil];
}

/* Test cursor within window, based on last reported position.
 */

int mn_macwm_cursor_within_window() {
  if (!mn_macwm.window) return 0;
  if (mn_macwm.window->mousex<0) return 0;
  if (mn_macwm.window->mousey<0) return 0;
  if (mn_macwm.window->mousex>=mn_macwm.window->w) return 0;
  if (mn_macwm.window->mousey>=mn_macwm.window->h) return 0;
  return 1;
}

/* Show or hide cursor.
 */

int mn_macwm_show_cursor(int show) {
  if (!mn_macwm.window) return -1;
  if (show) {
    if (mn_macwm.window->cursor_visible) return 0;
    if (mn_macwm_cursor_within_window()) {
      [NSCursor unhide];
    }
    mn_macwm.window->cursor_visible=1;
  } else {
    if (!mn_macwm.window->cursor_visible) return 0;
    if (mn_macwm_cursor_within_window()) {
      [NSCursor hide];
    }
    mn_macwm.window->cursor_visible=0;
  }
  return 0;
}

/* Toggle fullscreen.
 */

int mn_macwm_toggle_fullscreen() {

  [mn_macwm.window toggleFullScreen:mn_macwm.window];

  // Take it on faith that the state will change:
  return mn_macwm.window->fullscreen^1;
}

/* OpenGL frame control.
 */

int mn_macwm_swap() {
  if (!mn_macwm.window) return -1;
  return [mn_macwm.window endFrame];
}

/* Ridiculous hack to ensure key-up events.
 * Unfortunately, during a fullscreen transition we do not receive keyUp events.
 * If the main input is a keyboard, and the user strikes a key to toggle fullscreen,
 * odds are very strong that they will release that key during the transition.
 * We record every key currently held, and forcibly release them after a fullscreen transition.
 */
 
int mn_macwm_record_key_down(int key) {
  int p=-1;
  int i=MN_MACWM_KEYS_DOWN_LIMIT; while (i-->0) {
    if (mn_macwm.keys_down[i]==key) return 0;
    if (!mn_macwm.keys_down[i]) p=i;
  }
  if (p>=0) {
    mn_macwm.keys_down[p]=key;
  }
  return 0;
}

int mn_macwm_release_key_down(int key) {
  int i=MN_MACWM_KEYS_DOWN_LIMIT; while (i-->0) {
    if (mn_macwm.keys_down[i]==key) {
      mn_macwm.keys_down[i]=0;
    }
  }
  return 0;
}

int mn_macwm_drop_all_keys() {
  int i=MN_MACWM_KEYS_DOWN_LIMIT; while (i-->0) {
    if (mn_macwm.keys_down[i]) {
      if (mn_macwm.delegate.key) {
        int key=mn_macwm_translate_keysym(mn_macwm.keys_down[i]);
        if (key) {
          if (mn_macwm.delegate.key(key,0)<0) return -1;
        }
      }
      mn_macwm.keys_down[i]=0;
    }
  }
  return 0;
}
