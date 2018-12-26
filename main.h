
#ifndef GWU22PC_H
#define GWU22PC_H

#include "lib/app.h"
#include "lib/timef.h"
#include "lib/acp/main.h"
#include "lib/acp/app.h"
#include "lib/udp.h"
#include "lib/tsv.h"
#include "lib/lcorrection.h"
#include "lib/device/dht22.h"
#include "lib/filter/common.h"

#define APP_NAME gwu22pc
#define APP_NAME_STR TOSTRING(APP_NAME)

#ifdef MODE_FULL
#define CONF_DIR "/etc/controller/" APP_NAME_STR "/"
#endif
#ifndef MODE_FULL
#define CONF_DIR "./config/"
#endif
#define CONF_MAIN_FILE CONF_DIR "main.tsv"
#define CONF_DEVICE_FILE CONF_DIR "device.tsv"
#define CONF_THREAD_FILE CONF_DIR "thread.tsv"
#define CONF_THREAD_DEVICE_FILE CONF_DIR "thread_device.tsv"
#define CONF_LCORRECTION_FILE CONF_DIR "lcorrection.tsv"
#define CONF_FILTER_MA_FILE CONF_DIR "filter_ma.tsv"
#define CONF_FILTER_EXP_FILE CONF_DIR "filter_exp.tsv"
#define CONF_CHANNEL_FILTER_FILE CONF_DIR "channel_filter.tsv"
#define CONF_CHANNEL_LCORRECTION_FILE CONF_DIR "channel_lcorrection.tsv"


typedef struct {
    int id;
    FTS result;
    LCorrection *lcorrection;
    Filter *filter;
    Mutex mutex;
} Channel;
DEC_LIST ( Channel )


typedef struct {
    int pin;
    Channel *temp;
    Channel *hum;
} Device;
DEC_LIST ( Device )
DEC_PLIST ( Device )


struct thread_st {
    int id;
    DevicePList device_plist;
    pthread_t thread;
    struct timespec cycle_duration;
};
typedef struct thread_st Thread;
DEC_LIST ( Thread )

extern int readSettings();

extern void serverRun ( int *state, int init_state );

extern void *threadFunction ( void *arg );

extern int initApp();

extern int initData();

extern void freeData();

extern void freeApp();

extern void exit_nicely();

#endif

