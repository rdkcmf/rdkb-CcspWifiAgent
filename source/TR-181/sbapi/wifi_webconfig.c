/************************************************************************************
  If not stated otherwise in this file or this component's LICENSE file the
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
#include "safec_lib_common.h"
#include "cJSON.h"

#define WEBCONF_SSID           0
#define WEBCONF_SECURITY       1
#define SUBDOC_COUNT           4
#define MULTISUBDOC_COUNT      1
#define SSID_DEFAULT_TIMEOUT   90
#define XB6_DEFAULT_TIMEOUT   15
#define AUTH_MODE_STR_SIZE    32
#define ENC_MODE_STR_SIZE     32

static char *WiFiSsidVersion = "eRT.com.cisco.spvtg.ccsp.Device.WiFi.%s_version";

#ifdef WIFI_HAL_VERSION_3
wifi_vap_info_map_t vap_map_per_radio[MAX_NUM_RADIOS];
static bool SSID_UPDATED[MAX_NUM_RADIOS] = {FALSE};
static bool PASSPHRASE_UPDATED[MAX_NUM_RADIOS] = {FALSE};
static bool gradio_restart[MAX_NUM_RADIOS] = {FALSE};
const char *MFPConfigOptions[3] = {"Disabled", "Optional", "Required"};
#else
static bool SSID1_UPDATED = FALSE;
static bool SSID2_UPDATED = FALSE;
static bool PASSPHRASE1_UPDATED = FALSE;
static bool PASSPHRASE2_UPDATED = FALSE;
static bool gradio_restart[2];
#endif
static bool gHostapd_restart_reqd = FALSE;
static bool gbsstrans_support[WIFI_INDEX_MAX];
static bool gwirelessmgmt_support[WIFI_INDEX_MAX];
/*static int hotspot_vaps[4] = {4,5,8,9};*/

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
wifi_radio_operationParam_t radio_curr_cfg[MAX_NUM_RADIOS];
extern char notifyWiFiChangesVal[16] ;

extern UINT g_interworking_RFC;
extern UINT g_passpoint_RFC;

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/

/* Function to convert authentication mode integer to string */
void webconf_auth_mode_to_str(char *auth_mode_str, COSA_DML_WIFI_SECURITY sec_mode)
{
    errno_t   rc  = -1;
    switch(sec_mode)
    {
#ifndef _XB6_PRODUCT_REQ_
    case COSA_DML_WIFI_SECURITY_WEP_64:
        rc = strcpy_s(auth_mode_str, AUTH_MODE_STR_SIZE , "WEP-64"); //auth_mode_str is a ptr,pointing to an 32 bit Array
        ERR_CHK(rc);
        break;
    case COSA_DML_WIFI_SECURITY_WEP_128:
        rc = strcpy_s(auth_mode_str, AUTH_MODE_STR_SIZE ,"WEP-128");
        ERR_CHK(rc);
        break;
    case COSA_DML_WIFI_SECURITY_WPA_Personal:
        rc = strcpy_s(auth_mode_str, AUTH_MODE_STR_SIZE ,"WPA-Personal");
        ERR_CHK(rc);
        break;
    case COSA_DML_WIFI_SECURITY_WPA_Enterprise:
        rc = strcpy_s(auth_mode_str, AUTH_MODE_STR_SIZE , "WPA-Enterprise");
        ERR_CHK(rc);
        break;
    case COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal:
        rc = strcpy_s(auth_mode_str, AUTH_MODE_STR_SIZE , "WPA-WPA2-Personal");
        ERR_CHK(rc);
        break;
    case COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise:
        rc = strcpy_s(auth_mode_str, AUTH_MODE_STR_SIZE , "WPA-WPA2-Enterprise");
        ERR_CHK(rc);
        break;
#endif
    case COSA_DML_WIFI_SECURITY_WPA2_Personal:
        rc = strcpy_s(auth_mode_str, AUTH_MODE_STR_SIZE , "WPA2-Personal");
        ERR_CHK(rc);
        break;
    case COSA_DML_WIFI_SECURITY_WPA2_Enterprise:
        rc = strcpy_s(auth_mode_str, AUTH_MODE_STR_SIZE , "WPA2-Enterprise");
        ERR_CHK(rc);
        break;
#ifdef WIFI_HAL_VERSION_3
    case COSA_DML_WIFI_SECURITY_WPA3_Personal:
        rc = strcpy_s(auth_mode_str, AUTH_MODE_STR_SIZE , "WPA3-Personal");
        ERR_CHK(rc);
        break;
    case COSA_DML_WIFI_SECURITY_WPA3_Personal_Transition:
        rc = strcpy_s(auth_mode_str, AUTH_MODE_STR_SIZE , "WPA3-Personal-Transition");
        ERR_CHK(rc);
        break;
    case COSA_DML_WIFI_SECURITY_WPA3_Enterprise:
         rc = strcpy_s(auth_mode_str, AUTH_MODE_STR_SIZE , "WPA3-Enterprise");
         ERR_CHK(rc);
         break;
#endif
    case COSA_DML_WIFI_SECURITY_None:
        default:
        rc = strcpy_s(auth_mode_str, AUTH_MODE_STR_SIZE , "None");
        ERR_CHK(rc);
        break;
    }
}

/* Function to convert Encryption mode integer to string */
void webconf_enc_mode_to_str(char *enc_mode_str,COSA_DML_WIFI_AP_SEC_ENCRYPTION enc_mode)
{
    errno_t rc = -1;
    switch(enc_mode)
    {
    case COSA_DML_WIFI_AP_SEC_TKIP:
        rc = strcpy_s(enc_mode_str, ENC_MODE_STR_SIZE , "TKIP");
        ERR_CHK(rc);
        break;
    case COSA_DML_WIFI_AP_SEC_AES:
        rc = strcpy_s(enc_mode_str, ENC_MODE_STR_SIZE , "AES");
        ERR_CHK(rc);
        break;
    case COSA_DML_WIFI_AP_SEC_AES_TKIP:
        rc = strcpy_s(enc_mode_str, ENC_MODE_STR_SIZE , "AES+TKIP");
        ERR_CHK(rc);
        break;
    default:
        rc = strcpy_s(enc_mode_str, ENC_MODE_STR_SIZE , "None");
        ERR_CHK(rc);
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
#ifdef WIFI_HAL_VERSION_3
    else if (strcmp(auth_mode_str, "WPA3-Enterprise") == 0) {
        *auth_mode = COSA_DML_WIFI_SECURITY_WPA3_Enterprise;
    }
    else if ((strcmp(auth_mode_str, "WPA3-Personal") == 0)) {
        *auth_mode = COSA_DML_WIFI_SECURITY_WPA3_Personal;
    }
    else if ((strcmp(auth_mode_str, "WPA3-Personal-Transition") == 0)) {
        *auth_mode = COSA_DML_WIFI_SECURITY_WPA3_Personal_Transition;
    }
#endif /* WIFI_HAL_VERSION_3 */
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
#ifdef WIFI_HAL_VERSION_3
    UINT apIndex, radioIndex;
    for (apIndex = 0; apIndex < getTotalNumberVAPs(); apIndex++) 
    {
        if ( (ssid == WIFI_WEBCONFIG_PRIVATESSID && isVapPrivate(apIndex)) || (ssid == WIFI_WEBCONFIG_HOMESSID && isVapXhs(apIndex)) )
        {
            if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->SsidQueue, apIndex)) == NULL) 
            {
                CcspTraceError(("%s Data Model object not found!\n",__FUNCTION__));
                return RETURN_ERR;
            }
#else
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
#endif


        if((pWifiSsid = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) {
            CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
            return RETURN_ERR;
        }
#ifdef WIFI_HAL_VERSION_3
            if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->AccessPointQueue, apIndex)) == NULL) {
#else
        if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->AccessPointQueue, i)) == NULL) {
#endif
            CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
            return RETURN_ERR;
        }

        if((pWifiAp = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) {
            CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
            return RETURN_ERR;
        }
#ifdef WIFI_HAL_VERSION_3
            radioIndex = getRadioIndexFromAp(apIndex);
            strncpy(current_config->ssid[radioIndex].ssid_name, pWifiSsid->SSID.Cfg.SSID,COSA_DML_WIFI_MAX_SSID_NAME_LEN);
            current_config->ssid[radioIndex].enable = pWifiSsid->SSID.Cfg.bEnabled;
            current_config->ssid[radioIndex].ssid_advertisement_enabled = pWifiAp->AP.Cfg.SSIDAdvertisementEnabled;
            strncpy(current_config->security[radioIndex].passphrase, (char*)pWifiAp->SEC.Cfg.KeyPassphrase,
                    sizeof(current_config->security[radioIndex].passphrase)-1);
            webconf_auth_mode_to_str(current_config->security[radioIndex].mode_enabled,
                    pWifiAp->SEC.Cfg.ModeEnabled);
            webconf_enc_mode_to_str(current_config->security[radioIndex].encryption_method,
                    pWifiAp->SEC.Cfg.EncryptionMethod);
        }
#else
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
#endif

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
    void* pWifiApModeEnabled       = NULL;
    void* pWifiApEncryptionMethod  = NULL;

#ifdef WIFI_HAL_VERSION_3
    UINT apIndex, radioIndex;
    for (apIndex = 0; apIndex < getTotalNumberVAPs(); apIndex++) 
    {
        if ( (ssid == WIFI_WEBCONFIG_PRIVATESSID && isVapPrivate(apIndex)) || (ssid == WIFI_WEBCONFIG_HOMESSID && isVapXhs(apIndex)) )
        {
            radioIndex = getRadioIndexFromAp(apIndex);
            if (curr_config->ssid[radioIndex].ssid_changed) 
            {
                if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->SsidQueue, apIndex)) == NULL) 
                {
                    CcspTraceError(("%s Data Model object not found!\n",__FUNCTION__));
                    return RETURN_ERR;
                }

                if((pWifiSsid = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) 
                {
                    CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
                    return RETURN_ERR;
                }
                if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->AccessPointQueue, apIndex)) == NULL) 
                {
                    CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
                    return RETURN_ERR;
                }

                if((pWifiAp = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) 
                {
                    CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
                    return RETURN_ERR;
                }
                
                strncpy(pWifiSsid->SSID.Cfg.SSID, ps->ssid[radioIndex].ssid_name, sizeof(pWifiSsid->SSID.Cfg.SSID)-1);
                pWifiSsid->SSID.Cfg.bEnabled = ps->ssid[radioIndex].enable;
                pWifiAp->AP.Cfg.SSIDAdvertisementEnabled = ps->ssid[radioIndex].ssid_advertisement_enabled;

                memcpy(&sWiFiDmlSsidStoredCfg[pWifiSsid->SSID.Cfg.InstanceNumber-1], &pWifiSsid->SSID.Cfg, sizeof(COSA_DML_WIFI_SSID_CFG));
                memcpy(&sWiFiDmlApStoredCfg[pWifiAp->AP.Cfg.InstanceNumber-1].Cfg, &pWifiAp->AP.Cfg, sizeof(COSA_DML_WIFI_AP_CFG));
                memcpy(&sWiFiDmlSsidRunningCfg[pWifiSsid->SSID.Cfg.InstanceNumber-1], &pWifiSsid->SSID.Cfg, sizeof(COSA_DML_WIFI_SSID_CFG));
                memcpy(&sWiFiDmlApRunningCfg[pWifiAp->AP.Cfg.InstanceNumber-1].Cfg, &pWifiAp->AP.Cfg, sizeof(COSA_DML_WIFI_AP_CFG));
                
                curr_config->ssid[radioIndex].ssid_changed = false;
            }

            if (curr_config->security[radioIndex].sec_changed) 
            {
                if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->AccessPointQueue, apIndex)) == NULL) {
                    CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
                    return RETURN_ERR;
                }

                if((pWifiAp = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) {
                    CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
                    return RETURN_ERR;
                }
                pWifiApModeEnabled = &pWifiAp->SEC.Cfg.ModeEnabled;
                webconf_auth_mode_to_int(ps->security[radioIndex].mode_enabled, pWifiApModeEnabled); 
                strncpy((char*)pWifiAp->SEC.Cfg.KeyPassphrase, ps->security[radioIndex].passphrase,sizeof(pWifiAp->SEC.Cfg.KeyPassphrase)-1);
                strncpy((char*)pWifiAp->SEC.Cfg.PreSharedKey, ps->security[radioIndex].passphrase,sizeof(pWifiAp->SEC.Cfg.PreSharedKey)-1);
#ifdef WIFI_HAL_VERSION_3
                strncpy((char*)pWifiAp->SEC.Cfg.SAEPassphrase, ps->security[radioIndex].passphrase,sizeof(pWifiAp->SEC.Cfg.SAEPassphrase)-1);
#endif
                pWifiApEncryptionMethod = &pWifiAp->SEC.Cfg.EncryptionMethod;
                webconf_enc_mode_to_int(ps->security[radioIndex].encryption_method, pWifiApEncryptionMethod);

                memcpy(&sWiFiDmlApSecurityStored[apIndex].Cfg, &pWifiAp->SEC.Cfg, sizeof(COSA_DML_WIFI_APSEC_CFG));
                memcpy(&sWiFiDmlApSecurityRunning[apIndex].Cfg, &pWifiAp->SEC.Cfg, sizeof(COSA_DML_WIFI_APSEC_CFG));
                
                curr_config->security[radioIndex].sec_changed = false;
             }

        }
    }
#else
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
        
        pWifiApModeEnabled = &pWifiAp->SEC.Cfg.ModeEnabled;
        webconf_auth_mode_to_int(ps->security_2g.mode_enabled, pWifiApModeEnabled); 
        strncpy((char*)pWifiAp->SEC.Cfg.KeyPassphrase, ps->security_2g.passphrase,sizeof(pWifiAp->SEC.Cfg.KeyPassphrase)-1);
        strncpy((char*)pWifiAp->SEC.Cfg.PreSharedKey, ps->security_2g.passphrase,sizeof(pWifiAp->SEC.Cfg.PreSharedKey)-1);
        pWifiApEncryptionMethod = &pWifiAp->SEC.Cfg.EncryptionMethod;
        webconf_enc_mode_to_int(ps->security_2g.encryption_method, pWifiApEncryptionMethod);

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

        pWifiApModeEnabled = &pWifiAp->SEC.Cfg.ModeEnabled;
        webconf_auth_mode_to_int(ps->security_5g.mode_enabled, pWifiApModeEnabled);
        strncpy((char*)pWifiAp->SEC.Cfg.KeyPassphrase, ps->security_5g.passphrase,sizeof(pWifiAp->SEC.Cfg.KeyPassphrase)-1);
        strncpy((char*)pWifiAp->SEC.Cfg.PreSharedKey, ps->security_5g.passphrase,sizeof(pWifiAp->SEC.Cfg.PreSharedKey)-1);
        pWifiApEncryptionMethod = &pWifiAp->SEC.Cfg.EncryptionMethod;
        webconf_enc_mode_to_int(ps->security_5g.encryption_method, pWifiApEncryptionMethod);
   
        memcpy(&sWiFiDmlApSecurityStored[wlan_index+1].Cfg, &pWifiAp->SEC.Cfg, sizeof(COSA_DML_WIFI_APSEC_CFG));
        memcpy(&sWiFiDmlApSecurityRunning[wlan_index+1].Cfg, &pWifiAp->SEC.Cfg, sizeof(COSA_DML_WIFI_APSEC_CFG));
        
        curr_config->security_5g.sec_changed = false;
    }
#endif
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

#ifdef WIFI_HAL_VERSION_3
    UINT radioIndex = getRadioIndexFromAp(wlan_index);
    {
        wlan_ssid = &pssid_entry->ssid[radioIndex];
        cur_conf_ssid = &curr_config->ssid[radioIndex];        
    }
#else
    if ((wlan_index % 2) ==0) 
    {
        wlan_ssid = &pssid_entry->ssid_2g;
        cur_conf_ssid = &curr_config->ssid_2g;
    } else {
        wlan_ssid = &pssid_entry->ssid_5g;
        cur_conf_ssid = &curr_config->ssid_5g;
    }
#endif
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
#ifdef WIFI_HAL_VERSION_3
        if (isVapPrivate(wlan_index))
        {
            SSID_UPDATED[getRadioIndexFromAp(wlan_index)] = TRUE;
        }
        
#else
        if (wlan_index == 0) {
            SSID1_UPDATED = TRUE;
        } else if (wlan_index == 1) {
            SSID2_UPDATED = TRUE;
        }
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
#ifdef WIFI_HAL_VERSION_3
        webconf_auth_mode_to_int(curr_config->security[getRadioIndexFromAp(wlan_index)].mode_enabled, &auth_mode);
#else
        if ((wlan_index % 2) ==0) {
            webconf_auth_mode_to_int(curr_config->security_2g.mode_enabled, &auth_mode);
        } else {
            webconf_auth_mode_to_int(curr_config->security_5g.mode_enabled, &auth_mode);
        }
#endif
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
#ifdef WIFI_HAL_VERSION_3
        curr_config->security[getRadioIndexFromAp(wlan_index)].sec_changed = true;
#else
        if (!wlan_index) {
            curr_config->security_2g.sec_changed = true;
        } else {
            curr_config->security_5g.sec_changed = true;
        }
#endif
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
    errno_t  rc   =  -1;
    webconf_security_t *wlan_security = NULL, *cur_sec_cfg = NULL;
    BOOLEAN bForceDisableFlag = FALSE;

    COSA_DML_WIFI_SECURITY sec_mode = COSA_DML_WIFI_SECURITY_None;

#ifdef WIFI_HAL_VERSION_3
    UINT radioIndex = getRadioIndexFromAp(wlan_index);
    wlan_security = &pssid_entry->security[radioIndex];
    cur_sec_cfg = &curr_config->security[radioIndex];
#else
    if ((wlan_index % 2) ==0) {
        wlan_security = &pssid_entry->security_2g;
        cur_sec_cfg = &curr_config->security_2g;
    } else {
        wlan_security = &pssid_entry->security_5g;
        cur_sec_cfg = &curr_config->security_5g;
    }
#endif
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
        rc = strcpy_s(securityType, sizeof(securityType) , "None");
        ERR_CHK(rc);

        rc = strcpy_s(authMode, sizeof(authMode) , "None");
        ERR_CHK(rc);

    }
#ifndef _XB6_PRODUCT_REQ_
    else if (strcmp(mode, "WEP-64") == 0) {
        sec_mode = COSA_DML_WIFI_SECURITY_WEP_64;
    } else if (strcmp(mode, "WEP-128") == 0) {
        sec_mode = COSA_DML_WIFI_SECURITY_WEP_128;
    }
    else if (strcmp(mode, "WPA-Personal") == 0) {
        rc = strcpy_s(securityType, sizeof(securityType) , "WPA");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) , "PSKAuthentication");
        ERR_CHK(rc);
        sec_mode = COSA_DML_WIFI_SECURITY_WPA_Personal;
    } else if (strcmp(mode, "WPA2-Personal") == 0) {
        sec_mode = COSA_DML_WIFI_SECURITY_WPA2_Personal;
        rc = strcpy_s(securityType, sizeof(securityType) , "11i");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) , "PSKAuthentication");
        ERR_CHK(rc);
    } else if (strcmp(mode, "WPA-Enterprise") == 0) {
        sec_mode = COSA_DML_WIFI_SECURITY_WPA_Enterprise;
    } else if (strcmp(mode, "WPA-WPA2-Personal") == 0) {
        rc = strcpy_s(securityType, sizeof(securityType) , "WPAand11i");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) , "PSKAuthentication");
        ERR_CHK(rc);
        sec_mode = COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal;
    }
#else
    else if ((strcmp(mode, "WPA2-Personal") == 0)) {
        sec_mode = COSA_DML_WIFI_SECURITY_WPA2_Personal;
        rc = strcpy_s(securityType, sizeof(securityType) , "11i");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) , "SharedAuthentication");
        ERR_CHK(rc);
    }
#endif
    else if (strcmp(mode, "WPA2-Enterprise") == 0) {
        sec_mode = COSA_DML_WIFI_SECURITY_WPA2_Enterprise;
        rc = strcpy_s(securityType, sizeof(securityType) , "11i");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) , "EAPAuthentication");
        ERR_CHK(rc);
    } else if (strcmp(mode, "WPA-WPA2-Enterprise") == 0) {
        rc = strcpy_s(securityType, sizeof(securityType), "WPAand11i");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) , "EAPAuthentication");
        ERR_CHK(rc);
        sec_mode = COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise;
    }


    if ((strcmp(encryption, "TKIP") == 0)) {
        rc = strcpy_s(method, sizeof(method) ,"TKIPEncryption");
        ERR_CHK(rc);
    } else if ((strcmp(encryption, "AES") == 0)) {
        rc = strcpy_s(method, sizeof(method) , "AESEncryption");
        ERR_CHK(rc);
    }
#ifndef _XB6_PRODUCT_REQ_
    else if ((strcmp(encryption, "AES+TKIP") == 0)) {
        rc = strcpy_s(method, sizeof(method) ,"TKIPandAESEncryption");
        ERR_CHK(rc);
    }
#endif

    /* Apply Security Values to hal */
#ifdef WIFI_HAL_VERSION_3
        if ((isVapPrivate(wlan_index)) &&
            (sec_mode == COSA_DML_WIFI_SECURITY_None)) {

#else
    if (((wlan_index == 0) || (wlan_index == 1)) &&
        (sec_mode == COSA_DML_WIFI_SECURITY_None)) {
#endif
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
#ifdef WIFI_HAL_VERSION_3
        if (isVapPrivate(wlan_index))
        {
            PASSPHRASE_UPDATED[getRadioIndexFromAp(wlan_index)] = TRUE;
        }
#else
        if (wlan_index == 0) {
            PASSPHRASE1_UPDATED = TRUE;
        } else if (wlan_index == 1) {
            PASSPHRASE2_UPDATED = TRUE;
        }
#endif
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
        v_secure_system("/usr/bin/sysevent set wifi_ApSecurity \"RDK|%d|%s|%s|%s\"",wlan_index, passphrase, mode, method);
    }
 
#if (!defined(_XB6_PRODUCT_REQ_) || defined (_XB7_PRODUCT_REQ_))
    BOOL up;

#ifdef WIFI_HAL_VERSION_3
    up = pssid_entry->ssid[getRadioIndexFromAp(wlan_index)].enable;
#else
    if ((wlan_index % 2) == 0) {
        up = pssid_entry->ssid_2g.enable;
    } else {
        up = pssid_entry->ssid_5g.enable;
    }
#endif        

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

#if !defined(_XB7_PRODUCT_REQ_) && !defined(_XB8_PRODUCT_REQ_) && !defined(_CBR2_PRODUCT_REQ_)
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

#ifdef WIFI_HAL_VERSION_3
ANSC_STATUS  updateDMLConfigPerRadio(UINT radioIndex)
{
    UINT vapCount = 0;
    UINT vapArrayInstance = 0;
    UINT vapIndex = 0;
    PCOSA_DATAMODEL_WIFI pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_SSID             pWifiSsid           = (PCOSA_DML_WIFI_SSID      )NULL;
    PCOSA_DML_WIFI_AP               pWifiAp             = (PCOSA_DML_WIFI_AP        )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj            = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    for (vapCount = 0; vapCount < vap_map_per_radio[radioIndex].num_vaps; vapCount++)
    {
        //Update the DML cache from VAP structure
        vapIndex = vap_map_per_radio[radioIndex].vap_array[vapCount].vap_index;
        vapArrayInstance = vapIndex+1;
        CcspWifiTrace(("RDK_LOG_INFO, %s Updating VAP DML Structure for vapIndex : %d \n", __FUNCTION__, vapIndex));

        pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
        pLinkObj = CosaSListGetEntryByInsNum((PSLIST_HEADER)&pMyObject->SsidQueue, vapArrayInstance);
        pWifiSsid = pLinkObj->hContext;

        CosaDmlWiFiSsidGetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, &(pWifiSsid->SSID.Cfg));
        CosaDmlWiFiSsidGetSinfo((ANSC_HANDLE)pMyObject->hPoamWiFiDm, vapArrayInstance, &(pWifiSsid->SSID.StaticInfo));

        pLinkObj = CosaSListGetEntryByInsNum((PSLIST_HEADER)&pMyObject->AccessPointQueue, vapArrayInstance);
        pWifiAp = pLinkObj->hContext;

        CosaDmlWiFiApGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->AP);
        ccspWifiDbgPrint(CCSP_WIFI_TRACE, " %s Updating AP DML structure for vapindex : %d Instancenumber : %d Name : %s\n", __FUNCTION__, vapIndex, sWiFiDmlApStoredCfg[vapIndex].Cfg.InstanceNumber,  pWifiSsid->SSID.StaticInfo.Name);
        CosaDmlWiFiApSecGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->SEC);
        CosaDmlWiFiApWpsGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->WPS);
        CcspWifiTrace(("RDK_LOG_INFO, %s Successful Update VAP DML Structure for vapIndex : %d \n", __FUNCTION__, vapIndex));

        //Call the respective setPSM function to update the PSM Values
        CosaDmlWiFiSetAccessPointPsmData(&pWifiAp->AP.Cfg);
        CosaDmlWiFiSetApMacFilterPsmData(vapIndex, &pWifiAp->MF);
        CcspWifiTrace(("RDK_LOG_INFO, %s Successful Update PSM for vapIndex : %d \n", __FUNCTION__, vapIndex));

    }
    return ANSC_STATUS_SUCCESS;
}
#endif

int wifi_reset_radio()
{
    int retval = RETURN_ERR;
    PCOSA_DATAMODEL_WIFI pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    uint8_t i;

#ifdef DUAL_CORE_XB3
    wifi_initRadio(0);
#endif
#ifdef WIFI_HAL_VERSION_3
    for (i = 0;i < getNumberRadios();i++)
#else
    for (i = 0;i < 2;i++)
#endif
{
        if (gradio_restart[i]) {
#if defined(_INTEL_WAV_)
            wifi_applyRadioSettings(i);
#elif !defined(DUAL_CORE_XB3)
            wifi_initRadio(i);
#endif
#if defined WIFI_HAL_VERSION_3
            retval = CosaWifiReInitialize((ANSC_HANDLE)pMyObject, i, (i==0)?TRUE:FALSE);
#else
            retval = CosaWifiReInitialize((ANSC_HANDLE)pMyObject, i);
#endif //WIFI_HAL_VERSION_3
            if (retval != RETURN_OK) { 
                CcspTraceError(("%s: CosaWifiReInitialize Failed for Radio %u with retval %u\n",__FUNCTION__,i, retval));
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
#ifdef WIFI_HAL_VERSION_3
        else
        {
            updateDMLConfigPerRadio(i);
        }
#endif //WIFI_HAL_VERSION_3
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
#ifndef WIFI_HAL_VERSION_3
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

#if (defined(_COSA_BCM_ARM_) && defined(_XB7_PRODUCT_REQ_)) || defined(_XB8_PRODUCT_REQ_) || defined(_CBR2_PRODUCT_REQ_)
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
#endif
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


#ifdef WIFI_HAL_VERSION_3
    ssid_name = pssid_entry->ssid[getRadioIndexFromAp(wlan_index)].ssid_name;
#else
    if ((wlan_index % 2) == 0) {
        ssid_name = pssid_entry->ssid_2g.ssid_name;
    } else {
        ssid_name = pssid_entry->ssid_5g.ssid_name;
    }
#endif

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

#ifdef WIFI_HAL_VERSION_3
    UINT radioIndex = getRadioIndexFromAp(wlan_index);
    passphrase = pssid_entry->security[radioIndex].passphrase;
    mode_enabled = pssid_entry->security[radioIndex].mode_enabled;
    encryption_method = pssid_entry->security[radioIndex].encryption_method;    
#else
    if ((wlan_index % 2) == 0) {
        passphrase = pssid_entry->security_2g.passphrase;
        mode_enabled = pssid_entry->security_2g.mode_enabled;
        encryption_method = pssid_entry->security_2g.encryption_method;
    } else {
        passphrase = pssid_entry->security_5g.passphrase;
        mode_enabled = pssid_entry->security_5g.mode_enabled;
        encryption_method = pssid_entry->security_5g.encryption_method;
    }

#endif  

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
    int retval = RETURN_ERR;
    char *err = NULL;
    uint8_t i = 0, wlan_index = 0;

#ifdef WIFI_HAL_VERSION_3
    for (i = 0; i < getTotalNumberVAPs(); i++) 
    {
        if ( (ssid == WIFI_WEBCONFIG_PRIVATESSID && isVapPrivate(i)) || (ssid == WIFI_WEBCONFIG_HOMESSID && isVapXhs(i)) )
        {
#else
    if (ssid == WIFI_WEBCONFIG_PRIVATESSID) {
        wlan_index = 0;
    } else if (ssid == WIFI_WEBCONFIG_HOMESSID) {
        wlan_index = 2;
    }

    for (i = wlan_index;i < (wlan_index+2); i++) {
#endif
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
#ifdef WIFI_HAL_VERSION_3
    }
#endif
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
#ifdef WIFI_HAL_VERSION_3
    for (i = 0; i < getTotalNumberVAPs(); i++) 
    {
        if (pssid_entry->ssid[getRadioIndexFromAp(i)].configured) {
            if ( (ssid == WIFI_WEBCONFIG_PRIVATESSID && isVapPrivate(i)) || (ssid == WIFI_WEBCONFIG_HOMESSID && isVapXhs(i)) )
            {
#else
    if (ssid == WIFI_WEBCONFIG_PRIVATESSID) {
        wlan_index = 0;
    } else if (ssid == WIFI_WEBCONFIG_HOMESSID) {
        wlan_index = 2;
    }

    for (i = wlan_index;i < (wlan_index+2); i++) {
#endif
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
#ifdef WIFI_HAL_VERSION_3
        }
    }
#endif
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
#ifdef WIFI_HAL_VERSION_3
        UINT radioIndex = 0;
        for (UINT apIndex = 0; apIndex < getTotalNumberVAPs(); apIndex++)
        {
            if (isVapPrivate(apIndex))
            {
                radioIndex = getRadioIndexFromAp(apIndex);
                if (SSID_UPDATED[radioIndex])
                {
                    snprintf(param_name, sizeof(param_name), "Device.WiFi.SSID.%u.SSID", apIndex + 1);
                    val.parameterName = param_name;
                    val.newValue = ps->ssid[radioIndex].ssid_name;
                    WiFiPramValueChangedCB(&val, 0, NULL);
                    SSID_UPDATED[radioIndex] = FALSE;
                }

                if (PASSPHRASE_UPDATED[radioIndex])
                {
                    snprintf(param_name, sizeof(param_name), "Device.WiFi.AccessPoint.%u.Security.X_COMCAST-COM_KeyPassphrase", apIndex + 1);
                    val.parameterName = param_name;
                    val.newValue = ps->security[radioIndex].passphrase;
                    WiFiPramValueChangedCB(&val, 0, NULL);
                    PASSPHRASE_UPDATED[radioIndex] = FALSE;
                }
            }
        }
#else
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
    size_t offset = 0;
    msgpack_unpacked msg;
    msgpack_unpack_return mp_rv;
    msgpack_object_map *map = NULL;
    msgpack_object_kv* map_ptr  = NULL;
  
    webconf_wifi_t *ps = NULL;  
    unsigned int i = 0;

#ifdef WIFI_HAL_VERSION_3
    char ssid_str[MAX_NUM_RADIOS][20] = {0};
    char sec_str[MAX_NUM_RADIOS][20] = {0};
#else
    char ssid_2g_str[20] = {0};
    char ssid_5g_str[20] = {0};
    char sec_2g_str[20] = {0};
    char sec_5g_str[20] = {0};
#endif

    msgpack_unpacked_init( &msg );
    len +=  1;
    /* The outermost wrapper MUST be a map. */
    mp_rv = msgpack_unpack_next( &msg, (const char*) buf, len, &offset );
    if (mp_rv != MSGPACK_UNPACK_SUCCESS) {
        CcspTraceError(("%s: Failed to unpack wifi msg blob. Error %d",__FUNCTION__,mp_rv));
        msgpack_unpacked_destroy( &msg );
        return RETURN_ERR;
    }

    CcspTraceInfo(("%s:Msg unpack success. Offset is %u\n", __FUNCTION__,offset));
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
#ifdef WIFI_HAL_VERSION_3
        for (UINT radioIndex = 0; radioIndex < getNumberRadios(); ++radioIndex)
        {
            UINT band = convertRadioIndexToFrequencyNum(radioIndex);
            snprintf(ssid_str[radioIndex],sizeof(ssid_str[radioIndex]),"private_ssid_%ug", band);
            snprintf(sec_str[radioIndex],sizeof(sec_str[radioIndex]),"private_security_%ug", band);
        }
#else
        snprintf(ssid_2g_str,sizeof(ssid_2g_str),"private_ssid_2g");
        snprintf(ssid_5g_str,sizeof(ssid_5g_str),"private_ssid_5g");
        snprintf(sec_2g_str,sizeof(sec_2g_str),"private_security_2g");
        snprintf(sec_5g_str,sizeof(sec_5g_str),"private_security_5g");
#endif
    } else if (ssid == WIFI_WEBCONFIG_HOMESSID) {
#ifdef WIFI_HAL_VERSION_3
        for (UINT radioIndex = 0; radioIndex < getNumberRadios(); ++radioIndex)
        {
            UINT band = convertRadioIndexToFrequencyNum(radioIndex);
            snprintf(ssid_str[radioIndex],sizeof(ssid_str[radioIndex]),"home_ssid_%ug",band);
            snprintf(sec_str[radioIndex],sizeof(sec_str[radioIndex]),"home_security_%ug", band);
        }
#else
        snprintf(ssid_2g_str,sizeof(ssid_2g_str),"home_ssid_2g");
        snprintf(ssid_5g_str,sizeof(ssid_5g_str),"home_ssid_5g");
        snprintf(sec_2g_str,sizeof(sec_2g_str),"home_security_2g");
        snprintf(sec_5g_str,sizeof(sec_5g_str),"home_security_5g");
#endif
    } else {
        CcspTraceError(("%s: Invalid ssid type\n",__FUNCTION__));
    }
#ifdef WIFI_HAL_VERSION_3
    for (UINT radioIndex = 0; radioIndex < getNumberRadios(); ++radioIndex)
    {
        ps->ssid[radioIndex].configured = FALSE;
    }
#endif
    /* Parsing Config Msg String to Wifi Structure */
    for(i = 0;i < map->size;i++) {
#ifdef WIFI_HAL_VERSION_3
        for (UINT radioIndex = 0; radioIndex < getNumberRadios(); ++radioIndex)
        {
            if (strncmp(map_ptr->key.via.str.ptr, ssid_str[radioIndex], map_ptr->key.via.str.size) == 0) {
                ps->ssid[radioIndex].configured = TRUE;
                if (webconf_copy_wifi_ssid_params(map_ptr->val, &ps->ssid[radioIndex]) != RETURN_OK) {
                    CcspTraceError(("%s:Failed to copy wifi ssid params for wlan index 0",__FUNCTION__));
                    msgpack_unpacked_destroy( &msg );
                    if (ps) {  
                        free(ps);
                        ps = NULL;  
                    } 
                    return RETURN_ERR; 
                }  
            } 
            else if (strncmp(map_ptr->key.via.str.ptr, sec_str[radioIndex], map_ptr->key.via.str.size) == 0) {
                ps->security[radioIndex].configured = TRUE;
                if (webconf_copy_wifi_security_params(map_ptr->val, &ps->security[radioIndex]) != RETURN_OK) {
                    CcspTraceError(("%s:Failed to copy wifi security params for wlan index 0",__FUNCTION__));
                    msgpack_unpacked_destroy( &msg );
                    if (ps) {
                        free(ps);
                        ps = NULL;
                    } 
                    return RETURN_ERR;
                }
            }  
        }
        if (strncmp(map_ptr->key.via.str.ptr, "subdoc_name", map_ptr->key.via.str.size) == 0) {
            if (map_ptr->val.type == MSGPACK_OBJECT_STR) {
                strncpy(ps->subdoc_name, map_ptr->val.via.str.ptr, map_ptr->val.via.str.size);
                CcspTraceError(("subdoc name %s\n", ps->subdoc_name));
            }
        }
        else if (strncmp(map_ptr->key.via.str.ptr, "version", map_ptr->key.via.str.size) == 0) {
            if (map_ptr->val.type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
                ps->version = (uint64_t) map_ptr->val.via.u64;
                CcspTraceError(("Version type %d version %llu\n",map_ptr->val.type,ps->version));
                }
        }
        else if (strncmp(map_ptr->key.via.str.ptr, "transaction_id", map_ptr->key.via.str.size) == 0) {
            if (map_ptr->val.type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
                ps->transaction_id = (uint16_t) map_ptr->val.via.u64;
                CcspTraceError(("Tx id type %d tx id %d\n",map_ptr->val.type,ps->transaction_id));
            }
        }
#else
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
                CcspTraceError(("Version type %d version %llu\n",map_ptr->val.type,ps->version));
            }
        }
        else if (strncmp(map_ptr->key.via.str.ptr, "transaction_id", map_ptr->key.via.str.size) == 0) {
            if (map_ptr->val.type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
                ps->transaction_id = (uint16_t) map_ptr->val.via.u64;
                CcspTraceError(("Tx id type %d tx id %d\n",map_ptr->val.type,ps->transaction_id));
            }
        }
#endif
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
#ifdef WIFI_HAL_VERSION_3
//Here we populate the structure curr_cfg , since we will can createVAP API to apply the settings
    BOOLEAN bForceDisableFlag = FALSE;
    uint8_t wlan_index = curr_cfg->vap_index;

    if(ANSC_STATUS_FAILURE == CosaDmlWiFiGetCurrForceDisableWiFiRadio(&bForceDisableFlag))
    {
        CcspTraceError(("%s Failed to fetch ForceDisableWiFiRadio flag!!!\n",__FUNCTION__));
        return "Dml fetch failed";
    }

    /* Apply SSID Enable/Disable */
    if ((curr_cfg->u.bss_info.enabled != vap_cfg->u.bss_info.enabled) && (!bForceDisableFlag))
    {
        curr_cfg->u.bss_info.enabled = vap_cfg->u.bss_info.enabled;
        curr_cfg->u.bss_info.wps.enable = vap_cfg->u.bss_info.wps.enable;
        ccspWifiDbgPrint(CCSP_WIFI_TRACE, "%s wlanIndex : %d ssid enabled: %d wps enable : %d\n", __FUNCTION__, wlan_index, curr_cfg->u.bss_info.enabled, curr_cfg->u.bss_info.wps.enable);
        if (curr_cfg->u.bss_info.enabled)
        {
            gradio_restart[curr_cfg->radio_index] = TRUE;
        }

    }
    else if (bForceDisableFlag == TRUE)
    {
        CcspWifiTrace(("RDK_LOG_WARN, %s %d WIFI_ATTEMPT_TO_CHANGE_CONFIG_WHEN_FORCE_DISABLED \n", __FUNCTION__, __LINE__));
    }

    if (is_tunnel)
    {
        return NULL;
    }

    if (( ((vap_cfg->u.bss_info.ssid != NULL) && (strcmp(vap_cfg->u.bss_info.ssid, curr_cfg->u.bss_info.ssid) !=0)) || (is_vap_enabled)) && (!bForceDisableFlag))
    {
        t2_event_d("WIFI_INFO_XHCofigchanged", 1);
        strncpy(curr_cfg->u.bss_info.ssid, vap_cfg->u.bss_info.ssid, sizeof(curr_cfg->u.bss_info.ssid)-1);
        ccspWifiDbgPrint(CCSP_WIFI_TRACE, "%s wlanIndex : %d Updated ssid : %s\n", __FUNCTION__, wlan_index, curr_cfg->u.bss_info.ssid);
    }
    else if (bForceDisableFlag == TRUE)
    {
        CcspWifiTrace(("RDK_LOG_WARN, %s %d WIFI_ATTEMPT_TO_CHANGE_CONFIG_WHEN_FORCE_DISABLED \n", __FUNCTION__, __LINE__));
    }

    if (vap_cfg->u.bss_info.showSsid != curr_cfg->u.bss_info.showSsid || is_vap_enabled)
    {
        if (vap_cfg->u.bss_info.enabled == TRUE)
        {
            curr_cfg->u.bss_info.showSsid = vap_cfg->u.bss_info.showSsid;
            ccspWifiDbgPrint(CCSP_WIFI_TRACE, "%s wlanIndex : %d Updated showSsid : %d\n", __FUNCTION__, wlan_index, curr_cfg->u.bss_info.ssid);
        }
    }

    if (vap_cfg->u.bss_info.isolation != curr_cfg->u.bss_info.isolation)
    {
        if (vap_cfg->u.bss_info.enabled == TRUE)
        {
            curr_cfg->u.bss_info.isolation = vap_cfg->u.bss_info.isolation;
            ccspWifiDbgPrint(CCSP_WIFI_TRACE, "%s wlanIndex : %d Updated isolation : %d\n", __FUNCTION__, wlan_index, curr_cfg->u.bss_info.isolation);
        }
        else
        {
            CcspTraceError(("%s: Isolation enable cannot be changed when vap is disabled\n",__FUNCTION__));
        }
    }

    if (vap_cfg->u.bss_info.bssMaxSta != curr_cfg->u.bss_info.bssMaxSta)
    {
        if (vap_cfg->u.bss_info.enabled == TRUE)
        {
            curr_cfg->u.bss_info.bssMaxSta = vap_cfg->u.bss_info.bssMaxSta;
            ccspWifiDbgPrint(CCSP_WIFI_TRACE, "%s wlanIndex : %d Updated bssMaxSta : %d\n", __FUNCTION__, wlan_index, curr_cfg->u.bss_info.bssMaxSta);
        }
        else
        {
            CcspTraceError(("%s: bssMaxSta cannot be applied when vap is disabled\n",__FUNCTION__));
        }
    }


    if (vap_cfg->u.bss_info.nbrReportActivated != curr_cfg->u.bss_info.nbrReportActivated)
    {
        if (vap_cfg->u.bss_info.enabled == TRUE)
        {
            curr_cfg->u.bss_info.nbrReportActivated = vap_cfg->u.bss_info.nbrReportActivated;
            ccspWifiDbgPrint(CCSP_WIFI_TRACE, "%s wlanIndex : %d Updated nbrReportActivated : %d\n", __FUNCTION__, wlan_index, curr_cfg->u.bss_info.nbrReportActivated);
        }
        else
        {
            CcspTraceError(("%s: Neighboreport cannot be applied when vap is disabled\n",__FUNCTION__));
        }
    }


    if (vap_cfg->u.bss_info.vapStatsEnable != curr_cfg->u.bss_info.vapStatsEnable)
    {
        if (vap_cfg->u.bss_info.enabled == TRUE)
        {
            if ((wifi_stats_flag_change(wlan_index, vap_cfg->u.bss_info.vapStatsEnable, 1)) != RETURN_OK) {
                CcspTraceError(("%s: Failed to set wifi stats flag for wlan %d\n",
                            __FUNCTION__, wlan_index));
                return "wifi_stats_flag_change failed";
            }
            curr_cfg->u.bss_info.vapStatsEnable = vap_cfg->u.bss_info.vapStatsEnable;
            ccspWifiDbgPrint(CCSP_WIFI_TRACE, "%s wlanIndex : %d Updated vapStatsEnable : %d\n", __FUNCTION__, wlan_index, curr_cfg->u.bss_info.vapStatsEnable);
        }
        else
        {
            CcspTraceError(("%s: Vapstats enable cannot be applied when vap is disabled\n",__FUNCTION__));
        }
    }

    if (vap_cfg->u.bss_info.bssTransitionActivated != curr_cfg->u.bss_info.bssTransitionActivated)
    {
        if (vap_cfg->u.bss_info.enabled == TRUE)
        {
            curr_cfg->u.bss_info.bssTransitionActivated = vap_cfg->u.bss_info.bssTransitionActivated;
            ccspWifiDbgPrint(CCSP_WIFI_TRACE, "%s wlanIndex : %d Updated bssTransitionActivated : %d\n", __FUNCTION__, wlan_index, curr_cfg->u.bss_info.bssTransitionActivated);
        }
        else
        {
            CcspTraceError(("%s: Bss transition cannot be applied when vap is disabled\n",__FUNCTION__));
        }
    }

    if (vap_cfg->u.bss_info.mgmtPowerControl != curr_cfg->u.bss_info.mgmtPowerControl)
    {
        if (vap_cfg->u.bss_info.enabled == TRUE)
        {
            curr_cfg->u.bss_info.mgmtPowerControl = vap_cfg->u.bss_info.mgmtPowerControl;
            ccspWifiDbgPrint(CCSP_WIFI_TRACE, "%s wlanIndex : %d Updated mgmtPowerControl : %d\n", __FUNCTION__, wlan_index, curr_cfg->u.bss_info.mgmtPowerControl);
        }
        else
        {
            CcspTraceError(("%s: Frame power control cannot be applied when vap is disabled\n",__FUNCTION__));
        }
    }

    if (vap_cfg->u.bss_info.security.mfp != curr_cfg->u.bss_info.security.mfp)
    {
        if (vap_cfg->u.bss_info.enabled == TRUE)
        {
            curr_cfg->u.bss_info.security.mfp = vap_cfg->u.bss_info.security.mfp;
            ccspWifiDbgPrint(CCSP_WIFI_TRACE, "%s wlanIndex : %d Updated mfpConfig : %s\n", __FUNCTION__, wlan_index,
                         MFPConfigOptions[curr_cfg->u.bss_info.security.mfp]);
        }
        else
        {
            CcspTraceError(("%s: MFP Config cannot be applied when vap is disabled\n",__FUNCTION__));
        }
    }
    return NULL;

#else //WIFI_HAL_VERSION_3
    int retval = RETURN_ERR;
    BOOLEAN bForceDisableFlag = FALSE;
    uint8_t wlan_index = vap_cfg->vap_index;
    BOOL enable_vap = FALSE;

    if(ANSC_STATUS_FAILURE == CosaDmlWiFiGetCurrForceDisableWiFiRadio(&bForceDisableFlag))
    {
        CcspTraceError(("%s Failed to fetch ForceDisableWiFiRadio flag!!!\n",__FUNCTION__));
        return "Dml fetch failed";
    }       

    if ((vap_cfg->u.bss_info.enabled == TRUE) && (curr_cfg->u.bss_info.enabled == TRUE)) {
	char ssid_status[64] = {0};
	if (wifi_getSSIDStatus(wlan_index, ssid_status) != RETURN_OK) {
            CcspTraceError(("%s: Failed to get SSID Status\n", __FUNCTION__));
            return "wifi_getSSIDStatus failed";
        }
        if ((strcmp(ssid_status,"Enabled")!=0) && (strcmp(ssid_status,"Up") != 0)) {
            enable_vap = TRUE;
	}
    }
    /* Apply SSID Enable/Disable */
    if (((curr_cfg->u.bss_info.enabled != vap_cfg->u.bss_info.enabled) && (!bForceDisableFlag)) || (enable_vap)) {
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
            if ((wlan_index == 4) || (wlan_index == 5)) 
                wifi_setRouterEnable(wlan_index,TRUE);

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
    if (( ((vap_cfg->u.bss_info.ssid != NULL) && (strcmp(vap_cfg->u.bss_info.ssid, curr_cfg->u.bss_info.ssid) !=0)) || (is_vap_enabled)) && 
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
#ifdef WIFI_HAL_VERSION_3
        UINT apIndex = 0;
        if ( (getVAPIndexFromName(vap_cfg->vap_name, &apIndex) == ANSC_STATUS_SUCCESS) && (isVapPrivate(apIndex)) )
        {
            SSID_UPDATED[getRadioIndexFromAp(apIndex)] = TRUE;
        }
#else
        if (strcmp(vap_cfg->vap_name, "private_ssid_2g") == 0) {
            SSID1_UPDATED = TRUE;
        } else if (strcmp(vap_cfg->vap_name, "private_ssid_5g") == 0) {
            SSID2_UPDATED = TRUE;
        }
#endif
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

#if !defined(_HUB4_PRODUCT_REQ_)
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
#if !defined(_HUB4_PRODUCT_REQ_) || defined(HUB4_WLDM_SUPPORT)
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
#if defined(WIFI_HAL_VERSION_3)
    if (vap_cfg->u.bss_info.security.mfp != curr_cfg->u.bss_info.security.mfp)
    {
        curr_cfg->u.bss_info.security.mfp = vap_cfg->u.bss_info.security.mfp;
    }
#else
    if (0 != strcmp(vap_cfg->u.bss_info.security.mfpConfig, curr_cfg->u.bss_info.security.mfpConfig)) {
        if (vap_cfg->u.bss_info.enabled == TRUE) { 
        retval = wifi_setApSecurityMFPConfig(wlan_index,vap_cfg->u.bss_info.security.mfpConfig);
        if (retval != RETURN_OK) {
            CcspTraceError(("%s: Failed to set MFP Config for wlan %d\n",
                                                         __FUNCTION__, wlan_index));
            return "wifi_setApSecurityMFPConfig failed";
        }
        int rc = -1;
        rc = strcpy_s(curr_cfg->u.bss_info.security.mfpConfig, sizeof(curr_cfg->u.bss_info.security.mfpConfig), vap_cfg->u.bss_info.security.mfpConfig);
	if (rc != 0) {
            ERR_CHK(rc);
            return "strcpy_s failed to copy mfpConfig";
        }
        CcspTraceInfo(("%s: MFP Config applied for wlan index: %d\n",
                                          __FUNCTION__, wlan_index));
        } else {
            CcspTraceError(("%s: MFP Config cannot be applied when vap is disabled\n",__FUNCTION__));
        }
    }
#endif
    return NULL; 
#endif //WIFI_HAL_VERSION_3
}

char *wifi_apply_security_config(wifi_vap_info_t *vap_cfg, wifi_vap_info_t *curr_cfg,BOOL is_vap_enabled)
{
#ifdef WIFI_HAL_VERSION_3
//Here we populate the structure curr_cfg , since we will can createVAP API to apply the settings
    BOOLEAN bForceDisableFlag = FALSE;
    uint8_t wlan_index = curr_cfg->vap_index;

    if(ANSC_STATUS_FAILURE == CosaDmlWiFiGetCurrForceDisableWiFiRadio(&bForceDisableFlag))
    {
        CcspTraceError(("%s Failed to fetch ForceDisableWiFiRadio flag!!!\n",__FUNCTION__));
        return "Dml fetch failed";
    }

    if (vap_cfg->u.bss_info.security.mode != curr_cfg->u.bss_info.security.mode)
    {
        if (vap_cfg->u.bss_info.enabled == TRUE)
        {
            curr_cfg->u.bss_info.security.mode = vap_cfg->u.bss_info.security.mode;
            ccspWifiDbgPrint(CCSP_WIFI_TRACE, "%s wlanIndex : %d Updated SecurityMode : %d\n", __FUNCTION__, wlan_index, curr_cfg->u.bss_info.security.mode);
        }
        else
        {
            CcspTraceError(("%s: Security Mode cannot be changed when vap is disabled\n",__FUNCTION__));
        }
    }

    if ((vap_cfg->u.bss_info.security.mode >= wifi_security_mode_wpa_personal) &&
            (vap_cfg->u.bss_info.security.mode <= wifi_security_mode_wpa3_enterprise) &&
            (strcmp(vap_cfg->u.bss_info.security.u.key.key,curr_cfg->u.bss_info.security.u.key.key) != 0) &&
            (!bForceDisableFlag))
    {
        if (vap_cfg->u.bss_info.enabled == TRUE)
        {
            strncpy(curr_cfg->u.bss_info.security.u.key.key,vap_cfg->u.bss_info.security.u.key.key,
                    sizeof(curr_cfg->u.bss_info.security.u.key.key)-1);
            ccspWifiDbgPrint(CCSP_WIFI_TRACE, "%s wlanIndex : %d Updated SecurityKey : %s\n", __FUNCTION__, wlan_index, curr_cfg->u.bss_info.security.u.key.key);

            if (isVapPrivate(wlan_index))
            {
                PASSPHRASE_UPDATED[getRadioIndexFromAp(wlan_index)] = TRUE;
            }
        }
        else
        {
            CcspTraceError(("%s:Passphrase cannot be changed when vap is disabled \n", __FUNCTION__));
        }
    }
    else if (bForceDisableFlag == TRUE)
    {
        CcspWifiTrace(("RDK_LOG_WARN, WIFI_ATTEMPT_TO_CHANGE_CONFIG_WHEN_FORCE_DISABLED \n"));
    }

    if ((vap_cfg->u.bss_info.security.encr != curr_cfg->u.bss_info.security.encr) &&
            (vap_cfg->u.bss_info.security.mode >= (wifi_security_modes_t)COSA_DML_WIFI_SECURITY_WPA_Personal) &&
            (vap_cfg->u.bss_info.security.mode <= (wifi_security_modes_t)COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise))
    {
        if (vap_cfg->u.bss_info.enabled == TRUE)
        {
            curr_cfg->u.bss_info.security.encr = vap_cfg->u.bss_info.security.encr;

            ccspWifiDbgPrint(CCSP_WIFI_TRACE, "%s wlanIndex : %d Updated Encryption : %d\n", __FUNCTION__, wlan_index, curr_cfg->u.bss_info.security.encr);
            curr_cfg->u.bss_info.sec_changed = true;
        }
        else
        {
            CcspTraceError(("%s:Encryption mode cannot changed when vap is disabled \n", __FUNCTION__));
        }
    }

    if ((strcmp(vap_cfg->vap_name,"hotspot_secure_2g") == 0 || strcmp(vap_cfg->vap_name,"hotspot_secure_5g") == 0) &&
            ((strcmp((const char *)vap_cfg->u.bss_info.security.u.radius.ip, (const char *)curr_cfg->u.bss_info.security.u.radius.ip) != 0 ||
              vap_cfg->u.bss_info.security.u.radius.port != curr_cfg->u.bss_info.security.u.radius.port ||
              strcmp(vap_cfg->u.bss_info.security.u.radius.key, curr_cfg->u.bss_info.security.u.radius.key) != 0) || (is_vap_enabled)))
    {
        strncpy((char*)curr_cfg->u.bss_info.security.u.radius.ip,(char*) vap_cfg->u.bss_info.security.u.radius.ip,
                sizeof(curr_cfg->u.bss_info.security.u.radius.ip)-1);
        strncpy(curr_cfg->u.bss_info.security.u.radius.key, vap_cfg->u.bss_info.security.u.radius.key,
                sizeof(curr_cfg->u.bss_info.security.u.radius.key)-1);
        curr_cfg->u.bss_info.security.u.radius.port = vap_cfg->u.bss_info.security.u.radius.port;
        ccspWifiDbgPrint(CCSP_WIFI_TRACE, "%s wlanIndex : %d Updated ip : %s key : %s port : %d\n", __FUNCTION__, wlan_index, curr_cfg->u.bss_info.security.u.radius.ip,
                curr_cfg->u.bss_info.security.u.radius.key, curr_cfg->u.bss_info.security.u.radius.port);
    }

    if ((strcmp(vap_cfg->vap_name,"hotspot_secure_2g") == 0 || strcmp(vap_cfg->vap_name,"hotspot_secure_5g") == 0) &&
            ((strcmp((const char *)vap_cfg->u.bss_info.security.u.radius.s_ip, (const char *)curr_cfg->u.bss_info.security.u.radius.s_ip) != 0 ||
              vap_cfg->u.bss_info.security.u.radius.s_port != curr_cfg->u.bss_info.security.u.radius.s_port ||
              strcmp(vap_cfg->u.bss_info.security.u.radius.s_key, curr_cfg->u.bss_info.security.u.radius.s_key) != 0) || (is_vap_enabled)))
    {
        strncpy((char*)curr_cfg->u.bss_info.security.u.radius.s_ip, (char*)vap_cfg->u.bss_info.security.u.radius.s_ip,
                sizeof(curr_cfg->u.bss_info.security.u.radius.s_ip)-1);
        strncpy(curr_cfg->u.bss_info.security.u.radius.s_key, vap_cfg->u.bss_info.security.u.radius.s_key,
                sizeof(curr_cfg->u.bss_info.security.u.radius.s_key)-1);
        curr_cfg->u.bss_info.security.u.radius.s_port = vap_cfg->u.bss_info.security.u.radius.s_port;
        ccspWifiDbgPrint(CCSP_WIFI_TRACE, "%s wlanIndex : %d Updated s_ip : %s s_key : %s s_port : %d\n", __FUNCTION__, wlan_index, curr_cfg->u.bss_info.security.u.radius.s_ip,
                curr_cfg->u.bss_info.security.u.radius.s_key, curr_cfg->u.bss_info.security.u.radius.s_port);
    }
    if (vap_cfg->u.bss_info.security.mfp != curr_cfg->u.bss_info.security.mfp)
    {
        curr_cfg->u.bss_info.security.mfp = vap_cfg->u.bss_info.security.mfp;
        ccspWifiDbgPrint(CCSP_WIFI_TRACE, "%s wlanIndex : %d mfp : %d\n",__FUNCTION__, wlan_index, curr_cfg->u.bss_info.security.mfp);
    }
    if ( (vap_cfg->u.bss_info.security.mode == wifi_security_mode_wpa3_transition) &&
         (vap_cfg->u.bss_info.security.wpa3_transition_disable != curr_cfg->u.bss_info.security.wpa3_transition_disable) )
    {
        curr_cfg->u.bss_info.security.wpa3_transition_disable = vap_cfg->u.bss_info.security.wpa3_transition_disable;
    }
    return NULL;

#else //WIFI_HAL_VERSION_3
    int retval = RETURN_ERR;
    BOOLEAN bForceDisableFlag = FALSE;
    uint8_t wlan_index = vap_cfg->vap_index;
    char mode[32] = {0};
    char securityType[32] = {0};
    char authMode[32] = {0};
    char method[32] = {0};
    char encryption[32] = {0};
    errno_t rc  =  -1;

    if(ANSC_STATUS_FAILURE == CosaDmlWiFiGetCurrForceDisableWiFiRadio(&bForceDisableFlag))
    {
        CcspTraceError(("%s Failed to fetch ForceDisableWiFiRadio flag!!!\n",__FUNCTION__));
        return "Dml fetch failed";
    }

    webconf_auth_mode_to_str(mode, vap_cfg->u.bss_info.security.mode);
    /* Copy hal specific strings for respective Authentication Mode */
    if (strcmp(mode, "None") == 0 ) {
        rc = strcpy_s(securityType, sizeof(securityType) , "None");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) , "None");
        ERR_CHK(rc);
    }
#ifndef _XB6_PRODUCT_REQ_
    else if (strcmp(mode, "WEP-64") == 0 || strcmp(mode, "WEP-128") == 0) {
        rc = strcpy_s(securityType, sizeof(securityType) , "Basic");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) , "None");
        ERR_CHK(rc);
    }
    else if (strcmp(mode, "WPA-Personal") == 0) {
        rc = strcpy_s(securityType, sizeof(securityType) , "WPA");
        ERR_CHK(rc);

        rc = strcpy_s(authMode, sizeof(authMode) , "PSKAuthentication");
        ERR_CHK(rc);

    } else if (strcmp(mode, "WPA2-Personal") == 0) {
        rc = strcpy_s(securityType, sizeof(securityType) , "11i");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) , "PSKAuthentication");
        ERR_CHK(rc);
    } else if (strcmp(mode, "WPA-Enterprise") == 0) {
    } else if (strcmp(mode, "WPA-WPA2-Personal") == 0) {
        rc = strcpy_s(securityType, sizeof(securityType) , "WPAand11i");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) , "PSKAuthentication");
        ERR_CHK(rc);
    }
#else
    else if ((strcmp(mode, "WPA2-Personal") == 0)) {
        rc = strcpy_s(securityType, sizeof(securityType) , "11i");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) , "SharedAuthentication");
        ERR_CHK(rc);
    }
#endif
    else if (strcmp(mode, "WPA2-Enterprise") == 0) {
        rc = strcpy_s(securityType, sizeof(securityType) , "11i");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) , "EAPAuthentication");
        ERR_CHK(rc);
    } else if (strcmp(mode, "WPA-WPA2-Enterprise") == 0) {
        rc = strcpy_s(securityType, sizeof(securityType) , "WPAand11i");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) , "EAPAuthentication");
        ERR_CHK(rc);
    }

    webconf_enc_mode_to_str(encryption,vap_cfg->u.bss_info.security.encr);
    if ((strcmp(encryption, "TKIP") == 0)) {
        rc = strcpy_s(method, sizeof(method) , "TKIPEncryption");
        ERR_CHK(rc);
    } else if ((strcmp(encryption, "AES") == 0)) {
        rc = strcpy_s(method, sizeof(method) , "AESEncryption");
        ERR_CHK(rc);
    }
#ifndef _XB6_PRODUCT_REQ_
    else if ((strcmp(encryption, "AES+TKIP") == 0)) {
        rc = strcpy_s(method, sizeof(method) , "TKIPandAESEncryption");
        ERR_CHK(rc);
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
        (vap_cfg->u.bss_info.security.mode <= wifi_security_mode_wpa3_transition) &&
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
#ifdef WIFI_HAL_VERSION_3
        if (isVapPrivate(wlan_index))
        {
            PASSPHRASE_UPDATED[getRadioIndexFromAp(wlan_index)] = TRUE;
        }
#else
        if (wlan_index == 0) {
            PASSPHRASE1_UPDATED = TRUE;
        } else if (wlan_index == 1) {
            PASSPHRASE2_UPDATED = TRUE;
        }
#endif
        CcspWifiEventTrace(("RDK_LOG_NOTICE, KeyPassphrase changed \n "));
        CcspWifiTrace(("RDK_LOG_WARN, KeyPassphrase changed \n "));
        CcspWifiTrace(("RDK_LOG_WARN, RDKB_WIFI_CONFIG_CHANGED : %s KeyPassphrase changed for index = %d\n",
                        __FUNCTION__, wlan_index));
        CcspTraceInfo(("%s: Passpharse change applied for wlan index %d\n", __FUNCTION__, wlan_index));
        } else {
            CcspTraceError(("%s:Passphrase cannot be changed when vap is disabled \n", __FUNCTION__));
        }
    } else if (bForceDisableFlag == TRUE) {
        CcspWifiTrace(("RDK_LOG_WARN, WIFI_ATTEMPT_TO_CHANGE_CONFIG_WHEN_FORCE_DISABLED \n"));
    }

    if ((vap_cfg->u.bss_info.security.encr != curr_cfg->u.bss_info.security.encr) &&
        (vap_cfg->u.bss_info.security.mode >= (wifi_security_modes_t)COSA_DML_WIFI_SECURITY_WPA_Personal) &&
        (vap_cfg->u.bss_info.security.mode <= (wifi_security_modes_t)COSA_DML_WIFI_SECURITY_WPA3_Enterprise)) {
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
            CcspTraceError(("%s:Encryption mode cannot changed when vap is disabled \n", __FUNCTION__));
        }
    }
    
    if (vap_cfg->u.bss_info.sec_changed) {
        CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of Security changes\n",__FUNCTION__));
        v_secure_system("/usr/bin/sysevent set wifi_ApSecurity \"RDK|%d|%s|%s|%s\"",
          wlan_index, vap_cfg->u.bss_info.security.u.key.key, mode, method);
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

#if !defined(_XB7_PRODUCT_REQ_) && !defined(_XB8_PRODUCT_REQ_) && !defined(_CBR2_PRODUCT_REQ_)
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
#endif //WIFI_HAL_VERSION_3
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
            void* vapCfgInterworking = &vap_cfg->u.bss_info.interworking.interworking; 
            retval = wifi_pushApInterworkingElement(wlan_index, vapCfgInterworking);
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

char *wifi_apply_radio_config(wifi_radio_operationParam_t *radio_cfg, wifi_radio_operationParam_t *curr_radio_cfg, uint8_t radio_index) 
{
    if (radio_cfg->enable != curr_radio_cfg->enable) {
        curr_radio_cfg->enable = radio_cfg->enable;
#ifndef WIFI_HAL_VERSION_3
        if ( RETURN_OK != wifi_setRadioEnable(radio_index, curr_radio_cfg->enable)) {
            CcspTraceError(("%s: Failed to Enable Radio %d!!!\n",__FUNCTION__, radio_index));
            return "wifi_setRadioEnable failed";
        }
#endif
        //Telemetry
        if (curr_radio_cfg->enable) {
            CcspTraceInfo(("WIFI_RADIO_%d_ENABLED \n", radio_index));
        } else {
            CcspTraceInfo(("WIFI_RADIO_%d_DISABLED \n", radio_index));
        }
    }

    return NULL;
}

int wifi_get_initial_vap_config(wifi_vap_info_t *vap_cfg, uint8_t vap_index)
{
    PCOSA_DATAMODEL_WIFI pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PSINGLE_LINK_ENTRY pSLinkEntry = NULL;
    PCOSA_DML_WIFI_SSID  pWifiSsid = NULL;
    PCOSA_DML_WIFI_AP      pWifiAp = NULL;
    errno_t  rc  = -1;
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
#if defined(WIFI_HAL_VERSION_3)
    if (strncmp(pWifiAp->SEC.Cfg.MFPConfig, "Disabled", sizeof("Disabled")) == 0) {
        vap_cfg->u.bss_info.security.mfp = wifi_mfp_cfg_disabled;
    } else if (strncmp(pWifiAp->SEC.Cfg.MFPConfig, "Required", sizeof("Required")) == 0) {
        vap_cfg->u.bss_info.security.mfp = wifi_mfp_cfg_required;
    } else if (strncmp(pWifiAp->SEC.Cfg.MFPConfig, "Optional", sizeof("Optional")) == 0) {
        vap_cfg->u.bss_info.security.mfp = wifi_mfp_cfg_optional;
    }
#else
    strncpy(vap_cfg->u.bss_info.security.mfpConfig, pWifiAp->SEC.Cfg.MFPConfig, sizeof(vap_cfg->u.bss_info.security.mfpConfig)-1);
#endif
    vap_cfg->u.bss_info.bssMaxSta = pWifiAp->AP.Cfg.BssMaxNumSta;
    vap_cfg->u.bss_info.nbrReportActivated = pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_NeighborReportActivated;
    vap_cfg->u.bss_info.vapStatsEnable = pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_StatsEnable;
    vap_cfg->u.bss_info.bssTransitionActivated = pWifiAp->AP.Cfg.BSSTransitionActivated;
    vap_cfg->u.bss_info.mgmtPowerControl = pWifiAp->AP.Cfg.ManagementFramePowerControl;

    /* Store Radius settings for Secure Hotspot vap */
    if ((vap_index == 8) || (vap_index == 9)) {
#ifndef WIFI_HAL_VERSION_3_PHASE2
        strncpy((char *)vap_cfg->u.bss_info.security.u.radius.ip, (char *)pWifiAp->SEC.Cfg.RadiusServerIPAddr,
                sizeof(vap_cfg->u.bss_info.security.u.radius.ip)-1); /*wifi hal*/
        strncpy((char *)vap_cfg->u.bss_info.security.u.radius.s_ip, (char *)pWifiAp->SEC.Cfg.SecondaryRadiusServerIPAddr,
                sizeof(vap_cfg->u.bss_info.security.u.radius.s_ip)-1); /*wifi hal*/
#else
        if(inet_pton(AF_INET, (char*)pWifiAp->SEC.Cfg.RadiusServerIPAddr, &(vap_cfg->u.bss_info.security.u.radius.ip.u.IPv4addr)) > 0) {
           vap_cfg->u.bss_info.security.u.radius.ip.family = wifi_ip_family_ipv4;
        } else if(inet_pton(AF_INET6, (char*)pWifiAp->SEC.Cfg.RadiusServerIPAddr, &(vap_cfg->u.bss_info.security.u.radius.ip.u.IPv6addr)) > 0) {
           vap_cfg->u.bss_info.security.u.radius.ip.family = wifi_ip_family_ipv6;
        } else {
          CcspWifiTrace(("RDK_LOG_ERROR,<%s> <%d> : inet_pton falied for primary radius IP\n",__FUNCTION__, __LINE__));
          return ANSC_STATUS_FAILURE;
        }

        if(inet_pton(AF_INET, (char*)pWifiAp->SEC.Cfg.SecondaryRadiusServerIPAddr, &(vap_cfg->u.bss_info.security.u.radius.s_ip.u.IPv4addr)) > 0) {
           vap_cfg->u.bss_info.security.u.radius.s_ip.family = wifi_ip_family_ipv4;
        } else if(inet_pton(AF_INET6, (char*)pWifiAp->SEC.Cfg.SecondaryRadiusServerIPAddr, &(vap_cfg->u.bss_info.security.u.radius.s_ip.u.IPv6addr)) > 0) {
           vap_cfg->u.bss_info.security.u.radius.s_ip.family = wifi_ip_family_ipv6;
        } else {
          CcspWifiTrace(("RDK_LOG_ERROR,<%s> <%d> : inet_pton falied for secondary radius IP\n",__FUNCTION__, __LINE__));
          return ANSC_STATUS_FAILURE;
        }
#endif
        vap_cfg->u.bss_info.security.u.radius.port = pWifiAp->SEC.Cfg.RadiusServerPort;
        strncpy(vap_cfg->u.bss_info.security.u.radius.key, pWifiAp->SEC.Cfg.RadiusSecret,
                sizeof(vap_cfg->u.bss_info.security.u.radius.key)-1);
        vap_cfg->u.bss_info.security.u.radius.s_port = pWifiAp->SEC.Cfg.SecondaryRadiusServerPort;
        strncpy(vap_cfg->u.bss_info.security.u.radius.s_key, pWifiAp->SEC.Cfg.SecondaryRadiusSecret,
                sizeof(vap_cfg->u.bss_info.security.u.radius.s_key)-1);
    } else if((vap_index != 4) || (vap_index != 5)) {
#ifdef WIFI_HAL_VERSION_3
            if ((pWifiAp->SEC.Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WPA3_Personal) ||
                (pWifiAp->SEC.Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WPA3_Personal_Transition) ||
                (pWifiAp->SEC.Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WPA3_Enterprise))
            {
                strncpy(vap_cfg->u.bss_info.security.u.key.key, (char*)pWifiAp->SEC.Cfg.SAEPassphrase,
                    sizeof(vap_cfg->u.bss_info.security.u.key.key)-1);
            }
            else
            {
                strncpy(vap_cfg->u.bss_info.security.u.key.key, (char *)pWifiAp->SEC.Cfg.KeyPassphrase,
                    sizeof(vap_cfg->u.bss_info.security.u.key.key)-1);
            }
#else
            strncpy(vap_cfg->u.bss_info.security.u.key.key, (char *)pWifiAp->SEC.Cfg.KeyPassphrase,
                    sizeof(vap_cfg->u.bss_info.security.u.key.key)-1);
#endif
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
        rc = strcpy_s((char *)vap_cfg->u.bss_info.interworking.anqp.anqpParameters, sizeof(vap_cfg->u.bss_info.interworking.anqp.anqpParameters), pWifiAp->AP.Cfg.IEEE80211uCfg.PasspointCfg.ANQPConfigParameters);
        ERR_CHK(rc);
    }

    if (pWifiAp->AP.Cfg.IEEE80211uCfg.PasspointCfg.HS2Parameters) {
         rc = strcpy_s((char *)vap_cfg->u.bss_info.interworking.passpoint.hs2Parameters, sizeof(vap_cfg->u.bss_info.interworking.passpoint.hs2Parameters),
             pWifiAp->AP.Cfg.IEEE80211uCfg.PasspointCfg.HS2Parameters);
         ERR_CHK(rc);
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

int wifi_get_initial_radio_config(wifi_radio_operationParam_t *curr_cfg, uint8_t radio_index)
{
    PCOSA_DATAMODEL_WIFI pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_RADIO pWifiRadio = NULL;
    PCOSA_DML_WIFI_RADIO_FULL  pWifiRadioFull = NULL;
    pWifiRadio = pMyObject->pRadio+radio_index;

    if(pWifiRadio) {
        pWifiRadioFull = &pWifiRadio->Radio;
        if(pWifiRadioFull) {
            curr_cfg->enable = pWifiRadioFull->Cfg.bEnabled;
            curr_cfg->band = pWifiRadioFull->Cfg.OperatingFrequencyBand;
            CcspTraceInfo(("%s: Fetched Initial Radio Configs\n",__FUNCTION__));
        } else {
            CcspTraceInfo(("%s: Initial Radio config fetch failed\n",__FUNCTION__));
            return RETURN_ERR;
        }
    } else {
        CcspTraceInfo(("%s: Initial Radio config fetch failed\n",__FUNCTION__));
        return RETURN_ERR;
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
    errno_t rc = -1;
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
#ifndef WIFI_HAL_VERSION_3
    pWifiSsid->SSID.Cfg.bEnabled = vap_cfg->u.bss_info.enabled;
    
    strncpy(pWifiSsid->SSID.Cfg.SSID, vap_cfg->u.bss_info.ssid, sizeof(pWifiSsid->SSID.Cfg.SSID)-1);

    pWifiAp->AP.Cfg.SSIDAdvertisementEnabled = vap_cfg->u.bss_info.showSsid;

    if ((wifi_security_modes_t)pWifiAp->SEC.Cfg.ModeEnabled != curr_cfg->u.bss_info.security.mode) {
        pWifiAp->SEC.Cfg.ModeEnabled = vap_cfg->u.bss_info.security.mode;
    }
    if ((wifi_encryption_method_t)pWifiAp->SEC.Cfg.EncryptionMethod != curr_cfg->u.bss_info.security.encr) {
        pWifiAp->SEC.Cfg.EncryptionMethod = vap_cfg->u.bss_info.security.encr;
    }
    if (pWifiAp->AP.Cfg.ManagementFramePowerControl != curr_cfg->u.bss_info.mgmtPowerControl) {
        pWifiAp->AP.Cfg.ManagementFramePowerControl = vap_cfg->u.bss_info.mgmtPowerControl;
    }
#endif
    if (pWifiAp->AP.Cfg.IsolationEnable != vap_cfg->u.bss_info.isolation) {
        pWifiAp->AP.Cfg.IsolationEnable = vap_cfg->u.bss_info.isolation;
        rc = sprintf_s(recName, sizeof(recName), ApIsolationEnable, vap_index+1);
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        rc = sprintf_s(strValue, sizeof(strValue) , "%d", ((vap_cfg->u.bss_info.isolation == TRUE) ? 1 : 0) );
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        retPsmSet = PSM_Set_Record_Value2(bus_handle, g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            CcspTraceError(("%s Failed to set Isolation enable psm value\n",__FUNCTION__));
        }
    }

    if (pWifiAp->AP.Cfg.BssMaxNumSta != (int)vap_cfg->u.bss_info.bssMaxSta) {
        pWifiAp->AP.Cfg.BssMaxNumSta = vap_cfg->u.bss_info.bssMaxSta;
        rc = sprintf_s(recName, sizeof(recName) ,BssMaxNumSta, vap_index+1);
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        rc = sprintf_s(strValue, sizeof(strValue) , "%d",pWifiAp->AP.Cfg.BssMaxNumSta);
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            CcspTraceError(("%s Failed to set Max station allowed psm value\n",__FUNCTION__));
        }
    }

    if (pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_NeighborReportActivated != vap_cfg->u.bss_info.nbrReportActivated) {
        pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_NeighborReportActivated = vap_cfg->u.bss_info.nbrReportActivated;
        rc = sprintf_s(strValue, sizeof(strValue) , "%s", (vap_cfg->u.bss_info.nbrReportActivated ? "true" : "false"));
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        rc = sprintf_s(recName, sizeof(recName) , NeighborReportActivated, vap_index + 1 );
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        retPsmSet = PSM_Set_Record_Value2(bus_handle, g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            CcspTraceError(("%s Failed to set Neighbor report activated psm value\n",__FUNCTION__));
        }
    }

    if (pWifiAp->AP.Cfg.BSSTransitionActivated != vap_cfg->u.bss_info.bssTransitionActivated) {
        pWifiAp->AP.Cfg.BSSTransitionActivated = vap_cfg->u.bss_info.bssTransitionActivated;
        rc = sprintf_s(strValue, sizeof(strValue), "%s", (vap_cfg->u.bss_info.bssTransitionActivated ? "true" : "false"));
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        rc = sprintf_s(recName, sizeof(recName) ,BSSTransitionActivated, vap_index + 1 );
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        retPsmSet = PSM_Set_Record_Value2(bus_handle, g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            CcspTraceError(("%s Failed to set Bss Transition activated psm value\n",__FUNCTION__));
        }
    }

    if (pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_rapidReconnectCountEnable != vap_cfg->u.bss_info.rapidReconnectEnable) {
        pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_rapidReconnectCountEnable =
                                vap_cfg->u.bss_info.rapidReconnectEnable;
        rc = sprintf_s(strValue, sizeof(strValue) , "%d", vap_cfg->u.bss_info.rapidReconnectEnable);
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        rc = sprintf_s(recName, sizeof(recName) , RapidReconnCountEnable, vap_index+1);
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        retPsmSet = PSM_Set_Record_Value2(bus_handle, g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            CcspTraceError(("%s Failed to set Rapid Reconnect Enable psm value\n",__FUNCTION__));
        }
    }

    if (pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_rapidReconnectMaxTime != (int)vap_cfg->u.bss_info.rapidReconnThreshold) {
        pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_rapidReconnectMaxTime = vap_cfg->u.bss_info.rapidReconnThreshold;
        rc = sprintf_s(strValue, sizeof(strValue) , "%d", vap_cfg->u.bss_info.rapidReconnThreshold);
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        rc = sprintf_s(recName, sizeof(recName) , RapidReconnThreshold, vap_index+1);
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        retPsmSet = PSM_Set_Record_Value2(bus_handle, g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            CcspTraceError(("%s Failed to set Rapid Reconnection threshold psm value\n",__FUNCTION__));
        }
    }

    if (pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_StatsEnable != vap_cfg->u.bss_info.vapStatsEnable) {
        pWifiAp->AP.Cfg.X_RDKCENTRAL_COM_StatsEnable = vap_cfg->u.bss_info.vapStatsEnable;
        rc = sprintf_s(recName, sizeof(recName) , vAPStatsEnable, vap_index+1);
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        rc = sprintf_s(strValue, sizeof(strValue) , "%s", (vap_cfg->u.bss_info.vapStatsEnable ? "true" : "false"));
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        retPsmSet = PSM_Set_Record_Value2(bus_handle, g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            CcspTraceError(("%s Failed to set Vap stats enable psm value\n",__FUNCTION__));
        }
    }

#if defined(WIFI_HAL_VERSION_3)
    if (vap_cfg->u.bss_info.security.mfp != curr_cfg->u.bss_info.security.mfp)
    {
        snprintf(pWifiAp->SEC.Cfg.MFPConfig, sizeof(pWifiAp->SEC.Cfg.MFPConfig),
                        "%s", MFPConfigOptions[vap_cfg->u.bss_info.security.mfp]);
        rc = sprintf_s(recName, sizeof(recName) , ApMFPConfig, vap_index+1);
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        retPsmSet = PSM_Set_Record_Value2(bus_handle, g_Subsystem, recName, ccsp_string, pWifiAp->SEC.Cfg.MFPConfig);
        if (retPsmSet != CCSP_SUCCESS) {
            CcspTraceError(("%s Failed to set MFPConfig  psm value\n",__FUNCTION__));
        }
    }
#else
    if(strcmp(pWifiAp->SEC.Cfg.MFPConfig, curr_cfg->u.bss_info.security.mfpConfig) != 0) {
        strncpy(pWifiAp->SEC.Cfg.MFPConfig,vap_cfg->u.bss_info.security.mfpConfig, sizeof(vap_cfg->u.bss_info.security.mfpConfig)-1);
        rc = sprintf_s(recName, sizeof(recName) ,ApMFPConfig, vap_index+1);
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        retPsmSet = PSM_Set_Record_Value2(bus_handle, g_Subsystem, recName, ccsp_string, vap_cfg->u.bss_info.security.mfpConfig);
        if (retPsmSet != CCSP_SUCCESS) {
            CcspTraceError(("%s Failed to set MFPConfig  psm value\n",__FUNCTION__));
        }
    }
#endif

#ifndef WIFI_HAL_VERSION_3
    if ((strcmp(vap_cfg->vap_name,"hotspot_secure_2g") == 0 || strcmp(vap_cfg->vap_name,"hotspot_secure_5g") == 0)) {
#ifdef WIFI_HAL_VERSION_3_PHASE2
                if(inet_ntop(AF_INET, &(vap_cfg->u.bss_info.security.u.radius.ip.u.IPv4addr), (char*)pWifiAp->SEC.Cfg.RadiusServerIPAddr,
                                            sizeof(pWifiAp->SEC.Cfg.RadiusServerIPAddr)-1) == NULL)
                {
                    CcspWifiTrace(("RDK_LOG_ERROR,<%s> <%d> : inet_ntop falied for secondary radius IP\n",__FUNCTION__, __LINE__));
                    return ANSC_STATUS_FAILURE;
                }
                else if(inet_ntop(AF_INET6, &(vap_cfg->u.bss_info.security.u.radius.ip.u.IPv6addr), (char*)pWifiAp->SEC.Cfg.RadiusServerIPAddr,
                                            sizeof(pWifiAp->SEC.Cfg.RadiusServerIPAddr)-1) == NULL)
                {
                    CcspWifiTrace(("RDK_LOG_ERROR,<%s> <%d> : inet_ntop falied for secondary radius IP\n",__FUNCTION__, __LINE__));
                    return ANSC_STATUS_FAILURE;
                }

                if(inet_ntop(AF_INET, &(vap_cfg->u.bss_info.security.u.radius.s_ip.u.IPv4addr), (char*)pWifiAp->SEC.Cfg.SecondaryRadiusServerIPAddr,
                                            sizeof(pWifiAp->SEC.Cfg.SecondaryRadiusServerIPAddr)-1) == NULL)
                {
                    CcspWifiTrace(("RDK_LOG_ERROR,<%s> <%d> : inet_ntop falied for secondary radius IP\n",__FUNCTION__, __LINE__));
                    return ANSC_STATUS_FAILURE;
                }
                else if(inet_ntop(AF_INET6, &(vap_cfg->u.bss_info.security.u.radius.s_ip.u.IPv6addr),(char*)pWifiAp->SEC.Cfg.SecondaryRadiusServerIPAddr,
                                            sizeof(pWifiAp->SEC.Cfg.SecondaryRadiusServerIPAddr)-1) == NULL)
                {
                    CcspWifiTrace(("RDK_LOG_ERROR,<%s> <%d> : inet_ntop falied for secondary radius IP\n",__FUNCTION__, __LINE__));
                    return ANSC_STATUS_FAILURE;
                }
#else
        strncpy((char *)pWifiAp->SEC.Cfg.RadiusServerIPAddr, (char *)vap_cfg->u.bss_info.security.u.radius.ip,
                sizeof(pWifiAp->SEC.Cfg.RadiusServerIPAddr)-1);
        strncpy((char *)pWifiAp->SEC.Cfg.SecondaryRadiusServerIPAddr,(char *)vap_cfg->u.bss_info.security.u.radius.s_ip,
                sizeof(pWifiAp->SEC.Cfg.SecondaryRadiusServerIPAddr)-1);
#endif
        pWifiAp->SEC.Cfg.RadiusServerPort = vap_cfg->u.bss_info.security.u.radius.port;
        strncpy(pWifiAp->SEC.Cfg.RadiusSecret,vap_cfg->u.bss_info.security.u.radius.key,
                sizeof(pWifiAp->SEC.Cfg.RadiusSecret)-1);
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
#endif
        //Update Interworking Configuration
    pWifiAp->AP.Cfg.InterworkingEnable =
        vap_cfg->u.bss_info.interworking.interworking.interworkingEnabled;
    pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iAccessNetworkType =
        vap_cfg->u.bss_info.interworking.interworking.accessNetworkType;
    pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iInternetAvailable =
        vap_cfg->u.bss_info.interworking.interworking.internetAvailable =
    pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iASRA =
        vap_cfg->u.bss_info.interworking.interworking.asra;
    pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iESR =
        vap_cfg->u.bss_info.interworking.interworking.esr;
    pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iUESA =
        vap_cfg->u.bss_info.interworking.interworking.uesa;
    pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iVenueOptionPresent =
        vap_cfg->u.bss_info.interworking.interworking.venueOptionPresent;
    pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iVenueGroup =
        vap_cfg->u.bss_info.interworking.interworking.venueGroup;
    pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iVenueType =
        vap_cfg->u.bss_info.interworking.interworking.venueType;
    pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iHESSOptionPresent =
        vap_cfg->u.bss_info.interworking.interworking.hessOptionPresent;
    strncpy(pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iHESSID,
        vap_cfg->u.bss_info.interworking.interworking.hessid,
        sizeof(pWifiAp->AP.Cfg.IEEE80211uCfg.IntwrkCfg.iHESSID)-1);

#if defined (FEATURE_SUPPORT_PASSPOINT) &&  defined(ENABLE_FEATURE_MESHWIFI)
    //Save Interworking Config to DB
    update_ovsdb_interworking(vap_cfg->vap_name,&vap_cfg->u.bss_info.interworking.interworking);
#else
    if(CosaDmlWiFi_WriteInterworkingConfig(&pWifiAp->AP.Cfg) != ANSC_STATUS_SUCCESS) {
        CcspTraceWarning(("Failed to Save Interworking Configuration\n"));
    }
#endif
    pWifiAp->AP.Cfg.IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumCount =
        vap_cfg->u.bss_info.interworking.roamingConsortium.wifiRoamingConsortiumCount;
    memcpy(&pWifiAp->AP.Cfg.IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumOui,
        &vap_cfg->u.bss_info.interworking.roamingConsortium.wifiRoamingConsortiumOui,
       sizeof(pWifiAp->AP.Cfg.IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumOui));
    memcpy(&pWifiAp->AP.Cfg.IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumLen,
        &vap_cfg->u.bss_info.interworking.roamingConsortium.wifiRoamingConsortiumLen,
       sizeof(pWifiAp->AP.Cfg.IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumLen));

    //Save Interworking, ANQP, and Passpoint Config
    if((int)ANSC_STATUS_FAILURE == CosaDmlWiFi_SaveInterworkingWebconfig(&pWifiAp->AP.Cfg, &vap_cfg->u.bss_info.interworking, vap_index)) {
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

int wifi_update_dml_radio_config(wifi_radio_operationParam_t *radio_cfg, uint8_t radio_index)
{
    PCOSA_DATAMODEL_WIFI pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_RADIO pWifiRadio = NULL;
    PCOSA_DML_WIFI_RADIO_FULL  pWifiRadioFull = NULL;

    pWifiRadio = pMyObject->pRadio+radio_index;

    if (pWifiRadio) {
        pWifiRadioFull = &pWifiRadio->Radio;
        if (pWifiRadioFull) {
            pWifiRadioFull->Cfg.bEnabled = radio_cfg->enable;
            pWifiRadioFull->Cfg.OperatingFrequencyBand = radio_cfg->band;
            CcspTraceInfo(("%s: Copied Radio Configs to TR-181\n",__FUNCTION__));
        }
    }

    return RETURN_OK;
}

int wifi_vap_cfg_rollback_handler() 
{
#ifdef WIFI_HAL_VERSION_3
    wifi_vap_info_t *tempWifiVapInfo;
#else
    wifi_vap_info_map_t current_cfg;
#endif

    uint8_t i;
    char *err = NULL;

/* TBD: Handle Wifirestart during rollback */

    CcspTraceInfo(("Inside %s\n",__FUNCTION__));
    for (i=0; i < vap_curr_cfg.num_vaps; i++) {
#ifndef WIFI_HAL_VERSION_3
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
#else //WIFI_HAL_VERSION_3
        tempWifiVapInfo = getVapInfo(vap_curr_cfg.vap_array[i].vap_index);
        if (tempWifiVapInfo == NULL)
        {
            CcspTraceError(("%s Unable to get VAP info for vapIndex : %d\n", __FUNCTION__, vap_curr_cfg.vap_array[i].vap_index));
            return RETURN_ERR;
        }

        err = wifi_apply_interworking_config(tempWifiVapInfo, &vap_curr_cfg.vap_array[i]);
#endif //WIFI_HAL_VERSION_3
        if (err != NULL) {
            CcspTraceError(("%s: Failed to apply interworking config for index %d\n",err,
                                              vap_curr_cfg.vap_array[i].vap_index));
            return RETURN_ERR;
        }
    }
#ifndef WIFI_HAL_VERSION_3
    if (wifi_apply_radio_settings() != NULL) {
        CcspTraceError(("%s: Failed to Apply Radio settings\n", __FUNCTION__));
        return RETURN_ERR;
    }
#endif //WIFI_HAL_VERSION_3

    CcspTraceInfo(("%s: Rollback applied successfully\n",__FUNCTION__));
    return RETURN_OK;
}

int wifi_update_captiveportal (char *ssid, char *password, char *vap_name) {
    parameterSigStruct_t       val = {0};
    char param_name[64] = {0};
    bool *ssid_updated, *pwd_updated;
    uint8_t wlan_index;
    errno_t rc = -1;
    if ( strcmp(notifyWiFiChangesVal,"true") != 0 ) {
        return RETURN_OK;
    }
#ifdef WIFI_HAL_VERSION_3
    UINT apIndex = 0;
    wlan_index = 2;
    pwd_updated = NULL;
    ssid_updated = NULL;
    if ( (getVAPIndexFromName(vap_name, &apIndex) == ANSC_STATUS_SUCCESS) && (isVapPrivate(apIndex)) )
    {
        ssid_updated = &SSID_UPDATED[getRadioIndexFromAp(apIndex)];
        pwd_updated = &PASSPHRASE_UPDATED[getRadioIndexFromAp(apIndex)];
        wlan_index  = apIndex + 1;
        return RETURN_ERR;
    }
#else
    if (strcmp(vap_name,"private_ssid_2g") == 0) {
        ssid_updated = &SSID1_UPDATED;
        pwd_updated = &PASSPHRASE1_UPDATED;
        wlan_index  = 1;
    } else {
        ssid_updated = &SSID2_UPDATED;
        pwd_updated = &PASSPHRASE2_UPDATED;
        wlan_index = 2;
    }
#endif
    if (*ssid_updated) {
        rc = sprintf_s(param_name, sizeof(param_name) , "Device.WiFi.SSID.%d.SSID",wlan_index);
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
        val.parameterName = param_name;
        val.newValue = ssid;
        WiFiPramValueChangedCB(&val, 0, NULL);
        *ssid_updated = FALSE;
    }

    if (*pwd_updated) {
        rc = sprintf_s(param_name, sizeof(param_name) , "Device.WiFi.AccessPoint.%d.Security.X_COMCAST-COM_KeyPassphrase",wlan_index);
        if(rc < EOK)
        {
            ERR_CHK(rc);
        }
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

#ifdef WIFI_HAL_VERSION_3
#if defined(ENABLE_FEATURE_MESHWIFI)
ANSC_STATUS notifyMeshEvents(wifi_vap_info_t *vap_cfg)
{
    char mode[32] = {0};
    char securityType[32] = {0};
    char authMode[32] = {0};
    char method[32] = {0};
    char encryption[32] = {0};
    UINT wlan_index = 0;
    errno_t rc  =  -1;
    if (vap_cfg == NULL)
    {
        CcspWifiTrace(("RDK_LOG_WARN, WIFI %s : vap_cfg is NULL", __FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }

    wlan_index = vap_cfg->vap_index;

    CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of SSID change\n",__FUNCTION__));
    v_secure_system("/usr/bin/sysevent set wifi_SSIDName \"RDK|%d|%s\"",wlan_index, vap_cfg->u.bss_info.ssid);

    CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of SSID Advertise changes\n",__FUNCTION__));
    v_secure_system("/usr/bin/sysevent set wifi_SSIDAdvertisementEnable \"RDK|%d|%s\"",
            wlan_index, vap_cfg->u.bss_info.showSsid?"true":"false");

    webconf_auth_mode_to_str(mode, vap_cfg->u.bss_info.security.mode);
    /* Copy hal specific strings for respective Authentication Mode */
    if (strcmp(mode, "None") == 0 ) {
        rc = strcpy_s(securityType, sizeof(securityType) , "None");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) , "None");
        ERR_CHK(rc);
    }
#ifndef _XB6_PRODUCT_REQ_
    else if (strcmp(mode, "WEP-64") == 0 || strcmp(mode, "WEP-128") == 0) {
        rc = strcpy_s(securityType, sizeof(securityType) , "Basic");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) , "None");
        ERR_CHK(rc);
    }
    else if (strcmp(mode, "WPA-Personal") == 0) {
        rc = strcpy_s(securityType, sizeof(securityType) ,"WPA");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) , "PSKAuthentication");
        ERR_CHK(rc);
    } else if (strcmp(mode, "WPA2-Personal") == 0) {
        rc = strcpy_s(securityType, sizeof(securityType) , "11i");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) , "PSKAuthentication");
        ERR_CHK(rc);
    } else if (strcmp(mode, "WPA-Enterprise") == 0) {
    } else if (strcmp(mode, "WPA-WPA2-Personal") == 0) {
        rc = strcpy_s(securityType, sizeof(securityType) , "WPAand11i");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) ,"PSKAuthentication");
        ERR_CHK(rc);
    }
#else
    else if ((strcmp(mode, "WPA2-Personal") == 0)) {
        rc = strcpy_s(securityType, sizeof(securityType) , "11i");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) , "SharedAuthentication");
        ERR_CHK(rc);
    }
#endif
    else if (strcmp(mode, "WPA2-Enterprise") == 0) {
        rc = strcpy_s(securityType, sizeof(securityType) , "11i");
        if (rc != 0) {
              ERR_CHK(rc);
          }
        rc = strcpy_s(authMode, sizeof(authMode) , "EAPAuthentication");
        ERR_CHK(rc);
    } else if (strcmp(mode, "WPA-WPA2-Enterprise") == 0) {
        rc = strcpy_s(securityType, sizeof(securityType) , "WPAand11i");
        ERR_CHK(rc);
        rc = strcpy_s(authMode, sizeof(authMode) , "EAPAuthentication");
        ERR_CHK(rc);
    }

    webconf_enc_mode_to_str(encryption,vap_cfg->u.bss_info.security.encr);
    if ((strcmp(encryption, "TKIP") == 0))
    {
        rc = strcpy_s(method, sizeof(method) , "TKIPEncryption");
        ERR_CHK(rc);
    }
    else if ((strcmp(encryption, "AES") == 0))
    {
        rc = strcpy_s(method, sizeof(method) , "AESEncryption");
        ERR_CHK(rc);
    }
#ifndef _XB6_PRODUCT_REQ_
    else if ((strcmp(encryption, "AES+TKIP") == 0))
    {
        rc = strcpy_s(method, sizeof(method) , "TKIPandAESEncryption");
        ERR_CHK(rc);
    }
#endif

    if (vap_cfg->u.bss_info.sec_changed)
    {
        CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of Security changes\n",__FUNCTION__));
        v_secure_system("/usr/bin/sysevent set wifi_ApSecurity \"RDK|%d|%s|%s|%s\"",
                wlan_index, vap_cfg->u.bss_info.security.u.key.key, mode, method);
    }

    vap_cfg->u.bss_info.sec_changed = FALSE;

    return ANSC_STATUS_SUCCESS;

}
#endif //ENABLE_FEATURE_MESHWIFI
#endif //WIFI_HAL_VERSION_3

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
#ifdef WIFI_HAL_VERSION_3
    UINT radioIndex = 0;
    UINT vapIndex = 0;
    UINT vapArrayIndexPerRadio = 0;
    UINT vapCount =0;
    UINT radioCount =0;
    wifi_vap_info_t *tempWifiVapInfo;
    ANSC_STATUS ret;
#endif

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

#ifndef WIFI_HAL_VERSION_3
    memset(&vap_curr_cfg, 0, sizeof(vap_curr_cfg));
    vap_curr_cfg.num_vaps = 0;
#endif //WIFI_HAL_VERSION_3

    memset(&wifi_cfg,0,sizeof(wifi_cfg));

    if (wifi_get_initial_common_config(&wifi_cfg) != RETURN_OK) {
        CcspTraceError(("%s: Failed to retrieve common wifi config\n",__FUNCTION__));
        strncpy(execRetVal->ErrorMsg, "Failed to get WiFi Config",sizeof(execRetVal->ErrorMsg)-1);
        execRetVal->ErrorCode = VALIDATION_FALIED;
        return RETURN_ERR;
    }
#ifdef WIFI_HAL_VERSION_3
    for (radioCount=0; radioCount < getNumberRadios(); radioCount++)
    {
        memset(&vap_map_per_radio[radioCount], 0, sizeof(wifi_vap_info_map_t));
    }

    for (vapCount = 0; vapCount < vap_map.num_vaps; vapCount++)
    {
        vapIndex = vap_map.vap_array[vapCount].vap_index;
        //get the RadioIndex for corresponding vap_index
        radioIndex = getRadioIndexFromAp(vapIndex);

        ccspWifiDbgPrint(CCSP_WIFI_TRACE, " %s For radioIndex %d vapIndex : %d \n", __FUNCTION__, radioIndex, vapIndex);

        vapArrayIndexPerRadio = vap_map_per_radio[radioIndex].num_vaps;

        /* Get Initial dml config */
        tempWifiVapInfo = getVapInfo(vapIndex);
        if (tempWifiVapInfo == NULL)
        {
            CcspTraceError(("%s Unable to get VAP info for vapIndex : %d\n", __FUNCTION__, vapIndex));
            strncpy(execRetVal->ErrorMsg, "getVapInfo Failed", sizeof(execRetVal->ErrorMsg)-1);
            execRetVal->ErrorCode = WIFI_HAL_FAILURE;
            return RETURN_ERR;
        }

        memcpy(&vap_map_per_radio[radioIndex].vap_array[vapArrayIndexPerRadio], tempWifiVapInfo, sizeof(wifi_vap_info_t));

        vap_enabled = FALSE;
        if (vap_map_per_radio[radioIndex].vap_array[vapArrayIndexPerRadio].u.bss_info.enabled == FALSE &&
                vap_map.vap_array[vapCount].u.bss_info.enabled == TRUE)
        {
            vap_enabled = TRUE;
        }

        /*Update ssid parameters*/
        err = wifi_apply_ssid_config(&vap_map.vap_array[vapCount], &vap_map_per_radio[radioIndex].vap_array[vapArrayIndexPerRadio], FALSE,vap_enabled);
        if (err != NULL) {
              CcspTraceError(("%s: Failed to apply ssid config for index %d\n", err,
                          vap_map.vap_array[vapCount].vap_index));
              strncpy(execRetVal->ErrorMsg, err, sizeof(execRetVal->ErrorMsg)-1);
              execRetVal->ErrorCode = WIFI_HAL_FAILURE;
              return RETURN_ERR;
        }

        /* Update security parameters*/
        err = wifi_apply_security_config(&vap_map.vap_array[vapCount], &vap_map_per_radio[radioIndex].vap_array[vapArrayIndexPerRadio], vap_enabled);
        if (err != NULL) {
              CcspTraceError(("%s: Failed to apply security config for index %d\n", err,
                          vap_map.vap_array[vapCount].vap_index));
              strncpy(execRetVal->ErrorMsg, err, sizeof(execRetVal->ErrorMsg)-1);
              execRetVal->ErrorCode = WIFI_HAL_FAILURE;
              return RETURN_ERR;
        }

        /*Update Interworking parameters */
        err = wifi_apply_interworking_config(&vap_map.vap_array[vapCount], &vap_map_per_radio[radioIndex].vap_array[vapArrayIndexPerRadio]);
        if (err != NULL) {
              CcspTraceError(("%s: Failed to apply interworking config for index %d\n", err,
                          vap_map.vap_array[vapCount].vap_index));
              strncpy(execRetVal->ErrorMsg, err, sizeof(execRetVal->ErrorMsg)-1);
              execRetVal->ErrorCode = WIFI_HAL_FAILURE;
              return RETURN_ERR;
        }

        vap_map_per_radio[radioIndex].num_vaps++;
    }

    for (radioCount = 0; radioCount < getNumberRadios(); radioCount++)
    {
        ccspWifiDbgPrint(CCSP_WIFI_TRACE, " %s For radioIndex %d num_vaps : %d to be configured\n", __FUNCTION__, radioCount, vap_map_per_radio[radioCount].num_vaps);
        //For Each Radio call the createVAP
        if (vap_map_per_radio[radioCount].num_vaps != 0)
        {
            ret =  wifi_createVAP(radioCount, &vap_map_per_radio[radioCount]);
            if (ret != ANSC_STATUS_SUCCESS)
            {
                CcspTraceError((" %s wifi_createVAP returned with error %lu\n", __FUNCTION__, ret));
                strncpy(execRetVal->ErrorMsg, "wifi_createVAP Failed", sizeof(execRetVal->ErrorMsg)-1);
                execRetVal->ErrorCode = WIFI_HAL_FAILURE;
                return RETURN_ERR;
            }

            ccspWifiDbgPrint(CCSP_WIFI_TRACE, "%s wifi_createVAP Successful for Radio : %d\n", __FUNCTION__, radioCount);

            //Update the global wifi_vap_info_t structure.
            for (vapCount = 0; vapCount < vap_map_per_radio[radioCount].num_vaps; vapCount++)
            {
                tempWifiVapInfo = getVapInfo(vap_map_per_radio[radioCount].vap_array[vapCount].vap_index);
                if (tempWifiVapInfo == NULL)
                {
                    CcspTraceError((" %s %d Unable to get VAP info for vapIndex : %d\n", __FUNCTION__, __LINE__, vap_map_per_radio[radioCount].vap_array[vapCount].vap_index));
                    strncpy(execRetVal->ErrorMsg, "getVapInfo Failed", sizeof(execRetVal->ErrorMsg)-1);
                    execRetVal->ErrorCode = WIFI_HAL_FAILURE;
                    return RETURN_ERR;
                }
                memcpy(tempWifiVapInfo, &vap_map_per_radio[radioCount].vap_array[vapCount], sizeof(wifi_vap_info_t));
#if defined(ENABLE_FEATURE_MESHWIFI)
                notifyMeshEvents(tempWifiVapInfo);
#endif //ENABLE_FEATURE_MESHWIFI
            }
        }
    }

    wifi_reset_radio();

#else //WIFI_HAL_VERSION_3
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
#endif //WIFI_HAL_VERSION_3

    err = wifi_apply_common_config(&wifi,&wifi_cfg);
    if (err != NULL) {
        CcspTraceError(("%s: Failed to apply common WiFi config\n",__FUNCTION__));
        strncpy(execRetVal->ErrorMsg,err,sizeof(execRetVal->ErrorMsg)-1);
        execRetVal->ErrorCode = WIFI_HAL_FAILURE;
    }

#ifndef WIFI_HAL_VERSION_3
    if (gHostapd_restart_reqd) {
        err = wifi_apply_radio_settings();
        if (err != NULL) {
            CcspTraceError(("%s: Failed to Apply Radio settings\n", __FUNCTION__));
            strncpy(execRetVal->ErrorMsg,err,sizeof(execRetVal->ErrorMsg)-1);
            execRetVal->ErrorCode = WIFI_HAL_FAILURE;
            return RETURN_ERR;
        }
    }
#endif //WIFI_HAL_VERSION_3

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

#ifdef WIFI_HAL_VERSION_3
        UINT apIndex = 0;
        if ( (getVAPIndexFromName(vap_map.vap_array[i].vap_name, &apIndex) == ANSC_STATUS_SUCCESS) && (isVapPrivate(apIndex)) )
#else
        if (strcmp(vap_map.vap_array[i].vap_name, "private_ssid_2g") == 0 ||
            strcmp(vap_map.vap_array[i].vap_name, "private_ssid_5g") == 0)
#endif
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

void webconfig_update_subdoc_name(execData *execData)
{
#define MAX_JSON_BUFSIZE 10240
    wifi_vap_blob_data_t *vap_data;
    const char *buf;
    size_t len;
    size_t  json_len = 0;
    msgpack_zone msg_z;
    msgpack_object msg_obj;
    cJSON *root_json;
    const cJSON *subdocname;

    char *buffer = NULL;

    if (execData == NULL) {
        return;
    }

    vap_data = (wifi_vap_blob_data_t *)execData->user_data;
    buf = (const char *) vap_data->data;
    len = vap_data->msg_size;

    if (!buf) {
        return;
    }

    msgpack_zone_init(&msg_z, MAX_JSON_BUFSIZE);
    if(MSGPACK_UNPACK_SUCCESS != msgpack_unpack(buf, len, NULL, &msg_z, &msg_obj)) {
        msgpack_zone_destroy(&msg_z);
        return;
    }

    CcspTraceInfo(("%s:Msg unpack success.\n", __FUNCTION__));

    buffer = (char*) malloc (MAX_JSON_BUFSIZE);
    if (!buffer) {
        CcspTraceError(("%s: Failed to allocate memory\n",__FUNCTION__));
        msgpack_zone_destroy(&msg_z);
        return;
    }

    memset(buffer,0,MAX_JSON_BUFSIZE);
    json_len = msgpack_object_print_jsonstr(buffer, MAX_JSON_BUFSIZE, msg_obj);
    if (json_len <= 0) {
        CcspTraceError(("%s: Msgpack to json conversion failed\n",__FUNCTION__));
        free(buffer);
        msgpack_zone_destroy(&msg_z);
        return;
    }

    buffer[json_len] = '\0';
    msgpack_zone_destroy(&msg_z);
    CcspTraceInfo(("%s:Msgpack to JSON success.\n", __FUNCTION__));

    root_json = cJSON_Parse(buffer);
    if(root_json == NULL) {
        free(buffer);
        return;
    }
    subdocname = cJSON_GetObjectItem(root_json, "subdoc_name");
    if ((subdocname == NULL) || (cJSON_IsString(subdocname) == false) || (subdocname->valuestring == NULL)) {
        CcspTraceError(("%s: Getting subdoc_name json objet fail\n",__FUNCTION__));
        free(buffer);
        cJSON_Delete(root_json);
        return;
    }
    strncpy(execData->subdoc_name, subdocname->valuestring, sizeof(execData->subdoc_name)-1);

    CcspTraceError(("%s: new doc name %s\n",__FUNCTION__, execData->subdoc_name));
    free(buffer);
    cJSON_Delete(root_json);
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

    CcspTraceInfo(("%s:Msg unpack success. Offset is %u\n", __FUNCTION__,offset));
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
                CcspTraceInfo(("Version type %d version %llu\n",map_ptr->val.type,vap_data->version));
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
        webconfig_update_subdoc_name(execDataPf);
        PushBlobRequest(execDataPf);
        CcspTraceInfo(("PushBlobRequest Complete\n"));
    }

    return RETURN_OK;
}

/**
 * Function to Parse Msg packed Wifi Radio Config
 * 
 * @param buf Pointer to the decoded string
 * @param len Length of the Decoded Message
 *
 *  returns 0 on success, error otherwise
 */

int wifi_radioConfigSet(const char *buf, size_t len, pErr execRetVal)
{
#define MAX_JSON_BUFSIZE 10240
    size_t  json_len = 0;
    msgpack_zone msg_z;
    msgpack_object msg_obj;
    msgpack_unpack_return mp_rv = 0;
    char *buffer = NULL;
    char *err = NULL;
    UINT radioCount =0;
    wifi_radio_operationParam_t radio_oper[MAX_NUM_RADIOS];
#ifdef WIFI_HAL_VERSION_3
    wifi_radio_operationParam_t *tempRadioOperParam = NULL;
    ANSC_STATUS ret;
#else
    UINT i = 0;
#endif

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

#ifdef WIFI_HAL_VERSION_3
    for (radioCount = 0;radioCount < getNumberRadios();radioCount++)
#else
    for (radioCount = 0;radioCount < 2;radioCount++)
#endif
    {
        memset(&radio_oper[radioCount], 0, sizeof(wifi_radio_operationParam_t));
    }

    if (wifi_validate_radio_config(buffer, radio_oper, execRetVal) != RETURN_OK) {
        CcspTraceError(("%s: Failed to fetch and validate radio config from json. ErrorMsg: %s\n", __FUNCTION__,execRetVal->ErrorMsg));
        execRetVal->ErrorCode = VALIDATION_FALIED;
        free(buffer);
        return RETURN_ERR;
    }

    free(buffer);


#ifdef WIFI_HAL_VERSION_3
    for (radioCount=0; radioCount < getNumberRadios(); radioCount++)
    {
        memset(&vap_map_per_radio[radioCount], 0, sizeof(wifi_vap_info_map_t));
        memset(&radio_curr_cfg[radioCount], 0, sizeof(wifi_radio_operationParam_t));

        //Get the RadioOperation  structure
        tempRadioOperParam = getRadioOperationParam(radioCount);
        if (tempRadioOperParam == NULL) {
            CcspTraceInfo(("%s: Input radioIndex = %d not found for tempRadioOperParam\n", __FUNCTION__, radioCount));
            strncpy(execRetVal->ErrorMsg, "getRadioOperationParam Failed", sizeof(execRetVal->ErrorMsg)-1);
            execRetVal->ErrorCode = WIFI_HAL_FAILURE;
            return RETURN_ERR;
        }

        memcpy(&radio_curr_cfg[radioCount], tempRadioOperParam, sizeof(wifi_radio_operationParam_t));

        /*Update Radio Parameters*/
        err = wifi_apply_radio_config(&radio_oper[radioCount], &radio_curr_cfg[radioCount], radioCount);
        if (err != NULL) {
            CcspTraceError(("%s: Failed to apply WiFi Radio config\n",__FUNCTION__));
            strncpy(execRetVal->ErrorMsg,err,sizeof(execRetVal->ErrorMsg)-1);
            execRetVal->ErrorCode = WIFI_HAL_FAILURE;
        }
    }
    for (radioCount = 0; radioCount < getNumberRadios(); radioCount++)
    {
      /*Call the HAL function wifi_setRadioOperatingParameters */
      ret = wifi_setRadioOperatingParameters(radioCount, &radio_curr_cfg[radioCount]);
      if (ret != ANSC_STATUS_SUCCESS)
      {
         CcspTraceError(("%s: wifi_setRadioOperatingParameters failed\n", __FUNCTION__));
         return RETURN_ERR;
      }
      ccspWifiDbgPrint(CCSP_WIFI_TRACE, " %s For radioIndex:%d wifi_setRadioOperatingParameters returned Success\n", __FUNCTION__, radioCount);
    }
    wifi_reset_radio();

#else //WIFI_HAL_VERSION_3

    for (i=0; i < MAX_NUM_RADIOS; i++) {
        memset(&radio_curr_cfg[i], 0, sizeof(wifi_radio_operationParam_t));
        /* Get Initial dml config */
        if (wifi_get_initial_radio_config(&radio_curr_cfg[i], i) != RETURN_OK) {
            CcspTraceError(("%s: Failed to retrieve current wifi radio config\n",__FUNCTION__));
            strncpy(execRetVal->ErrorMsg, "Failed to get WiFi Radio Config",sizeof(execRetVal->ErrorMsg)-1);
            execRetVal->ErrorCode = VALIDATION_FALIED;
            return RETURN_ERR;
        }

        /*Update Radio Parameters*/
        err = wifi_apply_radio_config(&radio_oper[i], &radio_curr_cfg[i], i);
        if (err != NULL) {
            CcspTraceError(("%s: Failed to apply WiFi Radio config\n",__FUNCTION__));
            strncpy(execRetVal->ErrorMsg,err,sizeof(execRetVal->ErrorMsg)-1);
            execRetVal->ErrorCode = WIFI_HAL_FAILURE;
        }
    }
    err = wifi_apply_radio_settings();
    if (err != NULL) {
        CcspTraceError(("%s: Failed to Apply Radio settings\n", __FUNCTION__));
        strncpy(execRetVal->ErrorMsg,err,sizeof(execRetVal->ErrorMsg)-1);
        execRetVal->ErrorCode = WIFI_HAL_FAILURE;
        return RETURN_ERR;
    }
#endif

#ifdef WIFI_HAL_VERSION_3
    for (radioCount = 0;radioCount < getNumberRadios();radioCount++)
#else
    for (radioCount = 0;radioCount < 2;radioCount++)
#endif
    {
        /* Update TR-181 params */
        if (wifi_update_dml_radio_config(&radio_curr_cfg[radioCount], radioCount) != RETURN_OK) {
            CcspTraceError(("%s: Failed to update TR-181 radio config\n",__FUNCTION__));
            strncpy(execRetVal->ErrorMsg,"Failed to update TR-181 radio config",
                    sizeof(execRetVal->ErrorMsg)-1);
            return RETURN_ERR;
        }
    }

    execRetVal->ErrorCode = BLOB_EXEC_SUCCESS;
    return RETURN_OK;
}

int wifi_radio_cfg_rollback_handler()
{
#ifdef WIFI_HAL_VERSION_3
    wifi_radio_operationParam_t *tempRadioOperParam = NULL;
#else
    wifi_radio_operationParam_t current_radio_cfg[MAX_NUM_RADIOS];
#endif

    uint8_t radio_index = -1;
    char *err = NULL;

/* TBD: Handle Wifirestart during rollback */

    CcspTraceInfo(("Inside %s\n",__FUNCTION__));

#ifndef WIFI_HAL_VERSION_3
    for (radio_index=0; radio_index < MAX_NUM_RADIOS; radio_index++) {
        if (RETURN_OK != wifi_get_initial_radio_config(&current_radio_cfg[radio_index], radio_index)) {
            CcspTraceError(("%s: Failed to fetch initial dml config\n",__FUNCTION__));
            return RETURN_ERR;
        }

        err = wifi_apply_radio_config(&current_radio_cfg[radio_index], &radio_curr_cfg[radio_index], radio_index);
        if (err != NULL) {
            CcspTraceError(("%s: Failed to apply WiFi Radio config\n",__FUNCTION__));
            return RETURN_ERR;
        }
    }
#else
    for (radio_index=0; radio_index < getNumberRadios(); radio_index++) {
        //Get the RadioOperation  structure
        tempRadioOperParam = getRadioOperationParam(radio_index);
        if (tempRadioOperParam == NULL) {
            CcspTraceInfo(("%s: Input radioIndex = %d not found for tempRadioOperParam\n", __FUNCTION__, radio_index));
            return RETURN_ERR;
        }

        /*Update Radio Parameters*/
        err = wifi_apply_radio_config(tempRadioOperParam, &radio_curr_cfg[radio_index], radio_index);
        if (err != NULL) {
            CcspTraceError(("%s: Failed to apply WiFi Radio config\n",__FUNCTION__));
            return RETURN_ERR;
        }
    }
#endif

#ifndef WIFI_HAL_VERSION_3
    if (wifi_apply_radio_settings() != NULL) {
        CcspTraceError(("%s: Failed to Apply Radio settings\n", __FUNCTION__));
        return RETURN_ERR;
    }
#endif //WIFI_HAL_VERSION_3

    CcspTraceInfo(("%s: Rollback applied successfully\n",__FUNCTION__));
    return RETURN_OK;
}

pErr wifi_radio_cfg_exec_handler(void *data)
{
    pErr execRetVal = NULL;

    if (data == NULL) {
        CcspTraceError(("%s: Input Data is NULL\n",__FUNCTION__));
        return execRetVal;
    }

    wifi_radio_blob_data_t *radio_msg = (wifi_radio_blob_data_t *) data;

    execRetVal = (pErr ) malloc (sizeof(Err));
    if (execRetVal == NULL ) {
        CcspTraceError(("%s : Failed in allocating memory for error struct\n",__FUNCTION__));
        return execRetVal;
    }
    memset(execRetVal,0,(sizeof(Err)));

    if (wifi_radioConfigSet((const char *)radio_msg->data,radio_msg->msg_size,execRetVal) == RETURN_OK) {
        CcspTraceInfo(("%s : Radio config set success\n",__FUNCTION__));
    } else {
        CcspTraceError(("%s : Radio config set Failed\n",__FUNCTION__));
    }

    return execRetVal;
}

void wifi_radio_cfg_free_resources(void *arg)
{

    CcspTraceInfo(("Entering: %s\n",__FUNCTION__));
    if (arg == NULL) {
        CcspTraceError(("%s: Input Data is NULL\n",__FUNCTION__));
        return;
    }

    execData *blob_exec_data  = (execData*) arg;

    wifi_radio_blob_data_t *radio_Data = (wifi_radio_blob_data_t *) blob_exec_data->user_data;
 
    if (radio_Data && radio_Data->data) {
        free(radio_Data->data);
        radio_Data->data = NULL;
    }

    if (radio_Data) {
        free(radio_Data);
        radio_Data = NULL;
    }

    free(blob_exec_data);
    blob_exec_data = NULL;
    CcspTraceInfo(("%s:Success in Clearing wifi radio config resources\n",__FUNCTION__));
}

int wifi_radioBlobSet(void *data)
{
    char *decoded_data = NULL;
    unsigned long msg_size = 0;
    size_t offset = 0;
    msgpack_unpacked msg;
    msgpack_unpack_return mp_rv;
    msgpack_object_map *map = NULL;
    msgpack_object_kv* map_ptr  = NULL;
    wifi_radio_blob_data_t *radio_data = NULL;
    int i = 0;

    if (data == NULL) {
        CcspTraceError(("%s: Empty Blob Input\n",__FUNCTION__));
        return RETURN_ERR;
    }

    decoded_data = (char *)AnscBase64Decode((unsigned char *)data, &msg_size);

    if (!decoded_data) {
        CcspTraceError(("%s: Failed in Decoding radioconfig blob\n",__FUNCTION__));
        return RETURN_ERR;
    }

    msgpack_unpacked_init( &msg );
    /* The outermost wrapper MUST be a map. */
    mp_rv = msgpack_unpack_next( &msg, (const char*) decoded_data, msg_size+1, &offset );
    if (mp_rv != MSGPACK_UNPACK_SUCCESS) {
        CcspTraceError(("%s: Failed to unpack wifi radio blob. Error %d",__FUNCTION__,mp_rv));
        msgpack_unpacked_destroy( &msg );
        free(decoded_data);
        return RETURN_ERR;
    }

    CcspTraceInfo(("%s:Msg unpack success. Offset is %u\n", __FUNCTION__,offset));
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

    radio_data = (wifi_radio_blob_data_t *) malloc(sizeof(wifi_radio_blob_data_t));
    if (radio_data == NULL) {
        CcspTraceError(("%s: Wifi vap data malloc error\n",__FUNCTION__));
        free(decoded_data);
        return RETURN_ERR;
    }

    /* Parsing Config Msg String to Wifi Structure */
    for (i = 0;i < (int)map->size;i++) {
        if (strncmp(map_ptr->key.via.str.ptr, "version", map_ptr->key.via.str.size) == 0) {
            if (map_ptr->val.type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
                radio_data->version = (uint64_t) map_ptr->val.via.u64;
                CcspTraceInfo(("Version type %d version %llu\n",map_ptr->val.type,radio_data->version));
            }
        }
        else if (strncmp(map_ptr->key.via.str.ptr, "transaction_id", map_ptr->key.via.str.size) == 0) {
            if (map_ptr->val.type == MSGPACK_OBJECT_POSITIVE_INTEGER) {
                radio_data->transaction_id = (uint16_t) map_ptr->val.via.u64;
                CcspTraceInfo(("Tx id type %d tx id %d\n",map_ptr->val.type,radio_data->transaction_id));
            }
        }
        ++map_ptr;
    }

    msgpack_unpacked_destroy( &msg );

    radio_data->msg_size = msg_size;
    radio_data->data = decoded_data;

    execData *execDataPf = NULL ;
    execDataPf = (execData*) malloc (sizeof(execData));
    if (execDataPf != NULL) {
        memset(execDataPf, 0, sizeof(execData));
        execDataPf->txid = radio_data->transaction_id;
        execDataPf->version = radio_data->version;
        execDataPf->numOfEntries = 2;
        strncpy(execDataPf->subdoc_name, "wifiRadioData", sizeof(execDataPf->subdoc_name)-1);
        execDataPf->user_data = (void*) radio_data;
        execDataPf->calcTimeout = webconf_ssid_timeout_handler;
        execDataPf->executeBlobRequest = wifi_radio_cfg_exec_handler;
        execDataPf->rollbackFunc = wifi_radio_cfg_rollback_handler;
        execDataPf->freeResources = wifi_radio_cfg_free_resources;
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

    char *sub_docs[SUBDOC_COUNT+1]= {"privatessid","homessid","wifiVapData","wifiRadioData",(char *) 0 };
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
