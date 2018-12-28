#ifndef MN_MACWM_INTERNAL_H
#define MN_MACWM_INTERNAL_H

#include "mn_macwm.h"
#include <Cocoa/Cocoa.h>
#include <OpenGL/gl.h>

#ifndef GL_PROGRAM_POINT_SIZE
  #ifdef GL_PROGRAM_POINT_SIZE_EXT
    #define GL_PROGRAM_POINT_SIZE GL_PROGRAM_POINT_SIZE_EXT
  #elif defined(GL_VERTEX_PROGRAM_POINT_SIZE)
    #define GL_PROGRAM_POINT_SIZE GL_VERTEX_PROGRAM_POINT_SIZE
  #endif
#endif

/* Declaration of window class.
 */

@interface MNWindow : NSWindow <NSWindowDelegate> {
@public
  NSOpenGLContext *context;
  int cursor_visible;
  int w,h;
  int mousex,mousey;
  int modifiers;
  int fullscreen;
}

+(MNWindow*)newWithWidth:(int)width
  height:(int)height
  title:(const char*)title
  fullscreen:(int)fullscreen
;

-(int)beginFrame;
-(int)endFrame;

@end

/* Globals.
 */

#define MN_MACWM_KEYS_DOWN_LIMIT 8

extern struct mn_macwm {
  MNWindow *window;
  struct mn_macwm_delegate delegate;
  int keys_down[MN_MACWM_KEYS_DOWN_LIMIT];
} mn_macwm;

/* Miscellaneous.
 */

void mn_macwm_abort(const char *fmt,...);

int mn_macwm_decode_utf8(int *dst,const void *src,int srcc);
int mn_macwm_translate_codepoint(int src);
int mn_macwm_translate_keysym(int src);
int mn_macwm_translate_modifier(int src);
int mn_macwm_translate_mbtn(int src);

int mn_macwm_record_key_down(int key);
int mn_macwm_release_key_down(int key);
int mn_macwm_drop_all_keys();

int mn_macwm_cursor_within_window();

#endif
