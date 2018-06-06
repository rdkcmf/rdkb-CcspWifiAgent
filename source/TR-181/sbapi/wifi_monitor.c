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

void deinit_wifi_monitor    (void);
int device_deauthenticated(int apIndex, char *mac, int reason);
int device_associated(int apIndex, wifi_associated_dev_t *associated_dev, int reason);
void associated_devices_diagnostics    (void);
unsigned int get_max_upload_period  (void);
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

void upload_ap_telemetry_data()
{
    char buff[2048];
    char tmp[128];
	wifi_radioTrafficStats2_t stats;
	unsigned int i;
    
    for (i = 0; i < MAX_VAP; i++) {
		wifi_getRadioTrafficStats2(i, &stats);
		get_formatted_time(tmp);
        snprintf(buff, 2048, "%s WIFI_NOISE_FLOOR_%d:%d\n", tmp, i + 1, stats.radio_NoiseFloor);
		
        write_to_file(wifi_health_log, buff);
        wifi_dbg_print(1, "%s", buff);
	}
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
    
    for (i = 0; i < MAX_VAP; i++) {
        sta_map = g_monitor_module.sta_map[i];
        
        // Every hour, we need to calculate the good rssi time and bad rssi time and write into wifi log in following format
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

#ifdef WAIT_FOR_VENDOR 
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
#endif
        
    }
    

	// update thresholds if changed
	if (CosaDmlWiFi_GetGoodRssiThresholdValue(&rssi) == ANSC_STATUS_SUCCESS) {
		g_monitor_module.sta_health_rssi_threshold = rssi;
	}

    for (i = 0; i < MAX_RADIOS; i++) {
		// update rapid reconnect time limit if changed
        if (CosaDmlWiFi_GetRapidReconnectThresholdValue(i, &rapid_reconn_max) == ANSC_STATUS_SUCCESS) {
            g_monitor_module.radio_params[i].rapid_reconnect_threshold = rapid_reconn_max;
        }
	}    
}

void process_diagnostics	(unsigned int ap_index, wifi_associated_dev_t *dev, unsigned int num_devs)
{
	hash_map_t     *sta_map;
	sta_data_t *sta;	
	unsigned int i;
	wifi_associated_dev_t	*hal_sta;
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
 
    wifi_dbg_print(1, "Device:%s deauthenticated on ap:%d reason:%d\n", 
		to_sta_key(dev->sta_mac, sta_key), ap_index, dev->reason);

	// just discard anything from ssid(s) other than private
	if (ap_index >= 2) {
		wifi_dbg_print(1, "Indication from ap:%d discarding\n", ap_index);
		return;
	}

	// reason: 0 = unknown; 1 = wrong password; 2 = timeout;
	if (dev->reason == 1) { 
		get_formatted_time(tmp);
       	 
   		snprintf(buff, 2048, "%s WIFI_PASSWORD_FAIL:%d,%s\n", tmp, ap_index + 1, to_sta_key(dev->sta_mac, sta_key));
		// send telemetry of password failure
		write_to_file(wifi_health_log, buff);
	}

	process_disconnect(ap_index, dev);
}

void process_connect	(unsigned int ap_index, auth_deauth_dev_t *dev)
{
    sta_key_t sta_key;
    sta_data_t *sta;
    hash_map_t     *sta_map;
    struct timeval tv_now;
    
    sta_map = g_monitor_module.sta_map[ap_index];
    
    wifi_dbg_print(1, "Device:%s connected on ap:%d\n", to_sta_key(dev->sta_mac, sta_key), ap_index);
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
    if ((tv_now.tv_sec - sta->last_connected_time.tv_sec) <= g_monitor_module.radio_params[ap_index%2].rapid_reconnect_threshold) {
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
        //assert(0);
		return;
    }
    
    sta->total_connected_time += sta->connected_time;
    sta->connected_time = 0;

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
            proc_data->max_upload_period = get_max_upload_period();
            
            if (proc_data->current_poll_iter >= proc_data->max_upload_period) {
                upload_client_telemetry_data();
				upload_ap_telemetry_data();
                proc_data->current_poll_iter = 0;
            } else if ((proc_data->current_poll_iter % (proc_data->max_upload_period/4)) == 0) {
				upload_ap_telemetry_data();
			}
		} else {
        	pthread_mutex_unlock(&proc_data->lock);
			return NULL;
		}

        pthread_mutex_unlock(&proc_data->lock);
    }
        
    
    return NULL;
}

void associated_devices_diagnostics	()
{
	wifi_associated_dev_t *dev_array = NULL;
	unsigned int i, num_devs = 0;
      
	for (i = 0; i < MAX_VAP; i++) { 
		wifi_getApAssociatedDeviceDiagnosticResult(i, &dev_array, &num_devs);
        process_diagnostics(i, dev_array, num_devs);
        
        if (num_devs > 0) {
            free(dev_array);
        }
	}

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

int device_associated(int ap_index, wifi_associated_dev_t *associated_dev, int reason)
{
    wifi_monitor_data_t *data;

    data = (wifi_monitor_data_t *)malloc(sizeof(wifi_monitor_data_t));
    data->id = msg_id++;

    data->event_type = monitor_event_type_connect;

    data->ap_index = ap_index;
    data->u.dev.reason = reason;
    memcpy(data->u.dev.sta_mac, associated_dev->cli_MACAddress, sizeof(mac_addr_t));

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
    g_monitor_module.max_upload_period = get_max_upload_period();
    g_monitor_module.current_poll_iter = 0;

	if (CosaDmlWiFi_GetGoodRssiThresholdValue(&rssi) != ANSC_STATUS_SUCCESS) {
		g_monitor_module.sta_health_rssi_threshold = -65;
	} else {
		g_monitor_module.sta_health_rssi_threshold = rssi;
	}
    
    for (i = 0; i < MAX_RADIOS; i++) {
        // update rapid reconnect time limit if changed
        if (CosaDmlWiFi_GetRapidReconnectThresholdValue(i, &rapid_reconn_max) != ANSC_STATUS_SUCCESS) {
            g_monitor_module.radio_params[i].rapid_reconnect_threshold = 180;
        } else {
            g_monitor_module.radio_params[i].rapid_reconnect_threshold = rapid_reconn_max;
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

    //wifi_newApAssociatedDevice_callback_register(device_associated);
    wifi_apAuthEvent_callback_register(device_deauthenticated);
	//wifi_apDisassociatedDevice_callback_register(device_disassociated);
    
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

unsigned int get_max_upload_period  ()
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

#ifdef DEBUG
void wifi_dbg_print(int level, char *format, ...)
{
    char buff[1024];
    va_list list;
    static FILE *fp = NULL;
    
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
#endif
