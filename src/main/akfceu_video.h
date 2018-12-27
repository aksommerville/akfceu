/* akfceu_video.h
 * Caller establishes an OpenGL context and sends us the framebuffer and palette.
 */

#ifndef AKFCEU_VIDEO_H
#define AKFCEU_VIDEO_H

int akfceu_video_init();
void akfceu_video_quit();

/* Render one frame to master.
 * (palette) is 768 bytes of (r1,g1,b1,...,r255,g255,b255).
 * (framebuffer) is packed bytes at the "input" size you declared previously.
 * Each byte of (framebuffer) is an index into (palette).
 */
int akfceu_video_render(const void *framebuffer,const void *palette);

/* Notify video of a change in the master output size.
 */
int akfceu_video_set_output_size(int w,int h);

/* Notify video of a change in the framebuffer's input size.
 * Typically you'll do this just once, at startup.
 * The framebuffers delivered to subsequent akfceu_video_render() calls must be this size.
 */
int akfceu_video_set_input_size(int w,int h);

#endif
