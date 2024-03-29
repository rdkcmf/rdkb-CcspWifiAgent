/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <cjson/cJSON.h>
#include "wifi_hal.h"
#include "os.h"
#include "util.h"
#include "schema.h"
#include "log.h"
#include "ds.h"
#include "json_util.h"
#include "target.h"
#include <ev.h>
#include <assert.h>
#include "collection.h"
#include "wifi_ovsdb.h"
#include "ccsp_base_api.h"

void rdk_wifi_dbg_print(int level, char *format, ...)
{
    char buff[2048] = {0};
    va_list list;
    static FILE *fpg = NULL;

    if ((access("/nvram/rdkWifiDbg", R_OK)) != 0) {
        return;
    }

    va_start(list, format);
    vsprintf(&buff[strlen(buff)], format, list);
    va_end(list);

    if (fpg == NULL) {
        fpg = fopen("/tmp/rdkWifi", "a+");
        if (fpg == NULL) {
            return;
        } else {
            fputs(buff, fpg);
        }
    } else {
        fputs(buff, fpg);
    }

    fflush(fpg);
}

int wifidb_get_factory_reset_data(bool *data)
{
	return 0;
}

int wifidb_set_factory_reset_data(bool data)
{
	return 0;
}

int wifidb_del_interworking_entry()
{
    return 0;
}

int wifidb_check_wmm_params()
{
    return 0;
}

int wifidb_get_reset_hotspot_required(bool *req)
{
    return 0;
}

int wifidb_set_reset_hotspot_required(bool req)
{
    return 0;
}

int rdk_wifi_radio_get_vap_name(uint8_t vap_index, char *l_vap_name)
{
    if((vap_index > 9))
    {
        rdk_wifi_dbg_print(1, "wifidb radio get vap invalid index... %s\n", __FUNCTION__);
        return -1;
    }
    char *vap_name[] = {"private_ssid_2g", "private_ssid_5g", "iot_ssid_2g", "iot_ssid_5g", "hotspot_open_2g", "hotspot_open_5g", "lnf_psk_2g", "lnf_psk_5g", "hotspot_secure_2g", "hotspot_secure_5g"};

    strncpy(l_vap_name, vap_name[vap_index], strlen(vap_name[vap_index]));
    return CCSP_SUCCESS;
}

void rdk_wifi_radio_get_status(uint8_t r_index, bool *status)
{
    wifi_radio_operationParam_t radio_vap_map;
    memset(&radio_vap_map, 0, sizeof(radio_vap_map));

    rdk_wifi_dbg_print(1, "wifidb radio get status %s\n", __FUNCTION__);
    wifidb_get_wifi_radio_config(r_index, &radio_vap_map);
    *status = radio_vap_map.enable;
}

void rdk_wifi_radio_get_autochannel_status(uint8_t r_index, bool *autochannel_status)
{
    wifi_radio_operationParam_t radio_vap_map;
    memset(&radio_vap_map, 0, sizeof(radio_vap_map));

    rdk_wifi_dbg_print(1, "wifidb radio get auto channel status %s\n", __FUNCTION__);
    wifidb_get_wifi_radio_config(r_index, &radio_vap_map);
    *autochannel_status = radio_vap_map.autoChannelEnabled;
}

void rdk_wifi_radio_get_frequency_band(uint8_t r_index, char *band)
{
    wifi_radio_operationParam_t radio_vap_map;
    memset(&radio_vap_map, 0, sizeof(radio_vap_map));

    wifidb_get_wifi_radio_config(r_index, &radio_vap_map);
    if ( radio_vap_map.band == 1 )
    {
        strcpy(band, "2.4GHz");
    }
    else if ( radio_vap_map.band == 2 )
    {
        strcpy(band, "5GHz");
    }
}

void rdk_wifi_radio_get_dcs_status(uint8_t r_index, bool *dcs_status)
{
    wifi_radio_operationParam_t radio_vap_map;
    memset(&radio_vap_map, 0, sizeof(radio_vap_map));

    rdk_wifi_dbg_print(1, "wifidb radio get dcs status %s\n", __FUNCTION__);
    wifidb_get_wifi_radio_config(r_index, &radio_vap_map);
    *dcs_status = radio_vap_map.DCSEnabled;
}

void rdk_wifi_radio_get_channel(uint8_t r_index, ULONG *channel)
{
    wifi_radio_operationParam_t radio_vap_map;
    memset(&radio_vap_map, 0, sizeof(radio_vap_map));

    wifidb_get_wifi_radio_config(r_index, &radio_vap_map);
    *channel = radio_vap_map.channel;
}

void rdk_wifi_radio_get_channel_bandwidth(uint8_t r_index, ULONG *channel_bandwidth)
{
    wifi_radio_operationParam_t radio_vap_map;
    memset(&radio_vap_map, 0, sizeof(radio_vap_map));

    wifidb_get_wifi_radio_config(r_index, &radio_vap_map);
    *channel_bandwidth = radio_vap_map.channelWidth;
}

void rdk_wifi_radio_get_operating_standards(uint8_t r_index, char *buf)
{

    wifi_radio_operationParam_t radio_vap_map;
    memset(&radio_vap_map, 0, sizeof(radio_vap_map));

    wifidb_get_wifi_radio_config(r_index, &radio_vap_map);

        if (radio_vap_map.variant & WIFI_80211_VARIANT_A )
        {
            strcat(buf, "a");
        }
        
        if (radio_vap_map.variant & WIFI_80211_VARIANT_B )
        {
            if (strlen(buf) != 0)
            {
                strcat(buf, ",b");
            }
            else
            {
                strcat(buf, "b");
            }
        }
        
        if (radio_vap_map.variant & WIFI_80211_VARIANT_G )
        {
            if (strlen(buf) != 0)
            {
                strcat(buf, ",g");
            }
            else
            {
                strcat(buf, "g");
            }
        }
        
        if (radio_vap_map.variant & WIFI_80211_VARIANT_N )
        {
            if (strlen(buf) != 0)
            {
                strcat(buf, ",n");
            }
            else
            {
                strcat(buf, "n");
            }
        }

        if (radio_vap_map.variant & WIFI_80211_VARIANT_AC )
        {
            if (strlen(buf) != 0)
            {
                strcat(buf, ",ac");
            }
            else
            {
                strcat(buf, "ac");
            }
        }
#ifdef _WIFI_AX_SUPPORT_
        if (radio_vap_map.variant & WIFI_80211_VARIANT_AX )
        {
            if (strlen(buf) != 0)
            {
                strcat(buf, ",ax");
            }
            else
            {
                strcat(buf, "ax");
            }
        }
#endif

}

int rdk_wifi_vap_get_from_index(int wlanIndex, wifi_vap_info_t *vap_map)
{
    int retDbGet = CCSP_SUCCESS;
    char l_vap_name[32];
    memset(l_vap_name, 0, sizeof(l_vap_name));
    memset(vap_map, 0 ,sizeof(wifi_vap_info_t));

    retDbGet = rdk_wifi_radio_get_vap_name(wlanIndex, l_vap_name);
    if(retDbGet != CCSP_SUCCESS)
    {
        rdk_wifi_dbg_print(1, "wifidb vap name info get failure\n");
	return retDbGet;
    }
    retDbGet = wifidb_get_wifi_vap_info(l_vap_name, vap_map);
    if(retDbGet != CCSP_SUCCESS)
    {
        rdk_wifi_dbg_print(1, "wifidb vap info get failure\n");
    }
    else
    {
        rdk_wifi_dbg_print(1, "Get wifiDb_vap_parameter vap_index:%d:: l_vap_name = %s \n", wlanIndex, l_vap_name);
    }
    return retDbGet;
}

int rdk_wifi_vap_update_from_index(int wlanIndex, wifi_vap_info_t *vap_map)
{
    int retDbSet = CCSP_SUCCESS;
    char l_vap_name[32];
    memset(l_vap_name, 0, sizeof(l_vap_name));

    retDbSet = rdk_wifi_radio_get_vap_name(wlanIndex, l_vap_name);
    if(retDbSet != CCSP_SUCCESS)
    {
        rdk_wifi_dbg_print(1, "wifidb vap name info get failure\n");
	return retDbSet;
    }

    retDbSet = wifidb_update_wifi_vap_info(l_vap_name, vap_map);
    if(retDbSet != CCSP_SUCCESS)
    {
        rdk_wifi_dbg_print(1, "wifidb vap info set failure\n");
    }
    else
    {
        rdk_wifi_dbg_print(1, "Set wifiDb_vap_parameter success...vap_index:%d: vap_name: = %s\n", wlanIndex, l_vap_name);
    }
    return retDbSet;
}

int rdk_wifi_vap_security_get_from_index(int wlanIndex, wifi_vap_security_t *sec)
{
    rdk_wifi_dbg_print(1, "Enter vap security get from index\n");
    int retDbGet = CCSP_SUCCESS;
    char l_vap_name[32];
    memset(l_vap_name, 0, sizeof(l_vap_name));
    memset(sec, 0 ,sizeof(wifi_vap_security_t));

    retDbGet = rdk_wifi_radio_get_vap_name(wlanIndex, l_vap_name);
    if(retDbGet != CCSP_SUCCESS)
    {
        rdk_wifi_dbg_print(1, "wifidb vap name info get failure\n");
	return retDbGet;
    }

    retDbGet = wifidb_get_wifi_security_config(l_vap_name, sec);
    if(retDbGet != CCSP_SUCCESS)
    {
        rdk_wifi_dbg_print(1, "wifidb vap security info get failure\n");
    }
    else
    {
        rdk_wifi_dbg_print(1, "Get wifiDb_vap_security_parameter vap_index:%d:: l_vap_name = %s \n", wlanIndex, l_vap_name);
    }
    return retDbGet;
}

int rdk_wifi_vap_security_update_from_index(int wlanIndex, wifi_vap_security_t *sec)
{
    rdk_wifi_dbg_print(1, "Enter vap security update from index\n");
    int retDbSet = CCSP_SUCCESS;
    char l_vap_name[32];
    memset(l_vap_name, 0, sizeof(l_vap_name));

    retDbSet = rdk_wifi_radio_get_vap_name(wlanIndex, l_vap_name);
    if(retDbSet != CCSP_SUCCESS)
    {
        rdk_wifi_dbg_print(1, "wifidb vap name info get failure\n");
	return retDbSet;
    }

    retDbSet = wifidb_update_wifi_security_config(l_vap_name, sec); 
    if(retDbSet != CCSP_SUCCESS)
    {
        rdk_wifi_dbg_print(1, "wifidb vap info set failure\n");
    }
    else
    {
        rdk_wifi_dbg_print(1, "Set wifiDb_vap_security_parameter...vap_index:%d: vap_name: = %s\n", wlanIndex, l_vap_name);
    }
    return retDbSet;
}

int rdk_wifi_SetRapidReconnectThresholdValue(int wlanIndex, int rapidReconnThresholdValue)
{
    int ret = CCSP_SUCCESS;
    wifi_vap_info_t vap_map;
    ret = rdk_wifi_vap_get_from_index(wlanIndex, &vap_map);
    vap_map.u.bss_info.rapidReconnThreshold = rapidReconnThresholdValue;
    rdk_wifi_dbg_print(1, "wifidb vap info set rapidReconnThresholdValue %d\n", rapidReconnThresholdValue);
    ret = rdk_wifi_vap_update_from_index(wlanIndex, &vap_map);
    return ret;
}

int rdk_wifi_GetRapidReconnectThresholdValue(int wlanIndex, int *rapidReconnThresholdValue)
{
    int ret = CCSP_SUCCESS;
    wifi_vap_info_t vap_map;
    ret = rdk_wifi_vap_get_from_index(wlanIndex, &vap_map);
    if(ret != CCSP_SUCCESS)
    {
        rdk_wifi_dbg_print(1, "rdk wifi vap get index failure :%s\n",__FUNCTION__);
	return ret;
    }
    *rapidReconnThresholdValue = vap_map.u.bss_info.rapidReconnThreshold;
    rdk_wifi_dbg_print(1, "wifidb vap info get rapidReconnThresholdValue %d\n", *rapidReconnThresholdValue);
    return ret;
}

int rdk_wifi_SetRapidReconnectEnable(int wlanIndex, bool reconnectCountEnable)
{
    int ret = CCSP_SUCCESS;
    wifi_vap_info_t vap_map;
    ret = rdk_wifi_vap_get_from_index(wlanIndex, &vap_map);
    vap_map.u.bss_info.rapidReconnectEnable = reconnectCountEnable;
    rdk_wifi_dbg_print(1, "wifidb vap info set reconnectEnable %d\n", reconnectCountEnable);
    ret = rdk_wifi_vap_update_from_index(wlanIndex, &vap_map);
    return ret;
}

int rdk_wifi_GetRapidReconnectEnable(int wlanIndex, bool *reconnectCountEnable)
{
    int ret = CCSP_SUCCESS;
    wifi_vap_info_t vap_map;
    ret = rdk_wifi_vap_get_from_index(wlanIndex, &vap_map);
    if(ret != CCSP_SUCCESS)
    {
        rdk_wifi_dbg_print(1, "rdk wifi vap get index failure :%s\n",__FUNCTION__);
	return ret;
    }
    *reconnectCountEnable = vap_map.u.bss_info.rapidReconnectEnable;
    rdk_wifi_dbg_print(1, "wifidb vap info get reconnectEnable %d\n", *reconnectCountEnable);
    return ret;
}

int rdk_wifi_SetNeighborReportActivated(int wlanIndex, bool bNeighborReportActivated)
{
    int ret = CCSP_SUCCESS;
    wifi_vap_info_t vap_map;
    ret = rdk_wifi_vap_get_from_index(wlanIndex, &vap_map);
    vap_map.u.bss_info.nbrReportActivated = bNeighborReportActivated;
    rdk_wifi_dbg_print(1, "wifidb vap info set nbrReportActivated %d\n", bNeighborReportActivated);
    ret = rdk_wifi_vap_update_from_index(wlanIndex, &vap_map);
    return ret;
}

int rdk_wifi_GetNeighborReportActivated(int wlanIndex, bool *bNeighborReportActivated)
{
    int ret = CCSP_SUCCESS;
    wifi_vap_info_t vap_map;
    ret = rdk_wifi_vap_get_from_index(wlanIndex, &vap_map);
    if(ret != CCSP_SUCCESS)
    {
        rdk_wifi_dbg_print(1, "rdk wifi vap get index failure :%s\n",__FUNCTION__);
	return ret;
    }
    *bNeighborReportActivated = vap_map.u.bss_info.nbrReportActivated;
    rdk_wifi_dbg_print(1, "wifidb vap info get nbrReportActivated %d\n", *bNeighborReportActivated);
    return ret;
}

int rdk_wifi_ApSetStatsEnable(int wlanIndex, bool bValue)
{
    int ret = CCSP_SUCCESS;
    wifi_vap_info_t vap_map;
    ret = rdk_wifi_vap_get_from_index(wlanIndex, &vap_map);
    vap_map.u.bss_info.vapStatsEnable = bValue;
    rdk_wifi_dbg_print(1, "wifidb vap info set vapStatsEnable %d\n", bValue);
    ret = rdk_wifi_vap_update_from_index(wlanIndex, &vap_map);
    return ret;
}

int rdk_wifi_ApGetStatsEnable(int wlanIndex, bool *bValue)
{
    int ret = CCSP_SUCCESS;
    wifi_vap_info_t vap_map;
    ret = rdk_wifi_vap_get_from_index(wlanIndex, &vap_map);
    if(ret != CCSP_SUCCESS)
    {
        rdk_wifi_dbg_print(1, "rdk wifi vap get index failure :%s\n",__FUNCTION__);
	return ret;
    }
    *bValue = vap_map.u.bss_info.vapStatsEnable;
    rdk_wifi_dbg_print(1, "wifidb vap info get vapStatsEnable %d\n", *bValue);
    return ret;
}

int rdk_wifi_setBSSTransitionActivated(int wlanIndex, bool BSSTransitionActivated)
{
    int ret = CCSP_SUCCESS;
    wifi_vap_info_t vap_map;
    ret = rdk_wifi_vap_get_from_index(wlanIndex, &vap_map);
    vap_map.u.bss_info.bssTransitionActivated = BSSTransitionActivated;
    rdk_wifi_dbg_print(1, "wifidb vap info set BSSTransitionActivated %d\n", BSSTransitionActivated);
    ret = rdk_wifi_vap_update_from_index(wlanIndex, &vap_map);
    return ret;
}

int rdk_wifi_getBSSTransitionActivated(int wlanIndex, bool *BSSTransitionActivated)
{
    int ret = CCSP_SUCCESS;
    wifi_vap_info_t vap_map;
    ret = rdk_wifi_vap_get_from_index(wlanIndex, &vap_map);
    if(ret != CCSP_SUCCESS)
    {
        rdk_wifi_dbg_print(1, "rdk wifi vap get index failure :%s\n",__FUNCTION__);
	return ret;
    }
    *BSSTransitionActivated = vap_map.u.bss_info.bssTransitionActivated;
    rdk_wifi_dbg_print(1, "wifidb vap info get BSSTransitionActivated %d\n", *BSSTransitionActivated);
    return ret;
}

int rdk_wifi_GetApMacFilterMode(int wlanIndex, int *mode)
{
    int ret = CCSP_SUCCESS;
    wifi_vap_info_t vap_map;
    ret = rdk_wifi_vap_get_from_index(wlanIndex, &vap_map);
    if(ret != CCSP_SUCCESS)
    {
        rdk_wifi_dbg_print(1, "rdk wifi vap get index failure :%s\n",__FUNCTION__);
	return ret;
    }
    *mode = vap_map.u.bss_info.mac_filter_mode;
    rdk_wifi_dbg_print(1, "wifidb vap info get mac_filter_mode %d\n", *mode);
    return ret;
}

int rdk_wifi_SetApMacFilterMode(int wlanIndex, int mode)
{
    int ret = CCSP_SUCCESS;
    wifi_vap_info_t vap_map;
    ret = rdk_wifi_vap_get_from_index(wlanIndex, &vap_map);
    vap_map.u.bss_info.mac_filter_mode = mode;
    rdk_wifi_dbg_print(1, "wifidb vap info set mac_filter_mode %d\n", mode);
    ret = rdk_wifi_vap_update_from_index(wlanIndex, &vap_map);
    return ret;
}

int rdk_wifi_radio_get_BeaconInterval(uint8_t r_index, int *BeaconInterval)
{
    int ret = CCSP_SUCCESS;

    wifi_radio_operationParam_t radio_vap_map;
    memset(&radio_vap_map, 0, sizeof(radio_vap_map));

    ret = wifidb_get_wifi_radio_config(r_index, &radio_vap_map);
    if(ret == CCSP_SUCCESS)
    {
       rdk_wifi_dbg_print(1, "wifidb radio beacon info get success %s: r_index:%d\n", __FUNCTION__, r_index);
       *BeaconInterval = radio_vap_map.beaconInterval;
    }
    else
    {
       rdk_wifi_dbg_print(1, "wifidb radio beacon info get failure %s r_index:%d\n", __FUNCTION__, r_index);
    }
    return ret;
}

int rdk_wifi_radio_get_parameters(uint8_t r_index, wifi_radio_operationParam_t *radio_vap_map)
{
    int ret = CCSP_SUCCESS;
    memset(radio_vap_map, 0, sizeof(wifi_radio_operationParam_t));

    ret = wifidb_get_wifi_radio_config(r_index, radio_vap_map);
    if(ret == CCSP_SUCCESS)
    {
       rdk_wifi_dbg_print(1, "wifidb radio info get success %s r_index:%d\n", __FUNCTION__, r_index);
    }
    else
    {
       rdk_wifi_dbg_print(1, "wifidb radio info get failure %s r_index:%d\n", __FUNCTION__, r_index);
    }
    return ret;
}

