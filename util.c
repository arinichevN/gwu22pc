
#include "main.h"

int checkDeviceChannel ( DeviceList *dlist, ChannelList *clist ) {
    int valid = 1;
    FORLISTP ( dlist, i ) {
        if ( !checkPin ( dlist->item[i].pin ) ) {
            fprintf ( stderr, "%s(): check device configuration file: bad pin=%d\n", F, dlist->item[i].pin );
            valid = 0;
        }
    }
    //unique channel id
    FORLISTP ( clist, i ) {
        FORLISTPL ( clist, i, j ) {
            if ( clist->item[i].id == clist->item[j].id ) {
                fprintf ( stderr, "%s(): check device configuration file: channel id should be unique, repetition found where channel id=%d\n", F, clist->item[i].id );
                valid = 0;
            }
        }

    }

    return valid;
}
int checkThread ( ThreadList *list ) {
    int valid = 1;
    FORLi {
        //unique device pin
        FORLISTN ( LIi.device_plist, j ) {
            FORLISTNL ( LIi.device_plist, j, k ) {
                if ( LIi.device_plist.item[j]->pin == LIi.device_plist.item[k]->pin ) {
                    fprintf ( stderr, "%s(): check thread_device configuration file: device_pin should be unique, repetition found where thread_id=%d and device_pin=%d\n", F, LIi.id, LIi.device_plist.item[j]->pin );
                    valid = 0;
                }
            }
        }

    }

    return valid;
}

void freeChannelList ( ChannelList *list ) {
    FORLi {
        freeMutex ( &LIi.mutex );
    }
    FREE_LIST ( list );
}

void freeThreadList ( ThreadList *list ) {
    FORLi {
        FREE_LIST ( &LIi.device_plist );
    }
    FREE_LIST ( list );
}

static void updateChannel ( Channel *item, int success, double v, struct timespec tm ) {
    if ( success || ( !success && item->result.state ) ) {
        if ( lockMutex ( &item->mutex ) ) {
            if ( success ) {
                item->result.tm = tm;
                for ( int f = 0; f <  item->filter->af_list.length; f++ ) {
                    item->filter->af_list.item[f].fnc ( &v,  item->filter->af_list.item[f].ptr );
                }
                lcorrect ( &v, item->lcorrection );
                item->result.value = v;
            }
            item->result.state = success;
            unlockMutex ( &item->mutex );
        }
    }
}
void deviceListRead ( DevicePList *list, int *pin, double *temp, double *hum, int *success, size_t length ) {
#ifdef CPU_ANY
    for ( size_t i=0; i<length; i++ ) {
        temp[i]=20.0;
        hum[i]=70.0;
        success[i]=1;
    }
#else
    dht22_readp ( pin, temp, hum, success, length );
#endif
    struct timespec tm =getCurrentTime();
    for ( size_t i=0; i<length; i++ ) {
        updateChannel ( list->item[i]->temp, success[i], temp[i], tm );
        updateChannel ( list->item[i]->hum, success[i], hum[i], tm );
        printdo ( "%d %.3f %.3f %d %ld %ld\n", list->item[i]->temp->id, temp[i],  list->item[i]->temp->result.value, list->item[i]->temp->result.state, list->item[i]->temp->result.tm.tv_sec, list->item[i]->temp->result.tm.tv_nsec );
        printdo ( "%d %.3f %.3f %d %ld %ld\n", list->item[i]->hum->id, hum[i],  list->item[i]->hum->result.value, list->item[i]->hum->result.state, list->item[i]->hum->result.tm.tv_sec, list->item[i]->hum->result.tm.tv_nsec );
    }

}

int catFTS ( Channel *item, ACPResponse * response ) {
    if ( lockMutex ( &item->mutex ) ) {
        int r =  acp_responseFTSCat ( item->id, item->result.value, item->result.tm, item->result.state, response );
        unlockMutex ( &item->mutex );
        return r;
    }
    return 0;
}

void printData ( ACPResponse * response ) {
#define CLi channel_list.item[i]
#define TLi thread_list.item[i]
#define DPLj device_plist.item[j]
    char q[LINE_SIZE];
    snprintf ( q, sizeof q, "CONF_MAIN_FILE: %s\n", CONF_MAIN_FILE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "CONF_DEVICE_FILE: %s\n", CONF_DEVICE_FILE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "CONF_THREAD_FILE: %s\n", CONF_THREAD_FILE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "CONF_THREAD_DEVICE_FILE: %s\n", CONF_THREAD_DEVICE_FILE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "CONF_LCORRECTION_FILE: %s\n", CONF_LCORRECTION_FILE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "CONF_FILTER_MA_FILE: %s\n", CONF_FILTER_MA_FILE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "CONF_FILTER_EXP_FILE: %s\n", CONF_FILTER_EXP_FILE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "CONF_CHANNEL_FILTER_FILE: %s\n", CONF_CHANNEL_FILTER_FILE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "port: %d\n", sock_port );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "app_state: %s\n", getAppState ( app_state ) );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "PID: %d\n", getpid() );
    SEND_STR ( q )

    acp_sendLCorrectionListInfo ( &lcorrection_list, response, &peer_client );

    SEND_STR ( "+-----------------------------------------------+\n" )
    SEND_STR ( "|                    device                     |\n" )
    SEND_STR ( "+-----------+-----------+-----------+-----------+\n" )
    SEND_STR ( "|    pin    |    ptr    |chn_ptr_h  |chn_ptr_t  |\n" )
    SEND_STR ( "+-----------+-----------+-----------+-----------+\n" )
    FORLISTN ( device_list, i ) {
        snprintf ( q, sizeof q, "|%11d|%11p|%11p|%11p|\n",
                   device_list.item[i].pin,
                   (void *) &device_list.item[i],
                   ( void * ) device_list.item[i].temp,
                   ( void * ) device_list.item[i].hum
                 );
        SEND_STR ( q )
    }
    SEND_STR ( "+-----------+-----------+-----------+-----------+\n" )

        SEND_STR ( "+-----------------------+\n" )
    SEND_STR ( "|         thread        |\n" )
    SEND_STR ( "+-----------+-----------+\n" )
    SEND_STR ( "|     id    |device_ptr |\n" )
    SEND_STR ( "+-----------+-----------+\n" )
    FORLISTN ( thread_list, i ) {
        FORLISTN ( TLi.device_plist, j ) {
            snprintf ( q, sizeof q, "|%11d|%11p|\n",
                       TLi.id,
                       ( void * ) TLi.DPLj
                     );
            SEND_STR ( q )
        }
    }
    SEND_STR( "+-----------+-----------+\n" )

    SEND_STR ( "+-----------------------------------------------+\n" )
    SEND_STR ( "|                   channel                     |\n" )
    SEND_STR ( "+-----------+-----------+-----------+-----------+\n" )
    SEND_STR ( "|  pointer  |     id    | lcorr_ptr |filter_ptr |\n" )
    SEND_STR ( "+-----------+-----------+-----------+-----------+\n" )
    FORLISTN ( channel_list, i ) {
        snprintf ( q, sizeof q, "|%11p|%11d|%11p|%11p|\n",
                   ( void * ) &CLi,
                   CLi.id,
                   ( void * ) CLi.lcorrection,
                   ( void * ) CLi.filter
                 );
        SEND_STR ( q )
    }
    SEND_STR ( "+-----------+-----------+-----------+-----------+\n" )

    SEND_STR ( "+-----------------------------------------------------------+\n" )
    SEND_STR ( "|                    channel runtime                        |\n" )
    SEND_STR ( "+-----------+-----------+-----------+-----------+-----------+\n" )
    SEND_STR ( "|     id    |   value   |value_state|  tm_sec   |  tm_nsec  |\n" )
    SEND_STR ( "+-----------+-----------+-----------+-----------+-----------+\n" )
    FORLISTN ( channel_list, i ) {
        snprintf ( q, sizeof q, "|%11d|%11.3f|%11d|%11ld|%11ld|\n",
                   CLi.id,
                   CLi.result.value,
                   CLi.result.state,
                   CLi.result.tm.tv_sec,
                   CLi.result.tm.tv_nsec
                 );
        SEND_STR ( q )
    }
    SEND_STR ( "+-----------+-----------+-----------+-----------+-----------+\n" )

    acp_sendFilterListInfo ( &filter_list, response, &peer_client );
SEND_STR_L ( ".\n" )
#undef CLi
#undef TLi
#undef DPLj

}

void printHelp ( ACPResponse * response ) {
    char q[LINE_SIZE];
    SEND_STR ( "COMMAND LIST\n" )
    snprintf ( q, sizeof q, "%s\tput process into active mode; process will read configuration\n", ACP_CMD_APP_START );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tput process into standby mode; all running programs will be stopped\n", ACP_CMD_APP_STOP );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tfirst stop and then start process\n", ACP_CMD_APP_RESET );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tterminate process\n", ACP_CMD_APP_EXIT );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget state of process; response: B - process is in active mode, I - process is in standby mode\n", ACP_CMD_APP_PING );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget some variable's values; response will be packed into multiple packets\n", ACP_CMD_APP_PRINT );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget this help; response will be packed into multiple packets\n", ACP_CMD_APP_HELP );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget temperature in format: sensorId\\ttemperature\\ttimeSec\\ttimeNsec\\tvalid; program id expected\n", ACP_CMD_GET_FTS );
    SEND_STR_L ( q )
}
