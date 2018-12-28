/* mn_macioc.h
 */

#ifndef MN_MACIOC_H
#define MN_MACIOC_H

struct mn_ioc_delegate {

  /* Called exactly once, as the application starts up.
   * If (rompath) is present, it was provided on the command line, which is odd on MacOS.
   */
  int (*init)(const char *rompath);

  /* Called exactly once, as the application shuts down.
   */
  void (*quit)();

  /* Called routinely, about 60 times per second.
   */
  int (*update)();

  /* Called when we receive a request from the OS to open a file.
   * If we are launched that way, it is delivered to (init), not here.
   */
  int (*open_file)(const char *path);
};

int mn_macioc_main(int argc,char **argv,const struct mn_ioc_delegate *delegate);

// Force application to quit. Use only in emergencies.
void mn_macioc_quit();
void mn_macioc_abort(const char *fmt,...);

/* C wrapper around [NSLocale preferredLanguages].
 * Most preferred is at index zero; call with increasing index until it returns NULL for the full list.
 * Examples: "en-US", "fr-FR", "ru-US", "zh-Hans-US". This is an IETF or ISO format, I forget the name.
 * Anyway, the first two letters should always be an ISO 639-1 code.
 */
const char *mn_macioc_get_user_language(int index);

#endif
