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
#include "wifi_tests.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <assert.h>

unsigned char           sim_mac_base = 0x55;
wifi_tests_simulator_t   g_simulator = {0};

void *simulate_connect_disconnect(void *data);

int wifi_getApAssociatedDeviceDiagnosticResult(int apIndex, wifi_associated_dev_t **associated_dev_array, unsigned int *output_array_size)
{
    unsigned int i, total = 0;
	wifi_associated_dev_t *assoc;
    simulator_state_t   *sim;

    if (g_simulator.started == false) {
        *output_array_size = 0;
        *associated_dev_array = NULL;
        return RETURN_ERR;
    }
    
    if (apIndex == 1) {
        *output_array_size = 0;
        *associated_dev_array = NULL;
        return RETURN_ERR;
    }

	*associated_dev_array = (wifi_associated_dev_t *)malloc(SIM_NUMBER*sizeof(wifi_associated_dev_t));

	assoc = *associated_dev_array;
    
    pthread_mutex_lock(&g_simulator.lock);
    total = 0;
	for (i = 0; i < SIM_NUMBER; i++) {
        sim = &g_simulator.simulator[i];
        if (sim->connected == true) {
            memcpy(assoc->cli_MACAddress, sim->sta_mac, sizeof(mac_addr_t));
            assoc->cli_RSSI = sim->rssi;
            assoc++;
            total++;
        }
	}
    *output_array_size = total;
    
    pthread_mutex_unlock(&g_simulator.lock);
    
    return RETURN_OK;
}

int start_simulator    ()
{
    unsigned int i;
    simulator_state_t   *sim;
    
    pthread_cond_init(&g_simulator.cond, NULL);
    pthread_mutex_init(&g_simulator.lock, NULL);
    
    for (i = 0; i < SIM_NUMBER; i++) {
        sim = &g_simulator.simulator[i];
        sim->sta_mac[0] = 0x00; sim->sta_mac[1] = 0x11; sim->sta_mac[2] = 0x22;
        sim->sta_mac[3] = 0x33; sim->sta_mac[4] = 0x44; sim->sta_mac[5] = sim_mac_base++;
        sim->rssi = SIM_RSSI_HIGH;
        sim->connected = false;
        
        if (i == 0) {
            sim->update = true;
        } else {
            sim->update = false;
        }
    }
    
    
    if (pthread_create(&g_simulator.sim_id, NULL, simulate_connect_disconnect, &g_simulator) != 0) {
        wifi_dbg_print(1, "Error creating run thread\n");
        return -1;
    }
    
    g_simulator.started = true;
    return 0;
}

bool get_simulator_buffer   (simulator_state_t   *sim, char *buffer)
{
    bool send = false;
    
    sim->rssi -= 5;
    
    if ((sim->rssi < SIM_RSSI_CONNECTED_THRESH) && (sim->connected == true)) {
        // disconnect
        sprintf(buffer, "RDKB_WIFI_NOTIFY: ssid0 LEAVE: 0 %02x:%02x:%02x:%02x:%02x:%02x 0",
                sim->sta_mac[0], sim->sta_mac[1], sim->sta_mac[2],
                sim->sta_mac[3], sim->sta_mac[4], sim->sta_mac[5]);
        sim->connected = false;
        send = true;
    } else if ((sim->rssi > SIM_RSSI_CONNECTED_THRESH) && (sim->connected == false)) {
        sprintf(buffer, "RDKB_WIFI_NOTIFY: ssid0 JOIN: 0 %02x:%02x:%02x:%02x:%02x:%02x 1",
                sim->sta_mac[0], sim->sta_mac[1], sim->sta_mac[2],
                sim->sta_mac[3], sim->sta_mac[4], sim->sta_mac[5]);
        sim->connected = true;
        send = true;
    }
    
    if (sim->rssi <= SIM_RSSI_LOW) {
        sim->rssi = SIM_RSSI_HIGH;
    }
    
    return send;
}

simulator_state_t *get_simulator_to_update()
{
    unsigned int i;
    simulator_state_t   *sim = NULL;
    
    for (i = 0; i < SIM_NUMBER; i++) {
        sim = &g_simulator.simulator[i];
        if (sim->update == true) {
            break;
        }
    }
    
    if (i == (SIM_NUMBER - 1)) {
        i = 0;
    } else {
        i++;
    }
    
    sim->update = false;
    g_simulator.simulator[i].update = true;
    
    return sim;
}

void *simulate_connect_disconnect(void *data)
{
    int fd;
    bool ret;
    struct sockaddr_un name;
    struct timespec time_to_wait;
    struct timeval tv;
    simulator_state_t   *state;
    wifi_tests_simulator_t *sim = (wifi_tests_simulator_t *)data;
    char buffer[256];
    
    fd = socket(PF_LOCAL, SOCK_DGRAM, 0);
    if (fd < 0) {
        return NULL;
    }
    
    /* Bind a name to the socket. */
    name.sun_family = AF_LOCAL;
    strncpy (name.sun_path, KMSG_WRAPPER_FILE_NAME, sizeof(name.sun_path));
    name.sun_path[sizeof (name.sun_path) - 1] = '\0';
    
    while (1) {
        gettimeofday(&tv, NULL);
        time_to_wait.tv_nsec = 0;
        time_to_wait.tv_sec = tv.tv_sec + SIM_LOOP_TIMEOUT;
        pthread_mutex_lock(&sim->lock);
        pthread_cond_timedwait(&sim->cond, &sim->lock, &time_to_wait);
        state = get_simulator_to_update();
        ret = get_simulator_buffer(state, buffer);
        pthread_mutex_unlock(&sim->lock);
        if (ret == true) {
            sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr *)&name, sizeof(struct sockaddr_un));
        }
        
    }
    
    close(fd);
    
    return NULL;
}



