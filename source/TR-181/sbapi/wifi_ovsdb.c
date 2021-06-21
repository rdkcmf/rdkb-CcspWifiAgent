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
#include <cJSON.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "cJSON.h"
#include "os.h"
#include "util.h"
#include "ovsdb.h"
#include "ovsdb_update.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"
#include "ovsdb_cache.h"
#include "schema.h"
#include "ds.h"
#include "json_util.h"
#include "target.h"
#include <ev.h>
#include "dirent.h"
wifi_ovsdb_t	g_wifidb;
ovsdb_table_t table_Wifi_VAP_Config;
ovsdb_table_t table_Wifi_Security_Config;
ovsdb_table_t table_Wifi_Device_Config;
ovsdb_table_t table_Wifi_Interworking_Config;
ovsdb_table_t table_Wifi_GAS_Config;
#define OVSDB_SCHEMA_DIR "/usr/ccsp/wifi"
#define OVSDB_DIR "/nvram/wifi"
#define OVSDB_RUN_DIR "/var/tmp"
void callback_Wifi_Device_Config(ovsdb_update_monitor_t *mon,
        struct schema_Wifi_Device_Config *old_rec,
        struct schema_Wifi_Device_Config *new_rec)
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
void callback_Wifi_Security_Config(ovsdb_update_monitor_t *mon,
        struct schema_Wifi_Security_Config *old_rec,
        struct schema_Wifi_Security_Config *new_rec)
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
void callback_Wifi_Interworking_Config(ovsdb_update_monitor_t *mon,
        struct schema_Wifi_Interworking_Config *old_rec,
        struct schema_Wifi_Interworking_Config *new_rec)
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
void callback_Wifi_VAP_Config(ovsdb_update_monitor_t *mon,
        struct schema_Wifi_VAP_Config *old_rec,
        struct schema_Wifi_VAP_Config *new_rec)
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
void callback_Wifi_GAS_Config(ovsdb_update_monitor_t *mon,
        struct schema_Wifi_GAS_Config *old_rec,
        struct schema_Wifi_GAS_Config *new_rec)
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
int update_wifi_ovsdb_security(wifi_vap_info_t *vap_info, struct schema_Wifi_Security_Config *cfg, bool update)
{
    struct schema_Wifi_Security_Config *pcfg;
    json_t *where;
	int count;
	struct in_addr addr;
	int ret;
	if (update == true) {
		where = ovsdb_tran_cond(OCLM_UUID, "_uuid", OFUNC_EQ, cfg->_uuid.uuid);
    pcfg = ovsdb_table_select_where(g_wifidb.ovsdb_sock_path, &table_Wifi_Security_Config, where, &count);
		if ((count != 0) && (pcfg != NULL)) {
			assert(count == 1);
			memcpy(cfg, pcfg, sizeof(struct schema_Wifi_Security_Config));
			free(pcfg);
		}
	}
	wifi_passpoint_dbg_print("%s:%d: Found %d records with key: %s in Wifi Device table\n", 
    	__func__, __LINE__, count, vap_info->vap_name);
	strcpy(cfg->onboard_type, "manual");
	sprintf(cfg->security_mode, "%d", vap_info->u.bss_info.security.mode);
	sprintf(cfg->encryption_method, "%d", vap_info->u.bss_info.security.encr);
	if (vap_info->u.bss_info.security.mode < wifi_security_mode_wpa_enterprise) {
		strcpy(cfg->passphrase, vap_info->u.bss_info.security.u.key.key);
		strcpy(cfg->radius_server_ip, "");
		strcpy(cfg->radius_server_port, "");
		strcpy(cfg->radius_server_key, "");
		strcpy(cfg->secondary_radius_server_ip, "");
		strcpy(cfg->secondary_radius_server_port, "");
		strcpy(cfg->secondary_radius_server_key, "");
	} else {
		strcpy(cfg->passphrase, "");
		addr.s_addr = (in_addr_t)vap_info->u.bss_info.security.u.radius.ip;
		strcpy(cfg->radius_server_ip, inet_ntoa(addr));
		sprintf(cfg->radius_server_port, "%d", vap_info->u.bss_info.security.u.radius.port);
		strcpy(cfg->radius_server_key, vap_info->u.bss_info.security.u.radius.key);
		addr.s_addr = (in_addr_t)vap_info->u.bss_info.security.u.radius.s_ip;
		strcpy(cfg->secondary_radius_server_ip, inet_ntoa(addr));
		sprintf(cfg->secondary_radius_server_port, "%d", vap_info->u.bss_info.security.u.radius.s_port);
		strcpy(cfg->secondary_radius_server_key, vap_info->u.bss_info.security.u.radius.s_key);
	}
	if (update == true) {
		where = ovsdb_tran_cond(OCLM_UUID, "_uuid", OFUNC_EQ, cfg->_uuid.uuid);
    	ret = ovsdb_table_update_where(g_wifidb.ovsdb_sock_path, &table_Wifi_Security_Config, where, cfg);
		if (ret == -1) {
			wifi_passpoint_dbg_print("%s:%d: failed to update to table_Wifi_Security_Config table\n", 
				__func__, __LINE__);
			return -1;
		} else if (ret == 0) {
			wifi_passpoint_dbg_print("%s:%d: nothing to update to table_Wifi_Security_Config table\n", 
				__func__, __LINE__);
		} else {
			wifi_passpoint_dbg_print("%s:%d: update to table_Wifi_Security_Config table successful\n", 
				__func__, __LINE__);
		}
	} else {
    	if (ovsdb_table_insert(g_wifidb.ovsdb_sock_path, &table_Wifi_Security_Config, cfg) 
				== false) {
			wifi_passpoint_dbg_print("%s:%d: failed to insert in table_Wifi_Security_Config table\n", 
				__func__, __LINE__);
			return -1;
		} else {
			wifi_passpoint_dbg_print("%s:%d: insert in table_Wifi_Security_Config table successful\n", 
				__func__, __LINE__);
		}
	}
	return 0;
}
	
/*int update_wifi_ovsdb_mac_filter(wifi_vap_info_t *vap_info, hash_map_t *mac_filter_map)
{
	int ret;
	unsigned int count;
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
    where = ovsdb_tran_cond(OCLM_STR, "vap_name", OFUNC_EQ, vap_name);
    pcfg = ovsdb_table_select_where(g_wifidb.ovsdb_sock_path, &table_Wifi_Interworking_Config, where, &count);
	if ((count != 0) && (pcfg != NULL)) {
		assert(count == 1);
		memcpy(&cfg, pcfg, sizeof(struct schema_Wifi_Interworking_Config));
		update = true;
		free(pcfg);
	}
	wifi_passpoint_dbg_print("%s:%d: Found %d records with key: %s in Wifi VAP table\n", 
    	__func__, __LINE__, count, vap_name);
	strcpy(cfg.vap_name, vap_name);
        cfg.enable = interworking->interworkingEnabled;
        cfg.access_network_type = interworking->accessNetworkType;
        cfg.internet = interworking->internetAvailable;
        cfg.asra = interworking->asra;
        cfg.esr = interworking->esr;
        cfg.uesa = interworking->uesa;
        cfg.hess_option_present = interworking->hessOptionPresent;
	strcpy(cfg.hessid, interworking->hessid);
        cfg.venue_group = interworking->venueGroup;
        cfg.venue_type = interworking->venueType;
	if (update == true) {
		where = ovsdb_tran_cond(OCLM_STR, "vap_name", OFUNC_EQ, vap_name);
    	ret = ovsdb_table_update_where(g_wifidb.ovsdb_sock_path, &table_Wifi_Interworking_Config, where, &cfg);
		if (ret == -1) {
			wifi_passpoint_dbg_print("%s:%d: failed to update table_Wifi_Interworking_Config table\n", 
				__func__, __LINE__);
			return -1;
		} else if (ret == 0) {
			wifi_passpoint_dbg_print("%s:%d: nothing to update table_Wifi_Interworking_Config table\n", 
				__func__, __LINE__);
		} else {
			wifi_passpoint_dbg_print("%s:%d: update to table_Wifi_Interworking_Config table successful\n", 
				__func__, __LINE__);
		}
	} else {
    	if (ovsdb_table_insert(g_wifidb.ovsdb_sock_path, &table_Wifi_Interworking_Config, &cfg) == false) {
			wifi_passpoint_dbg_print("%s:%d: failed to insert in table_Wifi_Interworking_Config\n", 
				__func__, __LINE__);
			return -1;
		} else {
			wifi_passpoint_dbg_print("%s:%d: insert in table_Wifi_Interworking_Config table successful\n", 
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
        wifi_passpoint_dbg_print("%s:%d: table not table_Wifi_Interworking_Config not found\n",__func__, __LINE__);
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
    int  i;
    char *vap_name[] = {"private_ssid_2g", "private_ssid_5g", "iot_ssid_2g", "iot_ssid_5g", "hotspot_open_2g", "hotspot_open_5g", "lnf_psk_2g", "lnf_psk_5g", "hotspot_secure_2g", "hotspot_secure_5g"};
    
    char output[4096];
    wifi_passpoint_dbg_print("OVSDB JSON\nname:Open_vSwitch, version:1.00.000\n");
    wifi_passpoint_dbg_print("table: Wifi_Interworking_Config \n");
    for (i=0; i < 10; i++) {
        where = ovsdb_tran_cond(OCLM_STR, "vap_name", OFUNC_EQ, vap_name[i]);
        pcfg = ovsdb_table_select_where(g_wifidb.ovsdb_sock_path, &table_Wifi_Interworking_Config, where, &count);
    
        if ((pcfg == NULL) || (!count)) {
            continue;
            //wifi_passpoint_dbg_print("%s:%d: table table_Wifi_Interworking_Config not found\n",__func__, __LINE__);
            //return;
        }
        //wifi_passpoint_dbg_print("%s:%d: Found %d records with key: %s in Wifi_Interworking_Config table\n",
        //__func__, __LINE__, count, vap_name[i]);
        json_t *data_base = ovsdb_table_to_json(&table_Wifi_Interworking_Config, pcfg);
        if(data_base) {
            memset(output,0,sizeof(output));
            if(json_get_str(data_base,output, sizeof(output))) {
                wifi_passpoint_dbg_print("key: %s\nCount: %d\n%s\n",
                   vap_name[i],count,output);
            } else {
                wifi_passpoint_dbg_print("%s:%d: Unable to print Row\n",
                   __func__, __LINE__);
            }
        }
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
void *evloop_func(void *arg)
{
        UNREFERENCED_PARAMETER(arg);
	ev_run(g_wifidb.ovs_ev_loop, 0);
	return NULL;
}
int init_ovsdb_tables()
{
    unsigned int attempts = 0;
    g_wifidb.ovs_ev_loop = ev_loop_new(0);
    if (!g_wifidb.ovs_ev_loop) {
        wifi_passpoint_dbg_print("%s:%d: Could not find default target_loop\n", __func__, __LINE__);
        return -1;
    }
	pthread_create(&g_wifidb.evloop_thr_id, NULL, evloop_func, NULL);
    if (ovsdb_init_loop(g_wifidb.ovsdb_fd, &g_wifidb.wovsdb, g_wifidb.ovs_ev_loop) == false) {
        wifi_passpoint_dbg_print("%s:%d: Could not find default target_loop\n", __func__, __LINE__);
        return -1;
    }
	OVSDB_TABLE_INIT(Wifi_Device_Config, device_mac);
	OVSDB_TABLE_INIT_NO_KEY(Wifi_Security_Config);
	OVSDB_TABLE_INIT(Wifi_Interworking_Config, vap_name);
	OVSDB_TABLE_INIT(Wifi_GAS_Config, advertisement_id);
	OVSDB_TABLE_INIT(Wifi_VAP_Config, vap_name);
    snprintf(g_wifidb.ovsdb_sock_path, sizeof(g_wifidb.ovsdb_sock_path), "%s/wifi.ctl", OVSDB_RUN_DIR);
    while (attempts < 3) {
        if ((g_wifidb.ovsdb_fd = ovsdb_conn(g_wifidb.ovsdb_sock_path)) < 0) {
            wifi_passpoint_dbg_print("%s:%d:Failed to connect to ovsdb at %s\n",
                __func__, __LINE__, g_wifidb.ovsdb_sock_path);
            attempts++;
            sleep(1);
            if (attempts == 3) {
                return -1;
            }
        } else {
            break;
        }
    }
    wifi_passpoint_dbg_print("%s:%d:Connection to ovsdb at %s successful\n",
            __func__, __LINE__, g_wifidb.ovsdb_sock_path);
	OVSDB_TABLE_MONITOR(g_wifidb.ovsdb_fd, Wifi_Device_Config, true);
	OVSDB_TABLE_MONITOR(g_wifidb.ovsdb_fd, Wifi_Security_Config, true);
	OVSDB_TABLE_MONITOR(g_wifidb.ovsdb_fd, Wifi_Interworking_Config, true);
	OVSDB_TABLE_MONITOR(g_wifidb.ovsdb_fd, Wifi_VAP_Config, true);
	OVSDB_TABLE_MONITOR(g_wifidb.ovsdb_fd, Wifi_GAS_Config, true);
	return 0;
}
void *start_ovsdb_func(void *arg)
{
    UNREFERENCED_PARAMETER(arg);
    char cmd[1024];
    char db_file[128];
    struct stat sb;
    bool debug_option = false;
    DIR     *ovsDbDir = NULL;
    
    ovsDbDir = opendir(OVSDB_DIR);
    if(ovsDbDir){
        closedir(ovsDbDir);
    }else if(ENOENT == errno){
        if(0 != mkdir(OVSDB_DIR, 0777)){ 
            wifi_passpoint_dbg_print("Failed to Create OVSDB directory.\n");
            return NULL;
        }
    }else{
        wifi_passpoint_dbg_print("Error opening Passpoint Configuration directory. Setting Default\n");
        return NULL;
    }
//create a copy of ovs-db server
    sprintf(cmd, "cp /usr/sbin/ovsdb-server %s/wifidb-server", OVSDB_RUN_DIR);
    system(cmd);
    sprintf(db_file, "%s/rdkb-wifi.db", OVSDB_DIR);	
    if (stat(db_file, &sb) != 0) {
	wifi_passpoint_dbg_print("%s:%d: Could not find rdkb database, ..creating\n", __func__, __LINE__);
        sprintf(cmd, "ovsdb-tool create %s %s/rdkb-wifi.ovsschema", db_file, OVSDB_SCHEMA_DIR);
    	system(cmd);
    } else {
	wifi_passpoint_dbg_print("%s:%d: rdkb database already present\n", __func__, __LINE__);
    }
    
    sprintf(cmd, "%s/wifidb-server %s/rdkb-wifi.db --remote punix:%s/wifi.ctl %s --unixctl=%s/wifidb.sock", OVSDB_RUN_DIR, OVSDB_DIR, OVSDB_RUN_DIR, (debug_option == true)?"--verbose=dbg":"", OVSDB_RUN_DIR);
    
    system(cmd); 
    
    return NULL;
}
int start_ovsdb()
{
	pthread_create(&g_wifidb.ovsdb_thr_id, NULL, start_ovsdb_func, NULL);
	
    return 0;
}
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
    sprintf(index,"%d",advertisement_id);
    where = ovsdb_tran_cond(OCLM_STR, "advertisement_id", OFUNC_EQ, index);
    pcfg = ovsdb_table_select_where(g_wifidb.ovsdb_sock_path, &table_Wifi_GAS_Config, where, &count);
    if ((count != 0) && (pcfg != NULL)) {
	//assert(count == 1);
        wifi_passpoint_dbg_print("%s:%d: Found %d records with key: %d in Wifi GAS table\n", 
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
	    wifi_passpoint_dbg_print("%s:%d: failed to update table_Wifi_GAS_Config table\n", 
		__func__, __LINE__);
	    return -1;
	} else if (ret == 0) {
	    wifi_passpoint_dbg_print("%s:%d: nothing to update table_Wifi_GAS_Config table\n", 
		__func__, __LINE__);
	} else {
	    wifi_passpoint_dbg_print("%s:%d: update to table_Wifi_GAS_Config table successful\n", 
		__func__, __LINE__);
	}
    } else {
	strcpy(cfg.advertisement_id,index);
    	if (ovsdb_table_insert(g_wifidb.ovsdb_sock_path, &table_Wifi_GAS_Config, &cfg) == false) {
	    wifi_passpoint_dbg_print("%s:%d: failed to insert in table_Wifi_GAS_Config\n", 
		__func__, __LINE__);
	    return -1;
	} else {
	    wifi_passpoint_dbg_print("%s:%d: insert in table_Wifi_GAS_Config table successful\n", 
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
    sprintf(index,"%d",advertisement_id);
    where = ovsdb_tran_cond(OCLM_STR, "advertisement_id", OFUNC_EQ, index);
    pcfg = ovsdb_table_select_where(g_wifidb.ovsdb_sock_path, &table_Wifi_GAS_Config, where, &count);
    if (pcfg == NULL) {
        wifi_passpoint_dbg_print("%s:%d: table table_Wifi_GAS_Config not found\n",__func__, __LINE__);
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
