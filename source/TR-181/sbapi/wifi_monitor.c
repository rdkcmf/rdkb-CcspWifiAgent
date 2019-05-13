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

extern void* bus_handle;
extern char g_Subsystem[32];

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

static inline char *to_sta_key    (mac_addr_t mac, sta_key_t key) {
    snprintf(key, STA_KEY_LEN, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return (char *)key;
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
    char buff[2048];
    char tmp[128];
	wifi_radioTrafficStats2_t stats;
	unsigned int i;
    
	for (i = 0; i < MAX_RADIOS; i++) {
		wifi_getRadioTrafficStats2(i, &stats);
		get_formatted_time(tmp);
		snprintf(buff, 2048, "%s WIFI_NOISE_FLOOR_%d:%d\n", tmp, i + 1, stats.radio_NoiseFloor);

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

    wifi_dbg_print(1, "%s: Checking for client:%s connection on ap:%d\n", __func__, mac, apIndex);

    sta_map = g_monitor_module.sta_map[apIndex];
    sta = (sta_data_t *)hash_map_get(sta_map, mac);
    if (sta == NULL) {
        wifi_dbg_print(1, "%s: Client:%s could not be found on sta map of ap:%d\n", __func__, mac, apIndex);
        return FALSE;
    }

    if(sta->gate_time && (tv_now.tv_sec < sta->gate_time)) {
             wifi_dbg_print(1, "%s: Blocking burst client connections for few more seconds\n", __func__);
             return TRUE;
    } else {
             wifi_dbg_print(1, "%s: processing further\n", __func__);
    }

    if(!assocMonitorDuration) {
             wifi_dbg_print(1, "%s: Client fast reconnection check disabled, assocMonitorDuration:%d \n", __func__, assocMonitorDuration);
             return FALSE;
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
             return TRUE;
        }
    } else {
             sta->assoc_monitor_start_time = tv_now.tv_sec;
             sta->reconnect_count = 0;
             sta->gate_time = 0;
             wifi_dbg_print(1, "%s: resetting reconnect_count and assoc_monitor_start_time \n", __func__);
             return FALSE;
    }
    return FALSE;
}

void upload_client_telemetry_data()
{
    hash_map_t     *sta_map;
    sta_key_t    sta_key;
    unsigned int i, num_devs;;
    sta_data_t *sta;
    char buff[2048];
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
#if !defined (_XB6_PRODUCT_REQ_) && !defined(_XF3_PRODUCT_REQ_) && !defined(_CBR_PRODUCT_REQ_)
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
#if !defined(_PLATFORM_RASPBERRYPI_)
		wifi_setClientDetailedStatisticsEnable(0, enable24detailstats);
		wifi_setClientDetailedStatisticsEnable(1, enable5detailstats);
#endif
	}
#endif
    get_device_flag(snflag, "dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WIFI_TELEMETRY.SNRList");
    for (i = 0; i < MAX_VAP; i++) {
        sta_map = g_monitor_module.sta_map[i];
			get_formatted_time(tmp);
       	snprintf(buff, 2048, "%s WIFI_MAC_%d:", tmp, i + 1);

		num_devs = 0;
       	sta = hash_map_get_first(sta_map);
       	while (sta != NULL) {
			if (sta->dev_stats.cli_Active == true) {
				snprintf(tmp, 32, "%s,", to_sta_key(sta->sta_mac, sta_key));
				strncat(buff, tmp, 128);
				num_devs++;
			}

			sta = hash_map_get_next(sta_map, sta);
       	}
       	strncat(buff, "\n", 2);
       	write_to_file(wifi_health_log, buff);
       	wifi_dbg_print(1, "%s", buff);

		get_formatted_time(tmp);
		snprintf(buff, 2048, "%s WIFI_MAC_%d_TOTAL_COUNT:%d\n", tmp, i + 1, num_devs);
		write_to_file(wifi_health_log, buff);
        wifi_dbg_print(1, "%s", buff);

		get_formatted_time(tmp);
       	snprintf(buff, 2048, "%s WIFI_RSSI_%d:", tmp, i + 1);
       	sta = hash_map_get_first(sta_map);
       	while (sta != NULL) {
			if (sta->dev_stats.cli_Active == true) {
				snprintf(tmp, 32, "%d,", sta->dev_stats.cli_RSSI);
				strncat(buff, tmp, 128);
			}

			sta = hash_map_get_next(sta_map, sta);
       	}
       	strncat(buff, "\n", 2);
       	write_to_file(wifi_health_log, buff);
       	wifi_dbg_print(1, "%s", buff);

		get_formatted_time(tmp);
       	snprintf(buff, 2048, "%s WIFI_CHANNEL_WIDTH_%d:", tmp, i + 1);
       	sta = hash_map_get_first(sta_map);
       	while (sta != NULL) {
			if (sta->dev_stats.cli_Active == true) {
				snprintf(tmp, 64, "%s,", sta->dev_stats.cli_OperatingChannelBandwidth);
				strncat(buff, tmp, 128);
			}

			sta = hash_map_get_next(sta_map, sta);
       	}
       	strncat(buff, "\n", 2);
       	write_to_file(wifi_health_log, buff);
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
       		snprintf(buff, 2048, "%s WIFI_SNR_%d:", tmp, i + 1);
       		sta = hash_map_get_first(sta_map);
       		while (sta != NULL) {
				if (sta->dev_stats.cli_Active == true) {
					snprintf(tmp, 32, "%d,", sta->dev_stats.cli_SNR);
					strncat(buff, tmp, 128);
				}
            
				sta = hash_map_get_next(sta_map, sta);
       		}
       		strncat(buff, "\n", 2);
       		write_to_file(wifi_health_log, buff);
       		wifi_dbg_print(1, "%s", buff);
		}

		get_formatted_time(tmp);
       	snprintf(buff, 2048, "%s WIFI_TXCLIENTS_%d:", tmp, i + 1);
       	sta = hash_map_get_first(sta_map);
       	while (sta != NULL) {
			if (sta->dev_stats.cli_Active == true) {
				snprintf(tmp, 32, "%d,", sta->dev_stats.cli_LastDataDownlinkRate);
				strncat(buff, tmp, 128);
			}
            
			sta = hash_map_get_next(sta_map, sta);
       	}
       	strncat(buff, "\n", 2);
       	write_to_file(wifi_health_log, buff);
       	wifi_dbg_print(1, "%s", buff);

		get_formatted_time(tmp);
       	snprintf(buff, 2048, "%s WIFI_RXCLIENTS_%d:", tmp, i + 1);
       	sta = hash_map_get_first(sta_map);
       	while (sta != NULL) {
			if (sta->dev_stats.cli_Active == true) {
				snprintf(tmp, 32, "%d,", sta->dev_stats.cli_LastDataUplinkRate);
				strncat(buff, tmp, 128);
			}
            
			sta = hash_map_get_next(sta_map, sta);
       	}
       	strncat(buff, "\n", 2);
       	write_to_file(wifi_health_log, buff);
       	wifi_dbg_print(1, "%s", buff);
	
		if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && trflag[i]) {
			get_formatted_time(tmp);
       		snprintf(buff, 2048, "%s WIFI_MAX_TXCLIENTS_%d:", tmp, i + 1);
       		sta = hash_map_get_first(sta_map);
       		while (sta != NULL) {
				if (sta->dev_stats.cli_Active == true) {
					snprintf(tmp, 32, "%u,", sta->dev_stats.cli_MaxDownlinkRate);
					strncat(buff, tmp, 128);
				}
            
				sta = hash_map_get_next(sta_map, sta);
       		}
       		strncat(buff, "\n", 2);
       		write_to_file(wifi_health_log, buff);
       		wifi_dbg_print(1, "%s", buff);
		}

		if ((sWiFiDmlvApStatsFeatureEnableCfg == true) && trflag[i]) {
			get_formatted_time(tmp);
       		snprintf(buff, 2048, "%s WIFI_MAX_RXCLIENTS_%d:", tmp, i + 1);
       		sta = hash_map_get_first(sta_map);
       		while (sta != NULL) {
				if (sta->dev_stats.cli_Active == true) {
					snprintf(tmp, 32, "%u,", sta->dev_stats.cli_MaxUplinkRate);
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
       		snprintf(buff, 2048, "%s WIFI_PACKETSSENTCLIENTS_%d:", tmp, i + 1);
       		sta = hash_map_get_first(sta_map);
       		while (sta != NULL) {
				if (sta->dev_stats.cli_Active == true) {
					snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_PacketsSent - sta->dev_stats_last.cli_PacketsSent);
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
       		snprintf(buff, 2048, "%s WIFI_ERRORSSENT_%d:", tmp, i + 1);
       		sta = hash_map_get_first(sta_map);
       		while (sta != NULL) {
				if (sta->dev_stats.cli_Active == true) {
					snprintf(tmp, 32, "%lu,", sta->dev_stats.cli_ErrorsSent - sta->dev_stats_last.cli_ErrorsSent);
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
            
           		snprintf(tmp, 128, "%s,%d,%d;", to_sta_key(sta->sta_mac, sta_key), sta->good_rssi_time, sta->bad_rssi_time);
           		strncat(buff, tmp, 128);

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
				snprintf(buff, 2048, "%s WIFI_RECONNECT_%d:", tmp, i + 1);
				sta = hash_map_get_first(sta_map);
				while (sta != NULL) {
				
					snprintf(tmp, 128, "%s,%d;", to_sta_key(sta->sta_mac, sta_key), sta->rapid_reconnects);
					strncat(buff, tmp, 128);
				
					sta->rapid_reconnects = 0;
				
					sta = hash_map_get_next(sta_map, sta);
				
				}
				strncat(buff, "\n", 2);
				write_to_file(wifi_health_log, buff);
				wifi_dbg_print(1, "%s", buff);
			}
		}

#if !defined(_COSA_BCM_MIPS_) && !defined(_CBR_PRODUCT_REQ_) && !defined(_COSA_BCM_ARM_) && !defined(INTEL_PUMA7)
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
                g_monitor_module.ap_params[i].rapid_reconnect_threshold = rapid_reconn_max;
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

	sta_map = g_monitor_module.sta_map[apIndex];

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
            if (idx < MAX_VAP)
                flag[idx-1] = 1;

            while ((value = strtok(NULL, ",")) != NULL)
            {
                idx = atoi(value);
                if (idx < MAX_VAP)
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
    INT idx = 0;
	sta_key_t sta_key;
    ULONG channel = 0;
    char buffer[2048] = {0};
	hash_map_t     *sta_map;
	sta_data_t *sta;
	char tmp[128] = {0};

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

		sta_map = g_monitor_module.sta_map[apIndex];
		sta = hash_map_get_first(sta_map);

		while (sta != NULL) {

            char cmd[CLIENT_STATS_MAX_LEN_BUF] = {0};
            char buf[CLIENT_STATS_MAX_LEN_BUF] = {0};
            INT len = 0;
            char *value = NULL;
            char *ptr = NULL;
            FILE *fp  = NULL;

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
                char cmd[CLIENT_STATS_MAX_LEN_BUF] = {0};
                char buf[CLIENT_STATS_MAX_LEN_BUF] = {0};
                INT len = 0;
                char *value = NULL;
                char *ptr = NULL;
                FILE *fp = NULL;

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
                char cmd[CLIENT_STATS_MAX_LEN_BUF] = {0};
                char buf[CLIENT_STATS_MAX_LEN_BUF] = {0};
                INT len = 0;
                char *value = NULL;
                char *ptr = NULL;
                FILE *fp = NULL;

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
            ULONG txpower = 0;
            BOOL enable = false;
            char buf[64] = {0};

            /* adding transmit power and countrycode */
            wifi_getRadioCountryCode(apIndex, buf);
	    memset(tmp, 0, sizeof(tmp));
	    get_formatted_time(tmp);
            write_to_file(wifi_health_log, "\n%s WIFI_COUNTRY_CODE_%d:%s", tmp, apIndex+1, buf);
            wifi_getRadioTransmitPower(apIndex, &txpower);
	    memset(tmp, 0, sizeof(tmp));
	    get_formatted_time(tmp);
            write_to_file(wifi_health_log, "\n%s WIFI_TX_PWR_dBm_%d:%lu", tmp, apIndex+1, txpower);
            wifi_getBandSteeringEnable(&enable);
	    memset(tmp, 0, sizeof(tmp));
	    get_formatted_time(tmp);
            write_to_file(wifi_health_log, "\n%s WIFI_ACL_%d:%d", tmp, apIndex+1, enable);

            wifi_getRadioAutoChannelEnable(apIndex+1, &enable);
            if (true == enable)
            {
		memset(tmp, 0, sizeof(tmp));
		get_formatted_time(tmp);
                write_to_file(wifi_health_log, "\n%s WIFI_ACS_%d:true\n", tmp, apIndex+1);
            }
            else
            {
		memset(tmp, 0, sizeof(tmp));
		get_formatted_time(tmp);
                write_to_file(wifi_health_log, "\n%s WIFI_ACS_%d:false\n", tmp, apIndex+1);
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
    char buff[2048] = {0};

    wifi_getRadioOperatingChannelBandwidth(0, buffer);
    get_sub_string(buffer, bandwidth);
    get_formatted_time(tmp);
    snprintf(buff, 2048, "%s WiFi_config_2G_chan_width_split:%s\n", tmp, bandwidth);
    write_to_file(wifi_health_log, buff);

    memset(buffer, 0, sizeof(buffer));
    memset(bandwidth, 0, sizeof(bandwidth));
    memset(tmp, 0, sizeof(tmp));
    memset(buff, 0, sizeof(buff));

    wifi_getRadioOperatingChannelBandwidth(1, buffer);
    get_sub_string(buffer, bandwidth);
    get_formatted_time(tmp);
    snprintf(buff, 2048, "%s WiFi_config_5G_chan_width_split:%s\n", tmp, bandwidth);
    write_to_file(wifi_health_log, buff);
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
	
	sta_map = g_monitor_module.sta_map[ap_index];
        hal_sta = dev;
    
	// update all sta(s) that are in the record retrieved from hal
	for (i = 0; i < num_devs; i++) {
		sta = (sta_data_t *)hash_map_get(sta_map, to_sta_key(hal_sta->cli_MACAddress, sta_key));
		if (sta == NULL) {
			sta = (sta_data_t *)malloc(sizeof(sta_data_t));
			memset(sta, 0, sizeof(sta_data_t));	
			memcpy(sta->sta_mac, hal_sta->cli_MACAddress, sizeof(mac_addr_t));
			hash_map_put(sta_map, strdup(to_sta_key(hal_sta->cli_MACAddress, sta_key)), sta);
        }

        memcpy((unsigned char *)&sta->dev_stats_last, (unsigned char *)&sta->dev_stats, sizeof(wifi_associated_dev3_t));
	memcpy((unsigned char *)&sta->dev_stats, (unsigned char *)hal_sta, sizeof(wifi_associated_dev3_t)); 

        wifi_dbg_print(1, "Current Polled for:%s Packets Sent:%d Packets Recieved:%d Errors Sent:%d Retrans:%d Retry:%d Multiple:%d\n",
            to_sta_key(sta->dev_stats.cli_MACAddress, sta_key),
            hal_sta->cli_PacketsSent, hal_sta->cli_PacketsReceived, hal_sta->cli_ErrorsSent,
            hal_sta->cli_RetransCount, hal_sta->cli_RetryCount, hal_sta->cli_MultipleRetryCount);
        wifi_dbg_print(1, "Current Stored for:%s Packets Sent:%d Packets Recieved:%d Errors Sent:%d Retrans:%d Retry:%d Multiple:%d\n",
            to_sta_key(sta->dev_stats.cli_MACAddress, sta_key),
            sta->dev_stats.cli_PacketsSent, sta->dev_stats.cli_PacketsReceived, sta->dev_stats.cli_ErrorsSent,
            sta->dev_stats.cli_RetransCount, sta->dev_stats.cli_RetryCount, sta->dev_stats.cli_MultipleRetryCount);
        wifi_dbg_print(1, "Current Last for: %s Packets Sent:%d Packets Recieved:%d Errors Sent:%d Retrans:%d Retry:%d Multiple:%d\n",
            to_sta_key(sta->dev_stats.cli_MACAddress, sta_key),
            sta->dev_stats_last.cli_PacketsSent, sta->dev_stats_last.cli_PacketsReceived, sta->dev_stats_last.cli_ErrorsSent,
            sta->dev_stats_last.cli_RetransCount, sta->dev_stats_last.cli_RetryCount, sta->dev_stats_last.cli_MultipleRetryCount);

	sta->updated = true;
        sta->dev_stats.cli_Active = true;
	sta->dev_stats.cli_SignalStrength = hal_sta->cli_SignalStrength;  //zqiu: use cli_SignalStrength as normalized rssi
	if (sta->dev_stats.cli_SignalStrength >= g_monitor_module.sta_health_rssi_threshold) {
		sta->good_rssi_time += g_monitor_module.poll_period;
	} else {
		sta->bad_rssi_time += g_monitor_module.poll_period;

	}
        
        sta->connected_time += g_monitor_module.poll_period;
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
        /*Wrong password on private SSIDs*/
        if ((dev->reason == 2) && ( ap_index == 0 || ap_index == 1 )) 
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

    sta_map = g_monitor_module.sta_map[ap_index];
    
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

    if ((tv_now.tv_sec - sta->last_disconnected_time.tv_sec) <= g_monitor_module.ap_params[ap_index].rapid_reconnect_threshold) {
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
                   sta_map = g_monitor_module.sta_map[i];
                   sta = (sta_data_t *)hash_map_get(sta_map, to_sta_key(dev->sta_mac, sta_key));
                   if ((sta != NULL) && (sta->dev_stats.cli_Active = true)) {
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

    sta_map = g_monitor_module.sta_map[ap_index];
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
    sta->last_disconnected_time.tv_sec = tv_now.tv_sec;
    sta->last_disconnected_time.tv_usec = tv_now.tv_usec;
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
        time_to_wait.tv_sec = tv_now.tv_sec + proc_data->poll_period*60;
        
        if (proc_data->last_signalled_time.tv_sec > proc_data->last_polled_time.tv_sec) {
            // if were signalled withing poll interval if last poll the wait should be shorter
            time_diff = proc_data->last_signalled_time.tv_sec - proc_data->last_polled_time.tv_sec;
            if (time_diff < proc_data->poll_period*60) {
                time_to_wait.tv_sec = tv_now.tv_sec + (proc_data->poll_period*60 - time_diff);
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
            
            proc_data->current_poll_iter++;
            gettimeofday(&proc_data->last_polled_time, NULL);
            associated_devices_diagnostics();
            proc_data->upload_period = get_upload_period(proc_data->current_poll_iter,proc_data->upload_period);
            
            if (proc_data->current_poll_iter >= proc_data->upload_period) {
                upload_client_telemetry_data();
                upload_client_debug_stats();
                /* telemetry for WiFi Channel Width for 2.4G and 5G radios */
                upload_channel_width_telemetry();
		upload_ap_telemetry_data();
                proc_data->current_poll_iter = 0;
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
    char vap_buf[16]={0};
    char tmp[128]={0};
    wifi_dbg_print(1, "Entering %s:%d \n",__FUNCTION__,__LINE__);
	get_formatted_time(tmp);
    sprintf(log_buf,"%s WIFI_VAP_PERCENT_UP:",tmp);
    for(i=0;i<MAX_VAP;i++)
    {
        vapup_percentage=((int)(vap_up_arr[i])*100)/(vap_iteration);
        char delimiter = (i+1) < MAX_VAP?';':' ';
        sprintf(vap_buf,"%d,%d%c",(i+1),vapup_percentage, delimiter);
        strcat(log_buf,vap_buf);    
    }
    strcat(log_buf,"\n");    
    write_to_file(wifi_health_log,log_buf);
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

int init_wifi_monitor ()
{
	unsigned int i;
	int	rssi, rapid_reconn_max;
    long uptimeval=0; 
	memset(g_monitor_module.cliStatsList, 0, MAX_VAP);
	g_monitor_module.poll_period = 1;
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
            g_monitor_module.ap_params[i].rapid_reconnect_threshold = 180;
        } else {
            g_monitor_module.ap_params[i].rapid_reconnect_threshold = rapid_reconn_max;
        }
	}

    gettimeofday(&g_monitor_module.last_signalled_time, NULL);
    gettimeofday(&g_monitor_module.last_polled_time, NULL);
	pthread_cond_init(&g_monitor_module.cond, NULL);
    pthread_mutex_init(&g_monitor_module.lock, NULL);

	for (i = 0; i < MAX_VAP; i++) {
		g_monitor_module.sta_map[i] = hash_map_create();
		if (g_monitor_module.sta_map[i] == NULL) {
			deinit_wifi_monitor();
			wifi_dbg_print(1, "sta map create error\n");
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

        pthread_attr_init(&attr);
        pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );
	if (pthread_create(&g_monitor_module.id, &attr, monitor_function, &g_monitor_module) != 0) {
                pthread_attr_destroy( &attr );
		deinit_wifi_monitor();
		wifi_dbg_print(1, "monitor thread create error\n");
		return -1;
	}
    pthread_attr_destroy( &attr );
    g_monitor_module.sysevent_fd = sysevent_open("127.0.0.1", SE_SERVER_WELL_KNOWN_PORT, SE_VERSION, "wifiMonitor", &g_monitor_module.sysevent_token);
    if (g_monitor_module.sysevent_fd < 0) {
        wifi_dbg_print(1, "%s:%d: Failed to opne sysevent\n", __func__, __LINE__);
    } else {
        wifi_dbg_print(1, "%s:%d: Opened sysevent\n", __func__, __LINE__);
    }
   
        pthread_mutex_lock(&g_apRegister_lock);
        wifi_newApAssociatedDevice_callback_register(device_associated);
#if !defined(_PLATFORM_RASPBERRYPI_)
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
            if(g_monitor_module.sta_map[i] != NULL)
	        hash_map_destroy(g_monitor_module.sta_map[i]);
	}
    pthread_mutex_destroy(&g_monitor_module.lock);
	pthread_cond_destroy(&g_monitor_module.cond);
}

unsigned int get_poll_period 	()
{
	return g_monitor_module.poll_period;
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

void wifi_dbg_print(int level, char *format, ...)
{
    char buff[2048] = {0};
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
