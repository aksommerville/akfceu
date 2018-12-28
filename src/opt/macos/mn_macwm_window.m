#include "mn_macwm_internal.h"

@implementation MNWindow

/* Lifecycle.
 */

-(void)dealloc {
  if (self==mn_macwm.window) {
    mn_macwm.window=0;
  }
  [super dealloc];
}

-(id)initWithContentRect:(NSRect)contentRect styleMask:(NSWindowStyleMask)aStyle backing:(NSBackingStoreType)bufferingType defer:(BOOL)flag screen:(NSScreen*)screen {

  if (mn_macwm.window) {
    return 0;
  }

  if (!(self=[super initWithContentRect:contentRect styleMask:aStyle backing:bufferingType defer:flag screen:screen])) {
    return 0;
  }
 
  mn_macwm.window=self;
  self.delegate=self;

  [self makeKeyAndOrderFront:nil];

  [self registerForDraggedTypes:@[NSPasteboardTypeFileURL,NSPasteboardTypeURL,NSPasteboardTypeString]];

  return self;
}

/* Preferred public initializer.
 */

+(MNWindow*)newWithWidth:(int)width
  height:(int)height
  title:(const char*)title
  fullscreen:(int)fullscreen
{

  NSScreen *screen=[NSScreen mainScreen];
  if (!screen) {
    return 0;
  }
  int screenw=screen.frame.size.width;
  int screenh=screen.frame.size.height;

  if (width>screenw) width=screenw;
  if (width<0) width=0;
  if (height>screenh) height=screenh;
  if (height<0) height=0;

  NSRect bounds=NSMakeRect((screenw>>1)-(width>>1),(screenh>>1)-(height>>1),width,height);

  NSUInteger styleMask=
    NSWindowStyleMaskTitled|
    NSWindowStyleMaskClosable|
    NSWindowStyleMaskMiniaturizable|
    NSWindowStyleMaskResizable|
  0;
 
  MNWindow *window=[[MNWindow alloc]
    initWithContentRect:bounds
    styleMask:styleMask
    backing:NSBackingStoreBuffered
    defer:0
    screen:0
  ];
  if (!window) return 0;

  window->w=width;
  window->h=height;
 
  window.releasedWhenClosed=0;
  window.acceptsMouseMovedEvents=TRUE;
  window->cursor_visible=1;

  if (title) {
    window.title=[NSString stringWithUTF8String:title];
  }

  if ([window setupOpenGL]<0) {
    [window release];
    return 0;
  }

  if (fullscreen) {
    [window toggleFullScreen:window];
  }
 
  return window;
}

/* Create OpenGL context.
 */

-(int)setupOpenGL {

  NSOpenGLPixelFormatAttribute attrs[]={
    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFAColorSize,8,
    NSOpenGLPFAAlphaSize,0,
    NSOpenGLPFADepthSize,0,
    0
  };
  NSOpenGLPixelFormat *pixelFormat=[[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
  NSRect viewRect=NSMakeRect(0.0,0.0,self.frame.size.width,self.frame.size.height);
  NSOpenGLView *fullScreenView=[[NSOpenGLView alloc] initWithFrame:viewRect pixelFormat:pixelFormat];
  [self setContentView:fullScreenView];
  [pixelFormat release];
  [fullScreenView release];

  context=((NSOpenGLView*)self.contentView).openGLContext;
  if (!context) return -1;
  [context retain];
  [context setView:self.contentView];
 
  [context makeCurrentContext];
  glEnable(GL_POINT_SPRITE);
  glEnable(GL_PROGRAM_POINT_SIZE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

  /* Disable blocking on buffer swap. */
  GLint v=0;
  [context setValues:&v forParameter:NSOpenGLCPSwapInterval];

  return 0;
}

/* Frame control.
 */

-(int)beginFrame {
  [context makeCurrentContext];
  return 0;
}

-(int)endFrame {
  [context flushBuffer];
  return 0;
}

/* Receive keyboard events.
 */

-(void)keyDown:(NSEvent*)event {

  if (event.modifierFlags&NSEventModifierFlagCommand) {
    return;
  }

  int state=event.ARepeat?2:1;
  int key=mn_macwm_translate_keysym(event.keyCode);
  if (!key) return;

  mn_macwm_record_key_down(event.keyCode);

  if (mn_macwm.delegate.key) {
    if (mn_macwm.delegate.key(key,state)<0) {
      mn_macwm_abort("Failure in key event handler.");
    }
  }
}

-(void)keyUp:(NSEvent*)event {
  mn_macwm_release_key_down(event.keyCode);
  int key=mn_macwm_translate_keysym(event.keyCode);
  if (!key) return;
  if (mn_macwm.delegate.key) {
    if (mn_macwm.delegate.key(key,0)<0) {
      mn_macwm_abort("Failure in key event handler.");
    }
  }
}

-(void)flagsChanged:(NSEvent*)event {
  int nmodifiers=(int)event.modifierFlags;
  if (nmodifiers!=modifiers) {
    int omodifiers=modifiers;
    modifiers=nmodifiers;
    if (mn_macwm.delegate.key) {
      int mask=1; for (;mask;mask<<=1) {
   
        if ((nmodifiers&mask)&&!(omodifiers&mask)) {
          int key=mn_macwm_translate_modifier(mask);
          if (key) {
            if (mn_macwm.delegate.key(key,1)<0) mn_macwm_abort("Failure in key event handler.");
          }

        } else if (!(nmodifiers&mask)&&(omodifiers&mask)) {
          int key=mn_macwm_translate_modifier(mask);
          if (key) {
            if (mn_macwm.delegate.key(key,0)<0) mn_macwm_abort("Failure in key event handler.");
          }
        }
      }
    }
  }
}

/* Mouse events.
 */

static void mn_macwm_event_mouse_motion(NSPoint loc) {

  int wasin=mn_macwm_cursor_within_window();
  mn_macwm.window->mousex=loc.x;
  mn_macwm.window->mousey=mn_macwm.window->h-loc.y;
  int nowin=mn_macwm_cursor_within_window();

  if (!mn_macwm.window->cursor_visible) {
    if (wasin&&!nowin) {
      [NSCursor unhide];
    } else if (!wasin&&nowin) {
      [NSCursor hide];
    }
  }
}

-(void)mouseMoved:(NSEvent*)event { mn_macwm_event_mouse_motion(event.locationInWindow); }
-(void)mouseDragged:(NSEvent*)event { mn_macwm_event_mouse_motion(event.locationInWindow); }
-(void)rightMouseDragged:(NSEvent*)event { mn_macwm_event_mouse_motion(event.locationInWindow); }
-(void)otherMouseDragged:(NSEvent*)event { mn_macwm_event_mouse_motion(event.locationInWindow); }

/* Resize events.
 */

-(void)reportResize:(NSSize)size {

  /* Don't report redundant events (they are sent) */
  int nw=size.width;
  int nh=size.height;
  if ((w==nw)&&(h==nh)) return;
  w=nw;
  h=nh;
  if (mn_macwm.delegate.resize) {
    if (mn_macwm.delegate.resize(w,h)<0) {
      mn_macwm_abort("Failure in resize handler. (%d,%d)",w,h);
    }
  }
}

-(NSSize)windowWillResize:(NSWindow*)sender toSize:(NSSize)frameSize {
  [self reportResize:[self contentRectForFrameRect:(NSRect){.size=frameSize}].size];
  return frameSize;
}

-(NSSize)window:(NSWindow*)window willUseFullScreenContentSize:(NSSize)proposedSize {
  [self reportResize:proposedSize];
  return proposedSize;
}

-(void)windowWillStartLiveResize:(NSNotification*)note {
}

-(void)windowDidEndLiveResize:(NSNotification*)note {
}

-(void)windowDidEnterFullScreen:(NSNotification*)note {
  MNWindow *window=(MNWindow*)note.object;
  if ([window isKindOfClass:MNWindow.class]) {
    window->fullscreen=1;
  }
  if (mn_macwm_drop_all_keys()<0) {
    mn_macwm_abort("Failure in key release handler.");
  }
}

-(void)windowDidExitFullScreen:(NSNotification*)note {
  MNWindow *window=(MNWindow*)note.object;
  if ([window isKindOfClass:MNWindow.class]) {
    window->fullscreen=0;
  }
  if (mn_macwm_drop_all_keys()<0) {
    mn_macwm_abort("Failure in key release handler.");
  }
}

/* Dragon droppings
 */

-(NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
  if (!mn_macwm.delegate.receive_dragged_file) {
    return NSDragOperationNone;
  }
  return NSDragOperationCopy;
}

-(BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
  if (!mn_macwm.delegate.receive_dragged_file) return 0;
  const char *path=[[[NSURL URLFromPasteboard:[sender draggingPasteboard]] path] UTF8String];
  if (!path||!path[0]) return 0;
  if (mn_macwm.delegate.receive_dragged_file(path)<0) {
    mn_macwm_abort("Failed to process dragged file: %s",path);
  }
  return 1;
}

@end
