#include "mn_macioc_internal.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

struct mn_macioc mn_macioc={0};

/* Log to a text file. Will work even if the TTY is unset.
 */
#if 0 // I'll just leave this here in case we need it again...
static void mn_macioc_surelog(const char *fmt,...) {
  va_list vargs;
  va_start(vargs,fmt);
  char message[256];
  int messagec=vsnprintf(message,sizeof(message),fmt,vargs);
  if ((messagec<0)||(messagec>=sizeof(message))) {
    messagec=snprintf(message,sizeof(message),"(unable to generate message)");
  }
  int f=open("/Users/andy/proj/minten/surelog",O_WRONLY|O_APPEND|O_CREAT,0666);
  if (f<0) return;
  int err=write(f,message,messagec);
  close(f);
}

#define SURELOG(fmt,...) mn_macioc_surelog("%s:%d:%s: "fmt"\n",__FILE__,__LINE__,__func__,##__VA_ARGS__);
#endif

/* Reopen TTY.
 */
 
int mn_macioc_reopen_tty(const char *path) {
  int fd=open(path,O_RDWR);
  if (fd<0) return -1;
  dup2(fd,STDIN_FILENO);
  dup2(fd,STDOUT_FILENO);
  dup2(fd,STDERR_FILENO);
  close(fd);
  return 0;
}

/* First pass through argv.
 */

static int mn_macioc_argv_prerun(int argc,char **argv) {
  int argp;
  for (argp=1;argp<argc;argp++) {
    const char *k=argv[argp];
    if (!k) continue;

    if (k[0]!='-') {
      if (!mn_macioc.rompath) {
        mn_macioc.rompath=k;
        argv[argp]="";
        continue;
      }
    }
    
    if ((k[0]!='-')||(k[1]!='-')||!k[2]) continue;
    k+=2;
    int kc=0;
    while (k[kc]&&(k[kc]!='=')) kc++;
    const char *v=k+kc;
    int vc=0;
    if (v[0]=='=') {
      v++;
      while (v[vc]) vc++;
    }

    if ((kc==10)&&!memcmp(k,"reopen-tty",10)) {
      if (mn_macioc_reopen_tty(v)<0) return -1;
      argv[argp]="";

    } else if ((kc==5)&&!memcmp(k,"chdir",5)) {
      if (chdir(v)<0) return -1;
      argv[argp]="";

    }
  }
  return 0;
}

/* Main.
 */

int mn_macioc_main(int argc,char **argv,const struct mn_ioc_delegate *delegate) {

  if (mn_macioc.init) return 1;
  memset(&mn_macioc,0,sizeof(struct mn_macioc));
  mn_macioc.init=1;

  if (mn_macioc_argv_prerun(argc,argv)<0) {
    return -1;
  }
  
  if (delegate) memcpy(&mn_macioc.delegate,delegate,sizeof(struct mn_ioc_delegate));

  return NSApplicationMain(argc,(const char**)argv);
}

/* Simple termination.
 */
 
void mn_macioc_quit() {
  [NSApplication.sharedApplication terminate:nil];
  fprintf(stderr,"!!! [NSApplication.sharedApplication terminate:nil] did not terminate execution. Using exit() instead !!!\n");
  exit(1);
}

/* Abort.
 */
 
void mn_macioc_abort(const char *fmt,...) {
  if (fmt&&fmt[0]) {
    va_list vargs;
    va_start(vargs,fmt);
    char msg[256];
    int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
    if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
    fprintf(stderr,"macioc:FATAL: %.*s\n",msgc,msg);
  }
  mn_macioc_quit();
}

/* Callback triggers.
 */
 
int mn_macioc_call_init() {
  int result=(mn_macioc.delegate.init?mn_macioc.delegate.init(mn_macioc.rompath):0);
  mn_macioc.delegate.init=0; // Guarantee only one call.
  return result;
}

void mn_macioc_call_quit() {
  mn_macioc.terminate=1;
  if (mn_macioc.delegate.quit) {
    mn_macioc.delegate.quit();
    mn_macioc.delegate.quit=0; // Guarantee only one call.
  }
}

@implementation AKAppDelegate

/* Main loop.
 * This runs on a separate thread.
 */

-(void)mainLoop:(id)ignore {

  struct mn_clockassist clockassist={0};
  if (mn_clockassist_setup(&clockassist,MN_FRAME_RATE)<0) {
    mn_macioc_abort("Failed to initialize clock.");
  }
  
  while (1) {

    if (mn_macioc.terminate) break;

    /*
    if (mn_clockassist_update(&clockassist)<0) {
      mn_macioc_abort("Failed to update clock.");
    }
    */

    if (mn_macioc.terminate) break;

    if (mn_macioc.update_in_progress) {
      //mn_log(MACIOC,TRACE,"Dropping frame due to update still running.");
      continue;
    }

    /* With 'waitUntilDone:0', we will always be on manual timing.
     * I think that's OK. And window resizing is much more responsive this way.
     * Update:
     *   !!! After upgrading from 10.11 to 10.13, all the timing got fucked.
     *   Switching to 'waitUntilDone:1' seems to fix it.
     *   If the only problem that way in 10.11 was choppy window resizing, so be it.
     *   Resize seems OK with '1' and OS 10.13.
     */
    [self performSelectorOnMainThread:@selector(updateMain:) withObject:nil waitUntilDone:1];
  
  }
}

/* Route call from main loop.
 * This runs on the main thread.
 */

-(void)updateMain:(id)ignore {
  mn_macioc.update_in_progress=1;
  if (mn_macioc.delegate.update) {
    int err=mn_macioc.delegate.update();
    if (err<0) {
      mn_macioc_abort("Error %d updating application.",err);
    }
  }
  mn_macioc.update_in_progress=0;
}

/* Finish launching.
 * We fire the 'init' callback and launch an updater thread.
 */

-(void)applicationDidFinishLaunching:(NSNotification*)notification {
  
  int err=mn_macioc_call_init();
  if (err<0) {
    mn_macioc_abort("Initialization failed (%d). Aborting.",err);
  }

  [NSThread detachNewThreadSelector:@selector(mainLoop:) toTarget:self withObject:nil];
  
}

/* Termination.
 */

-(NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
  return NSTerminateNow;
}

-(BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
  return 1;
}

-(void)applicationWillTerminate:(NSNotification*)notification {
  mn_macioc_call_quit();
}

/* Receive system error.
 */

-(NSError*)application:(NSApplication*)application willPresentError:(NSError*)error {
  const char *message=error.localizedDescription.UTF8String;
  fprintf(stderr,"Cocoa error: %s\n",message);
  return error;
}

/* Change input focus.
 */

-(void)applicationDidBecomeActive:(NSNotification*)notification {
  if (mn_macioc.focus) return;
  mn_macioc.focus=1;
  /*TODO focus event
  if (mn_input_event_focus(1)<0) {
    mn_macioc_abort("Error in application focus callback.");
  }
  */
}

-(void)applicationDidResignActive:(NSNotification*)notification {
  if (!mn_macioc.focus) return;
  mn_macioc.focus=0;
  /*TODO focus event
  if (mn_input_event_focus(0)<0) {
    mn_macioc_abort("Error in application focus callback.");
  }
  */
}

/* Open with file.
 * BEWARE: """
 * If the user started up the application by double-clicking a file, 
 * the delegate receives the application:openFile: message before receiving applicationDidFinishLaunching:. 
 * """
 */

-(BOOL)application:(NSApplication*)application openFile:(NSString*)pathString {
  const char *path=[pathString UTF8String];
  if (!path||!path[0]) return 0;
  printf("application:openFile:%s\n",path);

  // If (delegate.init) is still set, it means we're not done starting up.
  // Record this path so it will be opened at that time.
  // This is a memory leak; I'm not worried about it.
  if (mn_macioc.delegate.init) {
    mn_macioc.rompath=strdup(path);
    return 1;
  }

  if (mn_macioc.delegate.open_file) {
    if (mn_macioc.delegate.open_file(path)<0) {
      mn_macioc_abort("Failed to open file: %s",path);
    }
    return 1;
  }
  
  return 0;
}

@end

/* Get user language.
 */
 
const char *mn_macioc_get_user_language(int index) {
  if (index<0) return 0;
  NSArray *languages=[NSLocale preferredLanguages];
  if (index>=[languages count]) return 0;
  return [[languages objectAtIndex:index] UTF8String];
}
