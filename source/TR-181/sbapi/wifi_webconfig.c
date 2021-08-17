/************************************************************************************
  If not stated otherwise in this file or this component's Licenses.txt file the
  following copyright and licenses apply:

  Copyright 2018 RDK Management

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
**************************************************************************/
#if defined (FEATURE_SUPPORT_WEBCONFIG)
#include "cosa_apis.h"
#include "cosa_dbus_api.h"
#include "cosa_wifi_apis.h"
#include "cosa_wifi_internal.h"
#include "ccsp_psm_helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include "wifi_hal.h"
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/un.h>
#include <assert.h>
#include "ansc_status.h"
#include <sysevent/sysevent.h>
#include <arpa/inet.h>
#include "plugin_main_apis.h"
#include "ctype.h"
#include "ccsp_WifiLog_wrapper.h"
#include "secure_wrapper.h"
#include "collection.h"
#include "msgpack.h"
#include "wifi_webconfig.h"
#include "wifi_validator.h"
#include "cosa_wifi_passpoint.h"
#include "wifi_monitor.h"

#define WEBCONF_SSID           0
#define WEBCONF_SECURITY       1
#define SUBDOC_COUNT           3
#define MULTISUBDOC_COUNT      1
#define SSID_DEFAULT_TIMEOUT   90
#define XB6_DEFAULT_TIMEOUT   15

static char *WiFiSsidVersion = "eRT.com.cisco.spvtg.ccsp.Device.WiFi.%s_version";

static bool SSID1_UPDATED = FALSE;
static bool SSID2_UPDATED = FALSE;
static bool PASSPHRASE1_UPDATED = FALSE;
static bool PASSPHRASE2_UPDATED = FALSE;
static bool gHostapd_restart_reqd = FALSE;
static bool gbsstrans_support[WIFI_INDEX_MAX];
static bool gwirelessmgmt_support[WIFI_INDEX_MAX];
/*static int hotspot_vaps[4] = {4,5,8,9};*/
static bool gradio_restart[2];

webconf_apply_t apply_params;
extern PCOSA_BACKEND_MANAGER_OBJECT g_pCosaBEManager;
extern ANSC_HANDLE bus_handle;
extern char   g_Subsystem[32];
webconf_wifi_t *curr_config = NULL;
extern COSA_DML_WIFI_SSID_CFG sWiFiDmlSsidStoredCfg[WIFI_INDEX_MAX];
extern COSA_DML_WIFI_AP_FULL sWiFiDmlApStoredCfg[WIFI_INDEX_MAX];
extern COSA_DML_WIFI_APSEC_FULL  sWiFiDmlApSecurityStored[WIFI_INDEX_MAX];
extern COSA_DML_WIFI_SSID_CFG sWiFiDmlSsidRunningCfg[WIFI_INDEX_MAX];
extern COSA_DML_WIFI_AP_FULL sWiFiDmlApRunningCfg[WIFI_INDEX_MAX];
extern COSA_DML_WIFI_APSEC_FULL  sWiFiDmlApSecurityRunning[WIFI_INDEX_MAX];
extern PCOSA_DML_WIFI_AP_MF_CFG  sWiFiDmlApMfCfg[WIFI_INDEX_MAX];
extern QUEUE_HEADER *sWiFiDmlApMfQueue[WIFI_INDEX_MAX];
extern BOOL g_newXH5Gpass;

extern void configWifi(BOOLEAN redirect);

#if 0
static char *NotifyWiFi = "eRT.com.cisco.spvtg.ccsp.Device.WiFi.NotifyWiFiChanges" ;
static char *WiFiRestored_AfterMig = "eRT.com.cisco.spvtg.ccsp.Device.WiFi.WiFiRestored_AfterMigration" ;
static char *FR   = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.FactoryReset";
#endif

extern char notifyWiFiChangesVal[16] ;


static char *ApIsolationEnable    = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.ApIsolationEnable";
static char *BSSTransitionActivated    = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.BSSTransitionActivated";
static char *vAPStatsEnable = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.vAPStatsEnable";
static char *NeighborReportActivated     = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.X_RDKCENTRAL-COM_NeighborReportActivated";
static char *RapidReconnThreshold        = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.RapidReconnThreshold";
static char *RapidReconnCountEnable      = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.RapidReconnCountEnable";
static char *BssMaxNumSta       = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.BssMaxNumSta";
static char *ApMFPConfig         = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.Security.MFPConfig";

wifi_vap_info_map_t vap_curr_cfg;
wifi_config_t  wifi_cfg;

extern char notifyWiFiChangesVal[16] ;

extern UINT g_interworking_RFC;
extern UINT g_passpoint_RFC;

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/

/* Function to convert authentication mode integer to string */
void webconf_auth_mode_to_str(char *auth_mode_str, COSA_DML_WIFI_SECURITY sec_mode) 
{
    switch(sec_mode)
    {
#ifndef _XB6_PRODUCT_REQ_
    case COSA_DML_WIFI_SECURITY_WEP_64:
        strcpy(auth_mode_str, "WEP-64");
        break;
    case COSA_DML_WIFI_SECURITY_WEP_128:
        strcpy(auth_mode_str, "WEP-128");
        break;
    case COSA_DML_WIFI_SECURITY_WPA_Personal:
        strcpy(auth_mode_str, "WPA-Personal");
        break;
    case COSA_DML_WIFI_SECURITY_WPA_Enterprise:
        strcpy(auth_mode_str, "WPA-Enterprise");
        break;
    case COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal:
        strcpy(auth_mode_str, "WPA-WPA2-Personal");
        break;
    case COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise:
        strcpy(auth_mode_str, "WPA-WPA2-Enterprise");
        break;
#endif
    case COSA_DML_WIFI_SECURITY_WPA2_Personal:
        strcpy(auth_mode_str, "WPA2-Personal");
        break;
    case COSA_DML_WIFI_SECURITY_WPA2_Enterprise:
        strcpy(auth_mode_str, "WPA2-Enterprise");
        break;
    case COSA_DML_WIFI_SECURITY_None:
        default:
        strcpy(auth_mode_str, "None");
        break;
    }
}

/* Function to convert Encryption mode integer to string */
void webconf_enc_mode_to_str(char *enc_mode_str,COSA_DML_WIFI_AP_SEC_ENCRYPTION enc_mode)
{
    switch(enc_mode)
    {
    case COSA_DML_WIFI_AP_SEC_TKIP:
        strcpy(enc_mode_str, "TKIP");
        break;
    case COSA_DML_WIFI_AP_SEC_AES:
        strcpy(enc_mode_str, "AES");
        break;
    case COSA_DML_WIFI_AP_SEC_AES_TKIP:
        strcpy(enc_mode_str, "AES+TKIP");
        break;
    default:
        strcpy(enc_mode_str, "None");
        break;
    }
}

/* Function to convert Authentication mode string to Integer */
void webconf_auth_mode_to_int(char *auth_mode_str, COSA_DML_WIFI_SECURITY * auth_mode)
{
    if (strcmp(auth_mode_str, "None") == 0 ) {
        *auth_mode = COSA_DML_WIFI_SECURITY_None;
    }
#ifndef _XB6_PRODUCT_REQ_
    else if (strcmp(auth_mode_str, "WEP-64") == 0) {
        *auth_mode = COSA_DML_WIFI_SECURITY_WEP_64;
    } else if (strcmp(auth_mode_str, "WEP-128") == 0) {
        *auth_mode = COSA_DML_WIFI_SECURITY_WEP_128;
    }
    else if (strcmp(auth_mode_str, "WPA-Personal") == 0) {
        *auth_mode = COSA_DML_WIFI_SECURITY_WPA_Personal;
    } else if (strcmp(auth_mode_str, "WPA2-Personal") == 0) {
        *auth_mode = COSA_DML_WIFI_SECURITY_WPA2_Personal;
    } else if (strcmp(auth_mode_str, "WPA-Enterprise") == 0) {
        *auth_mode = COSA_DML_WIFI_SECURITY_WPA_Enterprise;
    } else if (strcmp(auth_mode_str, "WPA2-Enterprise") == 0) {
        *auth_mode = COSA_DML_WIFI_SECURITY_WPA2_Enterprise;
    } else if (strcmp(auth_mode_str, "WPA-WPA2-Personal") == 0) {
        *auth_mode = COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal;
    } else if (strcmp(auth_mode_str, "WPA-WPA2-Enterprise") == 0) {
        *auth_mode = COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise;
    }
#else
    else if ((strcmp(auth_mode_str, "WPA2-Personal") == 0)) {
        *auth_mode = COSA_DML_WIFI_SECURITY_WPA2_Personal;
    }
    else if (strcmp(auth_mode_str, "WPA2-Enterprise") == 0) {
        *auth_mode = COSA_DML_WIFI_SECURITY_WPA2_Enterprise;
    }
#endif 
}

/* Function to convert Encryption mode string to integer */
void webconf_enc_mode_to_int(char *enc_mode_str, COSA_DML_WIFI_AP_SEC_ENCRYPTION *enc_mode)
{
    if ((strcmp(enc_mode_str, "TKIP") == 0)) {
        *enc_mode = COSA_DML_WIFI_AP_SEC_TKIP;
    } else if ((strcmp(enc_mode_str, "AES") == 0)) {
        *enc_mode = COSA_DML_WIFI_AP_SEC_AES;
    }
#ifndef _XB6_PRODUCT_REQ_
    else if ((strcmp(enc_mode_str, "AES+TKIP") == 0)) {
        *enc_mode = COSA_DML_WIFI_AP_SEC_AES_TKIP;
    }
#endif
}

/**
 *  Function to populate TR-181 parameters to wifi structure
 *
 *  @param current_config  Pointer to current config wifi structure
 *
 *  returns 0 on success, error otherwise
 *
 */
int webconf_populate_initial_dml_config(webconf_wifi_t *current_config, uint8_t ssid)
{
    PCOSA_DATAMODEL_WIFI pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PSINGLE_LINK_ENTRY pSLinkEntry = NULL;
    PCOSA_DML_WIFI_SSID  pWifiSsid = NULL;
    PCOSA_DML_WIFI_AP      pWifiAp = NULL;
    int wlan_index=0,i;

    if (ssid == WIFI_WEBCONFIG_PRIVATESSID) {
        wlan_index = 0;
    } else if (ssid == WIFI_WEBCONFIG_HOMESSID) {
        wlan_index = 2;
    }

    for (i = wlan_index;i < (wlan_index+2);i++) {
        if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->SsidQueue, i)) == NULL) {
            CcspTraceError(("%s Data Model object not found!\n",__FUNCTION__));
            return RETURN_ERR;
        }

        if((pWifiSsid = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) {
            CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
            return RETURN_ERR;
        }
        if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->AccessPointQueue, i)) == NULL) {
            CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
            return RETURN_ERR;
        }

        if((pWifiAp = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) {
            CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
            return RETURN_ERR;
        }
 
        if ((i % 2) == 0) {
            strncpy(current_config->ssid_2g.ssid_name, pWifiSsid->SSID.Cfg.SSID,COSA_DML_WIFI_MAX_SSID_NAME_LEN);
            current_config->ssid_2g.enable = pWifiSsid->SSID.Cfg.bEnabled;
            current_config->ssid_2g.ssid_advertisement_enabled = pWifiAp->AP.Cfg.SSIDAdvertisementEnabled;
            strncpy(current_config->security_2g.passphrase, (char*)pWifiAp->SEC.Cfg.KeyPassphrase,
                     sizeof(current_config->security_2g.passphrase)-1);
            webconf_auth_mode_to_str(current_config->security_2g.mode_enabled,
                                    pWifiAp->SEC.Cfg.ModeEnabled);
            webconf_enc_mode_to_str(current_config->security_2g.encryption_method,
                                     pWifiAp->SEC.Cfg.EncryptionMethod);
        } else {
            strncpy(current_config->ssid_5g.ssid_name, pWifiSsid->SSID.Cfg.SSID,COSA_DML_WIFI_MAX_SSID_NAME_LEN);
            current_config->ssid_5g.enable = pWifiSsid->SSID.Cfg.bEnabled;
            current_config->ssid_5g.ssid_advertisement_enabled = pWifiAp->AP.Cfg.SSIDAdvertisementEnabled;
            strncpy(current_config->security_5g.passphrase, (char*)pWifiAp->SEC.Cfg.KeyPassphrase,
                    sizeof(current_config->security_5g.passphrase)-1);
            webconf_auth_mode_to_str(current_config->security_5g.mode_enabled,
                                    pWifiAp->SEC.Cfg.ModeEnabled);
            webconf_enc_mode_to_str(current_config->security_5g.encryption_method,
                                     pWifiAp->SEC.Cfg.EncryptionMethod);
        }

    }
    return RETURN_OK;
}
 

/**
 *  Allocates memory to store current configuration
 *  to use in case of rollback
 *
 *  returns 0 on success, error otherwise
 */
int webconf_alloc_current_cfg(uint8_t ssid) {
    if (!curr_config) {
        curr_config = (webconf_wifi_t *) malloc(sizeof(webconf_wifi_t));
        if (!curr_config) {
            CcspTraceError(("%s: Memory allocation error\n", __FUNCTION__));
            return RETURN_ERR;
        }
        memset(curr_config, 0, sizeof(webconf_wifi_t));
    }
    if (webconf_populate_initial_dml_config(curr_config, ssid) != RETURN_OK) {
        CcspTraceError(("%s: Failed to copy initial configs\n", __FUNCTION__));
        return RETURN_ERR;
    }
    return RETURN_OK;
}

int webconf_update_dml_params(webconf_wifi_t *ps, uint8_t ssid) 
{
    PCOSA_DATAMODEL_WIFI pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PSINGLE_LINK_ENTRY pSLinkEntry = NULL;
    PCOSA_DML_WIFI_SSID  pWifiSsid = NULL;
    PCOSA_DML_WIFI_AP      pWifiAp = NULL;
    uint8_t wlan_index = 0; 

    if (ssid == WIFI_WEBCONFIG_PRIVATESSID) {
        wlan_index = 0;        
    } else if (ssid == WIFI_WEBCONFIG_HOMESSID) {
        wlan_index = 2;
    }

    if (curr_config->ssid_2g.ssid_changed) {
        if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->SsidQueue, wlan_index)) == NULL) {
            CcspTraceError(("%s Data Model object not found!\n",__FUNCTION__));
            return RETURN_ERR;
        }

        if((pWifiSsid = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) {
            CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
            return RETURN_ERR;
        }
        if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->AccessPointQueue, wlan_index)) == NULL) {
            CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
            return RETURN_ERR;
        }

        if((pWifiAp = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) {
            CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
            return RETURN_ERR;
        }
        
        strncpy(pWifiSsid->SSID.Cfg.SSID, ps->ssid_2g.ssid_name, sizeof(pWifiSsid->SSID.Cfg.SSID)-1);
        pWifiSsid->SSID.Cfg.bEnabled = ps->ssid_2g.enable;
        pWifiAp->AP.Cfg.SSIDAdvertisementEnabled = ps->ssid_2g.ssid_advertisement_enabled;

        memcpy(&sWiFiDmlSsidStoredCfg[pWifiSsid->SSID.Cfg.InstanceNumber-1], &pWifiSsid->SSID.Cfg, sizeof(COSA_DML_WIFI_SSID_CFG));
        memcpy(&sWiFiDmlApStoredCfg[pWifiAp->AP.Cfg.InstanceNumber-1].Cfg, &pWifiAp->AP.Cfg, sizeof(COSA_DML_WIFI_AP_CFG));
        memcpy(&sWiFiDmlSsidRunningCfg[pWifiSsid->SSID.Cfg.InstanceNumber-1], &pWifiSsid->SSID.Cfg, sizeof(COSA_DML_WIFI_SSID_CFG));
        memcpy(&sWiFiDmlApRunningCfg[pWifiAp->AP.Cfg.InstanceNumber-1].Cfg, &pWifiAp->AP.Cfg, sizeof(COSA_DML_WIFI_AP_CFG));
        
        curr_config->ssid_2g.ssid_changed = false;
    }

    if (curr_config->ssid_5g.ssid_changed) {
        if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->SsidQueue, wlan_index+1)) == NULL) {
            CcspTraceError(("%s Data Model object not found!\n",__FUNCTION__));
            return RETURN_ERR;
        }

        if((pWifiSsid = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) {
            CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
            return RETURN_ERR;
        }
        if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->AccessPointQueue, wlan_index+1)) == NULL) {
            CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
            return RETURN_ERR;
        }

        if((pWifiAp = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) {
            CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
            return RETURN_ERR;
        }
        
        strncpy(pWifiSsid->SSID.Cfg.SSID, ps->ssid_5g.ssid_name, sizeof(pWifiSsid->SSID.Cfg.SSID)-1);
        pWifiSsid->SSID.Cfg.bEnabled = ps->ssid_5g.enable;
        pWifiAp->AP.Cfg.SSIDAdvertisementEnabled = ps->ssid_5g.ssid_advertisement_enabled;
        
        memcpy(&sWiFiDmlSsidStoredCfg[pWifiSsid->SSID.Cfg.InstanceNumber-1], &pWifiSsid->SSID.Cfg, sizeof(COSA_DML_WIFI_SSID_CFG));
        memcpy(&sWiFiDmlApStoredCfg[pWifiAp->AP.Cfg.InstanceNumber-1].Cfg, &pWifiAp->AP.Cfg, sizeof(COSA_DML_WIFI_AP_CFG));
        memcpy(&sWiFiDmlSsidRunningCfg[pWifiSsid->SSID.Cfg.InstanceNumber-1], &pWifiSsid->SSID.Cfg, sizeof(COSA_DML_WIFI_SSID_CFG));
        memcpy(&sWiFiDmlApRunningCfg[pWifiAp->AP.Cfg.InstanceNumber-1].Cfg, &pWifiAp->AP.Cfg, sizeof(COSA_DML_WIFI_AP_CFG));
        
        curr_config->ssid_5g.ssid_changed = false;
    }

    if (curr_config->security_2g.sec_changed) {
        if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->AccessPointQueue, wlan_index)) == NULL) {
            CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
            return RETURN_ERR;
        }

        if((pWifiAp = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) {
            CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
            return RETURN_ERR;
        }
        
        webconf_auth_mode_to_int(ps->security_2g.mode_enabled, &pWifiAp->SEC.Cfg.ModeEnabled); 
        strncpy((char*)pWifiAp->SEC.Cfg.KeyPassphrase, ps->security_2g.passphrase,sizeof(pWifiAp->SEC.Cfg.KeyPassphrase)-1);
        strncpy((char*)pWifiAp->SEC.Cfg.PreSharedKey, ps->security_2g.passphrase,sizeof(pWifiAp->SEC.Cfg.PreSharedKey)-1);
        webconf_enc_mode_to_int(ps->security_2g.encryption_method, &pWifiAp->SEC.Cfg.EncryptionMethod);

        memcpy(&sWiFiDmlApSecurityStored[wlan_index].Cfg, &pWifiAp->SEC.Cfg, sizeof(COSA_DML_WIFI_APSEC_CFG));
        memcpy(&sWiFiDmlApSecurityRunning[wlan_index].Cfg, &pWifiAp->SEC.Cfg, sizeof(COSA_DML_WIFI_APSEC_CFG));
        
        curr_config->security_2g.sec_changed = false;
    }

    if (curr_config->security_5g.sec_changed) {
        if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->AccessPointQueue, wlan_index+1)) == NULL) {
            CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
            return RETURN_ERR;
        }

        if((pWifiAp = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) {
            CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
            return RETURN_ERR;
        }

        webconf_auth_mode_to_int(ps->security_5g.mode_enabled, &pWifiAp->SEC.Cfg.ModeEnabled);
        strncpy((char*)pWifiAp->SEC.Cfg.KeyPassphrase, ps->security_5g.passphrase,sizeof(pWifiAp->SEC.Cfg.KeyPassphrase)-1);
        strncpy((char*)pWifiAp->SEC.Cfg.PreSharedKey, ps->security_5g.passphrase,sizeof(pWifiAp->SEC.Cfg.PreSharedKey)-1);
        webconf_enc_mode_to_int(ps->security_5g.encryption_method, &pWifiAp->SEC.Cfg.EncryptionMethod);
   
        memcpy(&sWiFiDmlApSecurityStored[wlan_index+1].Cfg, &pWifiAp->SEC.Cfg, sizeof(COSA_DML_WIFI_APSEC_CFG));
        memcpy(&sWiFiDmlApSecurityRunning[wlan_index+1].Cfg, &pWifiAp->SEC.Cfg, sizeof(COSA_DML_WIFI_APSEC_CFG));
        
        curr_config->security_5g.sec_changed = false;
    }
    return RETURN_OK;
}
 
/**
 *   Applies Wifi SSID Parameters
 *
 *   @param wlan_index   AP Index
 *
 *   @return 0 on sucess, error otherswise
 */
int webconf_apply_wifi_ssid_params (webconf_wifi_t *pssid_entry, uint8_t wlan_index,
                                    pErr execRetVal)
{
    int retval = RETURN_ERR;
    char *ssid = NULL;
    bool enable = false, adv_enable = false;
    webconf_ssid_t *wlan_ssid = NULL, *cur_conf_ssid = NULL;
    BOOLEAN bForceDisableFlag = FALSE;

    if ((wlan_index % 2) ==0) {
        wlan_ssid = &pssid_entry->ssid_2g;
        cur_conf_ssid = &curr_config->ssid_2g;
    } else {
        wlan_ssid = &pssid_entry->ssid_5g;
        cur_conf_ssid = &curr_config->ssid_5g;
    }
    ssid = wlan_ssid->ssid_name;
    enable = wlan_ssid->enable;
    adv_enable = wlan_ssid->ssid_advertisement_enabled;

    if(ANSC_STATUS_FAILURE == CosaDmlWiFiGetCurrForceDisableWiFiRadio(&bForceDisableFlag))
    {
        CcspTraceError(("%s Failed to fetch ForceDisableWiFiRadio flag!!!\n",__FUNCTION__));
    }

    /* Apply SSID values to hal */
    if ((strcmp(cur_conf_ssid->ssid_name, ssid) != 0) && (!bForceDisableFlag)) {
        CcspTraceInfo(("RDKB_WIFI_CONFIG_CHANGED : %s Calling wifi_setSSID to "
                        "change SSID name on interface: %d SSID: %s \n",__FUNCTION__,wlan_index,ssid));
        t2_event_d("WIFI_INFO_XHCofigchanged", 1);
#if (!defined(_XB6_PRODUCT_REQ_) || defined (_XB7_PRODUCT_REQ_))
        retval = wifi_setSSIDName(wlan_index, ssid);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to apply SSID name for wlan %d\n",__FUNCTION__, wlan_index));
            if (execRetVal) {
                strncpy(execRetVal->ErrorMsg,"Ssid name apply failed",sizeof(execRetVal->ErrorMsg)-1);
            }
            return retval;
        }
#endif
        retval = wifi_pushSSID(wlan_index, ssid);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to push SSID name for wlan %d\n",__FUNCTION__, wlan_index));
            if (execRetVal) {
                strncpy(execRetVal->ErrorMsg,"Ssid name apply failed",sizeof(execRetVal->ErrorMsg)-1);
            } 
            return retval;
        }
        strncpy(cur_conf_ssid->ssid_name, ssid, COSA_DML_WIFI_MAX_SSID_NAME_LEN);
        cur_conf_ssid->ssid_changed = true;
        CcspTraceInfo(("%s: SSID name change applied for wlan %d\n",__FUNCTION__, wlan_index));

#if defined(ENABLE_FEATURE_MESHWIFI)
        CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of SSID change\n",__FUNCTION__));
        v_secure_system("/usr/bin/sysevent set wifi_SSIDName \"RDK|%d|%s\"",wlan_index, ssid);
#endif
        if (wlan_index == 0) {
            SSID1_UPDATED = TRUE;
        } else if (wlan_index == 1) {
            SSID2_UPDATED = TRUE;
        }
    } else if (bForceDisableFlag == TRUE) {
        CcspWifiTrace(("RDK_LOG_WARN, WIFI_ATTEMPT_TO_CHANGE_CONFIG_WHEN_FORCE_DISABLED \n"));
    }

    if ((cur_conf_ssid->enable != enable) && (!bForceDisableFlag)) {
        CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : %s Calling wifi_setEnable" 
                        " to enable/disable SSID on interface:  %d enable: %d\n",
                         __FUNCTION__, wlan_index, enable));
        retval = wifi_setSSIDEnable(wlan_index, enable);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to set AP Enable for wlan %d\n",__FUNCTION__, wlan_index));
            if (execRetVal) {
                strncpy(execRetVal->ErrorMsg,"Ssid enable failed",sizeof(execRetVal->ErrorMsg)-1);
            }
            return retval;
        }
        if (wlan_index == 3) {
            char passph[128]={0};
            wifi_getApSecurityKeyPassphrase(2, passph);
            wifi_setApSecurityKeyPassphrase(3, passph);
            wifi_getApSecurityPreSharedKey(2, passph);
            wifi_setApSecurityPreSharedKey(3, passph);
            g_newXH5Gpass=TRUE;
        }
        CcspWifiTrace(("RDK_LOG_WARN,WIFI %s wifi_setApEnable success  index %d , %d",
                     __FUNCTION__,wlan_index, enable));
#if (!defined(_XB6_PRODUCT_REQ_) || defined (_XB7_PRODUCT_REQ_))
        COSA_DML_WIFI_SECURITY auth_mode;
        if ((wlan_index % 2) == 0) {
            webconf_auth_mode_to_int(curr_config->security_2g.mode_enabled, &auth_mode);
        } else {
            webconf_auth_mode_to_int(curr_config->security_5g.mode_enabled, &auth_mode);
        }
        if (enable) {
            BOOL enable_wps = FALSE;
#ifdef CISCO_XB3_PLATFORM_CHANGES
            BOOL wps_cfg = 0;
            retval = wifi_getApWpsEnable(wlan_index, &wps_cfg);
            if (retval != RETURN_OK) {
                CcspTraceError(("%s: Failed to get Ap Wps Enable\n", __FUNCTION__));
                return retval;
            }
            enable_wps = (wps_cfg == 0) ? FALSE : TRUE;
#else
            retval = wifi_getApWpsEnable(wlan_index, &enable_wps);
            if (retval != RETURN_OK) {
                CcspTraceError(("%s: Failed to get Ap Wps Enable\n", __FUNCTION__));
                return retval;
            }
#endif            
            BOOL up;
            char status[64]={0};
            if (wifi_getSSIDStatus(wlan_index, status) != RETURN_OK) {
                CcspTraceError(("%s: Failed to get SSID Status\n", __FUNCTION__));
                return RETURN_ERR;
            }
            up = (strcmp(status,"Enabled")==0);
            CcspTraceInfo(("SSID status is %s\n",status));
            if (up == FALSE) {
#if defined(_INTEL_WAV_)
                retval = wifi_setSSIDEnable(wlan_index, TRUE);
                if (retval != RETURN_OK) {
                    CcspTraceError(("%s: Failed to enable AP Interface for wlan %d\n",
                                __FUNCTION__, wlan_index));
                    return retval;
                }
                CcspTraceInfo(("AP Enabled Successfully %d\n\n",wlan_index));
#else
                uint8_t radio_index = (wlan_index % 2);
                retval = wifi_createAp(wlan_index, radio_index, ssid, (adv_enable == TRUE) ? FALSE : TRUE);
                if (retval != RETURN_OK) {
                    CcspTraceError(("%s: Failed to create AP Interface for wlan %d\n",
                                __FUNCTION__, wlan_index));
                    return retval;
                }
                CcspTraceInfo(("AP Created Successfully %d\n\n",wlan_index));
#endif
                apply_params.hostapd_restart = true;
            }
            if (auth_mode >= COSA_DML_WIFI_SECURITY_WPA_Personal) {
                retval = wifi_removeApSecVaribles(wlan_index);
                if (retval != RETURN_OK) {
                    CcspTraceError(("%s: Failed to remove AP SEC Variable for wlan %d\n",
                                     __FUNCTION__, wlan_index));
                    return retval;
                }
                retval = wifi_createHostApdConfig(wlan_index, enable_wps);
                if (retval != RETURN_OK) {
                    CcspTraceError(("%s: Failed to create hostapd config for wlan %d\n",
                                 __FUNCTION__, wlan_index));
                    return retval;
                }
                apply_params.hostapd_restart = true;
                CcspTraceInfo(("Created hostapd config successfully wlan_index %d\n", wlan_index));
            }
            wifi_setApEnable(wlan_index, true);
#ifdef CISCO_XB3_PLATFORM_CHANGES
            wifi_ifConfigUp(wlan_index);
#endif
            PCOSA_DML_WIFI_AP_CFG pStoredApCfg = &sWiFiDmlApStoredCfg[wlan_index].Cfg;
            CosaDmlWiFiApPushCfg(pStoredApCfg);
            CosaDmlWiFiApMfPushCfg(sWiFiDmlApMfCfg[wlan_index], wlan_index);
            CosaDmlWiFiApPushMacFilter(sWiFiDmlApMfQueue[wlan_index], wlan_index);
            wifi_pushBridgeInfo(wlan_index);
        } else {
#ifdef CISCO_XB3_PLATFORM_CHANGES
            wifi_ifConfigDown(wlan_index);
#endif
        
            if (auth_mode >= COSA_DML_WIFI_SECURITY_WPA_Personal) {
                retval = wifi_removeApSecVaribles(wlan_index);
                if (retval != RETURN_OK) {
                    CcspTraceError(("%s: Failed to remove AP SEC Variable for wlan %d\n",
                                     __FUNCTION__, wlan_index));
                    return retval;
                }
                apply_params.hostapd_restart = true; 
            }
        }
#endif /* _XB6_PRODUCT_REQ_ */
        cur_conf_ssid->enable = enable;
        cur_conf_ssid->ssid_changed = true;
        CcspTraceInfo(("%s: SSID Enable change applied for wlan %d\n",__FUNCTION__, wlan_index));
    } else if (bForceDisableFlag == TRUE) {
        CcspWifiTrace(("RDK_LOG_WARN, WIFI_ATTEMPT_TO_CHANGE_CONFIG_WHEN_FORCE_DISABLED \n"));
    }
    
    if (cur_conf_ssid->ssid_advertisement_enabled != adv_enable) {
        retval = wifi_setApSsidAdvertisementEnable(wlan_index, adv_enable);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to set SSID Advertisement Status for wlan %d\n",
                                                             __FUNCTION__, wlan_index));
            if (execRetVal) {
                strncpy(execRetVal->ErrorMsg,"SSID Advertisement Status apply failed",
                        sizeof(execRetVal->ErrorMsg)-1);
            }
            return retval;
        }
#if (!defined(_XB6_PRODUCT_REQ_) || defined (_XB7_PRODUCT_REQ_))
        retval = wifi_pushSsidAdvertisementEnable(wlan_index, adv_enable);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to push SSID Advertisement Status for wlan %d\n",
                                                             __FUNCTION__, wlan_index));
            return retval;
        }
        if (!wlan_index) {
            curr_config->security_2g.sec_changed = true;
        } else {
            curr_config->security_5g.sec_changed = true;
        }
#endif

#if defined(ENABLE_FEATURE_MESHWIFI)
        CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of SSID Advertise changes\n",__FUNCTION__));
        v_secure_system("/usr/bin/sysevent set wifi_SSIDAdvertisementEnable \"RDK|%d|%s\"", 
                        wlan_index, adv_enable?"true":"false");
#endif
        apply_params.hostapd_restart = true;
        cur_conf_ssid->ssid_changed = true;
        cur_conf_ssid->ssid_advertisement_enabled = adv_enable;
        CcspTraceInfo(("%s: Advertisement change applied for wlan index: %d\n", 
                                                    __FUNCTION__, wlan_index));
    }

    return RETURN_OK;
}

/** Applies Wifi Security Parameters
 *
 *  @param wlan_index   AP Index
 *
 *  returns 0 on success, error otherwise
 */
int webconf_apply_wifi_security_params(webconf_wifi_t *pssid_entry, uint8_t wlan_index,
                                       pErr execRetVal)
{
    int retval = RETURN_ERR;
    char securityType[32] = {0};
    char authMode[32] = {0};
    char method[32] = {0};
    char *mode = NULL, *encryption = NULL, *passphrase = NULL;
    webconf_security_t *wlan_security = NULL, *cur_sec_cfg = NULL;
    BOOLEAN bForceDisableFlag = FALSE;

    COSA_DML_WIFI_SECURITY sec_mode = COSA_DML_WIFI_SECURITY_None;

    if ((wlan_index % 2) == 0) {
        wlan_security = &pssid_entry->security_2g;
        cur_sec_cfg = &curr_config->security_2g; 
    } else {
        wlan_security = &pssid_entry->security_5g;
        cur_sec_cfg = &curr_config->security_5g;
    }
    passphrase = wlan_security->passphrase;
    mode = wlan_security->mode_enabled;
    encryption = wlan_security->encryption_method;

    if(ANSC_STATUS_FAILURE == CosaDmlWiFiGetCurrForceDisableWiFiRadio(&bForceDisableFlag))
    {
        CcspTraceError(("%s Failed to fetch ForceDisableWiFiRadio flag!!!\n",__FUNCTION__));
    }

    /* Copy hal specific strings for respective Authentication Mode */
    if (strcmp(mode, "None") == 0 ) {
        sec_mode = COSA_DML_WIFI_SECURITY_None;
        strcpy(securityType,"None");
        strcpy(authMode,"None");
    }
#ifndef _XB6_PRODUCT_REQ_
    else if (strcmp(mode, "WEP-64") == 0) {
        sec_mode = COSA_DML_WIFI_SECURITY_WEP_64; 
    } else if (strcmp(mode, "WEP-128") == 0) {
        sec_mode = COSA_DML_WIFI_SECURITY_WEP_128;
    } 
    else if (strcmp(mode, "WPA-Personal") == 0) {
        strcpy(securityType,"WPA");
        strcpy(authMode,"PSKAuthentication");
        sec_mode = COSA_DML_WIFI_SECURITY_WPA_Personal;
    } else if (strcmp(mode, "WPA2-Personal") == 0) {
        sec_mode = COSA_DML_WIFI_SECURITY_WPA2_Personal;
        strcpy(securityType,"11i");
        strcpy(authMode,"PSKAuthentication");
    } else if (strcmp(mode, "WPA-Enterprise") == 0) {
        sec_mode = COSA_DML_WIFI_SECURITY_WPA_Enterprise;
    } else if (strcmp(mode, "WPA-WPA2-Personal") == 0) {
        strcpy(securityType,"WPAand11i");
        strcpy(authMode,"PSKAuthentication");
        sec_mode = COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal;
    }
#else
    else if ((strcmp(mode, "WPA2-Personal") == 0)) {
        sec_mode = COSA_DML_WIFI_SECURITY_WPA2_Personal;
        strcpy(securityType,"11i");
        strcpy(authMode,"SharedAuthentication");
    }
#endif
    else if (strcmp(mode, "WPA2-Enterprise") == 0) {
        sec_mode = COSA_DML_WIFI_SECURITY_WPA2_Enterprise;
        strcpy(securityType,"11i");
        strcpy(authMode,"EAPAuthentication");
    } else if (strcmp(mode, "WPA-WPA2-Enterprise") == 0) {
        strcpy(securityType,"WPAand11i");
        strcpy(authMode,"EAPAuthentication");
        sec_mode = COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise;
    }


    if ((strcmp(encryption, "TKIP") == 0)) {
        strcpy(method,"TKIPEncryption");
    } else if ((strcmp(encryption, "AES") == 0)) {
        strcpy(method,"AESEncryption");
    } 
#ifndef _XB6_PRODUCT_REQ_
    else if ((strcmp(encryption, "AES+TKIP") == 0)) {
        strcpy(method,"TKIPandAESEncryption");
    }
#endif

    /* Apply Security Values to hal */
    if (((wlan_index == 0) || (wlan_index == 1)) &&
        (sec_mode == COSA_DML_WIFI_SECURITY_None)) {
        retval = wifi_setApWpsEnable(wlan_index, FALSE);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to set AP Wps Status\n", __FUNCTION__));
            if (execRetVal) {
                strncpy(execRetVal->ErrorMsg,"Failed to set Wps Status",sizeof(execRetVal->ErrorMsg)-1);
            }
            return retval;
        }
    }

    if (strcmp(cur_sec_cfg->mode_enabled, mode) != 0) {
        retval = wifi_setApBeaconType(wlan_index, securityType);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to set AP Beacon type\n", __FUNCTION__));
            if (execRetVal) {
                strncpy(execRetVal->ErrorMsg,"Failed to set Ap Beacon type",sizeof(execRetVal->ErrorMsg)-1);
            }
            return retval;
        }
        CcspWifiTrace(("RDK_LOG_WARN,%s calling setBasicAuthenticationMode ssid : %d authmode : %s \n",
                       __FUNCTION__,wlan_index, mode));
        retval = wifi_setApBasicAuthenticationMode(wlan_index, authMode);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to set AP Authentication Mode\n", __FUNCTION__));
            if (execRetVal) {
                strncpy(execRetVal->ErrorMsg,"Failed to set Ap Auth Mode",sizeof(execRetVal->ErrorMsg)-1);
            } 
            return retval;
        }

        strncpy(cur_sec_cfg->mode_enabled, mode, sizeof(cur_sec_cfg->mode_enabled)-1);
        cur_sec_cfg->sec_changed = true;
        apply_params.hostapd_restart = true;
        CcspWifiEventTrace(("RDK_LOG_NOTICE, Wifi security mode %s is Enabled", mode));
        CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : Wifi security mode %s is Enabled\n",mode));
        CcspTraceInfo(("%s: Security Mode Change Applied for wlan index %d\n", __FUNCTION__,wlan_index));
    }

    if (strcmp(cur_sec_cfg->passphrase, passphrase) != 0 && (!bForceDisableFlag)) {
        CcspTraceInfo(("KeyPassphrase changed for index = %d\n",wlan_index));
        retval = wifi_setApSecurityKeyPassphrase(wlan_index, passphrase);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to set AP Security Passphrase\n", __FUNCTION__));
            if (execRetVal) {
                strncpy(execRetVal->ErrorMsg,"Failed to set Passphrase",sizeof(execRetVal->ErrorMsg)-1);
            }
            return retval;
        }
	wifi_setApSecurityPreSharedKey(wlan_index, passphrase);
        strncpy(cur_sec_cfg->passphrase, passphrase, sizeof(cur_sec_cfg->passphrase)-1);
        apply_params.hostapd_restart = true;
        cur_sec_cfg->sec_changed = true;
        if (wlan_index == 0) {
            PASSPHRASE1_UPDATED = TRUE;
        } else if (wlan_index == 1) {
            PASSPHRASE2_UPDATED = TRUE;
        }
        CcspWifiEventTrace(("RDK_LOG_NOTICE, KeyPassphrase changed \n "));
        CcspWifiTrace(("RDK_LOG_WARN, KeyPassphrase changed \n "));
        CcspWifiTrace(("RDK_LOG_WARN, RDKB_WIFI_CONFIG_CHANGED : %s KeyPassphrase changed for index = %d\n",
                        __FUNCTION__, wlan_index));
        CcspTraceInfo(("%s: Passpharse change applied for wlan index %d\n", __FUNCTION__, wlan_index));
    } else if (bForceDisableFlag == TRUE) {
        CcspWifiTrace(("RDK_LOG_WARN, WIFI_ATTEMPT_TO_CHANGE_CONFIG_WHEN_FORCE_DISABLED \n"));
    }

    if ((strcmp(cur_sec_cfg->encryption_method, encryption) != 0) &&
        (sec_mode >= COSA_DML_WIFI_SECURITY_WPA_Personal) &&
        (sec_mode <= COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise)) {
        CcspWifiTrace(("RDK_LOG_WARN, RDKB_WIFI_CONFIG_CHANGED :%s Encryption method changed , "
                       "calling setWpaEncryptionMode Index : %d mode : %s \n",
                       __FUNCTION__,wlan_index, encryption)); 
        retval = wifi_setApWpaEncryptionMode(wlan_index, method);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to set WPA Encryption Mode\n", __FUNCTION__));
            if (execRetVal) {
                strncpy(execRetVal->ErrorMsg,"Failed to set Encryption Mode",
                        sizeof(execRetVal->ErrorMsg)-1);
            }
            return retval;
        }
        strncpy(cur_sec_cfg->encryption_method, encryption, sizeof(cur_sec_cfg->encryption_method)-1);
        cur_sec_cfg->sec_changed = true;
        apply_params.hostapd_restart = true;
        CcspTraceInfo(("%s: Encryption mode change applied for wlan index %d\n", 
                        __FUNCTION__, wlan_index));
    }

    if (cur_sec_cfg->sec_changed) {
        CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of Security changes\n",__FUNCTION__));
        v_secure_system("/usr/bin/sysevent set wifi_ApSecurity \"RDK|%d|%s|%s|%s\"",wlan_index, passphrase, authMode, method);
    }
 
#if (!defined(_XB6_PRODUCT_REQ_) || defined (_XB7_PRODUCT_REQ_))
    BOOL up;

    if ((wlan_index % 2) == 0) {
        up = pssid_entry->ssid_2g.enable;
    } else {
        up = pssid_entry->ssid_5g.enable;
    }
    if ((cur_sec_cfg->sec_changed) && (up == TRUE)) {
        BOOL enable_wps = FALSE;
#ifdef CISCO_XB3_PLATFORM_CHANGES
        BOOL wps_cfg = 0;
        retval = wifi_getApWpsEnable(wlan_index, &wps_cfg);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to get Ap Wps Enable\n", __FUNCTION__));
            return retval;
        }
        enable_wps = (wps_cfg == 0) ? FALSE : TRUE;
#else
        retval = wifi_getApWpsEnable(wlan_index, &enable_wps);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to get Ap Wps Enable\n", __FUNCTION__));
            return retval;
        }
#endif        
        retval = wifi_removeApSecVaribles(wlan_index);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to remove Ap Sec variables\n", __FUNCTION__));
            return retval;
        }

#if !defined(_XB7_PRODUCT_REQ_) && !defined(_XB8_PRODUCT_REQ_) && !defined(_CBR2_PRODUCT_REQ)
        retval = wifi_disableApEncryption(wlan_index);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to disable AP Encryption\n",__FUNCTION__));
            return retval;
        }
#endif

        if (sec_mode == COSA_DML_WIFI_SECURITY_None) {
            retval = wifi_createHostApdConfig(wlan_index, TRUE);
        }
#if (!defined(DUAL_CORE_XB3) || defined(CISCO_XB3_PLATFORM_CHANGES))
        else {
            retval = wifi_createHostApdConfig(wlan_index, enable_wps);
            
        }
#endif
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to create Host Apd Config\n",__FUNCTION__));
            return retval;
        }
        if (wifi_setApEnable(wlan_index, TRUE) != RETURN_OK) {
            CcspTraceError(("%s: wifi_setApEnable failed  index %d\n",__FUNCTION__,wlan_index));
        }
        CcspTraceInfo(("%s: Security changes applied for wlan index %d\n",
                       __FUNCTION__, wlan_index));
    }
#endif /* #if (!defined(_XB6_PRODUCT_REQ_) || defined (_XB7_PRODUCT_REQ_)) */
    return RETURN_OK;
}

int wifi_reset_radio()
{
    int retval = RETURN_ERR;
    PCOSA_DATAMODEL_WIFI pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    uint8_t i;

#ifdef DUAL_CORE_XB3
    wifi_initRadio(0);
#endif

    for (i = 0;i < 2;i++) { 
        if (gradio_restart[i]) {
#if defined(_INTEL_WAV_)
            wifi_applyRadioSettings(i);
#elif !defined(DUAL_CORE_XB3)
            wifi_initRadio(i);
#endif
            retval = CosaWifiReInitialize((ANSC_HANDLE)pMyObject, i);
            if (retval != RETURN_OK) { 
                CcspTraceError(("%s: CosaWifiReInitialize Failed for Radio %d\n",__FUNCTION__,i, retval));
            }
#if defined(ENABLE_FEATURE_MESHWIFI)
            ULONG channel = 0;
            PCOSA_DML_WIFI_RADIO pWifiRadio    = NULL;
            pWifiRadio = pMyObject->pRadio+i;
            channel = pWifiRadio->Radio.Cfg.Channel;
     // notify mesh components that wifi radio settings changed
            CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of Radio Config changes channel %lu\n",__FUNCTION__,channel));
            v_secure_system("/usr/bin/sysevent set wifi_RadioChannel 'RDK|%lu|%lu'", (ULONG)i, channel);
#endif
        }
    }

    Load_Hotspot_APIsolation_Settings();

    Update_Hotspot_MacFilt_Entries(true);

    return RETURN_OK;
}
    
/**
 *   Applies Radio settings
 *
 *   return 0 on success, error otherwise
 */
char *wifi_apply_radio_settings()
{
    int retval = RETURN_ERR;

    if (gradio_restart[0] || gradio_restart[1]) {
        retval = wifi_reset_radio();
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: wifi_reset_radio failed %d\n",__FUNCTION__,retval));
            return "wifi_reset_radio failed";
        }
        gradio_restart[0] = FALSE;
        gradio_restart[1] = FALSE;
    }
    if (gHostapd_restart_reqd || apply_params.hostapd_restart) {
#if (defined(_COSA_INTEL_USG_ATOM_) && !defined(_INTEL_WAV_) ) || ( (defined(_COSA_BCM_ARM_) || defined(_PLATFORM_TURRIS_)) && !defined(_CBR_PRODUCT_REQ_) && !defined(_XB7_PRODUCT_REQ_) )
        wifi_restartHostApd();
#else
        if (wifi_stopHostApd() != RETURN_OK) {
            CcspTraceError(("%s: Failed restart hostapd\n",__FUNCTION__));
            return "wifi_stopHostApd failed";
        }
        if (wifi_startHostApd() != RETURN_OK) {
            CcspTraceError(("%s: Failed restart hostapd\n",__FUNCTION__));
            return "wifi_startHostApd failed";
        }
#endif
        CcspTraceInfo(("%s: Restarted Hostapd successfully\n", __FUNCTION__));

#if (defined(_COSA_BCM_ARM_) && defined(_XB7_PRODUCT_REQ_)) || defined(_XB8_PRODUCT_REQ_) || defined(_CBR2_PRODUCT_REQ)
    /* Converting brcm patch to code and this code will be removed as part of Hal Version 3 changes */
    fprintf(stderr, "%s: calling wifi_apply()...\n", __func__);
    wifi_apply();
#endif

#if (defined(_XB6_PRODUCT_REQ_) && !defined(_XB7_PRODUCT_REQ_))
        /* XB6 some cases needs AP Interface UP/Down for hostapd pick up security changes */
        char status[8] = {0};
        bool enable = false;
        uint8_t wlan_index = 0;
        int retval = RETURN_ERR;
        retval = wifi_getSSIDStatus(wlan_index, status);
        if (retval == RETURN_OK) {
            enable = (strcmp(status, "Enabled") == 0) ? true : false;
            if (wifi_setApEnable(wlan_index, enable) != RETURN_OK) {
                CcspTraceError(("%s: Error enabling AP Interface",__FUNCTION__));
                return "wifi_setApEnable failed";
            }
        }
        CcspTraceInfo(("%s:AP Interface UP Successful\n", __FUNCTION__));
#endif
    gHostapd_restart_reqd = false;
    apply_params.hostapd_restart = false;
    }
    return NULL;
}

/**
 *  Validation of WiFi SSID Parameters
 *
 *  @param  wlan_index  AP Index
 *
 *  returns 0 on success, error otherwise
 */
int webconf_validate_wifi_ssid_params (webconf_wifi_t *pssid_entry, uint8_t wlan_index,
                                       pErr execRetVal)
{
    char *ssid_name = NULL;
    int ssid_len = 0;
    int i = 0, j = 0;
    char ssid_char[COSA_DML_WIFI_MAX_SSID_NAME_LEN] = {0};
    char ssid_lower[COSA_DML_WIFI_MAX_SSID_NAME_LEN] = {0};


    if ((wlan_index % 2) == 0) {
        ssid_name = pssid_entry->ssid_2g.ssid_name;
    } else {
        ssid_name = pssid_entry->ssid_5g.ssid_name;
    }

    ssid_len = strlen(ssid_name);
    if ((ssid_len == 0) || (ssid_len > COSA_DML_WIFI_MAX_SSID_NAME_LEN)) {
        if (execRetVal) {
            strncpy(execRetVal->ErrorMsg,"Invalid SSID string size",sizeof(execRetVal->ErrorMsg)-1);
        }
        CcspTraceError(("%s: Invalid SSID size for wlan index %d \n",__FUNCTION__, wlan_index));
        return RETURN_ERR;
    }

 
    while (i < ssid_len) {
        ssid_lower[i] = tolower(ssid_name[i]);
        if (isalnum(ssid_name[i]) != 0) {
            ssid_char[j++] = ssid_lower[i];
        }
        i++;
    }
    ssid_lower[i] = '\0';
    ssid_char[j] = '\0';

    for (i = 0; i < ssid_len; i++) {
        if (!((ssid_name[i] >= ' ') && (ssid_name[i] <= '~'))) {
            CcspTraceError(("%s: Invalid character present in SSID %d\n",__FUNCTION__, wlan_index));
            if (execRetVal) {
                strncpy(execRetVal->ErrorMsg,"Invalid character in SSID",sizeof(execRetVal->ErrorMsg)-1);
            }
            return RETURN_ERR;
        }
    }
 

    /* SSID containing "optimumwifi", "TWCWiFi", "cablewifi" and "xfinitywifi" are reserved */
    if ((strstr(ssid_char, "cablewifi") != NULL) || (strstr(ssid_char, "twcwifi") != NULL) || (strstr(ssid_char, "optimumwifi") != NULL) ||
        (strstr(ssid_char, "xfinitywifi") != NULL) || (strstr(ssid_char, "xfinity") != NULL) || (strstr(ssid_char, "coxwifi") != NULL) ||
        (strstr(ssid_char, "spectrumwifi") != NULL) || (strstr(ssid_char, "shawopen") != NULL) || (strstr(ssid_char, "shawpasspoint") != NULL) ||
        (strstr(ssid_char, "shawguest") != NULL) || (strstr(ssid_char, "shawmobilehotspot") != NULL)) {

        CcspTraceError(("%s: Reserved SSID format used for ssid %d\n",__FUNCTION__, wlan_index));
        if (execRetVal) {
            strncpy(execRetVal->ErrorMsg,"Reserved SSID format used",sizeof(execRetVal->ErrorMsg)-1);
        }
        return RETURN_ERR;
    }
 
    return RETURN_OK;
}

/**
 *   Validates Wifi Security Paramters
 *
 *   @param wlan_index  AP Index
 *
 *   returns 0 on success, error otherwise
 */
int webconf_validate_wifi_security_params (webconf_wifi_t *pssid_entry, uint8_t wlan_index,
                                           pErr execRetVal)
{
    char *passphrase = NULL;
    char *mode_enabled = NULL;
    char *encryption_method = NULL;
    int pass_len = 0;

    if ((wlan_index % 2) == 0) {
        passphrase = pssid_entry->security_2g.passphrase;
        mode_enabled = pssid_entry->security_2g.mode_enabled;
        encryption_method = pssid_entry->security_2g.encryption_method;
    } else {
        passphrase = pssid_entry->security_5g.passphrase;
        mode_enabled = pssid_entry->security_5g.mode_enabled;
        encryption_method = pssid_entry->security_5g.encryption_method;
    }

    /* Sanity Checks */
    if ((strcmp(mode_enabled, "None") != 0) && (strcmp(mode_enabled, "WEP-64") != 0) && (strcmp(mode_enabled, "WEP-128") !=0) &&
        (strcmp(mode_enabled, "WPA-Personal") != 0) && (strcmp(mode_enabled, "WPA2-Personal") != 0) &&
        (strcmp(mode_enabled, "WPA-WPA2-Personal") != 0) && (strcmp(mode_enabled, "WPA2-Enterprise") != 0) &&
        (strcmp(mode_enabled, "WPA-WPA2-Enterprise") != 0) && (strcmp(mode_enabled, "WPA-Enterprise") !=0)) {
 
        CcspTraceError(("%s: Invalid Security Mode for wlan index %d\n",__FUNCTION__, wlan_index));
        if (execRetVal) {
            strncpy(execRetVal->ErrorMsg,"Invalid Security Mode",sizeof(execRetVal->ErrorMsg)-1);
        }
        return RETURN_ERR;
    }

    if ((strcmp(mode_enabled, "None") != 0) &&
        ((strcmp(encryption_method, "TKIP") != 0) && (strcmp(encryption_method, "AES") != 0) &&
        (strcmp(encryption_method, "AES+TKIP") != 0))) {
        CcspTraceError(("%s: Invalid Encryption Method for wlan index %d\n",__FUNCTION__, wlan_index));
        if (execRetVal) {
            strncpy(execRetVal->ErrorMsg,"Invalid Encryption Method",sizeof(execRetVal->ErrorMsg)-1);
        }
        return RETURN_ERR;
    }

    if (((strcmp(mode_enabled, "WPA-WPA2-Enterprise") == 0) || (strcmp(mode_enabled, "WPA-WPA2-Personal") == 0)) &&
        ((strcmp(encryption_method, "AES+TKIP") != 0) && (strcmp(encryption_method, "AES") != 0))) {
        CcspTraceError(("%s: Invalid Encryption Security Combination for wlan index %d\n",__FUNCTION__, wlan_index));
        if (execRetVal) {
            strncpy(execRetVal->ErrorMsg,"Invalid Encryption Security Combination",sizeof(execRetVal->ErrorMsg)-1);
        }
        return RETURN_ERR;
    }

    pass_len = strlen(passphrase);

    if ((pass_len < MIN_PWD_LEN) || (pass_len > MAX_PWD_LEN)) {
        CcspTraceInfo(("%s: Invalid Key passphrase length index %d\n",__FUNCTION__, wlan_index));
        if (execRetVal) {
            strncpy(execRetVal->ErrorMsg,"Invalid Passphrase length",sizeof(execRetVal->ErrorMsg)-1);
        }
        return RETURN_ERR;
    }


    CcspTraceInfo(("%s: Security Params validated Successfully for wlan index %d\n",__FUNCTION__, wlan_index));
    return RETURN_OK;
}

/**
 *   Function to call WiFi Apply Handlers (SSID, Security)
 *
 *   returns 0 on success, error otherwise
 */
int webconf_apply_wifi_param_handler (webconf_wifi_t *pssid_entry, pErr execRetVal,uint8_t ssid)
{
    uint8_t i = 0, wlan_index = 0;
    int retval = RETURN_ERR;
    char *err = NULL;

    if (ssid == WIFI_WEBCONFIG_PRIVATESSID) {
        wlan_index = 0;
    } else if (ssid == WIFI_WEBCONFIG_HOMESSID) {
        wlan_index = 2;
    }

    for (i = wlan_index;i < (wlan_index+2); i++) {
        retval  = webconf_apply_wifi_ssid_params(pssid_entry, i, execRetVal);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to apply ssid params for ap index %d\n",
                            __FUNCTION__, wlan_index));
            return retval;
        }

        retval = webconf_apply_wifi_security_params(pssid_entry, i, execRetVal);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to apply security params for ap index %d\n",
                             __FUNCTION__, wlan_index));
            return retval;
        }
    } 

    err = wifi_apply_radio_settings();
    if (err != NULL) {
        CcspTraceError(("%s: Failed to Apply Radio settings\n", __FUNCTION__));
        if (execRetVal) {
            strncpy(execRetVal->ErrorMsg, err,sizeof(execRetVal->ErrorMsg)-1);
        }
        return RETURN_ERR;
    }
    return RETURN_OK;
}

/**
 *  Function to call WiFi Validation handlers (SSID, Seurity)
 *
 *  returns 0 on success, error otherwise
 */
int webconf_validate_wifi_param_handler (webconf_wifi_t *pssid_entry, pErr execRetVal,uint8_t ssid)
{
    uint8_t i = 0, wlan_index = 0;
    int retval = RETURN_ERR;

    if (ssid == WIFI_WEBCONFIG_PRIVATESSID) {
        wlan_index = 0;
    } else if (ssid == WIFI_WEBCONFIG_HOMESSID) {
        wlan_index = 2;
    }

    for (i = wlan_index;i < (wlan_index+2); i++) {
        retval = webconf_validate_wifi_ssid_params(pssid_entry, i, execRetVal);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to validate ssid params for ap index %d\n",
                             __FUNCTION__,wlan_index));
            return retval;
        }

        retval = webconf_validate_wifi_security_params(pssid_entry, i, execRetVal);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to validate security params for ap index %d\n",
                             __FUNCTION__, wlan_index));
            return retval;
        }
    }
    return RETURN_OK;
}

/*
 * Function to rollback to previous configs if apply failed
 *
 * @returns 0 on success, failure otherwise
 */
int webconf_ssid_rollback_handler()
{

    webconf_wifi_t *prev_config = NULL;
    uint8_t ssid_type = 0;

    prev_config = (webconf_wifi_t *) malloc(sizeof(webconf_wifi_t));
    if (!prev_config) {
        CcspTraceError(("%s: Memory allocation error\n", __FUNCTION__));
        return RETURN_ERR;
    }
    memset(prev_config, 0, sizeof(webconf_wifi_t));

    if (strncmp(curr_config->subdoc_name, "privatessid",strlen("privatessid")) == 0) {
        ssid_type = WIFI_WEBCONFIG_PRIVATESSID;
    } else if (strncmp(curr_config->subdoc_name,"homessid",strlen("homessid")) == 0) {
        ssid_type = WIFI_WEBCONFIG_HOMESSID;
    }

    if (webconf_populate_initial_dml_config(prev_config, ssid_type) != RETURN_OK) {
        CcspTraceError(("%s: Failed to copy initial configs\n", __FUNCTION__));
        free(prev_config);
        return RETURN_ERR;
    }

    if (webconf_apply_wifi_param_handler(prev_config, NULL, ssid_type) != RETURN_OK) {
        CcspTraceError(("%s: Rollback of webconfig params failed!!\n",__FUNCTION__));
        free(prev_config);
        return RETURN_ERR;
    }
    CcspTraceInfo(("%s: Rollback of webconfig params applied successfully\n",__FUNCTION__));
    free(prev_config);
    return RETURN_OK;
}

/* API to free the resources after blob apply*/
void webconf_ssid_free_resources(void *arg)
{
    CcspTraceInfo(("Entering: %s\n",__FUNCTION__));
    if (arg == NULL) {
        CcspTraceError(("%s: Input Data is NULL\n",__FUNCTION__));
        return;
    }
    execData *blob_exec_data  = (execData*) arg;

    webconf_wifi_t *ps_data   = (webconf_wifi_t *) blob_exec_data->user_data;
    free(blob_exec_data);

    if (ps_data != NULL) {
        free(ps_data);
        ps_data = NULL;
        CcspTraceInfo(("%s:Success in Clearing wifi webconfig resources\n",__FUNCTION__));
    }
}

/*
 * Function to deserialize ssid params from msgpack object
 *
 * @param obj  Pointer to msgpack object
 * @param ssid Pointer to wifi ssid structure
 *
 * returns 0 on success, error otherwise
 */
int webconf_copy_wifi_ssid_params(msgpack_object obj, webconf_ssid_t *ssid) {
    unsigned int i;
    msgpack_object_kv* p = obj.via.map.ptr;

    for(i = 0;i < obj.via.map.size;i++) {
        if (strncmp(p->key.via.str.ptr, "SSID",p->key.via.str.size) == 0) {
            if (p->val.type == MSGPACK_OBJECT_STR) { 
                strncpy(ssid->ssid_name,p->val.via.str.ptr, p->val.via.str.size);
                ssid->ssid_name[p->val.via.str.size] = '\0';
            } else {
                CcspTraceError(("%s:Invalid value type for SSID",__FUNCTION__));
                return RETURN_ERR;
            }
        }
        else if (strncmp(p->key.via.str.ptr, "Enable",p->key.via.str.size) == 0) {
            if (p->val.type == MSGPACK_OBJECT_BOOLEAN) {
                if (p->val.via.boolean) {
                    ssid->enable = true;
                } else {
                    ssid->enable = false;
                }
            } else {
                CcspTraceError(("%s:Invalid value type for SSID Enable",__FUNCTION__));
                return RETURN_ERR;
            } 
        }
        else if (strncmp(p->key.via.str.ptr, "SSIDAdvertisementEnabled",p->key.via.str.size) == 0) {
            if (p->val.type == MSGPACK_OBJECT_BOOLEAN) {
                if (p->val.via.boolean) {
                    ssid->ssid_advertisement_enabled = true;
                } else {
                    ssid->ssid_advertisement_enabled = false;
                }
            } else {
                CcspTraceError(("%s:Invalid value type for SSID Advtertisement",__FUNCTION__));
                return RETURN_ERR;
            }
        }
        ++p;
    }
    return RETURN_OK;
}

/*
 * Function to deserialize security params from msgpack object
 *
 * @param obj  Pointer to msgpack object
 * @param ssid Pointer to wifi security structure
 *
 * returns 0 on success, error otherwise
 */
int webconf_copy_wifi_security_params(msgpack_object obj, webconf_security_t *security) {
    unsigned int i;
    msgpack_object_kv* p = obj.via.map.ptr;

    for(i = 0;i < obj.via.map.size;i++) {
        if (strncmp(p->key.via.str.ptr, "Passphrase",p->key.via.str.size) == 0) {
            if (p->val.type == MSGPACK_OBJECT_STR) {
                strncpy(security->passphrase,p->val.via.str.ptr, p->val.via.str.size);
                security->passphrase[p->val.via.str.size] = '\0';
            } else {
                CcspTraceError(("%s:Invalid value type for Security passphrase",__FUNCTION__));
                return RETURN_ERR;
            }
        }
        else if (strncmp(p->key.via.str.ptr, "EncryptionMethod",p->key.via.str.size) == 0) {
            if (p->val.type == MSGPACK_OBJECT_STR) {
                strncpy(security->encryption_method,p->val.via.str.ptr, p->val.via.str.size);
                security->encryption_method[p->val.via.str.size] = '\0';
            } else {
                CcspTraceError(("%s:Invalid value type for Encryption method",__FUNCTION__));
                return RETURN_ERR;
            }
        }
        else if (strncmp(p->key.via.str.ptr, "ModeEnabled",p->key.via.str.size) == 0) {
            if (p->val.type == MSGPACK_OBJECT_STR) {
                strncpy(security->mode_enabled,p->val.via.str.ptr, p->val.via.str.size);
                security->mode_enabled[p->val.via.str.size] = '\0';
            } else {
                CcspTraceError(("%s:Invalid value type for Authentication mode",__FUNCTION__));
                return RETURN_ERR;
            }
        }
        ++p;
    }
    return RETURN_OK;
}

/**
 *  Function to calculate timeout value for executing the blob
 *
 *  @param numOfEntries Number of Entries of blob
 *
 * returns timeout value
 */
size_t webconf_ssid_timeout_handler(size_t numOfEntries)
{
#if defined(_XB6_PRODUCT_REQ_) && !defined (_XB7_PRODUCT_REQ_)
    return (numOfEntries * XB6_DEFAULT_TIMEOUT);
#else
    return (numOfEntries * SSID_DEFAULT_TIMEOUT);
#endif
}
    
/**
 * Execute blob request callback handler
 *
 * @param  Data  Pointer to structure holding wifi parameters
 *
 * returns pErr structure populated with return code and error string incase of failure
 *
 */
pErr webconf_wifi_ssid_config_handler(void *Data)
{
    pErr execRetVal = NULL;
    int retval = RETURN_ERR;
    uint8_t ssid_type = 0;

    if (Data == NULL) {
        CcspTraceError(("%s: Input Data is NULL\n",__FUNCTION__));
        return execRetVal;
    }

    webconf_wifi_t *ps = (webconf_wifi_t *) Data;
    if(strncmp(ps->subdoc_name,"privatessid",strlen("privatessid")) == 0) {
        ssid_type = WIFI_WEBCONFIG_PRIVATESSID;
    } else if (strncmp(ps->subdoc_name,"homessid",strlen("homessid")) == 0) {
        ssid_type = WIFI_WEBCONFIG_HOMESSID;
    }
 
    /* Copy the initial configs */
    if (webconf_alloc_current_cfg(ssid_type) != RETURN_OK) {
        CcspTraceError(("%s: Failed to copy the current config\n",__FUNCTION__));
        return execRetVal;
    }
    strncpy(curr_config->subdoc_name, ps->subdoc_name, sizeof(curr_config->subdoc_name)-1);

    execRetVal = (pErr ) malloc (sizeof(Err));
    if (execRetVal == NULL )
    {
        CcspTraceError(("%s : Malloc failed\n",__FUNCTION__));
        return execRetVal;
    }

    memset(execRetVal,0,(sizeof(Err)));

    execRetVal->ErrorCode = BLOB_EXEC_SUCCESS;


    /* Validation of Input parameters */
    retval = webconf_validate_wifi_param_handler(ps, execRetVal, ssid_type);
    if (retval != RETURN_OK) {
        CcspTraceError(("%s: Validation of msg blob failed\n",__FUNCTION__));
        execRetVal->ErrorCode = VALIDATION_FALIED;
        return execRetVal;
    } else {
        /* Apply Paramters to hal and update TR-181 cache */
        retval = webconf_apply_wifi_param_handler(ps, execRetVal, ssid_type);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to Apply WebConfig Params\n",
                             __FUNCTION__));
            execRetVal->ErrorCode = WIFI_HAL_FAILURE;
            return execRetVal;
        }
    }
 
    if (webconf_update_dml_params(ps, ssid_type) != RETURN_OK) {
        CcspTraceError(("%s: Failed to Populate TR-181 Params\n",
                             __FUNCTION__));
        execRetVal->ErrorCode = WIFI_HAL_FAILURE;
        return execRetVal;
    }

    if ( strcmp(notifyWiFiChangesVal,"true") == 0 ) 
    {
        parameterSigStruct_t       val = {0};
        char param_name[64] = {0};
         
        if (SSID1_UPDATED) {
            strncpy(param_name, "Device.WiFi.SSID.1.SSID", sizeof(param_name));
            val.parameterName = param_name;
            val.newValue = ps->ssid_2g.ssid_name;
            WiFiPramValueChangedCB(&val, 0, NULL);
            SSID1_UPDATED = FALSE;
        } 
        if (SSID2_UPDATED) {
            strncpy(param_name, "Device.WiFi.SSID.2.SSID", sizeof(param_name));
            val.parameterName = param_name;
            val.newValue = ps->ssid_5g.ssid_name;
            WiFiPramValueChangedCB(&val, 0, NULL);
            SSID2_UPDATED = FALSE;
        }
        if (PASSPHRASE1_UPDATED) {
            strncpy(param_name, "Device.WiFi.AccessPoint.1.Security.X_COMCAST-COM_KeyPassphrase",sizeof(param_name));
            val.parameterName = param_name;
            val.newValue = ps->security_2g.passphrase;
            WiFiPramValueChangedCB(&val, 0, NULL);
            PASSPHRASE1_UPDATED = FALSE;
        }
        if (PASSPHRASE2_UPDATED) {
            strncpy(param_name, "Device.WiFi.AccessPoint.2.Security.X_COMCAST-COM_KeyPassphrase",sizeof(param_name));
            val.parameterName = param_name;
            val.newValue = ps->security_5g.passphrase;
            WiFiPramValueChangedCB(&val, 0, NULL);
            PASSPHRASE2_UPDATED = FALSE;
        }
    }       


    return execRetVal;
}

/**
 *  Function to apply WiFi Configs from dmcli
 *
 *  @param buf        Pointer to the decodedMsg
 *  @param size       Size of the Decoded Message
 *
 *  returns 0 on success, error otherwise
 */
int wifi_WebConfigSet(const void *buf, size_t len,uint8_t ssid)
{
    size_t offset = 0;
    msgpack_unpacked msg;
    msgpack_unpack_return mp_rv;
    msgpack_object_map *map = NULL;
    msgpack_object_kv* map_ptr  = NULL;
  
    webconf_wifi_t *ps = NULL;  
    unsigned int i = 0;
    char ssid_2g_str[20] = {0};
    char ssid_5g_str[20] = {0};
    char sec_2g_str[20] = {0};
    char sec_5g_str[20] = {0};
 
    msgpack_unpacked_init( &msg );
    len +=  1;
    /* The outermost wrapper MUST be a map. */
    mp_rv = msgpack_unpack_next( &msg, (const char*) buf, len, &offset );
    if (mp_rv != MSGPACK_UNPACK_SUCCESS) {
        CcspTraceError(("%s: Failed to unpack wifi msg blob. Error %d",__FUNCTION__,mp_rv));
        msgpack_unpacked_destroy( &msg );
        return RETURN_ERR;
    }

    CcspTraceInfo(("%s:Msg unpack success. Offset is %lu\n", __FUNCTION__,offset));
    msgpack_object obj = msg.data;
    
    map = &msg.data.via.map;
    
    map_ptr = obj.via.map.ptr;
    if ((!map) || (!map_ptr)) {
        CcspTraceError(("Failed to get object map\n"));
        msgpack_unpacked_destroy( &msg );
        return RETURN_ERR;
    }

    if (msg.data.type != MSGPACK_OBJECT_MAP) {
        CcspTraceError(("%s: Invalid msgpack type",__FUNCTION__));
        msgpack_unpacked_destroy( &msg );
        return RETURN_ERR;
    }

    /* Allocate memory for wifi structure */
    ps = (webconf_wifi_t *) malloc(sizeof(webconf_wifi_t));
    if (ps == NULL) {
        CcspTraceError(("%s: Wifi Struc malloc error\n",__FUNCTION__));
        return RETURN_ERR;
    }
    memset(ps, 0, sizeof(webconf_wifi_t));
    
    if (ssid == WIFI_WEBCONFIG_PRIVATESSID) {
        snprintf(ssid_2g_str,sizeof(ssid_2g_str),"private_ssid_2g");
        snprintf(ssid_5g_str,sizeof(ssid_5g_str),"private_ssid_5g");
        snprintf(sec_2g_str,sizeof(sec_2g_str),"private_security_2g");
        snprintf(sec_5g_str,sizeof(sec_5g_str),"private_security_5g");
    } else if (ssid == WIFI_WEBCONFIG_HOMESSID) {
        snprintf(ssid_2g_str,sizeof(ssid_2g_str),"home_ssid_2g");
        snprintf(ssid_5g_str,sizeof(ssid_5g_str),"home_ssid_5g");
        snprintf(sec_2g_str,sizeof(sec_2g_str),"home_security_2g");
        snprintf(sec_5g_str,sizeof(sec_5g_str),"home_security_5g");
    } else {
        CcspTraceError(("%s: Invalid ssid type\n",__FUNCTION__));
    }
    /* Parsing Config Msg String to Wifi Structure */
    for (i = 0;i < map->size;i++) {
        if (strncmp(map_ptr->key.via.str.ptr, ssid_2g_str,map_ptr->key.via.str.size) == 0) {
            if (webconf_copy_wifi_ssid_params(map_ptr->val, &ps->ssid_2g) != RETURN_OK) {
                CcspTraceError(("%s:Failed to copy wifi ssid params for wlan index 0",__FUNCTION__));
                msgpack_unpacked_destroy( &msg );
                if (ps) {  
                   free(ps);
                   ps = NULL;  
                } 
                return RETURN_ERR; 
            }  
        }
        else if (strncmp(map_ptr->key.via.str.ptr, ssid_5g_str,map_ptr->key.via.str.size) == 0) {
            if (webconf_copy_wifi_ssid_params(map_ptr->val, &ps->ssid_5g) != RETURN_OK) {
                CcspTraceError(("%s:Failed to copy wifi ssid params for wlan index 1",__FUNCTION__));
                msgpack_unpacked_destroy( &msg );
                if (ps) {
                   free(ps);
                   ps = NULL;
                } 
                return RETURN_ERR;
            }
        }
        else if (strncmp(map_ptr->key.via.str.ptr, sec_2g_str,map_ptr->key.via.str.size) == 0) {
            if (webconf_copy_wifi_security_params(map_ptr->val, &ps->security_2g) != RETURN_OK) {
                CcspTraceError(("%s:Failed to copy wifi security params for wlan index 0",__FUNCTION__));
                msgpack_unpacked_destroy( &msg );
                if (ps) {
                    free(ps);
                    ps = NULL;
                } 
                return RETURN_ERR;
            }
        }
        else if (strncmp(map_ptr->key.via.str.ptr, sec_5g_str,map_ptr->key.via.str.size) == 0) {
            if (webconf_copy_wifi_security_params(map_ptr->val, &ps->security_5g) != RETURN_OK) {
                CcspTraceError(("%s:Failed to copy wifi security params for wlan index 1",__FUNCTION__));
                msgpack_unpacked_destroy( &msg );
                if (ps) {
                    free(ps);
                    ps = NULL;
                } 
                return RETURN_ERR;
            }
        }
        
        else if (strncmp(map_ptr->key.via.str.ptr, "subdoc_name", map_ptr->key.via.str.size) == 0) {
            if (map_ptr->val.type == MSGPACK_OBJECT_STR) {
                strncpy(ps->subdoc_name, map_ptr->val.via.str.ptr, map_ptr->val.via.str.size);
                CcspTraceError(("subdoc name %s\n", ps->subdoc_name));
            }
        }
        else if (strncmp(map_ptr->key.via.str.ptr, "version", map_ptr->key.via.str.size) == 0) {
            if (map_ptr->val.type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
                ps->version = (uint64_t) map_ptr->val.via.u64;
                CcspTraceError(("Version type %d version %lu\n",map_ptr->val.type,ps->version));
            }
        }
        else if (strncmp(map_ptr->key.via.str.ptr, "transaction_id", map_ptr->key.via.str.size) == 0) {
            if (map_ptr->val.type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
                ps->transaction_id = (uint16_t) map_ptr->val.via.u64;
                CcspTraceError(("Tx id type %d tx id %d\n",map_ptr->val.type,ps->transaction_id));
            }
        }
        ++map_ptr;
    }

    msgpack_unpacked_destroy( &msg );

    execData *execDataPf = NULL ;
 
    execDataPf = (execData*) malloc (sizeof(execData));
    if (execDataPf != NULL) {
        memset(execDataPf, 0, sizeof(execData));

        execDataPf->txid = ps->transaction_id;
        execDataPf->version = ps->version;
        execDataPf->numOfEntries = 1;

        strncpy(execDataPf->subdoc_name,ps->subdoc_name, sizeof(execDataPf->subdoc_name)-1);

        execDataPf->user_data = (void*) ps;
        execDataPf->calcTimeout = webconf_ssid_timeout_handler;
        execDataPf->executeBlobRequest = webconf_wifi_ssid_config_handler;
        execDataPf->rollbackFunc = webconf_ssid_rollback_handler;
        execDataPf->freeResources = webconf_ssid_free_resources;
        PushBlobRequest(execDataPf);
        CcspTraceInfo(("PushBlobRequest Complete\n"));

    }
    return RETURN_OK;
}

char *wifi_apply_ssid_config(wifi_vap_info_t *vap_cfg, wifi_vap_info_t *curr_cfg, BOOL is_tunnel,BOOL is_vap_enabled) 
{
    int retval = RETURN_ERR;
    BOOLEAN bForceDisableFlag = FALSE;
    uint8_t wlan_index = vap_cfg->vap_index;

    if(ANSC_STATUS_FAILURE == CosaDmlWiFiGetCurrForceDisableWiFiRadio(&bForceDisableFlag))
    {
        CcspTraceError(("%s Failed to fetch ForceDisableWiFiRadio flag!!!\n",__FUNCTION__));
        return "Dml fetch failed";
    }       
    

    /* Apply SSID Enable/Disable */
    if ((curr_cfg->u.bss_info.enabled != vap_cfg->u.bss_info.enabled) && (!bForceDisableFlag)) {
        CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : %s Calling wifi_setEnable"
                        " to enable/disable SSID on interface:  %d enable: %d\n",
                         __FUNCTION__, wlan_index, vap_cfg->u.bss_info.enabled));
        retval = wifi_setSSIDEnable(wlan_index, vap_cfg->u.bss_info.enabled);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to set AP Enable for wlan %d\n",__FUNCTION__, wlan_index));
            return "wifi_setSSIDEnable failed";
        }
        if (strcmp(vap_cfg->vap_name,"iot_ssid_2g") == 0) {
            char passph[128]={0};
            wifi_getApSecurityKeyPassphrase(2, passph);
            wifi_setApSecurityKeyPassphrase(3, passph);
            wifi_getApSecurityPreSharedKey(2, passph);
            wifi_setApSecurityPreSharedKey(3, passph);
            g_newXH5Gpass=TRUE;
        }
        CcspWifiTrace(("RDK_LOG_WARN,WIFI %s wifi_setApEnable success  index %d , %d",
                     __FUNCTION__,wlan_index, vap_cfg->u.bss_info.enabled));

#if (!defined(_XB6_PRODUCT_REQ_) || defined (_XB7_PRODUCT_REQ_))
        if (vap_cfg->u.bss_info.enabled) {
            BOOL enable_wps = FALSE;
            retval = wifi_getApWpsEnable(wlan_index, &enable_wps);
            if (retval != RETURN_OK) {
                CcspTraceError(("%s: Failed to get Ap Wps Enable\n", __FUNCTION__));
                return "wifi_getApWpsEnable failed";
            }
            
            BOOL up;
            char status[64]={0};
            if (wifi_getSSIDStatus(wlan_index, status) != RETURN_OK) {
                CcspTraceError(("%s: Failed to get SSID Status\n", __FUNCTION__));
                return "wifi_getSSIDStatus failed";
            }
            up = (strcmp(status,"Enabled")==0) || (strcmp(status,"Up")==0);
            CcspTraceInfo(("SSID status is %s\n",status));
            if (up == FALSE) {
#if defined(_INTEL_WAV_)
                retval = wifi_setSSIDEnable(wlan_index, TRUE);
                if (retval != RETURN_OK) {
                    CcspTraceError(("%s: Failed to enable AP Interface for wlan %d\n",
                                __FUNCTION__, wlan_index));
                    return "wifi_setSSIDEnable failed";
                }
                CcspTraceInfo(("AP Enabled Successfully %d\n\n",wlan_index));
#else
                uint8_t radio_index = (wlan_index % 2);
                retval = wifi_createAp(wlan_index, radio_index, vap_cfg->u.bss_info.ssid, 
                             (vap_cfg->u.bss_info.showSsid == TRUE) ? FALSE : TRUE);
                if (retval != RETURN_OK) {
                    CcspTraceError(("%s: Failed to create AP Interface for wlan %d\n",
                                __FUNCTION__, wlan_index));
                    return "wifi_createAp failed";
                }
                CcspTraceInfo(("AP Created Successfully %d\n\n",wlan_index));
#endif
                gradio_restart[wlan_index%2] = TRUE;
            }
            if (vap_cfg->u.bss_info.security.mode >= (wifi_security_modes_t)COSA_DML_WIFI_SECURITY_WPA_Personal) {
                retval = wifi_removeApSecVaribles(wlan_index);
                if (retval != RETURN_OK) {
                    CcspTraceError(("%s: Failed to remove AP SEC Variable for wlan %d\n",
                                     __FUNCTION__, wlan_index));
                    return "wifi_removeApSecVaribles failed";
                }
                retval = wifi_createHostApdConfig(wlan_index, enable_wps);
                if (retval != RETURN_OK) {
                    CcspTraceError(("%s: Failed to create hostapd config for wlan %d\n",
                                 __FUNCTION__, wlan_index));
                    return "wifi_createHostApdConfig failed";
                }
                gHostapd_restart_reqd = true;
                CcspTraceInfo(("Created hostapd config successfully wlan_index %d\n", wlan_index));
            }
#ifdef CISCO_XB3_PLATFORM_CHANGES
            wifi_ifConfigUp(wlan_index);
#endif
            PCOSA_DML_WIFI_AP_CFG pStoredApCfg = &sWiFiDmlApStoredCfg[wlan_index].Cfg;
            CosaDmlWiFiApPushCfg(pStoredApCfg);
            CosaDmlWiFiApMfPushCfg(sWiFiDmlApMfCfg[wlan_index], wlan_index);
            CosaDmlWiFiApPushMacFilter(sWiFiDmlApMfQueue[wlan_index], wlan_index);
            wifi_pushBridgeInfo(wlan_index);
            wifi_setApEnable(wlan_index, TRUE);
        } else {
#ifdef CISCO_XB3_PLATFORM_CHANGES
            wifi_ifConfigDown(wlan_index);
#endif

            if (curr_cfg->u.bss_info.security.mode >= (wifi_security_modes_t)COSA_DML_WIFI_SECURITY_WPA_Personal) {
                retval = wifi_removeApSecVaribles(wlan_index);
                if (retval != RETURN_OK) {
                    CcspTraceError(("%s: Failed to remove AP SEC Variable for wlan %d\n",
                                     __FUNCTION__, wlan_index));
                    return "wifi_removeApSecVaribles failed"; 
                }
                gHostapd_restart_reqd = true;
            }
        }
#endif /* _XB6_PRODUCT_REQ_ */
        curr_cfg->u.bss_info.enabled = vap_cfg->u.bss_info.enabled;
        CcspTraceInfo(("%s: SSID Enable change applied for wlan %d\n",__FUNCTION__, wlan_index));
    } else if (bForceDisableFlag == TRUE) {
        CcspWifiTrace(("RDK_LOG_WARN, WIFI_ATTEMPT_TO_CHANGE_CONFIG_WHEN_FORCE_DISABLED \n"));
    } else if ((strcmp(vap_cfg->vap_name,"hotspot_secure_2g") == 0) || (strcmp(vap_cfg->vap_name,"hotspot_secure_5g") == 0) ||
             (strcmp(vap_cfg->vap_name,"hotspot_open_2g")== 0) || (strcmp(vap_cfg->vap_name,"hotspot_open_5g") == 0)) {
        if (vap_cfg->u.bss_info.enabled) {
            if (wifi_setApEnable(wlan_index, TRUE) != RETURN_OK) {
                CcspTraceError(("%s: wifi_setApEnable failed  index %d\n",__FUNCTION__,wlan_index));
                return "wifi_setApEnable failed";
            }
        }
    }
   
    if (is_tunnel) {
        return NULL;
    }

    /* Apply SSID values to hal */
    if (((strcmp(vap_cfg->u.bss_info.ssid, curr_cfg->u.bss_info.ssid) !=0) || (is_vap_enabled)) && 
        (!bForceDisableFlag)) {
        CcspTraceInfo(("RDKB_WIFI_CONFIG_CHANGED : %s Calling wifi_setSSID to "
                        "change SSID name on interface: %d SSID: %s \n",
                         __FUNCTION__,wlan_index,vap_cfg->u.bss_info.ssid));
        t2_event_d("WIFI_INFO_XHCofigchanged", 1);
        if (vap_cfg->u.bss_info.enabled == TRUE) {
        retval = wifi_setSSIDName(wlan_index, vap_cfg->u.bss_info.ssid);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to apply SSID name for wlan %d\n",__FUNCTION__, wlan_index));
            return "wifi_setSSIDName failed";
        }

        retval = wifi_pushSSID(wlan_index, vap_cfg->u.bss_info.ssid);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to push SSID name for wlan %d\n",__FUNCTION__, wlan_index));
            return "wifi_pushSSID failed";
        }
        strncpy(curr_cfg->u.bss_info.ssid,vap_cfg->u.bss_info.ssid, sizeof(curr_cfg->u.bss_info.ssid)-1);
        CcspTraceInfo(("%s: SSID name change applied for wlan %d\n",__FUNCTION__, wlan_index));

#if defined(ENABLE_FEATURE_MESHWIFI)
        CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of SSID change\n",__FUNCTION__));
        v_secure_system("/usr/bin/sysevent set wifi_SSIDName \"RDK|%d|%s\"",wlan_index, vap_cfg->u.bss_info.ssid);
#endif
        if (strcmp(vap_cfg->vap_name, "private_ssid_2g") == 0) {
            SSID1_UPDATED = TRUE;
        } else if (strcmp(vap_cfg->vap_name, "private_ssid_5g") == 0) {
            SSID2_UPDATED = TRUE;
        }
        } else {
            strncpy(curr_cfg->u.bss_info.ssid,vap_cfg->u.bss_info.ssid, sizeof(curr_cfg->u.bss_info.ssid)-1);
        }
    } else if (bForceDisableFlag == TRUE) {
        CcspWifiTrace(("RDK_LOG_WARN, WIFI_ATTEMPT_TO_CHANGE_CONFIG_WHEN_FORCE_DISABLED \n"));
    }
 
    if (vap_cfg->u.bss_info.showSsid != curr_cfg->u.bss_info.showSsid || is_vap_enabled) {
        if (vap_cfg->u.bss_info.enabled == TRUE) {
        retval = wifi_setApSsidAdvertisementEnable(wlan_index, vap_cfg->u.bss_info.showSsid);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to set SSID Advertisement Status for wlan %d\n",
                                                             __FUNCTION__, wlan_index));
            return "wifi_setApSsidAdvertisementEnable failed";
        }

//wifi_pushSsidAdvertisementEnable is required only for XB3
//This Macro is applicable to XB6 and XB7
#if (!defined(_XB6_PRODUCT_REQ_)) 
        retval = wifi_pushSsidAdvertisementEnable(wlan_index, vap_cfg->u.bss_info.showSsid);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to push SSID Advertisement Status for wlan %d\n",
                                                             __FUNCTION__, wlan_index));
            return "wifi_pushSsidAdvertisementEnable failed";
        }
        vap_cfg->u.bss_info.sec_changed = true;
#endif

#if defined(ENABLE_FEATURE_MESHWIFI)
        CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of SSID Advertise changes\n",__FUNCTION__));
        v_secure_system("/usr/bin/sysevent set wifi_SSIDAdvertisementEnable \"RDK|%d|%s\"",
                        wlan_index, vap_cfg->u.bss_info.showSsid?"true":"false");
#endif
        gHostapd_restart_reqd = true;
        curr_cfg->u.bss_info.showSsid = vap_cfg->u.bss_info.showSsid;
        CcspTraceInfo(("%s: Advertisement change applied for wlan index: %d\n",
                                                    __FUNCTION__, wlan_index));
    } else {
        curr_cfg->u.bss_info.showSsid = vap_cfg->u.bss_info.showSsid;
    }
    }
    if (vap_cfg->u.bss_info.isolation != curr_cfg->u.bss_info.isolation) {
        if (vap_cfg->u.bss_info.enabled == TRUE) {
        retval = wifi_setApIsolationEnable(wlan_index, vap_cfg->u.bss_info.isolation);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to Isolation enable for wlan %d\n",
                                                __FUNCTION__, wlan_index));
            return "wifi_setApIsolationEnable failed";
        }
        curr_cfg->u.bss_info.isolation = vap_cfg->u.bss_info.isolation;
        CcspTraceInfo(("%s: Isolation enable applied for wlan index: %d\n",
                                                __FUNCTION__, wlan_index));
        } else {
            CcspTraceError(("%s: Isolation enable cannot be changed when vap is disabled\n",__FUNCTION__));
        }
    }

    if (vap_cfg->u.bss_info.bssMaxSta != curr_cfg->u.bss_info.bssMaxSta) {
        if (vap_cfg->u.bss_info.enabled == TRUE) {
        retval = wifi_setApMaxAssociatedDevices(wlan_index, vap_cfg->u.bss_info.bssMaxSta);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to Isolation enable for wlan %d\n",
                                                __FUNCTION__, wlan_index));
            return "wifi_setApMaxAssociatedDevices failed";
        }
        curr_cfg->u.bss_info.bssMaxSta = vap_cfg->u.bss_info.bssMaxSta;
        CcspTraceInfo(("%s: Max station value applied for wlan index: %d\n",
                                                    __FUNCTION__, wlan_index));
        } else {
            CcspTraceError(("%s: Neighbor report cannot be applied when vap is disabled\n",__FUNCTION__));
        }
    }

#if !defined(_XB7_PRODUCT_REQ_) && !defined(_HUB4_PRODUCT_REQ_)
#if defined(ENABLE_FEATURE_MESHWIFI) || defined(_CBR_PRODUCT_REQ_) || defined(_COSA_BCM_MIPS_) || defined(HUB4_WLDM_SUPPORT)
    if (vap_cfg->u.bss_info.nbrReportActivated != curr_cfg->u.bss_info.nbrReportActivated) {
        if (vap_cfg->u.bss_info.enabled == TRUE) {
        retval = wifi_setNeighborReportActivation(wlan_index, vap_cfg->u.bss_info.nbrReportActivated);
        if (retval != RETURN_OK) {  // RDKB-34744 - Issue due to hal return status
            CcspTraceError(("%s: Failed to set neighbor report for wlan %d\n",
                                               __FUNCTION__, wlan_index));
            return "wifi_setNeighborReportActivation failed";
        }
        curr_cfg->u.bss_info.nbrReportActivated = vap_cfg->u.bss_info.nbrReportActivated;
        CcspTraceInfo(("%s: Neighbor report activation applied for wlan index: %d\n",
                                                    __FUNCTION__, wlan_index));
        } else {
            CcspTraceError(("%s: Neighboreport cannot be applied when vap is disabled\n",__FUNCTION__));
        }
    }
#endif
#endif

    if (vap_cfg->u.bss_info.vapStatsEnable != curr_cfg->u.bss_info.vapStatsEnable) {
        if (vap_cfg->u.bss_info.enabled == TRUE) {
        retval = wifi_stats_flag_change(wlan_index, vap_cfg->u.bss_info.vapStatsEnable, 1);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to set wifi stats flag for wlan %d\n",
                                               __FUNCTION__, wlan_index));
            return "wifi_stats_flag_change failed";
        }
        curr_cfg->u.bss_info.vapStatsEnable = vap_cfg->u.bss_info.vapStatsEnable;
        CcspTraceInfo(("%s: vap stats enable applied for wlan index: %d\n",
                                                    __FUNCTION__, wlan_index));
        } else {
            CcspTraceError(("%s: Vapstats enable cannot be applied when vap is disabled\n",__FUNCTION__));
        }
    }

    if (vap_cfg->u.bss_info.bssTransitionActivated != curr_cfg->u.bss_info.bssTransitionActivated) {
        if (vap_cfg->u.bss_info.enabled == TRUE) {
        if (gbsstrans_support[wlan_index] || gwirelessmgmt_support[wlan_index]) {
#if !defined(_HUB4_PRODUCT_REQ_) && !defined(_XB7_PRODUCT_REQ_) || defined(HUB4_WLDM_SUPPORT)
            if(vap_cfg->u.bss_info.security.mode != (wifi_security_modes_t)COSA_DML_WIFI_SECURITY_None) {
                retval = wifi_setBSSTransitionActivation(wlan_index, vap_cfg->u.bss_info.bssTransitionActivated);
                if (retval != RETURN_OK) { 
                    CcspTraceError(("%s: Failed to set BssTransition for wlan %d\n",__FUNCTION__, wlan_index));
                    return "wifi_setBSSTransitionActivation failed";
                }
                CcspTraceInfo(("%s: Bss Transition activation applied for wlan index: %d\n",
                                                    __FUNCTION__, wlan_index));
            }
#endif
        curr_cfg->u.bss_info.bssTransitionActivated = vap_cfg->u.bss_info.bssTransitionActivated;
        } else {
            CcspTraceError(("%s: Bss transition not supported for wlan index: %d\n",
                                                    __FUNCTION__, wlan_index));
        }
        } else {
            CcspTraceError(("%s: Bss transition cannot be applied when vap is disabled\n",__FUNCTION__));
        }
    }
   
    if (vap_cfg->u.bss_info.mgmtPowerControl != curr_cfg->u.bss_info.mgmtPowerControl) {
        if (vap_cfg->u.bss_info.enabled == TRUE) {
        retval = wifi_setApManagementFramePowerControl(wlan_index, vap_cfg->u.bss_info.mgmtPowerControl);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to set Management power control for wlan %d\n",
                                                         __FUNCTION__, wlan_index));
            return "wifi_setApManagementFramePowerControl failed";
        }
        curr_cfg->u.bss_info.mgmtPowerControl = vap_cfg->u.bss_info.mgmtPowerControl;
        CcspTraceInfo(("%s: Management frame power applied for wlan index: %d\n",
                                                    __FUNCTION__, wlan_index));
        } else {
            CcspTraceError(("%s: Frame power control cannot be applied when vap is disabled\n",__FUNCTION__));
        }
    }

    if (0 != strcmp(vap_cfg->u.bss_info.security.mfpConfig, curr_cfg->u.bss_info.security.mfpConfig)) {
        if (vap_cfg->u.bss_info.enabled == TRUE) { 
        retval = wifi_setApSecurityMFPConfig(wlan_index,vap_cfg->u.bss_info.security.mfpConfig);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to set MFP Config for wlan %d\n",
                                                         __FUNCTION__, wlan_index));
            return "wifi_setApSecurityMFPConfig failed";
        }
        strcpy(curr_cfg->u.bss_info.security.mfpConfig, vap_cfg->u.bss_info.security.mfpConfig);
        CcspTraceInfo(("%s: MFP Config applied for wlan index: %d\n",
                                          __FUNCTION__, wlan_index));
        } else {
            CcspTraceError(("%s: MFP Config cannot be applied when vap is disabled\n",__FUNCTION__));
        }
    }
    return NULL; 
}

char *wifi_apply_security_config(wifi_vap_info_t *vap_cfg, wifi_vap_info_t *curr_cfg,BOOL is_vap_enabled)
{
    int retval = RETURN_ERR;
    BOOLEAN bForceDisableFlag = FALSE;
    uint8_t wlan_index = vap_cfg->vap_index;
    char mode[32] = {0};
    char securityType[32] = {0};
    char authMode[32] = {0};
    char method[32] = {0};
    char encryption[32] = {0};

 
    if(ANSC_STATUS_FAILURE == CosaDmlWiFiGetCurrForceDisableWiFiRadio(&bForceDisableFlag))
    {
        CcspTraceError(("%s Failed to fetch ForceDisableWiFiRadio flag!!!\n",__FUNCTION__));
        return "Dml fetch failed";
    }
    
    webconf_auth_mode_to_str(mode, vap_cfg->u.bss_info.security.mode);
    /* Copy hal specific strings for respective Authentication Mode */
    if (strcmp(mode, "None") == 0 ) {
        strcpy(securityType,"None");
        strcpy(authMode,"None");
    }
#ifndef _XB6_PRODUCT_REQ_
    else if (strcmp(mode, "WEP-64") == 0 || strcmp(mode, "WEP-128") == 0) {
        strcpy(securityType, "Basic");
        strcpy(authMode,"None");
    }
    else if (strcmp(mode, "WPA-Personal") == 0) {
        strcpy(securityType,"WPA");
        strcpy(authMode,"PSKAuthentication");
    } else if (strcmp(mode, "WPA2-Personal") == 0) {
        strcpy(securityType,"11i");
        strcpy(authMode,"PSKAuthentication");
    } else if (strcmp(mode, "WPA-Enterprise") == 0) {
    } else if (strcmp(mode, "WPA-WPA2-Personal") == 0) {
        strcpy(securityType,"WPAand11i");
        strcpy(authMode,"PSKAuthentication");
    }
#else
    else if ((strcmp(mode, "WPA2-Personal") == 0)) {
        strcpy(securityType,"11i");
        strcpy(authMode,"SharedAuthentication");
    }
#endif
    else if (strcmp(mode, "WPA2-Enterprise") == 0) {
        strcpy(securityType,"11i");
        strcpy(authMode,"EAPAuthentication");
    } else if (strcmp(mode, "WPA-WPA2-Enterprise") == 0) {
        strcpy(securityType,"WPAand11i");
        strcpy(authMode,"EAPAuthentication");
    }

    webconf_enc_mode_to_str(encryption,vap_cfg->u.bss_info.security.encr);
    if ((strcmp(encryption, "TKIP") == 0)) {
        strcpy(method,"TKIPEncryption");
    } else if ((strcmp(encryption, "AES") == 0)) {
        strcpy(method,"AESEncryption");
    }
#ifndef _XB6_PRODUCT_REQ_
    else if ((strcmp(encryption, "AES+TKIP") == 0)) {
        strcpy(method,"TKIPandAESEncryption");
    }
#endif
     
    /* Apply Security Values to hal */
    
    if (vap_cfg->u.bss_info.security.mode != curr_cfg->u.bss_info.security.mode) {
        if (vap_cfg->u.bss_info.enabled == TRUE) {
        retval = wifi_setApBeaconType(wlan_index, securityType);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to set AP Beacon type\n", __FUNCTION__));
            return "wifi_setApBeaconType failed";
        }
        CcspWifiTrace(("RDK_LOG_WARN,%s calling setBasicAuthenticationMode ssid : %d authmode : %s \n",
                       __FUNCTION__,wlan_index, mode));
        retval = wifi_setApBasicAuthenticationMode(wlan_index, authMode);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to set AP Authentication Mode\n", __FUNCTION__));
            return "wifi_setApBasicAuthenticationMode failed";
        }
        
        if ((curr_cfg->u.bss_info.security.mode == wifi_security_mode_none) &&
            (vap_cfg->u.bss_info.security.mode >= wifi_security_mode_wpa_personal) &&
            (vap_cfg->u.bss_info.security.mode <= wifi_security_mode_wpa_wpa2_personal)) {
            retval = wifi_setApSecurityKeyPassphrase(wlan_index, vap_cfg->u.bss_info.security.u.key.key); 
            if (retval != RETURN_OK) {
                CcspTraceError(("%s: Failed to set AP Security Passphrase\n", __FUNCTION__));
                return "wifi_setApSecurityKeyPassphrase failed";
            }
	    wifi_setApSecurityPreSharedKey(wlan_index, vap_cfg->u.bss_info.security.u.key.key);
        }
        curr_cfg->u.bss_info.security.mode = vap_cfg->u.bss_info.security.mode;
        
        vap_cfg->u.bss_info.sec_changed = true; 
        gHostapd_restart_reqd = true;
        CcspWifiEventTrace(("RDK_LOG_NOTICE, Wifi security mode %s is Enabled", mode));
        CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : Wifi security mode %s is Enabled\n",mode));
        CcspTraceInfo(("%s: Security Mode Change Applied for wlan index %d\n", __FUNCTION__,wlan_index));
        } else {
            CcspTraceError(("%s: Security Mode cannot be changed when vap is disabled\n",__FUNCTION__));
        }
    }
    
    if ((vap_cfg->u.bss_info.security.mode >= wifi_security_mode_wpa_personal) &&
        (vap_cfg->u.bss_info.security.mode <= wifi_security_mode_wpa_wpa2_personal) &&
        (strcmp(vap_cfg->u.bss_info.security.u.key.key,curr_cfg->u.bss_info.security.u.key.key) != 0) &&
        (!bForceDisableFlag)) {
        if (vap_cfg->u.bss_info.enabled == TRUE) {
        CcspTraceInfo(("KeyPassphrase changed for index = %d\n",wlan_index));
        retval = wifi_setApSecurityKeyPassphrase(wlan_index, vap_cfg->u.bss_info.security.u.key.key);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to set AP Security Passphrase\n", __FUNCTION__));
            return "wifi_setApSecurityKeyPassphrase failed";
        }
	wifi_setApSecurityPreSharedKey(wlan_index, vap_cfg->u.bss_info.security.u.key.key);
        strncpy(curr_cfg->u.bss_info.security.u.key.key,vap_cfg->u.bss_info.security.u.key.key,
                sizeof(curr_cfg->u.bss_info.security.u.key.key)-1);

        gHostapd_restart_reqd = true;
        vap_cfg->u.bss_info.sec_changed = true; 
        if (wlan_index == 0) {
            PASSPHRASE1_UPDATED = TRUE;
        } else if (wlan_index == 1) {
            PASSPHRASE2_UPDATED = TRUE;
        }
        CcspWifiEventTrace(("RDK_LOG_NOTICE, KeyPassphrase changed \n "));
        CcspWifiTrace(("RDK_LOG_WARN, KeyPassphrase changed \n "));
        CcspWifiTrace(("RDK_LOG_WARN, RDKB_WIFI_CONFIG_CHANGED : %s KeyPassphrase changed for index = %d\n",
                        __FUNCTION__, wlan_index));
        CcspTraceInfo(("%s: Passpharse change applied for wlan index %d\n", __FUNCTION__, wlan_index));
        } else {
            CcspTraceError(("Passphrase cannot be changed when vap is disabled \n", __FUNCTION__));
        }
    } else if (bForceDisableFlag == TRUE) {
        CcspWifiTrace(("RDK_LOG_WARN, WIFI_ATTEMPT_TO_CHANGE_CONFIG_WHEN_FORCE_DISABLED \n"));
    }

    if ((vap_cfg->u.bss_info.security.encr != curr_cfg->u.bss_info.security.encr) &&
        (vap_cfg->u.bss_info.security.mode >= (wifi_security_modes_t)COSA_DML_WIFI_SECURITY_WPA_Personal) &&
        (vap_cfg->u.bss_info.security.mode <= (wifi_security_modes_t)COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise)) {
        if (vap_cfg->u.bss_info.enabled == TRUE) {
        CcspWifiTrace(("RDK_LOG_WARN, RDKB_WIFI_CONFIG_CHANGED :%s Encryption method changed , "
                       "calling setWpaEncryptionMode Index : %d mode : %s \n",
                       __FUNCTION__,wlan_index, encryption));
        retval = wifi_setApWpaEncryptionMode(wlan_index, method);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to set WPA Encryption Mode\n", __FUNCTION__));
            return "wifi_setApWpaEncryptionMode failed";
        }
        curr_cfg->u.bss_info.security.encr = vap_cfg->u.bss_info.security.encr;

        vap_cfg->u.bss_info.sec_changed = true; 
        gHostapd_restart_reqd = true;
        CcspTraceInfo(("%s: Encryption mode change applied for wlan index %d\n",
                        __FUNCTION__, wlan_index));
        } else {
            CcspTraceError(("Encryption mode cannot changed when vap is disabled \n", __FUNCTION__));
        }
    }
    
    if (vap_cfg->u.bss_info.sec_changed) {
        CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of Security changes\n",__FUNCTION__));
        v_secure_system("/usr/bin/sysevent set wifi_ApSecurity \"RDK|%d|%s|%s|%s\"",
          wlan_index, vap_cfg->u.bss_info.security.u.key.key, authMode, method);
    }

    if ((strcmp(vap_cfg->vap_name,"hotspot_secure_2g") == 0 || strcmp(vap_cfg->vap_name,"hotspot_secure_5g") == 0) &&
        ((strcmp((const char *)vap_cfg->u.bss_info.security.u.radius.ip, (const char *)curr_cfg->u.bss_info.security.u.radius.ip) != 0 || 
           vap_cfg->u.bss_info.security.u.radius.port != curr_cfg->u.bss_info.security.u.radius.port ||
        strcmp(vap_cfg->u.bss_info.security.u.radius.key, curr_cfg->u.bss_info.security.u.radius.key) != 0) || (is_vap_enabled))) {
        if (vap_cfg->u.bss_info.enabled == TRUE) {
        retval = wifi_setApSecurityRadiusServer(wlan_index, (char *)vap_cfg->u.bss_info.security.u.radius.ip,
                   vap_cfg->u.bss_info.security.u.radius.port, vap_cfg->u.bss_info.security.u.radius.key);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to apply Radius server configs for wlan %d\n",__FUNCTION__, wlan_index));
            return "Failed to apply Radius Config";
        }
        }
        strncpy((char*)curr_cfg->u.bss_info.security.u.radius.ip,(char*) vap_cfg->u.bss_info.security.u.radius.ip,
                sizeof(curr_cfg->u.bss_info.security.u.radius.ip)-1);
        strncpy(curr_cfg->u.bss_info.security.u.radius.key, vap_cfg->u.bss_info.security.u.radius.key,
                sizeof(curr_cfg->u.bss_info.security.u.radius.key)-1);
        curr_cfg->u.bss_info.security.u.radius.port = vap_cfg->u.bss_info.security.u.radius.port;
        CcspTraceInfo(("%s: Radius Configs applied for wlan index %d\n", __FUNCTION__, wlan_index));
    }

    if ((strcmp(vap_cfg->vap_name,"hotspot_secure_2g") == 0 || strcmp(vap_cfg->vap_name,"hotspot_secure_5g") == 0) &&
        ((strcmp((const char *)vap_cfg->u.bss_info.security.u.radius.s_ip, (const char *)curr_cfg->u.bss_info.security.u.radius.s_ip) != 0 ||
           vap_cfg->u.bss_info.security.u.radius.s_port != curr_cfg->u.bss_info.security.u.radius.s_port ||
        strcmp(vap_cfg->u.bss_info.security.u.radius.s_key, curr_cfg->u.bss_info.security.u.radius.s_key) != 0) || (is_vap_enabled))) {
        if (vap_cfg->u.bss_info.enabled == TRUE) {
        retval = wifi_setApSecuritySecondaryRadiusServer(wlan_index, (char *)vap_cfg->u.bss_info.security.u.radius.s_ip,
                  vap_cfg->u.bss_info.security.u.radius.s_port, vap_cfg->u.bss_info.security.u.radius.s_key);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to apply Secondary Radius server configs for wlan %d retval %d\n",
                                 __FUNCTION__, wlan_index,retval));
#if !defined(_CBR_PRODUCT_REQ_) /*TCCBR-5627 By default this api is giving error in cbr. This will be removed once ticket is fixed */
            /*return "wifi_setApSecuritySecondaryRadiusServer failed";*/
#endif
        }
        }
        strncpy((char*)curr_cfg->u.bss_info.security.u.radius.s_ip, (char*)vap_cfg->u.bss_info.security.u.radius.s_ip,
                sizeof(curr_cfg->u.bss_info.security.u.radius.s_ip)-1);
        strncpy(curr_cfg->u.bss_info.security.u.radius.s_key, vap_cfg->u.bss_info.security.u.radius.s_key,
                sizeof(curr_cfg->u.bss_info.security.u.radius.s_key)-1);
        curr_cfg->u.bss_info.security.u.radius.s_port = vap_cfg->u.bss_info.security.u.radius.s_port;
        CcspTraceInfo(("%s: Secondary Radius Configs applied for wlan index %d\n", __FUNCTION__, wlan_index));
    }

      
#if (!defined(_XB6_PRODUCT_REQ_) || defined (_XB7_PRODUCT_REQ_))

    if ((vap_cfg->u.bss_info.sec_changed == true) && (vap_cfg->u.bss_info.enabled == TRUE)) {
        BOOL enable_wps = FALSE;
        retval = wifi_getApWpsEnable(wlan_index, &enable_wps);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to get Ap Wps Enable\n", __FUNCTION__));
            return "wifi_getApWpsEnable failed"; 
        }

        retval = wifi_removeApSecVaribles(wlan_index);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to remove Ap Sec variables\n", __FUNCTION__));
            return "wifi_removeApSecVaribles failed";
        }

#if !defined(_XB7_PRODUCT_REQ_) && !defined(_XB8_PRODUCT_REQ_) && !defined(_CBR2_PRODUCT_REQ)
        retval = wifi_disableApEncryption(wlan_index);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to disable AP Encryption\n",__FUNCTION__));
            return "wifi_disableApEncryption failed";
        }
#endif

        if (vap_cfg->u.bss_info.security.mode == (wifi_security_modes_t)COSA_DML_WIFI_SECURITY_None) {
            if (enable_wps == TRUE) {
                retval = wifi_createHostApdConfig(wlan_index, TRUE);
            }
        }
        else {
            retval = wifi_createHostApdConfig(wlan_index, enable_wps);
        }
       
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to create Host Apd Config\n",__FUNCTION__));
            return "wifi_createHostApdConfig failed";
        }
        if (wifi_setApEnable(wlan_index, TRUE) != RETURN_OK) {
            CcspTraceError(("%s: wifi_setApEnable failed  index %d\n",__FUNCTION__,wlan_index));
        }
        CcspTraceInfo(("%s: Security changes applied for wlan index %d\n",
                       __FUNCTION__, wlan_index));
    }
#endif /* #if (!defined(_XB6_PRODUCT_REQ_) || defined (_XB7_PRODUCT_REQ_)) */
    vap_cfg->u.bss_info.sec_changed = false;
    return NULL;
}

char *wifi_apply_interworking_config(wifi_vap_info_t *vap_cfg, wifi_vap_info_t *curr_cfg)
{
#if defined (FEATURE_SUPPORT_INTERWORKING) || defined (FEATURE_SUPPORT_PASSPOINT)
    int retval = RETURN_ERR;
#endif
    uint8_t wlan_index = vap_cfg->vap_index;


    if (memcmp(&vap_cfg->u.bss_info.interworking.interworking,
          &curr_cfg->u.bss_info.interworking.interworking,
        sizeof(vap_cfg->u.bss_info.interworking.interworking)) != 0) {
        if (!(vap_cfg->u.bss_info.enabled == FALSE &&
             vap_cfg->u.bss_info.interworking.interworking.interworkingEnabled)) {
             gHostapd_restart_reqd = true;
#if defined (FEATURE_SUPPORT_INTERWORKING)
            retval = wifi_pushApInterworkingElement(wlan_index,&vap_cfg->u.bss_info.interworking.interworking);
            if (retval != RETURN_OK && vap_cfg->u.bss_info.enabled == TRUE) {
                CcspTraceError(("%s: Failed to push Interworking element to hal for wlan %d\n",
                                __FUNCTION__, wlan_index));
                return "wifi_pushApInterworkingElement failed";
            }
#endif
            memcpy(&curr_cfg->u.bss_info.interworking.interworking, &vap_cfg->u.bss_info.interworking.interworking,
                   sizeof(curr_cfg->u.bss_info.interworking.interworking));
            CcspTraceInfo(("%s: Applied Interworking configuration successfully for wlan %d\n",
                       __FUNCTION__, wlan_index));    
        } else {
            CcspTraceError(("%s: Interworking cannot be enabled. Vap %s is down \n",
                               __FUNCTION__, vap_cfg->vap_name));
        }
    }
     
#if defined (FEATURE_SUPPORT_PASSPOINT) 
    if ((strcmp(vap_cfg->vap_name,"hotspot_secure_2g") != 0) && (strcmp(vap_cfg->vap_name,"hotspot_secure_5g") != 0)) {
        CcspTraceInfo(("%s: Passpoint settings not needed for Non Secure Hotspot vaps\n",__FUNCTION__));
        return NULL;
    }  
    
    if (memcmp(&vap_cfg->u.bss_info.interworking.roamingConsortium,
            &curr_cfg->u.bss_info.interworking.roamingConsortium,
        sizeof(vap_cfg->u.bss_info.interworking.roamingConsortium)) != 0) {
        if ((vap_cfg->u.bss_info.interworking.interworking.interworkingEnabled == TRUE) &&
             (vap_cfg->u.bss_info.enabled == TRUE)) {
            retval = wifi_pushApRoamingConsortiumElement(wlan_index, 
                         &vap_cfg->u.bss_info.interworking.roamingConsortium);
            if (retval != RETURN_OK) {
                CcspTraceError(("%s: Failed to push Roaming Consotrium to hal for wlan %d\n",
                                __FUNCTION__, wlan_index));
                return "wifi_pushApRoamingConsortiumElement failed";
            }
            gHostapd_restart_reqd = true;
            memcpy(&curr_cfg->u.bss_info.interworking.roamingConsortium,
                &vap_cfg->u.bss_info.interworking.roamingConsortium,
            sizeof(curr_cfg->u.bss_info.interworking.roamingConsortium));
            CcspTraceInfo(("%s: Applied Roaming Consortium configuration successfully for wlan %d\n",
                           __FUNCTION__, wlan_index));
        } else {
            CcspTraceInfo(("%s: Cannot Configure Roaming Consortium for vap %d. Interworking is disabled\n",
                           __FUNCTION__, wlan_index));
        }
    }


    if ((vap_cfg->u.bss_info.interworking.passpoint.enable != curr_cfg->u.bss_info.interworking.passpoint.enable) ||
        (vap_cfg->u.bss_info.interworking.passpoint.gafDisable != curr_cfg->u.bss_info.interworking.passpoint.gafDisable) ||
        (vap_cfg->u.bss_info.interworking.passpoint.p2pDisable != curr_cfg->u.bss_info.interworking.passpoint.p2pDisable) ||
        (vap_cfg->u.bss_info.interworking.passpoint.l2tif != curr_cfg->u.bss_info.interworking.passpoint.l2tif)) {
        if (!(vap_cfg->u.bss_info.interworking.passpoint.enable == TRUE &&
              vap_cfg->u.bss_info.enabled != TRUE)) {
            retval = enablePassPointSettings(wlan_index, vap_cfg->u.bss_info.interworking.passpoint.enable,
                       vap_cfg->u.bss_info.interworking.passpoint.gafDisable,
                       vap_cfg->u.bss_info.interworking.passpoint.p2pDisable,
                       vap_cfg->u.bss_info.interworking.passpoint.l2tif);
            if (retval != RETURN_OK) {
                CcspTraceError(("%s: Failed to push passpoint params to hal for wlan %d\n",
                                __FUNCTION__, wlan_index));
                return "enablePassPointSettings failed";
            }
            gHostapd_restart_reqd = true;
            curr_cfg->u.bss_info.interworking.passpoint.enable = 
                 vap_cfg->u.bss_info.interworking.passpoint.enable;
            curr_cfg->u.bss_info.interworking.passpoint.gafDisable = 
                 vap_cfg->u.bss_info.interworking.passpoint.gafDisable;
            curr_cfg->u.bss_info.interworking.passpoint.p2pDisable =
                 vap_cfg->u.bss_info.interworking.passpoint.p2pDisable;
            curr_cfg->u.bss_info.interworking.passpoint.l2tif = 
                 vap_cfg->u.bss_info.interworking.passpoint.l2tif;  
            CcspTraceInfo(("%s: Applied Passpoint configuration successfully for wlan %d\n",
                       __FUNCTION__, wlan_index));
        } else {
            CcspTraceError(("%s: Cannot enable Passpoint when vap %s is down",__FUNCTION__,vap_cfg->vap_name));
        }
    }

    if (vap_cfg->u.bss_info.enabled == TRUE) {
        memcpy(&curr_cfg->u.bss_info.interworking,
          &vap_cfg->u.bss_info.interworking,
          sizeof(curr_cfg->u.bss_info.interworking));
    }
    
#endif
    return NULL;
}

char *wifi_apply_common_config(wifi_config_t *wifi_cfg, wifi_config_t *curr_cfg) 
{
#if defined (FEATURE_SUPPORT_PASSPOINT) 
    int retval;

    if (memcmp(&wifi_cfg->gas_config, &curr_cfg->gas_config,
               sizeof(wifi_cfg->gas_config)) != RETURN_OK) {

        retval = wifi_setGASConfiguration(wifi_cfg->gas_config.AdvertisementID,&wifi_cfg->gas_config);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to update HAL with GAS Config\n",__FUNCTION__));
            return "wifi_setGASConfiguration failed";
        }
    }
#else
    UNREFERENCED_PARAMETER(wifi_cfg);
    UNREFERENCED_PARAMETER(curr_cfg);
#endif
    return NULL;
}

int wifi_get_initial_vap_config(wifi_vap_info_t *vap_cfg, uint8_t vap_index)
{
    PCOSA_DATAMODEL_WIFI pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PSINGLE_LINK_ENTRY pSLinkEntry = NULL;
    PCOSA_DML_WIFI_SSID  pWifiSsid = NULL;
    PCOSA_DML_WIFI_AP      pWifiAp = NULL;

    if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->SsidQueue, vap_index)) == NULL) {
        CcspTraceError(("%s Data Model object not found!\n",__FUNCTION__));
        return RETURN_ERR;
    }

    if((pWifiSsid = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) {
        CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
        return RETURN_ERR;
    }
    if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->AccessPointQueue, vap_index)) == NULL) {
        CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
        return RETURN_ERR;
    }

    if((pWifiAp = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) {
        CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
        return RETURN_ERR;
    }

    strncpy(vap_cfg->u.bss_info.ssid, pWifiSsid->SSID.Cfg.SSID, sizeof(vap_cfg->u.bss_info.ssid)-1);
    vap_cfg->u.bss_info.enabled = pWifiSsid->SSID.Cfg.bEnabled;
    vap_cfg->u.bss_info.showSsid = pWifiAp->AP.Cfg.SSIDAdvertisementEnabled;
    vap_cfg->u.bss_info.isolation     = pWifiAp->AP.Cfg.IsolationEnable;
    vap_cfg->u.bss_info.security.mode = pWifiAp->SEC.Cfg.ModeEnabled;
    vap_cfg->u.bss_info.security.encr = pWifiAp->SEC.Cfg.EncryptionMethod;
    strncpy(vap_cfg->u.bss_info.security.mfpConfig, pWifiAp->SEC.Cfg.MFPConfig, sizeof(vap_cfg->u.bss_info.security.mfpConfig)-1);

    vap_cfg->u.bss_info.bssMaxSta = pWifiAp->AP.Cfg.BssMaxNumSta;
    vap_cfg->u.bss_info.nbrReportActivated = pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_NeighborReportActivated;
    vap_cfg->u.bss_info.vapStatsEnable = pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_StatsEnable;
    vap_cfg->u.bss_info.bssTransitionActivated = pWifiAp->AP.Cfg.BSSTransitionActivated;
    vap_cfg->u.bss_info.mgmtPowerControl = pWifiAp->AP.Cfg.ManagementFramePowerControl;

    /* Store Radius settings for Secure Hotspot vap */
    if ((vap_index == 8) || (vap_index == 9)) {
        strncpy((char *)vap_cfg->u.bss_info.security.u.radius.ip,(char *)pWifiAp->SEC.Cfg.RadiusServerIPAddr,
                sizeof(vap_cfg->u.bss_info.security.u.radius.ip)-1); /*wifi hal*/
        vap_cfg->u.bss_info.security.u.radius.port = pWifiAp->SEC.Cfg.RadiusServerPort;
        strncpy(vap_cfg->u.bss_info.security.u.radius.key, pWifiAp->SEC.Cfg.RadiusSecret,
                sizeof(vap_cfg->u.bss_info.security.u.radius.key)-1);
        strncpy((char *)vap_cfg->u.bss_info.security.u.radius.s_ip,(char *) pWifiAp->SEC.Cfg.SecondaryRadiusServerIPAddr,
                sizeof(vap_cfg->u.bss_info.security.u.radius.s_ip)-1); /*wifi hal*/
        vap_cfg->u.bss_info.security.u.radius.s_port = pWifiAp->SEC.Cfg.SecondaryRadiusServerPort;
        strncpy(vap_cfg->u.bss_info.security.u.radius.s_key, pWifiAp->SEC.Cfg.SecondaryRadiusSecret,
                sizeof(vap_cfg->u.bss_info.security.u.radius.s_key)-1);
    } else if((vap_index != 4) || (vap_index != 5)) {
        strncpy(vap_cfg->u.bss_info.security.u.key.key, (char *)pWifiAp->SEC.Cfg.KeyPassphrase,
                sizeof(vap_cfg->u.bss_info.security.u.key.key)-1);
    }

        
    vap_cfg->u.bss_info.interworking.interworking.interworkingEnabled =
                 pWifiAp->AP.Cfg.InterworkingEnable; 
    vap_cfg->u.bss_info.interworking.interworking.accessNetworkType =  
                pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iAccessNetworkType;
    vap_cfg->u.bss_info.interworking.interworking.internetAvailable = 
                pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iInternetAvailable;
    vap_cfg->u.bss_info.interworking.interworking.asra = 
                pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iASRA;
    vap_cfg->u.bss_info.interworking.interworking.esr = 
                pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iESR;
    vap_cfg->u.bss_info.interworking.interworking.uesa = 
                pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iUESA;
    vap_cfg->u.bss_info.interworking.interworking.venueOptionPresent = 
                pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iVenueOptionPresent;
    vap_cfg->u.bss_info.interworking.interworking.venueGroup =
               pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iVenueGroup; 
    vap_cfg->u.bss_info.interworking.interworking.venueType =
               pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iVenueType;
    vap_cfg->u.bss_info.interworking.interworking.hessOptionPresent = 
               pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iHESSOptionPresent;
    strncpy(vap_cfg->u.bss_info.interworking.interworking.hessid,
               pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iHESSID,
            sizeof(vap_cfg->u.bss_info.interworking.interworking.hessid)-1);
 
    vap_cfg->u.bss_info.interworking.roamingConsortium.wifiRoamingConsortiumCount = 
               pWifiAp->AP.Cfg.IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumCount;
    memcpy(&vap_cfg->u.bss_info.interworking.roamingConsortium.wifiRoamingConsortiumOui, 
     &pWifiAp->AP.Cfg.IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumOui, 
     sizeof(vap_cfg->u.bss_info.interworking.roamingConsortium.wifiRoamingConsortiumOui));
    memcpy(&vap_cfg->u.bss_info.interworking.roamingConsortium.wifiRoamingConsortiumLen, 
     &pWifiAp->AP.Cfg.IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumLen,
     sizeof(vap_cfg->u.bss_info.interworking.roamingConsortium.wifiRoamingConsortiumLen));

    vap_cfg->u.bss_info.interworking.passpoint.enable = 
       pWifiAp->AP.Cfg.IEEE80211uCfg.PasspointCfg.Status;
    vap_cfg->u.bss_info.interworking.passpoint.gafDisable =
       pWifiAp->AP.Cfg.IEEE80211uCfg.PasspointCfg.gafDisable;
    vap_cfg->u.bss_info.interworking.passpoint.p2pDisable =
       pWifiAp->AP.Cfg.IEEE80211uCfg.PasspointCfg.p2pDisable;
    vap_cfg->u.bss_info.interworking.passpoint.l2tif =
       pWifiAp->AP.Cfg.IEEE80211uCfg.PasspointCfg.l2tif;
  
    if (pWifiAp->AP.Cfg.IEEE80211uCfg.PasspointCfg.ANQPConfigParameters) {
        AnscCopyString((char *)vap_cfg->u.bss_info.interworking.anqp.anqpParameters, 
          pWifiAp->AP.Cfg.IEEE80211uCfg.PasspointCfg.ANQPConfigParameters);
    }
     
    if (pWifiAp->AP.Cfg.IEEE80211uCfg.PasspointCfg.HS2Parameters) {
        AnscCopyString((char *)vap_cfg->u.bss_info.interworking.passpoint.hs2Parameters,
          pWifiAp->AP.Cfg.IEEE80211uCfg.PasspointCfg.HS2Parameters);
    }
    gbsstrans_support[vap_index] = pWifiAp->AP.Cfg.BSSTransitionImplemented;
    gwirelessmgmt_support[vap_index] = pWifiAp->AP.Cfg.WirelessManagementImplemented;
 
    return RETURN_OK;
           
} 

int wifi_get_initial_common_config(wifi_config_t *curr_cfg) 
{
    PCOSA_DATAMODEL_WIFI pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_GASCFG  pGASconf = NULL;

    pGASconf = &pMyObject->GASCfg[0];

    if (pGASconf) {
        curr_cfg->gas_config.AdvertisementID = pGASconf->AdvertisementID;
        curr_cfg->gas_config.PauseForServerResponse = pGASconf->PauseForServerResponse;
        curr_cfg->gas_config.ResponseTimeout = pGASconf->ResponseTimeout;
        curr_cfg->gas_config.ComeBackDelay = pGASconf->ComeBackDelay;
        curr_cfg->gas_config.ResponseBufferingTime = pGASconf->ResponseBufferingTime;
        curr_cfg->gas_config.QueryResponseLengthLimit = pGASconf->QueryResponseLengthLimit;
        CcspTraceInfo(("%s: Fetched Initial GAS Configs\n",__FUNCTION__));
    }
    return RETURN_OK;
}

int wifi_update_dml_config(wifi_vap_info_t *vap_cfg, wifi_vap_info_t *curr_cfg, uint8_t vap_index)
{
    PCOSA_DATAMODEL_WIFI pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PSINGLE_LINK_ENTRY pSLinkEntry = NULL;
    PCOSA_DML_WIFI_SSID  pWifiSsid = NULL;
    PCOSA_DML_WIFI_AP      pWifiAp = NULL;
    char strValue[256];
    char recName[256];
    int   retPsmSet  = CCSP_SUCCESS;
 
    if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->SsidQueue, vap_index)) == NULL) {
        CcspTraceError(("%s Data Model object not found!\n",__FUNCTION__));
        return RETURN_ERR;
    }

    if((pWifiSsid = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) {
        CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
        return RETURN_ERR;
    }
    if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->AccessPointQueue, vap_index)) == NULL) {
        CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
        return RETURN_ERR;
    }

    if((pWifiAp = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) {
        CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
        return RETURN_ERR;
    }

    pWifiSsid->SSID.Cfg.bEnabled = vap_cfg->u.bss_info.enabled;
    
    strncpy(pWifiSsid->SSID.Cfg.SSID, vap_cfg->u.bss_info.ssid, sizeof(pWifiSsid->SSID.Cfg.SSID)-1);

    pWifiAp->AP.Cfg.SSIDAdvertisementEnabled = vap_cfg->u.bss_info.showSsid;

    if ((wifi_security_modes_t)pWifiAp->SEC.Cfg.ModeEnabled != curr_cfg->u.bss_info.security.mode) {
        pWifiAp->SEC.Cfg.ModeEnabled = vap_cfg->u.bss_info.security.mode;
    }
    if ((wifi_encryption_method_t)pWifiAp->SEC.Cfg.EncryptionMethod != curr_cfg->u.bss_info.security.encr) {
        pWifiAp->SEC.Cfg.EncryptionMethod = vap_cfg->u.bss_info.security.encr;
    }
    if (pWifiAp->AP.Cfg.ManagementFramePowerControl != (int)curr_cfg->u.bss_info.mgmtPowerControl) {
        pWifiAp->AP.Cfg.ManagementFramePowerControl = vap_cfg->u.bss_info.mgmtPowerControl;
    }

    if (pWifiAp->AP.Cfg.IsolationEnable != curr_cfg->u.bss_info.isolation) {
        pWifiAp->AP.Cfg.IsolationEnable = vap_cfg->u.bss_info.isolation;
        sprintf(recName, ApIsolationEnable, vap_index+1);
        sprintf(strValue,"%d",(vap_cfg->u.bss_info.isolation == TRUE) ? 1 : 0 );
        retPsmSet = PSM_Set_Record_Value2(bus_handle, g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            CcspTraceError(("%s Failed to set Isolation enable psm value\n",__FUNCTION__));
        }
    }

    if (pWifiAp->AP.Cfg.BssMaxNumSta != (int)curr_cfg->u.bss_info.bssMaxSta) {
        pWifiAp->AP.Cfg.BssMaxNumSta = vap_cfg->u.bss_info.bssMaxSta;
        sprintf(recName, BssMaxNumSta, vap_index+1);
        sprintf(strValue,"%d",pWifiAp->AP.Cfg.BssMaxNumSta);
        retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            CcspTraceError(("%s Failed to set Max station allowed psm value\n",__FUNCTION__));
        }           
    }

    if (pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_NeighborReportActivated != curr_cfg->u.bss_info.nbrReportActivated) {
        pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_NeighborReportActivated = vap_cfg->u.bss_info.nbrReportActivated;
        sprintf(strValue,"%s", (vap_cfg->u.bss_info.nbrReportActivated ? "true" : "false"));
        sprintf(recName, NeighborReportActivated, vap_index + 1 );
        retPsmSet = PSM_Set_Record_Value2(bus_handle, g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            CcspTraceError(("%s Failed to set Neighbor report activated psm value\n",__FUNCTION__));
        }
    }

    if (pWifiAp->AP.Cfg.BSSTransitionActivated != curr_cfg->u.bss_info.bssTransitionActivated) {
        pWifiAp->AP.Cfg.BSSTransitionActivated = vap_cfg->u.bss_info.bssTransitionActivated;
        sprintf(strValue,"%s", (vap_cfg->u.bss_info.bssTransitionActivated ? "true" : "false"));
        sprintf(recName, BSSTransitionActivated, vap_index + 1 );
        retPsmSet = PSM_Set_Record_Value2(bus_handle, g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            CcspTraceError(("%s Failed to set Bss Transition activated psm value\n",__FUNCTION__));
        }
    }

    if (pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_rapidReconnectCountEnable != curr_cfg->u.bss_info.rapidReconnectEnable) {
        pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_rapidReconnectCountEnable =
                                vap_cfg->u.bss_info.rapidReconnectEnable;
        sprintf(strValue,"%d", vap_cfg->u.bss_info.rapidReconnectEnable);
        sprintf(recName, RapidReconnCountEnable, vap_index+1);
        retPsmSet = PSM_Set_Record_Value2(bus_handle, g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            CcspTraceError(("%s Failed to set Rapid Reconnect Enable psm value\n",__FUNCTION__));
        }
    }

    if (pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_rapidReconnectMaxTime != (int)curr_cfg->u.bss_info.rapidReconnThreshold) {
        pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_rapidReconnectMaxTime = vap_cfg->u.bss_info.rapidReconnThreshold;
        sprintf(strValue,"%d", vap_cfg->u.bss_info.rapidReconnThreshold);
        sprintf(recName, RapidReconnThreshold, vap_index+1);
        retPsmSet = PSM_Set_Record_Value2(bus_handle, g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            CcspTraceError(("%s Failed to set Rapid Reconnection threshold psm value\n",__FUNCTION__));
        }
    }

    if (pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_StatsEnable != curr_cfg->u.bss_info.vapStatsEnable) {
        pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_StatsEnable = vap_cfg->u.bss_info.vapStatsEnable;
        sprintf(recName, vAPStatsEnable, vap_index+1);
        sprintf(strValue,"%s", (vap_cfg->u.bss_info.vapStatsEnable ? "true" : "false"));
        retPsmSet = PSM_Set_Record_Value2(bus_handle, g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            CcspTraceError(("%s Failed to set Vap stats enable psm value\n",__FUNCTION__));
        }
    }

    if(strcmp(pWifiAp->SEC.Cfg.MFPConfig, curr_cfg->u.bss_info.security.mfpConfig) != 0) {
        strncpy(pWifiAp->SEC.Cfg.MFPConfig,vap_cfg->u.bss_info.security.mfpConfig, sizeof(vap_cfg->u.bss_info.security.mfpConfig)-1);
        sprintf(recName, ApMFPConfig, vap_index+1);
        retPsmSet = PSM_Set_Record_Value2(bus_handle, g_Subsystem, recName, ccsp_string, vap_cfg->u.bss_info.security.mfpConfig);
        if (retPsmSet != CCSP_SUCCESS) {
            CcspTraceError(("%s Failed to set MFPConfig  psm value\n",__FUNCTION__));
        }
    }

    if ((strcmp(vap_cfg->vap_name,"hotspot_secure_2g") == 0 || strcmp(vap_cfg->vap_name,"hotspot_secure_5g") == 0)) {
        strncpy((char *)pWifiAp->SEC.Cfg.RadiusServerIPAddr, (char *)vap_cfg->u.bss_info.security.u.radius.ip,
                sizeof(pWifiAp->SEC.Cfg.RadiusServerIPAddr)-1);
        pWifiAp->SEC.Cfg.RadiusServerPort = vap_cfg->u.bss_info.security.u.radius.port;
        strncpy(pWifiAp->SEC.Cfg.RadiusSecret,vap_cfg->u.bss_info.security.u.radius.key,
                sizeof(pWifiAp->SEC.Cfg.RadiusSecret)-1);
        strncpy((char *)pWifiAp->SEC.Cfg.SecondaryRadiusServerIPAddr,(char *)vap_cfg->u.bss_info.security.u.radius.s_ip,
                sizeof(pWifiAp->SEC.Cfg.SecondaryRadiusServerIPAddr)-1);
        pWifiAp->SEC.Cfg.SecondaryRadiusServerPort = vap_cfg->u.bss_info.security.u.radius.s_port;
        strncpy(pWifiAp->SEC.Cfg.SecondaryRadiusSecret,vap_cfg->u.bss_info.security.u.radius.s_key,
                sizeof(pWifiAp->SEC.Cfg.SecondaryRadiusSecret)-1);
    } else if(vap_cfg->u.bss_info.security.mode != wifi_security_mode_none) {
        strncpy((char *)pWifiAp->SEC.Cfg.KeyPassphrase, vap_cfg->u.bss_info.security.u.key.key,
                sizeof(pWifiAp->SEC.Cfg.KeyPassphrase)-1);
    }

    memcpy(&sWiFiDmlSsidStoredCfg[pWifiSsid->SSID.Cfg.InstanceNumber-1], &pWifiSsid->SSID.Cfg, sizeof(COSA_DML_WIFI_SSID_CFG));
    memcpy(&sWiFiDmlApStoredCfg[pWifiAp->AP.Cfg.InstanceNumber-1].Cfg, &pWifiAp->AP.Cfg, sizeof(COSA_DML_WIFI_AP_CFG));
    memcpy(&sWiFiDmlSsidRunningCfg[pWifiSsid->SSID.Cfg.InstanceNumber-1], &pWifiSsid->SSID.Cfg, sizeof(COSA_DML_WIFI_SSID_CFG));
    memcpy(&sWiFiDmlApRunningCfg[pWifiAp->AP.Cfg.InstanceNumber-1].Cfg, &pWifiAp->AP.Cfg, sizeof(COSA_DML_WIFI_AP_CFG));
    memcpy(&sWiFiDmlApSecurityStored[pWifiAp->AP.Cfg.InstanceNumber-1].Cfg, &pWifiAp->SEC.Cfg, sizeof(COSA_DML_WIFI_APSEC_CFG));
    memcpy(&sWiFiDmlApSecurityRunning[pWifiAp->AP.Cfg.InstanceNumber-1].Cfg, &pWifiAp->SEC.Cfg, sizeof(COSA_DML_WIFI_APSEC_CFG));

    //Update Interworking Configuration
    pWifiAp->AP.Cfg.InterworkingEnable =
        curr_cfg->u.bss_info.interworking.interworking.interworkingEnabled;
    pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iAccessNetworkType =
        curr_cfg->u.bss_info.interworking.interworking.accessNetworkType;
    pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iInternetAvailable =
        curr_cfg->u.bss_info.interworking.interworking.internetAvailable =
    pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iASRA = 
        curr_cfg->u.bss_info.interworking.interworking.asra;
    pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iESR =
        curr_cfg->u.bss_info.interworking.interworking.esr;
    pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iUESA =
        curr_cfg->u.bss_info.interworking.interworking.uesa;
    pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iVenueOptionPresent =
        curr_cfg->u.bss_info.interworking.interworking.venueOptionPresent;
    pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iVenueGroup =
        curr_cfg->u.bss_info.interworking.interworking.venueGroup;
    pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iVenueType =
        curr_cfg->u.bss_info.interworking.interworking.venueType;
    pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iHESSOptionPresent =
        curr_cfg->u.bss_info.interworking.interworking.hessOptionPresent;
    strncpy(pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iHESSID,
        curr_cfg->u.bss_info.interworking.interworking.hessid,
        sizeof(pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iHESSID)-1);

#if defined (FEATURE_SUPPORT_PASSPOINT) &&  defined(ENABLE_FEATURE_MESHWIFI)
    //Save Interworking Config to DB
    update_ovsdb_interworking(vap_cfg->vap_name,&curr_cfg->u.bss_info.interworking.interworking);
#else 
    if(CosaDmlWiFi_WriteInterworkingConfig(&pWifiAp->AP.Cfg) != ANSC_STATUS_SUCCESS) {
        CcspTraceWarning(("Failed to Save Interworking Configuration\n"));
    }
#endif    
    pWifiAp->AP.Cfg.IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumCount =
        curr_cfg->u.bss_info.interworking.roamingConsortium.wifiRoamingConsortiumCount;
    memcpy(&pWifiAp->AP.Cfg.IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumOui,
        &curr_cfg->u.bss_info.interworking.roamingConsortium.wifiRoamingConsortiumOui,
       sizeof(pWifiAp->AP.Cfg.IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumOui));
    memcpy(&pWifiAp->AP.Cfg.IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumLen,
        &curr_cfg->u.bss_info.interworking.roamingConsortium.wifiRoamingConsortiumLen,
       sizeof(pWifiAp->AP.Cfg.IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumLen));

    //Save Interworking, ANQP, and Passpoint Config 
    if((int)ANSC_STATUS_FAILURE == CosaDmlWiFi_SaveInterworkingWebconfig(&pWifiAp->AP.Cfg, &curr_cfg->u.bss_info.interworking, vap_index)) {
        CcspTraceWarning(("Failed to Save ANQP Configuration\n"));
    }

    return RETURN_OK;     
}

int wifi_update_common_config(wifi_config_t *wifi_cfg)
{
    PCOSA_DATAMODEL_WIFI pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_GASCFG  pGASconf = NULL;

    pGASconf = &pMyObject->GASCfg[0];

    if (pGASconf) {
        pGASconf->AdvertisementID = wifi_cfg->gas_config.AdvertisementID;
        pGASconf->PauseForServerResponse = wifi_cfg->gas_config.PauseForServerResponse;
        pGASconf->ResponseTimeout = wifi_cfg->gas_config.ResponseTimeout;
        pGASconf->ComeBackDelay = wifi_cfg->gas_config.ComeBackDelay;
        pGASconf->ResponseBufferingTime = wifi_cfg->gas_config.ResponseBufferingTime;
        pGASconf->QueryResponseLengthLimit = wifi_cfg->gas_config.QueryResponseLengthLimit;
          
        CcspTraceInfo(("%s: Copied  GAS Configs to TR-181\n",__FUNCTION__));
    }
#if defined (FEATURE_SUPPORT_PASSPOINT) && defined(ENABLE_FEATURE_MESHWIFI) 
    //Update OVSDB
    if(RETURN_OK != update_ovsdb_gas_config(wifi_cfg->gas_config.AdvertisementID, &wifi_cfg->gas_config))
    {
        CcspTraceWarning(("Failed to update OVSDB with GAS Config\n"));
    }
#else 
    update_json_gas_config(&wifi_cfg->gas_config);
#endif
    return RETURN_OK;
}

int wifi_vap_cfg_rollback_handler() 
{
    wifi_vap_info_map_t current_cfg;
    uint8_t i;
    char *err = NULL;

/* TBD: Handle Wifirestart during rollback */

    CcspTraceInfo(("Inside %s\n",__FUNCTION__));
    for (i=0; i < vap_curr_cfg.num_vaps; i++) {
        if (RETURN_OK != wifi_get_initial_vap_config(&current_cfg.vap_array[i],
                     vap_curr_cfg.vap_array[i].vap_index)) {
            CcspTraceError(("%s: Failed to fetch initial dml config\n",__FUNCTION__));
            return RETURN_ERR;
        }

        current_cfg.vap_array[i].vap_index = vap_curr_cfg.vap_array[i].vap_index;
        strncpy(current_cfg.vap_array[i].vap_name, vap_curr_cfg.vap_array[i].vap_name,
                sizeof(current_cfg.vap_array[i].vap_name) - 1);
        err = wifi_apply_ssid_config(&current_cfg.vap_array[i], &vap_curr_cfg.vap_array[i],FALSE,FALSE);
        if (err != NULL) {
            CcspTraceError(("%s: Failed to apply ssid config for index %d\n",err,
                                              current_cfg.vap_array[i].vap_index));
            return RETURN_ERR;
        }

        err = wifi_apply_security_config(&current_cfg.vap_array[i], &vap_curr_cfg.vap_array[i],FALSE);
        if (err != NULL) {
            CcspTraceError(("%s: Failed to apply security config for index %d\n", err,
                                              current_cfg.vap_array[i].vap_index));
            return RETURN_ERR;
        }
        err = wifi_apply_interworking_config(&current_cfg.vap_array[i], &vap_curr_cfg.vap_array[i]);
        if (err != NULL) {
            CcspTraceError(("%s: Failed to apply interworking config for index %d\n",err,
                                              current_cfg.vap_array[i].vap_index));
            return RETURN_ERR;
        }
    }
    if (wifi_apply_radio_settings() != NULL) {
        CcspTraceError(("%s: Failed to Apply Radio settings\n", __FUNCTION__));
        return RETURN_ERR;
    }
    CcspTraceInfo(("%s: Rollback applied successfully\n",__FUNCTION__));
    return RETURN_OK;
}

int wifi_update_captiveportal (char *ssid, char *password, char *vap_name) {
    parameterSigStruct_t       val = {0};
    char param_name[64] = {0};
    bool *ssid_updated, *pwd_updated;
    uint8_t wlan_index;

    if ( strcmp(notifyWiFiChangesVal,"true") != 0 ) {
        return RETURN_OK;
    }

    if (strcmp(vap_name,"private_ssid_2g") == 0) {
        ssid_updated = &SSID1_UPDATED;
        pwd_updated = &PASSPHRASE1_UPDATED;
        wlan_index  = 1;
    } else {
        ssid_updated = &SSID2_UPDATED;
        pwd_updated = &PASSPHRASE2_UPDATED;
        wlan_index = 2; 
    }

    if (*ssid_updated) {
        sprintf(param_name, "Device.WiFi.SSID.%d.SSID",wlan_index);
        val.parameterName = param_name;
        val.newValue = ssid;
        WiFiPramValueChangedCB(&val, 0, NULL);
        *ssid_updated = FALSE;
    } 

    if (*pwd_updated) {
        sprintf(param_name, "Device.WiFi.AccessPoint.%d.Security.X_COMCAST-COM_KeyPassphrase",wlan_index);
        val.parameterName = param_name;
        val.newValue = password;
        WiFiPramValueChangedCB(&val, 0, NULL);
        *pwd_updated = FALSE;
    }
    return RETURN_OK;
}

size_t wifi_vap_cfg_timeout_handler()
{
#if defined(_XB6_PRODUCT_REQ_) && !defined (_XB7_PRODUCT_REQ_)
    return (2 * XB6_DEFAULT_TIMEOUT);
#else
    return (2 * SSID_DEFAULT_TIMEOUT);
#endif
}

void Tunnel_event_callback(char *info, void *data)
{
    /*PCOSA_DATAMODEL_WIFI pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PSINGLE_LINK_ENTRY pSLinkEntry = NULL;
    PCOSA_DML_WIFI_SSID  pWifiSsid = NULL;
    char *err = NULL;
    int i;*/
    wifi_vap_info_t cur_cfg, new_cfg;

    memset(&cur_cfg,0,sizeof(cur_cfg));
    memset(&new_cfg,0,sizeof(new_cfg));
    CcspTraceInfo(("%s : Tunnel status received %s Data %s\n",__FUNCTION__,info, (char *) data));
   
    /*Tunnel related vap up/down on hold until RDKB-35640 is fixed */ 
    /*if (strcmp(info, "TUNNEL_DOWN") == 0) {
        CcspTraceInfo(("%s Received tunnel down event\n",__FUNCTION__));
        for (i = 0; i < 4; i++) {
                cur_cfg.u.bss_info.enabled = TRUE;
                new_cfg.u.bss_info.enabled = FALSE;
                new_cfg.vap_index =  hotspot_vaps[i];
                if (new_cfg.vap_index <= 5) {
                    cur_cfg.u.bss_info.security.mode = wifi_security_mode_none;
                } else {
                    cur_cfg.u.bss_info.security.mode = wifi_security_mode_wpa2_enterprise;
                } 
                err = wifi_apply_ssid_config(&new_cfg, &cur_cfg, TRUE);
                if (err != NULL) {
                    CcspTraceError(("%s Error :%s",__FUNCTION__,err));
                    continue;
                }
            }
    } else if (strcmp(info, "TUNNEL_UP") == 0) {
        CcspTraceInfo(("%s Received tunnel up event\n",__FUNCTION__));
        for (i = 0; i < 4; i++) {
            if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->SsidQueue, hotspot_vaps[i])) == NULL) {
                CcspTraceError(("%s Data Model object not found!\n",__FUNCTION__));
                return RETURN_ERR;
            }

            if((pWifiSsid = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) {
                CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
                return RETURN_ERR;
            }             
            if (pWifiSsid->SSID.Cfg.bEnabled == FALSE) {
                CcspTraceInfo(("%s Vap %d is down \n",__FUNCTION__,hotspot_vaps[i]));
                continue;
            } else {
                cur_cfg.u.bss_info.enabled = FALSE;
                new_cfg.u.bss_info.enabled = TRUE;
                new_cfg.vap_index =  hotspot_vaps[i];
                if (new_cfg.vap_index <= 5) {
                    cur_cfg.u.bss_info.security.mode = wifi_security_mode_none;
                } else {
                    cur_cfg.u.bss_info.security.mode = wifi_security_mode_wpa2_enterprise;
                }
                strncpy(new_cfg.u.bss_info.ssid, pWifiSsid->SSID.Cfg.SSID,sizeof(new_cfg.u.bss_info.ssid)-1);
                err = wifi_apply_ssid_config(&new_cfg, &cur_cfg, TRUE);
                if (err != NULL) {
                    CcspTraceError(("%s Error :%s",__FUNCTION__,err));
                    continue;
                }
            }
        }
    } else {
        CcspTraceInfo(("%s Unsupported event\n",__FUNCTION__));
    }*/
    return;
}

/**
 * Function to Parse Msg packed Wifi Config
 * 
 * @param buf Pointer to the decoded string
 * @param size       Size of the Decoded Message
 *
 *  returns 0 on success, error otherwise
 */

int wifi_vapConfigSet(const char *buf, size_t len, pErr execRetVal) 
{
#define MAX_JSON_BUFSIZE 10240
    size_t  json_len = 0;
    msgpack_zone msg_z;
    msgpack_object msg_obj;
    msgpack_unpack_return mp_rv = 0;
    wifi_vap_info_map_t vap_map;
    wifi_config_t wifi;
    char *buffer = NULL;
    char *err = NULL;
    int i, retval;
    char *strValue = NULL;
    int   retPsmGet  = CCSP_SUCCESS;
    BOOL vap_enabled = FALSE;

    if (!buf || !execRetVal) {
        CcspTraceError(("%s: Empty input parameters for subdoc set\n",__FUNCTION__));
        if (execRetVal) {
            execRetVal->ErrorCode = VALIDATION_FALIED;
            strncpy(execRetVal->ErrorMsg, "Empty subdoc", sizeof(execRetVal->ErrorMsg)-1);
        }
        return RETURN_ERR;
    }

    msgpack_zone_init(&msg_z, MAX_JSON_BUFSIZE);
    if(MSGPACK_UNPACK_SUCCESS != msgpack_unpack(buf, len, NULL, &msg_z, &msg_obj)) {
        CcspTraceError(("%s: Failed to unpack wifi msg blob. Error %d\n",__FUNCTION__,mp_rv));
        if (execRetVal) {
            execRetVal->ErrorCode = VALIDATION_FALIED;
            strncpy(execRetVal->ErrorMsg, "Msg unpack failed", sizeof(execRetVal->ErrorMsg)-1);
        }
        msgpack_zone_destroy(&msg_z);
        return RETURN_ERR;
    }

    CcspTraceInfo(("%s:Msg unpack success.\n", __FUNCTION__));

    buffer = (char*) malloc (MAX_JSON_BUFSIZE);
    if (!buffer) {
        CcspTraceError(("%s: Failed to allocate memory\n",__FUNCTION__));
        strncpy(execRetVal->ErrorMsg, "Failed to allocate memory", sizeof(execRetVal->ErrorMsg)-1);
        execRetVal->ErrorCode = VALIDATION_FALIED;
        msgpack_zone_destroy(&msg_z);
        return RETURN_ERR;
    } 
    
    memset(buffer,0,MAX_JSON_BUFSIZE);
    json_len = msgpack_object_print_jsonstr(buffer, MAX_JSON_BUFSIZE, msg_obj);
    if (json_len <= 0) {
        CcspTraceError(("%s: Msgpack to json conversion failed\n",__FUNCTION__));
        if (execRetVal) {
            execRetVal->ErrorCode = VALIDATION_FALIED;
            strncpy(execRetVal->ErrorMsg, "Msgpack to json conversion failed", sizeof(execRetVal->ErrorMsg)-1);
        }
        free(buffer);
        msgpack_zone_destroy(&msg_z);
        return RETURN_ERR;
    }

    buffer[json_len] = '\0';
    msgpack_zone_destroy(&msg_z);
    CcspTraceInfo(("%s:Msgpack to JSON success.\n", __FUNCTION__));

    //Fetch RFC values for Interworking and Passpoint
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.WiFi-Interworking.Enable", NULL, &strValue);
    if ((retPsmGet == CCSP_SUCCESS) && (strValue)){
        g_interworking_RFC = _ansc_atoi(strValue);
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.WiFi-Passpoint.Enable", NULL, &strValue);
    if ((retPsmGet == CCSP_SUCCESS) && (strValue)){
        g_passpoint_RFC = _ansc_atoi(strValue);
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(&vap_map, 0, sizeof(vap_map));
    memset(&wifi, 0, sizeof(wifi));
    if (wifi_validate_config(buffer, &wifi, &vap_map, execRetVal) != RETURN_OK) {
        CcspTraceError(("%s: Failed to fetch and validate vaps from json. ErrorMsg: %s\n", __FUNCTION__,execRetVal->ErrorMsg));
        execRetVal->ErrorCode = VALIDATION_FALIED;
        free(buffer);
        return RETURN_ERR;
    }

    free(buffer);

    memset(&vap_curr_cfg, 0, sizeof(vap_curr_cfg));
    memset(&wifi_cfg,0,sizeof(wifi_cfg));
    vap_curr_cfg.num_vaps = 0;

    if (wifi_get_initial_common_config(&wifi_cfg) != RETURN_OK) {
        CcspTraceError(("%s: Failed to retrieve common wifi config\n",__FUNCTION__));
        strncpy(execRetVal->ErrorMsg, "Failed to get WiFi Config",sizeof(execRetVal->ErrorMsg)-1);
        execRetVal->ErrorCode = VALIDATION_FALIED;
        return RETURN_ERR;
    }
 
    for (i = 0; i < (int)vap_map.num_vaps; i++) {
        vap_curr_cfg.vap_array[i].vap_index = vap_map.vap_array[i].vap_index;
        strncpy(vap_curr_cfg.vap_array[i].vap_name, vap_map.vap_array[i].vap_name,
                sizeof(vap_curr_cfg.vap_array[i].vap_name)-1);

        /* Get Initial dml config */
        if (RETURN_OK != wifi_get_initial_vap_config(&vap_curr_cfg.vap_array[i], 
                     vap_map.vap_array[i].vap_index)) {
            CcspTraceError(("%s: Failed to fetch initial dml config\n",__FUNCTION__));
            strncpy(execRetVal->ErrorMsg,"Fetching dml config failed",sizeof(execRetVal->ErrorMsg)-1);
            execRetVal->ErrorCode = VALIDATION_FALIED; 
            return RETURN_ERR;
        }
        vap_curr_cfg.num_vaps++;

        vap_enabled = FALSE;
        if (vap_curr_cfg.vap_array[i].u.bss_info.enabled == FALSE &&
            vap_map.vap_array[i].u.bss_info.enabled == TRUE) {
            vap_enabled = TRUE;
        }
        /* Apply ssid parameters to hal */
        err = wifi_apply_ssid_config(&vap_map.vap_array[i], &vap_curr_cfg.vap_array[i], FALSE,vap_enabled);
        if (err != NULL) {
            CcspTraceError(("%s: Failed to apply ssid config for index %d\n", err,
                                              vap_map.vap_array[i].vap_index));
            strncpy(execRetVal->ErrorMsg, err, sizeof(execRetVal->ErrorMsg)-1);
            execRetVal->ErrorCode = WIFI_HAL_FAILURE;
            return RETURN_ERR;
        }
             
        /* Apply security parameters to hal */
        err = wifi_apply_security_config(&vap_map.vap_array[i], &vap_curr_cfg.vap_array[i],vap_enabled);
        if (err != NULL) {
            CcspTraceError(("%s: Failed to apply security config for index %d\n", err,
                                              vap_map.vap_array[i].vap_index));
            strncpy(execRetVal->ErrorMsg, err, sizeof(execRetVal->ErrorMsg)-1);
            execRetVal->ErrorCode = WIFI_HAL_FAILURE;
            return RETURN_ERR;
        }
         
    }


    err = wifi_apply_radio_settings();
    if (err != NULL) {
        CcspTraceError(("%s: Failed to Apply Radio settings\n", __FUNCTION__));
        strncpy(execRetVal->ErrorMsg,err,sizeof(execRetVal->ErrorMsg)-1);
        execRetVal->ErrorCode = WIFI_HAL_FAILURE;
        return RETURN_ERR;
    }

    
   
    for (i = 0; i < (int)vap_map.num_vaps; i++) {
#ifdef CISCO_XB3_PLATFORM_CHANGES
        if((strcmp(vap_map.vap_array[i].vap_name,"hotspot_open_5g") == 0) ||
           (strcmp(vap_map.vap_array[i].vap_name,"hotspot_open_2g") == 0)) {
            if (vap_map.vap_array[i].u.bss_info.enabled) {
                vap_curr_cfg.vap_array[i].u.bss_info.enabled = FALSE;
                err = wifi_apply_ssid_config(&vap_map.vap_array[i],&vap_curr_cfg.vap_array[i],TRUE,FALSE);
            }
        }
#endif
        err = wifi_apply_interworking_config(&vap_map.vap_array[i], &vap_curr_cfg.vap_array[i]);
        if (err != NULL) {
            CcspTraceError(("%s: Failed to apply interworking config for index %d\n", err,
                                              vap_map.vap_array[i].vap_index));
            strncpy(execRetVal->ErrorMsg, err, sizeof(execRetVal->ErrorMsg)-1);
            execRetVal->ErrorCode = WIFI_HAL_FAILURE;
            return RETURN_ERR;
        }
    }

    err = wifi_apply_common_config(&wifi,&wifi_cfg);
    if (err != NULL) {
        CcspTraceError(("%s: Failed to apply common WiFi config\n",__FUNCTION__));
        strncpy(execRetVal->ErrorMsg,err,sizeof(execRetVal->ErrorMsg)-1);
        execRetVal->ErrorCode = WIFI_HAL_FAILURE;
    }

    if (gHostapd_restart_reqd) {
        err = wifi_apply_radio_settings();
        if (err != NULL) {
            CcspTraceError(("%s: Failed to Apply Radio settings\n", __FUNCTION__));
            strncpy(execRetVal->ErrorMsg,err,sizeof(execRetVal->ErrorMsg)-1);
            execRetVal->ErrorCode = WIFI_HAL_FAILURE;
            return RETURN_ERR;
        }
    }

    for (i = 0; i < (int)vap_map.num_vaps; i++) {
        /* Update TR-181 params */
        retval = wifi_update_dml_config(&vap_map.vap_array[i], &vap_curr_cfg.vap_array[i],
                                         vap_map.vap_array[i].vap_index);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to update Tr-181 params\n", __FUNCTION__));
            strncpy(execRetVal->ErrorMsg,"Failed to update TR-181 params",
                    sizeof(execRetVal->ErrorMsg)-1); 
            return RETURN_ERR;
        }
        if (strcmp(vap_map.vap_array[i].vap_name, "private_ssid_2g") == 0 ||
            strcmp(vap_map.vap_array[i].vap_name, "private_ssid_5g") == 0)
        {
            /* Update captive portal */
            retval = wifi_update_captiveportal(vap_map.vap_array[i].u.bss_info.ssid, 
                          vap_map.vap_array[i].u.bss_info.security.u.key.key,
                          vap_map.vap_array[i].vap_name);
            if (retval != RETURN_OK) {
                CcspTraceError(("%s: Failed to update captive portal\n", __FUNCTION__));
                strncpy(execRetVal->ErrorMsg,"Failed to update captive portal settings",
                    sizeof(execRetVal->ErrorMsg)-1);
            }
        } 
    }

    if (wifi_update_common_config(&wifi) != RETURN_OK) {
        CcspTraceError(("%s: Failed to update TR-181 common config\n",__FUNCTION__));
        strncpy(execRetVal->ErrorMsg,"Failed to update TR-181 common config",
                sizeof(execRetVal->ErrorMsg)-1);
        return RETURN_ERR;
    }

    execRetVal->ErrorCode = BLOB_EXEC_SUCCESS;
    return RETURN_OK;
}


pErr wifi_multicomp_subdoc_handler(void *data)
{
    unsigned char *msg = NULL; 
    unsigned long msg_size = 0;
    pErr execRetVal = NULL;

    if (data == NULL) {
        CcspTraceError(("%s: Empty multi component subdoc\n",__FUNCTION__));
        return execRetVal;
    }
    msg = AnscBase64Decode((unsigned char *)data, &msg_size);
    
    if (!msg) {
        CcspTraceError(("%s: Failed in Decoding multicomp blob\n",__FUNCTION__));
        return execRetVal;
    }

    execRetVal = (pErr ) malloc (sizeof(Err));
    if (execRetVal == NULL ) {
        free(msg);
        CcspTraceError(("%s : Failed in allocating memory for error struct\n",__FUNCTION__));
        return execRetVal;
    }
    memset(execRetVal,0,(sizeof(Err)));

    if (wifi_vapConfigSet((char *)msg, msg_size, execRetVal) == RETURN_OK) {
        CcspTraceInfo(("%s:Successfully applied tbe subdoc\n",__FUNCTION__));
    } else {
        CcspTraceError(("%s : Failed to apply the subdoc\n", __FUNCTION__));
    }
    if (msg) {
        free(msg);
    }
    return execRetVal;     
} 

pErr wifi_vap_cfg_exec_handler(void *data)
{
    pErr execRetVal = NULL;

    if (data == NULL) {
        CcspTraceError(("%s: Input Data is NULL\n",__FUNCTION__));
        return execRetVal;
    }

    wifi_vap_blob_data_t *vap_msg = (wifi_vap_blob_data_t *) data;
 
    execRetVal = (pErr ) malloc (sizeof(Err));
    if (execRetVal == NULL ) {
        CcspTraceError(("%s : Failed in allocating memory for error struct\n",__FUNCTION__));
        return execRetVal;
    }
    memset(execRetVal,0,(sizeof(Err)));
    
    if (wifi_vapConfigSet((const char *)vap_msg->data,vap_msg->msg_size,execRetVal) == RETURN_OK) {
        CcspTraceInfo(("%s : Vap config set success\n",__FUNCTION__));
    } else {
        CcspTraceError(("%s : Vap config set Failed\n",__FUNCTION__));
    }

    return execRetVal;
}

void wifi_vap_cfg_free_resources(void *arg)
{

    CcspTraceInfo(("Entering: %s\n",__FUNCTION__));
    if (arg == NULL) {
        CcspTraceError(("%s: Input Data is NULL\n",__FUNCTION__));
        return;
    }
    
    execData *blob_exec_data  = (execData*) arg;

    wifi_vap_blob_data_t *vap_Data = (wifi_vap_blob_data_t *) blob_exec_data->user_data;
      
    if (vap_Data && vap_Data->data) {
        free(vap_Data->data);
        vap_Data->data = NULL;
    }
  
    if (vap_Data) {   
        free(vap_Data);
        vap_Data = NULL; 
    }

    free(blob_exec_data);
    blob_exec_data = NULL;
    CcspTraceInfo(("%s:Success in Clearing wifi vapconfig resources\n",__FUNCTION__));
}

int wifi_vapBlobSet(void *data)
{
    char *decoded_data = NULL; 
    unsigned long msg_size = 0;
    size_t offset = 0;
    msgpack_unpacked msg;
    msgpack_unpack_return mp_rv;
    msgpack_object_map *map = NULL;
    msgpack_object_kv* map_ptr  = NULL;
    wifi_vap_blob_data_t *vap_data = NULL;
    int i = 0;

    if (data == NULL) {
        CcspTraceError(("%s: Empty Blob Input\n",__FUNCTION__));
        return RETURN_ERR;
    }

    decoded_data = (char *)AnscBase64Decode((unsigned char *)data, &msg_size);

    if (!decoded_data) {
        CcspTraceError(("%s: Failed in Decoding vapconfig blob\n",__FUNCTION__));
        return RETURN_ERR;
    }

    msgpack_unpacked_init( &msg );
    /* The outermost wrapper MUST be a map. */
    mp_rv = msgpack_unpack_next( &msg, (const char*) decoded_data, msg_size+1, &offset );
    if (mp_rv != MSGPACK_UNPACK_SUCCESS) {
        CcspTraceError(("%s: Failed to unpack wifi msg blob. Error %d",__FUNCTION__,mp_rv));
        msgpack_unpacked_destroy( &msg );
        free(decoded_data);
        return RETURN_ERR;
    }

    CcspTraceInfo(("%s:Msg unpack success. Offset is %lu\n", __FUNCTION__,offset));
    msgpack_object obj = msg.data;
    
    map = &msg.data.via.map;
    
    map_ptr = obj.via.map.ptr;
    if ((!map) || (!map_ptr)) {
        CcspTraceError(("Failed to get object map\n"));
        msgpack_unpacked_destroy( &msg );
        free(decoded_data);
        return RETURN_ERR;
    }
    if (msg.data.type != MSGPACK_OBJECT_MAP) {
        CcspTraceError(("%s: Invalid msgpack type",__FUNCTION__));
        msgpack_unpacked_destroy( &msg );
        free(decoded_data);
        return RETURN_ERR;
    }    
   
    vap_data = (wifi_vap_blob_data_t *) malloc(sizeof(wifi_vap_blob_data_t));
    if (vap_data == NULL) {
        CcspTraceError(("%s: Wifi vap data malloc error\n",__FUNCTION__));
        free(decoded_data); 
        return RETURN_ERR;
    }
 
    /* Parsing Config Msg String to Wifi Structure */
    for (i = 0;i < (int)map->size;i++) {
        if (strncmp(map_ptr->key.via.str.ptr, "version", map_ptr->key.via.str.size) == 0) {
            if (map_ptr->val.type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
                vap_data->version = (uint64_t) map_ptr->val.via.u64;
                CcspTraceInfo(("Version type %d version %lu\n",map_ptr->val.type,vap_data->version));
            }
        }
        else if (strncmp(map_ptr->key.via.str.ptr, "transaction_id", map_ptr->key.via.str.size) == 0) {
            if (map_ptr->val.type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
                vap_data->transaction_id = (uint16_t) map_ptr->val.via.u64;
                CcspTraceInfo(("Tx id type %d tx id %d\n",map_ptr->val.type,vap_data->transaction_id));
            }
        }
        ++map_ptr;
    }

    msgpack_unpacked_destroy( &msg );
    
    vap_data->msg_size = msg_size;
    vap_data->data = decoded_data;

    execData *execDataPf = NULL ;
    execDataPf = (execData*) malloc (sizeof(execData));
    if (execDataPf != NULL) {
        memset(execDataPf, 0, sizeof(execData));
        execDataPf->txid = vap_data->transaction_id;
        execDataPf->version = vap_data->version;
        execDataPf->numOfEntries = 2;
        strncpy(execDataPf->subdoc_name, "wifiVapData", sizeof(execDataPf->subdoc_name)-1);
        execDataPf->user_data = (void*) vap_data;
        execDataPf->calcTimeout = webconf_ssid_timeout_handler;
        execDataPf->executeBlobRequest = wifi_vap_cfg_exec_handler;
        execDataPf->rollbackFunc = wifi_vap_cfg_rollback_handler;
        execDataPf->freeResources = wifi_vap_cfg_free_resources;
        PushBlobRequest(execDataPf);
        CcspTraceInfo(("PushBlobRequest Complete\n"));
    }

    return RETURN_OK;
}
    
/**
 * API to get Blob version from PSM db
 *
 * @param subdoc Pointer to name of the subdoc
 *
 * returns version number if present, 0 otherwise
 */
uint32_t getWiFiBlobVersion(char* subdoc)
{
    char *subdoc_ver = NULL;
    char  buf[72] = {0};
    int retval;
    uint32_t version = 0;

    snprintf(buf,sizeof(buf), WiFiSsidVersion, subdoc);

    retval = PSM_Get_Record_Value2(bus_handle,g_Subsystem, buf, NULL, &subdoc_ver);
    if ((retval == CCSP_SUCCESS) && (subdoc_ver))
    {
        version = strtoul(subdoc_ver, NULL, 10);
        CcspTraceInfo(("%s: Wifi %s blob version %s\n",__FUNCTION__, subdoc,subdoc_ver));
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(subdoc_ver);
        return (uint32_t)version;
    }
    return 0;
}

/**
 * API to set Blob version in PSM db
 *
 * @param subdoc  Pointer to name of the subdoc
 * @param version Version of the blob
 *
 * returns 0 on success, error otherwise
 */
int setWiFiBlobVersion(char* subdoc,uint32_t version)
{
    char subdoc_ver[32] = {0}, buf[72] = {0};
    int retval;

    snprintf(subdoc_ver,sizeof(subdoc_ver),"%u",version);
    snprintf(buf,sizeof(buf), WiFiSsidVersion,subdoc);

    retval = PSM_Set_Record_Value2(bus_handle,g_Subsystem, buf, ccsp_string, subdoc_ver);
    if (retval == CCSP_SUCCESS) {
        CcspTraceInfo(("%s: Wifi Blob version applied to PSM DB Successfully\n", __FUNCTION__));
        return RETURN_OK;
    } else {
        CcspTraceError(("%s: Failed to apply blob version to PSM DB\n", __FUNCTION__));
        return RETURN_ERR;
    } 
}

/**
 *  API to register Multicomponent supported subdocs with framework
 *  
 *  returns 0 on success, error otherwise
 *
 */
int register_multicomp_subdocs()
{
    multiCompSubDocReg *subdoc_data = NULL;
    char *subdocs[MULTISUBDOC_COUNT+1]= {"hotspot", (char *) 0 };
    uint8_t i;
    int ret;

    subdoc_data = (multiCompSubDocReg *) malloc(MULTISUBDOC_COUNT * sizeof(multiCompSubDocReg));
    if (subdoc_data == NULL) {
        CcspTraceError(("%s: Failed to alloc memory for registering multisubdocs\n",__FUNCTION__));
        return RETURN_ERR;
    }
    memset(subdoc_data, 0 , MULTISUBDOC_COUNT * sizeof(multiCompSubDocReg));
    
    for(i = 0; i < MULTISUBDOC_COUNT; i++) {
        strncpy(subdoc_data->multi_comp_subdoc, subdocs[i], sizeof(subdoc_data->multi_comp_subdoc)-1);
        subdoc_data->executeBlobRequest = wifi_multicomp_subdoc_handler;
        subdoc_data->calcTimeout = wifi_vap_cfg_timeout_handler;
        subdoc_data->rollbackFunc = wifi_vap_cfg_rollback_handler;
    }

    register_MultiComp_subdoc_handler(subdoc_data, MULTISUBDOC_COUNT);

     /* Register ccsp event to receive GRE Tunnel UP/DOWN */
    ret = CcspBaseIf_Register_Event(bus_handle,NULL,"TunnelStatus");
    if (ret != CCSP_SUCCESS) {
        CcspTraceError(("%s Failed to register for tunnel status notification event",__FUNCTION__));
        return ret;
    }
    CcspBaseIf_SetCallback2(bus_handle, "TunnelStatus", Tunnel_event_callback, NULL);
    return RETURN_OK;
}

/**
  *  API to register all the supported subdocs , versionGet 
  *  and versionSet are callback functions to get and set 
  *  the subdoc versions in db 
  *
  *  returns 0 on success, error otherwise
  */
int init_web_config()
{
    
    char *sub_docs[SUBDOC_COUNT+1]= {"privatessid","homessid","wifiVapData",(char *) 0 };
    blobRegInfo *blobData = NULL,*blobDataPointer = NULL;
    int i;

    blobData = (blobRegInfo*) malloc(SUBDOC_COUNT * sizeof(blobRegInfo));
    if (blobData == NULL) {
        CcspTraceError(("%s: Malloc error\n",__FUNCTION__)); 
        return RETURN_ERR;
    }
    memset(blobData, 0, SUBDOC_COUNT * sizeof(blobRegInfo));

    blobDataPointer = blobData;
    for (i=0 ;i < SUBDOC_COUNT; i++)
    {
        strncpy(blobDataPointer->subdoc_name, sub_docs[i], sizeof(blobDataPointer->subdoc_name)-1);
        blobDataPointer++;
    }
    blobDataPointer = blobData;
 
    getVersion versionGet = getWiFiBlobVersion;
    setVersion versionSet = setWiFiBlobVersion;

    register_sub_docs(blobData,SUBDOC_COUNT,versionGet,versionSet);

    if (register_multicomp_subdocs() != RETURN_OK) {
        CcspTraceError(("%s: Failed to register multicomp subdocs with framework\n",__FUNCTION__));
        return RETURN_ERR;
    }
    return RETURN_OK;
}
#endif
