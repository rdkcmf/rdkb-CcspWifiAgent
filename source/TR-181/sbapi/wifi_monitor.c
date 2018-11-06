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
#include <sys/un.h>
#include <assert.h>
#include "ansc_status.h"
#include <sysevent/sysevent.h>

static wifi_monitor_t g_monitor_module;
static unsigned msg_id = 1000;
static const char *wifi_health_log = "/rdklogs/logs/wifihealth.txt";

pthread_mutex_t g_apRegister_lock = PTHREAD_MUTEX_INITIALIZER;
void deinit_wifi_monitor    (void);
int device_deauthenticated(int apIndex, char *mac, int reason);
int device_associated(int apIndex, wifi_associated_dev_t *associated_dev);
void associated_devices_diagnostics    (void);
unsigned int get_upload_period  (void);
void process_disconnect    (unsigned int ap_index, auth_deauth_dev_t *dev);

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

void write_to_file(const char *file_name, char *buff)
{
    FILE *fp;

    fp = fopen(file_name, "a+");
    if (fp == NULL) {
        return;
    }
   
	fputs(buff, fp);
	fflush(fp);
    fclose(fp);
}

void upload_client_telemetry_data()
{
    hash_map_t     *sta_map;
    sta_key_t    sta_key;
    unsigned int i;
    sta_data_t *sta;
    char buff[2048];
    char tmp[128];
	int rssi, rapid_reconn_max;
	bool sendIndication = FALSE;
    
    for (i = 0; i < MAX_VAP; i++) {
        sta_map = g_monitor_module.sta_map[i];
       
		if (i < 2) { 
        	// Every hour, for private SSID(s) we need to calculate the good rssi time and bad rssi time 
			// and write into wifi log in following format
        	// WIFI_GOODBADRSSI_$apindex: $MAC,$GoodRssiTime,$BadRssiTime; $MAC,$GoodRssiTime,$BadRssiTime; ....
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
    	CosaDmlWiFi_GetRapidReconnectIndicationEnable(&sendIndication, FALSE);

		if (sendIndication == TRUE) {
			bool bReconnectCountEnable = 0;

			// check whether Reconnect Count is enabled or not fro individual vAP
			CosaDmlWiFi_GetRapidReconnectCountEnable( i , &bReconnectCountEnable, FALSE );
			if( TRUE == bReconnectCountEnable )
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
}

#if 0
/* cleanup_disconnected_clients_info of disconnected clients */
static void
cleanup_disconnected_clients_info(void)
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
                wifi_dbg_print(1, "%s:%d: vAP = %d cli_disconnect_poll_count"
                            " = %d for device mac = %s\n",
                            __func__, __LINE__, ap_index+1,
                            sta->cli_disconnect_poll_count,
                            to_sta_key(sta->sta_mac, sta_key));
                sta->cli_disconnect_poll_count += 1;
                /* keep the stas for 3 poll count after client is disconnected */
                if (sta->cli_disconnect_poll_count > 4)
                {
                    wifi_dbg_print(1, "%s:%d: Device: REMOVED from hash map\n",
                                    __func__, __LINE__);
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

static void
reset_client_stats_info(unsigned int apIndex)
{
    wifi_associated_dev3_t *device = NULL;
    UINT numDevices = 0;
    INT idx;

    wifi_getApAssociatedDeviceDiagnosticResult3(apIndex, &device, &numDevices);
    if ((NULL != device) && (numDevices > 0))
    {
        for (idx = 0 ; idx < numDevices; idx++)
        {
            sta_key_t       key = {0};
            sta_data_t      *sta = NULL;
            hash_map_t      *sta_map = g_monitor_module.sta_map[apIndex];

            sta = (sta_data_t *)hash_map_get(sta_map, to_sta_key(device[idx].cli_MACAddress, key));
            if (sta)
            {
                sta->client_stats.BytesSent = device[idx].cli_BytesSent;
                sta->client_stats.BytesReceived = device[idx].cli_BytesReceived;
                sta->client_stats.PacketsSent = device[idx].cli_PacketsSent;
                sta->client_stats.PacketsReceived = device[idx].cli_PacketsReceived;
                sta->client_stats.ErrorsSent = device[idx].cli_ErrorsSent;
                sta->client_stats.RetransCount = device[idx].cli_RetransCount;
                sta->client_stats.FailedRetransCount = device[idx].cli_FailedRetransCount;
                sta->client_stats.RetryCount = device[idx].cli_RetryCount;
                sta->client_stats.MultipleRetryCount = device[idx].cli_MultipleRetryCount;
            }
        }
        free(device);
        device = NULL;
        numDevices = 0;
    }
}

static void
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
                flag[idx] = 1;

            while ((value = strtok(NULL, ",")) != NULL)
            {
                idx = atoi(value);
                if (idx < MAX_VAP)
                    flag[idx] = 1;
            }
        }
        else
        {
            flag[0] = 1;
            flag[1] = 1;
        }
    }
}

/*
 * This API will Create telemetry and data model for client activity stats
 * like BytesSent, BytesReceived, RetransCount, FailedRetransCount, etc...
 */
static void
upload_client_activity_stats(void)
{
    wifi_associated_dev3_t *device = NULL;
    UINT numDevices = 0;
    INT apIndex = 0;
    INT idx = 0;
	sta_key_t sta_key;
    ULONG channel = 0;
    char trflag[MAX_VAP] = {0};
    char nrflag[MAX_VAP] = {0};
    char stflag[MAX_VAP] = {0};
    char snflag[MAX_VAP] = {0};
    char buffer[2048] = {0};

    if  (FALSE == IsCosaDmlWiFivAPStatsFeatureEnabled())
    {
        wifi_dbg_print(1, "Client activity stats feature is disabled\n");
        return;
    }

    get_device_flag(trflag,
        "psmcli get dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WHIX.TxRxRateList");
    get_device_flag(nrflag,
        "psmcli get dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WHIX.NormalizedRssiList");
    get_device_flag(stflag,
        "psmcli get dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WHIX.CliStatList");
    get_device_flag(snflag,
        "psmcli get dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WIFI_TELEMETRY.SNRList");

    for (apIndex = 0; apIndex < MAX_VAP; apIndex++)
    {
        if (FALSE == IsCosaDmlWiFiApStatsEnable(apIndex))
        {
            wifi_dbg_print(1, "Stats feature is disabled for apIndex = %d\n",
                            apIndex+1);
            continue;
        }

        wifi_getApAssociatedDeviceDiagnosticResult3(apIndex,
                    &device, &numDevices);
        if ((NULL != device) && (numDevices > 0))
        {
            for (idx = 0 ; idx < numDevices; idx++)
            {
                char cmd[CLIENT_STATS_MAX_LEN_BUF] = {0};
                char buf[CLIENT_STATS_MAX_LEN_BUF] = {0};
                INT len = 0;
                char *value = NULL;
                char *ptr = NULL;
                FILE *fp  = NULL;

                snprintf(cmd, CLIENT_STATS_MAX_LEN_BUF,
                        "dmesg | grep FA_INFO_%s | tail -1",
                        to_sta_key(device[idx].cli_MACAddress, sta_key));
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
                        write_to_file(wifi_health_log, "\nWIFI_UAPSD_%d:%s",
                                        apIndex+1, value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log,
                                        "\nWIFI_TX_DISCARDS_CNT_%d:%s",
                                        apIndex+1, value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log,
                                        "\nWIFI_TX_PKTS_CNT_%d:%s",
                                        apIndex+1, value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log,
                                        "\nWIFI_BMP_CLR_CNT_%d:%s",
                                        apIndex+1, value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log,
                                        "\nWIFI_BMP_SET_CNT_%d:%s",
                                        apIndex+1, value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log,
                                        "\nWIFI_TIM_%d:%s", apIndex+1, value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log,
                                        "\nWIFI_AID_%d:%s", apIndex+1, value);
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
                        to_sta_key(device[idx].cli_MACAddress, sta_key));
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
                        write_to_file(wifi_health_log,
                                        "\nWIFI_DATA_QUEUED_CNT_%d:%s",
                                        apIndex+1, value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log,
                                        "\nWIFI_DATA_DROPPED_CNT_%d:%s",
                                        apIndex+1, value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log,
                                        "\nWIFI_DATA_DEQUED_TX_CNT_%d:%s",
                                        apIndex+1, value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log,
                                        "\nWIFI_DATA_DEQUED_DROPPED_CNT_%d:%s",
                                        apIndex+1, value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log,
                                        "\nWIFI_DATA_EXP_DROPPED_CNT_%d:%s",
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
                        to_sta_key(device[idx].cli_MACAddress, sta_key));
                fp = popen(cmd, "r");
                if (fp == NULL)
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
                        write_to_file(wifi_health_log,
                                        "\nWIFI_MGMT_QUEUED_CNT_%d:%s",
                                        apIndex+1, value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log,
                                        "\nWIFI_MGMT_DROPPED_CNT_%d:%s",
                                        apIndex+1, value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log,
                                        "\nWIFI_MGMT_DEQUED_TX_CNT_%d:%s",
                                        apIndex+1, value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log,
                                        "\nWIFI_MGMT_DEQUED_DROPPED_CNT_%d:%s",
                                        apIndex+1, value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log,
                                        "\nWIFI_MGMT_EXP_DROPPED_CNT_%d:%s",
                                        apIndex+1, value);
                    }
                }
                else
                {
                    wifi_dbg_print(1, "Failed to run popen command\n" );
                }
            }

            snprintf(buffer, 2048, "\nWIFI_MAC_%d:", apIndex+1);
            for (idx = 0; idx < numDevices; idx++)
            {
                char tmp[32] = {0};

                snprintf(tmp, 32, "%s,",
                        to_sta_key(device[idx].cli_MACAddress, sta_key));
                strncat(buffer, tmp, 32);
            }
            write_to_file(wifi_health_log, buffer);

            write_to_file(wifi_health_log, "\nWIFI_MAC_%d_TOTAL_COUNT:%d", apIndex+1,
                    numDevices);

            snprintf(buffer, 2048, "\nWIFI_RSSI_%d:", apIndex+1);
            for (idx = 0; idx < numDevices; idx++)
            {
                char tmp[32] = {0};

                snprintf(tmp, 32, "%d,", device[idx].cli_RSSI);
                strncat(buffer, tmp, 32);
            }
            write_to_file(wifi_health_log, buffer);

            if (nrflag[apIndex])
            {
                snprintf(buffer, 2048, "\nWIFI_NORMALIZED_RSSI_%d:", apIndex+1);
                for (idx = 0; idx < numDevices; idx++)
                {
                    char tmp[32] = {0};

                    snprintf(tmp, 32, "%d,", device[idx].cli_SignalStrength);
                    strncat(buffer, tmp, 32);
                }
                write_to_file(wifi_health_log, buffer);
            }

            if (snflag[apIndex])
            {
                snprintf(buffer, 2048, "\nWIFI_SNR_%d:", apIndex+1);
                for (idx = 0; idx < numDevices; idx++)
                {
                    char tmp[32] = {0};

                    snprintf(tmp, 32, "%d,", device[idx].cli_SNR);
                    strncat(buffer, tmp, 32);
                }
                write_to_file(wifi_health_log, buffer);
            }

            snprintf(buffer, 2048, "\nWIFI_TXCLIENTS_%d:", apIndex+1);
            for (idx = 0; idx < numDevices; idx++)
            {
                char tmp[32] = {0};

                snprintf(tmp, 32, "%u,", device[idx].cli_LastDataDownlinkRate);
                strncat(buffer, tmp, 32);
            }
            write_to_file(wifi_health_log, buffer);

            snprintf(buffer, 2048, "\nWIFI_RXCLIENTS_%d:", apIndex+1);
            for (idx = 0; idx < numDevices; idx++)
            {
                char tmp[32] = {0};

                snprintf(tmp, 32, "%u,", device[idx].cli_LastDataUplinkRate);
                strncat(buffer, tmp, 32);
            }
            write_to_file(wifi_health_log, buffer);

            if (trflag[apIndex])
            {
                snprintf(buffer, 2048, "\nWIFI_MAX_TXCLIENTS_%d:", apIndex+1);
                for (idx = 0; idx < numDevices; idx++)
                {
                    char tmp[32] = {0};

                    snprintf(tmp, 32, "%u,", device[idx].cli_MaxDownlinkRate);
                    strncat(buffer, tmp, 32);
                }
                write_to_file(wifi_health_log, buffer);

                snprintf(buffer, 2048, "\nWIFI_MAX_RXCLIENTS_%d:", apIndex+1);
                for (idx = 0; idx < numDevices; idx++)
                {
                    char tmp[32] = {0};

                    snprintf(tmp, 32, "%u,", device[idx].cli_MaxUplinkRate);
                    strncat(buffer, tmp, 32);
                }
                write_to_file(wifi_health_log, buffer);
            }

            wifi_getRadioChannel(apIndex%2, &channel);
            write_to_file(wifi_health_log, "\nWIFI_CHANNEL_%d:%lu", apIndex+1, channel);

            snprintf(buffer, 2048, "\nWIFI_CHANNEL_WIDTH_%d:", apIndex+1);
            for (idx = 0; idx < numDevices; idx++)
            {
                char tmp[64] = {0};

                snprintf(tmp, 64, "%s,", device[idx].cli_OperatingChannelBandwidth);
                strncat(buffer, tmp, 64);
            }
            write_to_file(wifi_health_log, buffer);

            snprintf(buffer, 2048, "\nWIFI_RXTXCLIENTDELTA_%d:", apIndex+1);
            for (idx = 0; idx < numDevices; idx++)
            {
                char tmp[32] = {0};

                snprintf(tmp, 32, "%d,", (int)(device[idx].cli_LastDataDownlinkRate - device[idx].cli_LastDataUplinkRate));
                strncat(buffer, tmp, 32);
            }
            write_to_file(wifi_health_log, buffer);

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
                        ptr++;

                        value = strtok(ptr, ",");
                        write_to_file(wifi_health_log, "WIFI_PS_CLIENTS_1:%s\n", value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log, "WIFI_PS_CLIENTS_DATA_QUEUE_LEN_1:%s\n",
                                value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log, "WIFI_PS_CLIENTS_DATA_QUEUE_BYTES_1:%s\n",
                                value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log, "WIFI_PS_CLIENTS_DATA_FRAME_LEN_1:%s\n",
                                value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log, "WIFI_PS_CLIENTS_DATA_FRAME_COUNT_1:%s\n",
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
                        ptr++;

                        value = strtok(ptr, ",");
                        write_to_file(wifi_health_log, "WIFI_PS_CLIENTS_2:%s\n", value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log, "WIFI_PS_CLIENTS_DATA_QUEUE_LEN_2:%s\n",
                                value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log, "WIFI_PS_CLIENTS_DATA_QUEUE_BYTES_2:%s\n",
                                value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log, "WIFI_PS_CLIENTS_DATA_FRAME_LEN_2:%s\n",
                                value);
                        value = strtok(NULL, ",");
                        write_to_file(wifi_health_log, "WIFI_PS_CLIENTS_DATA_FRAME_COUNT_2:%s\n",
                                value);
                    }
                }
                else
                {
                    wifi_dbg_print(1, "Failed to run popen command\n" );
                }
            }

            if (stflag[apIndex])
            {
                hash_map_t *sta_map = g_monitor_module.sta_map[apIndex];

                snprintf(buffer, 2048, "\nWIFI_BYTESSENTCLIENTS_%d:", apIndex+1);
                for (idx = 0; idx < numDevices; idx++)
                {
                    char tmp[32] = {0};
                    sta_key_t key = {0};
                    sta_data_t      *sta = NULL;

                    sta = (sta_data_t *)hash_map_get(sta_map,
                            to_sta_key(device[idx].cli_MACAddress, key));
                    if (sta)
                    {
                        snprintf(tmp, 32, "%lu,", device[idx].cli_BytesSent - sta->client_stats.BytesSent);
                        strncat(buffer, tmp, 32);
                        sta->client_stats.BytesSent = device[idx].cli_BytesSent;
                    }
                }
                write_to_file(wifi_health_log, buffer);

                snprintf(buffer, 2048, "\nWIFI_BYTESRECEIVEDCLIENTS_%d:", apIndex+1);
                for (idx = 0; idx < numDevices; idx++)
                {
                    char tmp[32] = {0};
                    sta_key_t       key = {0};
                    sta_data_t      *sta = NULL;

                    sta = (sta_data_t *)hash_map_get(sta_map,
                            to_sta_key(device[idx].cli_MACAddress, key));
                    if (sta)
                    {
                        snprintf(tmp, 32, "%lu,", device[idx].cli_BytesReceived - sta->client_stats.BytesReceived);
                        strncat(buffer, tmp, 32);
                        sta->client_stats.BytesReceived = device[idx].cli_BytesReceived;
                    }
                }
                write_to_file(wifi_health_log, buffer);

                snprintf(buffer, 2048, "\nWIFI_PACKETSSENTCLIENTS_%d:", apIndex+1);
                for (idx = 0; idx < numDevices; idx++)
                {
                    char tmp[32] = {0};
                    sta_key_t       key = {0};
                    sta_data_t      *sta = NULL;

                    sta = (sta_data_t *)hash_map_get(sta_map,
                            to_sta_key(device[idx].cli_MACAddress, key));
                    if (sta)
                    {
                        snprintf(tmp, 32, "%lu,", device[idx].cli_PacketsSent - sta->client_stats.PacketsSent);
                        strncat(buffer, tmp, 32);
                        sta->client_stats.PacketsSent = device[idx].cli_PacketsSent;
                    }
                }
                write_to_file(wifi_health_log, buffer);

                snprintf(buffer, 2048, "\nWIFI_PACKETSRECEIVEDCLIENTS_%d:", apIndex+1);
                for (idx = 0; idx < numDevices; idx++)
                {
                    char tmp[32] = {0};
                    sta_key_t       key = {0};
                    sta_data_t      *sta = NULL;

                    sta = (sta_data_t *)hash_map_get(sta_map, to_sta_key(device[idx].cli_MACAddress, key));
                    if (sta)
                    {
                        snprintf(tmp, 32, "%lu,", device[idx].cli_PacketsReceived - sta->client_stats.PacketsReceived);
                        strncat(buffer, tmp, 32);
                        sta->client_stats.PacketsReceived = device[idx].cli_PacketsReceived;
                    }
                }
                write_to_file(wifi_health_log, buffer);

                snprintf(buffer, 2048, "\nWIFI_ERRORSSENT_%d:", apIndex+1);
                for (idx = 0; idx < numDevices; idx++)
                {
                    char tmp[32] = {0};

                    sta_key_t       key = {0};
                    sta_data_t      *sta = NULL;

                    sta = (sta_data_t *)hash_map_get(sta_map, to_sta_key(device[idx].cli_MACAddress, key));
                    if (sta)
                    {
                        snprintf(tmp, 32, "%lu,", device[idx].cli_ErrorsSent - sta->client_stats.ErrorsSent);
                        strncat(buffer, tmp, 32);
                        sta->client_stats.ErrorsSent = device[idx].cli_ErrorsSent;
                    }
                }
                write_to_file(wifi_health_log, buffer);

                snprintf(buffer, 2048, "\nWIFI_RETRANSCOUNT_%d:", apIndex+1);
                for (idx = 0; idx < numDevices; idx++)
                {
                    char tmp[32] = {0};
                    sta_key_t       key = {0};
                    sta_data_t      *sta = NULL;

                    sta = (sta_data_t *)hash_map_get(sta_map, to_sta_key(device[idx].cli_MACAddress, key));
                    if (sta)
                    {
                        snprintf(tmp, 32, "%lu,", device[idx].cli_RetransCount - sta->client_stats.RetransCount);
                        strncat(buffer, tmp, 32);
                        sta->client_stats.RetransCount = device[idx].cli_RetransCount;
                    }
                }
                write_to_file(wifi_health_log, buffer);

                snprintf(buffer, 2048, "\nWIFI_FAILEDRETRANSCOUNT_%d:", apIndex+1);
                for (idx = 0; idx < numDevices; idx++)
                {
                    char tmp[32] = {0};
                    sta_key_t       key = {0};
                    sta_data_t      *sta = NULL;

                    sta = (sta_data_t *)hash_map_get(sta_map, to_sta_key(device[idx].cli_MACAddress, key));
                    if (sta)
                    {
                        snprintf(tmp, 32, "%lu,", device[idx].cli_FailedRetransCount - sta->client_stats.FailedRetransCount);
                        strncat(buffer, tmp, 32);
                        sta->client_stats.FailedRetransCount = device[idx].cli_FailedRetransCount;
                    }
                }
                write_to_file(wifi_health_log, buffer);

                snprintf(buffer, 2048, "\nWIFI_RETRYCOUNT_%d:", apIndex+1);
                for (idx = 0; idx < numDevices; idx++)
                {
                    char tmp[32] = {0};
                    sta_key_t       key = {0};
                    sta_data_t      *sta = NULL;

                    sta = (sta_data_t *)hash_map_get(sta_map, to_sta_key(device[idx].cli_MACAddress, key));
                    if (sta)
                    {
                        snprintf(tmp, 32, "%lu,", device[idx].cli_RetryCount - sta->client_stats.RetryCount);
                        strncat(buffer, tmp, 32);
                        sta->client_stats.RetryCount = device[idx].cli_RetryCount;
                    }
                }
                write_to_file(wifi_health_log, buffer);

                snprintf(buffer, 2048, "\nWIFI_MULTIPLERETRYCOUNT_%d:", apIndex+1);
                for (idx = 0; idx < numDevices; idx++)
                {
                    char tmp[32] = {0};
                    sta_key_t       key = {0};
                    sta_data_t      *sta = NULL;

                    sta = (sta_data_t *)hash_map_get(sta_map, to_sta_key(device[idx].cli_MACAddress, key));
                    if (sta)
                    {
                        snprintf(tmp, 32, "%lu,", device[idx].cli_MultipleRetryCount - sta->client_stats.MultipleRetryCount);
                        strncat(buffer, tmp, 32);
                        sta->client_stats.MultipleRetryCount = device[idx].cli_MultipleRetryCount;
                    }
                }
                write_to_file(wifi_health_log, buffer);
            }

            free(device);
            device = NULL;
            numDevices = 0;
        }
        else
        {
            write_to_file(wifi_health_log, "\nWIFI_MAC_%d_TOTAL_COUNT:0", apIndex+1);
            wifi_getRadioChannel(apIndex%2, &channel);
            write_to_file(wifi_health_log, "\nWIFI_CHANNEL_%d:%lu\n", apIndex+1, channel);
        }

        if ((apIndex == 0) || (apIndex == 1))
        {
            ULONG txpower = 0;
            BOOL enable = FALSE;
            char buf[64] = {0};

            /* adding transmit power and countrycode */
            wifi_getRadioCountryCode(apIndex, buf);
            write_to_file(wifi_health_log, "\nWIFI_COUNTRY_CODE_%d:%s", apIndex+1, buf);
            wifi_getRadioTransmitPower(apIndex, &txpower);
            write_to_file(wifi_health_log, "\nWIFI_TX_PWR_dBm_%d:%lu", apIndex+1, txpower);
            wifi_getBandSteeringEnable(&enable);
            write_to_file(wifi_health_log, "\nWIFI_ACL_%d:%d", apIndex+1, enable);

            wifi_getRadioAutoChannelEnable(apIndex+1, &enable);
            if (TRUE == enable)
            {
                write_to_file(wifi_health_log, "\nWIFI_ACS_%d:true\n", apIndex+1);
            }
            else
            {
                write_to_file(wifi_health_log, "\nWIFI_ACS_%d:false\n", apIndex+1);
            }
        }
    }
}

static void
process_statsFlagChagne(unsigned int ap_index, client_stats_enable_t *flag)
{
    if (0 == flag->type) //Device.WiFi.X_RDKCENTRAL-COM_vAPStatsEnable = 0
    {
        int idx;

        write_to_file(wifi_health_log, "WIFI_STATS_FEATURE_ENABLE:%s\n",
                (flag->enable) ? "TRUE" : "FALSE");
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
                    (flag->enable) ? "TRUE" : "FALSE");
        }
    }
}

/*
 * wifi_stats_flag_change()
 * ap_index vAP
 * enable   TRUE/FALSE
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

#endif

void process_diagnostics	(unsigned int ap_index, wifi_associated_dev3_t *dev, unsigned int num_devs)
{
	hash_map_t     *sta_map;
	sta_data_t *sta;	
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

	sta->updated = true;
	sta->last_rssi = hal_sta->cli_SignalStrength;  //zqiu: use cli_SignalStrength as normalized rssi
	if (sta->last_rssi >= g_monitor_module.sta_health_rssi_threshold) {
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
		}
        
        sta = hash_map_get_next(sta_map, sta);
	}

}

void process_deauthenticate	(unsigned int ap_index, auth_deauth_dev_t *dev)
{
    char buff[2048];
    char tmp[128];
   	sta_key_t sta_key;
 
    wifi_dbg_print(1, "Device:%s deauthenticated on ap:%d\n", to_sta_key(dev->sta_mac, sta_key), ap_index);
	get_formatted_time(tmp);
       	 
   	snprintf(buff, 2048, "%s WIFI_PASSWORD_FAIL:%d,%s\n", tmp, ap_index + 1, to_sta_key(dev->sta_mac, sta_key));
	// send telemetry of password failure
	write_to_file(wifi_health_log, buff);

	process_disconnect(ap_index, dev);
}

void process_connect	(unsigned int ap_index, auth_deauth_dev_t *dev)
{
    sta_key_t sta_key;
    sta_data_t *sta;
    hash_map_t     *sta_map;
    struct timeval tv_now;
    
    sta_map = g_monitor_module.sta_map[ap_index];
    
    wifi_dbg_print(1, "sta map: %p Device:%s connected on ap:%d\n", sta_map, to_sta_key(dev->sta_mac, sta_key), ap_index);
    sta = (sta_data_t *)hash_map_get(sta_map, to_sta_key(dev->sta_mac, sta_key));
    if (sta == NULL) {
        sta = (sta_data_t *)malloc(sizeof(sta_data_t));
        memset(sta, 0, sizeof(sta_data_t));
        memcpy(sta->sta_mac, dev->sta_mac, sizeof(mac_addr_t));
        hash_map_put(sta_map, strdup(to_sta_key(sta->sta_mac, sta_key)), sta);
    }
    
    sta->total_disconnected_time += sta->disconnected_time;
    sta->disconnected_time = 0;
    
    gettimeofday(&tv_now, NULL);
    if ((tv_now.tv_sec - sta->last_connected_time.tv_sec) <= g_monitor_module.ap_params[ap_index].rapid_reconnect_threshold) {
        wifi_dbg_print(1, "Device:%s connected on ap:%d connected within rapid reconnect time\n", to_sta_key(dev->sta_mac, sta_key), ap_index);
        sta->rapid_reconnects++;    
    }
    
    sta->last_connected_time.tv_sec = tv_now.tv_sec;
    sta->last_connected_time.tv_usec = tv_now.tv_usec;
}

void process_disconnect	(unsigned int ap_index, auth_deauth_dev_t *dev)
{
    sta_key_t sta_key;
    sta_data_t *sta;
    hash_map_t     *sta_map;

    sta_map = g_monitor_module.sta_map[ap_index];
    wifi_dbg_print(1, "Device:%s disconnected on ap:%d\n", to_sta_key(dev->sta_mac, sta_key), ap_index);
    sta = (sta_data_t *)hash_map_get(sta_map, to_sta_key(dev->sta_mac, sta_key));
    if (sta == NULL) {
		wifi_dbg_print(1, "Device:%s could not be found on sta map of ap:%d\n", to_sta_key(dev->sta_mac, sta_key), ap_index);
		return;
    }
    
    sta->total_connected_time += sta->connected_time;
    sta->connected_time = 0;

    /*
     * Need to keep the client in hash for 3 poll times after client is disconnected.
     * Set the cli_disconnect_poll_count to 1 to identify this client is disconnected.
     */
    //sta->cli_disconnect_poll_count = 1;
    /* reset stats of disconnected client */
    //memset((unsigned char *)&sta->client_stats, 0, sizeof(wifi_client_stats_t));
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
            proc_data->upload_period = get_upload_period();
            
            if (proc_data->current_poll_iter >= proc_data->upload_period) {
                upload_client_telemetry_data();
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

void associated_devices_diagnostics	(void)
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
	//wifi_dbg_print(1, "%s:%d:Exit\n", __func__, __LINE__);      
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
    data->id = msg_id++;

	data->event_type = monitor_event_type_deauthenticate;

    data->ap_index = ap_index;
	sscanf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
           &mac_addr[0], &mac_addr[1], &mac_addr[2],
           &mac_addr[3], &mac_addr[4], &mac_addr[5]);
    data->u.dev.sta_mac[0] = mac_addr[0]; data->u.dev.sta_mac[1] = mac_addr[1]; data->u.dev.sta_mac[2] = mac_addr[2];
    data->u.dev.sta_mac[3] = mac_addr[3]; data->u.dev.sta_mac[4] = mac_addr[4]; data->u.dev.sta_mac[5] = mac_addr[5];
	data->u.dev.reason = reason;

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
 
	g_monitor_module.poll_period = 1;
    g_monitor_module.upload_period = get_upload_period();
    g_monitor_module.current_poll_iter = 0;

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
	if (pthread_create(&g_monitor_module.id, NULL, monitor_function, &g_monitor_module) != 0) {
		deinit_wifi_monitor();
		wifi_dbg_print(1, "monitor thread create error\n");
		return -1;
	}

    g_monitor_module.sysevent_fd = sysevent_open("127.0.0.1", SE_SERVER_WELL_KNOWN_PORT, SE_VERSION, "wifiMonitor", &g_monitor_module.sysevent_token);
    if (g_monitor_module.sysevent_fd < 0) {
        wifi_dbg_print(1, "%s:%d: Failed to opne sysevent\n", __func__, __LINE__);
    } else {
        wifi_dbg_print(1, "%s:%d: Opened sysevent\n", __func__, __LINE__);
    }
   
        pthread_mutex_lock(&g_apRegister_lock);
        wifi_newApAssociatedDevice_callback_register(device_associated);
        //wifi_apAuthEvent_callback_register(device_deauthenticated);
	wifi_apDisassociatedDevice_callback_register(device_disassociated);
        pthread_mutex_unlock(&g_apRegister_lock);
    
	return 0;
}

void deinit_wifi_monitor	()
{
	unsigned int i;

	sysevent_close(g_monitor_module.sysevent_fd, g_monitor_module.sysevent_token);
	queue_destroy(g_monitor_module.queue);
	for (i = 0; i < MAX_VAP; i++) {
		hash_map_destroy(g_monitor_module.sta_map[i]);
	}
    pthread_mutex_destroy(&g_monitor_module.lock);
	pthread_cond_destroy(&g_monitor_module.cond);
}

unsigned int get_poll_period 	()
{
	return g_monitor_module.poll_period;
}

unsigned int get_upload_period  ()
{
    FILE *fp;
    char buff[64];
    char *ptr;
    
    if ((fp = fopen("/tmp/upload", "r")) == NULL) {
        return 60;
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
    char buff[1024];
    va_list list;
    static FILE *fp = NULL;
    
	if ((access("/nvram/wifiMonDbg", R_OK)) != 0) {
        return;
    }

    va_start(list, format);
    vsprintf(buff, format, list);
    va_end(list);
    
    if (fp == NULL) {
        fp = fopen("/tmp/wifiMon", "a+");
        if (fp == NULL) {
            return;
        } else {
            fputs(buff, fp);
        }
    } else {
        fputs(buff, fp);
    }
    
    fflush(fp);
}
