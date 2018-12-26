#include <string.h>

#include "main.h"

int readSettings ( int *sock_port, const char *data_path ) {
    TSVresult tsv = TSVRESULT_INITIALIZER;
    TSVresult* r = &tsv;
    if ( !TSVinit ( r, data_path ) ) {
        TSVclear ( r );
        return 0;
    }
    char *str = TSVgetvalues ( r, 0, "port" );
    if ( str == NULL ) {
        TSVclear ( r );
        return 0;
    }
    *sock_port = atoi ( str );
    TSVclear ( r );
    return 1;
}

int initDeviceChannel ( DeviceList *dlist, ChannelList *clist, const char *data_path ) {
    TSVresult tsv = TSVRESULT_INITIALIZER;
    TSVresult* r = &tsv;
    if ( !TSVinit ( r, data_path ) ) {
        TSVclear ( r );
        return 0;
    }
    int n = TSVntuples ( r );
    if ( n <= 0 ) {
        TSVclear ( r );
        return 1;
    }
    RESIZE_M_LIST ( dlist, n );
    if ( dlist->max_length != n ) {
        putsde ( "failure while resizing device list\n" );
        TSVclear ( r );
        return 0;
    }
    NULL_LIST ( dlist );
    RESIZE_M_LIST ( clist, n*2 );
    if ( clist->max_length != n*2 ) {
        putsde ( "failure while resizing channel list\n" );
        TSVclear ( r );
        return 0;
    }
    NULL_LIST ( clist );
    for ( int i = 0; i < dlist->max_length; i++ ) {
        dlist->item[i].pin = TSVgetis ( r, i, "pin" );
        clist->item[i*2].id = TSVgetis ( r, i, "channel_id_temp" );
        clist->item[i*2+1].id = TSVgetis ( r, i, "channel_id_hum" );
        clist->item[i*2].result.id=clist->item[i*2].id;
        clist->item[i*2+1].result.id=clist->item[i*2+1].id;
        if ( TSVnullreturned ( r ) ) {
            break;
        }
        if ( !initMutex ( &clist->item[i*2].mutex ) ) {
            break;
        }
        if ( !initMutex ( &clist->item[i*2+1].mutex ) ) {
            break;
        }
        dlist->item[i].temp=&clist->item[i*2];
        dlist->item[i].hum=&clist->item[i*2+1];
        dlist->length++;
        clist->length+=2;
    }
    TSVclear ( r );
    if ( dlist->length != dlist->max_length ) {
        putsde ( "failure while reading rows for device list\n" );
        return 0;
    }
    if ( clist->length != clist->max_length ) {
        putsde ( "failure while reading rows for channel list\n" );
        return 0;
    }
    return 1;
}
int assignLCorrection ( ChannelList *clist, LCorrectionList *llist, const char *data_path ) {
    TSVresult tsv = TSVRESULT_INITIALIZER;
    TSVresult* r = &tsv;
    if ( !TSVinit ( r, data_path ) ) {
        TSVclear ( r );
        return 0;
    }
    int n = TSVntuples ( r );
    if ( n <= 0 ) {
        TSVclear ( r );
        return 1;
    }
    int success=1;
    for ( int i = 0; i < n; i++ ) {
        int channel_id = TSVgetis ( r, i, "channel_id" );
        int lcorrection_id = TSVgetis ( r, i, "lcorrection_id" );
        if ( TSVnullreturned ( r ) ) {
            break;
        }
        Channel *channel;
        LIST_GETBYID ( channel, clist, channel_id );
        if ( channel==NULL ) {
            printde ( "channel_id=%d not found in configuration file: %s\n", channel_id,data_path );
            success=0;
            continue;
        }
        LCorrection *lcorrection;
        LIST_GETBYID ( lcorrection, llist, lcorrection_id );
        if ( lcorrection==NULL ) {
            printde ( "lcorrection_id=%d not found in configuration file: %s\n", lcorrection_id,data_path );
            success=0;
        }
        channel->lcorrection=lcorrection;
    }
    TSVclear ( r );
    return success;
}
int prepFilterList ( FilterList *list, ChannelList *cl ) {
    RESIZE_M_LIST ( list, cl->length );
    if ( LML != cl->length ) {
        putsde ( "failure while resizing filter list\n" );
        return 0;
    }
    NULL_LIST ( list );
    for ( int i = 0; i < LML; i++ ) {
        LIi.id = cl->item[i].id;
        LL++;
    }
    if ( LL != LML ) {
        putsde ( "failure while making filter list\n" );
        return 0;
    }
    for(int i=0;i<cl->length;i++){
     cl->item[i].filter=&list->item[i];   
    }
    return 1;
}

static int countThreadItem ( int thread_id_in, TSVresult* r ) {
    int c = 0;
    int n = TSVntuples ( r );
    for ( int k = 0; k < n; k++ ) {
        int thread_id = TSVgetis ( r, k, "thread_id" );
        if ( TSVnullreturned ( r ) ) {
            return 0;
        }
        if ( thread_id == thread_id_in ) {
            c++;
        }
    }
    return c;
}

int initThread ( ThreadList *list, DeviceList *dl,  const char *thread_path, const char *thread_device_path ) {
    TSVresult tsv = TSVRESULT_INITIALIZER;
    TSVresult* r = &tsv;
    if ( !TSVinit ( r, thread_path ) ) {
        TSVclear ( r );
        return 0;
    }
    int n = TSVntuples ( r );
    if ( n <= 0 ) {
        TSVclear ( r );
        putsde ( "no data rows in file\n" );
        return 0;
    }
    RESIZE_M_LIST ( list, n );

    if ( LML != n ) {
        putsde ( "failure while resizing list\n" );
        TSVclear ( r );
        return 0;
    }
    NULL_LIST ( list );
    for ( int i = 0; i < LML; i++ ) {
        LIi.id = TSVgetis ( r, i, "id" );
        LIi.cycle_duration.tv_sec = TSVgetis ( r, i, "cd_sec" );
        LIi.cycle_duration.tv_nsec = TSVgetis ( r, i, "cd_nsec" );
        RESET_LIST ( &LIi.device_plist );
        if ( TSVnullreturned ( r ) ) {
            break;
        }
        LL++;
    }
    TSVclear ( r );
    if ( LL != LML ) {
        putsde ( "failure while reading rows\n" );
        return 0;
    }
    if ( !TSVinit ( r, thread_device_path ) ) {
        TSVclear ( r );
        return 0;
    }
    n = TSVntuples ( r );
    if ( n <= 0 ) {
        putsde ( "no data rows in thread device file\n" );
        TSVclear ( r );
        return 0;
    }

    FORLi {
        int thread_device_count = countThreadItem ( LIi.id, r );
        //allocating memory for thread device pointers
        RESET_LIST ( &LIi.device_plist )
        if ( thread_device_count <= 0 ) {
            continue;
        }
        RESIZE_M_LIST ( &LIi.device_plist, thread_device_count )
        if ( LIi.device_plist.max_length != thread_device_count ) {
            putsde ( "failure while resizing device_plist list\n" );
            TSVclear ( r );
            return 0;
        }
        //assigning channels to this thread
        for ( int k = 0; k < n; k++ ) {
            int thread_id = TSVgetis ( r, k, "thread_id" );
            int device_pin = TSVgetis ( r, k, "device_pin" );
            if ( TSVnullreturned ( r ) ) {
                break;
            }
            if ( thread_id == LIi.id ) {
                Device *d;
                LIST_GETBYFIELD (pin, d, dl, device_pin );
                if ( d!=NULL ) {
                    LIi.device_plist.item[LIi.device_plist.length] = d;
                    LIi.device_plist.length++;
                }
            }
        }
        if ( LIi.device_plist.max_length != LIi.device_plist.length ) {
            putsde ( "failure while assigning devices to threads: some not found\n" );
            TSVclear ( r );
            return 0;
        }
    }
    TSVclear ( r );

    //starting threads
    FORLi {
        if ( !createMThread ( &LIi.thread, &threadFunction, &LIi ) ) {
            return 0;
        }
    }
    return 1;
}
