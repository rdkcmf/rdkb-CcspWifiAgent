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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include "collection.h"
#include "wifi_hal.h"
#include "wifi_monitor.h"
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/un.h>
#include <assert.h>
#include "ansc_status.h"
#include <sysevent/sysevent.h>
#include "ccsp_base_api.h"
#include "harvester.h"

extern void* bus_handle;
extern char g_Subsystem[32];

#define SINGLE_CLIENT_WIFI_AVRO_FILENAME "WifiSingleClient.avsc"
#define MIN_MAC_LEN 12
#define DEFAULT_INSTANT_POLL_TIME 5
#define DEFAULT_INSTANT_REPORT_TIME 0
char *instSchemaIdBuffer = "8b27dafc-0c4d-40a1-b62c-f24a34074914/4388e585dd7c0d32ac47e71f634b579b";

static wifi_monitor_t g_monitor_module;
static unsigned msg_id = 1000;
static const char *wifi_health_log = "/rdklogs/logs/wifihealth.txt";
static unsigned char vap_up_arr[MAX_VAP]={0};
static unsigned int vap_iteration=0;

pthread_mutex_t g_apRegister_lock = PTHREAD_MUTEX_INITIALIZER;
void deinit_wifi_monitor    (void);
int device_deauthenticated(int apIndex, char *mac, int reason);
int device_associated(int apIndex, wifi_associated_dev_t *associated_dev);
void associated_devices_diagnostics    (void);
unsigned int get_upload_period  (int,int);
long get_sys_uptime();
void process_disconnect    (unsigned int ap_index, auth_deauth_dev_t *dev);
static void get_device_flag(char flag[], char *psmcli);
static void logVAPUpStatus();
extern BOOL sWiFiDmlvApStatsFeatureEnableCfg;

void associated_client_diagnostics();
void process_instant_msmt_stop (unsigned int ap_index, instant_msmt_t *msmt);
void process_instant_msmt_start        (unsigned int ap_index, instant_msmt_t *msmt); 

static inline char *to_sta_key    (mac_addr_t mac, sta_key_t key) {
    snprintf(key, STA_KEY_LEN, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return (char *)key;
}

static void to_mac_bytes (mac_addr_str_t key, mac_address_t bmac) {
   unsigned int mac[6];
   if(strlen(key) > MIN_MAC_LEN)
       sscanf(key, "%02x:%02x:%02x:%02x:%02x:%02x",
             &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
   else
       sscanf(key, "%02x%02x%02x%02x%02x%02x",
             &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
   bmac[0] = mac[0]; bmac[1] = mac[1]; bmac[2] = mac[2];
   bmac[3] = mac[3]; bmac[4] = mac[4]; bmac[5] = mac[5];

}

char *get_formatted_time(char *time)
{
    struct tm *tm_info;
    struct timeval tv_now;
	char tmp[128];

    gettimeofday(&tv_now, NULL);
   	tm_info = localtime(&tv_now.tv_sec);
        
    strftime(tmp, 128, "%y%m%d-%T", tm_info);
        
	snprintf(time, 128, "%s.%06d", tmp, tv_now.tv_usec);
	return time;
}

void write_to_file(const char *file_name, char *fmt, ...)
{
    FILE *fp = NULL;
    va_list args;

    fp = fopen(file_name, "a+");
    if (fp == NULL) {
        return;
    }
   
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);

    fflush(fp);
    fclose(fp);
}

void upload_ap_telemetry_data()
{
    char buff[1024];
    char tmp[128];
	wifi_radioTrafficStats2_t stats;
	unsigned int i;
    
	for (i = 0; i < MAX_RADIOS; i++) {
		wifi_getRadioTrafficStats2(i, &stats);
		get_formatted_time(tmp);
		snprintf(buff, 1024, "%s WIFI_NOISE_FLOOR_%d:%d\n", tmp, i + 1, stats.radio_NoiseFloor);

		write_to_file(wifi_health_log, buff);
		wifi_dbg_print(1, "%s", buff);
	}
}

BOOL client_fast_reconnect(unsigned int apIndex, char *mac)
{
    extern int assocCountThreshold;
    extern int assocMonitorDuration;
    extern int assocGateTime;
    sta_data_t  *sta;
    hash_map_t  *sta_map;
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);

    if(!assocMonitorDuration) {
             wifi_dbg_print(1, "%s: Client fast reconnection check disabled, assocMonitorDuration:%d \n", __func__, assocMonitorDuration);
             return FALSE;
    }

    wifi_dbg_print(1, "%s: Checking for client:%s connection on ap:%d\n", __func__, mac, apIndex);

    pthread_mutex_lock(&g_monitor_module.lock);
    sta_map = g_monitor_module.bssid_data[apIndex].sta_map;
    sta = (sta_data_t *)hash_map_get(sta_map, mac);
    if (sta == NULL) {
        wifi_dbg_print(1, "%s: Client:%s could not be found on sta map of ap:%d\n", __func__, mac, apIndex);
        pthread_mutex_unlock(&g_monitor_module.lock);
        return FALSE;
    }

    if(sta->gate_time && (tv_now.tv_sec < sta->gate_time)) {
             wifi_dbg_print(1, "%s: Blocking burst client connections for few more seconds\n", __func__);
             pthread_mutex_unlock(&g_monitor_module.lock);
             return TRUE;
    } else {
             wifi_dbg_print(1, "%s: processing further\n", __func__);
    }

    wifi_dbg_print(1, "%s: assocCountThreshold:%d assocMonitorDuration:%d assocGateTime:%d \n", __func__, assocCountThreshold, assocMonitorDuration, assocGateTime);

    if((tv_now.tv_sec - sta->assoc_monitor_start_time) < assocMonitorDuration)
    {
        sta->reconnect_count++;
        wifi_dbg_print(1, "%s: reconnect_count:%d \n", __func__, sta->reconnect_count);
        if(sta->reconnect_count > assocCountThreshold) {
             wifi_dbg_print(1, "%s: Blocking client connections for assocGateTime:%d \n", __func__, assocGateTime);
             sta->reconnect_count = 0;
             sta->gate_time = tv_now.tv_sec + assocGateTime;
             pthread_mutex_unlock(&g_monitor_module.lock);
             return TRUE;
        }
    } else {
             sta->assoc_monitor_start_time = tv_now.tv_sec;
             sta->reconnect_count = 0;
             sta->gate_time = 0;
             wifi_dbg_print(1, "%s: resetting reconnect_count and assoc_monitor_start_time \n", __func__);
             pthread_mutex_unlock(&g_monitor_module.lock);
             return FALSE;
    }
    pthread_mutex_unlock(&g_monitor_module.lock);
    return FALSE;
}

BOOL client_fast_redeauth(unsigned int apIndex, char *mac)
{
    extern int deauthCountThreshold;
    extern int deauthMonitorDuration;
    extern int deauthGateTime;
    sta_data_t  *sta;
    hash_map_t  *sta_map;
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);

    if(!deauthMonitorDuration) {
             wifi_dbg_print(1, "%s: Client fast deauth check disabled, deauthMonitorDuration:%d \n", __func__, deauthMonitorDuration);
             return FALSE;
    }

    wifi_dbg_print(1, "%s: Checking for client:%s deauth on ap:%d\n", __func__, mac, apIndex);

    pthread_mutex_lock(&g_monitor_module.lock);
    sta_map = g_monitor_module.bssid_data[apIndex].sta_map;
    sta = (sta_data_t *)hash_map_get(sta_map, mac);

    if (sta == NULL  ) {
        wifi_dbg_print(1, "%s: Client:%s could not be found on sta map of ap:%d,  Blocking client deauth notification\n", __func__, mac, apIndex);
        pthread_mutex_unlock(&g_monitor_module.lock);
        return TRUE;
    }

    if(sta->deauth_gate_time && (tv_now.tv_sec < sta->deauth_gate_time)) {
             wifi_dbg_print(1, "%s: Blocking burst client deauth for few more seconds\n", __func__);
             pthread_mutex_unlock(&g_monitor_module.lock);
             return TRUE;
    } else {
             wifi_dbg_print(1, "%s: processing further\n", __func__);
    }

    wifi_dbg_print(1, "%s: deauthCountThreshold:%d deauthMonitorDuration:%d deauthGateTime:%d \n", __func__, deauthCountThreshold, deauthMonitorDuration, deauthGateTime);

    if((tv_now.tv_sec - sta->deauth_monitor_start_time) < deauthMonitorDuration)
    {
        sta->redeauth_count++;
        wifi_dbg_print(1, "%s: redeauth_count:%d \n", __func__, sta->redeauth_count);
        if(sta->redeauth_count > deauthCountThreshold) {
             wifi_dbg_print(1, "%s: Blocking client deauth for deauthGateTime:%d \n", __func__, deauthGateTime);
             sta->redeauth_count = 0;
             sta->deauth_gate_time = tv_now.tv_sec + deauthGateTime;
             pthread_mutex_unlock(&g_monitor_module.lock);
             return TRUE;
        }
    } else {
             sta->deauth_monitor_start_time = tv_now.tv_sec;
             sta->redeauth_count = 0;
             sta->deauth_gate_time = 0;
             wifi_dbg_print(1, "%s: resetting redeauth_count and deauth_monitor_start_time \n", __func__);
             pthread_mutex_unlock(&g_monitor_module.lock);
             return FALSE;
    }
    pthread_mutex_unlock(&g_monitor_module.lock);
    return FALSE;
}

#define MAX_BUFFER 4096
#define TELEMETRY_MAX_BUFFER 4096
void upload_client_telemetry_data()
{
    hash_map_t     *sta_map;
    sta_key_t    sta_key;
    unsigned int i, num_devs;;
    sta_data_t *sta;
    char buff[MAX_BUFFER];
    char telemetryBuff[TELEMETRY_MAX_BUFFER] = { '\0' };
    char tmp[128];
    int rssi, rapid_reconn_max;
    bool sendIndication = false;
    char trflag[MAX_VAP] = {0};
    char nrflag[MAX_VAP] = {0};
    char stflag[MAX_VAP] = {0};
    char snflag[MAX_VAP] = {0};
    char vap_status[16];
    wifi_VAPTelemetry_t telemetry;
	bool enable24detailstats = false;
	bool enable5detailstats = false;

	// IsCosaDmlWiFivAPStatsFeatureEnabled needs to be set to get telemetry of some stats, the TR object is 
	// Device.WiFi.X_RDKCENTRAL-COM_vAPStatsEnable

    get_device_flag(trflag, "dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WHIX.TxRxRateList");
    get_device_flag(nrflag, "dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WHIX.NormalizedRssiList");
    get_device_flag(stflag, "dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WHIX.CliStatList");
#if !defined (_XB6_PRODUCT_REQ_) && !defined(_XF3_PRODUCT_REQ_) && !defined(_CBR_PRODUCT_REQ_) && !defined(_HUB4_PRODUCT_REQ_)
	// see if list has changed
	if (strncmp(stflag, g_monitor_module.cliStatsList, MAX_VAP) != 0) {
		strncpy(g_monitor_module.cliStatsList, stflag, MAX_VAP);
		// check if we should enable of disable detailed client stats collection on XB3

		for (i = 0; i < MAX_VAP; i++) {
			if ((i%2) == 0) {
				// 2.4GHz radio
				if (stflag[i] == 1) {
					enable24detailstats = true;
				}
			} else {
				// 5GHz radio
				if (stflag[i] == 1) {
					enable5detailstats = true;
				}
			}
		}

       	wifi_dbg_print(1, "%s:%d: client detailed stats collection for 2.4GHz radio set to %s\n", __func__, __LINE__, 
			(enable24detailstats == true)?"enabled":"disabled");
       	wifi_dbg_print(1, "%s:%d: client detailed stats collection for 5GHz radio set to %s\n", __func__, __LINE__, 
			(enable5detailstats == true)?"enabled":"disabled");
#if !defined(_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_)
		wifi_setClientDetailedStatisticsEnable(0, enable24detailstats);
		wifi_setClientDetailedStatisticsEnable(1, enable5detailstats);
#endif
	}
#endif
    get_device_flag(snflag, "dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WIFI_TELEMETRY.SNRList");
    memset(buff, 0, MAX_BUFFER);

    for (i = 0; i < MAX_VAP; i++) {
        sta_map = g_monitor_module.bssid_data[i].sta_map;
        memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
        int compare = i + 1 ;
            get_formatted_time(tmp);
       	    snprintf(buff, 2048, "%s WIFI_MAC_%d:", tmp, i + 1);

	    num_devs = 0;
       	    sta = hash_map_get_first(sta_map);
       	    while (sta != NULL) {
                if (sta->dev_stats.cli_Active == true) {
		    snprintf(tmp, 32, "%s,", to_sta_key(sta->sta_mac, sta_key));
		    strncat(buff, tmp, MAX_BUFFER - strlen(buff) - 1);
		    strncat(telemetryBuff, tmp, TELEMETRY_MAX_BUFFER - strlen(buff) - 1);
		    num_devs++;
		}
	        sta = hash_map_get_next(sta_map, sta);
       	    }
       	    strncat(buff, "\n", MAX_BUFFER - strlen(buff) - 1);
       	    write_to_file(wifi_health_log, buff);
       	    /*
            "header": "2GclientMac_split", "content": "WIFI_MAC_1:", "type": "wifihealth.txt",
            "header": "5GclientMac_split", "content": "WIFI_MAC_2:", "type": "wifihealth.txt",
            "header": "xh_mac_3_split",    "content": "WIFI_MAC_3:", "type": "wifihealth.txt",
            "header": "xh_mac_4_split",    "content": "WIFI_MAC_4:", "type": "wifihealth.txt",
       	     */
            #if defined(ENABLE_FEATURE_TELEMETRY2_0)
       	    if (1 == compare) {
       	        t2_event_s("2GclientMac_split", telemetryBuff);
       	    } else if (2 == compare) {
       	        t2_event_s("5GclientMac_split", telemetryBuff);
       	    } else if (3 == compare) {
                t2_event_s("xh_mac_3_split", telemetryBuff);
            } else if (4 == compare) {
                t2_event_s("xh_mac_4_split", telemetryBuff);
            }
            #endif

       	    wifi_dbg_print(1, "%s", buff);

	    get_formatted_time(tmp);
	    snprintf(buff, 2048, "%s WIFI_MAC_%d_TOTAL_COUNT:%d\n", tmp, i + 1, num_devs);
	    write_to_file(wifi_health_log, buff);
	    //    "header": "Total_2G_clients_split", "content": "WIFI_MAC_1_TOTAL_COUNT:", "type": "wifihealth.txt",
	    //    "header": "Total_5G_clients_split", "content": "WIFI_MAC_2_TOTAL_COUNT:","type": "wifihealth.txt",
	    //    "header": "xh_cnt_1_split","content": "WIFI_MAC_3_TOTAL_COUNT:","type": "wifihealth.txt",
            //    "header": "xh_cnt_2_split","content": "WIFI_MAC_4_TOTAL_COUNT:","type": "wifihealth.txt",
            #if defined(ENABLE_FEATURE_TELEMETRY2_0)
            if (1 == compare) {
                if (0 == num_devs) {
                    t2_event_d("WIFI_INFO_Zero_2G_Clients", 1);
                } else {
                	t2_event_d("Total_2G_clients_split", num_devs);
                }

            } else if (2 == compare) {
                if(0 == num_devs) {
                    t2_event_d("WIFI_INFO_Zero_5G_Clients", 1);
                } else{
                	t2_event_d("Total_5G_clients_split", num_devs);
                }

            } else if (3 == compare) {
                    t2_event_d("xh_cnt_1_split", num_devs);
            } else if (4 == compare) {
                    t2_event_d("xh_cnt_2_split", num_devs);
            }
            #endif

            wifi_dbg_print(1, "%s", buff);

	get_formatted_time(tmp);
	memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
       	snprintf(buff, 2048, "%s WIFI_RSSI_%d:", tmp, i + 1);
       	sta = hash_map_get_first(sta_map);
       	while (sta != NULL) {
			if (sta->dev_stats.cli_Active == true) {
				snprintf(tmp, 32, "%d,", sta->dev_stats.cli_RSSI);
				strncat(buff, tmp, 128);
				strncat(telemetryBuff, tmp, 128);
			}

			sta = hash_map_get_next(sta_map, sta);
       	}
       	strncat(buff, "\n", 2);
       	write_to_file(wifi_health_log, buff);
        #if defined(ENABLE_FEATURE_TELEMETRY2_0)
        if ( 1 == compare ) {
            t2_event_s("2GRSSI_split", telemetryBuff);
        } else if ( 2 == compare ) {
            t2_event_s("5GRSSI_split", telemetryBuff);
        } else if ( 3 == compare ) {
            t2_event_s("xh_rssi_3_split", telemetryBuff);
        }
        #endif
       	wifi_dbg_print(1, "%s", buff);

		get_formatted_time(tmp);
	memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
       	snprintf(buff, 2048, "%s WIFI_CHANNEL_WIDTH_%d:", tmp, i + 1);
       	sta = hash_map_get_first(sta_map);
       	while (sta != NULL) {
			if (sta->dev_stats.cli_Active == true) {
				snprintf(tmp, 64, "%s,", sta->dev_stats.cli_OperatingChannelBandwidth);
				strncat(buff, tmp, 128);
				strncat(telemetryBuff, tmp, 128);
			}

			sta = hash_map_get_next(sta_map, sta);
       	}
       	strncat(buff, "\n", 2);
       	write_to_file(wifi_health_log, buff);
        #if defined(ENABLE_FEATURE_TELEMETRY2_0)
        if ( 1 == compare ) {
            t2_event_s("WIFI_CW_1_split", telemetryBuff);
        } else if ( 2 == compare ) {
            t2_event_s("WIFI_CW_2_split", telemetryBuff);
        }
        #endif
       	wifi_dbg_print(1, "%s", buff);

		if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && nrflag[i]) {
			get_formatted_time(tmp);
       		snprintf(buff, 2048, "%s WIFI_NORMALIZED_RSSI_%d:", tmp, i + 1);
       		sta = hash_map_get_first(sta_map);
       		while (sta != NULL) {
				if (sta->dev_stats.cli_Active == true) {
					snprintf(tmp, 32, "%d,", sta->dev_stats.cli_SignalStrength);
					strncat(buff, tmp, 128);
				}
            
				sta = hash_map_get_next(sta_map, sta);
       		}
       		strncat(buff, "\n", 2);
       		write_to_file(wifi_health_log, buff);
       		wifi_dbg_print(1, "%s", buff);
		}	

		if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && snflag[i]) {
			get_formatted_time(tmp);
		memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
       		snprintf(buff, 2048, "%s WIFI_SNR_%d:", tmp, i + 1);
       		sta = hash_map_get_first(sta_map);
       		while (sta != NULL) {
				if (sta->dev_stats.cli_Active == true) {
					snprintf(tmp, 32, "%d,", sta->dev_stats.cli_SNR);
					strncat(buff, tmp, 128);
					strncat(telemetryBuff, tmp, 128);
				}
            
				sta = hash_map_get_next(sta_map, sta);
       		}
       		strncat(buff, "\n", 2);
       		write_to_file(wifi_health_log, buff);
                #if defined(ENABLE_FEATURE_TELEMETRY2_0)
                if ( 1 == compare ) {
                    t2_event_s("WIFI_SNR_1_split", telemetryBuff);
                } else if ( 2 == compare ) {
                    t2_event_s("WIFI_SNR_2_split", telemetryBuff);
                }
                #endif
       		wifi_dbg_print(1, "%s", buff);
		}

		get_formatted_time(tmp);
	memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
       	snprintf(buff, 2048, "%s WIFI_TXCLIENTS_%d:", tmp, i + 1);
       	sta = hash_map_get_first(sta_map);
       	while (sta != NULL) {
			if (sta->dev_stats.cli_Active == true) {
				snprintf(tmp, 32, "%d,", sta->dev_stats.cli_LastDataDownlinkRate);
				strncat(buff, tmp, 128);
				strncat(telemetryBuff, tmp, 128);
			}
            
			sta = hash_map_get_next(sta_map, sta);
       	}
       	strncat(buff, "\n", 2);
       	write_to_file(wifi_health_log, buff);
        #if defined(ENABLE_FEATURE_TELEMETRY2_0)
        if ( 1 == compare ) {
            t2_event_s("WIFI_TX_1_split", telemetryBuff);
        } else if ( 2 == compare ) {
            t2_event_s("WIFI_TX_2_split", telemetryBuff);
        }
        #endif
       	wifi_dbg_print(1, "%s", buff);

		get_formatted_time(tmp);
	memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
       	snprintf(buff, 2048, "%s WIFI_RXCLIENTS_%d:", tmp, i + 1);
       	sta = hash_map_get_first(sta_map);
       	while (sta != NULL) {
			if (sta->dev_stats.cli_Active == true) {
				snprintf(tmp, 32, "%d,", sta->dev_stats.cli_LastDataUplinkRate);
				strncat(buff, tmp, 128);
				strncat(telemetryBuff, tmp, 128);
			}
            
			sta = hash_map_get_next(sta_map, sta);
       	}
       	strncat(buff, "\n", 2);
       	write_to_file(wifi_health_log, buff);

       	//  "header": "WIFI_RX_1_split", "content": "WIFI_RXCLIENTS_1:", "type": "wifihealth.txt",
        #if defined(ENABLE_FEATURE_TELEMETRY2_0)
       	if ( 1 == compare ) {
       	    t2_event_s("WIFI_RX_1_split", telemetryBuff);
       	} else if ( 2 == compare ) {
       	    t2_event_s("WIFI_RX_2_split", telemetryBuff);
       	}
        #endif

       	wifi_dbg_print(1, "%s", buff);
	
		if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && trflag[i]) {
			get_formatted_time(tmp);
		memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
       		snprintf(buff, 2048, "%s WIFI_MAX_TXCLIENTS_%d:", tmp, i + 1);
       		sta = hash_map_get_first(sta_map);
       		while (sta != NULL) {
				if (sta->dev_stats.cli_Active == true) {
					snprintf(tmp, 32, "%u,", sta->dev_stats.cli_MaxDownlinkRate);
					strncat(buff, tmp, 128);
					strncat(telemetryBuff, tmp, 128);
				}
            
				sta = hash_map_get_next(sta_map, sta);
       		}
       		strncat(buff, "\n", 2);
       		write_to_file(wifi_health_log, buff);
                #if defined(ENABLE_FEATURE_TELEMETRY2_0)
                if ( 1 == compare ) {
                    t2_event_s("MAXTX_1_split", telemetryBuff);
                } else if ( 2 == compare ) {
                    t2_event_s("MAXTX_2_split", telemetryBuff);
                }
                #endif
       		wifi_dbg_print(1, "%s", buff);
		}

		if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && trflag[i]) {
			get_formatted_time(tmp);
		memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
       		snprintf(buff, 2048, "%s WIFI_MAX_RXCLIENTS_%d:", tmp, i + 1);
       		sta = hash_map_get_first(sta_map);
       		while (sta != NULL) {
				if (sta->dev_stats.cli_Active == true) {
					snprintf(tmp, 32, "%u,", sta->dev_stats.cli_MaxUplinkRate);
					strncat(buff, tmp, 128);
					strncat(telemetryBuff, tmp, 128);
				}
            
				sta = hash_map_get_next(sta_map, sta);
       		}
       		strncat(buff, "\n", 2);
       		write_to_file(wifi_health_log, buff);
                #if defined(ENABLE_FEATURE_TELEMETRY2_0)
                if ( 1 == compare ) {
                    t2_event_s("MAXRX_1_split", telemetryBuff);
                } else if ( 2 == compare ) {
                    t2_event_s("MAXRX_2_split", telemetryBuff);
                }
                #endif
       		wifi_dbg_print(1, "%s", buff);
		}

                if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && trflag[i])
                {
                    get_formatted_time(tmp);
                    snprintf(buff, 2048, "%s WIFI_RXTXCLIENTDELTA_%d:", tmp, i + 1);
                    sta = hash_map_get_first(sta_map);
                    while (sta != NULL)
                    {
                        if (sta->dev_stats.cli_Active == true)
                        {
                            snprintf(tmp, 32, "%u,", (sta->dev_stats.cli_LastDataDownlinkRate - sta->dev_stats.cli_LastDataUplinkRate));
                            strncat(buff, tmp, 128);
                        }
                        sta = hash_map_get_next(sta_map, sta);
                    }
                    strncat(buff, "\n", 2);
                    write_to_file(wifi_health_log, buff);
                    wifi_dbg_print(1, "%s", buff);
                }

		if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && stflag[i]) {
			get_formatted_time(tmp);
       		snprintf(buff, 2048, "%s WIFI_BYTESSENTCLIENTS_%d:", tmp, i + 1);
       		sta = hash_map_get_first(sta_map);
       		while (sta != NULL) {
				if (sta->dev_stats.cli_Active == true) {
					snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_BytesSent - sta->dev_stats_last.cli_BytesSent);
					strncat(buff, tmp, 128);
				}
            
				sta = hash_map_get_next(sta_map, sta);
       		}
       		strncat(buff, "\n", 2);
       		write_to_file(wifi_health_log, buff);
       		wifi_dbg_print(1, "%s", buff);
		}

		if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && stflag[i]) {
			get_formatted_time(tmp);
       		snprintf(buff, 2048, "%s WIFI_BYTESRECEIVEDCLIENTS_%d:", tmp, i + 1);
       		sta = hash_map_get_first(sta_map);
       		while (sta != NULL) {
				if (sta->dev_stats.cli_Active == true) {
					snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_BytesReceived - sta->dev_stats_last.cli_BytesReceived);
					strncat(buff, tmp, 128);
				}
            
				sta = hash_map_get_next(sta_map, sta);
       		}
       		strncat(buff, "\n", 2);
       		write_to_file(wifi_health_log, buff);
       		wifi_dbg_print(1, "%s", buff);
		}

		if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && stflag[i]) {
			get_formatted_time(tmp);
		memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
       		snprintf(buff, 2048, "%s WIFI_PACKETSSENTCLIENTS_%d:", tmp, i + 1);
       		sta = hash_map_get_first(sta_map);
       		while (sta != NULL) {
				if (sta->dev_stats.cli_Active == true) {
					snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_PacketsSent - sta->dev_stats_last.cli_PacketsSent);
					strncat(buff, tmp, 128);
					strncat(telemetryBuff, tmp, 128);
				}
            
				sta = hash_map_get_next(sta_map, sta);
       		}
       		strncat(buff, "\n", 2);
       		write_to_file(wifi_health_log, buff);
                #if defined(ENABLE_FEATURE_TELEMETRY2_0)
                if ( 1 == compare ) {
                    t2_event_s("WIFI_PACKETSSENTCLIENTS_1_split", telemetryBuff);
                } else if ( 2 == compare ) {
                    t2_event_s("WIFI_PACKETSSENTCLIENTS_2_split", telemetryBuff);
                }
                #endif
       		wifi_dbg_print(1, "%s", buff);
		}

		if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && stflag[i]) {
			get_formatted_time(tmp);
       		snprintf(buff, 2048, "%s WIFI_PACKETSRECEIVEDCLIENTS_%d:", tmp, i + 1);
       		sta = hash_map_get_first(sta_map);
       		while (sta != NULL) {
				if (sta->dev_stats.cli_Active == true) {
					snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_PacketsReceived - sta->dev_stats_last.cli_PacketsReceived);
					strncat(buff, tmp, 128);
				}
            
				sta = hash_map_get_next(sta_map, sta);
       		}
       		strncat(buff, "\n", 2);
       		write_to_file(wifi_health_log, buff);
       		wifi_dbg_print(1, "%s", buff);
		}

		if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && stflag[i]) {
			get_formatted_time(tmp);
		memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
       		snprintf(buff, 2048, "%s WIFI_ERRORSSENT_%d:", tmp, i + 1);
       		sta = hash_map_get_first(sta_map);
       		while (sta != NULL) {
				if (sta->dev_stats.cli_Active == true) {
					snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_ErrorsSent - sta->dev_stats_last.cli_ErrorsSent);
					strncat(buff, tmp, 128);
					strncat(telemetryBuff, tmp, 128);
				}
            
				sta = hash_map_get_next(sta_map, sta);
       		}
       		strncat(buff, "\n", 2);
       		write_to_file(wifi_health_log, buff);
                #if defined(ENABLE_FEATURE_TELEMETRY2_0)
                if ( 1 == compare ) {
                    t2_event_s("WIFI_ERRORSSENT_1_split", telemetryBuff);
                } else if ( 2 == compare ) {
                    t2_event_s("WIFI_ERRORSSENT_2_split", telemetryBuff);
                }
                #endif
       		wifi_dbg_print(1, "%s", buff);
		}

		if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && stflag[i]) {
			get_formatted_time(tmp);
       		snprintf(buff, 2048, "%s WIFI_RETRANSCOUNT_%d:", tmp, i + 1);
       		sta = hash_map_get_first(sta_map);
       		while (sta != NULL) {
				if (sta->dev_stats.cli_Active == true) {
					snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_RetransCount - sta->dev_stats_last.cli_RetransCount);
					strncat(buff, tmp, 128);
				}
            
				sta = hash_map_get_next(sta_map, sta);
       		}
       		strncat(buff, "\n", 2);
       		write_to_file(wifi_health_log, buff);
       		wifi_dbg_print(1, "%s", buff);
		}

		if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && stflag[i]) {
			get_formatted_time(tmp);
       		snprintf(buff, 2048, "%s WIFI_FAILEDRETRANSCOUNT_%d:", tmp, i + 1);
       		sta = hash_map_get_first(sta_map);
       		while (sta != NULL) {
				if (sta->dev_stats.cli_Active == true) {
					snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_FailedRetransCount - sta->dev_stats_last.cli_FailedRetransCount);
					strncat(buff, tmp, 128);
				}
            
				sta = hash_map_get_next(sta_map, sta);
       		}
       		strncat(buff, "\n", 2);
       		write_to_file(wifi_health_log, buff);
       		wifi_dbg_print(1, "%s", buff);
		}

		if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && stflag[i]) {
			get_formatted_time(tmp);
       		snprintf(buff, 2048, "%s WIFI_RETRYCOUNT_%d:", tmp, i + 1);
       		sta = hash_map_get_first(sta_map);
       		while (sta != NULL) {
				if (sta->dev_stats.cli_Active == true) {
					snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_RetryCount - sta->dev_stats_last.cli_RetryCount);
					strncat(buff, tmp, 128);
				}
            
				sta = hash_map_get_next(sta_map, sta);
       		}
       		strncat(buff, "\n", 2);
       		write_to_file(wifi_health_log, buff);
       		wifi_dbg_print(1, "%s", buff);
		}

		if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && stflag[i]) {
			get_formatted_time(tmp);
       		snprintf(buff, 2048, "%s WIFI_MULTIPLERETRYCOUNT_%d:", tmp, i + 1);
       		sta = hash_map_get_first(sta_map);
       		while (sta != NULL) {
				if (sta->dev_stats.cli_Active == true) {
					snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_MultipleRetryCount - sta->dev_stats_last.cli_MultipleRetryCount);
					strncat(buff, tmp, 128);
				}
            
				sta = hash_map_get_next(sta_map, sta);
       		}
       		strncat(buff, "\n", 2);
       		write_to_file(wifi_health_log, buff);
       		wifi_dbg_print(1, "%s", buff);
		}

       	// Every hour, for private SSID(s) we need to calculate the good rssi time and bad rssi time 
		// and write into wifi log in following format
       	// WIFI_GOODBADRSSI_$apindex: $MAC,$GoodRssiTime,$BadRssiTime; $MAC,$GoodRssiTime,$BadRssiTime; ....
		if (i < 2) {
			get_formatted_time(tmp);
       		snprintf(buff, 2048, "%s WIFI_GOODBADRSSI_%d:", tmp, i + 1);
        
       		sta = hash_map_get_first(sta_map);
       		while (sta != NULL) {

			sta->total_connected_time += sta->connected_time;
			sta->connected_time = 0;

			sta->total_disconnected_time += sta->disconnected_time;
			sta->disconnected_time = 0;

			if (sta->dev_stats.cli_Active == true) {
				snprintf(tmp, 128, "%s,%d,%d;", to_sta_key(sta->sta_mac, sta_key), sta->good_rssi_time, sta->bad_rssi_time);
				strncat(buff, tmp, 128);
			}

			sta->good_rssi_time = 0;
			sta->bad_rssi_time = 0;

			sta = hash_map_get_next(sta_map, sta);
            
       		}
       		strncat(buff, "\n", 2);
       		write_to_file(wifi_health_log, buff);
       		wifi_dbg_print(1, "%s", buff);		
		}

		// check if failure indication is enabled in TR swicth
    	CosaDmlWiFi_GetRapidReconnectIndicationEnable(&sendIndication, false);

		if (sendIndication == true) {                
			bool bReconnectCountEnable = 0;

			// check whether Reconnect Count is enabled or not fro individual vAP
			CosaDmlWiFi_GetRapidReconnectCountEnable(i , &bReconnectCountEnable, false);
			if (bReconnectCountEnable == true)
			{
				get_formatted_time(tmp);
				memset(telemetryBuff, 0, TELEMETRY_MAX_BUFFER);
				snprintf(buff, 2048, "%s WIFI_RECONNECT_%d:", tmp, i + 1);
				sta = hash_map_get_first(sta_map);
				while (sta != NULL) {
				
					snprintf(tmp, 128, "%s,%d;", to_sta_key(sta->sta_mac, sta_key), sta->rapid_reconnects);
					strncat(buff, tmp, 128);
					strncat(telemetryBuff, tmp, 128);
				
					sta->rapid_reconnects = 0;
				
					sta = hash_map_get_next(sta_map, sta);
				
				}
				strncat(buff, "\n", 2);
				write_to_file(wifi_health_log, buff);
                                #if defined(ENABLE_FEATURE_TELEMETRY2_0)
                                if ( 1 == compare ) {
                                    t2_event_s("WIFI_REC_1_split", telemetryBuff);
                                } else if ( 2 == compare ) {
                                    t2_event_s("WIFI_REC_2_split", telemetryBuff);
                                }
                                #endif
				wifi_dbg_print(1, "%s", buff);
			}
		}

#if !defined(_COSA_BCM_MIPS_) && !defined(_CBR_PRODUCT_REQ_) && !defined(_COSA_BCM_ARM_) && !defined(INTEL_PUMA7) && !defined(_PLATFORM_TURRIS_)
                memset(vap_status,0,16);
                wifi_getApStatus(i, vap_status);
                wifi_getVAPTelemetry(i, &telemetry);
                if(strncmp(vap_status,"Up",2)==0)
                {
                        get_formatted_time(tmp);
                        snprintf(buff, 2048, "%s WiFi_TX_Overflow_SSID_%d:%d\n", tmp, i + 1, telemetry.txOverflow[i]);
                        write_to_file(wifi_health_log, buff);
                        wifi_dbg_print(1, "%s", buff);
                }
#endif
    }
    

	// update thresholds if changed
	if (CosaDmlWiFi_GetGoodRssiThresholdValue(&rssi) == ANSC_STATUS_SUCCESS) {
		g_monitor_module.sta_health_rssi_threshold = rssi;
	}

        for (i = 0; i < MAX_VAP; i++) {
		// update rapid reconnect time limit if changed
            if (CosaDmlWiFi_GetRapidReconnectThresholdValue(i, &rapid_reconn_max) == ANSC_STATUS_SUCCESS) {
                g_monitor_module.bssid_data[i].ap_params.rapid_reconnect_threshold = rapid_reconn_max;
            }  
	}    
    logVAPUpStatus();
}

static char*
macbytes_to_string(mac_address_t mac, unsigned char* string)
{
    sprintf(string, "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0] & 0xff,
            mac[1] & 0xff,
            mac[2] & 0xff,
            mac[3] & 0xff,
            mac[4] & 0xff,
            mac[5] & 0xff);
    return string;
}
#if 0
/* cleanup_client_stats_info */
void cleanup_client_stats_info(void)
{
    hash_map_t *sta_map;
    sta_data_t *sta = NULL;
    sta_key_t sta_key;
    int ap_index;

    for (ap_index = 0; ap_index < MAX_VAP; ap_index++)
    {
        sta_map = g_monitor_module.sta_map[ap_index];

        // now check all sta(s) and cleanup disconnected clients.
        sta = hash_map_get_first(sta_map);
        while (sta != NULL)
        {
            /*
             * If the device is disconnected,
             * the cli_disconnect_poll_count is set to 1
             * in process_disconnect().
             */
            if (sta->cli_disconnect_poll_count > 0)
            {
                sta->cli_disconnect_poll_count += 1;
                /* keep the stas for 3 poll count after client is disconnected */
                if (sta->cli_disconnect_poll_count > 4)
                {
                    sta = (sta_data_t *)hash_map_remove(sta_map,
                                to_sta_key(sta->sta_mac, sta_key));
                    if (sta == NULL)
                    {
                        wifi_dbg_print(1, "%s:%d: Device: NOT FOUND in hash map\n",
                                        __func__, __LINE__);
                        assert(0);
                    }
                }
            }
            sta = hash_map_get_next(sta_map, sta);
        }
    }
}
#endif
static void
reset_client_stats_info(unsigned int apIndex)
{
	sta_data_t      *sta = NULL;
	hash_map_t      *sta_map;

	sta_map = g_monitor_module.bssid_data[apIndex].sta_map;

	sta = hash_map_get_first(sta_map);
	while (sta != NULL) {
		memset((unsigned char *)&sta->dev_stats_last, 0, sizeof(wifi_associated_dev3_t));
		memset((unsigned char *)&sta->dev_stats, 0,  sizeof(wifi_associated_dev3_t));
		sta = hash_map_get_next(sta_map, sta);
	}

}

void
get_device_flag(char flag[], char *psmcli)
{
    int retPsmGet = CCSP_SUCCESS;
    char *strValue = NULL;
    char buf[CLIENT_STATS_MAX_LEN_BUF] = {0};
    char *value = NULL;
    char idx = 0;

    retPsmGet = PSM_Get_Record_Value2(bus_handle, g_Subsystem,
                                        psmcli, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS)
    {
        if (strlen(strValue))
        {
            strncpy(buf, strValue, CLIENT_STATS_MAX_LEN_BUF);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);

            value = strtok(buf, ",");
            idx = atoi(value);
            if ((idx <= MAX_VAP) && (idx > 0))
                flag[idx-1] = 1;

            while ((value = strtok(NULL, ",")) != NULL)
            {
                idx = atoi(value);
                if ((idx <= MAX_VAP) && (idx > 0))
                    flag[idx-1] = 1;
            }
        }
        else
        {
            flag[0] = 1;
            flag[1] = 1;
        }
    }
    else {
        flag[0] = 1;
        flag[1] = 1;
    }
}

/*
 * This API will Create telemetry and data model for client activity stats
 * like BytesSent, BytesReceived, RetransCount, FailedRetransCount, etc...
 */
static void
upload_client_debug_stats(void)
{
    INT apIndex = 0;
    sta_key_t sta_key;
    ULONG channel = 0;
    hash_map_t     *sta_map;
    sta_data_t *sta;
    char tmp[128] = {0};
    char cmd[CLIENT_STATS_MAX_LEN_BUF] = {0};
    char buf[CLIENT_STATS_MAX_LEN_BUF] = {0};
    INT len = 0;
    char *value = NULL;
    char *ptr = NULL;
    FILE *fp  = NULL;
    ULONG txpower = 0;
    BOOL enable = false;

    if  (false == sWiFiDmlvApStatsFeatureEnableCfg)
    {
        wifi_dbg_print(1, "Client activity stats feature is disabled\n");
        return;
    }

    for (apIndex = 0; apIndex < MAX_VAP; apIndex++)
    {
        if (false == IsCosaDmlWiFiApStatsEnable(apIndex))
        {
            wifi_dbg_print(1, "Stats feature is disabled for apIndex = %d\n",
                            apIndex+1);
            continue;
        }

        wifi_getRadioChannel(apIndex%2, &channel);
        memset(tmp, 0, sizeof(tmp));
        get_formatted_time(tmp);
        write_to_file(wifi_health_log, "\n%s WIFI_CHANNEL_%d:%lu\n", tmp, apIndex+1, channel);
        
	#if defined(ENABLE_FEATURE_TELEMETRY2_0)
        if ( 1 == apIndex+1 ) {
            // Eventing for telemetry profile = "header": "WIFI_CH_1_split", "content": "WIFI_CHANNEL_1:", "type": "wifihealth.txt",
            t2_event_d("WIFI_CH_1_split", 1);
        } else if ( 2 == apIndex+1 ) {
            // Eventing for telemetry profile = "header": "WIFI_CH_2_split", "content": "WIFI_CHANNEL_2:", "type": "wifihealth.txt",
            t2_event_d("WIFI_CH_2_split", 1);
            if(1 == channel) {
                //         "header": "WIFI_INFO_UNI3_channel", "content": "WIFI_CHANNEL_2:1", "type": "wifihealth.txt",
                t2_event_d("WIFI_INFO_UNI3_channel", 1);
            }
        }
        #endif


        sta_map = g_monitor_module.bssid_data[apIndex].sta_map;
        sta = hash_map_get_first(sta_map);

	while (sta != NULL) {
            snprintf(cmd, CLIENT_STATS_MAX_LEN_BUF,
                   "dmesg | grep FA_INFO_%s | tail -1",
                    to_sta_key(sta->sta_mac, sta_key));
            fp = popen(cmd, "r");
            if (fp)
            {
                fgets(buf, CLIENT_STATS_MAX_LEN_BUF, fp);
                pclose(fp);

                len = strlen(buf);
                if (len)
                {
                    ptr = buf + len;
                    while (len-- && ptr-- && *ptr != ':');
                    ptr++;

                    value = strtok(ptr, ",");
		    memset(tmp, 0, sizeof(tmp));
		    get_formatted_time(tmp);
                    write_to_file(wifi_health_log,
                                    "\n%s WIFI_AID_%d:%s", tmp, apIndex+1, value);
                    value = strtok(NULL, ",");
		    memset(tmp, 0, sizeof(tmp));
		    get_formatted_time(tmp);
                    write_to_file(wifi_health_log,
                                    "\n%s WIFI_TIM_%d:%s", tmp, apIndex+1, value);
                    value = strtok(NULL, ",");
		    memset(tmp, 0, sizeof(tmp));
		    get_formatted_time(tmp);
                    write_to_file(wifi_health_log,
                                    "\n%s WIFI_BMP_SET_CNT_%d:%s", tmp,
                                    apIndex+1, value);
                    value = strtok(NULL, ",");
		    memset(tmp, 0, sizeof(tmp));
		    get_formatted_time(tmp);
                    write_to_file(wifi_health_log,
                                    "\n%s WIFI_BMP_CLR_CNT_%d:%s", tmp,
                                    apIndex+1, value);
                    value = strtok(NULL, ",");
		    memset(tmp, 0, sizeof(tmp));
		    get_formatted_time(tmp);
                    write_to_file(wifi_health_log,
                                    "\n%s WIFI_TX_PKTS_CNT_%d:%s", tmp,
                                    apIndex+1, value);
                    value = strtok(NULL, ",");
		    memset(tmp, 0, sizeof(tmp));
		    get_formatted_time(tmp);
                    write_to_file(wifi_health_log,
                                   "\n%s WIFI_TX_DISCARDS_CNT_%d:%s", tmp, 
                                    apIndex+1, value);
                    value = strtok(NULL, ",");
		    memset(tmp, 0, sizeof(tmp));
		    get_formatted_time(tmp);
                    write_to_file(wifi_health_log, "\n%s WIFI_UAPSD_%d:%s", tmp, 
                                   apIndex+1, value);
                }
            }
            else
            {
                wifi_dbg_print(1, "Failed to run popen command\n");
            }

            memset (cmd, 0, CLIENT_STATS_MAX_LEN_BUF);
            memset (buf, 0, CLIENT_STATS_MAX_LEN_BUF);
            len = 0;
            value = NULL;
            ptr = NULL;

            snprintf(cmd, CLIENT_STATS_MAX_LEN_BUF,
                    "dmesg | grep FA_LMAC_DATA_STATS_%s | tail -1",
                    to_sta_key(sta->sta_mac, sta_key));
            fp = popen(cmd, "r");
            if (fp)
            {
                fgets(buf, CLIENT_STATS_MAX_LEN_BUF, fp);
                pclose(fp);

                len = strlen(buf);
                if (len)
                {
                    ptr = buf + len;
                    while (len-- && ptr-- && *ptr != ':');
                    ptr++;

                    value = strtok(ptr, ",");
		    memset(tmp, 0, sizeof(tmp));
		    get_formatted_time(tmp);
                    write_to_file(wifi_health_log,
                                    "\n%s WIFI_DATA_QUEUED_CNT_%d:%s", tmp, 
                                    apIndex+1, value);
                    value = strtok(NULL, ",");
		    memset(tmp, 0, sizeof(tmp));
		    get_formatted_time(tmp);
                    write_to_file(wifi_health_log,
                                    "\n%s WIFI_DATA_DROPPED_CNT_%d:%s", tmp,
                                    apIndex+1, value);
                    value = strtok(NULL, ",");
		    memset(tmp, 0, sizeof(tmp));
		    get_formatted_time(tmp);
                    write_to_file(wifi_health_log,
                                    "\n%s WIFI_DATA_DEQUED_TX_CNT_%d:%s", tmp,
                                    apIndex+1, value);
                    value = strtok(NULL, ",");
		    memset(tmp, 0, sizeof(tmp));
		    get_formatted_time(tmp);
                    write_to_file(wifi_health_log,
                                    "\n%s WIFI_DATA_DEQUED_DROPPED_CNT_%d:%s", tmp,
                                    apIndex+1, value);
                    value = strtok(NULL, ",");
		    memset(tmp, 0, sizeof(tmp));
		    get_formatted_time(tmp);
                    write_to_file(wifi_health_log,
                                    "\n%s WIFI_DATA_EXP_DROPPED_CNT_%d:%s", tmp,
                                    apIndex+1, value);
                }
            }
            else
            {
                wifi_dbg_print(1, "Failed to run popen command\n" );
            }

            memset (cmd, 0, CLIENT_STATS_MAX_LEN_BUF);
            memset (buf, 0, CLIENT_STATS_MAX_LEN_BUF);
            len = 0;
            value = NULL;
            ptr = NULL;

            snprintf(cmd, CLIENT_STATS_MAX_LEN_BUF,
                    "dmesg | grep FA_LMAC_MGMT_STATS_%s | tail -1",
                    to_sta_key(sta->sta_mac, sta_key));
            fp = popen(cmd, "r");
            if (fp)
            {
                fgets(buf, CLIENT_STATS_MAX_LEN_BUF, fp);
                pclose(fp);

                len = strlen(buf);
                if (len)
                {
                    ptr = buf + len;
                    while (len-- && ptr-- && *ptr != ':');
                    ptr++;

                    value = strtok(ptr, ",");
		    memset(tmp, 0, sizeof(tmp));
		    get_formatted_time(tmp);
                    write_to_file(wifi_health_log,
                                    "\n%s WIFI_MGMT_QUEUED_CNT_%d:%s", tmp,
                                    apIndex+1, value);
                    value = strtok(NULL, ",");
		    memset(tmp, 0, sizeof(tmp));
		    get_formatted_time(tmp);
                    write_to_file(wifi_health_log,
                                    "\n%s WIFI_MGMT_DROPPED_CNT_%d:%s", tmp,
                                    apIndex+1, value);
                    value = strtok(NULL, ",");
		    memset(tmp, 0, sizeof(tmp));
		    get_formatted_time(tmp);
                    write_to_file(wifi_health_log,
                                    "\n%s WIFI_MGMT_DEQUED_TX_CNT_%d:%s", tmp, 
                                    apIndex+1, value);
                    value = strtok(NULL, ",");
		    memset(tmp, 0, sizeof(tmp));
		    get_formatted_time(tmp);
                    write_to_file(wifi_health_log,
                                    "\n%s WIFI_MGMT_DEQUED_DROPPED_CNT_%d:%s", tmp,
                                    apIndex+1, value);
                    value = strtok(NULL, ",");
		    memset(tmp, 0, sizeof(tmp));
		    get_formatted_time(tmp);
                    write_to_file(wifi_health_log,
                                    "\n%s WIFI_MGMT_EXP_DROPPED_CNT_%d:%s", tmp,
                                    apIndex+1, value);
                }
            }
            else
            {
                wifi_dbg_print(1, "Failed to run popen command\n" );
            }

            if (0 == apIndex) // no check in script. Added in C code.
            {
                memset (cmd, 0, CLIENT_STATS_MAX_LEN_BUF);
                memset (buf, 0, CLIENT_STATS_MAX_LEN_BUF);
                len = 0;
                value = NULL;
                ptr = NULL;

                snprintf(cmd, CLIENT_STATS_MAX_LEN_BUF,
                            "dmesg | grep VAP_ACTIVITY_ath0 | tail -1");
                fp = popen(cmd, "r");
                if (fp)
                {
                    fgets(buf, CLIENT_STATS_MAX_LEN_BUF, fp);
                    pclose(fp);

                    len = strlen(buf);
                    if (len)
                    {
                        ptr = buf + len;
                        while (len-- && ptr-- && *ptr != ':');
			ptr += 3;

                        value = strtok(ptr, ",");
			memset(tmp, 0, sizeof(tmp));
			get_formatted_time(tmp);
			write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_1:%s\n", tmp, value);
                        value = strtok(NULL, ",");
			memset(tmp, 0, sizeof(tmp));
			get_formatted_time(tmp);
			write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_DATA_QUEUE_LEN_1:%s\n", tmp,
                                value);
                        value = strtok(NULL, ",");
			memset(tmp, 0, sizeof(tmp));
			get_formatted_time(tmp);
			write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_DATA_QUEUE_BYTES_1:%s\n", tmp,
                                value);
                        value = strtok(NULL, ",");
			memset(tmp, 0, sizeof(tmp));
			get_formatted_time(tmp);
			write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_DATA_FRAME_LEN_1:%s\n", tmp,
                                value);
                        value = strtok(NULL, ",");
			memset(tmp, 0, sizeof(tmp));
			get_formatted_time(tmp);
			write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_DATA_FRAME_COUNT_1:%s\n", tmp,
                                value);
                    }
                }
                else
                {
                    wifi_dbg_print(1, "Failed to run popen command\n" );
                }
            }

            if (1 == apIndex) // no check in script. Added in C code.
            {
                memset (cmd, 0, CLIENT_STATS_MAX_LEN_BUF);
                memset (buf, 0, CLIENT_STATS_MAX_LEN_BUF);
                len = 0;
                value = NULL;
                ptr = NULL;

                snprintf(cmd, CLIENT_STATS_MAX_LEN_BUF,
                            "dmesg | grep VAP_ACTIVITY_ath1 | tail -1");
                fp = popen(cmd, "r");
                if (fp)
                {
                    fgets(buf, CLIENT_STATS_MAX_LEN_BUF, fp);
                    pclose(fp);

                    len = strlen(buf);
                    if (len)
                    {
                        ptr = buf + len;
                        while (len-- && ptr-- && *ptr != ':');
			ptr += 3;

                        value = strtok(ptr, ",");
			memset(tmp, 0, sizeof(tmp));
			get_formatted_time(tmp);
			write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_2:%s\n", tmp, value);
                        value = strtok(NULL, ",");
			memset(tmp, 0, sizeof(tmp));
			get_formatted_time(tmp);
			write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_DATA_QUEUE_LEN_2:%s\n", tmp,
                                value);
                        value = strtok(NULL, ",");
			memset(tmp, 0, sizeof(tmp));
			get_formatted_time(tmp);
			write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_DATA_QUEUE_BYTES_2:%s\n", tmp,
                                value);
                        value = strtok(NULL, ",");
			memset(tmp, 0, sizeof(tmp));
			get_formatted_time(tmp);
			write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_DATA_FRAME_LEN_2:%s\n", tmp,
                                value);
                        value = strtok(NULL, ",");
			memset(tmp, 0, sizeof(tmp));
			get_formatted_time(tmp);
			write_to_file(wifi_health_log, "%s WIFI_PS_CLIENTS_DATA_FRAME_COUNT_2:%s\n", tmp,
                                value);
                    }
                }
                else
                {
                    wifi_dbg_print(1, "Failed to run popen command\n" );
                }
            }
	    sta = hash_map_get_next(sta_map, sta);
        }

        if ((apIndex == 0) || (apIndex == 1))
        {
            txpower = 0;
            enable = false;
            memset (buf, 0, CLIENT_STATS_MAX_LEN_BUF);

            /* adding transmit power and countrycode */
            wifi_getRadioCountryCode(apIndex, buf);
	    memset(tmp, 0, sizeof(tmp));
	    get_formatted_time(tmp);
            write_to_file(wifi_health_log, "\n%s WIFI_COUNTRY_CODE_%d:%s", tmp, apIndex+1, buf);
            wifi_getRadioTransmitPower(apIndex, &txpower);
	    memset(tmp, 0, sizeof(tmp));
	    get_formatted_time(tmp);
            write_to_file(wifi_health_log, "\n%s WIFI_TX_PWR_dBm_%d:%lu", tmp, apIndex+1, txpower);
            //    "header": "WIFI_TXPWR_1_split",   "content": "WIFI_TX_PWR_dBm_1:", "type": "wifihealth.txt",
            //    "header": "WIFI_TXPWR_2_split",   "content": "WIFI_TX_PWR_dBm_2:", "type": "wifihealth.txt",
            #if defined(ENABLE_FEATURE_TELEMETRY2_0)
            if(1 == (apIndex+1)) {
                t2_event_d("WIFI_TXPWR_1_split", txpower);
            } else if (2 == (apIndex+1)) {
                t2_event_d("WIFI_TXPWR_2_split", txpower);
            }
            #endif
            wifi_getBandSteeringEnable(&enable);
	    memset(tmp, 0, sizeof(tmp));
	    get_formatted_time(tmp);
            write_to_file(wifi_health_log, "\n%s WIFI_ACL_%d:%d", tmp, apIndex+1, enable);

            wifi_getRadioAutoChannelEnable(apIndex, &enable);
            if (true == enable)
            {
		memset(tmp, 0, sizeof(tmp));
		get_formatted_time(tmp);
                write_to_file(wifi_health_log, "\n%s WIFI_ACS_%d:true\n", tmp, apIndex+1);
                // "header": "WIFI_ACS_1_split",  "content": "WIFI_ACS_1:", "type": "wifihealth.txt",
                // "header": "WIFI_ACS_2_split", "content": "WIFI_ACS_2:", "type": "wifihealth.txt",
                #if defined(ENABLE_FEATURE_TELEMETRY2_0)
                if ( 1 == (apIndex+1) ) {
                    t2_event_s("WIFI_ACS_1_split", "true");
                } else if ( 2 == (apIndex+1) ) {
                    t2_event_s("WIFI_ACS_2_split", "true");
                }
                #endif
            }
            else
            {
		memset(tmp, 0, sizeof(tmp));
		get_formatted_time(tmp);
                write_to_file(wifi_health_log, "\n%s WIFI_ACS_%d:false\n", tmp, apIndex+1);
                // "header": "WIFI_ACS_1_split",  "content": "WIFI_ACS_1:", "type": "wifihealth.txt",
                // "header": "WIFI_ACS_2_split", "content": "WIFI_ACS_2:", "type": "wifihealth.txt",
                #if defined(ENABLE_FEATURE_TELEMETRY2_0)
                if ( 1 == (apIndex+1) ) {
                    t2_event_s("WIFI_ACS_1_split", "false");
                } else if ( 2 == (apIndex+1) ) {
                    t2_event_s("WIFI_ACS_2_split", "false");
                }
                #endif
            }
        }
    }
}

static void
process_stats_flag_changed(unsigned int ap_index, client_stats_enable_t *flag)
{

    if (0 == flag->type) //Device.WiFi.X_RDKCENTRAL-COM_vAPStatsEnable = 0
    {
	int idx;

	write_to_file(wifi_health_log, "WIFI_STATS_FEATURE_ENABLE:%s\n",
                (flag->enable) ? "true" : "false");
	for(idx = 0; idx < MAX_VAP; idx++)
        {
            reset_client_stats_info(idx);
        }
    }
    else if (1 == flag->type) //Device.WiFi.AccessPoint.<vAP>.X_RDKCENTRAL-COM_StatsEnable = 1
    {
        if(MAX_VAP > ap_index)
        {
            reset_client_stats_info(ap_index);
            write_to_file(wifi_health_log, "WIFI_STATS_ENABLE_%d:%s\n", ap_index+1,
                    (flag->enable) ? "true" : "false");
        }
    }
}

static void
radio_stats_flag_changed(unsigned int radio_index, client_stats_enable_t *flag)
{
	int idx = 0;

	if (0 == radio_index)	//2.4GHz
	{
		for(idx = 0; idx <= MAX_VAP - 2; idx = idx + 2)
		{
			reset_client_stats_info(idx);
		}
		write_to_file(wifi_health_log, "WIFI_RADIO_STATUS_ENABLE_%d:%s\n", radio_index+1,
				(flag->enable) ? "true" : "false");
	} else {	//5GHz
		for(idx = 1; idx < MAX_VAP; idx = idx + 2)
		{
			reset_client_stats_info(idx);
		}
		write_to_file(wifi_health_log, "WIFI_RADIO_STATUS_ENABLE_%d:%s\n", radio_index+1,
				(flag->enable) ? "true" : "false");
	}
}

static void
vap_stats_flag_changed(unsigned int ap_index, client_stats_enable_t *flag)
{
      //Device.WiFi.SSID.<vAP>.Enable = 0
            reset_client_stats_info(ap_index);
            write_to_file(wifi_health_log, "WIFI_VAP_STATUS_ENABLE_%d:%s\n", ap_index+1,
                    (flag->enable) ? "true" : "false");
}

static void
get_sub_string(char *bandwidth, char *dest)
{
    if (5 == strlen(bandwidth)) // 20MHz. Copy only the first 2 bytes
        strncpy(dest, bandwidth, 2);
    else
        strncpy(dest, bandwidth, 3); //160MHz Copy only the first 3 bytes
}

static void
upload_channel_width_telemetry(void)
{
    char buffer[64] = {0};
    char bandwidth[4] = {0};
    char tmp[128] = {0};
    char buff[1024] = {0};

    wifi_getRadioOperatingChannelBandwidth(0, buffer);
    get_sub_string(buffer, bandwidth);
    get_formatted_time(tmp);
    snprintf(buff, 1024, "%s WiFi_config_2G_chan_width_split:%s\n", tmp, bandwidth);
    write_to_file(wifi_health_log, buff);
    //"header": "WIFI_CWconfig_1_split", "content": "WiFi_config_2G_chan_width_split:", "type": "wifihealth.txt",
    #if defined(ENABLE_FEATURE_TELEMETRY2_0)
    t2_event_s("WIFI_CWconfig_1_split", bandwidth);
    #endif

    memset(buffer, 0, sizeof(buffer));
    memset(bandwidth, 0, sizeof(bandwidth));
    memset(tmp, 0, sizeof(tmp));
    memset(buff, 0, sizeof(buff));

    wifi_getRadioOperatingChannelBandwidth(1, buffer);
    get_sub_string(buffer, bandwidth);
    get_formatted_time(tmp);
    snprintf(buff, 1024, "%s WiFi_config_5G_chan_width_split:%s\n", tmp, bandwidth);
    write_to_file(wifi_health_log, buff);

    // "header": "WIFI_CWconfig_2_split", "content": "WiFi_config_5G_chan_width_split:", "type": "wifihealth.txt",
    #if defined(ENABLE_FEATURE_TELEMETRY2_0)
    t2_event_s("WIFI_CWconfig_2_split", bandwidth);
    #endif
}

/*
 * wifi_stats_flag_change()
 * ap_index vAP
 * enable   true/false
 * type     Device.WiFi.X_RDKCENTRAL-COM_vAPStatsEnable= 0,
            Device.WiFi.AccessPoint.<vAP>.X_RDKCENTRAL-COM_StatsEnable = 1
*/
int wifi_stats_flag_change(int ap_index, bool enable, int type)
{
    wifi_monitor_data_t *data;
    unsigned int mac_addr[MAC_ADDR_LEN];

    data = (wifi_monitor_data_t *)malloc(sizeof(wifi_monitor_data_t));
    data->id = msg_id++;

    data->event_type = monitor_event_type_StatsFlagChange;

    data->ap_index = ap_index;

    data->u.flag.type = type;
    data->u.flag.enable = enable;


    wifi_dbg_print(1, "%s:%d: flag changed apIndex=%d enable=%d type=%d\n",
        __func__, __LINE__, ap_index, enable, type);

    pthread_mutex_lock(&g_monitor_module.lock);
    queue_push(g_monitor_module.queue, data);

    pthread_cond_signal(&g_monitor_module.cond);
    pthread_mutex_unlock(&g_monitor_module.lock);

    return 0;
}

/*
 * radio_stats_flag_change()
 * ap_index vAP
 * enable   true/false
 * type     Device.WiFi.Radio.<Index>.Enable = 1
*/
int radio_stats_flag_change(int radio_index, bool enable)
{
    wifi_monitor_data_t *data;
    unsigned int mac_addr[MAC_ADDR_LEN];

    data = (wifi_monitor_data_t *)malloc(sizeof(wifi_monitor_data_t));
    data->id = msg_id++;

    data->event_type = monitor_event_type_RadioStatsFlagChange;

    data->ap_index = radio_index;	//Radio_Index = 0, 1

    data->u.flag.enable = enable;

    wifi_dbg_print(1, "%s:%d: flag changed radioIndex=%d enable=%d\n",
        __func__, __LINE__, radio_index, enable);

    pthread_mutex_lock(&g_monitor_module.lock);
    queue_push(g_monitor_module.queue, data);

    pthread_cond_signal(&g_monitor_module.cond);
    pthread_mutex_unlock(&g_monitor_module.lock);

    return 0;
}

/*
 * vap_stats_flag_change()
 * ap_index vAP
 * enable   true/false
 * type     Device.WiFi.SSID.<vAP>.Enable = 0
*/
int vap_stats_flag_change(int ap_index, bool enable)
{
    wifi_monitor_data_t *data;
    unsigned int mac_addr[MAC_ADDR_LEN];

    data = (wifi_monitor_data_t *)malloc(sizeof(wifi_monitor_data_t));
    data->id = msg_id++;

    data->event_type = monitor_event_type_VapStatsFlagChange;

    data->ap_index = ap_index;	//vap_Index

    data->u.flag.enable = enable;

    wifi_dbg_print(1, "%s:%d: flag changed vapIndex=%d enable=%d \n",
        __func__, __LINE__, ap_index, enable);

    pthread_mutex_lock(&g_monitor_module.lock);
    queue_push(g_monitor_module.queue, data);

    pthread_cond_signal(&g_monitor_module.cond);
    pthread_mutex_unlock(&g_monitor_module.lock);

    return 0;
}

void process_diagnostics	(unsigned int ap_index, wifi_associated_dev3_t *dev, unsigned int num_devs)
{
	hash_map_t     *sta_map;
    sta_data_t *sta, *tmp_sta = NULL;
	unsigned int i;
	wifi_associated_dev3_t	*hal_sta;
	sta_key_t	sta_key;
    char bssid[MIN_MAC_LEN+1];

        sta_map = g_monitor_module.bssid_data[ap_index].sta_map;
    
        snprintf(bssid, MIN_MAC_LEN+1, "%02x%02x%02x%02x%02x%02x",
                    g_monitor_module.bssid_data[ap_index].bssid[0], g_monitor_module.bssid_data[ap_index].bssid[1],
                    g_monitor_module.bssid_data[ap_index].bssid[2], g_monitor_module.bssid_data[ap_index].bssid[3],
                    g_monitor_module.bssid_data[ap_index].bssid[4], g_monitor_module.bssid_data[ap_index].bssid[5]);

        hal_sta = dev;
        memset(sta_key, 0, STA_KEY_LEN); 
	// update all sta(s) that are in the record retrieved from hal
	for (i = 0; i < num_devs; i++) {
		sta = (sta_data_t *)hash_map_get(sta_map, to_sta_key(hal_sta->cli_MACAddress, sta_key));
		if (sta == NULL) {
			sta = (sta_data_t *)malloc(sizeof(sta_data_t));
			memset(sta, 0, sizeof(sta_data_t));	
			memcpy(sta->sta_mac, hal_sta->cli_MACAddress, sizeof(mac_addr_t));
			hash_map_put(sta_map, strdup(to_sta_key(hal_sta->cli_MACAddress, sta_key)), sta);
        }

        //wifi_dbg_print(1, "Current Stored for:%s Packets Sent:%d Packets Recieved:%d Errors Sent:%d Retrans:%d Retry:%d Multiple:%d at index:%d on vap:%d\n",
        //    to_sta_key(sta->dev_stats.cli_MACAddress, sta_key),
        //    sta->dev_stats.cli_PacketsSent, sta->dev_stats.cli_PacketsReceived, sta->dev_stats.cli_ErrorsSent,
        //    sta->dev_stats.cli_RetransCount, sta->dev_stats.cli_RetryCount, sta->dev_stats.cli_MultipleRetryCount, i, ap_index);

        memcpy((unsigned char *)&sta->dev_stats_last, (unsigned char *)&sta->dev_stats, sizeof(wifi_associated_dev3_t));
	memcpy((unsigned char *)&sta->dev_stats, (unsigned char *)hal_sta, sizeof(wifi_associated_dev3_t)); 

        //wifi_dbg_print(1, "Current Polled for:%s Packets Sent:%d Packets Recieved:%d Errors Sent:%d Retrans:%d Retry:%d Multiple:%d\n",
        //    to_sta_key(sta->dev_stats.cli_MACAddress, sta_key),
        //    hal_sta->cli_PacketsSent, hal_sta->cli_PacketsReceived, hal_sta->cli_ErrorsSent,
        //    hal_sta->cli_RetransCount, hal_sta->cli_RetryCount, hal_sta->cli_MultipleRetryCount);
        //wifi_dbg_print(1, "Current Last for: %s Packets Sent:%d Packets Recieved:%d Errors Sent:%d Retrans:%d Retry:%d Multiple:%d\n",
        //    to_sta_key(sta->dev_stats.cli_MACAddress, sta_key),
        //    sta->dev_stats_last.cli_PacketsSent, sta->dev_stats_last.cli_PacketsReceived, sta->dev_stats_last.cli_ErrorsSent,
        //    sta->dev_stats_last.cli_RetransCount, sta->dev_stats_last.cli_RetryCount, sta->dev_stats_last.cli_MultipleRetryCount);

	sta->updated = true;
        sta->dev_stats.cli_Active = true;
	sta->dev_stats.cli_SignalStrength = hal_sta->cli_SignalStrength;  //zqiu: use cli_SignalStrength as normalized rssi
	if (sta->dev_stats.cli_SignalStrength >= g_monitor_module.sta_health_rssi_threshold) {
		sta->good_rssi_time += g_monitor_module.poll_period;
	} else {
		sta->bad_rssi_time += g_monitor_module.poll_period;

	}
        
        sta->connected_time += g_monitor_module.poll_period;
          
        wifi_dbg_print(1, "Polled station info for, vap:%d bssid:%s ClientMac:%s Uplink rate:%d Downlink rate:%d Packets Sent:%d Packets Recieved:%d Errors Sent:%d Retrans:%d\n",
             ap_index+1, bssid, to_sta_key(sta->dev_stats.cli_MACAddress, sta_key), sta->dev_stats.cli_LastDataUplinkRate, sta->dev_stats.cli_LastDataDownlinkRate,
             sta->dev_stats.cli_PacketsSent, sta->dev_stats.cli_PacketsReceived, sta->dev_stats.cli_ErrorsSent, sta->dev_stats.cli_RetransCount);

        wifi_dbg_print(1, "Polled radio NF %d \n",g_monitor_module.radio_data.NoiseFloor);
        wifi_dbg_print(1, "Polled channel info for radio 2.4 : channel util:%d, channel interference:%d \n",
          g_monitor_module.radio_data.channelUtil_radio_1, g_monitor_module.radio_data.channelInterference_radio_1);
        wifi_dbg_print(1, "Polled channel info for radio 5 : channel util:%d, channel interference:%d \n",
          g_monitor_module.radio_data.channelUtil_radio_2, g_monitor_module.radio_data.channelInterference_radio_2);

        hal_sta++;

	}

	// now update all sta(s) in cache that were not updated
	sta = hash_map_get_first(sta_map);
	while (sta != NULL) {
		
		if (sta->updated == true) {
			sta->updated = false;
		} else {
			// this was not present in hal record
                        sta->disconnected_time += g_monitor_module.poll_period;
		        wifi_dbg_print(1, "Device:%s is disassociated from ap:%d, for %d amount of time, assoc status:%d\n",
                                to_sta_key(sta->sta_mac, sta_key), ap_index, sta->disconnected_time, sta->dev_stats.cli_Active);
			if ((sta->disconnected_time > 4*g_monitor_module.poll_period) && (sta->dev_stats.cli_Active == false)) {
				tmp_sta = sta;
			}
		}
        
		sta = hash_map_get_next(sta_map, sta);

		if (tmp_sta != NULL) {
                        wifi_dbg_print(1, "Device:%s being removed from map of ap:%d, and being deleted\n", to_sta_key(tmp_sta->sta_mac, sta_key), ap_index);
			hash_map_remove(sta_map, to_sta_key(tmp_sta->sta_mac, sta_key));
			free(tmp_sta);
			tmp_sta = NULL;
		}        
	}

}

void process_deauthenticate	(unsigned int ap_index, auth_deauth_dev_t *dev)
{
        char buff[2048];
        char tmp[128];
   	sta_key_t sta_key;
 
        wifi_dbg_print(1, "%s:%d Device:%s deauthenticated on ap:%d with reason : %d\n", __func__, __LINE__, to_sta_key(dev->sta_mac, sta_key), ap_index, dev->reason);
        /*Wrong password on private, Xfinity Home and LNF SSIDs*/
        if ((dev->reason == 2) && ( ap_index == 0 || ap_index == 1 || ap_index == 2 || ap_index == 3 || ap_index == 6 || ap_index == 7 )) 
        {
	       get_formatted_time(tmp);
       	 
   	       snprintf(buff, 2048, "%s WIFI_PASSWORD_FAIL:%d,%s\n", tmp, ap_index + 1, to_sta_key(dev->sta_mac, sta_key));
	       /* send telemetry of password failure */
	       write_to_file(wifi_health_log, buff);
        }
        /*Calling process_disconnect as station is disconncetd from vAP*/
        process_disconnect(ap_index, dev);
}

void process_connect	(unsigned int ap_index, auth_deauth_dev_t *dev)
{
    sta_key_t sta_key;
    sta_data_t *sta;
    hash_map_t     *sta_map;
    struct timeval tv_now;
    int i;
    char vap_status[16];

    sta_map = g_monitor_module.bssid_data[ap_index].sta_map;
    
    wifi_dbg_print(1, "sta map: %p Device:%s connected on ap:%d\n", sta_map, to_sta_key(dev->sta_mac, sta_key), ap_index);
    sta = (sta_data_t *)hash_map_get(sta_map, to_sta_key(dev->sta_mac, sta_key));
    if (sta == NULL) { /* new client */
        sta = (sta_data_t *)malloc(sizeof(sta_data_t));
        memset(sta, 0, sizeof(sta_data_t));
        memcpy(sta->sta_mac, dev->sta_mac, sizeof(mac_addr_t));
        hash_map_put(sta_map, strdup(to_sta_key(sta->sta_mac, sta_key)), sta);
    }
    
    sta->total_disconnected_time += sta->disconnected_time;
    sta->disconnected_time = 0;
    
    gettimeofday(&tv_now, NULL);
    if(!sta->assoc_monitor_start_time)
        sta->assoc_monitor_start_time = tv_now.tv_sec;

    if ((tv_now.tv_sec - sta->last_disconnected_time.tv_sec) <= g_monitor_module.bssid_data[i].ap_params.rapid_reconnect_threshold) {
        wifi_dbg_print(1, "Device:%s connected on ap:%d connected within rapid reconnect time\n", to_sta_key(dev->sta_mac, sta_key), ap_index);
        sta->rapid_reconnects++;    
    }
    
    sta->last_connected_time.tv_sec = tv_now.tv_sec;
    sta->last_connected_time.tv_usec = tv_now.tv_usec;

        /* reset stats of client */
	memset((unsigned char *)&sta->dev_stats, 0, sizeof(wifi_associated_dev3_t));
	memset((unsigned char *)&sta->dev_stats_last, 0, sizeof(wifi_associated_dev3_t));
	sta->dev_stats.cli_Active = true;
        /*To avoid duplicate entries in hash map of different vAPs eg:RDKB-21582
          Also when clients moved away from a vAP and connect back to other vAP this will be usefull*/
        for (i = 0; i < MAX_VAP; i++) {
              if ( i == ap_index)
                    continue;
              memset(vap_status,0,16);
              wifi_getApStatus(i, vap_status);
              if(strncasecmp(vap_status,"Up",2)==0) {
                   sta_map = g_monitor_module.bssid_data[i].sta_map;
                   sta = (sta_data_t *)hash_map_get(sta_map, to_sta_key(dev->sta_mac, sta_key));
                   if ((sta != NULL) && (sta->dev_stats.cli_Active == true)) {
                        sta->dev_stats.cli_Active = false;
                   }
           
              }
        }
}

void process_disconnect	(unsigned int ap_index, auth_deauth_dev_t *dev)
{
    sta_key_t sta_key;
    sta_data_t *sta;
    hash_map_t     *sta_map;
    struct timeval tv_now;
	instant_msmt_t	msmt;

    sta_map = g_monitor_module.bssid_data[ap_index].sta_map;
    wifi_dbg_print(1, "Device:%s disconnected on ap:%d\n", to_sta_key(dev->sta_mac, sta_key), ap_index);
    sta = (sta_data_t *)hash_map_get(sta_map, to_sta_key(dev->sta_mac, sta_key));
    if (sta == NULL) {
		wifi_dbg_print(1, "Device:%s could not be found on sta map of ap:%d\n", to_sta_key(dev->sta_mac, sta_key), ap_index);
		return;
    }
    
    sta->total_connected_time += sta->connected_time;
    sta->connected_time = 0;
	sta->dev_stats.cli_Active = false;
    gettimeofday(&tv_now, NULL);
    if(!sta->deauth_monitor_start_time)
        sta->deauth_monitor_start_time = tv_now.tv_sec;

    sta->last_disconnected_time.tv_sec = tv_now.tv_sec;
    sta->last_disconnected_time.tv_usec = tv_now.tv_usec;

	// stop instant measurements if its going on with this client device
	msmt.ap_index = ap_index;
	memcpy(msmt.sta_mac, dev->sta_mac, sizeof(mac_address_t));
	process_instant_msmt_stop(ap_index, &msmt);
}

void process_instant_msmt_start	(unsigned int ap_index, instant_msmt_t *msmt)
{
	memcpy(g_monitor_module.inst_msmt.sta_mac, msmt->sta_mac, sizeof(mac_address_t));
	g_monitor_module.inst_msmt.ap_index = ap_index;
	g_monitor_module.poll_period = g_monitor_module.instantPollPeriod;
	g_monitor_module.inst_msmt.active = g_monitor_module.instntMsmtenable;

        if((g_monitor_module.instantDefOverrideTTL == 0) || (g_monitor_module.instantPollPeriod == 0))
	    g_monitor_module.maxCount = 0;
        else
	    g_monitor_module.maxCount = g_monitor_module.instantDefOverrideTTL/g_monitor_module.instantPollPeriod;

	g_monitor_module.count = 0;
	wifi_dbg_print(0, "%s:%d: count:%d, maxCount:%d, TTL:%d, poll:%d\n",__func__, __LINE__, 
			g_monitor_module.count, g_monitor_module.maxCount, g_monitor_module.instantDefOverrideTTL, g_monitor_module.instantPollPeriod);

        //Enable Radio channel Stats
        wifi_setRadioStatsEnable(ap_index, 1);
}

void process_instant_msmt_stop	(unsigned int ap_index, instant_msmt_t *msmt)
{
	/*if ((g_monitor_module.inst_msmt.active == true) && (memcmp(g_monitor_module.inst_msmt.sta_mac, msmt->sta_mac, sizeof(mac_address_t)) == 0)) {		
		g_monitor_module.inst_msmt.active = false;
		g_monitor_module.poll_period = DEFAULT_INSTANT_POLL_TIME;
		g_monitor_module.maxCount = g_monitor_module.instantDefReportPeriod/DEFAULT_INSTANT_POLL_TIME;
		g_monitor_module.count = 0;
	}*/
        
        g_monitor_module.inst_msmt.active = false;
        g_monitor_module.poll_period = DEFAULT_INSTANT_POLL_TIME;
 	g_monitor_module.maxCount = 0;
	g_monitor_module.count = 0;

        //Disable Radio channel Stats
        wifi_setRadioStatsEnable(ap_index, 0);
}

void *monitor_function  (void *data)
{
	wifi_monitor_t *proc_data;
	struct timespec time_to_wait;
	struct timeval tv_now;
	wifi_monitor_data_t	*queue_data;
	int rc;
        time_t  time_diff;

	proc_data = (wifi_monitor_t *)data;

	while (proc_data->exit_monitor == false) {
		gettimeofday(&tv_now, NULL);
		
        time_to_wait.tv_nsec = 0;
        time_to_wait.tv_sec = tv_now.tv_sec + proc_data->poll_period;
        
        if (proc_data->last_signalled_time.tv_sec > proc_data->last_polled_time.tv_sec) {
            // if were signalled withing poll interval if last poll the wait should be shorter
            time_diff = proc_data->last_signalled_time.tv_sec - proc_data->last_polled_time.tv_sec;
            if (time_diff < proc_data->poll_period) {
                time_to_wait.tv_sec = tv_now.tv_sec + (proc_data->poll_period - time_diff);
            }
        }
        
        pthread_mutex_lock(&proc_data->lock);
        rc = pthread_cond_timedwait(&proc_data->cond, &proc_data->lock, &time_to_wait);
		if (rc == 0) {
			// dequeue data
			while (queue_count(proc_data->queue)) {
				queue_data = queue_pop(proc_data->queue);
				if (queue_data == NULL) {
					continue;
				}

				switch (queue_data->event_type) {
					case monitor_event_type_diagnostics:
						//process_diagnostics(queue_data->ap_index, &queue_data->u.devs);
						break;

					case monitor_event_type_connect:
						process_connect(queue_data->ap_index, &queue_data->u.dev);
						break;

					case monitor_event_type_disconnect:
						process_disconnect(queue_data->ap_index, &queue_data->u.dev);
						break;
                        
					case monitor_event_type_deauthenticate:
						process_deauthenticate(queue_data->ap_index, &queue_data->u.dev);
						break;

					case monitor_event_type_stop_inst_msmt:
						process_instant_msmt_stop(queue_data->ap_index, &queue_data->u.imsmt);
						break;

					case monitor_event_type_start_inst_msmt:
						process_instant_msmt_start(queue_data->ap_index, &queue_data->u.imsmt);
						break;

                                        case monitor_event_type_StatsFlagChange:
                                                process_stats_flag_changed(queue_data->ap_index, &queue_data->u.flag);
						break;
                                        case monitor_event_type_RadioStatsFlagChange:
                                                radio_stats_flag_changed(queue_data->ap_index, &queue_data->u.flag);
						break;
                                        case monitor_event_type_VapStatsFlagChange:
                                                vap_stats_flag_changed(queue_data->ap_index, &queue_data->u.flag);
						break;
                        
                    default:
                        break;

				}

				free(queue_data);
                gettimeofday(&proc_data->last_signalled_time, NULL);
			}	
		} else if (rc == ETIMEDOUT) {

			wifi_dbg_print(1, "%s:%d: Monitor timed out, to get stats\n", __func__, __LINE__);
			proc_data->current_poll_iter++;
			gettimeofday(&proc_data->last_polled_time, NULL);
			proc_data->upload_period = get_upload_period(proc_data->upload_period,60);

			/*Call the diagnostic API based to get single client stats*/
			if (g_monitor_module.instntMsmtenable == true) { 
				wifi_dbg_print(0, "%s:%d: count:maxCount is %d:%d\n",__func__, __LINE__, g_monitor_module.count, g_monitor_module.maxCount);
				if(g_monitor_module.count >= g_monitor_module.maxCount)
				{
					wifi_dbg_print(1, "%s:%d: instant polling freq reached threshold\n", __func__, __LINE__);
					g_monitor_module.instantDefOverrideTTL = DEFAULT_INSTANT_REPORT_TIME;
                                        g_monitor_module.instntMsmtenable = false;  
					process_instant_msmt_stop(queue_data->ap_index, &queue_data->u.imsmt);
				}
                                else
                                {   
				     g_monitor_module.count += 1;
				     wifi_dbg_print(1, "%s:%d: client %s on ap %d\n", __func__, __LINE__, g_monitor_module.instantMac, g_monitor_module.inst_msmt.ap_index);
				     associated_client_diagnostics(); //for single client
				     stream_client_msmt_data();
                                }  
                        } else {
                             g_monitor_module.count = 0;
                             g_monitor_module.maxCount = 0;

			     associated_devices_diagnostics();
                             if (proc_data->current_poll_iter >= proc_data->upload_period) {
                                     upload_client_telemetry_data();
                                     upload_client_debug_stats();
                                     /* telemetry for WiFi Channel Width for 2.4G and 5G radios */
                                     upload_channel_width_telemetry();
                                     upload_ap_telemetry_data();
                                     proc_data->current_poll_iter = 0;
                             }
                        }

		} else {
			pthread_mutex_unlock(&proc_data->lock);
			return NULL;
		}

        pthread_mutex_unlock(&proc_data->lock);
    }
        
    
    return NULL;
}

/* Log VAP status on percentage basis */
static void logVAPUpStatus()
{
    int i=0;
    int vapup_percentage=0;
    char log_buf[1024]={0};
    char telemetry_buf[1024]={0};
    char vap_buf[16]={0};
    char tmp[128]={0};
    wifi_dbg_print(1, "Entering %s:%d \n",__FUNCTION__,__LINE__);
	get_formatted_time(tmp);
    sprintf(log_buf,"%s WIFI_VAP_PERCENT_UP:",tmp);
    for(i=0;i<MAX_VAP;i++)
    {
        vapup_percentage=((int)(vap_up_arr[i])*100)/(vap_iteration);
        char delimiter = (i+1) < (MAX_VAP+1) ?';':' ';
        sprintf(vap_buf,"%d,%d%c",(i+1),vapup_percentage, delimiter);
        strcat(log_buf,vap_buf);
        strcat(telemetry_buf,vap_buf);
    }
    strcat(log_buf,"\n");
    write_to_file(wifi_health_log,log_buf);
    #if defined(ENABLE_FEATURE_TELEMETRY2_0)
    t2_event_s("WIFI_VAPPERC_split", telemetry_buf);
    #endif
    vap_iteration=0;
    memset(vap_up_arr,0,MAX_VAP);
    wifi_dbg_print(1, "Exiting %s:%d \n",__FUNCTION__,__LINE__);
}
/* Capture the VAP status periodically */
static void captureVAPUpStatus()
{
    int i=0;
    char vap_status[16];
    wifi_dbg_print(1, "Entering %s:%d \n",__FUNCTION__,__LINE__);
    vap_iteration++;
    for(i=0;i<MAX_VAP;i++)
    {
        memset(vap_status,0,16);
        wifi_getApStatus(i,vap_status);
        if(strncasecmp(vap_status,"Up",2)==0)
        {
            vap_up_arr[i]=vap_up_arr[i]+1;
        }
    }
    wifi_dbg_print(1, "Exiting %s:%d \n",__FUNCTION__,__LINE__);

}
static int readLogInterval()
{

    int logInterval=60;//Default Value 60mins.
    int retPsmGet = CCSP_SUCCESS;
    char *strValue=NULL;
    FILE *fp=NULL;

    wifi_dbg_print(1, "Entering %s:%d \n",__FUNCTION__,__LINE__);
    retPsmGet = PSM_Get_Record_Value2(bus_handle, g_Subsystem,"dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WHIX.LogInterval",NULL,&strValue);
    
    if (retPsmGet == CCSP_SUCCESS)
    {
        if (strValue && strlen(strValue))
        {
            logInterval=atoi(strValue);
            wifi_dbg_print(1, "The LogInterval is %dsec or %dmin \n",logInterval,(int)logInterval/60);
            logInterval=(int)(logInterval/60);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
        }
    }
    else
    {
            wifi_dbg_print(1, "%s:%d The PSM_Get_Record_Value2  is failed with %d retval  \n",__FUNCTION__,__LINE__,retPsmGet);
    }
    wifi_dbg_print(1, "Exiting %s:%d \n",__FUNCTION__,__LINE__);
    return logInterval;
}

void associated_client_diagnostics ()
{
    wifi_associated_dev3_t dev_conn ;
    wifi_channelStats_t channelStats;
    wifi_radioTrafficStats2_t radioStats;
    int radioIndex;
    int chan_util = 0;
  
    char s_mac[MIN_MAC_LEN+1];
    int index = g_monitor_module.inst_msmt.ap_index;
   
    memset(&radioStats, 0, sizeof(wifi_radioTrafficStats2_t));
    memset(&dev_conn, 0, sizeof(wifi_associated_dev3_t));
    snprintf(s_mac, MIN_MAC_LEN+1, "%02x%02x%02x%02x%02x%02x", g_monitor_module.inst_msmt.sta_mac[0],
       g_monitor_module.inst_msmt.sta_mac[1],g_monitor_module.inst_msmt.sta_mac[2], g_monitor_module.inst_msmt.sta_mac[3],
                g_monitor_module.inst_msmt.sta_mac[4], g_monitor_module.inst_msmt.sta_mac[5]);

     memset(&g_monitor_module.radio_data, 0, sizeof(radio_data_t));
     radioIndex = ((index % 2) == 0)? 0:1;

    wifi_getRadioTrafficStats2(radioIndex, &radioStats); 
    wifi_dbg_print(1, "%s:%d: get radio NF %d\n", __func__, __LINE__, radioStats.radio_NoiseFloor);
    g_monitor_module.radio_data.NoiseFloor = radioStats.radio_NoiseFloor; 

    /* ToDo: We can get channel_util percentage now, channel_ineterference percentage is 0 */
    if (wifi_getRadioBandUtilization(index, &chan_util) == RETURN_OK) {
            wifi_dbg_print(1, "%s:%d: get channel stats for radio %d\n", __func__, __LINE__, radioIndex);
            if (radioIndex == 0){
                   g_monitor_module.radio_data.channelUtil_radio_1 = chan_util;
                   g_monitor_module.radio_data.channelInterference_radio_1 = 0;
                   g_monitor_module.radio_data.channelUtil_radio_2 = 0;
                   g_monitor_module.radio_data.channelInterference_radio_2 = 0;
            } else {
                   g_monitor_module.radio_data.channelUtil_radio_2 = chan_util;
		   g_monitor_module.radio_data.channelInterference_radio_2 = 0;
                   g_monitor_module.radio_data.channelUtil_radio_1 = 0;
                   g_monitor_module.radio_data.channelInterference_radio_1 = 0;
            }
    }
 
    wifi_dbg_print(1, "%s:%d: get single connected client %s stats\n", __func__, __LINE__, s_mac);
#if !defined(_XB7_PRODUCT_REQ_) && !defined(_XF3_PRODUCT_REQ_) && !defined(_CBR_PRODUCT_REQ_) && !defined(_HUB4_PRODUCT_REQ_)
    wifi_dbg_print(1, "WIFI_HAL enabled, calling wifi_getApAssociatedClientDiagnosticResult\n");
    if (wifi_getApAssociatedClientDiagnosticResult(index, s_mac, &dev_conn) == RETURN_OK) {
           process_diagnostics(index, &dev_conn, 1);
    }
#else
    wifi_dbg_print(1, "WIFI_HAL Not enabled. Using wifi default values\n");
    process_diagnostics(index, &dev_conn, 1);
#endif
}

void associated_devices_diagnostics	()
{
	wifi_associated_dev3_t *dev_array = NULL;
	unsigned int i, num_devs = 0;

	//wifi_dbg_print(1, "%s:%d:Enter\n", __func__, __LINE__);      
	for (i = 0; i < MAX_VAP; i++) { 
		if (wifi_getApAssociatedDeviceDiagnosticResult3(i, &dev_array, &num_devs) == RETURN_OK) {
        	process_diagnostics(i, dev_array, num_devs);
        
        	if ((num_devs > 0) && (dev_array != NULL)) {
            	free(dev_array);
        	}
		}

		num_devs = 0;		
		dev_array = NULL;
	}
    captureVAPUpStatus();
	//wifi_dbg_print(1, "%s:%d:Exit\n", __func__, __LINE__);      
    /* upload the client activity stats */
//    cleanup_client_stats_info();
}

int device_disassociated(int ap_index, char *mac, int reason)
{
    wifi_monitor_data_t *data;
	unsigned int mac_addr[MAC_ADDR_LEN];

    data = (wifi_monitor_data_t *)malloc(sizeof(wifi_monitor_data_t));
    data->id = msg_id++;

	data->event_type = monitor_event_type_disconnect;

    data->ap_index = ap_index;
	sscanf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
           &mac_addr[0], &mac_addr[1], &mac_addr[2],
           &mac_addr[3], &mac_addr[4], &mac_addr[5]);
    data->u.dev.sta_mac[0] = mac_addr[0]; data->u.dev.sta_mac[1] = mac_addr[1]; data->u.dev.sta_mac[2] = mac_addr[2];
    data->u.dev.sta_mac[3] = mac_addr[3]; data->u.dev.sta_mac[4] = mac_addr[4]; data->u.dev.sta_mac[5] = mac_addr[5];
    data->u.dev.reason = reason;
	   
	wifi_dbg_print(1, "%s:%d:Device diaassociated on interface:%d mac:%02x:%02x:%02x:%02x:%02x:%02x\n",
        __func__, __LINE__, ap_index,
        data->u.dev.sta_mac[0], data->u.dev.sta_mac[1], data->u.dev.sta_mac[2], 
		data->u.dev.sta_mac[3], data->u.dev.sta_mac[4], data->u.dev.sta_mac[5]);

    pthread_mutex_lock(&g_monitor_module.lock);
    queue_push(g_monitor_module.queue, data);

    pthread_cond_signal(&g_monitor_module.cond);
    pthread_mutex_unlock(&g_monitor_module.lock);

	return 0;
}

int device_deauthenticated(int ap_index, char *mac, int reason)
{
    wifi_monitor_data_t *data;
    unsigned int mac_addr[MAC_ADDR_LEN];

    data = (wifi_monitor_data_t *)malloc(sizeof(wifi_monitor_data_t));
    if (data == NULL) {
          return -1;      
    }
   
    data->id = msg_id++;

	data->event_type = monitor_event_type_deauthenticate;
    
    data->ap_index = ap_index;
	sscanf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
           &mac_addr[0], &mac_addr[1], &mac_addr[2],
           &mac_addr[3], &mac_addr[4], &mac_addr[5]);
    data->u.dev.sta_mac[0] = mac_addr[0]; data->u.dev.sta_mac[1] = mac_addr[1]; data->u.dev.sta_mac[2] = mac_addr[2];
    data->u.dev.sta_mac[3] = mac_addr[3]; data->u.dev.sta_mac[4] = mac_addr[4]; data->u.dev.sta_mac[5] = mac_addr[5];
	data->u.dev.reason = reason;
  
    wifi_dbg_print(1, "%s:%d   Device deauthenticated on interface:%d mac:%02x:%02x:%02x:%02x:%02x:%02x with reason %d\n",
        __func__, __LINE__, ap_index,
        data->u.dev.sta_mac[0], data->u.dev.sta_mac[1], data->u.dev.sta_mac[2],
                data->u.dev.sta_mac[3], data->u.dev.sta_mac[4], data->u.dev.sta_mac[5], reason);

    pthread_mutex_lock(&g_monitor_module.lock);
    queue_push(g_monitor_module.queue, data);

    pthread_cond_signal(&g_monitor_module.cond);
    pthread_mutex_unlock(&g_monitor_module.lock);

	return 0;
}

int device_associated(int ap_index, wifi_associated_dev_t *associated_dev)
{
    wifi_monitor_data_t *data;

    data = (wifi_monitor_data_t *)malloc(sizeof(wifi_monitor_data_t));
    data->id = msg_id++;

    data->event_type = monitor_event_type_connect;

    data->ap_index = ap_index;
    //data->u.dev.reason = reason;

	data->u.dev.sta_mac[0] = associated_dev->cli_MACAddress[0]; data->u.dev.sta_mac[1] = associated_dev->cli_MACAddress[1];
	data->u.dev.sta_mac[2] = associated_dev->cli_MACAddress[2]; data->u.dev.sta_mac[3] = associated_dev->cli_MACAddress[3];
	data->u.dev.sta_mac[4] = associated_dev->cli_MACAddress[4]; data->u.dev.sta_mac[5] = associated_dev->cli_MACAddress[5];

	wifi_dbg_print(1, "%s:%d:Device associated on interface:%d mac:%02x:%02x:%02x:%02x:%02x:%02x\n",
        __func__, __LINE__, ap_index,
        data->u.dev.sta_mac[0], data->u.dev.sta_mac[1], data->u.dev.sta_mac[2], 
		data->u.dev.sta_mac[3], data->u.dev.sta_mac[4], data->u.dev.sta_mac[5]);

    pthread_mutex_lock(&g_monitor_module.lock);
    queue_push(g_monitor_module.queue, data);

    pthread_cond_signal(&g_monitor_module.cond);
    pthread_mutex_unlock(&g_monitor_module.lock);

	return 0;
}

bool is_device_associated(int ap_index, char *mac)
{
	mac_address_t bmac;
    sta_data_t *sta;
    hash_map_t     *sta_map;

	to_mac_bytes(mac, bmac);
	sta_map = g_monitor_module.bssid_data[ap_index].sta_map;
        
	sta = hash_map_get_first(sta_map);
   	while (sta != NULL) {
		if ((memcmp(sta->sta_mac, bmac, sizeof(mac_address_t)) == 0) && (sta->dev_stats.cli_Active == true)) {
			return true;
        }
       		
		sta = hash_map_get_next(sta_map, sta);
	}
	return false;
}

int init_wifi_monitor ()
{
	unsigned int i;
	int	rssi, rapid_reconn_max;
    long uptimeval=0; 
	char mac_str[32];
	
	for (i = 0; i < MAX_VAP; i++) {
	       if (wifi_getSSIDMACAddress(i, mac_str) == RETURN_OK) {
			to_mac_bytes(mac_str, g_monitor_module.bssid_data[i].bssid);
	       }
  /* This is not part of Single reporting Schema, handle this when default reporting will be enabled */
             //  wifi_getRadioOperatingFrequencyBand(i%2, g_monitor_module.bssid_data[i].frequency_band);
             //  wifi_getSSIDName(i, g_monitor_module.bssid_data[i].ssid);
             //  wifi_getRadioChannel(i, (ULONG *)&g_monitor_module.bssid_data[i].primary_radio_channel);
             //  wifi_getRadioOperatingChannelBandwidth(i%2, g_monitor_module.bssid_data[i].channel_bandwidth);

	}

	
	memset(g_monitor_module.cliStatsList, 0, MAX_VAP);
	g_monitor_module.poll_period = 5;
    g_monitor_module.upload_period = get_upload_period(0,60);//Default value 60
    uptimeval=get_sys_uptime();
    wifi_dbg_print(1, "%s:%d system uptime val is %ld and upload period is %d in secs\n",
        __FUNCTION__,__LINE__,uptimeval,(g_monitor_module.upload_period*60));
    /* If uptime is less than the upload period then we should calculate the current
        VAP iteration for measuring correct VAP UP percentatage. Becaues we should show
        the uptime value as VAP down percentatage.
    */
    if(uptimeval<(g_monitor_module.upload_period*60))
    {
        vap_iteration=(int)uptimeval/60;
        g_monitor_module.current_poll_iter = vap_iteration;
        wifi_dbg_print(1, "%s:%d Current VAP UP check iteration  %d \n",__FUNCTION__,__LINE__,vap_iteration);
    }
    else
    {
        vap_iteration=0;
        g_monitor_module.current_poll_iter = 0;
        wifi_dbg_print(1, "%s:%d Upload period is already crossed \n",__FUNCTION__,__LINE__);
    }

	if (CosaDmlWiFi_GetGoodRssiThresholdValue(&rssi) != ANSC_STATUS_SUCCESS) {
		g_monitor_module.sta_health_rssi_threshold = -65;
	} else {
		g_monitor_module.sta_health_rssi_threshold = rssi;
	}
    
    for (i = 0; i < MAX_VAP; i++) {
        // update rapid reconnect time limit if changed
        if (CosaDmlWiFi_GetRapidReconnectThresholdValue(i, &rapid_reconn_max) != ANSC_STATUS_SUCCESS) {
            g_monitor_module.bssid_data[i].ap_params.rapid_reconnect_threshold = 180;
        } else {
            g_monitor_module.bssid_data[i].ap_params.rapid_reconnect_threshold = rapid_reconn_max;
        }
	}

    gettimeofday(&g_monitor_module.last_signalled_time, NULL);
    gettimeofday(&g_monitor_module.last_polled_time, NULL);
	pthread_cond_init(&g_monitor_module.cond, NULL);
    pthread_mutex_init(&g_monitor_module.lock, NULL);

	for (i = 0; i < MAX_VAP; i++) {
		g_monitor_module.bssid_data[i].sta_map = hash_map_create();
		if (g_monitor_module.bssid_data[i].sta_map == NULL) {
			deinit_wifi_monitor();
			//wifi_dbg_print(1, "sta map create error\n");
			return -1;
		}
	}

	g_monitor_module.queue = queue_create();
	if (g_monitor_module.queue == NULL) {
		deinit_wifi_monitor();
		wifi_dbg_print(1, "monitor queue create error\n");
		return -1;
	}

	g_monitor_module.exit_monitor = false;
        pthread_attr_t attr;
        pthread_attr_t *attrp = NULL;

        attrp = &attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );
	if (pthread_create(&g_monitor_module.id, attrp, monitor_function, &g_monitor_module) != 0) {
                if(attrp != NULL)
                    pthread_attr_destroy( attrp );
		deinit_wifi_monitor();
		wifi_dbg_print(1, "monitor thread create error\n");
		return -1;
	}
        if(attrp != NULL)
            pthread_attr_destroy( attrp );
    g_monitor_module.sysevent_fd = sysevent_open("127.0.0.1", SE_SERVER_WELL_KNOWN_PORT, SE_VERSION, "wifiMonitor", &g_monitor_module.sysevent_token);
    if (g_monitor_module.sysevent_fd < 0) {
        wifi_dbg_print(1, "%s:%d: Failed to opne sysevent\n", __func__, __LINE__);
    } else {
        wifi_dbg_print(1, "%s:%d: Opened sysevent\n", __func__, __LINE__);
    }

	if (initparodusTask() == -1) {
        //wifi_dbg_print(1, "%s:%d: Failed to initialize paroduc task\n", __func__, __LINE__);

	}
   
    pthread_mutex_lock(&g_apRegister_lock);
    wifi_newApAssociatedDevice_callback_register(device_associated);
#if !defined(_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_)
    wifi_apDeAuthEvent_callback_register(device_deauthenticated);
	wifi_apDisassociatedDevice_callback_register(device_disassociated);
#endif
    pthread_mutex_unlock(&g_apRegister_lock);
    
	return 0;
}

void deinit_wifi_monitor	()
{
	unsigned int i;

	sysevent_close(g_monitor_module.sysevent_fd, g_monitor_module.sysevent_token);
        if(g_monitor_module.queue != NULL)
            queue_destroy(g_monitor_module.queue);
	for (i = 0; i < MAX_VAP; i++) {
        if(g_monitor_module.bssid_data[i].sta_map != NULL)
	        hash_map_destroy(g_monitor_module.bssid_data[i].sta_map);
	}
    pthread_mutex_destroy(&g_monitor_module.lock);
	pthread_cond_destroy(&g_monitor_module.cond);
}

unsigned int get_poll_period 	()
{
	return g_monitor_module.poll_period;
}

unsigned int GetINSTOverrideTTL()
{
    return g_monitor_module.instantDefOverrideTTL;
}

void SetINSTOverrideTTL(int defTTL)
{
    g_monitor_module.instantDefOverrideTTL = defTTL;
}

unsigned int GetINSTDefReportingPeriod()
{
    return g_monitor_module.instantDefReportPeriod;
}

void SetINSTDefReportingPeriod(int defPeriod)
{
    g_monitor_module.instantDefReportPeriod = defPeriod;
}

void SetINSTReportingPeriod(unsigned long pollPeriod)
{
    g_monitor_module.instantPollPeriod = pollPeriod;
}

unsigned int GetINSTPollingPeriod()
{
    return g_monitor_module.instantPollPeriod;
}

void SetINSTMacAddress(char *mac_addr)
{
    strncpy(g_monitor_module.instantMac, mac_addr, MIN_MAC_LEN);
}

char* GetInstAssocDevSchemaIdBuffer()
{
     return instSchemaIdBuffer;
}

int GetInstAssocDevSchemaIdBufferSize()
{
    int len = 0;
    if(instSchemaIdBuffer)
        len = strlen(instSchemaIdBuffer);

    return len;
}

void instant_msmt_reporting_period(int pollPeriod)
{
    g_monitor_module.instantPollPeriod = pollPeriod;
    int timeSpent = 0;
    int timeLeft = 0;

    wifi_dbg_print(1, "%s:%d: reporting period changed\n", __func__, __LINE__);
    pthread_mutex_lock(&g_monitor_module.lock);

    if(pollPeriod == 0){
        g_monitor_module.maxCount = 0;
        g_monitor_module.count = 0;
    }else{
        timeSpent = g_monitor_module.count * g_monitor_module.poll_period ;
	timeLeft = g_monitor_module.instantDefOverrideTTL - timeSpent;
	g_monitor_module.maxCount = timeLeft/pollPeriod;
	g_monitor_module.poll_period = pollPeriod;

	if(g_monitor_module.count > g_monitor_module.maxCount)
            g_monitor_module.count = 0;
    }
    if(g_monitor_module.instntMsmtenable == true)
    {
	pthread_cond_signal(&g_monitor_module.cond);
    }
    pthread_mutex_unlock(&g_monitor_module.lock);
}

void instant_msmt_def_period(int defPeriod)
{
    int curCount = 0;
    int newCount = 0;

    wifi_dbg_print(1, "%s:%d: def period changed\n", __func__, __LINE__);
    g_monitor_module.instantDefReportPeriod = defPeriod;

    if(g_monitor_module.instntMsmtenable == false)
    { 
         pthread_mutex_lock(&g_monitor_module.lock);

	 curCount = g_monitor_module.count;
	 newCount = g_monitor_module.instantDefReportPeriod / DEFAULT_INSTANT_POLL_TIME;

	 if(newCount > curCount){
              g_monitor_module.maxCount = newCount - curCount;
	      g_monitor_module.count = 0;
	 }else{
              wifi_dbg_print(1, "%s:%d:created max non instant report, stop polling now\n", __func__, __LINE__);
	      g_monitor_module.maxCount = 0;
	 }
	 pthread_cond_signal(&g_monitor_module.cond);
	 pthread_mutex_unlock(&g_monitor_module.lock);
    }
}

void instant_msmt_ttl(int overrideTTL)
{
    int curCount = 0;
    int newCount = 0;

    wifi_dbg_print(1, "%s:%d: TTL changed\n", __func__, __LINE__);
    g_monitor_module.instantDefOverrideTTL = overrideTTL;

    if(g_monitor_module.instantPollPeriod == 0)
        return;

    pthread_mutex_lock(&g_monitor_module.lock);

    if(overrideTTL == 0){
        g_monitor_module.maxCount = 0;
        g_monitor_module.count = 0;
    }else{   
        curCount = g_monitor_module.count;
	newCount = g_monitor_module.instantDefOverrideTTL/g_monitor_module.instantPollPeriod;
	if(newCount > curCount){
	    g_monitor_module.maxCount = newCount - curCount;
	    g_monitor_module.count = 0;
	}else{
            wifi_dbg_print(1, "%s:%d:already created maxCount report, stop polling now\n", __func__, __LINE__);
	    g_monitor_module.maxCount = 0;
	}
    }
    if(g_monitor_module.instntMsmtenable == true)
    {
	pthread_cond_signal(&g_monitor_module.cond);
    }
    pthread_mutex_unlock(&g_monitor_module.lock);
}

void instant_msmt_macAddr(char *mac_addr)
{
    mac_address_t bmac;
    int i;
    char s_mac[MIN_MAC_LEN+1]; 

    wifi_dbg_print(1, "%s:%d: get new client %s stats\n", __func__, __LINE__, mac_addr);
    strncpy(g_monitor_module.instantMac, mac_addr, MIN_MAC_LEN);
  
    to_mac_bytes(mac_addr, bmac);
    for (i = 0; i < MAX_VAP; i++) {
	  if( is_device_associated(i, mac_addr)  == true)
          {
               wifi_dbg_print(1, "%s:%d: found client %s on ap %d\n", __func__, __LINE__, mac_addr,i);
               pthread_mutex_lock(&g_monitor_module.lock);
               g_monitor_module.inst_msmt.ap_index = i; 
	       memcpy(g_monitor_module.inst_msmt.sta_mac, bmac, sizeof(mac_address_t));

	       pthread_cond_signal(&g_monitor_module.cond);
	       pthread_mutex_unlock(&g_monitor_module.lock);
          
               break;
          }
    }
}

void monitor_enable_instant_msmt(mac_address_t sta_mac, bool enable)
{
	mac_addr_str_t sta;
	unsigned int i;
	hash_map_t  *sta_map;
	sta_data_t *data;
	wifi_monitor_data_t *event;

	to_sta_key(sta_mac, sta);
	wifi_dbg_print(1, "%s:%d: instant measurements %s for sta:%s\n", __func__, __LINE__, (enable == true)?"start":"stop", sta);

        g_monitor_module.instntMsmtenable = enable;  
	pthread_mutex_lock(&g_monitor_module.lock);
	
	if (g_monitor_module.inst_msmt.active == true) {
		if (enable == false) {
			if (memcmp(g_monitor_module.inst_msmt.sta_mac, sta_mac, sizeof(mac_address_t)) == 0) {
				wifi_dbg_print(1, "%s:%d: instant measurements active for sta:%s, should stop\n", __func__, __LINE__, sta);
				g_monitor_module.instantDefOverrideTTL = DEFAULT_INSTANT_REPORT_TIME;

				event = (wifi_monitor_data_t *)malloc(sizeof(wifi_monitor_data_t));
				event->event_type = monitor_event_type_stop_inst_msmt;
				memcpy(event->u.imsmt.sta_mac, sta_mac, sizeof(mac_address_t));

				event->u.imsmt.ap_index = g_monitor_module.inst_msmt.ap_index;
				event->ap_index = g_monitor_module.inst_msmt.ap_index;

				queue_push(g_monitor_module.queue, event);

				pthread_cond_signal(&g_monitor_module.cond);
			}

		} else {
			// must return 
			wifi_dbg_print(1, "%s:%d: instant measurements active for sta:%s, should just return\n", __func__, __LINE__, sta);
		}
        	
		pthread_mutex_unlock(&g_monitor_module.lock);

		return;

	}
			
	wifi_dbg_print(1, "%s:%d: instant measurements not active should look for sta:%s\n", __func__, __LINE__, sta);

	for (i = 0; i < MAX_VAP; i++) {

		if ( is_device_associated(i, sta) == true ) {		
			wifi_dbg_print(1, "%s:%d: found sta:%s on ap index:%d starting instant measurements\n", __func__, __LINE__, sta, i);
			event = (wifi_monitor_data_t *)malloc(sizeof(wifi_monitor_data_t));

			event->event_type = monitor_event_type_start_inst_msmt;	

			memcpy(event->u.imsmt.sta_mac, sta_mac, sizeof(mac_address_t));

			event->u.imsmt.ap_index = i;
			event->ap_index = i;

			queue_push(g_monitor_module.queue, event);
			pthread_cond_signal(&g_monitor_module.cond);

			break;
		} 
	}

	pthread_mutex_unlock(&g_monitor_module.lock);
}

bool monitor_is_instant_msmt_enabled()
{
        return g_monitor_module.instntMsmtenable;
}

/* This function returns the system uptime at the time of init */
long get_sys_uptime()
{
    struct sysinfo s_info;
    int error = sysinfo(&s_info);
    if(error != 0)
    {
        wifi_dbg_print(1, "%s:%d: Error reading sysinfo %d \n", __func__, __LINE__,error);
    }
    return s_info.uptime;
}

/*
The get_upload_period takes two arguments iteration and oldInterval.
Because, it will return old interval value if check is less than 5mins.
*/
unsigned int get_upload_period  (int iteration,int oldInterval)
{
    FILE *fp;
    char buff[64];
    char *ptr;
    int logInterval=oldInterval;
    
    if ((fp = fopen("/tmp/upload", "r")) == NULL) {
    /* Minimum LOG Interval we can set is 300 sec, just verify every 5 mins any change in the LogInterval
       if any change in log_interval do the calculation and dump the VAP status */
         if(iteration%5==0)
            logInterval=readLogInterval();
        return logInterval;
    }
    
    fgets(buff, 64, fp);
    if ((ptr = strchr(buff, '\n')) != NULL) {
        *ptr = 0;
    }
    fclose(fp);
    
    return atoi(buff);
}

wifi_monitor_t *get_wifi_monitor	()
{
	return &g_monitor_module;
}

void wifi_dbg_print(int level, char *format, ...)
{
    char buff[4096] = {0};
    va_list list;
    static FILE *fpg = NULL;
    
    if ((access("/nvram/wifiMonDbg", R_OK)) != 0) {
        return;
    }

    get_formatted_time(buff);
    strcat(buff, " ");

    va_start(list, format);
    vsprintf(&buff[strlen(buff)], format, list);
    va_end(list);
    
    if (fpg == NULL) {
        fpg = fopen("/tmp/wifiMon", "a+");
        if (fpg == NULL) {
            return;
        } else {
            fputs(buff, fpg);
        }
    } else {
        fputs(buff, fpg);
    }
    
    fflush(fpg);
}
