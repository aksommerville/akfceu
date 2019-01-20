/* ra_client.h
 * This is not part of Romassist.
 * Rather, it is a reference implementation of a Romassist IPC client.
 * Feel free to drop this whole 'ipcclient' directory into your C project.
 */

#ifndef RA_CLIENT_H
#define RA_CLIENT_H
#ifdef __cplusplus
  extern "C" {
#endif

typedef struct ra_client *romassist_t;

struct romassist_delegate {

  // Name of your program. This will be reported to users of the web interface.
  const char *client_name;

  // Comma-delimited list of platforms you support.
  // These strings come from enum 1. See <db/ra_db.h>.
  // Beware that the user can change his enums. I guess that's his own problem.
  const char *platforms;

  // Server wants a file launched. This is the important one!
  // (path) is NUL-terminated and we also provide the length, since we happen to have it.
  int (*launch_file)(romassist_t client,const char *path,int pathlen);

  // Response to your romassist_request_details().
  // (json) is *NOT* terminated.
  int (*details)(romassist_t client,const char *json,int jsonlen);

  // Some plain-text command, typically issued by the web user.
  // Examples: "quit", "restart", "pause", "screenshot"
  int (*command)(romassist_t client,const char *cmd,int cmdc);
  
};

/* Create client.
 * You can pass 0 to new for defaults (127.0.0.1:8111).
 * We only offer IPv4/TCP connections.
 */
romassist_t romassist_new(
  const void *ipv4addr,int port,
  const struct romassist_delegate *delegate
);

/* Delete client.
 * This closes the connection, if open.
 */
void romassist_del(romassist_t client);

/* Update. Polls socket, reads or writes, fires callbacks as warranted.
 * This will fail if the connection is lost.
 */
int romassist_update(romassist_t client);

/* Tell the server what we're playing.
 * Server expects you to call this after a successful 'launch_file' callback.
 */
int romassist_launched_file(romassist_t client,const char *path);

/* Ask for details specific to one file.
 * The response will come back through your 'details' callback.
 * Beware that if the file is unknown, or some error occurs, there will be no response.
 */
int romassist_request_details(romassist_t client,const char *path);

/* We do this on startup; you shouldn't need to.
 */
int romassist_send_hello(romassist_t client);

#ifdef __cplusplus
  }
#endif
#endif
