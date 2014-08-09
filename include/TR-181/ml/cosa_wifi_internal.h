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



/* Collection */
typedef  struct
_COSA_DML_WIFI_RADIO
{
    COSA_DML_WIFI_RADIO_FULL        Radio;
    COSA_DML_WIFI_RADIO_STATS       Stats;
    BOOLEAN                         bRadioChanged;
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
    ULONG                           AssocDeviceCount;
}
COSA_DML_WIFI_AP, *PCOSA_DML_WIFI_AP;

#define  COSA_DATAMODEL_WIFI_CLASS_CONTENT                                                  \
    /* duplication of the base object class content */                                      \
    COSA_BASE_CONTENT                                                                       \
    /* start of WIFI object class content */                                                \
    PCOSA_DML_WIFI_RADIO            pRadio;                                                 \
    ULONG                           RadioCount;                                             \
    ULONG                           ext_count;                                              \
    ULONG                           ext_update_time;                                        \
    ULONG                           ulSsidNextInstance;                                     \
    QUEUE_HEADER                    SsidQueue;                                              \
    ULONG                           ulAPNextInstance;                                       \
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

typedef  struct
_COSA_DATAMODEL_WIFI                                               
{
	COSA_DATAMODEL_WIFI_CLASS_CONTENT
}
COSA_DATAMODEL_WIFI,  *PCOSA_DATAMODEL_WIFI;

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

ANSC_STATUS
CosaWifiReInitialize
    (
        ANSC_HANDLE                 hThisObject,
        ULONG                       uIndex
    );

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

#endif 
