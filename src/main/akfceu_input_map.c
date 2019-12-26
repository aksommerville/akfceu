#include "akfceu_input_map.h"
#include "util/akfceu_file.h"
#include "fceu/types.h"
#include "fceu/fceu.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>

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

/* Test completeness of map.
 */
 
int akfceu_input_map_is_suitable_for_gamepad(const struct akfceu_input_map *map) {
  if (!map) return 0;
  uint8_t outputmask=0;
  const struct akfceu_button_map *button=map->buttonv;
  int i=map->buttonc;
  for (;i-->0;button++) {
    if (button->dstbtnid==AKFCEU_DSTBTNID_HAT) {
      outputmask|=JOY_UP|JOY_DOWN|JOY_LEFT|JOY_RIGHT;
    } else {
      outputmask|=button->dstbtnid;
    }
  }
  if ((outputmask&0xff)==0xff) return 1;
  return 0;
}

/* Read mappings from file in memory.
 */

static struct akfceu_input_map *akfceu_input_map_load_configuration_introducer(const char *src,int srcc) {

  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *kw=src+srcp;
  int kwc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; kwc++; }
  if ((kwc!=6)||memcmp(kw,"device",6)) return 0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *vidsrc=src+srcp;
  int vidsrcc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; vidsrcc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *pidsrc=src+srcp;
  int pidsrcc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; pidsrcc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp<srcc) return 0;

  int vid,pid;
  if (akfceu_int_eval(&vid,vidsrc,vidsrcc)<0) return 0;
  if (akfceu_int_eval(&pid,pidsrc,pidsrcc)<0) return 0;
  if ((vid<0)||(vid>0xffff)||(pid<0)||(pid>0xffff)) return 0;

  return akfceu_input_map_new(vid,pid);
}

static int akfceu_input_map_load_configuration_line(struct akfceu_input_map *map,const char *src,int srcc) {

  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *inputsrc=src+srcp;
  int inputsrcc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; inputsrcc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *outputsrc=src+srcp;
  int outputsrcc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; outputsrcc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *losrc=src+srcp;
  int losrcc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; losrcc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *hisrc=src+srcp;
  int hisrcc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; hisrcc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp<srcc) return -1;

  int input,output,lo,hi;
  if (akfceu_int_eval(&input,inputsrc,inputsrcc)<0) return -1;
  if ((output=akfceu_input_map_dstbtnid_eval(outputsrc,outputsrcc))<0) return -1;
  if (losrcc) {
    if (akfceu_int_eval(&lo,losrc,losrcc)<0) return -1;
  } else {
    lo=1;
  }
  if (hisrcc) {
    if (akfceu_int_eval(&hi,hisrc,hisrcc)<0) return -1;
  } else {
    hi=INT_MAX;
  }

  switch (output) {
    case AKFCEU_DSTBTNID_HAT: {
        if (akfceu_input_map_add_hat(map,-1,input,lo,hi)<0) return -1;
      } break;
    case AKFCEU_DSTBTNID_HORZ: if (lo<hi) {
        if (akfceu_input_map_add_button(map,-1,input,INT_MIN,lo,JOY_LEFT)<0) return -1;
        if (akfceu_input_map_add_button(map,-1,input,hi,INT_MAX,JOY_RIGHT)<0) return -1;
      } else {
        if (akfceu_input_map_add_button(map,-1,input,INT_MIN,lo,JOY_RIGHT)<0) return -1;
        if (akfceu_input_map_add_button(map,-1,input,hi,INT_MAX,JOY_LEFT)<0) return -1;
      } break;
    case AKFCEU_DSTBTNID_VERT: if (lo<hi) {
        if (akfceu_input_map_add_button(map,-1,input,INT_MIN,lo,JOY_UP)<0) return -1;
        if (akfceu_input_map_add_button(map,-1,input,hi,INT_MAX,JOY_DOWN)<0) return -1;
      } else {
        if (akfceu_input_map_add_button(map,-1,input,INT_MIN,lo,JOY_DOWN)<0) return -1;
        if (akfceu_input_map_add_button(map,-1,input,hi,INT_MAX,JOY_UP)<0) return -1;
      } break;
    default: {
        if (akfceu_input_map_add_button(map,-1,input,lo,hi,output)<0) return -1;
      } break;
  }

  return 0;
}

static int akfceu_input_map_load_configuration_text(const char *src,int srcc,const char *path) {
  struct akfceu_input_map *map=0;
  struct akfceu_line_reader reader={.src=src,.srcc=srcc};
  while (akfceu_line_reader_next(&reader)>0) {
    if (map) {
      if ((reader.linec==10)&&!memcmp(reader.line,"end device",10)) {
        map=0;
      } else {
        if (akfceu_input_map_load_configuration_line(map,reader.line,reader.linec)<0) {
          fprintf(stderr,"%s:%d: Failed to process input configuration.\n",path,reader.lineno);
          return -1;
        }
      }
    } else if ((reader.linec>=7)&&!memcmp(reader.line,"device ",7)) {
      if (!(map=akfceu_input_map_load_configuration_introducer(reader.line,reader.linec))) {
        fprintf(stderr,"%s:%d: Failed to process device introducer.\n",path,reader.lineno);
        return -1;
      }
    }
  }
  if (map) {
    fprintf(stderr,"%s: Unterminated device config block.\n",path);
    return -1;
  }
  return 0;
}

/* Read mappings from a file, given its path.
 */
 
int akfceu_input_map_load_configuration(const char *path) {
  fprintf(stderr,"Read input configuration from '%s'...\n",path);
  char *src=0;
  int srcc=akfceu_file_read(&src,path);
  if (srcc<0) return 0;
  int err=akfceu_input_map_load_configuration_text(src,srcc,path);
  free(src);
  return err;
}

/* Evaluate single map output name.
 */
 
int akfceu_input_map_dstbtnid_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc>8) return -1;
  char lower[8];
  int i=srcc; while (i-->0) {
    if ((src[i]>='A')&&(src[i]<='Z')) lower[i]=src[i]+0x20;
    else lower[i]=src[i];
  }
  switch (srcc) {
    case 1: switch (lower[0]) {
        case 'a': return JOY_A;
        case 'b': return JOY_B;
      } break;
    case 2: {
        if (!memcmp(lower,"up",2)) return JOY_UP;
      } break;
    case 4: {
        if (!memcmp(lower,"down",4)) return JOY_DOWN;
        if (!memcmp(lower,"left",4)) return JOY_LEFT;
        if (!memcmp(lower,"dpad",4)) return AKFCEU_DSTBTNID_HAT;
        if (!memcmp(lower,"horz",4)) return AKFCEU_DSTBTNID_HORZ;
        if (!memcmp(lower,"vert",4)) return AKFCEU_DSTBTNID_VERT;
      } break;
    case 5: {
        if (!memcmp(lower,"right",5)) return JOY_RIGHT;
        if (!memcmp(lower,"start",5)) return JOY_START;
      } break;
    case 6: {
        if (!memcmp(lower,"select",6)) return JOY_SELECT;
      } break;
  }
  return -1;
}
