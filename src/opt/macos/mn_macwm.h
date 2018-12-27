/* mn_macwm.h
 * Bare-bones window manager interface for MacOS.
 * Link: -framework Cocoa -framework OpenGL
 */

#ifndef MN_MACWM_H
#define MN_MACWM_H

struct mn_macwm_delegate {
  int (*key)(int keycode,int value);
  int (*resize)(int w,int h);
  int (*receive_dragged_file)(const char *path);
};

int mn_macwm_init(
  int w,int h,
  int fullscreen,
  const char *title,
  const struct mn_macwm_delegate *delegate
);

int mn_macwm_connect_input();

void mn_macwm_quit();

int mn_macwm_show_cursor(int show);

// We only provide 'toggle', because it's easy on MacOS, and I think it's all we need.
int mn_macwm_toggle_fullscreen();

int mn_macwm_swap();

#endif
