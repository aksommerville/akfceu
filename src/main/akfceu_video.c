#include "akfceu_video.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

//TODO: This includes <gl.h> for MacOS. How can we get the right header location for others?
#if USE_macos
  #include <OpenGL/gl.h>
#elif USE_linux
  #include <GL/gl.h>
#endif

/* Globals.
 */

static struct {
  int init;
  int inw,inh;
  int outw,outh;
  uint8_t *inrgba;
  GLuint texid;
  int dstx,dsty,dstw,dsth; // Output area, within (outw,outh)
  double fuzz_tolerance; // 0=stretch anything .. 1=don't stretch
  double aspect_correction; // Multiply output width by this.
} akfceu_video={0};

/* Initialize.
 */

int akfceu_video_init() {
  if (akfceu_video.init) return -1;
  memset(&akfceu_video,0,sizeof(akfceu_video));
 
  akfceu_video.init=1;
  akfceu_video.fuzz_tolerance=0.95;
  akfceu_video.aspect_correction=1.10;

  glEnable(GL_TEXTURE_2D);
  glGenTextures(1,&akfceu_video.texid);
  glBindTexture(GL_TEXTURE_2D,akfceu_video.texid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR); // TODO Let user select LINEAR or NEAREST
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 
  return 0;
}

/* Quit.
 */

void akfceu_video_quit() {
  if (akfceu_video.inrgba) free(akfceu_video.inrgba);
  glDeleteTextures(1,&akfceu_video.texid);
  memset(&akfceu_video,0,sizeof(akfceu_video));
}

/* Input or output size has changed, so recalculate the target rect (dstx et al).
 */

static void akfceu_video_recalculate_output_area() {
  if (akfceu_video.outw<1) return;
  if (akfceu_video.inw<1) return;

  int inwadj=akfceu_video.inw*akfceu_video.aspect_correction;

  /* The critical bit: If we were to use one output dimension exactly, what would the other be?
   * If either matches exactly (then they both should...), fill the screen.
   */
  int wforh=(inwadj*akfceu_video.outh)/akfceu_video.inh;
  int hforw=(akfceu_video.outw*akfceu_video.inh)/inwadj;
  if ((wforh==akfceu_video.outw)||(hforw==akfceu_video.outh)) {
    akfceu_video.dstx=0;
    akfceu_video.dsty=0;
    akfceu_video.dstw=akfceu_video.outw;
    akfceu_video.dsth=akfceu_video.outh;
    return;
  }

  /* The fuzz factor: If we are within a certain distance of the output's natural size, stretch it.
   */
  if (akfceu_video.fuzz_tolerance<1.0) {
    double xfuzz,yfuzz;
    xfuzz=(double)akfceu_video.outw/(double)wforh;
    if (xfuzz>1.0) xfuzz=1.0/xfuzz;
    yfuzz=(double)akfceu_video.outh/(double)hforw;
    if (yfuzz>1.0) yfuzz=1.0/yfuzz;
    double fuzz=(xfuzz<yfuzz)?xfuzz:yfuzz;
    if (fuzz>=akfceu_video.fuzz_tolerance) {
      akfceu_video.dstx=0;
      akfceu_video.dsty=0;
      akfceu_video.dstw=akfceu_video.outw;
      akfceu_video.dsth=akfceu_video.outh;
      return;
    }
  }

  /* We are not filling the screen.
   * Select whichever of (wforh,hforw) fits in its axis, then center in the output.
   */
  if (wforh<=akfceu_video.outw) {
    akfceu_video.dstw=wforh;
    akfceu_video.dsth=akfceu_video.outh;
  } else {
    akfceu_video.dstw=akfceu_video.outw;
    akfceu_video.dsth=hforw;
  }
  akfceu_video.dstx=(akfceu_video.outw>>1)-(akfceu_video.dstw>>1);
  akfceu_video.dsty=(akfceu_video.outh>>1)-(akfceu_video.dsth>>1);

}

/* Set output size.
 */

int akfceu_video_set_output_size(int w,int h) {
  if (!akfceu_video.init) return -1;
  if ((w<1)||(h<1)) return -1;
  if ((w==akfceu_video.outw)&&(h==akfceu_video.outh)) return 0;
 
  akfceu_video.outw=w;
  akfceu_video.outh=h;

  akfceu_video_recalculate_output_area();
 
  return 0;
}

/* Set input size.
 */

int akfceu_video_set_input_size(int w,int h) {
  if (!akfceu_video.init) return -1;
  if ((w<1)||(h<1)) return -1;
  if ((w==akfceu_video.inw)&&(h==akfceu_video.inh)) return 0;

  void *nv=malloc(w*h*4);
  if (!nv) return -1;
  if (akfceu_video.inrgba) free(akfceu_video.inrgba);
  akfceu_video.inrgba=nv;
 
  akfceu_video.inw=w;
  akfceu_video.inh=h;

  akfceu_video_recalculate_output_area();
 
  return 0;
}

/* Render.
 */

int akfceu_video_render(const void *framebuffer,const void *palette) {
  if (!akfceu_video.init) return -1;
  if (!framebuffer||!palette) return -1;
  if (!akfceu_video.inrgba) return -1;

  /* Apply palette, copy to (inrgba). */
  const uint8_t *srcp=framebuffer;
  uint8_t *dstp=akfceu_video.inrgba;
  int i=akfceu_video.inw*akfceu_video.inh;
  for (;i-->0;srcp++,dstp+=4) {
    const uint8_t *rgb=((const uint8_t*)palette)+(*srcp)*3;
    dstp[0]=rgb[0];
    dstp[1]=rgb[1];
    dstp[2]=rgb[2];
    dstp[3]=0xff;
  }

  /* Upload pixels to texture. */
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D,akfceu_video.texid);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,akfceu_video.inw,akfceu_video.inh,0,GL_RGBA,GL_UNSIGNED_BYTE,akfceu_video.inrgba);

  /* Clear output if necessary. */
  glViewport(0,0,akfceu_video.outw,akfceu_video.outh);
  if (
    (akfceu_video.dstx>0)||(akfceu_video.dsty>0)||
    (akfceu_video.dstx+akfceu_video.dstw<akfceu_video.outw)||
    (akfceu_video.dsty+akfceu_video.dsth<akfceu_video.outh)
  ) {
    glClearColor(0.0,0.0,0.0,1.0);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  /* Draw a single textured quad.
   * TODO This is GL 1. Should we be using 2? (eg shaders).
   */
  glLoadIdentity();
  glOrtho(0,akfceu_video.outw,akfceu_video.outh,0,0,1);
  glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0.0f,0.0f);
    glVertex2i(akfceu_video.dstx,akfceu_video.dsty);
    glTexCoord2f(0.0f,1.0f);
    glVertex2i(akfceu_video.dstx,akfceu_video.dsty+akfceu_video.dsth);
    glTexCoord2f(1.0f,0.0f);
    glVertex2i(akfceu_video.dstx+akfceu_video.dstw,akfceu_video.dsty);
    glTexCoord2f(1.0f,1.0f);
    glVertex2i(akfceu_video.dstx+akfceu_video.dstw,akfceu_video.dsty+akfceu_video.dsth);
  glEnd();
 
  return 0;
}
