#ifndef	_WIFI_MON_H_
#define	_WIFI_MON_H_

#define MAX_ASSOCIATED_WIFI_DEVS    64
#define MAX_AP  2
#define MAC_ADDR_LEN	6
#define STA_KEY_LEN		2*MAC_ADDR_LEN + 6
#define MAX_IPC_DATA_LEN    1024
#define KMSG_WRAPPER_FILE_NAME  "/tmp/goodbad-rssi"

typedef unsigned char   mac_addr_t[MAC_ADDR_LEN];
typedef signed short    rssi_t;
typedef char			sta_key_t[STA_KEY_LEN];

#ifdef DEBUG
#define wifi_dbg_print(level, fmt, args...) do {printf("[WIFI-MONITOR] %s(%d) "fmt, __FUNCTION__, __LINE__, ##args);} while(0)
#else
void wifi_dbg_print(int level, char *format, ...);
#endif

typedef enum {
    monitor_event_type_diagnostics,
    monitor_event_type_connect,
    monitor_event_type_disconnect,
    monitor_event_type_max
} wifi_monitor_event_type_t;

typedef struct {
    unsigned int num;
    wifi_associated_dev_t   devs[MAX_ASSOCIATED_WIFI_DEVS];
} associated_devs_t;

typedef struct {
    unsigned int id;
    wifi_monitor_event_type_t   event_type;
    unsigned int    ap_index;
    union {
        mac_addr_t  sta;
        associated_devs_t   devs;
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
    queue_t             *queue;
    hash_map_t          *sta_map[MAX_AP];
    pthread_cond_t      cond;
    pthread_mutex_t     lock;
    pthread_t           id;
    bool                exit_monitor;
	unsigned int		poll_period;
    unsigned int        upload_period;
    unsigned int        current_poll_iter;
    struct timeval      last_signalled_time;
    struct timeval      last_polled_time;
	rssi_t				sta_health_rssi_threshold;
    unsigned int        rapid_reconnect_threshold;
} wifi_monitor_t;

#endif	//_WIFI_MON_H_
