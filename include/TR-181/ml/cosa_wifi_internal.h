/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2015 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/**********************************************************************
   Copyright [2014] [Cisco Systems, Inc.]
 
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at
 
       http://www.apache.org/licenses/LICENSE-2.0
 
   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
**********************************************************************/

#ifndef  _COSA_WIFI_INTERNAL_H
#define  _COSA_WIFI_INTERNAL_H
#include "cosa_wifi_apis.h"
#include "poam_irepfo_interface.h"
#include "sys_definitions.h"


#include <telemetry_busmessage_sender.h>

/*
#include "poam_cosa_wifi_dm_interface.h"
#include "poam_cosa_wifi_dm_exported_api.h"
#include "slap_cosa_wifi_dm_interface.h"
#include "slap_cosa_wifi_dm_exported_api.h"
*/

#define  WECB_EXT_REFRESH_INTERVAL                       180

#define  COSA_IREP_FOLDER_NAME_WIFI                      "WIFI"

#define  COSA_IREP_FOLDER_NAME_WIFI_SSID                 "SSID"
#define  COSA_DML_RR_NAME_WifiSsidNextInsNunmber         "SsidNextInstance"
#define  COSA_DML_RR_NAME_WifiSsidName                   "Name"
#define  COSA_DML_RR_NAME_WifiSsidInsNum                 "SsidInsNum"

#define  COSA_IREP_FOLDER_NAME_WIFI_AP                   "AccessPoint"
#define  COSA_DML_RR_NAME_WifiAPNextInsNunmber           "APNextInstance"
#define  COSA_DML_RR_NAME_WifiAPSSID                     "RelatedSSID"
#define  COSA_DML_RR_NAME_WifiAPSSIDReference            "SSIDReference"

#define  COSA_IREP_FOLDER_NAME_MAC_FILT_TAB             "MacFilterTable"
#define  COSA_DML_RR_NAME_MacFiltTabNextInsNunmber      "MacFilterTableNextInstance"
#define  COSA_DML_RR_NAME_MacFiltTab                    "MacFilterTable"
#define  COSA_DML_RR_NAME_MacFiltTabInsNum              "MacFilterTableInsNum"


/* Active Measurement macro's */
#define MIN_ACTIVE_MSMT_PKT_SIZE 64
#define MAX_ACTIVE_MSMT_PKT_SIZE 1470
#define MIN_ACTIVE_MSMT_SAMPLE_COUNT 1
#define MAX_ACTIVE_MSMT_SAMPLE_COUNT 100
#define MIN_ACTIVE_MSMT_SAMPLE_DURATION 1
#define MAX_ACTIVE_MSMT_SAMPLE_DURATION 10000

/* Collection */
typedef  struct
_COSA_DML_WIFI_RADIO
{
    COSA_DML_WIFI_RADIO_FULL          Radio;
    COSA_DML_WIFI_RADIO_STATS         Stats;
    COSA_DML_WIFI_RADIO_CHANNEL_STATS ChStats;
    BOOLEAN                           bRadioChanged;
}
COSA_DML_WIFI_RADIO, *PCOSA_DML_WIFI_RADIO;

/* Collection */
typedef  struct
_COSA_DML_WIFI_SSID
{
    COSA_DML_WIFI_SSID_FULL         SSID;
    COSA_DML_WIFI_SSID_STATS        Stats;
    BOOLEAN                         bSsidChanged;
}
COSA_DML_WIFI_SSID, *PCOSA_DML_WIFI_SSID;

/* Collection */
typedef  struct
_COSA_DML_WIFI_AP
{
    COSA_DML_WIFI_AP_FULL           AP;
    COSA_DML_WIFI_APSEC_FULL        SEC;
    COSA_DML_WIFI_APWPS_FULL        WPS;
    COSA_DML_WIFI_AP_MF_CFG         MF;
    BOOLEAN                         bApChanged;
    BOOLEAN                         bSecChanged;
    BOOLEAN                         bWpsChanged;
    BOOLEAN                         bMfChanged;
 
    PCOSA_DML_WIFI_AP_ASSOC_DEVICE  AssocDevices;
    //COSA_DML_WIFI_RadiusSetting     RadiusSetting; //zqiu: move to COSA_DML_WIFI_AP_FULL

    ULONG                           AssocDeviceCount;
    ULONG			    AssociatedDevice1PreviousVisitTime;
#if !defined(_HUB4_PRODUCT_REQ_) && !defined (_XB7_PRODUCT_REQ_)
    COSA_DML_WIFI_DPP_CFG      		DPP;
#endif
}
COSA_DML_WIFI_AP, *PCOSA_DML_WIFI_AP;

#ifdef USE_NOTIFY_COMPONENT
#define LM_GEN_STR_SIZE   	64 
#define LM_MAX_HOSTS_NUM	256

typedef struct {
    unsigned char ssid[LM_GEN_STR_SIZE];
    unsigned char AssociatedDevice[LM_GEN_STR_SIZE];
	unsigned char phyAddr[32]; /* Byte alignment*/
    int RSSI;
	int Status;
}__attribute__((packed, aligned(1))) LM_wifi_host_t;

typedef struct{
    int count;
    LM_wifi_host_t   host[LM_MAX_HOSTS_NUM];
}__attribute__((packed, aligned(1))) LM_wifi_hosts_t;
#endif
/* Collection */
typedef  struct
_COSA_DML_WIFI_BANDSTEERING
{
    COSA_DML_WIFI_BANDSTEERING_OPTION       BSOption;
    PCOSA_DML_WIFI_BANDSTEERING_SETTINGS    pBSSettings; // Static Settings per Radio
    INT                                     RadioCount;  // Static Settings per Radio						 
    BOOLEAN                                 bBSOptionChanged;
    BOOLEAN                                 bBSSettingsChanged;	
}
COSA_DML_WIFI_BANDSTEERING, *PCOSA_DML_WIFI_BANDSTEERING;

#define MIN_MAC_LEN     12

#define  COSA_DATAMODEL_WIFI_CLASS_CONTENT                                                  \
    /* duplication of the base object class content */                                      \
    COSA_BASE_CONTENT                                                                       \
    /* start of WIFI object class content */                                                \
    PCOSA_DATAMODEL_RDKB_WIFIREGION			pWiFiRegion;			    \
    PCOSA_DML_WIFI_RADIO            pRadio;                                                 \
    COSA_DML_NEIGHTBOURING_WIFI_DIAG_CFG     Diagnostics;				    \
    ULONG                           RadioCount;                                             \
    ULONG                           ext_count;                                              \
    ULONG                           ext_update_time;                                        \
    ULONG                           ulSsidNextInstance;                                     \
    QUEUE_HEADER                    SsidQueue;                                              \
    SLIST_HEADER                    ResultList;                                              \
    ULONG                           ulAPNextInstance;                                       \
    ULONG                           ulResultNextInstance;                                       \
    QUEUE_HEADER                    AccessPointQueue;                                       \
    ANSC_HANDLE                     hIrepFolderCOSA;                                        \
    ANSC_HANDLE                     hIrepFolderWifi;                                        \
    ANSC_HANDLE                     hIrepFolderWifiSsid;                                    \
    ANSC_HANDLE                     hIrepFolderWifiAP;                                      \
    BOOLEAN                         bTelnetEnabled;                                         \
    /*PPOAM_COSAWIFIDM_OBJECT*/ANSC_HANDLE         hPoamWiFiDm;                                            \
    /*PSLAP_COSAWIFIDM_OBJECT*/ANSC_HANDLE         hSlapWiFiDm;                                            \
    BOOLEAN                         bWiFiUpdated;                                           \
    BOOLEAN                         bSSIDUpdated;                                           \
	CHAR                            *disconnect_client;						                \
	CHAR                            *ext_status;                                            \
	PCOSA_DML_WIFI_BANDSTEERING		pBandSteering;					\
	PCOSA_DML_WIFI_HARVESTER		pHarvester;					\
	PCOSA_DML_WIFI_ATM				pATM;							\
	BOOLEAN                         bPreferPrivateEnabled;					\
	BOOLEAN                         bRapidReconnectIndicationEnabled;                  \
	INT                         	iX_RDKCENTRAL_COM_GoodRssiThreshold;					\
        INT                             iX_RDKCENTRAL_COM_AssocCountThreshold;                  \
        INT                             iX_RDKCENTRAL_COM_AssocMonitorDuration;                 \
        INT                             iX_RDKCENTRAL_COM_AssocGateTime;                        \
	BOOLEAN                         bX_RDKCENTRAL_COM_vAPStatsEnable;                  \
	BOOLEAN                         bFeatureMFPConfig;                  \
	BOOLEAN                         bTxOverflowSelfheal;                  \
	BOOLEAN                         bForceDisableWiFiRadio;	              \
	BOOLEAN				bEnableRadiusGreyList;		\
	BOOLEAN				bShowWiFiCredential;		\
        COSA_DML_WIFI_GASCFG            GASCfg[1];                  \
        COSA_DML_WIFI_GASSTATS          GASStats[1];                  \
        char                            *GASConfiguration;                  \
        BOOLEAN                         bEnableHostapdAuthenticator;          \
        BOOLEAN                         b2G80211axEnabled;          \
        BOOLEAN                         bDFS;          \
        BOOLEAN                         bDFSAtBootUp;          \
	
typedef  struct
_COSA_DATAMODEL_WIFI                                               
{
    COSA_DATAMODEL_WIFI_CLASS_CONTENT
#if defined (FEATURE_CSI)        
    ULONG                           ulCSINextInstance;
    QUEUE_HEADER                    CSIList;
#endif

}
COSA_DATAMODEL_WIFI,  *PCOSA_DATAMODEL_WIFI;

/*
*  This struct is for creating entry context link in writable table when call GetEntry()
*/
#define  COSA_CONTEXT_RSL_LINK_CLASS_CONTENT                                      \
        COSA_CONTEXT_LINK_CLASS_CONTENT                                            \
        ULONG                            InterfaceIndex;                           \
        ULONG                            Index;                                    \

typedef  struct
_COSA_CONTEXT_RSL_LINK_OBJECT
{
    COSA_CONTEXT_RSL_LINK_CLASS_CONTENT
}
COSA_CONTEXT_RSL_LINK_OBJECT,  *PCOSA_CONTEXT_RSL_LINK_OBJECT;

/*
    Standard function declaration 
*/
ANSC_HANDLE
CosaWifiCreate
    (
        VOID
    );

ANSC_STATUS
CosaWifiInitialize
    (
        ANSC_HANDLE                 hThisObject
    );

#ifdef WIFI_HAL_VERSION_3
ANSC_STATUS
CosaWifiReInitialize
    (
        ANSC_HANDLE                 hThisObject,
        ULONG                       uIndex,
        BOOL                        initNeeded
    );
#else
ANSC_STATUS
CosaWifiReInitialize
    (
        ANSC_HANDLE                 hThisObject,
        ULONG                       uIndex
    );
#endif

ANSC_STATUS
CosaWifiRemove
    (
        ANSC_HANDLE                 hThisObject
    );
    
ANSC_STATUS
CosaWifiRegGetSsidInfo
    (
        ANSC_HANDLE                 hThisObject
    );

ANSC_STATUS
CosaWifiRegAddSsidInfo
    (
        ANSC_HANDLE                 hThisObject,
        ANSC_HANDLE                 hCosaContext
    );
 
ANSC_STATUS
CosaWifiRegDelSsidInfo
    (
        ANSC_HANDLE                 hThisObject,
        ANSC_HANDLE                 hCosaContext
    );
    
ANSC_STATUS
CosaWifiRegGetAPInfo
    (
        ANSC_HANDLE                 hThisObject
    );
    
ANSC_STATUS
CosaWifiRegAddAPInfo
    (
        ANSC_HANDLE                 hThisObject,
        ANSC_HANDLE                 hCosaContext
    );
 
ANSC_STATUS
CosaWifiRegDelAPInfo
    (
        ANSC_HANDLE                 hThisObject,
        ANSC_HANDLE                 hCosaContext
    );

ANSC_STATUS
CosaDmlWiFiApMfGetMacList
    (
        UCHAR       *mac,
        CHAR        *maclist,
        ULONG       numList
    );


ANSC_STATUS
CosaDmlWiFiApMfSetMacList
    (
        CHAR        *maclist,
        UCHAR       *mac,
        ULONG       *numList
    );

ANSC_STATUS
CosaWifiRegGetMacFiltInfo
    (
        ANSC_HANDLE                 hThisObject
    );

ANSC_STATUS
CosaWifiRegAddMacFiltInfo
    (
        ANSC_HANDLE                 hThisObject,
        ANSC_HANDLE                 hCosaContext
    );
ANSC_STATUS
CosaWifiRegDelMacFiltInfo
    (
        ANSC_HANDLE                 hThisObject,
        ANSC_HANDLE                 hCosaContext
    );

/* Prototype for Active Measurement SET/GET calls */
BOOL
CosaDmlWiFi_IsActiveMeasurementEnable();

ANSC_STATUS
CosaDmlWiFi_ActiveMsmtNumberOfSamples
    (
        PCOSA_DML_WIFI_HARVESTER pHarvester
    );

ANSC_STATUS
CosaDmlWiFi_ActiveMsmtSampleDuration
    (
        PCOSA_DML_WIFI_HARVESTER pHarvester
    );

ANSC_STATUS
CosaDmlWiFi_ActiveMsmtPktSize
    (
        PCOSA_DML_WIFI_HARVESTER pHarvester
    );

ANSC_STATUS
CosaDmlWiFi_ActiveMsmtEnable
    (
       PCOSA_DML_WIFI_HARVESTER pHarvester
    );

/* Prototype for Activev Measurement Plan & Step SET/GET calls */
ANSC_STATUS
CosaDmlWiFiClient_SetActiveMsmtPlanId
    (
        PCOSA_DML_WIFI_HARVESTER pHarvester
    );

ANSC_STATUS
CosaDmlWiFiClient_ResetActiveMsmtStep
   (
       PCOSA_DML_WIFI_HARVESTER pHarvester
   );

ANSC_STATUS
CosaDmlWiFiClient_SetActiveMsmtStepId
    (
        UINT StepId,
        ULONG StepIns
    );

ANSC_STATUS
CosaDmlActiveMsmt_Step_SetSrcMac
    (
        char *SrcMac,
        ULONG StepIns
    );

ANSC_STATUS
CosaDmlActiveMsmt_Step_SetDestMac
    (
        char *DestMac,
        ULONG StepIns
    );

ANSC_STATUS
GetActiveMsmtStepInsNum
    (
        PCOSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG pStepCfg,
        ULONG *StepIns
    );

ANSC_STATUS
ValidateActiveMsmtPlanID
   (
       UCHAR *pPlanId
   );

#ifdef WIFI_HAL_VERSION_3
ANSC_STATUS
CosaWifiReInitializeRadioAndAp
    (
        ANSC_HANDLE hThisObject,
        CHAR *indexes
    );
#else
ANSC_STATUS
CosaWifiReInitializeRadioAndAp
    (
        ANSC_HANDLE hThisObject,
        ULONG indexes
    );
#endif

#endif 
