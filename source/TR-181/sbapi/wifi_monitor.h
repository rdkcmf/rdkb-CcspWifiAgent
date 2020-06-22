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

#include "wifi_blaster.h"

#define MAX_ASSOCIATED_WIFI_DEVS    64
#define MAX_VAP  16
#define MAX_RADIOS  2
#define MAC_ADDR_LEN	6
#define STA_KEY_LEN		2*MAC_ADDR_LEN + 6
#define MAX_IPC_DATA_LEN    1024
#define KMSG_WRAPPER_FILE_NAME  "/tmp/goodbad-rssi"

#define CLIENT_STATS_MAX_LEN_BUF    (128)
#define MIN_MAC_ADDR_LEN	2*MAC_ADDR_LEN + 1

#define RADIO_STATS_INTERVAL 30

typedef unsigned char   mac_addr_t[MAC_ADDR_LEN];
typedef signed short    rssi_t;
typedef char			sta_key_t[STA_KEY_LEN];

void wifi_dbg_print(int level, char *format, ...);

typedef enum {
    monitor_event_type_diagnostics,
    monitor_event_type_connect,
    monitor_event_type_disconnect,
    monitor_event_type_deauthenticate,
	monitor_event_type_start_inst_msmt,
	monitor_event_type_stop_inst_msmt,
    monitor_event_type_StatsFlagChange,
    monitor_event_type_RadioStatsFlagChange,
    monitor_event_type_VapStatsFlagChange,
    monitor_event_type_process_active_msmt,
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
    int type;  //Device.WiFi.X_RDKCENTRAL-COM_vAPStatsEnable= 0, Device.WiFi.AccessPoint.<vAP>.X_RDKCENTRAL-COM_StatsEnable = 1
    bool enable; // ture, false
} client_stats_enable_t;

typedef struct {
    mac_addr_t  sta_mac;
	unsigned int ap_index;
	bool active;
} instant_msmt_t;

typedef struct {
    unsigned int id;
    wifi_monitor_event_type_t   event_type;
    unsigned int    ap_index;
    union {
        associated_devs_t   devs;
		auth_deauth_dev_t	dev;
        client_stats_enable_t   flag;
		instant_msmt_t		imsmt;
    } u;
} __attribute__((__packed__)) wifi_monitor_data_t;

typedef struct {
    mac_addr_t  sta_mac;
    unsigned int    good_rssi_time;
    unsigned int    bad_rssi_time;
    unsigned int    connected_time;
    unsigned int    disconnected_time;
    unsigned int    total_connected_time;
    unsigned int    total_disconnected_time;
    struct timeval  last_connected_time;
    struct timeval  last_disconnected_time;
    unsigned int    rapid_reconnects;
	bool			updated;
    wifi_associated_dev3_t dev_stats;
    wifi_associated_dev3_t dev_stats_last;
    unsigned int    reconnect_count;
    long            assoc_monitor_start_time;
    long            gate_time;
    unsigned int    redeauth_count;
    long            deauth_monitor_start_time;
    long            deauth_gate_time;
    active_msmt_data_t *sta_active_msmt_data;
} sta_data_t;

typedef struct {
    unsigned int        rapid_reconnect_threshold;
} ap_params_t;

typedef struct {
	bssid_t			bssid;
	hash_map_t		*sta_map;
	ap_params_t		ap_params;
	ssid_t                  ssid;
} bssid_data_t;

typedef struct {
       char                    frequency_band[64];
       char                    ChannelsInUse[256];
       unsigned int            primary_radio_channel;
       char                    channel_bandwidth[64];
       unsigned int            RadioActivityFactor;
       unsigned int            CarrierSenseThreshold_Exceeded;
       int                     NoiseFloor;
       int                     channelUtil;
       int                     channelInterference;
} radio_data_t;

typedef struct {
    queue_t             *queue;
    bssid_data_t        bssid_data[MAX_VAP];
    radio_data_t        radio_data[MAX_RADIOS];
    pthread_cond_t      cond;
    pthread_mutex_t     lock;
    pthread_t           id;
    bool                exit_monitor;
    unsigned int        poll_period;
    unsigned int        upload_period;
    unsigned int        current_poll_iter;
	instant_msmt_t		inst_msmt;
    struct timeval      last_signalled_time;
    struct timeval      last_polled_time;
    rssi_t		sta_health_rssi_threshold;
    int                 sysevent_fd;
    unsigned int        sysevent_token;
    ap_params_t      	ap_params[MAX_VAP];
    char 		cliStatsList[MAX_VAP];
    int			count;
    int			maxCount;
    int			instantDefReportPeriod;
    int			instantDefOverrideTTL;
    int			instantPollPeriod;
    bool        instntMsmtenable;
    char        instantMac[MIN_MAC_ADDR_LEN];
} wifi_monitor_t;

int
wifi_stats_flag_change
    (
        int             ap_index,
        bool            enable,
        int             type
    );

#endif	//_WIFI_MON_H_
