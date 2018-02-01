#ifndef _WIFI_TESTS_H
#define _WIFI_TESTS_H

#define     SIM_NUMBER          5
#define     SIM_LOOP_TIMEOUT    5
#define     SIM_RSSI_HIGH       -35
#define     SIM_RSSI_LOW        -100
#define     SIM_RSSI_CONNECTED_THRESH   -75

typedef struct {
    mac_addr_t  sta_mac;
    bool        connected;
    rssi_t      rssi;
    bool        update;
} simulator_state_t;

typedef struct {

    simulator_state_t       simulator[SIM_NUMBER];
    pthread_t        sim_id;
    
    pthread_cond_t      cond;
    pthread_mutex_t     lock;
    bool            started;
} wifi_tests_simulator_t;


#endif // _WIFI_TESTS_H
