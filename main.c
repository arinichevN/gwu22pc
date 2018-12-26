#include "main.h"

int app_state = APP_INIT;
int sock_port = -1;
int sock_fd = -1;

Peer peer_client = {.fd = &sock_fd, .addr_size = sizeof peer_client.addr};

DeviceList device_list = LIST_INITIALIZER;
ChannelList channel_list = LIST_INITIALIZER;
FilterList filter_list = LIST_INITIALIZER;//parallel to channel_list
LCorrectionList lcorrection_list = LIST_INITIALIZER;
ThreadList thread_list = LIST_INITIALIZER;


#include "util.c"
#include "init_f.c"

void serverRun ( int *state, int init_state ) {
    SERVER_HEADER
    SERVER_APP_ACTIONS
    DEF_SERVER_I1LIST
    if ( ACP_CMD_IS ( ACP_CMD_GET_FTS ) ) {
        SERVER_PARSE_I1LIST
        FORLISTN ( i1l, i ) {
            Channel *item;
            LIST_GETBYID ( item, &channel_list, i1l.item[i] );
            if ( item == NULL ) continue;
            if ( !catFTS ( item, &response ) ) return;
        }
    }
    acp_responseSend ( &response, &peer_client );
}

void cleanup_handler ( void *arg ) {
    Thread *item = arg;
    printf ( "cleaning up thread %d\n", item->id );
}

void *threadFunction ( void *arg ) {
    Thread *item = arg;
#ifdef MODE_DEBUG
    printf ( "thread for program with id=%d has been started\n", item->id );
#endif
#ifdef MODE_DEBUG
    pthread_cleanup_push ( cleanup_handler, item );
#endif
    size_t n=item->device_plist.length;
    int pin[n];
    double temp[n];
    double hum[n];
    int success[n];
    for ( size_t i=0; i<n; i++ ) {
        pin[i]=item->device_plist.item[i]->pin;
        temp[n]=0.0;
        hum[n]=0.0;
        success[n]=0;
    }
    while ( 1 ) {
        struct timespec t1 = getCurrentTime();
        int old_state;
        if ( threadCancelDisable ( &old_state ) ) {
            deviceListRead ( &item->device_plist,pin, temp, hum, success, n  );
            threadSetCancelState ( old_state );
        }
        sleepRest ( item->cycle_duration, t1 );
    }
#ifdef MODE_DEBUG
    pthread_cleanup_pop ( 1 );
#endif

}

int initApp() {
    if ( !readSettings ( &sock_port, CONF_MAIN_FILE ) ) {
        putsde ( "failed to read settings\n" );
        return 0;
    }
    if ( !initServer ( &sock_fd, sock_port ) ) {
        putsde ( "failed to initialize socket server\n" );
        return 0;
    }
    if ( !gpioSetup() ) {
        freeSocketFd ( &sock_fd );
        putsde ( "failed to initialize GPIO\n" );
        return 0;
    }
    return 1;
}

int initData() {
    initLCorrection ( &lcorrection_list, CONF_LCORRECTION_FILE );
    if ( !initDeviceChannel ( &device_list, &channel_list, CONF_DEVICE_FILE ) ) {
        freeChannelList ( &channel_list );
        FREE_LIST ( &device_list );
        FREE_LIST ( &lcorrection_list );
        return 0;
    }
    assignLCorrection ( &channel_list, &lcorrection_list, CONF_CHANNEL_LCORRECTION_FILE );
    if ( !checkDeviceChannel ( &device_list, &channel_list ) ) {
        freeChannelList ( &channel_list );
        FREE_LIST ( &device_list );
        FREE_LIST ( &lcorrection_list );
        return 0;
    }
    if ( !prepFilterList ( &filter_list, &channel_list ) ) {
        filter_freeList ( &filter_list );
        freeChannelList ( &channel_list );
        FREE_LIST ( &device_list );
        FREE_LIST ( &lcorrection_list );
        return 0;
    }
    filter_initFilterList ( &filter_list, CONF_FILTER_MA_FILE, CONF_FILTER_EXP_FILE, CONF_CHANNEL_FILTER_FILE );
    if ( !initThread ( &thread_list, &device_list, CONF_THREAD_FILE, CONF_THREAD_DEVICE_FILE ) ) {
        freeThreadList ( &thread_list );
        filter_freeList ( &filter_list );
        freeChannelList ( &channel_list );
        FREE_LIST ( &device_list );
        FREE_LIST ( &lcorrection_list );
        return 0;
    }
    if ( !checkThread ( &thread_list ) ) {
        freeThreadList ( &thread_list );
        filter_freeList ( &filter_list );
        freeChannelList ( &channel_list );
        FREE_LIST ( &device_list );
        FREE_LIST ( &lcorrection_list );
        return 0;
    }
    return 1;
}

void freeData() {
    STOP_ALL_LIST_THREADS ( &thread_list );
        freeThreadList ( &thread_list );
        filter_freeList ( &filter_list );
        freeChannelList ( &channel_list );
        FREE_LIST ( &device_list );
        FREE_LIST ( &lcorrection_list );
}

void freeApp() {
    freeData();
    freeSocketFd ( &sock_fd );
    gpioFree();
}

void exit_nicely ( ) {
    freeApp();
    putsdo ( "\nexiting now...\n" );
    exit ( EXIT_SUCCESS );
}

int main ( int argc, char** argv ) {
    if ( geteuid() != 0 ) {
        putsde ( "root user expected\n" );
        return ( EXIT_FAILURE );
    }
#ifndef MODE_DEBUG
    daemon ( 0, 0 );
#endif
    conSig ( &exit_nicely );
    if ( mlockall ( MCL_CURRENT | MCL_FUTURE ) == -1 ) {
        perrorl ( "mlockall()" );
    }
#ifndef MODE_DEBUG
    setPriorityMax ( SCHED_FIFO );
#endif
    int data_initialized = 0;
    while ( 1 ) {
#ifdef MODE_DEBUG
        printf ( "%s(): %s %d\n", F, getAppState ( app_state ), data_initialized );
#endif
        switch ( app_state ) {
        case APP_INIT:
            if ( !initApp() ) {
                return ( EXIT_FAILURE );
            }
            app_state = APP_INIT_DATA;
            break;
        case APP_INIT_DATA:
            data_initialized = initData();
            app_state = APP_RUN;
            break;
        case APP_RUN:
            serverRun ( &app_state, data_initialized );
            break;
        case APP_STOP:
            freeData();
            data_initialized = 0;
            app_state = APP_RUN;
            break;
        case APP_RESET:
            freeApp();
            data_initialized = 0;
            app_state = APP_INIT;
            break;
        case APP_EXIT:
            exit_nicely();
            break;
        default:
            freeApp();
            putsde ( "unknown application state\n" );
            return ( EXIT_FAILURE );
        }
    }
    freeApp();
    return ( EXIT_SUCCESS );
}

