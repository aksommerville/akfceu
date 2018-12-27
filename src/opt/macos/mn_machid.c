#include "mn_machid.h"
#include <stdio.h>
#include <IOKit/hid/IOHIDLib.h>

/* Private definitions.
 */

#define AKMACHID_RUNLOOP_MODE CFSTR("com.aksommerville.mn_machid")

struct mn_machid_btn {
  int btnid;
  int usage;
  int value;
  int lo,hi;
};

struct mn_machid_dev {
  IOHIDDeviceRef obj; // weak
  int devid;
  int vid,pid;
  char *mfrname,*prodname,*serial;
  int usagepage,usage;
  struct mn_machid_btn *btnv; int btnc,btna;
};

/* Globals.
 */

static struct {
  IOHIDManagerRef hidmgr;
  struct mn_machid_dev *devv; // Sorted by (obj)
  int devc,deva;
  struct mn_machid_delegate delegate; // All callbacks are set, possibly with defaults.
  int error; // Sticky error from callbacks, reported at end of update.
} mn_machid={0};

/* Get integer field from device.
 */

static int dev_get_int(IOHIDDeviceRef dev,CFStringRef k) {
  CFNumberRef vobj=IOHIDDeviceGetProperty(dev,k);
  if (!vobj) return 0;
  if (CFGetTypeID(vobj)!=CFNumberGetTypeID()) return 0;
  int v=0;
  CFNumberGetValue(vobj,kCFNumberIntType,&v);
  return v;
}

/* Cleanup device record.
 */

static void mn_machid_dev_cleanup(struct mn_machid_dev *dev) {
  if (!dev) return;
  if (dev->btnv) free(dev->btnv);
  if (dev->mfrname) free(dev->mfrname);
  if (dev->prodname) free(dev->prodname);
  if (dev->serial) free(dev->serial);
  memset(dev,0,sizeof(struct mn_machid_dev));
}

/* Search global device list.
 */

static int mn_machid_dev_search_devid(int devid) {
  int i; for (i=0;i<mn_machid.devc;i++) if (mn_machid.devv[i].devid==devid) return i;
  return -mn_machid.devc-1;
}

static int mn_machid_dev_search_obj(IOHIDDeviceRef obj) {
  int lo=0,hi=mn_machid.devc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (obj<mn_machid.devv[ck].obj) hi=ck;
    else if (obj>mn_machid.devv[ck].obj) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int mn_machid_devid_unused() {
  if (mn_machid.devc<1) return 1;
  
  /* Most cases: return one more than the highest used ID. */
  int i,top=0;
  for (i=0;i<mn_machid.devc;i++) {
    if (mn_machid.devv[i].devid>top) top=mn_machid.devv[i].devid;
  }
  if (top<INT_MAX) return top+1;

  /* If we've reached ID INT_MAX, there must be a gap somewhere. Find it. */
  int devid=1;
  for (i=0;i<mn_machid.devc;i++) if (mn_machid.devv[i].devid==devid) { devid++; i=-1; }
  return devid;
}

/* Insert device to global list.
 */

static int mn_machid_dev_insert_validate(int p,IOHIDDeviceRef obj,int devid) {
  if ((p<0)||(p>mn_machid.devc)) return -1;
  if (!obj||(CFGetTypeID(obj)!=IOHIDDeviceGetTypeID())) return -1;
  if ((devid<1)||(devid>0xffff)) return -1;
  if (p&&(obj<=mn_machid.devv[p-1].obj)) return -1;
  if ((p<mn_machid.devc)&&(obj>=mn_machid.devv[p].obj)) return -1;
  int i; for (i=0;i<mn_machid.devc;i++) if (mn_machid.devv[i].devid==devid) return -1;
  return 0;
}

static int mn_machid_devv_require() {
  if (mn_machid.devc<mn_machid.deva) return 0;
  int na=mn_machid.deva+8;
  if (na>INT_MAX/sizeof(struct mn_machid_dev)) return -1;
  void *nv=realloc(mn_machid.devv,sizeof(struct mn_machid_dev)*na);
  if (!nv) return -1;
  mn_machid.devv=nv;
  mn_machid.deva=na;
  return 0;
}

static int mn_machid_dev_insert(int p,IOHIDDeviceRef obj,int devid) {

  if (mn_machid_dev_insert_validate(p,obj,devid)<0) return -1;
  if (mn_machid_devv_require()<0) return -1;
  
  struct mn_machid_dev *dev=mn_machid.devv+p;
  memmove(dev+1,dev,sizeof(struct mn_machid_dev)*(mn_machid.devc-p));
  mn_machid.devc++;
  memset(dev,0,sizeof(struct mn_machid_dev));
  dev->obj=obj;
  dev->devid=devid;
  
  return 0;
}

/* Remove from global device list.
 */

static int mn_machid_dev_remove(int p) {
  if ((p<0)||(p>=mn_machid.devc)) return -1;
  mn_machid_dev_cleanup(mn_machid.devv+p);
  mn_machid.devc--;
  memmove(mn_machid.devv+p,mn_machid.devv+p+1,sizeof(struct mn_machid_dev)*(mn_machid.devc-p));
  return 0;
}

/* Search buttons in device.
 */

static int mn_machid_dev_search_button(struct mn_machid_dev *dev,int btnid) {
  if (!dev) return -1;
  int lo=0,hi=dev->btnc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (btnid<dev->btnv[ck].btnid) hi=ck;
    else if (btnid>dev->btnv[ck].btnid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Add button record to device.
 */

static int mn_machid_dev_insert_button(struct mn_machid_dev *dev,int p,int btnid,int usage,int lo,int hi,int v) {
  if (!dev||(p<0)||(p>dev->btnc)) return -1;
  
  if (dev->btnc>=dev->btna) {
    int na=dev->btna+8;
    if (na>INT_MAX/sizeof(struct mn_machid_btn)) return -1;
    void *nv=realloc(dev->btnv,sizeof(struct mn_machid_btn)*na);
    if (!nv) return -1;
    dev->btnv=nv;
    dev->btna=na;
  }

  struct mn_machid_btn *btn=dev->btnv+p;
  memmove(btn+1,btn,sizeof(struct mn_machid_btn)*(dev->btnc-p));
  dev->btnc++;
  memset(btn,0,sizeof(struct mn_machid_btn));
  btn->btnid=btnid;
  btn->usage=usage;
  btn->lo=lo;
  btn->hi=hi;
  if ((v>=lo)&&(v<=hi)) btn->value=v;
  else if ((lo<=0)&&(hi>=0)) btn->value=0;
  else if (lo>0) btn->value=lo;
  else btn->value=hi;

  /* If it's a hat switch, Minten Core does the heavy lifting.
   * We only need to ensure that OOB values are never clamped.
   */
  if ((usage==0x00010039)&&(lo==0)&&(hi==7)) {
    btn->lo=-1; // Hats report a value outside the declared range for "none", which is annoying.
    btn->hi=8; // Annoying but well within spec (see USB-HID HUT)
    btn->value=8; // Start out of range, ie (0,0)
  }

  return 0;
}

static int mn_machid_dev_define_button(struct mn_machid_dev *dev,int btnid,int usage,int lo,int hi,int v) {
  int p=mn_machid_dev_search_button(dev,btnid);
  if (p>=0) return 0; // Already have this btnid, don't panic. Keep the first definition.
  return mn_machid_dev_insert_button(dev,-p-1,btnid,usage,lo,hi,v);
}

/* Welcome a new device, before exposing to user.
 */

static char *mn_machid_dev_get_string(struct mn_machid_dev *dev,CFStringRef key) {
  char buf[256]={0};

  CFStringRef string=IOHIDDeviceGetProperty(dev->obj,key);
  if (!string) return 0;
  if (!CFStringGetCString(string,buf,sizeof(buf),kCFStringEncodingUTF8)) return 0;
  int bufc=0; while ((bufc<sizeof(buf))&&buf[bufc]) bufc++;
  if (!bufc) return 0;

  /* Force non-G0 to space, then trim leading and trailing spaces. */
  int i; for (i=0;i<bufc;i++) {
    if ((buf[i]<0x20)||(buf[i]>0x7e)) buf[i]=0x20;
  }
  while (bufc&&(buf[bufc-1]==0x20)) bufc--;
  int leadc=0; while ((leadc<bufc)&&(buf[leadc]==0x20)) leadc++;
  if (leadc==bufc) return 0;
  
  char *dst=malloc(bufc-leadc+1);
  if (!dst) return 0;
  memcpy(dst,buf+leadc,bufc-leadc);
  dst[bufc-leadc]=0;
  return dst;
}

static int mn_machid_dev_apply_IOHIDElement(struct mn_machid_dev *dev,IOHIDElementRef element) {
      
  IOHIDElementCookie cookie=IOHIDElementGetCookie(element);
  CFIndex lo=IOHIDElementGetLogicalMin(element);
  CFIndex hi=IOHIDElementGetLogicalMax(element);
  if ((int)cookie<INT_MIN) cookie=INT_MIN; else if (cookie>INT_MAX) cookie=INT_MAX;
  if ((int)lo<INT_MIN) lo=INT_MIN; else if (lo>INT_MAX) lo=INT_MAX;
  if ((int)hi<INT_MIN) hi=INT_MIN; else if (hi>INT_MAX) hi=INT_MAX;
  if (lo>hi) { lo=INT_MIN; hi=INT_MAX; }
  uint16_t usagepage=IOHIDElementGetUsagePage(element);
  uint16_t usage=IOHIDElementGetUsage(element);

  IOHIDValueRef value=0;
  int v=0;
  if (IOHIDDeviceGetValue(dev->obj,element,&value)==kIOReturnSuccess) {
    v=IOHIDValueGetIntegerValue(value);
    if (v<lo) v=lo; else if (v>hi) v=hi;
  }

  mn_machid_dev_define_button(dev,cookie,(usagepage<<16)|usage,lo,hi,v);

  return 0;
}

static int mn_machid_dev_handshake(struct mn_machid_dev *dev,int vid,int pid,int usagepage,int usage) {

  dev->vid=vid;
  dev->pid=pid;
  dev->usagepage=usagepage;
  dev->usage=usage;

  /* Store manufacturer name, product name, and serial number if we can get them. */
  dev->mfrname=mn_machid_dev_get_string(dev,CFSTR(kIOHIDManufacturerKey));
  dev->prodname=mn_machid_dev_get_string(dev,CFSTR(kIOHIDProductKey));
  dev->serial=mn_machid_dev_get_string(dev,CFSTR(kIOHIDSerialNumberKey));

  /* Get limits and current value for each reported element. */
  CFArrayRef elements=IOHIDDeviceCopyMatchingElements(dev->obj,0,0);
  if (elements) {
    CFTypeID elemtypeid=IOHIDElementGetTypeID();
    CFIndex elemc=CFArrayGetCount(elements);
    CFIndex elemi; for (elemi=0;elemi<elemc;elemi++) {
      IOHIDElementRef element=(IOHIDElementRef)CFArrayGetValueAtIndex(elements,elemi);
      if (element&&(CFGetTypeID(element)==elemtypeid)) {
        mn_machid_dev_apply_IOHIDElement(dev,element);
      }
    }
    CFRelease(elements);
  }

  return 0;
}

/* Connect device, callback from IOHIDManager.
 */

static void mn_machid_cb_DeviceMatching(void *context,IOReturn result,void *sender,IOHIDDeviceRef obj) {
  
  int vid=dev_get_int(obj,CFSTR(kIOHIDVendorIDKey));
  int pid=dev_get_int(obj,CFSTR(kIOHIDProductIDKey));
  int usagepage=dev_get_int(obj,CFSTR(kIOHIDPrimaryUsagePageKey));
  int usage=dev_get_int(obj,CFSTR(kIOHIDPrimaryUsageKey));

  if (!mn_machid.delegate.test_device(obj,vid,pid,usagepage,usage)) {
    IOHIDDeviceClose(obj,0);
    return;
  }
        
  int devid=mn_machid_devid_unused();
  int p=mn_machid_dev_search_obj(obj);
  if (p>=0) return; // PANIC! Device is already listed.
  p=-p-1;
  if (mn_machid_dev_insert(p,obj,devid)<0) return;
  if (mn_machid_dev_handshake(mn_machid.devv+p,vid,pid,usagepage,usage)<0) { mn_machid_dev_remove(p); return; }
  int err=mn_machid.delegate.connect(devid);
  if (err<0) mn_machid.error=err;
}

/* Disconnect device, callback from IOHIDManager.
 */

static void mn_machid_cb_DeviceRemoval(void *context,IOReturn result,void *sender,IOHIDDeviceRef obj) {
  int p=mn_machid_dev_search_obj(obj);
  if (p>=0) {
    int err=mn_machid.delegate.disconnect(mn_machid.devv[p].devid);
    if (err<0) mn_machid.error=err;
    mn_machid_dev_remove(p);
  }
}

/* Event callback from IOHIDManager.
 */

static void mn_machid_axis_values_from_hat(int *x,int *y,int v) {
  *x=*y=0;
  switch (v) {
    case 0: *y=-1; break;
    case 1: *x=1; *y=-1; break;
    case 2: *x=1; break;
    case 3: *x=1; *y=1; break;
    case 4: *y=1; break;
    case 5: *x=-1; *y=1; break;
    case 6: *x=-1; break;
    case 7: *x=-1; *y=-1; break;
  }
}

static void mn_machid_cb_InputValue(void *context,IOReturn result,void *sender,IOHIDValueRef value) {

  /* Locate device and buttons. */
  IOHIDElementRef element=IOHIDValueGetElement(value);
  int btnid=IOHIDElementGetCookie(element);
  if (btnid<0) return;
  IOHIDDeviceRef obj=IOHIDElementGetDevice(element);
  if (!obj) return;
  int p=mn_machid_dev_search_obj(obj);
  if (p<0) return;
  struct mn_machid_dev *dev=mn_machid.devv+p;
  int btnp=mn_machid_dev_search_button(dev,btnid);
  if (btnp<0) return;
  struct mn_machid_btn *btn=dev->btnv+btnp;

  /* Clamp value and confirm it actually changed. */
  CFIndex v=IOHIDValueGetIntegerValue(value);
  if (v<btn->lo) v=btn->lo;
  else if (v>btn->hi) v=btn->hi;
  if (v==btn->value) return;
  int ov=btn->value;
  btn->value=v;

  /* Report ordinary scalar buttons. */
  int err=mn_machid.delegate.button(dev->devid,btnid,v);
  if (err<0) mn_machid.error=err;
}

/* Default delegate callbacks.
 */
 
static int mn_machid_delegate_test_device_default(void *hiddevice,int vid,int pid,int usagepage,int usage) {
  if (usagepage==0x0001) switch (usage) { // Usage Page 0x0001: Generic Desktop Controls
    case 0x0004: return 1; // ...0x0004: Joystick
    case 0x0005: return 1; // ...0x0005: Game Pad
    case 0x0008: return 1; // ...0x0008: Multi-axis Controller
  }
  return 0;
}

static int mn_machid_delegate_connect_default(int devid) {
  return 0;
}

static int mn_machid_delegate_disconnect_default(int devid) {
  return 0;
}

static int mn_machid_delegate_button_default(int devid,int btnid,int value) {
  return 0;
}

/* Init.
 */

int mn_machid_init(const struct mn_machid_delegate *delegate) {
  if (mn_machid.hidmgr) return -1; // Already initialized.
  memset(&mn_machid,0,sizeof(mn_machid));
  
  if (!(mn_machid.hidmgr=IOHIDManagerCreate(kCFAllocatorDefault,0))) return -1;

  if (delegate) memcpy(&mn_machid.delegate,delegate,sizeof(struct mn_machid_delegate));
  if (!mn_machid.delegate.test_device) mn_machid.delegate.test_device=mn_machid_delegate_test_device_default;
  if (!mn_machid.delegate.connect) mn_machid.delegate.connect=mn_machid_delegate_connect_default;
  if (!mn_machid.delegate.disconnect) mn_machid.delegate.disconnect=mn_machid_delegate_disconnect_default;
  if (!mn_machid.delegate.button) mn_machid.delegate.button=mn_machid_delegate_button_default;

  IOHIDManagerRegisterDeviceMatchingCallback(mn_machid.hidmgr,mn_machid_cb_DeviceMatching,0);
  IOHIDManagerRegisterDeviceRemovalCallback(mn_machid.hidmgr,mn_machid_cb_DeviceRemoval,0);
  IOHIDManagerRegisterInputValueCallback(mn_machid.hidmgr,mn_machid_cb_InputValue,0);

  IOHIDManagerSetDeviceMatching(mn_machid.hidmgr,0); // match every HID

  IOHIDManagerScheduleWithRunLoop(mn_machid.hidmgr,CFRunLoopGetCurrent(),AKMACHID_RUNLOOP_MODE);
    
  if (IOHIDManagerOpen(mn_machid.hidmgr,0)<0) {
    IOHIDManagerUnscheduleFromRunLoop(mn_machid.hidmgr,CFRunLoopGetCurrent(),AKMACHID_RUNLOOP_MODE);
    IOHIDManagerClose(mn_machid.hidmgr,0);
    memset(&mn_machid,0,sizeof(mn_machid));
    return -1;
  }
  
  return 0;
}

/* Quit.
 */

void mn_machid_quit() {
  if (!mn_machid.hidmgr) return;
  IOHIDManagerUnscheduleFromRunLoop(mn_machid.hidmgr,CFRunLoopGetCurrent(),AKMACHID_RUNLOOP_MODE);
  IOHIDManagerClose(mn_machid.hidmgr,0);
  if (mn_machid.devv) {
    while (mn_machid.devc--) mn_machid_dev_cleanup(mn_machid.devv+mn_machid.devc);
    free(mn_machid.devv);
  }
  memset(&mn_machid,0,sizeof(mn_machid));
}

/* Update.
 */

int mn_machid_update(double timeout) {
  if (!mn_machid.hidmgr) return -1;
  CFRunLoopRunInMode(AKMACHID_RUNLOOP_MODE,timeout,0);
  if (mn_machid.error) {
    int result=mn_machid.error;
    mn_machid.error=0;
    return result;
  }
  return 0;
}

/* Trivial public device accessors.
 */

int mn_machid_count_devices() {
  return mn_machid.devc;
}

int mn_machid_devid_for_index(int index) {
  if ((index<0)||(index>=mn_machid.devc)) return 0;
  return mn_machid.devv[index].devid;
}

int mn_machid_index_for_devid(int devid) {
  return mn_machid_dev_search_devid(devid);
}

#define GETINT(fldname) { \
  int p=mn_machid_dev_search_devid(devid); \
  if (p<0) return 0; \
  return mn_machid.devv[p].fldname; \
}
#define GETSTR(fldname) { \
  int p=mn_machid_dev_search_devid(devid); \
  if (p<0) return ""; \
  if (!mn_machid.devv[p].fldname) return ""; \
  return mn_machid.devv[p].fldname; \
}
 
void *mn_machid_dev_get_IOHIDDeviceRef(int devid) GETINT(obj)
int mn_machid_dev_get_vendor_id(int devid) GETINT(vid)
int mn_machid_dev_get_product_id(int devid) GETINT(pid)
int mn_machid_dev_get_usage_page(int devid) GETINT(usagepage)
int mn_machid_dev_get_usage(int devid) GETINT(usage)
const char *mn_machid_dev_get_manufacturer_name(int devid) GETSTR(mfrname)
const char *mn_machid_dev_get_product_name(int devid) GETSTR(prodname)
const char *mn_machid_dev_get_serial_number(int devid) GETSTR(serial)

#undef GETINT
#undef GETSTR

/* Count buttons.
 * We need to report the 'aux' as distinct buttons.
 */

int mn_machid_dev_count_buttons(int devid) {
  int devp=mn_machid_dev_search_devid(devid);
  if (devp<0) return 0;
  const struct mn_machid_dev *dev=mn_machid.devv+devp;
  return dev->btnc;
}

/* Get button properties.
 */

int mn_machid_dev_get_button_info(int *btnid,int *usage,int *lo,int *hi,int *value,int devid,int index) {
  if (index<0) return -1;
  int devp=mn_machid_dev_search_devid(devid);
  if (devp<0) return -1;
  struct mn_machid_dev *dev=mn_machid.devv+devp;
  if (index<dev->btnc) {
    struct mn_machid_btn *btn=dev->btnv+index;
    if (btnid) *btnid=btn->btnid;
    if (usage) *usage=btn->usage;
    if (lo) *lo=btn->lo;
    if (hi) *hi=btn->hi;
    if (value) *value=btn->value;
    return 0;
  }
  return -1;
}
