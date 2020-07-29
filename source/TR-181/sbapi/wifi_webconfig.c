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
#include "wifi_webconfig.h"
#include "plugin_main_apis.h"
#include "ctype.h"
#include "msgpack.h"
#include "webconfig_framework.h"
#include "ccsp_WifiLog_wrapper.h"
#include "secure_wrapper.h"

#define WEBCONF_SSID           0
#define WEBCONF_SECURITY       1
#define MIN_PWD_LEN            8
#define MAX_PWD_LEN           63
#define SUBDOC_COUNT           2 
#define SSID_DEFAULT_TIMEOUT   60
#define XB6_DEFAULT_TIMEOUT   15

static char *WiFiSsidVersion = "eRT.com.cisco.spvtg.ccsp.Device.WiFi.%s_version";
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

static char *NotifyWiFi = "eRT.com.cisco.spvtg.ccsp.Device.WiFi.NotifyWiFiChanges" ;
static char *WiFiRestored_AfterMig = "eRT.com.cisco.spvtg.ccsp.Device.WiFi.WiFiRestored_AfterMigration" ;
static char *FR   = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.FactoryReset";


extern char notifyWiFiChangesVal[16] ;


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
            strncpy(current_config->security_2g.passphrase, pWifiAp->SEC.Cfg.KeyPassphrase,
                     sizeof(current_config->security_2g.passphrase)-1);
            webconf_auth_mode_to_str(current_config->security_2g.mode_enabled,
                                    pWifiAp->SEC.Cfg.ModeEnabled);
            webconf_enc_mode_to_str(current_config->security_2g.encryption_method,
                                     pWifiAp->SEC.Cfg.EncryptionMethod);
        } else {
            strncpy(current_config->ssid_5g.ssid_name, pWifiSsid->SSID.Cfg.SSID,COSA_DML_WIFI_MAX_SSID_NAME_LEN);
            current_config->ssid_5g.enable = pWifiSsid->SSID.Cfg.bEnabled;
            current_config->ssid_5g.ssid_advertisement_enabled = pWifiAp->AP.Cfg.SSIDAdvertisementEnabled;
            strncpy(current_config->security_5g.passphrase, pWifiAp->SEC.Cfg.KeyPassphrase,
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
    int retval = RETURN_ERR;
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
        strncpy(pWifiAp->SEC.Cfg.KeyPassphrase, ps->security_2g.passphrase,sizeof(pWifiAp->SEC.Cfg.KeyPassphrase)-1);
        strncpy(pWifiAp->SEC.Cfg.PreSharedKey, ps->security_2g.passphrase,sizeof(pWifiAp->SEC.Cfg.PreSharedKey)-1);
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
        strncpy(pWifiAp->SEC.Cfg.KeyPassphrase, ps->security_5g.passphrase,sizeof(pWifiAp->SEC.Cfg.KeyPassphrase)-1);
        strncpy(pWifiAp->SEC.Cfg.PreSharedKey, ps->security_5g.passphrase,sizeof(pWifiAp->SEC.Cfg.PreSharedKey)-1);
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
    uint8_t radio_index = (wlan_index % 2);
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
            int wps_cfg = 0;
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
                retval = wifi_createAp(wlan_index, radio_index, ssid, (adv_enable == TRUE) ? FALSE : TRUE);
                if (retval != RETURN_OK) {
                    CcspTraceError(("%s: Failed to create AP Interface for wlan %d\n",
                                __FUNCTION__, wlan_index));
                    return retval;
                }
                CcspTraceInfo(("AP Created Successfully %d\n\n",wlan_index));
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
    COSA_DML_WIFI_AP_SEC_ENCRYPTION encryption_method = COSA_DML_WIFI_AP_SEC_TKIP;

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
        encryption_method = COSA_DML_WIFI_AP_SEC_TKIP;
        strcpy(method,"TKIPEncryption");
    } else if ((strcmp(encryption, "AES") == 0)) {
        encryption_method = COSA_DML_WIFI_AP_SEC_AES;
        strcpy(method,"AESEncryption");
    } 
#ifndef _XB6_PRODUCT_REQ_
    else if ((strcmp(encryption, "AES+TKIP") == 0)) {
        encryption_method = COSA_DML_WIFI_AP_SEC_AES_TKIP;
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
        CcspWifiTrace(("RDK_LOG_WARN,\n%s calling setBasicAuthenticationMode ssid : %d authmode : %s \n",
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
        strncpy(cur_sec_cfg->passphrase, passphrase, sizeof(cur_sec_cfg->passphrase)-1);
        apply_params.hostapd_restart = true;
        cur_sec_cfg->sec_changed = true;
        CcspWifiEventTrace(("RDK_LOG_NOTICE, KeyPassphrase changed \n "));
        CcspWifiTrace(("RDK_LOG_WARN, KeyPassphrase changed \n "));
        CcspWifiTrace(("RDK_LOG_WARN,\n RDKB_WIFI_CONFIG_CHANGED : %s KeyPassphrase changed for index = %d\n",
                        __FUNCTION__, wlan_index));
        CcspTraceInfo(("%s: Passpharse change applied for wlan index %d\n", __FUNCTION__, wlan_index));
    } else if (bForceDisableFlag == TRUE) {
        CcspWifiTrace(("RDK_LOG_WARN, WIFI_ATTEMPT_TO_CHANGE_CONFIG_WHEN_FORCE_DISABLED \n"));
    }

    if ((strcmp(cur_sec_cfg->encryption_method, encryption) != 0) &&
        (sec_mode >= COSA_DML_WIFI_SECURITY_WPA_Personal) &&
        (sec_mode <= COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise)) {
        CcspWifiTrace(("RDK_LOG_WARN,\n RDKB_WIFI_CONFIG_CHANGED :%s Encryption method changed , "
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
    BOOL ap_enable,up;

    if ((wlan_index % 2) == 0) {
        up = pssid_entry->ssid_2g.enable;
    } else {
        up = pssid_entry->ssid_5g.enable;
    }
    if ((cur_sec_cfg->sec_changed) && (up == TRUE)) {
        BOOL enable_wps = FALSE;
#ifdef CISCO_XB3_PLATFORM_CHANGES
        int wps_cfg = 0;
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

        retval = wifi_disableApEncryption(wlan_index);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to disable AP Encryption\n",__FUNCTION__));
            return retval;
        }

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

/**
 *   Applies Radio settings
 *
 *   return 0 on success, error otherwise
 */
int webconf_apply_radio_settings()
{
    int retval = RETURN_ERR;

    if (apply_params.hostapd_restart) {
#if (defined(_COSA_INTEL_USG_ATOM_) && !defined(_INTEL_WAV_) ) || ( (defined(_COSA_BCM_ARM_) || defined(_PLATFORM_TURRIS_)) && !defined(_CBR_PRODUCT_REQ_) && !defined(_XB7_PRODUCT_REQ_) )
        wifi_restartHostApd();
#else
        if (wifi_stopHostApd() != RETURN_OK) {
            CcspTraceError(("%s: Failed restart hostapd\n",__FUNCTION__));
            return RETURN_ERR;
        }
        if (wifi_startHostApd() != RETURN_OK) {
            CcspTraceError(("%s: Failed restart hostapd\n",__FUNCTION__));
            return RETURN_ERR;
        }
#endif
        CcspTraceInfo(("%s: Restarted Hostapd successfully\n", __FUNCTION__));
#if (defined(_XB6_PRODUCT_REQ_) && !defined(_XB7_PRODUCT_REQ_))
        /* XB6 some cases needs AP Interface UP/Down for hostapd pick up security changes */
        char status[8] = {0};
        bool enable = false;
        uint8_t wlan_index = 0;

        retval = wifi_getSSIDStatus(wlan_index, status);
        if (retval == RETURN_OK) {
            enable = (strcmp(status, "Enabled") == 0) ? true : false;
            if (wifi_setApEnable(wlan_index, enable) != RETURN_OK) {
                CcspTraceError(("%s: Error enabling AP Interface",__FUNCTION__));
                return RETURN_ERR;
            }
        }
        CcspTraceInfo(("%s:AP Interface UP Successful\n", __FUNCTION__));
#endif
    apply_params.hostapd_restart = false;
    }
    return RETURN_OK;
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
    bool  status = false, adv_enable = false;
    int ssid_len = 0;
    int i = 0, j = 0;
    char ssid_char[COSA_DML_WIFI_MAX_SSID_NAME_LEN] = {0};
    char ssid_lower[COSA_DML_WIFI_MAX_SSID_NAME_LEN] = {0};


    if ((wlan_index % 2) == 0) {
        ssid_name = pssid_entry->ssid_2g.ssid_name;
        status = pssid_entry->ssid_2g.enable;
        adv_enable = pssid_entry->ssid_2g.ssid_advertisement_enabled;
    } else {
        ssid_name = pssid_entry->ssid_5g.ssid_name;
        status = pssid_entry->ssid_5g.enable;
        adv_enable = pssid_entry->ssid_5g.ssid_advertisement_enabled;
    }

    ssid_len = strlen(ssid_name);
    if ((ssid_len == 0) || (ssid_len > COSA_DML_WIFI_MAX_SSID_NAME_LEN)) {
        strncpy(execRetVal->ErrorMsg,"Invalid SSID string size",sizeof(execRetVal->ErrorMsg)-1);
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
            strncpy(execRetVal->ErrorMsg,"Invalid character in SSID",sizeof(execRetVal->ErrorMsg)-1);
            return RETURN_ERR;
        }
    }
 

    /* SSID containing "optimumwifi", "TWCWiFi", "cablewifi" and "xfinitywifi" are reserved */
    if ((strstr(ssid_char, "cablewifi") != NULL) || (strstr(ssid_char, "twcwifi") != NULL) || (strstr(ssid_char, "optimumwifi") != NULL) ||
        (strstr(ssid_char, "xfinitywifi") != NULL) || (strstr(ssid_char, "xfinity") != NULL) || (strstr(ssid_char, "coxwifi") != NULL) ||
        (strstr(ssid_char, "spectrumwifi") != NULL) || (strstr(ssid_char, "shawopen") != NULL) || (strstr(ssid_char, "shawpasspoint") != NULL) ||
        (strstr(ssid_char, "shawguest") != NULL) || (strstr(ssid_char, "shawmobilehotspot") != NULL)) {

        CcspTraceError(("%s: Reserved SSID format used for ssid %d\n",__FUNCTION__, wlan_index));
        strncpy(execRetVal->ErrorMsg,"Reserved SSID format used",sizeof(execRetVal->ErrorMsg)-1);
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
    int retval = RETURN_ERR;


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
        strncpy(execRetVal->ErrorMsg,"Invalid Security Mode",sizeof(execRetVal->ErrorMsg)-1);
        return RETURN_ERR;
    }

    if ((strcmp(mode_enabled, "None") != 0) &&
        ((strcmp(encryption_method, "TKIP") != 0) && (strcmp(encryption_method, "AES") != 0) &&
        (strcmp(encryption_method, "AES+TKIP") != 0))) {
        CcspTraceError(("%s: Invalid Encryption Method for wlan index %d\n",__FUNCTION__, wlan_index));
        strncpy(execRetVal->ErrorMsg,"Invalid Encryption Method",sizeof(execRetVal->ErrorMsg)-1);
        return RETURN_ERR;
    }

    if (((strcmp(mode_enabled, "WPA-WPA2-Enterprise") == 0) || (strcmp(mode_enabled, "WPA-WPA2-Personal") == 0)) &&
        ((strcmp(encryption_method, "AES+TKIP") != 0) && (strcmp(encryption_method, "AES") != 0))) {
        CcspTraceError(("%s: Invalid Encryption Security Combination for wlan index %d\n",__FUNCTION__, wlan_index));
        strncpy(execRetVal->ErrorMsg,"Invalid Encryption Security Combination",sizeof(execRetVal->ErrorMsg)-1);
        return RETURN_ERR;
    }

    pass_len = strlen(passphrase);

    if ((pass_len < MIN_PWD_LEN) || (pass_len > MAX_PWD_LEN)) {
        CcspTraceInfo(("%s: Invalid Key passphrase length index %d\n",__FUNCTION__, wlan_index));
        strncpy(execRetVal->ErrorMsg,"Invalid Passphrase length",sizeof(execRetVal->ErrorMsg)-1);
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

    retval = webconf_apply_radio_settings();
    if (retval != RETURN_OK) {
        CcspTraceError(("%s: Failed to Apply Radio settings\n", __FUNCTION__));
        if (execRetVal) {
            strncpy(execRetVal->ErrorMsg,"Failed in Apply Radio settings",
                    sizeof(execRetVal->ErrorMsg)-1);
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
    int i;
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
    int i;
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
        int retPsm = CCSP_SUCCESS;

        BOOLEAN redirect;
        redirect = FALSE;
        retPsm = PSM_Set_Record_Value2(bus_handle,g_Subsystem, NotifyWiFi, ccsp_string,"false");
        CcspWifiTrace(("RDK_LOG_WARN,CaptivePortal:%s - start reverting redirection changes...\n",__FUNCTION__));

        strncpy(notifyWiFiChangesVal,"false",sizeof(notifyWiFiChangesVal)-1);
        configWifi(redirect);

        #ifdef CISCO_XB3_PLATFORM_CHANGES
            retPsm=PSM_Set_Record_Value2(bus_handle,g_Subsystem, WiFiRestored_AfterMig, ccsp_string,"false");
            if (retPsm == CCSP_SUCCESS) {
                    CcspWifiTrace(("RDK_LOG_INFO,CaptivePortal:%s - PSM set of WiFiRestored_AfterMigration success ...\n",__FUNCTION__));
            }
            else
            {
                    CcspWifiTrace(("RDK_LOG_ERROR,CaptivePortal:%s - PSM set of WiFiRestored_AfterMigration failed and ret value is %d...\n",__FUNCTION__,retPsm));
            }

            PSM_Set_Record_Value2(bus_handle,g_Subsystem, FR, ccsp_string, "0");
            CcspWifiTrace(("RDK_LOG_WARN, %s:%d Reset FactoryReset to 0\n",__FUNCTION__,__LINE__));
        #endif 
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
    FILE *fp = NULL;
    size_t offset = 0;
    msgpack_unpacked msg;
    msgpack_unpack_return mp_rv;
    msgpack_object_map *map = NULL;
    msgpack_object_kv* map_ptr  = NULL;
  
    webconf_wifi_t *ps = NULL;  
    int retval = RETURN_ERR;
    int i = 0;
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
  *  API to register all the supported subdocs , versionGet 
  *  and versionSet are callback functions to get and set 
  *  the subdoc versions in db 
  *
  *  returns 0 on success, error otherwise
  */
int init_web_config()
{
    
    char *sub_docs[SUBDOC_COUNT+1]= {"privatessid","homessid",(char *) 0 };
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
    return RETURN_OK;
}
#endif
