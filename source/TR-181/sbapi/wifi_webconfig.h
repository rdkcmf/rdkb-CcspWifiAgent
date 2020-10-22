 /****************************************************************************
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
                                                                            
 ****************************************************************************/

#ifndef _WIFI_WEBCONF_H_
#define _WIFI_WEBCONF_H_

#define WIFI_WEBCONFIG_PRIVATESSID 1
#define WIFI_WEBCONFIG_HOMESSID    2

typedef struct
{
    char  ssid_name[64];
    bool  enable;       
    bool  ssid_advertisement_enabled;
    bool  ssid_changed;
} webconf_ssid_t;

typedef struct
{
    char   passphrase[64];
    char   encryption_method[16];       
    char   mode_enabled[32];
    bool   sec_changed;
} webconf_security_t;

typedef struct
{
    webconf_ssid_t ssid_2g;
    webconf_security_t security_2g; 
    webconf_ssid_t ssid_5g;
    webconf_security_t security_5g;
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

int wifi_WebConfigSet(const void *buf, size_t len,uint8_t ssid);

#endif

