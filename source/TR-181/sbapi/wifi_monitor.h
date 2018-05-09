/************************************************************************************
  If not stated otherwise in this file or this component's Licenses.txt file the
  following copyright and licenses apply:

  Copyright 2018 RDK Management

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
**************************************************************************/

#ifndef	_WIFI_MON_H_
#define	_WIFI_MON_H_

#define MAX_ASSOCIATED_WIFI_DEVS    64
#define MAX_VAP  2
#define MAX_RADIOS  2
#define MAC_ADDR_LEN	6
#define STA_KEY_LEN		2*MAC_ADDR_LEN + 6
#define MAX_IPC_DATA_LEN    1024
#define KMSG_WRAPPER_FILE_NAME  "/tmp/goodbad-rssi"

typedef unsigned char   mac_addr_t[MAC_ADDR_LEN];
typedef signed short    rssi_t;
typedef char			sta_key_t[STA_KEY_LEN];

#ifndef DEBUG
//#define wifi_dbg_print(level, fmt, args...) do {printf("[WIFI-MONITOR] %s(%d) "fmt, __FUNCTION__, __LINE__, ##args);} while(0)
#define wifi_dbg_print(level, fmt, args...)
#else
void wifi_dbg_print(int level, char *format, ...);
#endif

typedef enum {
    monitor_event_type_diagnostics,
    monitor_event_type_connect,
    monitor_event_type_disconnect,
    monitor_event_type_deauthenticate,
    monitor_event_type_max
} wifi_monitor_event_type_t;

typedef struct {
    unsigned int num;
    wifi_associated_dev_t   devs[MAX_ASSOCIATED_WIFI_DEVS];
} associated_devs_t;

typedef struct {
    mac_addr_t  sta_mac;
	int 	reason;
} auth_deauth_dev_t;

typedef struct {
    unsigned int id;
    wifi_monitor_event_type_t   event_type;
    unsigned int    ap_index;
    union {
        associated_devs_t   devs;
		auth_deauth_dev_t	dev;
    } u;
} __attribute__((__packed__)) wifi_monitor_data_t;

typedef struct {
    mac_addr_t  sta_mac;
    rssi_t  last_rssi;
    unsigned int    good_rssi_time;
    unsigned int    bad_rssi_time;
    unsigned int    connected_time;
    unsigned int    disconnected_time;
    unsigned int    total_connected_time;
    unsigned int    total_disconnected_time;
    struct timeval  last_connected_time;
    unsigned int    rapid_reconnects;
	bool			updated;
} sta_data_t;

typedef struct {
    unsigned int        rapid_reconnect_threshold;
} radio_params_t;

typedef struct {
    queue_t             *queue;
    hash_map_t          *sta_map[MAX_VAP];
    pthread_cond_t      cond;
    pthread_mutex_t     lock;
    pthread_t           id;
    bool                exit_monitor;
	unsigned int		poll_period;
    unsigned int        max_upload_period;
    unsigned int        current_poll_iter;
    struct timeval      last_signalled_time;
    struct timeval      last_polled_time;
	rssi_t				sta_health_rssi_threshold;
    int                 sysevent_fd;
    unsigned int        sysevent_token;
	radio_params_t      radio_params[MAX_RADIOS];
} wifi_monitor_t;

#endif	//_WIFI_MON_H_
