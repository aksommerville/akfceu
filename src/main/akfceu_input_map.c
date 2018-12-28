#include "akfceu_input_map.h"
#include "fceu/types.h"
#include "fceu/fceu.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Global registry of input maps.
 */

static struct akfceu_input_map **mapv=0;
static int mapc=0,mapa=0;

/* Global registry primitives.
 */

static int akfceu_input_map_global_search(int vid,int pid) {
  int lo=0,hi=mapc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (vid<mapv[ck]->vid) hi=ck;
    else if (vid>mapv[ck]->vid) lo=ck+1;
    else if (pid<mapv[ck]->pid) hi=ck;
    else if (pid>mapv[ck]->pid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int akfceu_input_map_global_insert(int p,struct akfceu_input_map *map) {
  if ((p<0)||(p>mapc)) return -1;
  if (mapc>=mapa) {
    int na=mapa+16;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(mapv,sizeof(void*)*na);
    if (!nv) return -1;
    mapv=nv;
    mapa=na;
  }
  memmove(mapv+p+1,mapv+p,sizeof(void*)*(mapc-p));
  mapc++;
  mapv[p]=map;
  return 0;
}

/* Find or create map, public.
 */

struct akfceu_input_map *akfceu_input_map_find(int vid,int pid) {
  int p=akfceu_input_map_global_search(vid,pid);
  if (p<0) return 0;
  return mapv[p];
}

struct akfceu_input_map *akfceu_input_map_new(int vid,int pid) {
  int p=akfceu_input_map_global_search(vid,pid);
  if (p>=0) return 0;
  p=-p-1;
  struct akfceu_input_map *map=calloc(1,sizeof(struct akfceu_input_map));
  if (!map) return 0;
  map->vid=vid;
  map->pid=pid;
  if (akfceu_input_map_global_insert(p,map)<0) {
    free(map);
    return 0;
  }
  return map;
}

/* Search button list in map.
 */

int akfceu_input_map_search_srcbtnid(const struct akfceu_input_map *map,int srcbtnid) {
  if (!map) return -1;
  int lo=0,hi=map->buttonc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (srcbtnid<map->buttonv[ck].srcbtnid) hi=ck;
    else if (srcbtnid>map->buttonv[ck].srcbtnid) lo=ck+1;
    else {
      // Duplicate srcbtnid is permitted.
      while ((ck>lo)&&(map->buttonv[ck-1].srcbtnid==srcbtnid)) ck--;
      return ck;
    }
  }
  return -lo-1;
}

/* Validate button position and grow list if necessary.
 */

static int akfceu_input_map_consider_addition(struct akfceu_input_map *map,int p,int srcbtnid) {
  if (!map) return -1;
  if ((p<0)||(p>map->buttonc)) {
    p=akfceu_input_map_search_srcbtnid(map,srcbtnid);
    if (p<0) p=-p-1;
  }
  if (p&&(srcbtnid<map->buttonv[p-1].srcbtnid)) return -1;
  if ((p<map->buttonc)&&(srcbtnid>map->buttonv[p].srcbtnid)) return -1;
  if (map->buttonc>=map->buttona) {
    int na=map->buttona+16;
    if (na>INT_MAX/sizeof(struct akfceu_button_map)) return -1;
    void *nv=realloc(map->buttonv,sizeof(struct akfceu_button_map)*na);
    if (!nv) return -1;
    map->buttonv=nv;
    map->buttona=na;
  }
  return p;
}

/* Add button to map.
 */

int akfceu_input_map_add_button(struct akfceu_input_map *map,int p,int srcbtnid,int srclo,int srchi,int dstbtnid) {
  if ((p=akfceu_input_map_consider_addition(map,p,srcbtnid))<0) return -1;
  struct akfceu_button_map *button=map->buttonv+p;
  memmove(button+1,button,sizeof(struct akfceu_button_map)*(map->buttonc-p));
  map->buttonc++;
  button->srcbtnid=srcbtnid;
  button->srclo=srclo;
  button->srchi=srchi;
  button->dstbtnid=dstbtnid;
  return 0;
}

/* Add hat to map.
 */

int akfceu_input_map_add_hat(struct akfceu_input_map *map,int p,int srcbtnid,int validlo,int validhi) {
  if ((p=akfceu_input_map_consider_addition(map,p,srcbtnid))<0) return -1;
  struct akfceu_button_map *button=map->buttonv+p;
  memmove(button+1,button,sizeof(struct akfceu_button_map)*(map->buttonc-p));
  map->buttonc++;
  button->srcbtnid=srcbtnid;
  button->srclo=validlo;
  button->srchi=validhi;
  button->dstbtnid=AKFCEU_DSTBTNID_HAT;
  return 0;
}

/* Process event.
 */

int akfceu_input_map_update(
  const struct akfceu_input_map *map,
  int srcbtnid,int srcvalue,
  int (*cb)(int dstbtnid,int value,void *userdata),
  void *userdata
) {
  if (!map) return -1;
  if (!cb) return -1;
  int buttonp=akfceu_input_map_search_srcbtnid(map,srcbtnid);
  if (buttonp<0) return 0;
  const struct akfceu_button_map *button=map->buttonv+buttonp;
  for (;(buttonp<map->buttonc)&&(button->srcbtnid==srcbtnid);buttonp++,button++) {

    if (button->dstbtnid==AKFCEU_DSTBTNID_HAT) {
      // Make sense of the damn hat...
      int hatv=srcvalue-button->srclo;
      int x=0,y=0,err;
      switch (hatv) {
        case 0: y=-1; break;
        case 1: x=1; y=-1; break;
        case 2: x=1; break;
        case 3: x=1; y=1; break;
        case 4: y=1; break;
        case 5: x=-1; y=1; break;
        case 6: x=-1; break;
        case 7: x=-1; y=-1; break;
      }
      // Turn off anything that might have been on...
      if (x>=0) {
        if (err=cb(JOY_LEFT,0,userdata)) return err;
      }
      if (x<=0) {
        if (err=cb(JOY_RIGHT,0,userdata)) return err;
      }
      if (y>=0) {
        if (err=cb(JOY_UP,0,userdata)) return err;
      }
      if (y<=0) {
        if (err=cb(JOY_DOWN,0,userdata)) return err;
      }
      // Turn on anything we know is on...
      if (x<0) {
        if (err=cb(JOY_LEFT,1,userdata)) return err;
      } else if (x>0) {
        if (err=cb(JOY_RIGHT,1,userdata)) return err;
      }
      if (y<0) {
        if (err=cb(JOY_UP,1,userdata)) return err;
      } else if (y>0) {
        if (err=cb(JOY_DOWN,1,userdata)) return err;
      }

    } else {
      // Meanwhile, in the civilized, bare-headed world...
      int dstvalue=((srcvalue>=button->srclo)&&(srcvalue<=button->srchi))?1:0;
      int err=cb(button->dstbtnid,dstvalue,userdata);
      if (err) return err;
    }
  }
  return 0;
}

/* Create mappings for my own devices.
 */

int akfceu_define_andys_devices() {
  struct akfceu_input_map *map;

  /* Cheap PS knockoffs.
   * Press the Heart button, LED should be off, to use D-Pad as axes 19 and 20.
   */
  if (!(map=akfceu_input_map_new(0x0e8f,0x0003))) return -1;
  if (akfceu_input_map_add_button(map,-1, 6,1,1,JOY_A)<0) return -1;
  if (akfceu_input_map_add_button(map,-1, 7,1,1,JOY_B)<0) return -1;
  if (akfceu_input_map_add_button(map,-1,12,1,1,JOY_SELECT)<0) return -1;
  if (akfceu_input_map_add_button(map,-1,13,1,1,JOY_START)<0) return -1;
  if (akfceu_input_map_add_button(map,-1,19,0,64,JOY_LEFT)<0) return -1;
  if (akfceu_input_map_add_button(map,-1,19,192,256,JOY_RIGHT)<0) return -1;
  if (akfceu_input_map_add_button(map,-1,20,0,64,JOY_UP)<0) return -1;
  if (akfceu_input_map_add_button(map,-1,20,192,256,JOY_DOWN)<0) return -1;

  /* Expensive PS knockoff. */
  if (!(map=akfceu_input_map_new(0x20d6,0xca6d))) return -1;
  if (akfceu_input_map_add_hat(map,-1,15,0,7)<0) return -1;
  if (akfceu_input_map_add_button(map,-1, 3,1,1,JOY_A)<0) return -1;
  if (akfceu_input_map_add_button(map,-1, 2,1,1,JOY_B)<0) return -1;
  if (akfceu_input_map_add_button(map,-1,10,1,1,JOY_SELECT)<0) return -1;
  if (akfceu_input_map_add_button(map,-1,11,1,1,JOY_START)<0) return -1;

  /* Wii Official one. */
  if (!(map=akfceu_input_map_new(0x20d6,0xa711))) return -1;
  if (akfceu_input_map_add_hat(map,-1,16,0,7)<0) return -1;
  if (akfceu_input_map_add_button(map,-1, 3,1,1,JOY_A)<0) return -1;
  if (akfceu_input_map_add_button(map,-1, 2,1,1,JOY_B)<0) return -1;
  if (akfceu_input_map_add_button(map,-1,14,1,1,JOY_START)<0) return -1;
  if (akfceu_input_map_add_button(map,-1,11,1,1,JOY_START)<0) return -1;
  if (akfceu_input_map_add_button(map,-1,15,1,1,JOY_SELECT)<0) return -1;
  if (akfceu_input_map_add_button(map,-1,10,1,1,JOY_SELECT)<0) return -1;
 
  return 0;
}
