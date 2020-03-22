#include "akfceu_input.h"
#include "akfceu_input_map.h"
#include "akfceu_input_automapper.h"
#include "fceu/driver.h"
#include "fceu/fceu.h"
#include "fceu/general.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#if USE_macos
  #include "opt/macos/mn_machid.h"
  #include "opt/macos/mn_macwm.h"
#else
  #error "No input backend."
#endif

/* Globals.
 */

struct akfceu_input_assignment {
  int devid; // From backend, must be unique.
  int playerid; // 1,2,3,4
  int type; // SI_GAMEPAD; we may support others in the future
  struct akfceu_input_map *map; // WEAK, OPTIONAL
  int ignore_for_mapping; // For system keyboard. Map it but also give the next device to player one.
};

static struct {
  int init;
  struct akfceu_input_assignment *assignmentv;
  int assignmentc,assignmenta;
  uint32_t gamepad_state;
} akfceu_input={0};

/* Assignment list primitives.
 */

static int akfceu_input_assignmentv_search(int devid) {
  int lo=0,hi=akfceu_input.assignmentc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (devid<akfceu_input.assignmentv[ck].devid) hi=ck;
    else if (devid>akfceu_input.assignmentv[ck].devid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int akfceu_input_assignmentv_require() {
  if (akfceu_input.assignmentc<akfceu_input.assignmenta) return 0;
  int na=akfceu_input.assignmenta+8;
  if (na>INT_MAX/sizeof(struct akfceu_input_assignment)) return -1;
  void *nv=realloc(akfceu_input.assignmentv,sizeof(struct akfceu_input_assignment)*na);
  if (!nv) return -1;
  akfceu_input.assignmentv=nv;
  akfceu_input.assignmenta=na;
  return 0;
}

static void akfceu_input_assignmentv_remove(int p) {
  if ((p<0)||(p>=akfceu_input.assignmentc)) return;
  akfceu_input.assignmentc--;
  memmove(akfceu_input.assignmentv+p,akfceu_input.assignmentv+p+1,sizeof(struct akfceu_input_assignment)*(akfceu_input.assignmentc-p));
}

static struct akfceu_input_assignment *akfceu_input_assignment_new(int p,int devid) {
  if ((p<0)||(p>akfceu_input.assignmentc)) {
    p=akfceu_input_assignmentv_search(devid);
    if (p>=0) return 0;
    p=-p-1;
  }
  if (p&&(devid<=akfceu_input.assignmentv[p-1].devid)) return 0;
  if ((p<akfceu_input.assignmentc)&&(devid>=akfceu_input.assignmentv[p].devid)) return 0;
  if (akfceu_input_assignmentv_require()<0) return 0;
  struct akfceu_input_assignment *assignment=akfceu_input.assignmentv+p;
  memmove(assignment+1,assignment,sizeof(struct akfceu_input_assignment)*(akfceu_input.assignmentc-p));
  akfceu_input.assignmentc++;
  memset(assignment,0,sizeof(struct akfceu_input_assignment));
  assignment->devid=devid;
  return assignment;
}

/* Drop input for a given virtual device.
 */

static void akfceu_input_drop_state(int type,int playerid) {
  if (type==SI_GAMEPAD) {
    switch (playerid) {
      case 1: akfceu_input.gamepad_state&=0xffffff00; break;
      case 2: akfceu_input.gamepad_state&=0xffff00ff; break;
      case 3: akfceu_input.gamepad_state&=0xff00ffff; break;
      case 4: akfceu_input.gamepad_state&=0x00ffffff; break;
    }
  }
}

/* Populate default mapping for some device.
 */

static int akfceu_input_populate_default_map(struct akfceu_input_map *map,int devid,int vid,int pid) {
  struct akfceu_input_automapper mapper={0};

  #if USE_macos
    const char *mfrname=mn_machid_dev_get_manufacturer_name(devid);
    const char *prodname=mn_machid_dev_get_product_name(devid);
    fprintf(stderr,"Generating map for device \"%s %s\"...\n",mfrname,prodname);
    int p=0; while (1) {
      int btnid,usage,lo,hi,value;
      if (mn_machid_dev_get_button_info(&btnid,&usage,&lo,&hi,&value,devid,p)<0) break;
      if (akfceu_input_automapper_add_button(&mapper,btnid,lo,hi,value,usage)<0) {
        akfceu_input_automapper_cleanup(&mapper);
        return -1;
      }
      p++;
    }

  #else
    fprintf(stderr,"Input driver does not support generic device description... Sorry, can't map this device.\n");
    akfceu_input_automapper_cleanup(&mapper);
    return 0;
  #endif
  
  char *inputcfgpath=FCEU_MakeFName(FCEUMKF_CONFIG,0,0);
  if (!inputcfgpath) {
    akfceu_input_automapper_cleanup(&mapper);
    return -1;
  }

  /* Now that the automapper is populated, let it do its magic.
   * Then we populate the map and also dump text the user can use to permanently configure it like this.
   */
  int result=akfceu_input_automapper_compile(&mapper);
  if (!result) {
    fprintf(stderr,"WARNING: Input map is incomplete, this device is not usable as-is.\n");
  }
  if (result>=0) {
    fprintf(stderr,"----- Add the following text to %s to make this input device configuration permanent. -----\n",inputcfgpath);
    fprintf(stderr,"device 0x%04x 0x%04x\n",vid,pid);
    const struct akfceu_input_automapper_field *field=mapper.fieldv;
    int i=mapper.fieldc;
    for (;i-->0;field++) {

      #define FAIL { \
        akfceu_input_automapper_cleanup(&mapper); \
        free(inputcfgpath); \
        return -1; \
      }
      const char *btnname;
      int dstbtnid;
      switch (field->target) {
      
        case AKFCEU_INPUT_AUTOMAPPER_TARGET_HAT: {
            fprintf(stderr,"  %d DPAD %d %d\n",field->btnid,field->lo,field->hi);
            if (akfceu_input_map_add_hat(map,-1,field->btnid,field->lo,field->hi)<0) FAIL
          } break;
          
        case AKFCEU_INPUT_AUTOMAPPER_TARGET_HORZ: {
            int threshlo,threshhi;
            int mid=(field->lo+field->hi)>>1;
            threshlo=(field->lo+mid)>>1;
            threshhi=(mid+field->hi)>>1;
            if (threshlo>=threshhi-1) {
              threshlo=field->lo;
              threshhi=field->hi;
            }
            fprintf(stderr,"  %d HORZ %d %d\n",field->btnid,threshlo,threshhi);
            if (akfceu_input_map_add_button(map,-1,field->btnid,INT_MIN,threshlo,JOY_LEFT)<0) FAIL
            if (akfceu_input_map_add_button(map,-1,field->btnid,threshhi,INT_MAX,JOY_RIGHT)<0) FAIL
          } break;
          
        case AKFCEU_INPUT_AUTOMAPPER_TARGET_VERT: {
            int threshlo,threshhi;
            int mid=(field->lo+field->hi)>>1;
            threshlo=(field->lo+mid)>>1;
            threshhi=(mid+field->hi)>>1;
            if (threshlo>=threshhi-1) {
              threshlo=field->lo;
              threshhi=field->hi;
            }
            fprintf(stderr,"  %d VERT %d %d\n",field->btnid,threshlo,threshhi);
            if (akfceu_input_map_add_button(map,-1,field->btnid,INT_MIN,threshlo,JOY_UP)<0) FAIL
            if (akfceu_input_map_add_button(map,-1,field->btnid,threshhi,INT_MAX,JOY_DOWN)<0) FAIL
          } break;

        case AKFCEU_INPUT_AUTOMAPPER_TARGET_UP: btnname="UP"; dstbtnid=JOY_UP; goto _two_state_;
        case AKFCEU_INPUT_AUTOMAPPER_TARGET_DOWN: btnname="DOWN"; dstbtnid=JOY_DOWN; goto _two_state_;
        case AKFCEU_INPUT_AUTOMAPPER_TARGET_LEFT: btnname="LEFT"; dstbtnid=JOY_LEFT; goto _two_state_;
        case AKFCEU_INPUT_AUTOMAPPER_TARGET_RIGHT: btnname="RIGHT"; dstbtnid=JOY_RIGHT; goto _two_state_;
        case AKFCEU_INPUT_AUTOMAPPER_TARGET_A: btnname="A"; dstbtnid=JOY_A; goto _two_state_;
        case AKFCEU_INPUT_AUTOMAPPER_TARGET_B: btnname="B"; dstbtnid=JOY_B; goto _two_state_;
        case AKFCEU_INPUT_AUTOMAPPER_TARGET_START: btnname="START"; dstbtnid=JOY_START; goto _two_state_;
        case AKFCEU_INPUT_AUTOMAPPER_TARGET_SELECT: btnname="SELECT"; dstbtnid=JOY_SELECT; goto _two_state_;
        _two_state_: {
            if ((field->lo!=0)||(field->hi<1)||(field->hi>2)) {
              int thresh=(field->lo+field->hi)>>1;
              fprintf(stderr,"  %d %s %d %d\n",field->btnid,btnname,thresh,field->hi);
              if (akfceu_input_map_add_button(map,-1,field->btnid,thresh,field->hi,dstbtnid)<0) return -1;
            } else {
              fprintf(stderr,"  %d %s\n",field->btnid,btnname);
              if (akfceu_input_map_add_button(map,-1,field->btnid,1,INT_MAX,dstbtnid)<0) return -1;
            }
          } break;
        
        default: fprintf(stderr,"  # %d unused, usage=0x%08x, range=(%d..%d)\n",field->btnid,field->usage,field->lo,field->hi); break;
      }
      #undef FAIL

    }
    fprintf(stderr,"end device\n");
    fprintf(stderr,"----- End of input device configuration. -----\n");
  }
  
  akfceu_input_automapper_cleanup(&mapper);
  free(inputcfgpath);
  return 0;
}

/* Select a fresh playerid.
 * Returns the least used, with ties breaking low.
 */

static int akfceu_input_select_playerid_for_new_device() {

  /* How many have we currently assigned to each playerid?
   */
  int count_by_playerid[5]={0}; // [0] is a dummy
  const struct akfceu_input_assignment *assignment=akfceu_input.assignmentv;
  int i=akfceu_input.assignmentc;
  for (;i-->0;assignment++) {
    if (assignment->ignore_for_mapping) continue;
    if ((assignment->playerid>=1)&&(assignment->playerid<=4)) {
      count_by_playerid[assignment->playerid]++;
    }
  }

  /* Find the least used.
   */
  int best_playerid=1;
  int best_count=INT_MAX;
  for (i=1;i<=4;i++) {
    if (count_by_playerid[i]<=0) return i;
    if (count_by_playerid[i]<best_count) {
      best_playerid=i;
      best_count=count_by_playerid[i];
    }
  }
  return best_playerid;
}

/* Connect a device.
 * This is designed against the Minten 'machid' unit, but we can write other backends with the same interface.
 */

static int akfceu_input_generic_connect(int devid) {

  #if USE_macos
    int vid=mn_machid_dev_get_vendor_id(devid);
    int pid=mn_machid_dev_get_product_id(devid);
  #else
    int vid=0;
    int pid=0;
  #endif

  fprintf(stderr,"Connected input device %d [%04x:%04x]\n",devid,vid,pid);
  struct akfceu_input_map *map=akfceu_input_map_find(vid,pid);
  if (map) {
    fprintf(stderr,"Located input mappings.\n");
  } else {
    fprintf(stderr,"Input mappings not found, will attempt to synthesize defaults...\n");
    if (!(map=akfceu_input_map_new(vid,pid))) return -1;
    if (akfceu_input_populate_default_map(map,devid,vid,pid)<0) return -1;
  }

  if (akfceu_input_map_is_suitable_for_gamepad(map)) {
    struct akfceu_input_assignment *assignment=akfceu_input_assignment_new(-1,devid);
    if (!assignment) return -1;
    assignment->map=map;
    assignment->type=SI_GAMEPAD;
    assignment->playerid=akfceu_input_select_playerid_for_new_device();
    fprintf(stderr,"Assigned to player %d.\n",assignment->playerid);
  } else {
    fprintf(stderr,"Unable to map this device to a gamepad.\n");
  }

  return 0;
}

/* Disconnect device.
 */

static int akfceu_input_generic_disconnect(int devid) {
  fprintf(stderr,"Disconnect device %d.\n",devid);
  int p=akfceu_input_assignmentv_search(devid);
  if (p>=0) {
    struct akfceu_input_assignment *assignment=akfceu_input.assignmentv+p;
    akfceu_input_drop_state(assignment->type,assignment->playerid);
    akfceu_input_assignmentv_remove(p);
  }
  return 0;
}

/* Receive generic input event.
 */

static int akfceu_input_generic_event_mapped(int btnid,int value,void *userdata) {
  struct akfceu_input_assignment *assignment=userdata;
  int shift;
  switch (assignment->playerid) {
    case 1: shift=0; break;
    case 2: shift=8; break;
    case 3: shift=16; break;
    case 4: shift=24; break;
    default: return 0;
  }
  uint32_t mask=btnid<<shift;
  if (value) {
    if (mask&akfceu_input.gamepad_state) return 0;
    akfceu_input.gamepad_state|=mask;
  } else {
    if (!(mask&akfceu_input.gamepad_state)) return 0;
    akfceu_input.gamepad_state&=~mask;
  }
  //fprintf(stderr,"INPUT: %d.%02x=%d\n",assignment->playerid,btnid,value);
  return 0;
}

int akfceu_input_generic_event(int devid,int btnid,int value) {
  int p=akfceu_input_assignmentv_search(devid);
  if (p<0) return 0;
  struct akfceu_input_assignment *assignment=akfceu_input.assignmentv+p;
  if (!assignment->map) return 0;
  if (akfceu_input_map_update(assignment->map,btnid,value,akfceu_input_generic_event_mapped,assignment)<0) return -1;
  return 0;
}

/* Setup system keyboard.
 */

static int akfceu_input_init_system_keyboard() {

  struct akfceu_input_map *map=akfceu_input_map_new(-1,-1);
  if (!map) return -1;

  if (akfceu_input_map_add_button(map,0,0x0007001b,1,INT_MAX,JOY_B)<0) return -1; // x
  if (akfceu_input_map_add_button(map,1,0x0007001d,1,INT_MAX,JOY_A)<0) return -1; // z
  if (akfceu_input_map_add_button(map,2,0x00070028,1,INT_MAX,JOY_START)<0) return -1; // enter
  if (akfceu_input_map_add_button(map,3,0x0007002b,1,INT_MAX,JOY_SELECT)<0) return -1; // tab
  if (akfceu_input_map_add_button(map,4,0x0007004f,1,INT_MAX,JOY_RIGHT)<0) return -1; // right
  if (akfceu_input_map_add_button(map,5,0x00070050,1,INT_MAX,JOY_LEFT)<0) return -1; // left
  if (akfceu_input_map_add_button(map,6,0x00070051,1,INT_MAX,JOY_DOWN)<0) return -1; // down
  if (akfceu_input_map_add_button(map,7,0x00070052,1,INT_MAX,JOY_UP)<0) return -1; // up

  struct akfceu_input_assignment *assignment=akfceu_input_assignment_new(-1,-1);
  if (!assignment) return -1;
  assignment->map=map;
  assignment->type=SI_GAMEPAD;
  assignment->playerid=1;
  assignment->ignore_for_mapping=1;
  fprintf(stderr,"Assigned system keyboard to player %d.\n",assignment->playerid);

  return 0;
}

/* Init.
 */

int akfceu_input_init() {
  if (akfceu_input.init) return -1;
  memset(&akfceu_input,0,sizeof(akfceu_input));
  akfceu_input.init=1;

  char *path=FCEU_MakeFName(FCEUMKF_CONFIG,0,0);
  if (path) {
    int err=akfceu_input_map_load_configuration(path);
    free(path);
    if (err<0) return err;
  }

  #if USE_macos
    struct mn_machid_delegate machid_delegate={
      .connect=akfceu_input_generic_connect,
      .disconnect=akfceu_input_generic_disconnect,
      .button=akfceu_input_generic_event,
    };
    if (mn_machid_init(&machid_delegate)<0) {
      return -1;
    }
  #endif

  if (akfceu_input_init_system_keyboard()<0) {
    return -1;
  }

  return 0;
}

/* Quit.
 */

void akfceu_input_quit() {

  #if USE_macos
    mn_machid_quit();
  #endif

  if (akfceu_input.assignmentv) free(akfceu_input.assignmentv);

  memset(&akfceu_input,0,sizeof(akfceu_input));
}

/* Update.
 */

int akfceu_input_update() {
  if (!akfceu_input.init) return -1;

  #if USE_macos
    if (mn_machid_update()<0) return -1;
  #endif
  
  return 0;
}

/* Register with FCEU core.
 */

int akfceu_input_register_with_fceu() {
  if (!akfceu_input.init) return -1;
  akfceu_input.gamepad_state=0;
  
  FCEUI_DisableFourScore(0);
  
  FCEUI_SetInput(0,SI_GAMEPAD,&akfceu_input.gamepad_state,0);
  FCEUI_SetInput(1,SI_GAMEPAD,&akfceu_input.gamepad_state,0);

  /* Review of 4-player games, with this setup:
   *   FCEUI_DisableFourScore(0);
   *   FCEUI_SetInput(0,SI_GAMEPAD,&akfceu_input.gamepad_state,0);
   *   FCEUI_SetInput(1,SI_GAMEPAD,&akfceu_input.gamepad_state,0);
   * Bomberman 2: 2 players
   * Indy Heat: Works fine.
   * Gaunlet 2: Works.
   * Greg Norman's Golf Power: Wish it didn't, but it works.
   * Harlem Globtrotters: Works.
   * Justice Duel: Don't have it.
   * Kings of the Beach: Works nicely. I'd never played this one; it looks fun.
   * Magic Johnson's Fast Break: Looks good but hard to tell, I keep getting "10-second violation" before figuring out which controller.
   * Monopoly: Works... Doesn't bind players to joysticks, you can start anyone's turn from any joystick. Weird.
   * Monster Truck Rally: Works but oh lord does this suck.
   * MULE: We take turns, but with different joysticks. I really don't understand this game.
   * NES Play Action Football: Don't have it.
   * Nightmare on Elm Street: Works. Nice 4-way simultaneous sidescroller.
   * Nintendo World Cup: Ooh I love this game. Works great.
   * RC Pro Am II: Clearly what Monster Truck Rally was going for. Real nice.
   * Rackets & Rivals: Don't have it.
   * Rock N Ball: Only using 2 joysticks, even in 4-player mode.
   * Roundball: Works.
   * Spot: Don't have it.
   * Smash TV: Two heroes; Each has a shooter and walker... cool
   * Super Off Road: Works nice. Some non-4-player-related glitches.
   * Super Jeopardy: Works
   * Super Spike VBall: Works
   * Swords and Serpents: Uh, I guess it works? Hard to tell.
   * Top Players Tennis: Seems to work
   * US Championship VBall: Yes. Same as Super Spike
   */

  return 0;
}
