#ifndef _WIFI_EASY_CONNECT_H
#define _WIFI_EASY_CONNECT_H

typedef struct {
    unsigned int    num;
    unsigned int    channels[8];
} wifi_easy_connect_best_enrollee_channels_t;

typedef struct {
    queue_t              *queue;
    pthread_cond_t       cond;
    pthread_mutex_t      lock;
    pthread_t            tid;
    wifi_easy_connect_best_enrollee_channels_t    channels_on_ap[2];
} wifi_easy_connect_t;

#define str(x) #x
#define enum_str(x) str(x)

#endif //_WIFI_EASY_CONNECT_H
