/* mn_machid.h
 * Standalone C API for HID in MacOS.
 * You must call all mn_machid functions from the same thread.
 * It does not need to be the main thread. (TODO confirm this)
 * Link: -framework IOKit
 */

#ifndef MN_MACHID_H
#define MN_MACHID_H

/* All delegate functions are optional.
 * All except test_device can return <0 for error.
 * We will return the same error code through mn_machid_update().
 * There may be additional callbacks before an error gets reported; it doesn't abort all processing.
 */
struct mn_machid_delegate {

  /* Return nonzero to accept this device.
   * No errors.
   * Unset for the default behavior (Joystick, Game Pad, and Multi-Axis Controller).
   * This is called before full connection, so we don't have complete device properties yet.
   */
  int (*test_device)(void *hiddevice,int vid,int pid,int usagepage,int usage);

  /* Notify connection or disconnection of a device.
   * This is the first and last time you will see this devid.
   * If it appears again, it's a new device.
   */
  int (*connect)(int devid);
  int (*disconnect)(int devid);

  /* Notify of activity on a device.
   */
  int (*button)(int devid,int btnid,int value);

};

/* Main public API.
 */

int mn_machid_init(const struct mn_machid_delegate *delegate);

void mn_machid_quit();

/* Trigger our private run loop mode and return any errors gathered during it.
 */
int mn_machid_update();

/* Access to installed devices.
 */

int mn_machid_count_devices();
int mn_machid_devid_for_index(int index);
int mn_machid_index_for_devid(int devid);

void *mn_machid_dev_get_IOHIDDeviceRef(int devid);
int mn_machid_dev_get_vendor_id(int devid);
int mn_machid_dev_get_product_id(int devid);
int mn_machid_dev_get_usage_page(int devid);
int mn_machid_dev_get_usage(int devid);
const char *mn_machid_dev_get_manufacturer_name(int devid);
const char *mn_machid_dev_get_product_name(int devid);
const char *mn_machid_dev_get_serial_number(int devid);

int mn_machid_dev_count_buttons(int devid);
int mn_machid_dev_get_button_info(int *btnid,int *usage,int *lo,int *hi,int *value,int devid,int index);

#endif
