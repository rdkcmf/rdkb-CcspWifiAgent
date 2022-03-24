 /****************************************************************************
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
                                                                            
 ****************************************************************************/

#ifndef _WIFI_WEBCONF_H_
#define _WIFI_WEBCONF_H_

#include "webconfig_framework.h"

#define WIFI_WEBCONFIG_PRIVATESSID 1
#define WIFI_WEBCONFIG_HOMESSID    2
#define WIFI_SSID_CONFIG           3
#ifdef FEATURE_RADIO_WEBCONFIG
#define WIFI_RADIO_CONFIG          4
#endif
#define MIN_PWD_LEN                8
#define MAX_PWD_LEN                63
#define SSID_NAME_MAX_LEN          32
#define VAP_NAME_MAX_LEN           16
#define ENC_METHOD_MAX_LEN         16
#define SEC_MODE_MAX_LEN           32

#ifdef WIFI_HAL_VERSION_3
#define MAX_VAP_PER_RADIO         MAX_NUM_VAP_PER_RADIO
#define MAX_VAP_COUNT             MAX_VAP
#else
#define MAX_VAP_PER_RADIO          8
#define MAX_VAP_COUNT              16
#endif

typedef struct
{
    char  ssid_name[64];
    bool  enable;       
    bool  ssid_advertisement_enabled;
    bool  ssid_changed;
#ifdef WIFI_HAL_VERSION_3
    bool  configured;
#endif
} webconf_ssid_t;

typedef struct
{
    char   passphrase[64];
    char   encryption_method[16];       
    char   mode_enabled[32];
    bool   sec_changed;
#ifdef WIFI_HAL_VERSION_3
    bool   configured;
#endif
} webconf_security_t;

typedef struct
{
#ifdef WIFI_HAL_VERSION_3
    webconf_ssid_t ssid[MAX_NUM_RADIOS];
    webconf_security_t security[MAX_NUM_RADIOS]; 
#else
    webconf_ssid_t ssid_2g;
    webconf_security_t security_2g; 
    webconf_ssid_t ssid_5g;
    webconf_security_t security_5g;
#endif
    char    subdoc_name[32];
    uint64_t  version;
    uint16_t   transaction_id;
} webconf_wifi_t;

typedef struct
{
    bool hostapd_restart;
    bool init_radio;
    bool sec_changed;
} webconf_apply_t;

typedef struct
{
    void     *data;
    char      subdoc_name[32];
    uint64_t  version;
    uint16_t  transaction_id;
    unsigned  long msg_size;
} wifi_vap_blob_data_t;

#ifdef FEATURE_RADIO_WEBCONFIG
typedef struct
{
    void     *data;
    char      subdoc_name[32];
    uint64_t  version;
    uint16_t  transaction_id;
    unsigned  long msg_size;
} wifi_radio_blob_data_t;
#endif

int wifi_WebConfigSet(const void *buf, size_t len,uint8_t ssid);
int wifi_vapConfigSet(const char *buf, size_t len, pErr execRetVal);
int wifi_vapBlobSet(void *buf);
#ifdef FEATURE_RADIO_WEBCONFIG
int wifi_radioConfigSet(const char *buf, size_t len, pErr execRetVal);
int wifi_radioBlobSet(void *buf);
#endif
#ifdef WIFI_HAL_VERSION_3
ANSC_STATUS  updateDMLConfigPerRadio(UINT radioIndex);
ANSC_STATUS  notifyMeshEvents(wifi_vap_info_t *vap_cfg);
#endif

int init_web_config();
#ifdef CISCO_XB3_PLATFORM_CHANGES
    INT wifi_ifConfigDown(INT apIndex);
#endif
#endif

