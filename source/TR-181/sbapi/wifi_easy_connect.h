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

#ifndef _WIFI_EASY_CONNECT_H
#define _WIFI_EASY_CONNECT_H

typedef struct {
    unsigned int    apIndex;
    mac_address_t   sta_mac;
    wifi_dpp_state_t state;
} wifi_easy_connect_event_match_criteria_t;

typedef struct {
    unsigned int    num;
    unsigned int    channels[32];
} wifi_easy_connect_best_enrollee_channels_t;

#define MAX_DPP_VAP	2

typedef struct {
	void 	*reconf_ctx;
	char	reconf_pub_key[512];
} wifi_easy_connect_reconfig_t;

typedef struct {
	void	*csign_inst;
	unsigned char sign_key_hash[512];
} wifi_easy_connect_csign_t;

typedef struct {
	PCOSA_DATAMODEL_WIFI	wifi_dml;
	wifi_easy_connect_reconfig_t	reconfig[MAX_DPP_VAP];
	wifi_easy_connect_csign_t		csign[MAX_DPP_VAP];
    wifi_easy_connect_best_enrollee_channels_t    channels_on_ap[2];
} wifi_easy_connect_t;

#define str(x) #x
#define enum_str(x) str(x)

#endif //_WIFI_EASY_CONNECT_H
