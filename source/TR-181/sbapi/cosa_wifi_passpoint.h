 /*****************************************************************************
  If not stated otherwise in this file or this component's LICENSE     
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
#define WIFI_PASSPOINT_DEFAULT_GAS_CFG     "{\"GASConfig\": [{ \"AdvertisementId\": 0, \"PauseForServerResp\": true, \"RespTimeout\": 5000, \"ComebackDelay\": 1000, \"RespBufferTime\": 1000, \"QueryRespLengthLimit\": 127 }]}"
#define WIFI_PASSPOINT_ANQP_CFG_FILE        "/nvram/passpoint/passpointAnqpCfg.json"
#define WIFI_PASSPOINT_DEFAULT_ANQP_CFG     "{\"ANQP\": {}}"
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
ANSC_STATUS CosaDmlWiFi_InitGasConfig(PANSC_HANDLE phContext);
ANSC_STATUS CosaDmlWiFi_SetHS2Status(PCOSA_DML_WIFI_AP_CFG pCfg, BOOL bValue, BOOL setToPSM);
ANSC_STATUS CosaDmlWiFi_GetGasStats(PANSC_HANDLE phContext);
ANSC_STATUS CosaDmlWiFi_SetANQPConfig(PCOSA_DML_WIFI_AP_CFG pCfg, char *JSON_STR);
ANSC_STATUS CosaDmlWiFi_SaveANQPCfg(PCOSA_DML_WIFI_AP_CFG pCfg);
ANSC_STATUS CosaDmlWiFi_GetWANMetrics(PCOSA_DML_WIFI_AP_CFG pCfg);
void CosaDmlWiFi_GetHS2Stats(PCOSA_DML_WIFI_AP_CFG pCfg);
ANSC_STATUS CosaDmlWiFi_SetHS2Config(PCOSA_DML_WIFI_AP_CFG pCfg, char *JSON_STR);
ANSC_STATUS CosaDmlWiFi_SaveHS2Cfg(PCOSA_DML_WIFI_AP_CFG pCfg);
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
