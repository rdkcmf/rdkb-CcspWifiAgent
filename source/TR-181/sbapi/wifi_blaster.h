/************************************************************************************
  If not stated otherwise in this file or this component's LICENSE file the
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
#ifndef       _WIFI_BLAST_H_
#define       _WIFI_BLAST_H_

#include <pthread.h>

#define OPER_BUFFER_LEN 64
#define BUFF_LEN_MIN OPER_BUFFER_LEN
#define BUFF_LEN_MAX 1024
#define PKTGEN_CNTRL_FILE                               "/proc/net/pktgen/pgctrl"
#define PKTGEN_THREAD_FILE_0                    "/proc/net/pktgen/kpktgend_0"
#define PKTGEN_DEVICE_FILE_ATH0                 "/proc/net/pktgen/ath0"
#define PKTGEN_DEVICE_FILE_ATH1                 "/proc/net/pktgen/ath1"
#define PKTGEN_DEVICE_FILE                              "/proc/net/pktgen/"
#define PKTGEN_SAMPLE_SIZE                              30
#define MAX_NUMBER_OF_CLIENTS                   10      /* Define it to 10 for now. Shall be updated based on the capacity of pktgen & device */
#define MAC_ADDRESS_STR_LEN                             17
#define PKTGEN_START_RUN_TIME_IN_MS             1000            /* 1 secs */
#define PKTGEN_WAIT_IN_MS                               30                      /* 30 ms */
#define PKTGEN_HEADER_SIZE_TO_DISCOUNT  24

#define MAX_RADIO_INDEX                                 2
#define RADIO0_INTERFACE_NAME                   "ath0"          /* 2.4 Ghz */
#define RADIO1_INTERFACE_NAME                   "ath1"          /* 5 Ghz */
#define MAX_STEP_COUNT  32 /*Active Measurement Step Count */
#define CONVERT_MILLI_TO_NANO           1000000
#define BITS_TO_MEGABITS                        1000000

#define CHCOUNT2                                        11
#define CHCOUNT5                                        25

#ifdef ENABLE_DEBUG_PRINTS
#define DEBUG_PRINT(x)                                  printf x
#else
#define DEBUG_PRINT(x)
#endif
#define CRITICAL_PRINT

#define PLAN_ID_LENGTH     16

typedef struct _pktGenConfig {
        unsigned int    packetSize;
        unsigned int    packetCount;
        unsigned int    sendDuration;
        int             clientIndex;
        char            wlanInterface[BUFF_LEN_MIN];
} pktGenConfig;

typedef struct _pktGenFrameCountSamples {
        ULONG   PacketsSentAck;
        ULONG   PacketsSentTotal;
        int             WaitAndLatencyInMs;             //Wait duration + API Latency in millsecs
} pktGenFrameCountSamples;

typedef struct {
    mac_address_t SrcMac;
    mac_address_t DestMac;
    unsigned int StepId;
    int ApIndex;
} active_msmt_step;

typedef struct {
    bool                ActiveMsmtEnable;
    unsigned int        ActiveMsmtSampleDuration;
    unsigned int        ActiveMsmtPktSize;
    unsigned int        ActiveMsmtNumberOfSamples;
    unsigned char       PlanId[PLAN_ID_LENGTH];
    unsigned char       StepInstance[MAX_STEP_COUNT];
    active_msmt_step    Step[MAX_STEP_COUNT];
} active_msmt_t;

typedef struct {
unsigned int   MaxTxRate;
unsigned int   MaxRxRate;
int            rssi;
unsigned long  TxPhyRate;
unsigned long  RxPhyRate;
int            SNR;
int            ReTransmission;
double         throughput;
char           Operating_standard[OPER_BUFFER_LEN + 1];
char           Operating_channelwidth[OPER_BUFFER_LEN + 1];
} active_msmt_data_t;


typedef struct {
    pthread_mutex_t     lock;
    active_msmt_t       active_msmt;
    active_msmt_step    curStepData;
    active_msmt_data_t  *active_msmt_data;
} wifi_actvie_msmt_t;
/* prototype for Active Measurement */

/* Active Measurement GET calls */
bool monitor_is_active_msmt_enabled();
unsigned int GetActiveMsmtPktSize();
unsigned int GetActiveMsmtSampleDuration();
unsigned int GetActiveMsmtNumberOfSamples();

/* Active Measurement SET calls */
void SetActiveMsmtEnable(bool enable);
void SetActiveMsmtPktSize(unsigned int PktSize);
void SetActiveMsmtSampleDuration(unsigned int Duration);
void SetActiveMsmtNumberOfSamples(unsigned int NoOfSamples);

/* Active Measurement Step & Plan SET calls */
void SetActiveMsmtStepDstMac(char *DstMac, ULONG StepIns);
void SetActiveMsmtStepSrcMac(char *SrcMac, ULONG StepIns);
void SetActiveMsmtStepID(unsigned int StepId, ULONG StepIns);
void SetActiveMsmtPlanID(char *pPlanID);

/* Active Measurement Step & Plan GET calls */
unsigned int GetActiveMsmtStepID();
void GetActiveMsmtPlanID(unsigned int *pPlanID);
void GetActiveMsmtStepSrcMac(mac_address_t pStepSrcMac);
void GetActiveMsmtStepDestMac(mac_address_t pStepDstMac);

/* Function prototype for wifiblaster */
void *startWifiBlast(void *vargp);
int StopWifiBlast(void);
//int executeCommand(char* command,char* result);
unsigned long getCurrentTimeInMicroSeconds();
int isVapEnabled (int wlanIndex);
int WaitForDuration (int timeInMs);
void pktGen_BlastClient ();
void *WiFiBlastClient(void* data);
void process_active_msmt_diagnostics (int ap_index);

void stream_client_msmt_data(bool ActiveMsmtFlag);
#endif /* _WIFI_BLAST_H_ */
