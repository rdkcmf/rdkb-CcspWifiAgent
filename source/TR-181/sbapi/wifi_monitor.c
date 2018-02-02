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


static wifi_monitor_t g_monitor_module;
static unsigned msg_id = 1000;

void deinit_wifi_monitor    (void);
void associated_devices_diagnostics    (void);
unsigned int get_upload_period  (void);
static inline char *to_sta_key    (mac_addr_t mac, sta_key_t key) {
    snprintf(key, STA_KEY_LEN, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return (char *)key;
}


void upload_monitor_data()
{
    hash_map_t     *sta_map;
    sta_key_t    sta_key;
    unsigned int i;
    sta_data_t *sta;
    char buff[2048];
    char tmp[128];
    struct tm *tm_info;
    struct timeval tv_now;
    FILE *fp;
	int rssi;
    
    // Every hour, we need to calculate the good rssi time and bad rssi time and write into wifi log in following format
    // WIFI_GOODBADRSSI_$apindex: $MAC,$GoodRssiTime,$BadRssiTime; $MAC,$GoodRssiTime,$BadRssiTime; ....
    
    fp = fopen("/rdklogs/logs/goodbad_rssi", "a+");
    
    for (i = 0; i < MAX_AP; i++) {
        gettimeofday(&tv_now, NULL);
        tm_info = localtime(&tv_now.tv_sec);
        
        strftime(tmp, 128, "%y%m%d-%T", tm_info);
        
        snprintf(buff, 2048, "%s.%06d WIFI_GOODBADRSSI_%d:", tmp, tv_now.tv_usec, i + 1);
        sta_map = g_monitor_module.sta_map[i];
        sta = hash_map_get_first(sta_map);
        while (sta != NULL) {
            
            sta->total_connected_time += sta->connected_time;
            sta->connected_time = 0;
            
            sta->total_disconnected_time += sta->disconnected_time;
            sta->disconnected_time = 0;
            
            snprintf(tmp, 128, "%s,%d,%d;", to_sta_key(sta->sta_mac, sta_key), sta->good_rssi_time, sta->bad_rssi_time);
            strncat(buff, tmp, 128);
            sta = hash_map_get_next(sta_map, sta);
            
        }
        strncat(buff, "\n", 2);
        fputs(buff, fp);
        
        wifi_dbg_print(1, "%s", buff);
    }
    
    fflush(fp);
    fclose(fp);

	// update thresholds if changed
	if (CosaDmlWiFi_GetGoodRssiThresholdValue(&rssi) == ANSC_STATUS_SUCCESS) {
		g_monitor_module.sta_health_rssi_threshold = rssi;
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

void process_connect	(unsigned int ap_index, mac_addr_t *sta_mac)
{
    sta_key_t sta_key;
    sta_data_t *sta;
    hash_map_t     *sta_map;
    struct timeval tv_now;
    
    sta_map = g_monitor_module.sta_map[ap_index];
    
    wifi_dbg_print(1, "Device:%s connected on ap:%d\n", to_sta_key(*sta_mac, sta_key), ap_index);
    sta = (sta_data_t *)hash_map_get(sta_map, to_sta_key(*sta_mac, sta_key));
    if (sta == NULL) {
        sta = (sta_data_t *)malloc(sizeof(sta_data_t));
        memset(sta, 0, sizeof(sta_data_t));
        memcpy(sta->sta_mac, *sta_mac, sizeof(mac_addr_t));
        hash_map_put(sta_map, strdup(to_sta_key(sta->sta_mac, sta_key)), sta);
    }
    
    sta->total_disconnected_time += sta->disconnected_time;
    sta->disconnected_time = 0;
    
    gettimeofday(&tv_now, NULL);
    if ((tv_now.tv_sec - sta->last_connected_time.tv_sec) <= g_monitor_module.rapid_reconnect_threshold) {
        sta->rapid_reconnects++;
    }
    
    sta->last_connected_time.tv_sec = tv_now.tv_sec;
    sta->last_connected_time.tv_usec = tv_now.tv_usec;

}

void process_disconnect	(unsigned int ap_index, mac_addr_t *sta_mac)
{
    sta_key_t sta_key;
    sta_data_t *sta;
    hash_map_t     *sta_map;
    
    sta_map = g_monitor_module.sta_map[ap_index];
    
    wifi_dbg_print(1, "Device:%s disconnected on ap:%d\n", to_sta_key(*sta_mac, sta_key), ap_index);
    sta = (sta_data_t *)hash_map_get(sta_map, to_sta_key(*sta_mac, sta_key));
    if (sta == NULL) {
        assert(0);
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
						process_connect(queue_data->ap_index, &queue_data->u.sta);
						break;

					case monitor_event_type_disconnect:
						process_disconnect(queue_data->ap_index, &queue_data->u.sta);
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
                upload_monitor_data();
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

void associated_devices_diagnostics	()
{
	wifi_associated_dev_t *dev_array = NULL;
	unsigned int i, num_devs = 0;
      
	for (i = 0; i < MAX_AP; i++) { 
		wifi_getApAssociatedDeviceDiagnosticResult(i, &dev_array, &num_devs);
        process_diagnostics(i, dev_array, num_devs);
        
        if (num_devs > 0) {
            free(dev_array);
        }
	}

}

void device_associated_states   (char *buff, ssize_t size)
{
    unsigned int ap_index, reason, vap, mac[6];
    mac_addr_t sta_mac;
    wifi_monitor_data_t *data;
    
    data = (wifi_monitor_data_t *)malloc(sizeof(wifi_monitor_data_t));
    data->id = msg_id++;
    
    if (strstr(buff, "JOIN") != NULL) {
        sscanf(buff, "RDKB_WIFI_NOTIFY: ssid%d JOIN: %d %02x:%02x:%02x:%02x:%02x:%02x %d",
               &ap_index, &vap,
               (unsigned int *)&mac[0], (unsigned int *)&mac[1], (unsigned int *)&mac[2],
               (unsigned int *)&mac[3], (unsigned int *)&mac[4], (unsigned int *)&mac[5],
               &reason);
        sta_mac[0] = mac[0]; sta_mac[1] = mac[1]; sta_mac[2] = mac[2];
        sta_mac[3] = mac[3]; sta_mac[4] = mac[4]; sta_mac[5] = mac[5];
        data->event_type = monitor_event_type_connect;
    } else {
        sscanf(buff, "RDKB_WIFI_NOTIFY: ssid%d LEAVE: %d %02x:%02x:%02x:%02x:%02x:%02x %d",
               &ap_index, &vap,
               (unsigned int *)&mac[0], (unsigned int *)&mac[1], (unsigned int *)&mac[2],
               (unsigned int *)&mac[3], (unsigned int *)&mac[4], (unsigned int *)&mac[5],
               &reason);
        sta_mac[0] = mac[0]; sta_mac[1] = mac[1]; sta_mac[2] = mac[2];
        sta_mac[3] = mac[3]; sta_mac[4] = mac[4]; sta_mac[5] = mac[5];
        data->event_type = monitor_event_type_disconnect;
    }
    
    data->ap_index = ap_index;
    memcpy(data->u.sta, sta_mac, sizeof(mac_addr_t));
    
    pthread_mutex_lock(&g_monitor_module.lock);
    queue_push(g_monitor_module.queue, data);
    
    pthread_cond_signal(&g_monitor_module.cond);
    pthread_mutex_unlock(&g_monitor_module.lock);
}

void *wifi_connections_listener    (void *arg)
{
    fd_set rfds;
    int retval;
    int fd;
    struct sockaddr_un name;
    char data[MAX_IPC_DATA_LEN];
    socklen_t size;
    ssize_t ret;
    
    unlink(KMSG_WRAPPER_FILE_NAME);
    
    fd = socket(PF_LOCAL, SOCK_DGRAM, 0);
    if (fd < 0) {
        wifi_dbg_print(1, "Error opening local socket err:%d\n", errno);
        return NULL;
    }
    
    /* Bind a name to the socket. */
    name.sun_family = AF_LOCAL;
    strncpy (name.sun_path, KMSG_WRAPPER_FILE_NAME, sizeof(name.sun_path));
    name.sun_path[sizeof (name.sun_path) - 1] = '\0';
    
    if (bind(fd, (struct sockaddr *)&name, sizeof(struct sockaddr_un)) < 0) {
        wifi_dbg_print(1, "Error binding to socket:%d\n", errno);
        return NULL;
    }
    
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
   
    
    while ((retval = select(fd + 1, &rfds, NULL, NULL, NULL)) >= 0) {
    
        if (retval) {
            // some sta joined or left
            size = sizeof(struct sockaddr_un);
            
            memset(data, 0, MAX_IPC_DATA_LEN);
            if (( ret = recvfrom(fd, data, MAX_IPC_DATA_LEN, 0, (struct sockaddr *)&name, &size)) > 0) {
                device_associated_states(data, ret);
            }
        
        } else if (retval == 0) {
            // time out, should never happen
        }
        
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        
        
    }
    
    close(fd);
    
    return NULL;
}

int init_wifi_monitor ()
{
	unsigned int i;
	pthread_t 	listener_id;
	int	rssi;
 
	g_monitor_module.poll_period = 1;
    g_monitor_module.upload_period = get_upload_period();
    g_monitor_module.current_poll_iter = 0;

	if (CosaDmlWiFi_GetGoodRssiThresholdValue(&rssi) != ANSC_STATUS_SUCCESS) {
		g_monitor_module.sta_health_rssi_threshold = -65;
	} else {
		g_monitor_module.sta_health_rssi_threshold = rssi;
	}

    gettimeofday(&g_monitor_module.last_signalled_time, NULL);
    gettimeofday(&g_monitor_module.last_polled_time, NULL);
	pthread_cond_init(&g_monitor_module.cond, NULL);
    pthread_mutex_init(&g_monitor_module.lock, NULL);

	for (i = 0; i < MAX_AP; i++) {
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
    
	if (pthread_create(&listener_id, NULL, wifi_connections_listener, &g_monitor_module) != 0) {
		deinit_wifi_monitor();
		wifi_dbg_print(1, "monitor thread create error\n");
		return -1;
	}
    
	return 0;
}

void deinit_wifi_monitor	()
{
	unsigned int i;

	queue_destroy(g_monitor_module.queue);
	for (i = 0; i < MAX_AP; i++) {
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

#ifndef DEBUG
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
