 /*****************************************************************************
  If not stated otherwise in this file or this component's Licenses.txt     
  file the following copyright and licenses apply:                          
                                                                            
  Copyright 2020 RDK Management                                             
                                                                            
  Licensed under the Apache License, Version 2.0 (the "License");           
  you may not use this file except in compliance with the License.          
  You may obtain a copy of the License at                                   
                                                                            
      http://www.apache.org/licenses/LICENSE-2.0                            
                                                                            
  Unless required by applicable law or agreed to in writing, software       
  distributed under the License is distributed on an "AS IS" BASIS,         
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  
  See the License for the specific language governing permissions and       
  limitations under the License.                                            
                                                                            
 *****************************************************************************/

#ifndef _WIFI_PASSPOINT_H
#define _WIFI_PASSPOINT_H

#define WIFI_PASSPOINT_DIR                  "/nvram/passpoint"
#define WIFI_PASSPOINT_GAS_CFG_FILE        "/nvram/passpoint/passpointGasCfg.json"
#define WIFI_PASSPOINT_DEFAULT_GAS_CFG     "{\"gasConfig\": [{ \"advertId\": 0, \"pauseForServerResp\": true, \"respTimeout\": 5000, \"comebackDelay\": 1000, \"respBufferTime\": 1000, \"queryRespLengthLimit\": 127 }]}"
#define WIFI_PASSPOINT_ANQP_CFG_FILE        "/nvram/passpoint/passpointAnqpCfg.json"
#define WIFI_PASSPOINT_DEFAULT_ANQP_CFG     "{\"InterworkingService\": {}}"
#define WIFI_PASSPOINT_HS2_CFG_FILE        "/nvram/passpoint/passpointHs2Cfg.json"
#define WIFI_PASSPOINT_DEFAULT_HS2_CFG     "{\"Passpoint\": {}}"

#define WIFI_INTERWORKING_CFG_FILE        "/nvram/passpoint/InterworkingCfg_%d.json"

#include "collection.h"
#include "cosa_wifi_internal.h"

typedef struct {
    UCHAR apIndex;
    mac_address_t sta;
    unsigned char token;
    wifi_anqp_node_t *head;
} cosa_wifi_anqp_context_t;

typedef struct
{
    USHORT info_id;
    USHORT len;
    UCHAR  oi[3];
    UCHAR  wfa_type;
} __attribute__((packed)) wifi_vendor_specific_anqp_capabilities_t;

typedef struct {
    int    capabilityInfoLength;
    wifi_capabilityListANQP_t  capabilityInfo;//wifi_HS2_CapabilityList_t
    UCHAR  venueCount;
    int    venueInfoLength;
    UCHAR  *venueInfo;//wifi_venueName_t
    UCHAR  *ipAddressInfo;//wifi_ipAddressAvailabality_t
    UCHAR  realmCount;
    int    realmInfoLength;
    UCHAR  *realmInfo;//wifi_naiRealm_t
    int    gppInfoLength;
    UCHAR  *gppInfo;//wifi_3gppCellularNetwork_t
    int    roamInfoLength;
    UCHAR  *roamInfo;//wifi_roamingConsoritum_t
    int    domainInfoLength;
    UCHAR  *domainNameInfo;//wifi_domainName_t
} cosa_wifi_anqp_data_t;

typedef struct {
    BOOL   hs2Status;
    BOOL   gafDisable;
    BOOL   p2pDisable;
    int    capabilityInfoLength;
    wifi_HS2_CapabilityList_t  capabilityInfo;//wifi_HS2_CapabilityList_t
    int    opFriendlyNameInfoLength;
    UCHAR  *opFriendlyNameInfo;//wifi_HS2_OperatorFriendlyName_t
    int    connCapabilityLength;
    UCHAR  *connCapabilityInfo;//wifi_HS2_ConnectionCapability_t
    int    realmInfoLength;
    UCHAR  *realmInfo;//wifi_HS2_NAI_Home_Realm_Query_t
    wifi_HS2_WANMetrics_t wanMetricsInfo;
    UCHAR  passpointStats[1024];
    UINT   domainRespCount;
    UINT   realmRespCount;
    UINT   gppRespCount;
    UINT   domainFailedCount;
    UINT   realmFailedCount;
    UINT   gppFailedCount;
} cosa_wifi_hs2_data_t;

void process_passpoint_timeout();
void wifi_anqpStartReceivingTestFrame();
void process_passpoint_event(cosa_wifi_anqp_context_t *anqpReq);
ANSC_STATUS CosaDmlWiFi_RestoreAPInterworking (int apIndex);
ANSC_STATUS CosaDmlWiFi_ApplyInterworkingElement(PCOSA_DML_WIFI_AP_CFG pCfg);
ANSC_STATUS CosaDmlWiFi_initPasspoint(void);
int enablePassPointSettings(int ap_index, BOOL passpoint_enable, BOOL downstream_disable, BOOL p2p_disable, BOOL layer2TIF);
ANSC_STATUS CosaDmlWiFi_InitANQPConfig(PCOSA_DML_WIFI_AP_CFG pCfg);
ANSC_STATUS CosaDmlWiFi_InitHS2Config(PCOSA_DML_WIFI_AP_CFG pCfg);
void CosaDmlWiFi_UpdateANQPVenueInfo(PCOSA_DML_WIFI_AP_CFG pCfg);
ANSC_STATUS CosaDmlWiFi_SetGasConfig(PANSC_HANDLE phContext, char *JSON_STR);
ANSC_STATUS CosaDmlWiFi_SaveGasCfg(char *buffer, int len);
ANSC_STATUS CosaDmlWiFi_InitGasConfig(PANSC_HANDLE phContext);
ANSC_STATUS CosaDmlWiFi_SetHS2Status(PCOSA_DML_WIFI_AP_CFG pCfg, BOOL bValue, BOOL setToPSM);
ANSC_STATUS CosaDmlWiFi_GetGasStats(PANSC_HANDLE phContext);
ANSC_STATUS CosaDmlWiFi_SetANQPConfig(PCOSA_DML_WIFI_AP_CFG pCfg, char *JSON_STR);
ANSC_STATUS CosaDmlWiFi_SaveANQPCfg(PCOSA_DML_WIFI_AP_CFG pCfg, char *buffer, int len);
ANSC_STATUS CosaDmlWiFi_GetWANMetrics(PCOSA_DML_WIFI_AP_CFG pCfg);
void CosaDmlWiFi_GetHS2Stats(PCOSA_DML_WIFI_AP_CFG pCfg);
ANSC_STATUS CosaDmlWiFi_SetHS2Config(PCOSA_DML_WIFI_AP_CFG pCfg, char *JSON_STR);
ANSC_STATUS CosaDmlWiFi_SaveHS2Cfg(PCOSA_DML_WIFI_AP_CFG pCfg, char *buffer, int len);
#if defined (DUAL_CORE_XB3)
int wifi_restoreAPInterworkingElement(int apIndex);
#endif
ANSC_STATUS CosaDmlWiFi_DefaultInterworkingConfig(PCOSA_DML_WIFI_AP_CFG pCfg);
ANSC_STATUS CosaDmlWiFi_WriteInterworkingConfig (PCOSA_DML_WIFI_AP_CFG pCfg);
ANSC_STATUS CosaDmlWiFi_InitInterworkingElement (PCOSA_DML_WIFI_AP_CFG pCfg);
void wifi_passpoint_dbg_print(char *format, ...);

typedef struct {
    PCOSA_DATAMODEL_WIFI    wifi_dml;
} wifi_passpoint_t;
        
#endif //_WIFI_PASSPOINT_H
