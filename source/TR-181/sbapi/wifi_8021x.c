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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include "ansc_wrapper_base.h"
#include "collection.h"
#include "cosa_wifi_apis.h"
#include "wifi_hal.h"
#include "wifi_8021x.h"
#include "cosa_wifi_internal.h"
#include "wifi_monitor.h"
#include "safec_lib_common.h"

#if defined(FEATURE_HOSTAP_AUTHENTICATOR)
#include "utils/common.h"
#endif //FEATURE_HOSTAP_AUTHENTICATOR

static char *to_mac_str    (mac_address_t mac, mac_addr_str_t key) {
    snprintf(key, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return (char *)key;
}

#if 0
static void to_mac_bytes   (mac_addr_str_t key, mac_address_t bmac) {
   unsigned int mac[6];
    sscanf(key, "%02x:%02x:%02x:%02x:%02x:%02x",
             &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
   bmac[0] = mac[0]; bmac[1] = mac[1]; bmac[2] = mac[2];
   bmac[3] = mac[3]; bmac[4] = mac[4]; bmac[5] = mac[5];

}
#endif

void process_eap_data(wifi_8021x_data_t *data, wifi_8021x_t *module, bool new_event)
{
    wifi_eap_frame_t *eap, *prev_eap;
    wifi_8021x_data_t *prev_data;
    mac_addr_str_t mac_str;
    char direction[32], msg[32];
    struct timeval tnow;
    errno_t rc = -1;

    gettimeofday(&tnow, NULL);

    eap = (wifi_eap_frame_t *)data->data;

    to_mac_str(data->mac, mac_str);

    if (new_event == false) {
        // this is an existing data, call originated from timeout
        if ((tnow.tv_sec - data->packet_time.tv_sec) > 2) {
            if ((eap->code == wifi_eap_code_request) || (eap->code == wifi_eap_code_response)) {
                module->bssid[data->vap].eap_timeout++;
            }

            wifi_dbg_print(1, "%s:%d: Removing from hash and deleting\n", __func__, __LINE__);
            hash_map_remove(module->bssid[data->vap].sta_map, mac_str);
            if (data->data != NULL)
                free(data->data);
            if (data != NULL)
                free(data);
        }

        return;
    }
#if defined(FEATURE_HOSTAP_AUTHENTICATOR)
    hapd_process_eapol_frame(data->vap, data->mac, data->data, data->len);
#endif
    prev_data = hash_map_get(module->bssid[data->vap].sta_map, mac_str);

    // if this is an eap success or failure
    if (eap->code == wifi_eap_code_success) {
        module->bssid[data->vap].eap_success++;
        if (prev_data != NULL) {
            hash_map_remove(module->bssid[data->vap].sta_map, mac_str);
            free(prev_data->data);
            free(prev_data);
        }
        return;
    } else if (eap->code == wifi_eap_code_success) {
        module->bssid[data->vap].eap_failure++;
        if (prev_data != NULL) {
            hash_map_remove(module->bssid[data->vap].sta_map, mac_str);
            free(prev_data->data);
            free(prev_data);
        }
        return;

    } else if ((eap->code == wifi_eap_code_request) || (eap->code == wifi_eap_code_response)) {
        if (prev_data != NULL) {
            if (prev_data->type == wifi_eapol_type_eap_packet) {
                if (prev_data->dir == data->dir) {

                    prev_eap = (wifi_eap_frame_t *)prev_data->data;
                    if ((prev_eap->code == eap->code) && (prev_eap->id == eap->id)) {
                        module->bssid[data->vap].eap_retries++;
                    }
                }
                hash_map_remove(module->bssid[data->vap].sta_map, mac_str);
                free(prev_data->data);
                free(prev_data);
            }

        }

        gettimeofday(&data->packet_time, NULL);
        hash_map_put(module->bssid[data->vap].sta_map, strdup(mac_str), data);
    }

    if (data->dir == wifi_direction_unknown) {
        rc = strcpy_s(direction, sizeof(direction), "unknown");
    } else if (data->dir == wifi_direction_uplink) {
        rc = strcpy_s(direction, sizeof(direction), "uplink");
    } else if (data->dir == wifi_direction_downlink) {
        rc = strcpy_s(direction, sizeof(direction), "downlink");
    }
    ERR_CHK(rc);

    if (eap->code == wifi_eap_code_request) {
        rc = strcpy_s(msg, sizeof(msg), "request");
    } else if (eap->code == wifi_eap_code_response) {
       rc = strcpy_s(msg, sizeof(msg), "response");
    } else if (eap->code == wifi_eap_code_success) {
       rc = strcpy_s(msg, sizeof(msg), "success");
    } else if (eap->code == wifi_eap_code_failure) {
       rc = strcpy_s(msg, sizeof(msg), "failure");
    }
    ERR_CHK(rc);

    wifi_dbg_print(1, "%s:%d: Received eap %s  id:%d diretion:%s\n", __func__, __LINE__, msg, eap->id, direction);
}

#if defined(FEATURE_HOSTAP_AUTHENTICATOR)
void process_eapol_key_data(wifi_8021x_data_t *data, wifi_8021x_t *module, bool new_event)
{
    wifi_eapol_key_frame_t  *key, *prev_key;
    wifi_8021x_data_t *prev_data;
    mac_addr_str_t mac_str;
    char direction[32], msg[32];
    struct timeval tnow;
    unsigned short key_info = 0, key_data_length = 0;
    errno_t rc = -1;

    gettimeofday(&tnow, NULL);

    wifi_8021x_frame_t *hdr = (wifi_8021x_frame_t *) data->data;
    key = (wifi_eapol_key_frame_t *) (hdr + 1);

    to_mac_str(data->mac, mac_str);

    if (new_event == false) {
        // this is an existing data, call originated from timeout
        if ((tnow.tv_sec - data->packet_time.tv_sec) > 2) {
            if (KEY_MSG_1_OF_4(key) || KEY_MSG_2_OF_4(key) || KEY_MSG_3_OF_4(key)) {
                module->bssid[data->vap].wpa_failure++;
                wifi_dbg_print(1, "%s:%d: timeout \n", __func__, __LINE__);
            } else {
                module->bssid[data->vap].wpa_success++;
                wifi_dbg_print(1, "%s:%d: timeout  \n", __func__, __LINE__);
            }
            wifi_dbg_print(1, "%s:%d: Removing from hash and deleting\n", __func__, __LINE__);
            hash_map_remove(module->bssid[data->vap].sta_map, mac_str);
            if (data->data != NULL)
                free(data->data);
            if (data)
                    free(data);
        }

        return;
    }

    prev_data = hash_map_get(module->bssid[data->vap].sta_map, mac_str);

    key_info = WPA_GET_BE16(key->key_info);
    key_data_length = WPA_GET_BE16((const uint8_t *)&key->key_len);

    if (KEY_MSG_1_OF_4(key)) {
        rc = strcpy_s(msg, sizeof(msg), "Message 1 of 4");
        ERR_CHK(rc);
        module->bssid[data->vap].wpa_msg_1_of_4++;
        wifi_dbg_print(1, "%s:%d:  Message 1 of 4 \n", __func__, __LINE__);

    }

    else if (!(key_info & WPA_KEY_INFO_KEY_TYPE)) { // 2/2 Groupwise
        hapd_process_eapol_frame(data->vap, data->mac, data->data, data->len);

        rc = strcpy_s(msg, sizeof(msg), "Message 2 of 2 Group");
        ERR_CHK(rc);
        wifi_dbg_print(1, "%s:%d:  Message 2 of 2 Group  \n", __func__, __LINE__);

        module->bssid[data->vap].wpa_msg_2_of_4++;

        if (prev_data != NULL) {
            if (prev_data->type == wifi_eapol_type_eapol_key) {
                if (prev_data->dir != data->dir) {
                    prev_key = (wifi_eapol_key_frame_t *)prev_data->data;
                    if (KEY_MSG_1_OF_4(prev_key)) {
                        hash_map_remove(module->bssid[data->vap].sta_map, mac_str);
                        free(prev_data->data);
                        free(prev_data);
                    }
                }
            }
        }
    } else if (key_data_length == 0) {  // 4/4 Pairwise
        hapd_process_eapol_frame(data->vap, data->mac, data->data, data->len);

        rc = strcpy_s(msg, sizeof(msg), "Message 4 of 4 Pairwise");
        ERR_CHK(rc);
        module->bssid[data->vap].wpa_msg_4_of_4++;
        wifi_dbg_print(1, "%s:%d: Message 4 of 4 Pairwise\n", __func__, __LINE__);

        if (prev_data != NULL) {
            if (prev_data->type == wifi_eapol_type_eapol_key) {
                if (prev_data->dir != data->dir) {
                    prev_key = (wifi_eapol_key_frame_t *)prev_data->data;
                    if (KEY_MSG_3_OF_4(prev_key)) {
                        hash_map_remove(module->bssid[data->vap].sta_map, mac_str);
                        free(prev_data->data);
                        free(prev_data);
                    }
                }
            }
        }
    } else if (KEY_MSG_3_OF_4(key)) {
        rc = strcpy_s(msg, sizeof(msg), "Message 3 of 4");
        ERR_CHK(rc);
        module->bssid[data->vap].wpa_msg_3_of_4++;
        wifi_dbg_print(1, "%s:%d:   Message 3 of 4 \n", __func__, __LINE__);

        if (prev_data != NULL) {
            if (prev_data->type == wifi_eapol_type_eapol_key) {
                if (prev_data->dir != data->dir) {
                    prev_key = (wifi_eapol_key_frame_t *)prev_data->data;
                    if (KEY_MSG_2_OF_4(prev_key)) {
                        hash_map_remove(module->bssid[data->vap].sta_map, mac_str);
                        free(prev_data->data);
                        free(prev_data);

                    }
                }
            }
        }
    } else  {   // 2/4 pairwise
        hapd_process_eapol_frame(data->vap, data->mac, data->data, data->len);

        rc = strcpy_s(msg, sizeof(msg), "Message 2 of 4 Pairwise");
        ERR_CHK(rc);
        wifi_dbg_print(1, "%s:%d:  Message 2 of 4 Pairwise\n", __func__, __LINE__);

        module->bssid[data->vap].wpa_msg_2_of_4++;

        if (prev_data != NULL) {
            if (prev_data->type == wifi_eapol_type_eapol_key) {
                if (prev_data->dir != data->dir) {
                    prev_key = (wifi_eapol_key_frame_t *)prev_data->data;
                    if (KEY_MSG_1_OF_4(prev_key)) {
                        hash_map_remove(module->bssid[data->vap].sta_map, mac_str);
                        free(prev_data->data);
                        free(prev_data);
                    }
                }
            }
        }
    }

    gettimeofday(&data->packet_time, NULL);
    hash_map_put(module->bssid[data->vap].sta_map, strdup(mac_str), data);


    if (data->dir == wifi_direction_unknown) {
        rc = strcpy_s(direction, sizeof(direction), "unknown");
    } else if (data->dir == wifi_direction_uplink) {
        rc = strcpy_s(direction, sizeof(direction), "uplink");
    } else if (data->dir == wifi_direction_downlink) {
        rc = strcpy_s(direction, sizeof(direction), "downlink");
    }
    ERR_CHK(rc);

    wifi_dbg_print(1, "%s:%d: Received eapol key packet: %s direction:%s\n", __func__, __LINE__, msg, direction);
}

void process_eapol_start(wifi_8021x_data_t *data, wifi_8021x_t *module, bool new_event)
{
        mac_addr_str_t mac_str;
        char direction[32], msg[32];
        struct timeval tnow;
        errno_t rc = -1;

        gettimeofday(&tnow, NULL);

        //    key = (struct wifi_eapol_key_frame_t *) (hdr + 1);

        if (!data->mac)
            return;

        to_mac_str(data->mac, mac_str);

        if (new_event == false) {
                // this is an existing data, call originated from timeout
                wifi_dbg_print(1, "%s:%d: Handle timeout\n", __func__, __LINE__);

                return;
        }

        hash_map_get(module->bssid[data->vap].sta_map, mac_str);

        hapd_process_eapol_frame(data->vap, data->mac, data->data, data->len);

        rc = strcpy_s(msg, sizeof(msg), "EAPOL-Start");
        ERR_CHK(rc);
        wifi_dbg_print(1, "%s:%d:  EAPOL-Start\n", __func__, __LINE__);

        gettimeofday(&data->packet_time, NULL);
        hash_map_put(module->bssid[data->vap].sta_map, strdup(mac_str), data);

        if (data->dir == wifi_direction_unknown) {
                rc = strcpy_s(direction, sizeof(direction), "unknown");
        } else if (data->dir == wifi_direction_uplink) {
                rc = strcpy_s(direction, sizeof(direction), "uplink");
        } else if (data->dir == wifi_direction_downlink) {
                rc = strcpy_s(direction, sizeof(direction), "downlink");
        }
        ERR_CHK(rc);

        wifi_dbg_print(1, "%s:%d: Received eapol start packet: %s direction:%s\n", __func__, __LINE__, msg, direction);
}
#endif //FEATURE_HOSTAP_AUTHENTICATOR

void process_assoc_req_packet(wifi_assoc_req_data_t *assoc_data, wifi_8021x_t *module)
{
#if defined(FEATURE_HOSTAP_AUTHENTICATOR)
    hapd_process_assoc_req_frame(assoc_data->vap, assoc_data->mac, assoc_data->data, assoc_data->len);
#endif
    UNREFERENCED_PARAMETER(module);
    free(assoc_data->data);
}

void process_assoc_rsp_packet(wifi_assoc_rsp_data_t *assoc_data, wifi_8021x_t *module)
{
#if defined(FEATURE_HOSTAP_AUTHENTICATOR)
//    hapd_process_assoc_rsp_frame(assoc_data->vap, assoc_data->mac, assoc_data->data, assoc_data->len);
#endif
    UNREFERENCED_PARAMETER(module);
    free(assoc_data->data);
}

void process_auth_packet(wifi_auth_data_t *auth_data, wifi_8021x_t *module)
{
#if defined(FEATURE_HOSTAP_AUTHENTICATOR)
    hapd_process_auth_frame(auth_data->vap, auth_data->mac, auth_data->data, auth_data->len, auth_data->dir);
#endif
    UNREFERENCED_PARAMETER(module);
    free(auth_data->data);
}

void process_8021x_packet(wifi_8021x_data_t *data, wifi_8021x_t *module)
{
    switch (data->type) {
        case wifi_eapol_type_eap_packet:
            wifi_dbg_print(1, "%s:%d: wifi_eapol_type_eap_packet:\n", __func__, __LINE__);
            process_eap_data(data, module, true);
            break;

        case wifi_eapol_type_eapol_start:
            wifi_dbg_print(1, "%s:%d: EAPOL_start:\n", __func__, __LINE__);
#if defined(FEATURE_HOSTAP_AUTHENTICATOR)
            process_eapol_start(data, module, true);
#endif
            break;

        case wifi_eapol_type_eapol_logoff:
            break;

        case wifi_eapol_type_eapol_key:
            wifi_dbg_print(1, "%s:%d  wifi_eapol_type_eapol_key:\n", __func__, __LINE__);
#if defined(FEATURE_HOSTAP_AUTHENTICATOR)
            process_eapol_key_data(data, module, true);
#endif
            break;
    }

}

void process_8021x_data_timeout(wifi_8021x_t *module)
{
    wifi_8021x_data_t *data, *tmp;
    unsigned int i;

#ifdef WIFI_HAL_VERSION_3
    for (i = 0; i < (UINT)getTotalNumberVAPs(); i++) {
#else
    for (i = 0; i < MAX_VAP; i++) {
#endif
        data = hash_map_get_first(module->bssid[i].sta_map);

        while (data != NULL) {
            tmp = data;
            data = hash_map_get_next(module->bssid[i].sta_map, data);

    
            switch (tmp->type) {
            case wifi_eapol_type_eap_packet:
                wifi_dbg_print(1, "%s:%d:   wifi_eapol_type_eap_packet:\n", __func__, __LINE__);
                process_eap_data(tmp, module, false);
                break;

            case wifi_eapol_type_eapol_start:
                wifi_dbg_print(1, "%s:%d:   wifi_eapol_type_eapol_start:\n", __func__, __LINE__);
                break;

            case wifi_eapol_type_eapol_logoff:
                wifi_dbg_print(1, "%s:%d:   wifi_eapol_type_eapol_logoff:\n", __func__, __LINE__);
                break;

            case wifi_eapol_type_eapol_key:
                wifi_dbg_print(1, "%s:%d:   wifi_eapol_type_eapol_key: \n", __func__, __LINE__);
#if defined(FEATURE_HOSTAP_AUTHENTICATOR)
                process_eapol_key_data(tmp, module, false);
#endif
                break;
            }

        }

        if (i == 0) {
            wifi_dbg_print(1, "%s:%d: Objects:%d \n \
                EAP Success:%d EAP Failures:%d EAP Timeouts:%d EAP Retries:%d\n \
                WPA Msg 1 of 4:%d WPA Msg 2 of 4:%d WPA Msg 3 of 4:%d WPA Msg 4 of 4:%d\n \
                WPA Success:%d WPA Failures:%d\n", __func__, __LINE__,
            hash_map_count(module->bssid[i].sta_map), module->bssid[i].eap_success, module->bssid[i].eap_failure,
                module->bssid[i].eap_timeout, module->bssid[i].eap_retries,
                module->bssid[i].wpa_msg_1_of_4, module->bssid[i].wpa_msg_2_of_4, module->bssid[i].wpa_msg_3_of_4, module->bssid[i].wpa_msg_4_of_4,
                module->bssid[i].wpa_success, module->bssid[i].wpa_failure);
        }
    }
}

void deinit_8021x(wifi_8021x_t *module)
{
       unsigned int i;

#ifdef WIFI_HAL_VERSION_3
        for (i = 0; i < (UINT)getTotalNumberVAPs(); i++) {
#else
        for (i = 0; i < MAX_VAP; i++) {
#endif
               hash_map_destroy(module->bssid[i].sta_map);
       }

}

int init_8021x(wifi_8021x_t *module)
{
       unsigned int i;
#ifdef WIFI_HAL_VERSION_3
        for (i = 0; i < (UINT) getTotalNumberVAPs(); i++) {
#else
        for (i = 0; i < MAX_VAP; i++) {
#endif               
            module->bssid[i].sta_map = hash_map_create();
       }

       return 0;
}

#if defined(FEATURE_HOSTAP_AUTHENTICATOR)
int init_lib_hostapd(PCOSA_DATAMODEL_WIFI pWifi, PCOSA_DML_WIFI_AP pWifiAp, PCOSA_DML_WIFI_SSID pWifiSsid, PCOSA_DML_WIFI_RADIO_FULL pWifiRadioFull)
{
#if defined (_XB7_PRODUCT_REQ_)
    return libhostapd_wpa_init(pWifi, pWifiAp, pWifiSsid, pWifiRadioFull);
#else
    return hapd_wpa_init(pWifi, pWifiAp, pWifiSsid, pWifiRadioFull);
#endif
}

void libhostap_eloop_run()
{
    hapd_wpa_run();
}

int deinit_lib_hostapd(int ap_index)
{
#if defined (_XB7_PRODUCT_REQ_)
    return libhostapd_wpa_deinit(ap_index);
#else
    return hapd_wpa_deinit(ap_index);
#endif
}
#endif /* FEATURE_HOSTAP_AUTHENTICATOR */
