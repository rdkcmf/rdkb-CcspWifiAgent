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

#include "wifi_data_plane.h"
#include "wifi_monitor.h"
#include "plugin_main_apis.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/un.h>
#include <assert.h>
#include "ansc_status.h"
#include <sysevent/sysevent.h>
#include <cjson/cJSON.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "cJSON.h"
#include "os.h"
#include "util.h"
#include "schema.h"
#include "ds.h"
#include "json_util.h"
#include "target.h"
#include <ev.h>
#include "wifi_ovsdb.h"
#include "dirent.h"
#include "ccsp_psm_helper.h"
#include "safec_lib_common.h"

wifi_ovsdb_t	g_wifidb;

ovsdb_table_t table_Wifi_Radio_Config;
ovsdb_table_t table_Wifi_VAP_Config;
ovsdb_table_t table_Wifi_Security_Config;
ovsdb_table_t table_Wifi_Device_Config;
ovsdb_table_t table_Wifi_Interworking_Config;
ovsdb_table_t table_Wifi_GAS_Config;
ovsdb_table_t table_Wifi_Global_Config;
ovsdb_table_t table_Wifi_MacFilter_Config;
static char *MacFilterList      = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.MacFilterList";
static char *MacFilter          = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.MacFilter.%d";
static char *MacFilterDevice    = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.MacFilterDevice.%d";

int wifidb_update_wifi_macfilter_config(unsigned int apIndex);

int wifidb_wfd = 0;

#define WIFI_MAX_VAP 16
extern ANSC_HANDLE bus_handle;
extern char   g_Subsystem[32];
extern PCOSA_BACKEND_MANAGER_OBJECT g_pCosaBEManager;

#ifdef WIFI_HAL_VERSION_3
const char *MFPConfig[3] = {"Disabled", "Optional", "Required"};
#endif
void wifi_db_dbg_print(int level, char *format, ...)
{
    char buff[2048] = {0};
    va_list list;
    static FILE *fpg = NULL;

    if ((access("/nvram/wifiDbDbg", R_OK)) != 0) {
        return;
    }

    va_start(list, format);
    vsprintf(&buff[strlen(buff)], format, list);
    va_end(list);

    if (fpg == NULL) {
        fpg = fopen("/tmp/wifiDb", "a+");
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

void callback_Wifi_Device_Config(ovsdb_update_monitor_t *mon,
        struct schema_Wifi_Device_Config *old_rec,
        struct schema_Wifi_Device_Config *new_rec)
{
	if (mon->mon_type == OVSDB_UPDATE_DEL) {
		wifi_db_dbg_print(1,"%s:%d:Delete\n", __func__, __LINE__); 
	} else if (mon->mon_type == OVSDB_UPDATE_NEW) {
		wifi_db_dbg_print(1,"%s:%d:New\n", __func__, __LINE__); 
	} else if (mon->mon_type == OVSDB_UPDATE_MODIFY) {
		wifi_db_dbg_print(1,"%s:%d:Modify\n", __func__, __LINE__); 
	} else {
		wifi_db_dbg_print(1,"%s:%d:Unknown\n", __func__, __LINE__);
	}
}
void callback_Wifi_Security_Config(ovsdb_update_monitor_t *mon,
        struct schema_Wifi_Security_Config *old_rec,
        struct schema_Wifi_Security_Config *new_rec)
{
	if (mon->mon_type == OVSDB_UPDATE_DEL) {
		wifi_db_dbg_print(1,"%s:%d:Delete\n", __func__, __LINE__); 
	} else if (mon->mon_type == OVSDB_UPDATE_NEW) {
		wifi_db_dbg_print(1,"%s:%d:New\n", __func__, __LINE__); 
	} else if (mon->mon_type == OVSDB_UPDATE_MODIFY) {
		wifi_db_dbg_print(1,"%s:%d:Modify\n", __func__, __LINE__); 
	} else {
		wifi_db_dbg_print(1,"%s:%d:Unknown\n", __func__, __LINE__);
	}
}
void callback_Wifi_Interworking_Config(ovsdb_update_monitor_t *mon,
        struct schema_Wifi_Interworking_Config *old_rec,
        struct schema_Wifi_Interworking_Config *new_rec)
{      
        if (mon->mon_type == OVSDB_UPDATE_DEL) {
                wifi_db_dbg_print(1,"%s:%d:Delete\n", __func__, __LINE__);
        } else if (mon->mon_type == OVSDB_UPDATE_NEW) {
                wifi_db_dbg_print(1,"%s:%d:New\n", __func__, __LINE__);
        } else if (mon->mon_type == OVSDB_UPDATE_MODIFY) {
                wifi_db_dbg_print(1,"%s:%d:Modify\n", __func__, __LINE__);
        } else {
                wifi_db_dbg_print(1,"%s:%d:Unknown\n", __func__, __LINE__);
        }
}
void callback_Wifi_VAP_Config(ovsdb_update_monitor_t *mon,
        struct schema_Wifi_VAP_Config *old_rec,
        struct schema_Wifi_VAP_Config *new_rec)
{
	if (mon->mon_type == OVSDB_UPDATE_DEL) {
		wifi_db_dbg_print(1,"%s:%d:Delete\n", __func__, __LINE__); 
	} else if (mon->mon_type == OVSDB_UPDATE_NEW) {
		wifi_db_dbg_print(1,"%s:%d:New\n", __func__, __LINE__); 
	} else if (mon->mon_type == OVSDB_UPDATE_MODIFY) {
		wifi_db_dbg_print(1,"%s:%d:Modify\n", __func__, __LINE__); 
	} else {
		wifi_db_dbg_print(1,"%s:%d:Unknown\n", __func__, __LINE__);
	}
}
void callback_Wifi_GAS_Config(ovsdb_update_monitor_t *mon,
        struct schema_Wifi_GAS_Config *old_rec,
        struct schema_Wifi_GAS_Config *new_rec)
{
        if (mon->mon_type == OVSDB_UPDATE_DEL) {
                wifi_db_dbg_print(1,"%s:%d:Delete\n", __func__, __LINE__);
        } else if (mon->mon_type == OVSDB_UPDATE_NEW) {
                wifi_db_dbg_print(1,"%s:%d:New\n", __func__, __LINE__);
        } else if (mon->mon_type == OVSDB_UPDATE_MODIFY) {
                wifi_db_dbg_print(1,"%s:%d:Modify\n", __func__, __LINE__);
        } else {
                wifi_db_dbg_print(1,"%s:%d:Unknown\n", __func__, __LINE__);
        }
}
void callback_Wifi_Global_Config(ovsdb_update_monitor_t *mon,
        struct schema_Wifi_Global_Config *old_rec,
        struct schema_Wifi_Global_Config *new_rec)
{
        UNREFERENCED_PARAMETER(old_rec);
        UNREFERENCED_PARAMETER(new_rec);
        if (mon->mon_type == OVSDB_UPDATE_DEL) {
                wifi_passpoint_dbg_print("%s:%d:Delete\n", __func__, __LINE__);
        } else if (mon->mon_type == OVSDB_UPDATE_NEW) {
                wifi_passpoint_dbg_print("%s:%d:New\n", __func__, __LINE__);
        } else if (mon->mon_type == OVSDB_UPDATE_MODIFY) {
                wifi_passpoint_dbg_print("%s:%d:Modify\n", __func__, __LINE__);
        } else {
                wifi_passpoint_dbg_print("%s:%d:Unknown\n", __func__, __LINE__);
        }
}

int wifidb_del_wifi_macfilter_config(unsigned int ap_index, char *macaddr)
{
    char macfilterkey[128];
    json_t *where;
    int ret;
#ifndef WIFI_HAL_VERSION_3
    char *temp_vap_name[] = {"private_ssid_2g", "private_ssid_5g", "iot_ssid_2g", "iot_ssid_5g",
        "hotspot_open_2g", "hotspot_open_5g", "lnf_psk_2g", "lnf_psk_5g",
        "hotspot_secure_2g", "hotspot_secure_5g","lnf_radius_2g","lnf_radius_5g",
        "mesh_backhaul_2g","mesh_backhaul_5g","guest_ssid_2g","guest_ssid_5g"};
#endif
    char vapname [32];

    wifi_db_dbg_print(1, "%s:%d Enter \n", __func__, __LINE__);

    memset(macfilterkey, 0, sizeof(macfilterkey));
    memset(vapname, 0, sizeof(vapname));

    if (macaddr == NULL) {
        wifi_db_dbg_print(1, "%s:%d NULL mac address\n", __func__, __LINE__);
        return -1;
    }

#ifdef WIFI_HAL_VERSION_3
    snprintf(vapname, sizeof(vapname), "%s", getVAPName(ap_index));
    if(strncmp((CHAR *) vapname, "unused", strlen("unused")) == 0)
    {
        wifi_db_dbg_print(1, "%s:%d vapname is %s for ap_index : %d\n", __func__, __LINE__, vapname, ap_index);
        return -1;
    }

#else
    snprintf(vapname, sizeof(vapname), "%s", temp_vap_name[ap_index]);
#endif

    snprintf(macfilterkey, sizeof(macfilterkey), "%s-%s", vapname, macaddr);
    wifi_db_dbg_print(1, "%s:%d Enter macfilterkey : %s\n", __func__, __LINE__, macfilterkey);

    where = ovsdb_tran_cond(OCLM_STR, "macfilter_key", OFUNC_EQ, macfilterkey);
    ret = ovsdb_table_delete_where(g_wifidb.ovsdb_sock_path, &table_Wifi_MacFilter_Config, where);
    if (ret != 1) {
        wifi_db_dbg_print(1, "%s:%d Failed to delete mac filter entry %s\n", __func__, __LINE__, macfilterkey);
        return -1;
    }
    wifi_db_dbg_print(1, "%s:%d Succesfully deleted mac filter entry %s\n", __func__, __LINE__, macfilterkey);

    return 0;
}

int wifidb_add_wifi_macfilter_config(unsigned int ap_index, PCOSA_DML_WIFI_AP_MAC_FILTER pMacFilt)
{
    struct schema_Wifi_MacFilter_Config cfg_mac;
    char *filter_mac[] = {"-", NULL};
    char macfilterkey[128];
#ifndef WIFI_HAL_VERSION_3
    char *temp_vap_name[] = {"private_ssid_2g", "private_ssid_5g", "iot_ssid_2g", "iot_ssid_5g",
        "hotspot_open_2g", "hotspot_open_5g", "lnf_psk_2g", "lnf_psk_5g",
        "hotspot_secure_2g", "hotspot_secure_5g","lnf_radius_2g","lnf_radius_5g",
        "mesh_backhaul_2g","mesh_backhaul_5g","guest_ssid_2g","guest_ssid_5g"};
#endif
    char vapname [32];
    int ret = 0;


    wifi_db_dbg_print(1, "%s:%d Enter \n", __func__, __LINE__);
    memset(macfilterkey, 0, sizeof(macfilterkey));

    if (pMacFilt == NULL) {
        wifi_db_dbg_print(1, "%s:%d NULL MAcfilter Instance\n", __func__, __LINE__);
        return -1;
    }
    memset(vapname, 0, sizeof(vapname));
#ifdef WIFI_HAL_VERSION_3
    snprintf(vapname, sizeof(vapname), "%s", getVAPName(ap_index));
    if(strncmp((CHAR *) vapname, "unused", strlen("unused")) == 0)
    {
        wifi_db_dbg_print(1, "%s:%d vapname is %s for ap_index : %d\n", __func__, __LINE__, vapname, ap_index);
        return -1;
    }

#else
    snprintf(vapname, sizeof(vapname), "%s", temp_vap_name[ap_index]);
#endif

    //Check for Macaddress
    if (strncmp(pMacFilt->MACAddress, "00:00:00:00:00:00", strlen("00:00:00:00:00:00")) == 0)
    {
        wifi_db_dbg_print(1, "%s:%d MACAddress %s is not valid for vapname is %s for ap_index : %d\n", __func__, __LINE__, pMacFilt->MACAddress, vapname, ap_index);
        return -1;
    }

    snprintf(macfilterkey, sizeof(macfilterkey), "%s-%s", vapname, pMacFilt->MACAddress);

    memset(&cfg_mac, 0, sizeof(cfg_mac));
    strncpy(cfg_mac.device_mac, pMacFilt->MACAddress, sizeof(cfg_mac.device_mac)-1);
    strncpy(cfg_mac.device_name, pMacFilt->DeviceName, sizeof(cfg_mac.device_name)-1);

    strncpy(cfg_mac.macfilter_key, macfilterkey, sizeof(cfg_mac.macfilter_key));

    if (ovsdb_table_upsert_with_parent(g_wifidb.ovsdb_sock_path, &table_Wifi_MacFilter_Config, &cfg_mac, false, filter_mac,
                SCHEMA_TABLE(Wifi_VAP_Config), ovsdb_where_simple(SCHEMA_COLUMN(Wifi_VAP_Config, vap_name), vapname),
                SCHEMA_COLUMN(Wifi_VAP_Config, mac_filter)) ==  false) {
        wifi_db_dbg_print(1,"%s:%d: failed to update macfilter for %s\n", __func__, __LINE__, macfilterkey);
        ret = -1;
    }
    else {
        wifi_db_dbg_print(1,"%s:%d: updated table_Wifi_MacFilter_Config table for %s\n", __func__, __LINE__, macfilterkey);
    }
    return ret;
}

int update_wifi_ovsdb_security(wifi_vap_info_t *vap_info, struct schema_Wifi_Security_Config *cfg, bool update)
{
	struct schema_Wifi_Security_Config *pcfg;
	json_t *where;
	int count;
	int ret;
	errno_t rc = -1;
	if (update == true) {
		where = ovsdb_tran_cond(OCLM_UUID, "_uuid", OFUNC_EQ, cfg->_uuid.uuid);
		pcfg = ovsdb_table_select_where(g_wifidb.ovsdb_sock_path, &table_Wifi_Security_Config, where, &count);
		if ((count != 0) && (pcfg != NULL)) {
			assert(count == 1);
			memcpy(cfg, pcfg, sizeof(struct schema_Wifi_Security_Config));
			free(pcfg);
		}
	}
	wifi_db_dbg_print(1,"%s:%d: Found %d records with key: %s in Wifi Device table\n", 
			__func__, __LINE__, count, vap_info->vap_name);
	//strcpy(cfg->onboard_type, "manual");
	cfg->security_mode = vap_info->u.bss_info.security.mode;
	cfg->encryption_method = vap_info->u.bss_info.security.encr;
	if (vap_info->u.bss_info.security.mode < wifi_security_mode_wpa_enterprise) {
		//strcpy(cfg->passphrase, vap_info->u.bss_info.security.u.key.key);
		cfg->radius_server_ip[0] = '\0';
		cfg->radius_server_port = 1812;
		cfg->radius_server_key[0] = '\0';
		cfg->secondary_radius_server_ip[0] = '\0';
		cfg->secondary_radius_server_port = 1812;
		cfg->secondary_radius_server_key[0] = '\0';
	} else {
		//strcpy(cfg->passphrase, "");
		cfg->radius_server_port = vap_info->u.bss_info.security.u.radius.port;
		rc = strcpy_s(cfg->radius_server_key, sizeof(cfg->radius_server_key), vap_info->u.bss_info.security.u.radius.key);
		ERR_CHK(rc);
		cfg->secondary_radius_server_port = vap_info->u.bss_info.security.u.radius.s_port;
		rc = strcpy_s(cfg->secondary_radius_server_key, sizeof(cfg->secondary_radius_server_key), vap_info->u.bss_info.security.u.radius.s_key);
		ERR_CHK(rc);
#ifndef WIFI_HAL_VERSION_3_PHASE2
        if (strlen((char *)vap_info->u.bss_info.security.u.radius.ip) != 0)
        {
            rc = strcpy_s(cfg->radius_server_ip, sizeof(cfg->radius_server_ip), (char *)vap_info->u.bss_info.security.u.radius.ip);
        }
        else 
        {    
            rc = strcpy_s(cfg->radius_server_ip, sizeof(cfg->radius_server_ip), "0.0.0.0");
        }
        ERR_CHK(rc);
        if (strlen((char *)vap_info->u.bss_info.security.u.radius.s_ip) != 0)
        {
            rc = strcpy_s(cfg->secondary_radius_server_ip, sizeof(cfg->secondary_radius_server_ip), (char *)vap_info->u.bss_info.security.u.radius.s_ip);
        }
        else
        {
            rc = strcpy_s(cfg->secondary_radius_server_ip, sizeof(cfg->secondary_radius_server_ip), "0.0.0.0");
        }
        ERR_CHK(rc);
#else
        if(inet_ntop(AF_INET, &(vap_info->u.bss_info.security.u.radius.ip.u.IPv4addr), cfg->radius_server_ip,
                                    sizeof(cfg->radius_server_ip)) == NULL)
        {
			wifi_passpoint_dbg_print("%s:%d: failed to convert radius IPv4 address\n",
				__func__, __LINE__);
			return -1;
        }
        else if(inet_ntop(AF_INET6, &(vap_info->u.bss_info.security.u.radius.ip.u.IPv6addr), cfg->radius_server_ip,
                                    sizeof(cfg->radius_server_ip)) == NULL)
        {
           wifi_passpoint_dbg_print("%s:%d: failed to convert radius IPv6 address\n",
           	__func__, __LINE__);
           return -1;
        }
        
        if(inet_ntop(AF_INET, &(vap_info->u.bss_info.security.u.radius.s_ip.u.IPv4addr), cfg->secondary_radius_server_ip,
                                    sizeof(cfg->secondary_radius_server_ip)) == NULL)
        {
           wifi_passpoint_dbg_print("%s:%d: failed to convert radius IPv4 address\n",
                   __func__, __LINE__);
           return -1;
        }
        else if(inet_ntop(AF_INET6, &(vap_info->u.bss_info.security.u.radius.s_ip.u.IPv6addr), cfg->secondary_radius_server_ip,
                                    sizeof(cfg->secondary_radius_server_ip)) == NULL)
        {
           wifi_passpoint_dbg_print("%s:%d: failed to convert radius IPv6 address\n",
                   __func__, __LINE__);
           return -1;
        }
#endif
	}
	if (update == true) {
		where = ovsdb_tran_cond(OCLM_UUID, "_uuid", OFUNC_EQ, cfg->_uuid.uuid);
		ret = ovsdb_table_update_where(g_wifidb.ovsdb_sock_path, &table_Wifi_Security_Config, where, cfg);
		if (ret == -1) {
			wifi_db_dbg_print(1,"%s:%d: failed to update to table_Wifi_Security_Config table\n", 
					__func__, __LINE__);
			return -1;
		} else if (ret == 0) {
			wifi_db_dbg_print(1,"%s:%d: nothing to update to table_Wifi_Security_Config table\n", 
				__func__, __LINE__);
		} else {
			wifi_db_dbg_print(1,"%s:%d: update to table_Wifi_Security_Config table successful\n", 
				__func__, __LINE__);
		}
	} else {
    	if (ovsdb_table_insert(g_wifidb.ovsdb_sock_path, &table_Wifi_Security_Config, cfg) 
				== false) {
			wifi_db_dbg_print(1,"%s:%d: failed to insert in table_Wifi_Security_Config table\n", 
				__func__, __LINE__);
			return -1;
		} else {
			wifi_db_dbg_print(1,"%s:%d: insert in table_Wifi_Security_Config table successful\n", 
				__func__, __LINE__);
		}
	}
	return 0;
}
	
/*int update_wifi_ovsdb_mac_filter(wifi_vap_info_t *vap_info, hash_map_t *mac_filter_map)
{
	int ret;
	int count;
    struct schema_Wifi_Device_Config cfg;
	//mac_filter_data_t *filter_data, *tmp;
	json_t *where;
	// remove all filters from the table and insert
	where = ovsdb_tran_cond(OCLM_STR, "vap_name", OFUNC_EQ, vap_info->vap_name);
	ret = ovsdb_table_delete_where(g_wifidb.ovsdb_sock_path, &table_Wifi_Device_Config, where);
	if (ret == -1) {
		CcspWifiTrace(("%s:%d: failed to delete from table_Wifi_Device_Config table\n", 
			__func__, __LINE__);
		return -1;
	} else if (ret == 0) {
		CcspWifiTrace(("%s:%d: nothing to delete from table_Wifi_Device_Config table\n", 
			__func__, __LINE__);
	} else {
		CcspWifiTrace(("%s:%d: delete from table_Wifi_Device_Config table successful\n", 
			__func__, __LINE__);
	}
	filter_data = hash_map_get_first(mac_filter_map);
	while (filter_data != NULL) {
		strcpy(cfg.vap_name, vap_info->vap_name);
		strcpy(cfg.device_name, filter_data->dev_name);
		strcpy(cfg.device_mac, filter_data->mac);
    	if (ovsdb_table_insert(g_wifidb.ovsdb_sock_path, &table_Wifi_Device_Config, &cfg) == false) {
			CcspWifiTrace(("%s:%d: failed to insert in table_Wifi_Device_Config table\n", 
				__func__, __LINE__);
			return -1;
		} else {
			CcspWifiTrace(("%s:%d: insert in table_Wifi_Device_Config table successful\n", 
				__func__, __LINE__);
		}
		tmp = filter_data;
		filter_data = hash_map_get_next(mac_filter_map, filter_data);
		free(tmp);
	}
	where = ovsdb_tran_cond(OCLM_STR, "vap_name", OFUNC_EQ, vap_info->vap_name);
    ovsdb_table_select_where(g_wifidb.ovsdb_sock_path, &table_Wifi_Device_Config, where, &count);
	CcspWifiTrace(("%s:%d: mac filter count in table_Wifi_Device_Config table:%d \n", 
		__func__, __LINE__, count);
	return 0;
}*/
int update_ovsdb_interworking(char *vap_name, wifi_InterworkingElement_t *interworking)
{
    struct schema_Wifi_Interworking_Config cfg, *pcfg;
    
    json_t *where;
    bool update = false;
    int count;
    int ret;
    errno_t rc = -1;
    where = ovsdb_tran_cond(OCLM_STR, "vap_name", OFUNC_EQ, vap_name);
    pcfg = ovsdb_table_select_where(g_wifidb.ovsdb_sock_path, &table_Wifi_Interworking_Config, where, &count);
	if ((count != 0) && (pcfg != NULL)) {
		//assert(count == 1);
		memcpy(&cfg, pcfg, sizeof(struct schema_Wifi_Interworking_Config));
		update = true;
		free(pcfg);
	}
	wifi_db_dbg_print(1,"%s:%d: Found %d records with key: %s in Wifi VAP table\n", 
    	__func__, __LINE__, count, vap_name);
	rc = strcpy_s(cfg.vap_name, sizeof(cfg.vap_name), vap_name);
	ERR_CHK(rc);
        cfg.enable = interworking->interworkingEnabled;
        cfg.access_network_type = interworking->accessNetworkType;
        cfg.internet = interworking->internetAvailable;
        cfg.asra = interworking->asra;
        cfg.esr = interworking->esr;
        cfg.uesa = interworking->uesa;
        cfg.hess_option_present = interworking->hessOptionPresent;
	rc = strcpy_s(cfg.hessid, sizeof(cfg.hessid), interworking->hessid);
	ERR_CHK(rc);
        cfg.venue_group = interworking->venueGroup;
        cfg.venue_type = interworking->venueType;
	if (update == true) {
		where = ovsdb_tran_cond(OCLM_STR, "vap_name", OFUNC_EQ, vap_name);
    	ret = ovsdb_table_update_where(g_wifidb.ovsdb_sock_path, &table_Wifi_Interworking_Config, where, &cfg);
		if (ret == -1) {
			wifi_db_dbg_print(1,"%s:%d: failed to update table_Wifi_Interworking_Config table\n", 
				__func__, __LINE__);
			return -1;
		} else if (ret == 0) {
			wifi_db_dbg_print(1,"%s:%d: nothing to update table_Wifi_Interworking_Config table\n", 
				__func__, __LINE__);
		} else {
			wifi_db_dbg_print(1,"%s:%d: update to table_Wifi_Interworking_Config table successful\n", 
				__func__, __LINE__);
		}
	} else {
    	if (ovsdb_table_insert(g_wifidb.ovsdb_sock_path, &table_Wifi_Interworking_Config, &cfg) == false) {
			wifi_db_dbg_print(1,"%s:%d: failed to insert in table_Wifi_Interworking_Config\n", 
				__func__, __LINE__);
			return -1;
		} else {
			wifi_db_dbg_print(1,"%s:%d: insert in table_Wifi_Interworking_Config table successful\n", 
				__func__, __LINE__);
		}
	}
	return 0;
}
int get_ovsdb_interworking_config(char *vap_name, wifi_InterworkingElement_t *interworking)
{
    struct schema_Wifi_Interworking_Config  *pcfg;
    json_t *where;
    int count;
    where = ovsdb_tran_cond(OCLM_STR, "vap_name", OFUNC_EQ, vap_name);
    pcfg = ovsdb_table_select_where(g_wifidb.ovsdb_sock_path, &table_Wifi_Interworking_Config, where, &count);
    if (pcfg == NULL) {
        wifi_db_dbg_print(1,"%s:%d: table not table_Wifi_Interworking_Config not found\n",__func__, __LINE__);
        return -1;
    }
    interworking->interworkingEnabled = pcfg->enable;
    interworking->accessNetworkType = pcfg->access_network_type;
    interworking->internetAvailable = pcfg->internet;
    interworking->asra = pcfg->asra;
    interworking->esr = pcfg->esr;
    interworking->uesa = pcfg->uesa;
    interworking->hessOptionPresent = pcfg->hess_option_present;
    strncpy(interworking->hessid, pcfg->hessid, sizeof(interworking->hessid)-1);
    interworking->venueGroup = pcfg->venue_group;
    interworking->venueType = pcfg->venue_type;
    return 0;
}
void print_ovsdb_interworking_config ()
{
    struct schema_Wifi_Interworking_Config  *pcfg;
    json_t *where;
    int count;
#ifndef WIFI_HAL_VERSION_3
    int  i;
    char *vap_name[] = {"private_ssid_2g", "private_ssid_5g", "iot_ssid_2g", "iot_ssid_5g", "hotspot_open_2g", "hotspot_open_5g", "lnf_psk_2g", "lnf_psk_5g", "hotspot_secure_2g", "hotspot_secure_5g"};
#endif
    char output[4096];
    wifi_passpoint_dbg_print("OVSDB JSON\nname:Open_vSwitch, version:1.00.000\n");
    wifi_passpoint_dbg_print("table: Wifi_Interworking_Config \n");
#ifdef WIFI_HAL_VERSION_3
    for (UINT apIndex = 0; apIndex < getTotalNumberVAPs(); apIndex++) {
        if ((isVapPrivate(apIndex) || isVapXhs(apIndex) || isVapHotspot(apIndex) || isVapLnfPsk(apIndex)) )
        {
            where = ovsdb_tran_cond(OCLM_STR, "vap_name", OFUNC_EQ, getVAPName(apIndex));
#else
    for (i=0; i < 10; i++) {
        where = ovsdb_tran_cond(OCLM_STR, "vap_name", OFUNC_EQ, vap_name[i]);
#endif
        pcfg = ovsdb_table_select_where(g_wifidb.ovsdb_sock_path, &table_Wifi_Interworking_Config, where, &count);
    
        if ((pcfg == NULL) || (!count)) {
            continue;
            //wifi_db_dbg_print(1,"%s:%d: table table_Wifi_Interworking_Config not found\n",__func__, __LINE__);
            //return;
        }
        //wifi_db_dbg_print(1,"%s:%d: Found %d records with key: %s in Wifi_Interworking_Config table\n",
        //__func__, __LINE__, count, vap_name[i]);
        json_t *data_base = ovsdb_table_to_json(&table_Wifi_Interworking_Config, pcfg);
        if(data_base) {
            memset(output,0,sizeof(output));
            if(json_get_str(data_base,output, sizeof(output))) {
#ifdef WIFI_HAL_VERSION_3
                wifi_passpoint_dbg_print("key: %s\nCount: %d\n%s\n",
                   getVAPName(apIndex),count,output);
#else
                wifi_passpoint_dbg_print("key: %s\nCount: %d\n%s\n",
                   vap_name[i],count,output);
#endif
            } else {
                wifi_db_dbg_print(1,"%s:%d: Unable to print Row\n",
                   __func__, __LINE__);
            }
        }
#ifdef WIFI_HAL_VERSION_3
        }
#endif
    }
}
/*int update_wifi_ovsdb_vap_map(wifi_vap_info_map_t *vap_map)
{
	wifi_vap_info_t *vap_info;
    unsigned int i;
	for (i = 0; i < vap_map->num_vaps; i++) {
		vap_info = &vap_map->vap_array[i];
		CcspWifiTrace(("%s:%d:updating ovsdb of vap:%s\n", __func__, __LINE__, vap_info->vap_name);
		if (update_wifi_ovsdb_vap(vap_info) != 0) {
			return -1;
		}		
	}
    return 0;
}*/

/*
int device_config_update_test()
{
    struct schema_Wifi_Device_Config cfg;
    int ret;
    memset(&cfg, 0, sizeof(cfg));
	strcpy(cfg.vap_name, "private_ssid_2g");
	strcpy(cfg.device_name, "test_dev");
	strcpy(cfg.device_mac, "00:11:22:33:44:55");
    ret = ovsdb_table_upsert_simple(g_wifidb.ovsdb_sock_path, &table_Wifi_Device_Config,
                                   SCHEMA_COLUMN(Wifi_Device_Config, device_mac),
                                   cfg.device_mac,
                                   &cfg,
                                   NULL);
    if (!ret) {
        CcspWifiTrace(("%s:%d:Insert new row failed for %s", __func__, __LINE__, cfg.vap_name));
	}
	return 0;
}
int vap_config_update_test()
{
    struct schema_Wifi_VAP_Config cfg;
    int ret;
    memset(&cfg, 0, sizeof(cfg));
	strcpy(cfg.vap_name, "private_ssid_2g");
	strcpy(cfg.ssid, "sams_home_2g");
    ret = ovsdb_table_upsert_simple(g_wifidb.ovsdb_sock_path, &table_Wifi_VAP_Config,
                                   SCHEMA_COLUMN(Wifi_VAP_Config, vap_name),
                                   cfg.vap_name,
                                   &cfg,
                                   NULL);
    if (!ret) {
        CcspWifiTrace(("%s:%d:Insert new row failed for %s", __func__, __LINE__, cfg.vap_name));
	}
    return ret;
}
*/
int update_ovsdb_gas_config(UINT advertisement_id, wifi_GASConfiguration_t *gas_info)
{
    struct schema_Wifi_GAS_Config cfg, *pcfg;
    
    json_t *where;
    bool update = false;
    int count;
    int ret;
    char index[4] = {0};
    errno_t rc = -1;
    rc = sprintf_s(index, sizeof(index), "%d",advertisement_id);
    if(rc < EOK)
    {
        ERR_CHK(rc);
    }
    where = ovsdb_tran_cond(OCLM_STR, "advertisement_id", OFUNC_EQ, index);
    pcfg = ovsdb_table_select_where(g_wifidb.ovsdb_sock_path, &table_Wifi_GAS_Config, where, &count);
    if ((count != 0) && (pcfg != NULL)) {
	//assert(count == 1);
        wifi_db_dbg_print(1,"%s:%d: Found %d records with key: %d in Wifi GAS table\n", 
    	__func__, __LINE__, count, advertisement_id);
	memcpy(&cfg, pcfg, sizeof(struct schema_Wifi_GAS_Config));
	update = true;
	free(pcfg);
    }
    cfg.pause_for_server_response = gas_info->PauseForServerResponse;
    cfg.response_timeout = gas_info->ResponseTimeout;
    cfg.comeback_delay = gas_info->ComeBackDelay;
    cfg.response_buffering_time = gas_info->ResponseBufferingTime;
    cfg.query_responselength_limit = gas_info->QueryResponseLengthLimit;
    if (update == true) {
        where = ovsdb_tran_cond(OCLM_STR, "advertisement_id", OFUNC_EQ, index); 
        ret = ovsdb_table_update_where(g_wifidb.ovsdb_sock_path, &table_Wifi_GAS_Config, where, &cfg);
	if (ret == -1) {
	    wifi_db_dbg_print(1,"%s:%d: failed to update table_Wifi_GAS_Config table\n", 
		__func__, __LINE__);
	    return -1;
	} else if (ret == 0) {
	    wifi_db_dbg_print(1,"%s:%d: nothing to update table_Wifi_GAS_Config table\n", 
		__func__, __LINE__);
	} else {
	    wifi_db_dbg_print(1,"%s:%d: update to table_Wifi_GAS_Config table successful\n", 
		__func__, __LINE__);
	}
    } else {
	rc = strcpy_s(cfg.advertisement_id, sizeof(cfg.advertisement_id), index);
	ERR_CHK(rc);
        if (ovsdb_table_upsert_simple(g_wifidb.ovsdb_sock_path, &table_Wifi_GAS_Config, 
                                  SCHEMA_COLUMN(Wifi_GAS_Config, advertisement_id),
                                  cfg.advertisement_id,
                                  &cfg, NULL) == false) {
	    wifi_db_dbg_print(1,"%s:%d: failed to insert in table_Wifi_GAS_Config\n", 
		__func__, __LINE__);
	    return -1;
	} else {
	    wifi_db_dbg_print(1,"%s:%d: insert in table_Wifi_GAS_Config table successful\n", 
		__func__, __LINE__);
 	}
    }
    return 0;
}

int get_ovsdb_gas_config(UINT advertisement_id, wifi_GASConfiguration_t *gas_info)
{
    struct schema_Wifi_GAS_Config  *pcfg;
    json_t *where;
    int count;
    char index[4] = {0};
    errno_t rc = -1;
    rc = sprintf_s(index, sizeof(index), "%d",advertisement_id);
    if(rc < EOK)
    {
        ERR_CHK(rc);
    }
    where = ovsdb_tran_cond(OCLM_STR, "advertisement_id", OFUNC_EQ, index);
    pcfg = ovsdb_table_select_where(g_wifidb.ovsdb_sock_path, &table_Wifi_GAS_Config, where, &count);
    if (pcfg == NULL) {
        wifi_db_dbg_print(1,"%s:%d: table table_Wifi_GAS_Config not found\n",__func__, __LINE__);
        return -1;
    }
    gas_info->AdvertisementID = atoi(pcfg->advertisement_id);
    gas_info->PauseForServerResponse = pcfg->pause_for_server_response;
    gas_info->ResponseTimeout = pcfg->response_timeout;
    gas_info->ComeBackDelay = pcfg->comeback_delay;
    gas_info->ResponseBufferingTime = pcfg->response_buffering_time;
    gas_info->QueryResponseLengthLimit = pcfg->query_responselength_limit;
    return 0;
}

#define MAX_RADIO_OVSDB 2
#define BUFFER_LENGTH_OVSDB 32

int convert_radio_to_name(int index,char *name)
{
    if(index == 1)
    {
        strncpy(name,"radio1",BUFFER_LENGTH_OVSDB);
	return 0;
    }
    else
    {
        strncpy(name,"radio2",BUFFER_LENGTH_OVSDB);
	return 0;
    }
    return -1;
}

int convert_radio_name_to_index(int *index,char *name)
{
    if((strncmp(name,"radio1",BUFFER_LENGTH_OVSDB))== 0)
    {
        *index = 1;
        return 0;
    }
    else
    {
        *index = 2;
        return 0;
    } 
    return -1;
}

int wifidb_update_wifi_radio_config(int radio_index, wifi_radio_operationParam_t *config)
{
    struct schema_Wifi_Radio_Config cfg;
    char name[BUFFER_LENGTH_OVSDB] = {0};
    char *insert_filter[] = {"-",SCHEMA_COLUMN(Wifi_Radio_Config,vap_configs),NULL};
    unsigned int i = 0;
    int k = 0;
    int len = 0;
    char channel_list[BUFFER_LENGTH_OVSDB] = {0};
    len = sizeof(channel_list)-1;

    memset(&cfg,0,sizeof(cfg));
    wifi_db_dbg_print(1,"%s:%d:Update Radio Config for radio_index=%d \n",__func__, __LINE__,radio_index);
    if((config == NULL) || (convert_radio_to_name(radio_index,name)!=0))
    {
        wifi_db_dbg_print(1,"%s:%d: Radio Config update failed \n",__func__, __LINE__);
        return -1;
    }
    cfg.enabled = config->enable;
    cfg.freq_band = config->band;
    cfg.auto_channel_enabled = config->autoChannelEnabled;
    cfg.channel = config->channel;
    cfg.channel_width = config->channelWidth;
    cfg.hw_mode = config->variant;
    cfg.csa_beacon_count = config->csa_beacon_count;
    cfg.country = config->countryCode;
    cfg.dcs_enabled = config->DCSEnabled;
    cfg.dtim_period = config->dtimPeriod;
    cfg.beacon_interval = config->beaconInterval;
    cfg.operating_class = config->operatingClass;
    cfg.basic_data_transmit_rate = config->basicDataTransmitRates;
    cfg.operational_data_transmit_rate = config->operationalDataTransmitRates;
    cfg.fragmentation_threshold = config->fragmentationThreshold;
    cfg.guard_interval = config->guardInterval;
    cfg.transmit_power = config->transmitPower;
    cfg.rts_threshold = config->rtsThreshold;
    cfg.factory_reset_ssid = config->factoryResetSsid;
    cfg.radio_stats_measuring_rate = config->radioStatsMeasuringRate;
    cfg.radio_stats_measuring_interval = config->radioStatsMeasuringInterval;
    cfg.cts_protection = config->ctsProtection;
    cfg.obss_coex = config->obssCoex;
    cfg.stbc_enable = config->stbcEnable;
    cfg.greenfield_enable = config->greenFieldEnable;
    cfg.user_control = config->userControl;
    cfg.admin_control = config->adminControl;
    cfg.chan_util_threshold = config->chanUtilThreshold;
    cfg.chan_util_selfheal_enable = config->chanUtilSelfHealEnable;

    for(i=0;i<(config->numSecondaryChannels);i++)
    {
	if(k >= (len-1))
	{
	    wifi_db_dbg_print(1,"%s:%d Wifi_Radio_Config table Maximum size reached for secondary_channels_list\n",__func__, __LINE__);
	    break;
	}
        snprintf(channel_list+k,sizeof(channel_list)-k,"%d,",config->channelSecondary[i]);
        wifi_db_dbg_print(1,"%s:%d Wifi_Radio_Config table Channel list %s %d\t",__func__, __LINE__,channel_list,strlen(channel_list));
        k = strlen(channel_list);
    }
    strncpy(cfg.secondary_channels_list,channel_list,sizeof(cfg.secondary_channels_list)-1);
    cfg.num_secondary_channels = config->numSecondaryChannels;
    strncpy(cfg.radio_name,name,sizeof(cfg.radio_name)-1);

    wifi_db_dbg_print(1,"%s:%d: Wifi_Radio_Config data enabled=%d freq_band=%d auto_channel_enabled=%d channel=%d  channel_width=%d hw_mode=%d csa_beacon_count=%d country=%d dcs_enabled=%d numSecondaryChannels=%d channelSecondary=%s dtim_period %d beacon_interval %d operating_class %d basic_data_transmit_rate %d operational_data_transmit_rate %d  fragmentation_threshold %d guard_interval %d transmit_power %d rts_threshold %d  \n",__func__, __LINE__,config->enable,config->band,config->autoChannelEnabled,config->channel,config->channelWidth,config->variant,config->csa_beacon_count,config->countryCode,config->DCSEnabled,config->numSecondaryChannels,cfg.secondary_channels_list,config->dtimPeriod,config->beaconInterval,config->operatingClass,config->basicDataTransmitRates,config->operationalDataTransmitRates,config->fragmentationThreshold,config->guardInterval,config->transmitPower,config->rtsThreshold);

    if(ovsdb_table_upsert_f(g_wifidb.ovsdb_sock_path,&table_Wifi_Radio_Config,&cfg,false,insert_filter) == false)
    {
        wifi_db_dbg_print(1,"%s:%d: failed to insert Wifi_Radio_Config table\n",__func__, __LINE__);
	return -1;
    }
    else
    {
        wifi_db_dbg_print(1,"%s:%d: Insert Wifi_Radio_Config table complete\n",__func__, __LINE__);
    }

    return 0;
}

int wifidb_get_wifi_radio_config(int radio_index, wifi_radio_operationParam_t *config)
{
    struct schema_Wifi_Radio_Config *cfg;
    json_t *where;
    int count;
    char name[BUFFER_LENGTH_OVSDB] = {0};
    int i = 0;
    char *tmp, *ptr;

    if((config == NULL) || (convert_radio_to_name(radio_index,name)!=0))
    {
        wifi_db_dbg_print(1,"%s:%d:Get Radio Config  failed \n",__func__, __LINE__);
        return -1;
    }
    wifi_db_dbg_print(1,"%s:%d:Get radio config for index=%d radio_name=%s \n",__func__, __LINE__,radio_index,name);

    where = ovsdb_tran_cond(OCLM_STR, "radio_name", OFUNC_EQ, name);
    cfg = ovsdb_table_select_where(g_wifidb.ovsdb_sock_path, &table_Wifi_Radio_Config, where, &count);
    if(cfg == NULL)
    {
        wifi_db_dbg_print(1,"%s:%d: table not table_Wifi_Radio_Config not found\n",__func__, __LINE__);
        return -1;
    }
    config->enable = cfg->enabled;
    config->band = cfg->freq_band;
    config->autoChannelEnabled = cfg->auto_channel_enabled;
    config->channel = cfg->channel;
    config->channelWidth = cfg->channel_width;
    config->variant = cfg->hw_mode;
    config->csa_beacon_count = cfg->csa_beacon_count;
    config->countryCode = cfg->country;
    config->DCSEnabled = cfg->dcs_enabled;
    config->dtimPeriod = cfg->dtim_period;
    config->beaconInterval = cfg->beacon_interval;
    config->operatingClass = cfg->operating_class;
    config->basicDataTransmitRates = cfg->basic_data_transmit_rate;
    config->operationalDataTransmitRates = cfg->operational_data_transmit_rate;
    config->fragmentationThreshold = cfg->fragmentation_threshold;
    config->guardInterval = cfg->guard_interval;
    config->transmitPower = cfg->transmit_power;
    config->rtsThreshold = cfg->rts_threshold;
    tmp = cfg->secondary_channels_list;
    while ((ptr = strchr(tmp, ',')) != NULL)
    {
        ptr++;
        wifi_db_dbg_print(1,"%s:%d: Wifi_Radio_Config Secondary Channel list %d \t",__func__, __LINE__,atoi(tmp));
	config->channelSecondary[i] = atoi(tmp);
        tmp = ptr;
	i++;
    }
    config->numSecondaryChannels = cfg->num_secondary_channels;

    wifi_db_dbg_print(1,"%s:%d: Wifi_Radio_Config data enabled=%d freq_band=%d auto_channel_enabled=%d channel=%d  channel_width=%d hw_mode=%d csa_beacon_count=%d country=%d dcs_enabled=%d numSecondaryChannels=%d channelSecondary=%s dtim_period %d beacon_interval %d operating_class %d basic_data_transmit_rate %d operational_data_transmit_rate %d  fragmentation_threshold %d guard_interval %d transmit_power %d rts_threshold %d  \n",__func__, __LINE__,config->enable,config->band,config->autoChannelEnabled,config->channel,config->channelWidth,config->variant,config->csa_beacon_count,config->countryCode,config->DCSEnabled,config->numSecondaryChannels,cfg->secondary_channels_list,config->dtimPeriod,config->beaconInterval,config->operatingClass,config->basicDataTransmitRates,config->operationalDataTransmitRates,config->fragmentationThreshold,config->guardInterval,config->transmitPower,config->rtsThreshold);
    free(cfg);
    return 0;
}

int wifidb_get_wifi_vap_config(int radio_index,wifi_vap_info_map_t *config)
{
    struct schema_Wifi_VAP_Config *pcfg;
    json_t *where;
    char name[BUFFER_LENGTH_OVSDB] = {0};
    char vap_name[BUFFER_LENGTH_OVSDB] = {0};
    int i =0;
    int vap_count = 0;
    void* pConfigInterworking = NULL;

    if((config == NULL) || (convert_radio_to_name(radio_index,name)!=0))
    {
        wifi_db_dbg_print(1,"%s:%d:Get VAP Config failed \n",__func__, __LINE__);
        return -1;
    }

    where = ovsdb_tran_cond(OCLM_STR, "radio_name", OFUNC_EQ, name);
    pcfg = ovsdb_table_select_where(g_wifidb.ovsdb_sock_path, &table_Wifi_VAP_Config, where, &vap_count);
    wifi_db_dbg_print(1,"%s:%d:VAP Config get index=%d radio_name=%s \n",__func__, __LINE__,radio_index,name);
    if((pcfg == NULL) || (vap_count== 0))
    {
        wifi_db_dbg_print(1,"%s:%d: table_Wifi_VAP_Config not found count=%d\n",__func__, __LINE__,vap_count);
        return -1;
    }
    for (i = 0; i < vap_count; i++)
    {
        if(pcfg != NULL)
        {
            wifi_db_dbg_print(1,"%s:%d:VAP Config Row=%d radio_name=%s vap_name=%s ssid=%s enabled=%d ssid_advertisement_enable=%d isolation_enabled=%d mgmt_power_control=%d bss_max_sta =%d bss_transition_activated=%d nbr_report_activated=%d  rapid_connect_enabled=%d rapid_connect_threshold=%d vap_stats_enable=%d mac_filter_enabled =%d mac_filter_mode=%d  wmm_enabled=%d \n",__func__, __LINE__,i,(pcfg+i)->radio_name,(pcfg+i)->vap_name,(pcfg+i)->ssid,(pcfg+i)->enabled,(pcfg+i)->ssid_advertisement_enabled,(pcfg+i)->isolation_enabled,(pcfg+i)->mgmt_power_control,(pcfg+i)->bss_max_sta,(pcfg+i)->bss_transition_activated,(pcfg+i)->nbr_report_activated,(pcfg+i)->rapid_connect_enabled,(pcfg+i)->rapid_connect_threshold,(pcfg+i)->vap_stats_enable,(pcfg+i)->mac_filter_enabled,(pcfg+i)->mac_filter_mode,(pcfg+i)->wmm_enabled);

            strncpy(vap_name,(pcfg+i)->vap_name,sizeof(vap_name));
            config->vap_array[i].radio_index = radio_index;
            wifidb_get_wifi_vap_info(vap_name,&config->vap_array[i]);
            wifi_db_dbg_print(1,"%s:%d:VAP Config Row=%d radio_name=%s radioindex=%d vap_name=%s ssid=%s enabled=%d ssid_advertisement_enable=%d isolation_enabled=%d mgmt_power_control=%d bss_max_sta =%d bss_transition_activated=%d nbr_report_activated=%d  rapid_connect_enabled=%d rapid_connect_threshold=%d vap_stats_enable=%d mac_filter_enabled =%d mac_filter_mode=%d wmm_enabled=%d \n",__func__, __LINE__,i,name,config->vap_array[i].radio_index,config->vap_array[i].vap_name,config->vap_array[i].u.bss_info.ssid,config->vap_array[i].u.bss_info.enabled,config->vap_array[i].u.bss_info.showSsid ,config->vap_array[i].u.bss_info.isolation,config->vap_array[i].u.bss_info.mgmtPowerControl,config->vap_array[i].u.bss_info.bssMaxSta,config->vap_array[i].u.bss_info.bssTransitionActivated,config->vap_array[i].u.bss_info.nbrReportActivated,config->vap_array[i].u.bss_info.rapidReconnectEnable,config->vap_array[i].u.bss_info.rapidReconnThreshold,config->vap_array[i].u.bss_info.vapStatsEnable,config->vap_array[i].u.bss_info.mac_filter_enable,config->vap_array[i].u.bss_info.mac_filter_mode,config->vap_array[i].u.bss_info.wmm_enabled);

            pConfigInterworking = &config->vap_array[i].u.bss_info.interworking.interworking;
            get_ovsdb_interworking_config(vap_name, pConfigInterworking);
            wifi_db_dbg_print(1,"%s:%d: Get Wifi_Interworking_Config table vap_name=%s Enable=%d accessNetworkType=%d internetAvailable=%d asra=%d esr=%d uesa=%d hess_present=%d hessid=%s venueGroup=%d venueType=%d \n",__func__, __LINE__,vap_name,config->vap_array[i].u.bss_info.interworking.interworking.interworkingEnabled,config->vap_array[i].u.bss_info.interworking.interworking.accessNetworkType,config->vap_array[i].u.bss_info.interworking.interworking.internetAvailable,config->vap_array[i].u.bss_info.interworking.interworking.asra,config->vap_array[i].u.bss_info.interworking.interworking.esr,config->vap_array[i].u.bss_info.interworking.interworking.uesa,config->vap_array[i].u.bss_info.interworking.interworking.hessOptionPresent,config->vap_array[i].u.bss_info.interworking.interworking.hessid,config->vap_array[i].u.bss_info.interworking.interworking.venueGroup,config->vap_array[i].u.bss_info.interworking.interworking.venueType);

            wifidb_get_wifi_security_config(vap_name,&config->vap_array[i].u.bss_info.security);

            if (config->vap_array[i].u.bss_info.security.mode < wifi_security_mode_wpa_enterprise)
            {
                wifi_db_dbg_print(1,"%s:%d: Get Wifi_Security_Config table sec type=%d  sec key=%s \n",__func__, __LINE__,config->vap_array[i].u.bss_info.security.u.key.type,config->vap_array[i].u.bss_info.security.u.key.key,config->vap_array[i].u.bss_info.security.u.key.type,config->vap_array[i].u.bss_info.security.u.key.key);
            }
            else
            {
                wifi_db_dbg_print(1,"%s:%d: Get Wifi_Security_Config table radius server ip =%s  port =%d sec key=%s Secondary radius server ip=%s port=%d key=%s\n",__func__, __LINE__,config->vap_array[i].u.bss_info.security.u.radius.ip,config->vap_array[i].u.bss_info.security.u.radius.port,config->vap_array[i].u.bss_info.security.u.radius.key,config->vap_array[i].u.bss_info.security.u.radius.s_ip,config->vap_array[i].u.bss_info.security.u.radius.s_port,config->vap_array[i].u.bss_info.security.u.radius.s_key);
            }
#ifdef WIFI_HAL_VERSION_3
            wifi_db_dbg_print(1,"%s:%d: Get Wifi_Security_Config table vap_name=%s Sec_mode=%d enc_mode=%d mfp_config=%s\n",__func__, __LINE__,vap_name,config->vap_array[i].u.bss_info.security.mode,config->vap_array[i].u.bss_info.security.encr,MFPConfig[config->vap_array[i].u.bss_info.security.mfp]);
#else
            wifi_db_dbg_print(1,"%s:%d: Get Wifi_Security_Config table vap_name=%s Sec_mode=%d enc_mode=%d mfp_config=%s\n",__func__, __LINE__,vap_name,config->vap_array[i].u.bss_info.security.mode,config->vap_array[i].u.bss_info.security.encr,config->vap_array[i].u.bss_info.security.mfpConfig);
#endif
        }
    }
    free(pcfg);
    config->num_vaps = i+1;
    wifi_db_dbg_print(1,"%s:%d:VAP Config get index=%d radio_name=%s complete \n",__func__, __LINE__,radio_index,name);
    return 0;
}

int wifidb_update_wifi_vap_config(int radio_index, wifi_vap_info_map_t *config)
{
    unsigned int i = 0;
    char name[BUFFER_LENGTH_OVSDB];
    int ret = 0;

    wifi_db_dbg_print(1,"%s:%d:VAP Config update for radio index=%d No of Vaps=%d\n",__func__, __LINE__,radio_index,config->num_vaps);
    if((config == NULL) || (convert_radio_to_name(radio_index,name)!=0))
    {
        wifi_db_dbg_print(1,"%s:%d:Null pointer VAP Config update failed \n",__func__, __LINE__);
        return -1;
    }
    for(i=0;i<config->num_vaps;i++)
    {
        wifi_db_dbg_print(1,"%s:%d:Update radio=%s vap name=%s \n",__func__, __LINE__,name,config->vap_array[i].vap_name);
        if (wifidb_update_wifi_vap_info(name,&config->vap_array[i]) != 0)
        {
            wifi_db_dbg_print(1,"%s:%d:Update radio=%s vap name=%s failed to update vapinfo\n",__func__, __LINE__,name,config->vap_array[i].vap_name);
            ret = -1;
        }

        if (wifidb_update_wifi_security_config(config->vap_array[i].vap_name,&config->vap_array[i].u.bss_info.security) != 0)
        {
            wifi_db_dbg_print(1,"%s:%d:Update radio=%s vap name=%s failed to update security\n",__func__, __LINE__,name,config->vap_array[i].vap_name);
            ret = -1;
        }
        if (wifidb_update_wifi_interworking_config(config->vap_array[i].vap_name,&config->vap_array[i].u.bss_info.interworking.interworking) != 0)
        {
            wifi_db_dbg_print(1,"%s:%d:Update radio=%s vap name=%s failed to update interworking\n",__func__, __LINE__,name,config->vap_array[i].vap_name);
            ret = -1;
        }
        if (wifidb_update_wifi_macfilter_config(config->vap_array[i].vap_index) != 0)
        {
            wifi_db_dbg_print(1,"%s:%d:Update radio=%s vap name=%s failed to update macfilter\n",__func__, __LINE__,name,config->vap_array[i].vap_name);
            ret = -1;
        }
    }
    return ret;
}

int wifidb_get_wifi_security_config(char *vap_name, wifi_vap_security_t *sec)
{
    struct schema_Wifi_Security_Config  *pcfg;
    json_t *where;
    int count;

    if(sec == NULL)
    {
        wifi_db_dbg_print(1,"%s:%d:Get table_Wifi_Security_Config failed \n",__func__, __LINE__);
        return -1;
    }

    where = ovsdb_tran_cond(OCLM_STR, "vap_name", OFUNC_EQ, vap_name);
    pcfg = ovsdb_table_select_where(g_wifidb.ovsdb_sock_path, &table_Wifi_Security_Config, where, &count);
    if (pcfg == NULL) {
        wifi_db_dbg_print(1,"%s:%d: table not table_Wifi_Security_Config not found\n",__func__, __LINE__);
        return -1;
    }
     wifi_db_dbg_print(1,"%s:%d: Get Wifi_Security_Config table Sec_mode=%d enc_mode=%d r_ser_ip=%s r_ser_port=%d r_ser_key=%s rs_ser_ip=%s rs_ser_ip sec_rad_ser_port=%d rs_ser_key=%s cfg_key_type=%d keyphrase=%s vap_name=%s\n",__func__, __LINE__,pcfg->security_mode,pcfg->encryption_method,pcfg->radius_server_ip,pcfg->radius_server_port,pcfg->radius_server_key,pcfg->secondary_radius_server_ip,pcfg->secondary_radius_server_port,pcfg->secondary_radius_server_key,pcfg->key_type,pcfg->keyphrase,pcfg->vap_name);

    sec->mode = pcfg->security_mode;
    sec->encr = pcfg->encryption_method;
    if (sec->mode < wifi_security_mode_wpa_enterprise)
    {
        sec->u.key.type = pcfg->key_type;
        strncpy(sec->u.key.key,pcfg->keyphrase,sizeof(sec->u.key.key)-1);
    }
    else
    {
        strncpy((char *)sec->u.radius.ip,pcfg->radius_server_ip,sizeof(sec->u.radius.ip)-1);
        sec->u.radius.port = pcfg->radius_server_port;
        strncpy(sec->u.radius.key,pcfg->radius_server_key,sizeof(sec->u.radius.key)-1);
        strncpy((char *)sec->u.radius.s_ip,pcfg->secondary_radius_server_ip,sizeof(sec->u.radius.s_ip)-1);
        sec->u.radius.s_port = pcfg->secondary_radius_server_port;
        strncpy(sec->u.radius.s_key,pcfg->secondary_radius_server_key,sizeof(sec->u.radius.s_key)-1);
    }
    free(pcfg);
    return 0;
}

int wifidb_get_wifi_vap_info(char *vap_name,wifi_vap_info_t *config)
{
    struct schema_Wifi_VAP_Config *pcfg;
    json_t *where;
    int count = 0;
    int index = 0;

    if(config == NULL)
    {
        wifi_db_dbg_print(1,"%s:%d:Null pointer Get VAP info failed \n",__func__, __LINE__);
        return -1;
    }

    where = ovsdb_tran_cond(OCLM_STR, "vap_name", OFUNC_EQ, vap_name);
    pcfg = ovsdb_table_select_where(g_wifidb.ovsdb_sock_path, &table_Wifi_VAP_Config, where, &count);
    wifi_db_dbg_print(1,"%s:%d:VAP Config get vap_name=%s count=%d\n",__func__, __LINE__,vap_name,count);
    if((pcfg == NULL) || (count== 0))
    {
        wifi_db_dbg_print(1,"%s:%d: table_Wifi_VAP_Config not found count=%d\n",__func__, __LINE__,count);
        return -1;
    }
    if(pcfg != NULL)
    {
        wifi_db_dbg_print(1,"%s:%d:VAP Config radio_name=%s vap_name=%s ssid=%s enabled=%d ssid_advertisement_enable=%d isolation_enabled=%d mgmt_power_control=%d bss_max_sta =%d bss_transition_activated=%d nbr_report_activated=%d  rapid_connect_enabled=%d rapid_connect_threshold=%d vap_stats_enable=%d mac_filter_enabled =%d mac_filter_mode=%d  wmm_enabled=%d \n",__func__, __LINE__,pcfg->radio_name,pcfg->vap_name,pcfg->ssid,pcfg->enabled,pcfg->ssid_advertisement_enabled,pcfg->isolation_enabled,pcfg->mgmt_power_control,pcfg->bss_max_sta,pcfg->bss_transition_activated,pcfg->nbr_report_activated,pcfg->rapid_connect_enabled,pcfg->rapid_connect_threshold,pcfg->vap_stats_enable,pcfg->mac_filter_enabled,pcfg->mac_filter_mode,pcfg->wmm_enabled);

        convert_radio_name_to_index(&index,pcfg->radio_name);
        config->radio_index = index;
        strncpy(config->vap_name, pcfg->vap_name,(sizeof(config->vap_name)-1));
        strncpy(config->u.bss_info.ssid,pcfg->ssid,(sizeof(config->u.bss_info.ssid)-1));
        config->u.bss_info.enabled = pcfg->enabled;
        config->u.bss_info.showSsid = pcfg->ssid_advertisement_enabled;
        config->u.bss_info.isolation = pcfg->isolation_enabled;
        config->u.bss_info.mgmtPowerControl = pcfg->mgmt_power_control;
        config->u.bss_info.bssMaxSta = pcfg->bss_max_sta;
        config->u.bss_info.bssTransitionActivated = pcfg->bss_transition_activated;
        config->u.bss_info.nbrReportActivated = pcfg->nbr_report_activated;
        config->u.bss_info.rapidReconnectEnable = pcfg->rapid_connect_enabled;
        config->u.bss_info.rapidReconnThreshold = pcfg->rapid_connect_threshold;
        config->u.bss_info.vapStatsEnable = pcfg->vap_stats_enable;
        config->u.bss_info.mac_filter_enable = pcfg->mac_filter_enabled;
        config->u.bss_info.mac_filter_mode = pcfg->mac_filter_mode;
        config->u.bss_info.wmm_enabled = pcfg->wmm_enabled;
    }
    free(pcfg);
    return 0;
}

int wifidb_update_wifi_interworking_config(char *vap_name, wifi_InterworkingElement_t *config)
{
    struct schema_Wifi_Interworking_Config cfg_interworking;
    char *filter_vapinterworking[] = {"-",NULL};

    memset(&cfg_interworking,0,sizeof(cfg_interworking));

    wifi_db_dbg_print(1,"%s:%d:Interworking update for vap name=%s\n",__func__, __LINE__,vap_name);
    if(config == NULL)
    {
        wifi_db_dbg_print(1,"%s:%d:Null pointer Interworking update failed \n",__func__, __LINE__);
        return -1;
    }

    cfg_interworking.enable = config->interworkingEnabled;
    cfg_interworking.access_network_type = config->accessNetworkType;
    cfg_interworking.internet = config->internetAvailable;
    cfg_interworking.asra = config->asra;
    cfg_interworking.esr = config->esr;
    cfg_interworking.uesa = config->uesa;
    cfg_interworking.hess_option_present = config->hessOptionPresent;
    strncpy(cfg_interworking.hessid,config->hessid,sizeof(cfg_interworking.hessid));
    cfg_interworking.venue_group = config->venueGroup;
    cfg_interworking.venue_type = config->venueType;
    strncpy(cfg_interworking.vap_name, vap_name,(sizeof(cfg_interworking.vap_name)-1));

    wifi_db_dbg_print(1,"%s:%d: Update Wifi_Interworking_Config table vap_name=%s Enable=%d access_network_type=%d internet=%d asra=%d esr=%d uesa=%d hess_present=%d hessid=%s venue_group=%d venue_type=%d",__func__, __LINE__,cfg_interworking.vap_name,cfg_interworking.enable,cfg_interworking.access_network_type,cfg_interworking.internet,cfg_interworking.asra,cfg_interworking.esr,cfg_interworking.uesa,cfg_interworking.hess_option_present,cfg_interworking.hessid,cfg_interworking.venue_group,cfg_interworking.venue_type);

    if(ovsdb_table_upsert_with_parent(g_wifidb.ovsdb_sock_path,&table_Wifi_Interworking_Config,&cfg_interworking,false,filter_vapinterworking,SCHEMA_TABLE(Wifi_VAP_Config),ovsdb_where_simple(SCHEMA_COLUMN(Wifi_VAP_Config,vap_name),vap_name),SCHEMA_COLUMN(Wifi_VAP_Config,interworking)) == false)
    {
        wifi_db_dbg_print(1,"%s:%d: failed to update Wifi_Interworking_Config table\n",__func__, __LINE__);
    }
    else
    {
        wifi_db_dbg_print(1,"%s:%d:  update table Wifi_Interworking_Config table successful\n",__func__, __LINE__);
    }
    return 0;
}

int decode_ipv4_address(char *ip) {
    struct sockaddr_in sa;

    if (inet_pton(AF_INET,ip, &(sa.sin_addr)) != 1 ) {
        return 1;
    }
    return 0;
}

int wifidb_update_wifi_security_config(char *vap_name, wifi_vap_security_t *sec)
{
    struct schema_Wifi_Security_Config cfg_sec;
    char *filter_vapsec[] = {"-",NULL};

    memset(&cfg_sec,0,sizeof(cfg_sec));
    wifi_db_dbg_print(1,"%s:%d:Security update for vap name=%s\n",__func__, __LINE__,vap_name);
    if(sec == NULL)
    {
        wifi_db_dbg_print(1,"%s:%d:Null pointer Security Config update failed \n",__func__, __LINE__);
        return -1;
    }
    cfg_sec.security_mode = sec->mode;
    cfg_sec.encryption_method = sec->encr;
    strncpy(cfg_sec.vap_name,vap_name,(sizeof(cfg_sec.vap_name)-1));

    if((strstr(cfg_sec.vap_name, "_6g")) == NULL)
    {
        if ((strncmp(vap_name, "iot_ssid", strlen("iot_ssid")) == 0) || (strncmp(vap_name, "lnf_psk", strlen("lnf_psk"))==0))
        {
            switch (cfg_sec.security_mode)
            {
                case wifi_security_mode_none:
                case wifi_security_mode_wpa_personal:
                case wifi_security_mode_wpa2_personal:
                case wifi_security_mode_wpa_wpa2_personal:
                case wifi_security_mode_wpa3_personal:
                case wifi_security_mode_wpa3_transition:
                break;
                default :
                    cfg_sec.security_mode= wifi_security_mode_wpa2_personal;
                break;
            }

            switch (cfg_sec.encryption_method)
            {
                case wifi_encryption_aes_tkip:
                case wifi_encryption_none:
                case wifi_encryption_aes:
                case wifi_encryption_tkip:
                break;
                default :
                    cfg_sec.encryption_method = wifi_encryption_aes_tkip;
                break;
            }
        }
        else if(strncmp(vap_name, "hotspot_open", strlen("hotspot_open")) == 0)
        {
            switch (cfg_sec.security_mode)
            {
                case wifi_security_mode_none:
                break;
                default:
                    cfg_sec.security_mode = wifi_security_mode_none;
                break;
            }
        }
        else if(strncmp(vap_name, "hotspot_secure", strlen("hotspot_secure")) == 0)
        {
            switch (cfg_sec.security_mode)
            {
                case wifi_security_mode_wpa_enterprise:
                case wifi_security_mode_wpa2_enterprise:
                case wifi_security_mode_wpa3_enterprise:
                case wifi_security_mode_wpa_wpa2_enterprise:
                break;
                default :
                    cfg_sec.security_mode = wifi_security_mode_wpa2_enterprise;
                break;
            }

        }
    }
    switch (cfg_sec.security_mode)
    {
        case wifi_security_mode_none:
        case wifi_security_mode_wpa_personal:
        case wifi_security_mode_wpa2_personal:
        case wifi_security_mode_wpa_wpa2_personal:
        case wifi_security_mode_wpa3_personal:
        case wifi_security_mode_wpa3_transition:
            strncpy(cfg_sec.radius_server_ip,"",sizeof(cfg_sec.radius_server_ip)-1);
            cfg_sec.radius_server_port = 0;
            strncpy(cfg_sec.radius_server_key, "",sizeof(cfg_sec.radius_server_key)-1);
            strncpy(cfg_sec.secondary_radius_server_ip,"",sizeof(cfg_sec.secondary_radius_server_ip)-1);
            cfg_sec.secondary_radius_server_port = 0;
            strncpy(cfg_sec.secondary_radius_server_key, "",sizeof(cfg_sec.secondary_radius_server_key)-1);
            cfg_sec.key_type = sec->u.key.type;
            strncpy(cfg_sec.keyphrase,sec->u.key.key,sizeof(cfg_sec.keyphrase)-1);
        break;
        case wifi_security_mode_wpa_enterprise:
        case wifi_security_mode_wpa2_enterprise:
        case wifi_security_mode_wpa3_enterprise:
        case wifi_security_mode_wpa_wpa2_enterprise:
            strncpy(cfg_sec.radius_server_ip,(char *)sec->u.radius.ip,sizeof(cfg_sec.radius_server_ip)-1);
            cfg_sec.radius_server_port = (int)sec->u.radius.port;
            strncpy(cfg_sec.radius_server_key, sec->u.radius.key,sizeof(cfg_sec.radius_server_key)-1);
            strncpy(cfg_sec.secondary_radius_server_ip,(char *)sec->u.radius.s_ip,sizeof(cfg_sec.secondary_radius_server_ip)-1);
            cfg_sec.secondary_radius_server_port =(int)sec->u.radius.s_port;
            strncpy(cfg_sec.secondary_radius_server_key, sec->u.radius.s_key,sizeof(cfg_sec.secondary_radius_server_key)-1);
            cfg_sec.key_type = 0;
            strncpy(cfg_sec.keyphrase,"",sizeof(cfg_sec.keyphrase)-1);

            if ((strlen(cfg_sec.radius_server_ip) == 0) || (decode_ipv4_address(cfg_sec.radius_server_ip) != 0))
            {
                strncpy(cfg_sec.radius_server_ip, "0.0.0.0", sizeof(cfg_sec.radius_server_ip)-1);
            }

            if ((strlen(cfg_sec.secondary_radius_server_ip) == 0) || (decode_ipv4_address(cfg_sec.secondary_radius_server_ip) != 0))
            {
                strncpy(cfg_sec.secondary_radius_server_ip, "0.0.0.0", sizeof(cfg_sec.secondary_radius_server_ip)-1);
            }
        break;
        default:
        break;
    }

    wifi_db_dbg_print(1,"%s:%d: Update table_Wifi_Security_Config table Sec_mode=%d enc_mode=%d r_ser_ip=%s r_ser_port=%d r_ser_key=%s rs_ser_ip=%s rs_ser_ip sec_rad_ser_port=%d rs_ser_key=%s cfg_key_type=%d cfg_sec_keyphrase=%s cfg_vap_name=%s\n",__func__, __LINE__,cfg_sec.security_mode,cfg_sec.encryption_method,cfg_sec.radius_server_ip,cfg_sec.radius_server_port,cfg_sec.radius_server_key,cfg_sec.secondary_radius_server_ip,cfg_sec.secondary_radius_server_port,cfg_sec.secondary_radius_server_key,cfg_sec.key_type,cfg_sec.keyphrase,cfg_sec.vap_name);

    if(ovsdb_table_upsert_with_parent(g_wifidb.ovsdb_sock_path,&table_Wifi_Security_Config,&cfg_sec,false,filter_vapsec,SCHEMA_TABLE(Wifi_VAP_Config),ovsdb_where_simple(SCHEMA_COLUMN(Wifi_VAP_Config,vap_name),vap_name),SCHEMA_COLUMN(Wifi_VAP_Config,security)) == false)
    {
        wifi_db_dbg_print(1,"%s:%d: failed to update table_Wifi_Security_Config table\n",__func__, __LINE__);
    }
    else
    {
        wifi_db_dbg_print(1,"%s:%d:  update table_Wifi_Security_Config table successful\n",__func__, __LINE__);
    }

    return 0;
}

int wifidb_update_wifi_vap_info(char *radio_name,wifi_vap_info_t *config)
{
    struct schema_Wifi_VAP_Config cfg;
    char *filter_vap[] = {"-",SCHEMA_COLUMN(Wifi_VAP_Config,security),SCHEMA_COLUMN(Wifi_VAP_Config,interworking), SCHEMA_COLUMN(Wifi_VAP_Config,mac_filter),NULL};

    memset(&cfg,0,sizeof(cfg));
    wifi_db_dbg_print(1,"%s:%d:VAP Config update for radio name=%s\n",__func__, __LINE__,radio_name);
    if(config == NULL)
    {
        wifi_db_dbg_print(1,"%s:%d:VAP Config update failed \n",__func__, __LINE__);
        return -1;
    }
    wifi_db_dbg_print(1,"%s:%d:Update radio=%s vap name=%s \n",__func__, __LINE__,radio_name,config->vap_name);
    strncpy(cfg.radio_name,radio_name,sizeof(cfg.radio_name)-1);
    strncpy(cfg.vap_name, config->vap_name,(sizeof(cfg.vap_name)-1));
    strncpy(cfg.ssid, config->u.bss_info.ssid, (sizeof(cfg.ssid)-1));
    cfg.enabled = config->u.bss_info.enabled;
    cfg.ssid_advertisement_enabled = config->u.bss_info.showSsid;
    cfg.isolation_enabled = config->u.bss_info.isolation;
    cfg.mgmt_power_control = config->u.bss_info.mgmtPowerControl;
    cfg.bss_max_sta = config->u.bss_info.bssMaxSta;
    cfg.bss_transition_activated = config->u.bss_info.bssTransitionActivated;
    cfg.nbr_report_activated = config->u.bss_info.nbrReportActivated;
    cfg.rapid_connect_enabled = config->u.bss_info.rapidReconnectEnable;
    cfg.rapid_connect_threshold = config->u.bss_info.rapidReconnThreshold;
    cfg.vap_stats_enable = config->u.bss_info.vapStatsEnable;
    cfg.mac_filter_enabled = config->u.bss_info.mac_filter_enable;
    cfg.mac_filter_mode = config->u.bss_info.mac_filter_mode;
    cfg.wmm_enabled = config->u.bss_info.wmm_enabled;
    cfg.uapsd_enabled = config->u.bss_info.UAPSDEnabled;
    cfg.wmm_noack = config->u.bss_info.wmmNoAck;
    cfg.wep_key_length = config->u.bss_info.wepKeyLength;
    cfg.bss_hotspot = config->u.bss_info.bssHotspot;
    cfg.wps_push_button = config->u.bss_info.wpsPushButton;
#ifdef WIFI_HAL_VERSION_3
    snprintf(cfg.beacon_rate_ctl, sizeof(cfg.beacon_rate_ctl), "%dMbps", config->u.bss_info.beaconRate);
#else
    strncpy(cfg.beacon_rate_ctl,config->u.bss_info.beaconRateCtl,sizeof(cfg.beacon_rate_ctl)-1);
#endif
#ifdef WIFI_HAL_VERSION_3
    strncpy(cfg.mfp_config,MFPConfig[config->u.bss_info.security.mfp],sizeof(cfg.mfp_config)-1);
#else
    strncpy(cfg.mfp_config,config->u.bss_info.security.mfpConfig,sizeof(cfg.mfp_config)-1);
#endif

    wifi_db_dbg_print(1,"%s:%d:VAP Config update data cfg.radio_name=%s cfg.radio_name=%s cfg.ssid=%s cfg.enabled=%d cfg.advertisement=%d cfg.isolation_enabled=%d cfg.mgmt_power_control=%d cfg.bss_max_sta =%d cfg.bss_transition_activated=%d cfg.nbr_report_activated=%d cfg.rapid_connect_enabled=%d cfg.rapid_connect_threshold=%d cfg.vap_stats_enable=%d cfg.mac_filter_enabled =%d cfg.mac_filter_mode=%d cfg.wmm_enabled=%d mfp=%s\n",__func__, __LINE__,cfg.radio_name,cfg.vap_name,cfg.ssid,cfg.enabled,cfg.ssid_advertisement_enabled,cfg.isolation_enabled,cfg.mgmt_power_control,cfg.bss_max_sta,cfg.bss_transition_activated,cfg.nbr_report_activated,cfg.rapid_connect_enabled,cfg.rapid_connect_threshold,cfg.vap_stats_enable,cfg.mac_filter_enabled,cfg.mac_filter_mode,cfg.wmm_enabled,cfg.mfp_config);

    if(ovsdb_table_upsert_with_parent(g_wifidb.ovsdb_sock_path,&table_Wifi_VAP_Config,&cfg,false,filter_vap,SCHEMA_TABLE(Wifi_Radio_Config),ovsdb_where_simple(SCHEMA_COLUMN(Wifi_Radio_Config,radio_name),radio_name),SCHEMA_COLUMN(Wifi_Radio_Config,vap_configs)) == false)
    {
        wifi_db_dbg_print(1,"%s:%d: failed to update table_Wifi_VAP_Config table\n",__func__, __LINE__);
    }
    else
    {
        wifi_db_dbg_print(1,"%s:%d:  update table_Wifi_VAP_Config table successful\n",__func__, __LINE__);
    }
    return 0;
}

int wifidb_update_wifi_global_config(struct schema_Wifi_Global_Config *cfg)
{
    char *insert_filter[] = {"-",SCHEMA_COLUMN(Wifi_Global_Config,gas_config),NULL};
    
    if (cfg == NULL) {
        wifi_db_dbg_print(1,"%s:%d: Cfg is NULL \n",__func__, __LINE__);
        return RETURN_ERR;
    }

    if (ovsdb_table_upsert_f(g_wifidb.ovsdb_sock_path,&table_Wifi_Global_Config, cfg, false,insert_filter) == false)
    {
        wifi_db_dbg_print(1,"%s:%d: Failed to update global config table\n", __func__, __LINE__);
        return RETURN_ERR;
    }
    return RETURN_OK;
}

void *wifi_db_get_table_entry(char *key, char *key_name,ovsdb_table_t *table,ovsdb_col_t key_type)
{
    json_t *where;
    void *pcfg;
    int count;
    
    if (key == NULL) {
        struct schema_Wifi_Global_Config *gcfg = NULL;
        json_t *jrow;
        where = json_array();
        pjs_errmsg_t perr;

        jrow  = ovsdb_sync_select_where(g_wifidb.ovsdb_sock_path,SCHEMA_TABLE(Wifi_Global_Config),where);
        if (json_array_size(jrow) != 1)
        {
            wifi_db_dbg_print(1,"%s:%d: Empty global config table\n",__func__, __LINE__);
            return NULL;
        }
        gcfg = (struct schema_Wifi_Global_Config*)malloc(sizeof(struct schema_Wifi_Global_Config));
        if (gcfg == NULL) {
            wifi_db_dbg_print(1,"%s:%d: Failed to allocate memory\n",__func__, __LINE__);
            return NULL;
        }
        memset(gcfg,0,sizeof(struct schema_Wifi_Global_Config));
        if (!schema_Wifi_Global_Config_from_json(
                  gcfg,
                  json_array_get(jrow, 0),
                  false,
                  perr))
        {
            wifi_db_dbg_print(1,"%s:%d: Error in parsing globalconfig \n",__func__, __LINE__);
        }
        return gcfg;
    } else {
        where = ovsdb_tran_cond(key_type, key_name, OFUNC_EQ, key);
        pcfg = ovsdb_table_select_where(g_wifidb.ovsdb_sock_path, table, where, &count);

        if (pcfg == NULL) {
            wifi_db_dbg_print(1,"%s:%d:  Table not found\n",__func__, __LINE__);
            return NULL;
        }
    }
    return pcfg;
}

int wifi_db_get_harvester_config(PCOSA_DML_WIFI_HARVESTER pHarvester)
{
    json_t *where;
    struct schema_Wifi_Global_Config *gcfg = NULL;
    json_t *jrow;
    pjs_errmsg_t perr;

    where = json_array();

    jrow  = ovsdb_sync_select_where(g_wifidb.ovsdb_sock_path,SCHEMA_TABLE(Wifi_Global_Config),where);
    if (json_array_size(jrow) != 1)
    {
        wifi_db_dbg_print(1,"%s:%d: Empty global config table\n",__func__, __LINE__);
        return RETURN_ERR;
    }
    gcfg = (struct schema_Wifi_Global_Config*)malloc(sizeof(struct schema_Wifi_Global_Config));
    if (gcfg == NULL) {
        wifi_db_dbg_print(1,"%s:%d: Failed to allocate memory\n",__func__, __LINE__);
        return RETURN_ERR;
    }
    memset(gcfg,0,sizeof(struct schema_Wifi_Global_Config));
    if (!schema_Wifi_Global_Config_from_json(
              gcfg,
              json_array_get(jrow, 0),
              false,
              perr))
    {
        wifi_db_dbg_print(1,"%s:%d: Error in parsing globalconfig \n",__func__, __LINE__);
    }
    pHarvester->uINSTClientReportingPeriod = gcfg->inst_wifi_client_reporting_period;
    pHarvester->uINSTClientDefReportingPeriod = gcfg->inst_wifi_client_def_reporting_period;
    strncpy(pHarvester->MacAddress, gcfg->inst_wifi_client_mac, sizeof(pHarvester->MacAddress)-1);
    pHarvester->bActiveMsmtEnabled = gcfg->wifi_active_msmt_enabled;
    pHarvester->uActiveMsmtSampleDuration = gcfg->wifi_active_msmt_sample_duration;
    pHarvester->uActiveMsmtPktSize = gcfg->wifi_active_msmt_pktsize;
    pHarvester->uActiveMsmtNumberOfSamples = gcfg->wifi_active_msmt_num_samples;
    pHarvester->bINSTClientEnabled = gcfg->inst_wifi_client_enabled;
  
    return RETURN_OK;
}

int wifi_ovsdb_update_table_entry(char *key, char *key_name,ovsdb_col_t key_type, ovsdb_table_t *table, void *cfg,char *filter[])
{
    json_t *where;
    int ret;

    if (key == NULL) {
        ret = ovsdb_table_update(g_wifidb.ovsdb_sock_path, table,cfg);
    } else {
        where = ovsdb_tran_cond(key_type, key_name, OFUNC_EQ, key);
        ret = ovsdb_table_update_where_f(g_wifidb.ovsdb_sock_path, table,where, cfg,filter);
        wifi_db_dbg_print(1,"%s:%d: ret val %d",__func__, __LINE__,ret);
    }
    return ret;
}

/*int wifi_ovsdb_update_vap_table_entry(char *key, char *key_name,ovsdb_col_t key_type, ovsdb_table_t *table, void *cfg)
{
    json_t *where;
    int ret;
    char *filter_vap[] = {"-",SCHEMA_COLUMN(Wifi_VAP_Config,security),SCHEMA_COLUMN(Wifi_VAP_Config,interworking),NULL};

    if (key == NULL) {
        ret = ovsdb_table_update(g_wifidb.ovsdb_sock_path, table,cfg);
    } else {
        where = ovsdb_tran_cond(key_type, key_name, OFUNC_EQ, key);
        ret = ovsdb_table_update_where_f(g_wifidb.ovsdb_sock_path, table,where, cfg,filter_vap);
        wifi_db_dbg_print(1,"%s:%d: ret val %d",__func__, __LINE__,ret);
    }
    return ret;
}*/

int wifi_db_update_global_config()
{

    int retPsmGet = CCSP_SUCCESS;
    struct schema_Wifi_Global_Config cfg;
    char *str_val = NULL;

    memset(&cfg,0,sizeof(cfg));
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.Device.WiFi.NotifyWiFiChanges" , NULL, &str_val);
    if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
        if (strncmp(str_val,"true",strlen("true")) == 0) {
            cfg.notify_wifi_changes = true;
        } else {
            cfg.notify_wifi_changes = false;
        }
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        
    }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.PreferPrivate" , NULL, &str_val);
    if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
        cfg.prefer_private = _ansc_atoi(str_val);
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
    }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.PreferPrivateConfigure", NULL, &str_val);
    if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
        cfg.prefer_private_configure = _ansc_atoi(str_val);
       ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
    }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.FactoryReset" , NULL, &str_val);
    if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
        cfg.factory_reset = _ansc_atoi(str_val);
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
    }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.TxOverflowSelfheal" , NULL, &str_val);
    if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
        if ((strncmp(str_val,"true",strlen("true")) == 0) || (strncmp(str_val,"TRUE",strlen("TRUE")) == 0 )) {
            cfg.tx_overflow_selfheal = true;
        } else {
            cfg.tx_overflow_selfheal = false;
        }
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
    }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.Device.WiFi.VlanCfgVerion" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            cfg.vlan_cfg_version = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.Device.WiFi.WPSPin" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            strncpy(cfg.wps_pin, str_val,sizeof(cfg.wps_pin)-1);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.Device.WiFi.X_RDKCENTRAL-COM_BandSteering.Enable" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            cfg.bandsteering_enable = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.X_RDKCENTRAL-COM_GoodRssiThreshold" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            cfg.good_rssi_threshold = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.X_RDKCENTRAL-COM_AssocCountThreshold" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            cfg.assoc_count_threshold = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.X_RDKCENTRAL-COM_AssocGateTime" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            cfg.assoc_gate_time = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.X_RDKCENTRAL-COM_AssocMonitorDuration" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            cfg.assoc_monitor_duration = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.X_RDKCENTRAL-COM_RapidReconnectIndicationEnable" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            cfg.rapid_reconnect_enable = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.vAPStatsEnable" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            if (strncmp(str_val,"true",strlen("true")) == 0) {
            cfg.vap_stats_feature = true;
        } else {
            cfg.vap_stats_feature = false;
        }

        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.FeatureMFPConfig" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            cfg.mfp_config_feature = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.X_RDK-CENTRAL_COM_ForceDisable" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            if ((strncmp(str_val,"true",strlen("true")) == 0) || (strncmp(str_val,"TRUE",strlen("TRUE")) == 0 )) {
                cfg.force_disable_radio_feature = true;
            } else {
                cfg.force_disable_radio_feature = false;
            }
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.X_RDK-CENTRAL_COM_ForceDisable_RadioStatus" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            cfg.force_disable_radio_status = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.FixedWmmParamsValues" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            cfg.fixed_wmm_params = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.X_RDKCENTRAL-COM_Syndication.WiFiRegion.Code" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            strncpy(cfg.wifi_region_code, str_val,sizeof(cfg.wifi_region_code)-1);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }
    
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.Device.WiFi.InstWifiClientEnabled" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            cfg.inst_wifi_client_enabled = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }
 
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.Device.WiFi.InstWifiClientReportingPeriod" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            cfg.inst_wifi_client_reporting_period = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.Device.WiFi.InstWifiClientMacAddress" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            strncpy(cfg.inst_wifi_client_mac,str_val,sizeof(cfg.inst_wifi_client_mac)-1);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.Device.WiFi.InstWifiClientDefReportingPeriod" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            cfg.inst_wifi_client_def_reporting_period = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.Device.WiFi.WiFiActiveMsmtEnabled" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            cfg.wifi_active_msmt_enabled = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.Device.WiFi.WiFiActiveMsmtPktSize" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            cfg.wifi_active_msmt_pktsize = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.Device.WiFi.WiFiActiveMsmtNumberOfSample" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            cfg.wifi_active_msmt_num_samples = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.Device.WiFi.WiFiActiveMsmtSampleDuration" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            cfg.wifi_active_msmt_sample_duration = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.Device.WiFi.NeighbouringDiagnosticEnable" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            cfg.diagnostic_enable = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,"eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.ValidateSSIDName" , NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            cfg.validate_ssid = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }
  
    if (wifidb_update_wifi_global_config(&cfg) != RETURN_OK) {
        wifi_db_dbg_print(1,"%s:%d: Failed to update global config\n", __func__, __LINE__);
        return RETURN_ERR;
    } else {
        wifi_db_dbg_print(1,"%s:%d: Updated global config table successfully\n",__func__, __LINE__);
    }
    return RETURN_OK;
}

int wifi_db_update_radio_config()
{
    PCOSA_DATAMODEL_WIFI pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_RADIO pWifiRadio    = NULL;
    
    wifi_radio_operationParam_t radio_cfg;
    int radio_index,retval=0;
    char *str_val = NULL;
    char recName[256];
    int retPsmGet = CCSP_SUCCESS;
    errno_t rc = -1;
    for(radio_index=0;radio_index<MAX_RADIO_INDEX;radio_index++) 
    {
        memset(&radio_cfg,0,sizeof(radio_cfg));
        pWifiRadio = pMyObject->pRadio+radio_index;
        if(!pWifiRadio) {
            wifi_db_dbg_print(1,"%s:%d: Could not get dml cache for radio index %d\n",__func__, __LINE__,radio_index);
            continue;
        }
        PCOSA_DML_WIFI_RADIO_CFG  pWifiRadioCfg   = &pWifiRadio->Radio.Cfg;

        //Convert config from DML to HAL 
        memset(&radio_cfg, 0, sizeof(wifi_radio_operationParam_t));
        radioGetCfgUpdateFromDmlToHal(radio_index, pWifiRadioCfg, &radio_cfg);

        radio_cfg.radioStatsMeasuringRate = pWifiRadio->Stats.RadioStatisticsMeasuringRate;
        radio_cfg.radioStatsMeasuringInterval  = pWifiRadio->Stats.RadioStatisticsMeasuringInterval;

        rc = sprintf_s(recName, sizeof(recName), "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.Radio.%d.FactoryResetSSID",radio_index+1);
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,recName, NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            radio_cfg.factoryResetSsid = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

        rc = sprintf_s(recName, sizeof(recName), "eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.%d.SetChanUtilThreshold",radio_index+1);
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,recName, NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            radio_cfg.chanUtilThreshold = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

        rc = sprintf_s(recName, sizeof(recName), "eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.%d.ChanUtilSelfHealEnable",radio_index+1);
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,recName, NULL, &str_val);
        if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
            radio_cfg.chanUtilSelfHealEnable = _ansc_atoi(str_val);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
        }

        retval = wifidb_update_wifi_radio_config(radio_index+1,&radio_cfg);
        if (retval != 0) {
            wifi_db_dbg_print(1,"%s:%d: Failed to update radio config in wifi db\n",__func__, __LINE__);
        } else {
            wifi_db_dbg_print(1,"%s:%d: Successfully updated radio config in wifidb for index:%d\n",__func__, __LINE__,radio_index+1);
        }
    }
   
    return RETURN_OK; 
}

int wifi_db_update_vap_config()
{
    char *vap_name[] = {"private_ssid_2g", "private_ssid_5g", "iot_ssid_2g", "iot_ssid_5g", "hotspot_open_2g", "hotspot_open_5g", "lnf_psk_2g", "lnf_psk_5g", "hotspot_secure_2g", "hotspot_secure_5g","lnf_radius_2g","lnf_radius_5g","mesh_backhaul_2g","mesh_backhaul_5g","guest_ssid_2g","guest_ssid_5g"};
    int i;
    wifi_vap_info_t vap_cfg;
    PCOSA_DATAMODEL_WIFI pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PSINGLE_LINK_ENTRY pSLinkEntry = NULL;
    PCOSA_DML_WIFI_SSID  pWifiSsid = NULL;
    PCOSA_DML_WIFI_AP      pWifiAp = NULL;
    char *str_val = NULL;
    char recName[256];
    int retPsmGet = CCSP_SUCCESS;
    unsigned int mf_mode = 0;
    int retval =0;
    char radio_name[BUFFER_LENGTH_OVSDB] = {0};
    errno_t rc = -1;
#ifdef WIFI_HAL_VERSION_3
    wifi_bitrate_t beaconRate;
#endif

    for(i=0;i<WIFI_MAX_VAP;i++) {
    if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->SsidQueue, i)) == NULL) {
        wifi_db_dbg_print(1,"%s:%d: Data Model object not found!\n",__func__, __LINE__);
        continue;
    }

    if((pWifiSsid = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) {
        wifi_db_dbg_print(1,"%s:%d: Link object not found!\n",__func__, __LINE__);
        continue;
    }
    if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->AccessPointQueue, i)) == NULL) {
        wifi_db_dbg_print(1,"%s:%d: Error linking Data Model object!\n",__func__, __LINE__);
        continue;
    }

    if((pWifiAp = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) {
        wifi_db_dbg_print(1,"%s:%d: Error linking Data Model object!\n",__func__, __LINE__);
        continue;
    }

    memset(&vap_cfg,0,sizeof(vap_cfg));
    strncpy(vap_cfg.vap_name,vap_name[i],sizeof(vap_cfg.vap_name)-1);
    if(convert_radio_to_name((i%2)+1,radio_name) == 0) {
        wifi_db_dbg_print(1,"%s:%d: Radio name conversion successful\n",__func__, __LINE__);
    }

    if (vap_SECCfgUpdateFromDmlToHal(i, pWifiSsid, pWifiAp, &vap_cfg) < 0) {
        wifi_db_dbg_print(1, "%s:%d: Error updating Cfg from DML to HAL\n", __func__, __LINE__);
    }

    if (vap_WPSCfgUpdateFromDmlToHal(pWifiAp, &vap_cfg) < 0) {
        wifi_db_dbg_print(1, "%s:%d: Error updating Cfg from DML to HAL\n", __func__, __LINE__);
    }

    if (vap_UpdateBeaconRate(pWifiAp, &vap_cfg) < 0) {
        wifi_db_dbg_print(1, "%s:%d: Error updating Cfg from DML to HAL\n", __func__, __LINE__);
    }

    strncpy(vap_cfg.u.bss_info.ssid, pWifiSsid->SSID.Cfg.SSID, sizeof(vap_cfg.u.bss_info.ssid)-1);
    vap_cfg.u.bss_info.enabled = pWifiSsid->SSID.Cfg.bEnabled;
    vap_cfg.u.bss_info.showSsid = pWifiAp->AP.Cfg.SSIDAdvertisementEnabled;
    vap_cfg.u.bss_info.isolation     = pWifiAp->AP.Cfg.IsolationEnable;

    vap_cfg.u.bss_info.bssMaxSta = pWifiAp->AP.Cfg.BssMaxNumSta;
    vap_cfg.u.bss_info.nbrReportActivated = pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_NeighborReportActivated;
    vap_cfg.u.bss_info.vapStatsEnable = pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_StatsEnable;
    vap_cfg.u.bss_info.bssTransitionActivated = pWifiAp->AP.Cfg.BSSTransitionActivated;
    vap_cfg.u.bss_info.mgmtPowerControl = pWifiAp->AP.Cfg.ManagementFramePowerControl;
    vap_cfg.u.bss_info.wmm_enabled = pWifiAp->AP.Cfg.WMMEnable;
    vap_cfg.u.bss_info.UAPSDEnabled = pWifiAp->AP.Cfg.UAPSDEnable;
    vap_cfg.u.bss_info.wmmNoAck = pWifiAp->AP.Cfg.WmmNoAck;
    vap_cfg.u.bss_info.bssHotspot = pWifiAp->AP.Cfg.BssHotSpot;
    vap_cfg.u.bss_info.wpsPushButton = pWifiAp->WPS.Cfg.WpsPushButton;
    vap_cfg.u.bss_info.rapidReconnThreshold = pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_rapidReconnectMaxTime;
    vap_cfg.u.bss_info.rapidReconnectEnable = pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_rapidReconnectCountEnable;

#ifdef WIFI_HAL_VERSION_3
    if (getBeaconRateFromString(pWifiAp->AP.Cfg.BeaconRate, &beaconRate) == 0) {
        wifi_db_dbg_print(1, "%s:%d: Error updating BeaconRate\n", __func__, __LINE__);
    }
    vap_cfg.u.bss_info.beaconRate = beaconRate;

#else
    strncpy(vap_cfg.u.bss_info.beaconRateCtl,pWifiAp->AP.Cfg.BeaconRate,sizeof(vap_cfg.u.bss_info.beaconRateCtl)-1);
#endif
    
    rc = sprintf_s(recName, sizeof(recName), "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.WepKeyLength",i+1);
    if(rc < EOK)
    {
        ERR_CHK(rc);
    }
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,recName, NULL, &str_val);
    if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
        vap_cfg.u.bss_info.wepKeyLength = _ansc_atoi(str_val);
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
    }

    rc = sprintf_s(recName, sizeof(recName), "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.MacFilterMode",i+1);
    if(rc < EOK)
    {
        ERR_CHK(rc);
    }
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem,recName, NULL, &str_val);
    if ((retPsmGet == CCSP_SUCCESS) && (str_val != NULL)) {
        mf_mode = _ansc_atoi(str_val);
        if (mf_mode == 0) {
            vap_cfg.u.bss_info.mac_filter_enable = FALSE;
            vap_cfg.u.bss_info.mac_filter_mode  = wifi_mac_filter_mode_white_list;
        }
        else if(mf_mode == 1) {
            vap_cfg.u.bss_info.mac_filter_enable = TRUE;
            vap_cfg.u.bss_info.mac_filter_mode  = wifi_mac_filter_mode_white_list;
        }
        else if(mf_mode == 2) {
            vap_cfg.u.bss_info.mac_filter_enable = TRUE;
            vap_cfg.u.bss_info.mac_filter_mode  = wifi_mac_filter_mode_black_list;
        }
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(str_val);
    }

    retval = wifidb_update_wifi_vap_info(radio_name,&vap_cfg);
    if (retval != 0) {
         wifi_db_dbg_print(1,"%s:%d: Failed to update vap config in wifi db\n",__func__, __LINE__);
    } else {
        wifi_db_dbg_print(1,"%s:%d: Successfully updated vap config in wifidb for index:%d\n",__func__, __LINE__,i+1);
    }

    retval = wifidb_update_wifi_security_config(vap_cfg.vap_name, &vap_cfg.u.bss_info.security);
    if (retval != 0) {
        wifi_db_dbg_print(1,"%s:%d: Failed to update vap config in wifi db\n",__func__, __LINE__);
    } else {
        wifi_db_dbg_print(1,"%s:%d: Successfully updated vap config in wifidb for index:%d\n",__func__, __LINE__,i+1);
    }
    retval = wifidb_update_wifi_macfilter_config(i);
    if (retval != 0) {
        wifi_db_dbg_print(1,"%s:%d: Failed to update mac filter entries for %d in wifi db\n",__func__, __LINE__, i+1);
    } else {
        wifi_db_dbg_print(1,"%s:%d: Successfully updated vap config in wifidb for index:%d\n",__func__, __LINE__,i+1);
    }


    }
    return RETURN_OK;
}

int wifi_db_update_psm_values()
{
    int retval;
    retval = wifi_db_update_global_config();
    wifi_db_dbg_print(1,"%s:%d: Global config update %d\n",__func__, __LINE__,retval);

    retval = wifi_db_update_radio_config();
    
    wifi_db_dbg_print(1,"%s:%d: Radio config update %d\n",__func__, __LINE__,retval);

    retval = wifi_db_update_vap_config();

    wifi_db_dbg_print(1,"%s:%d: Vap config update %d\n",__func__, __LINE__,retval);
    return retval; 
}

int wifi_db_init_global_config_default()
{
    struct schema_Wifi_Global_Config cfg, *pcfg = NULL;

    pcfg = (struct schema_Wifi_Global_Config  *) wifi_db_get_table_entry(NULL, NULL,&table_Wifi_Global_Config,OCLM_UUID);
    if (pcfg != NULL) {
        wifi_db_dbg_print(1,"%s:%d: Global config table entry already present",__func__, __LINE__);
        if(strncmp(pcfg->wifi_region_code, "", 1) == 0) {
            wifi_db_dbg_print(1,"%s: country code is empty, updating global config\n",__func__);
            wifi_db_update_global_config();
        }
        free(pcfg);
        return RETURN_OK;
    }
    memset(&cfg,0,sizeof(cfg));

    cfg.notify_wifi_changes = true;
    cfg.prefer_private =  true;
    cfg.prefer_private_configure = true;
    cfg.tx_overflow_selfheal = false;
    cfg.vlan_cfg_version = 2;
    
    cfg.bandsteering_enable = false;
    cfg.good_rssi_threshold = -65;
    cfg.assoc_count_threshold = 0;
    cfg.assoc_gate_time  = 0;
    cfg.assoc_monitor_duration = 0;
    cfg.rapid_reconnect_enable = false;
    cfg.vap_stats_feature =  true;
    cfg.mfp_config_feature = false;
    cfg.force_disable_radio_feature = false;
    cfg.force_disable_radio_status = false;
    cfg.fixed_wmm_params = 3;
    strncpy(cfg.wifi_region_code, "USI",sizeof(cfg.wifi_region_code)-1);
    cfg.inst_wifi_client_enabled = false;
    cfg.inst_wifi_client_reporting_period = 0;
    cfg.inst_wifi_client_def_reporting_period = 0;
    cfg.wifi_active_msmt_enabled = false;
    cfg.wifi_active_msmt_pktsize = 1470;
    cfg.wifi_active_msmt_num_samples = 5;
    cfg.wifi_active_msmt_sample_duration = 400;
    cfg.diagnostic_enable = false;
    cfg.validate_ssid = true;

    if (wifidb_update_wifi_global_config(&cfg) != RETURN_OK) {
        wifi_db_dbg_print(1,"%s:%d: Failed to update global config\n", __func__, __LINE__);
    } else {
        wifi_db_dbg_print(1,"%s:%d: Updated global config table successfully\n",__func__, __LINE__);
    }
    return RETURN_OK;
}

int wifi_db_init_radio_config_default()
{
    wifi_radio_operationParam_t cfg;
    int i;
    char radio_name[16] = {0};
    struct schema_Wifi_Radio_Config  *pcfg= NULL;

    for(i=0;i<MAX_RADIO_INDEX;i++)  {
        if (convert_radio_to_name(i+1,radio_name) == 0) {
            pcfg = (struct schema_Wifi_Radio_Config  *) wifi_db_get_table_entry(radio_name, "radio_name",&table_Wifi_Radio_Config,OCLM_STR);
            if (pcfg != NULL) {
                wifi_db_dbg_print(1,"%s:%d WIFI DB Radio entry for %s already present\n",__func__, __LINE__,radio_name);
                continue;
            }
        }
        pcfg = NULL;
        memset(&cfg,0,sizeof(cfg));
        cfg.dtimPeriod = 1;
        cfg.beaconInterval = 100;
        cfg.fragmentationThreshold = 2346;
        cfg.transmitPower = 100;
        cfg.rtsThreshold = 2347;
        cfg.guardInterval = wifi_guard_interval_auto;
        cfg.ctsProtection = false;
        cfg.obssCoex = true;
        cfg.stbcEnable = true;
        cfg.greenFieldEnable = false;
        cfg.userControl = 1;
        cfg.adminControl = 254;
        cfg.chanUtilThreshold = 90;
        cfg.chanUtilSelfHealEnable = 0;

        if (wifidb_update_wifi_radio_config(i+1, &cfg) == RETURN_OK) {
            wifi_db_dbg_print(1,"%s:%d Updated radio %d config successfully\n",__func__, __LINE__,i);
        } else {
            wifi_db_dbg_print(1,"%s:%d Failed to update radio %d config\n",__func__, __LINE__,i);
        }
    }
    return RETURN_OK;
}

int wifi_db_init_vap_config_default()
{
    char *vap_name[] = {"private_ssid_2g", "private_ssid_5g", "iot_ssid_2g", "iot_ssid_5g", "hotspot_open_2g", "hotspot_open_5g", "lnf_psk_2g", "lnf_psk_5g", "hotspot_secure_2g", "hotspot_secure_5g","lnf_radius_2g","lnf_radius_5g","mesh_backhaul_2g","mesh_backhaul_5g","guest_ssid_2g","guest_ssid_5g"};

    wifi_vap_info_t cfg;
    int i;
    char radio_name[16] = {0};
    struct schema_Wifi_VAP_Config  *pcfg= NULL;

    for(i = 0; i < WIFI_MAX_VAP; i++) {
        pcfg = (struct schema_Wifi_VAP_Config  *) wifi_db_get_table_entry(vap_name[i], "vap_name",&table_Wifi_VAP_Config,OCLM_STR);
        if (pcfg != NULL) {
            wifi_db_dbg_print(1,"%s:%d WIFI DB Vap entry for %s already present\n",__func__, __LINE__,vap_name[i]);
            continue;
        }
        pcfg = NULL;

        memset(&cfg,0,sizeof(cfg));
        strncpy(cfg.vap_name,vap_name[i],sizeof(cfg.vap_name)-1);
        cfg.u.bss_info.wmm_enabled = true;
        if (i == 4 || i == 5 || i == 8 || i == 9) {
            cfg.u.bss_info.bssMaxSta = 5;
        } else {
            cfg.u.bss_info.bssMaxSta = 30;
        }
        cfg.u.bss_info.bssTransitionActivated = false;
        cfg.u.bss_info.nbrReportActivated = false;
        if (i == 0 || i == 1) {
            cfg.u.bss_info.vapStatsEnable = true;
            cfg.u.bss_info.rapidReconnectEnable = true;
        } else {
            cfg.u.bss_info.vapStatsEnable = false;
            cfg.u.bss_info.rapidReconnectEnable = false;
        }
        cfg.u.bss_info.rapidReconnThreshold = 180;
        cfg.u.bss_info.mac_filter_enable = false;
        cfg.u.bss_info.UAPSDEnabled = true;
        cfg.u.bss_info.wmmNoAck = true;
        cfg.u.bss_info.wepKeyLength = 128;
        if (i == 4 || i == 5) {
            cfg.u.bss_info.bssHotspot = true;
        } else {
            cfg.u.bss_info.bssHotspot = false;
        }
        if (i % 2 == 0) {
            strncpy(cfg.u.bss_info.beaconRateCtl,"6Mbps",sizeof(cfg.u.bss_info.beaconRateCtl)-1);
        }
#ifdef WIFI_HAL_VERSION_3
        cfg.u.bss_info.security.mfp = wifi_mfp_cfg_disabled;
#else
        strncpy(cfg.u.bss_info.security.mfpConfig,"Disabled",sizeof(cfg.u.bss_info.security.mfpConfig)-1);
#endif
        if (convert_radio_to_name((i%2)+1, radio_name) == 0) {
            if (wifidb_update_wifi_vap_info(radio_name,&cfg) == RETURN_OK) {
                wifi_db_dbg_print(1,"%s:%d: Updated vap %d config successfully \n",__func__, __LINE__,i);
            } else {
                wifi_db_dbg_print(1,"%s:%d: Failed to update vap %d config\n",__func__, __LINE__,i);
            }
        }
    }
    return RETURN_OK;
}

int  wifidb_update_wifi_macfilter_config(unsigned int apIndex)
{
    unsigned int macfilterCount = 0;
    unsigned int cosaApInstance = apIndex + 1;
    unsigned int index = 0, i = 0;
    int retPsmGet = CCSP_SUCCESS;
    errno_t  rc  =  -1;
    char recName[256];
    char *strValue;
    char *macFilterList;
    //      unsigned int macInstanceNum = 0;
    COSA_DML_WIFI_AP_MAC_FILTER cosaApMacFilter;
    char *devName;
    char *devMac;
    int ret = 0;

    wifi_db_dbg_print(1, "%s:%d Enter \n", __func__, __LINE__);
    macfilterCount  = CosaDmlMacFilt_GetNumberOfEntries(cosaApInstance);
    for (index = 0; index < macfilterCount; index++)
    {
        memset(&cosaApMacFilter, 0, sizeof(COSA_DML_WIFI_AP_MAC_FILTER));
        i = 0;
        rc = sprintf_s(recName, sizeof(recName) , MacFilterList, cosaApInstance);
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
        if (retPsmGet == CCSP_SUCCESS)
        {
            if ((macFilterList = strstr(strValue, ":" )))
            {
                macFilterList += 1;
                while (i < index  && macFilterList != NULL )
                {
                    i++;
                    if ((macFilterList = strstr(macFilterList,",")))
                    {
                        if(macFilterList == NULL)
                        {
                            break;
                        }
                        macFilterList += 1;
                    }
                }
                if (i == index && macFilterList != NULL)
                {
                    cosaApMacFilter.InstanceNumber = _ansc_atoi(macFilterList);
                    memset(recName, 0, sizeof(recName));
                    snprintf(recName, sizeof(recName), MacFilter, cosaApInstance, cosaApMacFilter.InstanceNumber);
                    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &devMac);
                    if (retPsmGet == CCSP_SUCCESS)
                    {
                        rc = sprintf_s(cosaApMacFilter.MACAddress, sizeof(cosaApMacFilter.MACAddress) ,"%s",devMac);
                        if(rc < EOK)
                        {
                            ERR_CHK(rc);
                        }
                        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(devMac);
                    }

                    rc = sprintf_s(recName, sizeof(recName), MacFilterDevice, cosaApInstance, cosaApMacFilter.InstanceNumber);
                    if(rc < EOK)
                    {
                        ERR_CHK(rc);
                    }

                    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &devName);
                    if (retPsmGet == CCSP_SUCCESS)
                    {
                        rc = strcpy_s(cosaApMacFilter.DeviceName, sizeof(cosaApMacFilter.DeviceName), devName);
                        ERR_CHK(rc);
                        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(devName);
                    }

                    if (wifidb_add_wifi_macfilter_config((unsigned int)cosaApInstance-1, &cosaApMacFilter) != 0)
                    {
                        wifidb_print("%s: WIFI DB update error !!!. Failed to update add macfilter for apIndex %d\n",__func__, apIndex);
                        ret = -1;
                    }
                    else
                    {
                        wifidb_print("%s Updated WIFI DB. updated to addition of macfilter successfully\n",__func__);
                    }

                }
                ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
            }

        }

    }
    return ret;
}

#ifdef WIFI_HAL_VERSION_3
int wifidb_update_vapmap_to_db(UINT radioIndex, wifi_vap_info_map_t *vapMapDb)
{
    rdk_wifi_vap_map_t *rdkWifiVap = NULL;
    wifi_vap_info_map_t tempVapMapDb;
    UINT vapCount = 0, vapCountDb = 0;
    wifi_radio_operationParam_t *wifiRadioOperParam = NULL;
    UINT rdkVapCount = 0;
    BOOL isValidVap = FALSE;
    char rdkVapName[32];

    if (vapMapDb == NULL)
    {
        wifi_db_dbg_print(1,"%s input arguements are NULL\n", __FUNCTION__);
        return RETURN_ERR;
    }

    memset(&tempVapMapDb, 0, sizeof(wifi_vap_info_map_t));

    rdkWifiVap = getRdkWifiVap(radioIndex);
    if (rdkWifiVap == NULL)
    {
        wifi_db_dbg_print(1,"%s getRdkWifiVap failed for %d \n", __FUNCTION__, radioIndex);
        return RETURN_ERR;
    }

    wifiRadioOperParam = getRadioOperationParam(radioIndex);
    if (wifiRadioOperParam == NULL)
    {
        wifi_db_dbg_print(1, "%s Input radioIndex = %d not found for wifiRadioOperParam\n", __FUNCTION__, radioIndex);
        return RETURN_ERR;
    }


    for (vapCount = 0; vapCount < vapMapDb->num_vaps; vapCount++)
    {
        memset(rdkVapName, 0, sizeof(rdkVapName));
        isValidVap = FALSE;
        for (rdkVapCount = 0; rdkVapCount < rdkWifiVap->num_vaps; rdkVapCount++)
        {
            if (rdkWifiVap->rdk_vap_array[rdkVapCount].vap_index == vapMapDb->vap_array[vapCount].vap_index)
            {
                snprintf(rdkVapName, sizeof(rdkVapName), "%s", (CHAR* )rdkWifiVap->rdk_vap_array[rdkVapCount].vap_name);
                if (strncmp((CHAR *)rdkVapName, "unused_", strlen("unused_")) != 0)
                {
                    isValidVap = TRUE;
                }
                break;
            }
        }
        if (isValidVap == TRUE)
        {
            memcpy(&tempVapMapDb.vap_array[vapCountDb], &vapMapDb->vap_array[vapCount], sizeof(wifi_vap_info_t));
            snprintf(tempVapMapDb.vap_array[vapCountDb].vap_name, sizeof(tempVapMapDb.vap_array[vapCountDb].vap_name), "%s", rdkVapName);
            vapCountDb++;
        }
        else
        {
            continue;
        }
    }

    tempVapMapDb.num_vaps = vapCountDb;
    wifi_db_dbg_print(1, "%s DB vapcount : %d for radioIndex:%d \n", __FUNCTION__, tempVapMapDb.num_vaps, radioIndex);

    if (wifidb_update_wifi_vap_config(radioIndex+1, &tempVapMapDb) != RETURN_OK)
    {
        wifi_db_dbg_print(1, "%s wifidb_update_wifi_vap_config failed for radioIndex : %d\n", __FUNCTION__, radioIndex);
        return RETURN_ERR;
    }

    return RETURN_OK;
}
#endif

void wifi_db_init()
{
    if (wifi_db_init_global_config_default() == RETURN_OK) {
        wifi_db_dbg_print(1,"%s:%d: Initialized Global Config successfully\n",__func__, __LINE__);
    }
    
    if (wifi_db_init_radio_config_default() == RETURN_OK) {
        wifi_db_dbg_print(1,"%s:%d: Initialized Radio Config successfully\n",__func__, __LINE__);
    }
 
    if (wifi_db_init_vap_config_default() == RETURN_OK) {
        wifi_db_dbg_print(1,"%s:%d: Initialized Vap Config successfully\n",__func__, __LINE__);
    }
    return;
}

void *evloop_func(void *arg)
{
	ev_run(g_wifidb.ovs_ev_loop, 0);
	return NULL;
}

int init_ovsdb_tables()
{
    unsigned int attempts = 0;

    OVSDB_TABLE_INIT(Wifi_Device_Config, device_mac);
    OVSDB_TABLE_INIT(Wifi_Security_Config,vap_name);
    OVSDB_TABLE_INIT(Wifi_Interworking_Config, vap_name);
    OVSDB_TABLE_INIT(Wifi_GAS_Config, advertisement_id);
    OVSDB_TABLE_INIT(Wifi_VAP_Config, vap_name);
    OVSDB_TABLE_INIT(Wifi_Radio_Config, radio_name);
    OVSDB_TABLE_INIT_NO_KEY(Wifi_Global_Config);
    OVSDB_TABLE_INIT(Wifi_MacFilter_Config, macfilter_key);
    
    //connect to ovsdb with sock path
    snprintf(g_wifidb.ovsdb_sock_path, sizeof(g_wifidb.ovsdb_sock_path), "%s/wifidb.sock", OVSDB_RUN_DIR);

    while (attempts < 3) {
        if ((g_wifidb.ovsdb_fd = ovsdb_conn(g_wifidb.ovsdb_sock_path)) < 0) {
            wifi_db_dbg_print(1,"%s:%d:Failed to connect to ovsdb at %s\n",
                __func__, __LINE__, g_wifidb.ovsdb_sock_path);
            attempts++;
            sleep(1);
            if (attempts == 3) {
            wifi_db_dbg_print(1,"%s:%d:Failed to connect to ovsdb at %s attempts : %d\n",
                __func__, __LINE__, g_wifidb.ovsdb_sock_path, attempts);
                return -1;
            }
        } else {
            break;
        }
    }
    wifi_db_dbg_print(1,"%s:%d:Connection to ovsdb at %s successful\n",
            __func__, __LINE__, g_wifidb.ovsdb_sock_path);

    return 0;
}

void *start_ovsdb_func(void *arg)
{
    char cmd[1024];
    char db_file[128];
    struct stat sb;
    bool debug_option = false;
    DIR     *ovsDbDir = NULL;
    errno_t rc = -1;
    
    ovsDbDir = opendir(OVSDB_DIR);
    if(ovsDbDir){
        closedir(ovsDbDir);
    }else if(ENOENT == errno){
        if(0 != mkdir(OVSDB_DIR, 0777)){ 
            wifi_db_dbg_print(1,"Failed to Create OVSDB directory.\n");
            return NULL;
        }
    }else{
        wifi_db_dbg_print(1,"Error opening Db Configuration directory. Setting Default\n");
        return NULL;
    }
//create a copy of ovs-db server
    rc = sprintf_s(cmd, sizeof(cmd), "cp /usr/sbin/ovsdb-server %s/wifidb-server", OVSDB_RUN_DIR);
    if(rc < EOK)
    {
        ERR_CHK(rc);
    }
    system(cmd);
    rc = sprintf_s(db_file, sizeof(db_file), "%s/rdkb-wifi.db", OVSDB_DIR);
    if(rc < EOK)
    {
        ERR_CHK(rc);
    }
    if (stat(db_file, &sb) != 0) {
	wifi_db_dbg_print(1,"%s:%d: Could not find rdkb database, ..creating\n", __func__, __LINE__);
        rc = sprintf_s(cmd, sizeof(cmd), "ovsdb-tool create %s %s/rdkb-wifi.ovsschema", db_file, OVSDB_SCHEMA_DIR);
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        system(cmd);
    } else {
        wifi_db_dbg_print(1,"%s:%d: rdkb database already present\n", __func__, __LINE__);
        rc = sprintf_s(cmd, sizeof(cmd), "ovsdb-tool convert %s %s/rdkb-wifi.ovsschema",db_file,OVSDB_SCHEMA_DIR);
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        wifi_db_dbg_print(1,"%s:%d: rdkb database check for version upgrade/downgrade %s\n", __func__, __LINE__,cmd);
        system(cmd);
    }
   
    rc = sprintf_s(cmd, sizeof(cmd), "%s/wifidb-server %s --remote=punix:%s/wifidb.sock %s --unixctl=%s/wifi.ctl --log-file=%s/wifidb.log --detach", OVSDB_RUN_DIR, db_file, OVSDB_RUN_DIR, (debug_option == true)?"--verbose=dbg":"", OVSDB_RUN_DIR, OVSDB_RUN_DIR); 
    if(rc < EOK)
    {
        ERR_CHK(rc);
    }
    
    system(cmd); 
    
    return NULL;
}

int start_ovsdb()
{
    wifidb_wfd = -1;
    g_wifidb.ovsdb_fd = -1;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );

    pthread_create(&g_wifidb.ovsdb_thr_id, &attr, start_ovsdb_func, NULL);
	
    return 0;
}

void ovsdb_cleanup()
{
    char cmd[1024];
    errno_t rc = -1;
    wifi_db_dbg_print(1,"%s:%d: ovsdb cleanup called\n", __func__, __LINE__);
    if (wifidb_wfd >= 0)
    {
        close(wifidb_wfd);
    }

    if (g_wifidb.ovsdb_fd >= 0)
    {
        close(g_wifidb.ovsdb_fd);
    }
    g_wifidb.ovsdb_fd = 0;
    wifidb_wfd = 0;
    unlink(g_wifidb.ovsdb_sock_path);
    rc = sprintf_s(cmd, sizeof(cmd), "%s/wifi.ctl", OVSDB_RUN_DIR);
    if(rc < EOK)
    {
        ERR_CHK(rc);
    }
    unlink(cmd);
    system("killall -9 wifidb-server");
}


