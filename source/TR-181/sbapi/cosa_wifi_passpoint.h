#ifndef _WIFI_PASSPOINT_H
#define _WIFI_PASSPOINT_H

#define WIFI_PASSPOINT_DIR                  "/nvram/passpoint"
#define WIFI_PASSPOINT_GAS_CFG_FILE        "/nvram/passpoint/passpointGasCfg.json"
#define WIFI_PASSPOINT_DEFAULT_GAS_CFG     "{\"gasConfig\": [{ \"advertId\": 0, \"pauseForServerResp\": true, \"respTimeout\": 5000, \"comebackDelay\": 1000, \"respBufferTime\": 1000, \"queryRespLengthLimit\": 127 }]}"

#include "collection.h"
typedef struct {
    queue_t              *queue;
    pthread_cond_t       cond;
    pthread_mutex_t      lock;
    pthread_t            tid;
} wifi_passpoint_t;

#endif //_WIFI_PASSPOINT_H
