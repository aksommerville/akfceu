#include <string.h>
#include <pthread.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include "mn_macaudio.h"

/* Globals.
 */

static struct {
  int init;
  int state;
  AudioComponent component;
  AudioComponentInstance instance;
  void (*cb)(int16_t *dst,int dstc);
  pthread_mutex_t cbmtx;
} mn_macaudio={0};

/* AudioUnit callback.
 */

static OSStatus mn_macaudio_cb(
  void *userdata,
  AudioUnitRenderActionFlags *flags,
  const AudioTimeStamp *time,
  UInt32 bus,
  UInt32 framec,
  AudioBufferList *data
) {
  if (pthread_mutex_lock(&mn_macaudio.cbmtx)) return 0;
  mn_macaudio.cb(data->mBuffers[0].mData,data->mBuffers[0].mDataByteSize>>1);
  pthread_mutex_unlock(&mn_macaudio.cbmtx);
  return 0;
}

/* Init.
 */
 
int mn_macaudio_init(
  int rate,int chanc,
  void (*cb)(int16_t *dst,int dstc)
) {
  int err;
  if (mn_macaudio.init||!cb) return -1;
  if ((rate<1000)||(rate>1000000)) return -1;
  if ((chanc<1)||(chanc>16)) return -1;
  memset(&mn_macaudio,0,sizeof(mn_macaudio));

  pthread_mutex_init(&mn_macaudio.cbmtx,0);

  mn_macaudio.init=1;
  mn_macaudio.cb=cb;

  AudioComponentDescription desc={0};
  desc.componentType=kAudioUnitType_Output;
  desc.componentSubType=kAudioUnitSubType_DefaultOutput;

  mn_macaudio.component=AudioComponentFindNext(0,&desc);
  if (!mn_macaudio.component) {
    return -1;
  }

  if (AudioComponentInstanceNew(mn_macaudio.component,&mn_macaudio.instance)) {
    return -1;
  }

  if (AudioUnitInitialize(mn_macaudio.instance)) {
    return -1;
  }

  AudioStreamBasicDescription fmt={0};
  fmt.mSampleRate=rate;
  fmt.mFormatID=kAudioFormatLinearPCM;
  fmt.mFormatFlags=kAudioFormatFlagIsSignedInteger; // implies little-endian
  fmt.mFramesPerPacket=1;
  fmt.mChannelsPerFrame=chanc;
  fmt.mBitsPerChannel=16;
  fmt.mBytesPerPacket=chanc*2;
  fmt.mBytesPerFrame=chanc*2;

  if (err=AudioUnitSetProperty(mn_macaudio.instance,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Input,0,&fmt,sizeof(fmt))) {
    return -1;
  }

  AURenderCallbackStruct aucb={0};
  aucb.inputProc=mn_macaudio_cb;
  aucb.inputProcRefCon=0;

  if (AudioUnitSetProperty(mn_macaudio.instance,kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input,0,&aucb,sizeof(aucb))) {
    return -1;
  }

  if (AudioOutputUnitStart(mn_macaudio.instance)) {
    return -1;
  }

  mn_macaudio.state=1;

  return 0;
}

/* Quit.
 */

void mn_macaudio_quit() {
  if (mn_macaudio.state) {
    AudioOutputUnitStop(mn_macaudio.instance);
  }
  if (mn_macaudio.instance) {
    AudioComponentInstanceDispose(mn_macaudio.instance);
  }
  pthread_mutex_destroy(&mn_macaudio.cbmtx);
  memset(&mn_macaudio,0,sizeof(mn_macaudio));
}

/* Callback lock.
 */
 
int mn_macaudio_lock() {
  if (pthread_mutex_lock(&mn_macaudio.cbmtx)) return -1;
  return 0;
}

int mn_macaudio_unlock() {
  if (pthread_mutex_unlock(&mn_macaudio.cbmtx)) return -1;
  return 0;
}
