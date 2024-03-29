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
#include "cosa_apis.h"
#include "cosa_dbus_api.h"
#include "cosa_wifi_apis.h"
#include "cosa_wifi_internal.h"
#include "plugin_main_apis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <openssl/bn.h>
#include <openssl/sha.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include "collection.h"
#include "wifi_hal.h"
#include "wifi_easy_connect.h"
#include "wifi_data_plane_types.h"
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/un.h>
#include <assert.h>
#include "ansc_status.h"
#include <sysevent/sysevent.h>
#include "wifi_monitor.h"
#include "wifi_data_plane.h"
#include "safec_lib_common.h"

#if !defined(_BWG_PRODUCT_REQ_)
#if !defined(_XF3_PRODUCT_REQ_) && !defined(_CBR_PRODUCT_REQ_) && !defined(_HUB4_PRODUCT_REQ_) && !defined(_XB7_PRODUCT_REQ_) && !defined(_PLATFORM_TURRIS_) && !defined(_PLATFORM_RASPBERRYPI_)
static const char *wifi_health_log = "/rdklogs/logs/wifihealth.txt";

extern bool is_device_associated(int ap_index, char *mac);
static wifi_easy_connect_t g_easy_connect = {0};

PCOSA_DML_WIFI_DPP_STA_CFG find_dpp_sta_dml_wifi_ap(unsigned int ap_index, mac_address_t sta_mac);
PCOSA_DML_WIFI_DPP_CFG find_dpp_dml_wifi_ap(unsigned int ap_index);
INT wifi_dppProcessAuthResponse(wifi_device_dpp_context_t *dpp_ctx);
INT wifi_dppProcessConfigRequest(wifi_device_dpp_context_t *ctx);
INT wifi_dppProcessConfigResult(wifi_device_dpp_context_t *dpp_ctx);
int wifi_dppReconfigInitiate(wifi_device_dpp_context_t *ctx);
INT wifi_dppProcessReconfigAuthResponse(wifi_device_dpp_context_t *dpp_ctx);
int wifi_dppSendReconfigAuthCnf(wifi_device_dpp_context_t *dpp_ctx);

static void wifi_easy_connect_dbg_print(int level, char *format, ...)
{
    char buff[2048] = {0};
    va_list list;
    static FILE *fpg = NULL;
    UNREFERENCED_PARAMETER(level);
    errno_t rc = -1;

    if ((access("/nvram/wifiDppDbg", R_OK)) != 0) {
        return;
    }

    get_formatted_time(buff);
    rc = strcat_s(buff, sizeof(buff), " ");
    ERR_CHK(rc);

    va_start(list, format);
    vsprintf(&buff[strlen(buff)], format, list);
    va_end(list);

    if (fpg == NULL) {
        fpg = fopen("/tmp/wifiDPP", "a+");
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

static char *to_mac_str    (mac_address_t mac, mac_addr_str_t key) {
    snprintf(key, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return (char *)key;
}


static void to_mac_bytes   (mac_addr_str_t key, mac_address_t bmac) {
   unsigned int mac[6];
    sscanf(key, "%02x:%02x:%02x:%02x:%02x:%02x",
             &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
   bmac[0] = mac[0]; bmac[1] = mac[1]; bmac[2] = mac[2];
   bmac[3] = mac[3]; bmac[4] = mac[4]; bmac[5] = mac[5];

}

void end_device_provisioning    (wifi_device_dpp_context_t *ctx)
{

    wifi_dppCancel(ctx);

    free(ctx);
    ctx = NULL;

}

void log_dpp_diagnostics(char *format, ...)
{
#define BUFF_SIZE 512
    char buff[BUFF_SIZE] = {0};
	va_list list;
    
    va_start(list, format);
    vsprintf(buff, format, list);
    va_end(list);
                
    write_to_file(wifi_health_log, buff);
}

static char *acti_status[] = {
    "ActStatus_Idle",
    "ActStatus_Config_Error",
    "ActStatus_In_Progress",
    "ActStatus_No_Response",
    "ActStatus_Failed",
    "ActStatus_OK"
};

static char *resp_status[] = {
    "RESPONDER_STATUS_OK", 
    "RESPONDER_STATUS_NOT_COMPATIBLE", 
    "RESPONDER_STATUS_AUTH_FAILURE", 
    "RESPONDER_STATUS_BAD_CODE", 
    "RESPONDER_STATUS_BAD_GROUP", 
    "RESPONDER_STATUS_CONFIGURATION_FAILURE", 
    "RESPONDER_STATUS_RESPONSE_PENDING", 
    "RESPONDER_STATUS_INVALID_CONNECTOR", 
    "RESPONDER_STATUS_NO_MATCH", 
    "RESPONDER_STATUS_CONFIG_REJECTED", 
    "RESPONDER_STATUS_NOT_AVAILABLE"
};

void set_dpp_device_context_states(wifi_device_dpp_context_t *ctx, wifi_dpp_state_t state, 
					wifi_activation_status_t activation_status, wifi_enrollee_responder_status_t enrollee_status, 
					PCOSA_DML_WIFI_DPP_STA_CFG pWifiDppSta)
{
	ctx->session_data.state = state;
    ctx->activation_status = activation_status;
    ctx->enrollee_status = enrollee_status;
    errno_t rc = -1;
    rc = strcpy_s((char*)pWifiDppSta->ActivationStatus, sizeof(pWifiDppSta->ActivationStatus), acti_status[activation_status]);
    ERR_CHK(rc);
    rc = strcpy_s((char*)pWifiDppSta->EnrolleeResponderStatus, sizeof(pWifiDppSta->EnrolleeResponderStatus), resp_status[enrollee_status]);
    ERR_CHK(rc);
}

void process_easy_connect_event(wifi_device_dpp_context_t *ctx, wifi_easy_connect_t *module)
{
    UNREFERENCED_PARAMETER(module);
    int rc;
    int safe_rc = -1;
    ssid_t ssid;
    PCOSA_DML_WIFI_DPP_CFG pWifiDppCfg;
    PCOSA_DML_WIFI_DPP_STA_CFG pWifiDppSta = NULL;
    char passphrase[64] = {0x0};
    pWifiDppCfg = find_dpp_dml_wifi_ap(ctx->ap_index);
    if (pWifiDppCfg == NULL) {
        wifi_easy_connect_dbg_print(1, "%s:%d: Could not find dpp config in database\n", __func__, __LINE__);
        return;
        }
    wifi_easy_connect_dbg_print(1, "%s:%d: Found dpp config in database\n", __func__, __LINE__);
    // check if the STA was provisioned by us
    pWifiDppSta = find_dpp_sta_dml_wifi_ap(ctx->ap_index, ctx->session_data.sta_mac);
    if (pWifiDppSta == NULL) {
        return;
        }
            if (ctx->session_data.state == STATE_DPP_UNPROVISIONED) { 
                if (wifi_dppInitiate(ctx) == RETURN_OK) {
                    wifi_easy_connect_dbg_print(1, "%s:%d: DPP Authentication Request Frame send success\n", __func__, __LINE__);
                    set_dpp_device_context_states(ctx, STATE_DPP_AUTH_RSP_PENDING, 
                            ActStatus_In_Progress, RESPONDER_STATUS_RESPONSE_PENDING, pWifiDppSta);
                    log_dpp_diagnostics("Wifi DPP: STATE_DPP_AUTH_RSP_PENDING\n");
                } else {
                    wifi_easy_connect_dbg_print(1, "%s:%d: DPP Authentication Request Frame send failed\n", __func__, __LINE__);
                    ctx->dpp_init_retries++;
                }
                data_plane_queue_push(data_plane_queue_create_event(ctx,wifi_data_plane_event_type_dpp, FALSE)); //need to pass NoLock
            } else if (ctx->session_data.state == STATE_DPP_AUTH_RSP_PENDING) {
                if (ctx->type == dpp_context_type_received_frame_auth_rsp) {
                    wifi_easy_connect_dbg_print(1, "%s:%d: Sending DPP Authentication Cnf ... \n", __func__, __LINE__);
                    rc = wifi_dppProcessAuthResponse(ctx);
                    ctx->type = dpp_context_type_session_data;
                    free(ctx->received_frame.frame);
                    ctx->received_frame.length = 0;
					if (rc == RETURN_OK) {
                        rc = wifi_dppSendAuthCnf(ctx);
                    if (rc == RETURN_OK) {
                        set_dpp_device_context_states(ctx, STATE_DPP_AUTHENTICATED, 
                                ActStatus_In_Progress, RESPONDER_STATUS_RESPONSE_PENDING, pWifiDppSta);
                        log_dpp_diagnostics("%s MAC: %s\n", "Wifi DPP: STATE_DPP_AUTHENTICATED", pWifiDppSta->ClientMac);
                        data_plane_queue_push(data_plane_queue_create_event(ctx,wifi_data_plane_event_type_dpp, FALSE)); //need to pass NoLock
                    } else {
                        set_dpp_device_context_states(ctx, STATE_DPP_AUTH_FAILED, 
                                ActStatus_No_Response, RESPONDER_STATUS_AUTH_FAILURE, pWifiDppSta);
                        end_device_provisioning(ctx);
                        log_dpp_diagnostics("%s MAC: %s\n", "Wifi DPP: RESPONSE PENDING FAILURE", pWifiDppSta->ClientMac);
                    }
                    } else {
                        set_dpp_device_context_states(ctx, STATE_DPP_AUTH_FAILED, 
                                ActStatus_No_Response, RESPONDER_STATUS_AUTH_FAILURE, pWifiDppSta);
                        end_device_provisioning(ctx);
                        log_dpp_diagnostics("%s MAC: %s\n", "Wifi DPP: RESPONSE PENDING FAILURE", pWifiDppSta->ClientMac);
                    }
                }
            } else if (ctx->session_data.state == STATE_DPP_AUTHENTICATED) {
                if ((ctx->type == dpp_context_type_received_frame_cfg_req) && (wifi_dppProcessConfigRequest(ctx) == RETURN_OK)) {
                    ctx->config.wifiTech = WIFI_DPP_TECH_INFRA;
		    /*TODO CID: 160007 Out-of-bounds access - Fix in QTN code*/
                    wifi_getSSIDName(ctx->ap_index, ssid);
                    /*CID: 160016 BUFFER_SIZE_WARNING*/
                    safe_rc = strcpy_s(ctx->config.discovery, sizeof(ctx->config.discovery), ssid);
		    if (safe_rc != 0) {
                        ERR_CHK(safe_rc);
			return;
		    }
                    ctx->config.discovery[sizeof(ctx->config.discovery)-1] = '\0';
                    wifi_getApSecurityKeyPassphrase(ctx->ap_index, passphrase);
                    safe_rc = strcpy_s(ctx->config.credentials.creds.passPhrase, sizeof(ctx->config.credentials.creds.passPhrase), passphrase);
		    if (safe_rc != 0) {
                        ERR_CHK(safe_rc);
			return;
		    }
                    wifi_easy_connect_dbg_print(1, "%s:%d: Sending DPP Config Rsp ... ssid: %s passphrase: %s\n", __func__, __LINE__,
                            ctx->config.discovery, ctx->config.credentials.creds.passPhrase);
                    rc = wifi_dppSendConfigResponse(ctx);
                    ctx->type = dpp_context_type_session_data;
                    free(ctx->received_frame.frame);
                    ctx->received_frame.length = 0;
                    if (rc == RETURN_OK) {
                        set_dpp_device_context_states(ctx, STATE_DPP_CFG_RSP_SENT, 
                                ActStatus_In_Progress, RESPONDER_STATUS_RESPONSE_PENDING, pWifiDppSta);
                        log_dpp_diagnostics("Wifi DPP: STATE_DPP_CFG_RSP_SENT\n");
                        data_plane_queue_push(data_plane_queue_create_event(ctx,wifi_data_plane_event_type_dpp, FALSE)); //need to pass NoLock
                    } else {
                        set_dpp_device_context_states(ctx, STATE_DPP_CFG_FAILED, 
                                ActStatus_Config_Error, RESPONDER_STATUS_CONFIGURATION_FAILURE, pWifiDppSta);
                        log_dpp_diagnostics("%s MAC: %s\n", "Wifi DPP: RESPONDER_STATUS_CONFIGURATION_FAILURE", pWifiDppSta->ClientMac);
                        pWifiDppSta->Activate = FALSE;
                        end_device_provisioning(ctx);
                    }
                }
            } else if (ctx->session_data.state == STATE_DPP_CFG_RSP_SENT) {
                if (ctx->type == dpp_context_type_received_frame_cfg_result) {
                    rc = wifi_dppProcessConfigResult(ctx);
                    ctx->type = dpp_context_type_session_data;
                    free(ctx->received_frame.frame);
                    ctx->received_frame.length = 0;
                    if (rc == RETURN_OK) {
                        set_dpp_device_context_states(ctx, STATE_DPP_PROVISIONED, 
                                ActStatus_OK, RESPONDER_STATUS_OK, pWifiDppSta);
                        pWifiDppSta->Activate = FALSE;
                        log_dpp_diagnostics("Wifi DPP: RESPONDER_STATUS_OK\n");
                        end_device_provisioning(ctx);
                    } else {
                        set_dpp_device_context_states(ctx, STATE_DPP_CFG_FAILED, 
                                ActStatus_Config_Error, RESPONDER_STATUS_CONFIG_REJECTED, pWifiDppSta);
                        pWifiDppSta->Activate = FALSE;
                        log_dpp_diagnostics("%s MAC: %s\n", "Wifi DPP: RESPONDER_STATUS_CONFIG_REJECTED", pWifiDppSta->ClientMac);
                        end_device_provisioning(ctx);
                    }
                }
            } else if (ctx->session_data.state == STATE_DPP_PROVISIONED) {
                if (ctx->type == dpp_context_type_received_frame_recfg_announce) {
                    wifi_easy_connect_dbg_print(1, "%s:%d: Trying to send DPP Reconfig Authentication Request\n", __func__, __LINE__);
                    if (wifi_dppReconfigInitiate(ctx) == RETURN_OK) {
                        wifi_easy_connect_dbg_print(1, "%s:%d: DPP Reconfig Authentication Request Frame send success\n", __func__, __LINE__);
                        log_dpp_diagnostics("Wifi DPP: STATE_DPP_RECFG_AUTH_RSP_PENDING\n");
                    } else {
                        wifi_easy_connect_dbg_print(1, "%s:%d: DPP Authentication Request Frame send failed\n", __func__, __LINE__);
                        ctx->dpp_init_retries++;
                    }
                    set_dpp_device_context_states(ctx, STATE_DPP_RECFG_AUTH_RSP_PENDING, 
                            ActStatus_In_Progress, RESPONDER_STATUS_RESPONSE_PENDING, pWifiDppSta);
                    data_plane_queue_push(data_plane_queue_create_event(ctx,wifi_data_plane_event_type_dpp, FALSE)); //need to pass NoLock
                }
            } else if (ctx->session_data.state == STATE_DPP_RECFG_AUTH_RSP_PENDING) {
                if (ctx->type == dpp_context_type_received_frame_recfg_auth_rsp) {
                    rc = wifi_dppProcessReconfigAuthResponse(ctx);
                    ctx->type = dpp_context_type_session_data;
                    free(ctx->received_frame.frame);
                    ctx->received_frame.length = 0;
                    if (rc == RETURN_OK) {	
                        wifi_easy_connect_dbg_print(1, "%s:%d: Sending DPP Authentication Cnf ... \n", __func__, __LINE__);
                        rc = wifi_dppSendReconfigAuthCnf(ctx);
                        if (rc == RETURN_OK) {
                            set_dpp_device_context_states(ctx, STATE_DPP_AUTHENTICATED,
                                    ActStatus_In_Progress, RESPONDER_STATUS_RESPONSE_PENDING, pWifiDppSta);
                            log_dpp_diagnostics("%s MAC: %s\n", "Wifi DPP: STATE_DPP_AUTHENTICATED", pWifiDppSta->ClientMac);
                            data_plane_queue_push(data_plane_queue_create_event(ctx,wifi_data_plane_event_type_dpp, FALSE)); //need to pass NoLock
                        } else {
                            set_dpp_device_context_states(ctx, STATE_DPP_RECFG_AUTH_FAILED,
                                    ActStatus_No_Response, RESPONDER_STATUS_AUTH_FAILURE, pWifiDppSta);
                            end_device_provisioning(ctx);
                            log_dpp_diagnostics("%s MAC: %s\n", "Wifi DPP: RESPONSE PENDING FAILURE", pWifiDppSta->ClientMac);
                        }
                    } else {
                        set_dpp_device_context_states(ctx, STATE_DPP_RECFG_AUTH_FAILED,
                                ActStatus_No_Response, RESPONDER_STATUS_AUTH_FAILURE, pWifiDppSta);
                        end_device_provisioning(ctx);
                        log_dpp_diagnostics("%s MAC: %s\n", "Wifi DPP: RESPONSE PENDING FAILURE", pWifiDppSta->ClientMac);
                    }
                }
            }
}

void process_easy_connect_event_timeout(wifi_device_dpp_context_t *ctx, wifi_easy_connect_t *module)
{
    UNREFERENCED_PARAMETER(module);
    PCOSA_DML_WIFI_DPP_CFG pWifiDppCfg;
    PCOSA_DML_WIFI_DPP_STA_CFG pWifiDppSta = NULL;
    mac_addr_str_t mac_str;
    int next_ch = 0;
    pWifiDppCfg = find_dpp_dml_wifi_ap(ctx->ap_index);
    if (pWifiDppCfg == NULL) {
        wifi_easy_connect_dbg_print(1, "%s:%d: Could not find dpp config in database\n", __func__, __LINE__);
        return;
    }
    wifi_easy_connect_dbg_print(1, "%s:%d: Found dpp config in database\n", __func__, __LINE__);
    // check if the STA was provisioned by us
    pWifiDppSta = find_dpp_sta_dml_wifi_ap(ctx->ap_index, ctx->session_data.sta_mac);
    if (pWifiDppSta == NULL) {
        return;
    }
            wifi_easy_connect_dbg_print(1, "%s:%d: DPP context state:%d DPP init retries:%d Max retries:%d\n", __func__, __LINE__,
                    ctx->session_data.state, ctx->dpp_init_retries, ctx->max_retries);
            if ((ctx->session_data.state == STATE_DPP_AUTH_RSP_PENDING) || (ctx->session_data.state == STATE_DPP_UNPROVISIONED)) {
                if (ctx->dpp_init_retries < ctx->max_retries) {
                    wifi_easy_connect_dbg_print(1, "%s:%d: Trying to send DPP Authentication Request Frame ... \n", __func__, __LINE__);
                    if (wifi_dppInitiate(ctx) == RETURN_OK) {
                        set_dpp_device_context_states(ctx, STATE_DPP_AUTH_RSP_PENDING, 
                                ActStatus_No_Response, RESPONDER_STATUS_RESPONSE_PENDING, pWifiDppSta);
                        log_dpp_diagnostics("Wifi DPP: STATE_DPP_AUTH_RSP_PENDING\n");
                        wifi_easy_connect_dbg_print(1, "%s:%d: DPP Authentication Request Frame send success\n", __func__, __LINE__);
                    } else {
                        wifi_easy_connect_dbg_print(1, "%s:%d: DPP Authentication Request Frame send failed\n", __func__, __LINE__);
                    } 
                    ctx->dpp_init_retries++;
                    data_plane_queue_push(data_plane_queue_create_event(ctx,wifi_data_plane_event_type_dpp, FALSE)); //need to pass NoLock
                } else if ((next_ch = find_best_dpp_channel(ctx)) != -1) {
                    ctx->dpp_init_retries = 0;
                    ctx->session_data.channel = next_ch;
                    if (wifi_dppInitiate(ctx) == RETURN_OK) {
                        set_dpp_device_context_states(ctx, STATE_DPP_AUTH_RSP_PENDING, 
                                ActStatus_No_Response, RESPONDER_STATUS_RESPONSE_PENDING, pWifiDppSta);
                        log_dpp_diagnostics("Wifi DPP: STATE_DPP_AUTH_RSP_PENDING\n");
                        wifi_easy_connect_dbg_print(1, "%s:%d: DPP Authentication Request Frame send success\n", __func__, __LINE__);
                    } else {
                        wifi_easy_connect_dbg_print(1, "%s:%d: DPP Authentication Request Frame send failed\n", __func__, __LINE__);
                    }
                    ctx->dpp_init_retries++;
                    data_plane_queue_push(data_plane_queue_create_event(ctx,wifi_data_plane_event_type_dpp, FALSE)); //need to pass NoLock
                } else {
                    set_dpp_device_context_states(ctx, STATE_DPP_AUTH_FAILED, 
                            ActStatus_Failed, RESPONDER_STATUS_AUTH_FAILURE, pWifiDppSta);
                    log_dpp_diagnostics("%s MAC: %s\n", "Wifi DPP: RESPONDER_STATUS_AUTH_FAILURE", pWifiDppSta->ClientMac);
                    pWifiDppSta->Activate = FALSE;
                    end_device_provisioning(ctx);
                }
            } else if (ctx->session_data.state == STATE_DPP_RECFG_AUTH_RSP_PENDING) {
                if (ctx->dpp_init_retries < ctx->max_retries) {
                    wifi_easy_connect_dbg_print(1, "%s:%d: Trying to send DPP Reconfig Authentication Request Frame ... \n", __func__, __LINE__);
                    ctx->dpp_init_retries++;
                    if (wifi_dppReconfigInitiate(ctx) == RETURN_OK) {
                        log_dpp_diagnostics("Wifi DPP: STATE_DPP_RECFG_AUTH_RSP_PENDING\n");
                        wifi_easy_connect_dbg_print(1, "%s:%d: DPP Reconfig Authentication Request Frame send success\n", __func__, __LINE__);
                    } else {
                        wifi_easy_connect_dbg_print(1, "%s:%d: DPP Reconfig Authentication Request Frame send failed\n", __func__, __LINE__);
                    } 
                    set_dpp_device_context_states(ctx, STATE_DPP_RECFG_AUTH_RSP_PENDING, 
                            ActStatus_No_Response, RESPONDER_STATUS_RESPONSE_PENDING, pWifiDppSta);
                    ctx->session_data.state = STATE_DPP_RECFG_AUTH_RSP_PENDING;
                    data_plane_queue_push(data_plane_queue_create_event(ctx,wifi_data_plane_event_type_dpp, FALSE)); //need to pass NoLock
                } else {
                    set_dpp_device_context_states(ctx, STATE_DPP_RECFG_AUTH_FAILED, 
                            ActStatus_Failed, RESPONDER_STATUS_AUTH_FAILURE, pWifiDppSta);
                    log_dpp_diagnostics("%s MAC: %s\n", "Wifi DPP: RESPONDER_STATUS_AUTH_FAILURE", pWifiDppSta->ClientMac);
                    pWifiDppSta->Activate = FALSE;
                    end_device_provisioning(ctx);
                }
            } else if (ctx->session_data.state == STATE_DPP_AUTHENTICATED) {
                // DPP Config Request never arrived
                if (ctx->check_for_config_requested >= ctx->max_retries/*5*/) {
                    set_dpp_device_context_states(ctx, STATE_DPP_UNPROVISIONED, 
                            ActStatus_Failed, RESPONDER_STATUS_AUTH_FAILURE, pWifiDppSta);
                    log_dpp_diagnostics("%s MAC: %s\n", "Wifi DPP: RESPONDER_STATUS_AUTH_FAILURE", pWifiDppSta->ClientMac);
                    pWifiDppSta->Activate = FALSE;
                    end_device_provisioning(ctx);
                } else {
                    ctx->check_for_config_requested++;
                    set_dpp_device_context_states(ctx, STATE_DPP_AUTHENTICATED, 
                            ActStatus_In_Progress, RESPONDER_STATUS_RESPONSE_PENDING, pWifiDppSta);
                    data_plane_queue_push(data_plane_queue_create_event(ctx,wifi_data_plane_event_type_dpp, FALSE)); //need to pass NoLock
                }
            } else if (ctx->session_data.state == STATE_DPP_CFG_RSP_SENT) {
                // now start checking for associated state on the vap index
                if ((ctx->enrollee_version == 1)) { /* configurator shall support both 2.0 & 1.0, hence check only enrollee */
                    to_mac_str(ctx->session_data.sta_mac, mac_str);
                    if (wifi_api_is_device_associated(ctx->ap_index, mac_str) == TRUE) {
                        set_dpp_device_context_states(ctx, STATE_DPP_PROVISIONED, 
                                ActStatus_OK, RESPONDER_STATUS_OK, pWifiDppSta);
                        pWifiDppSta->Activate = FALSE;
                        log_dpp_diagnostics("Wifi DPP: RESPONDER_STATUS_OK\n");
                        end_device_provisioning(ctx);
                    } else if (ctx->check_for_associated >= ctx->max_retries/*5*/) {
                        set_dpp_device_context_states(ctx, STATE_DPP_UNPROVISIONED, 
                                ActStatus_Config_Error, RESPONDER_STATUS_CONFIG_REJECTED, pWifiDppSta);
                        pWifiDppSta->Activate = FALSE;
                        log_dpp_diagnostics("%s MAC: %s\n", "Wifi DPP: RESPONDER_STATUS_CONFIG_REJECTED", pWifiDppSta->ClientMac);
                        end_device_provisioning(ctx);
                    } else {
                        ctx->check_for_associated++;
                        data_plane_queue_push(data_plane_queue_create_event(ctx,wifi_data_plane_event_type_dpp, FALSE)); //need to pass NoLock
                    }
                }
            } else if ((ctx->session_data.state == STATE_DPP_AUTH_FAILED) || (ctx->session_data.state == STATE_DPP_RECFG_AUTH_FAILED)) {
                // Authentication Cnf send failure
                set_dpp_device_context_states(ctx, STATE_DPP_UNPROVISIONED, 
                        ActStatus_Failed, RESPONDER_STATUS_AUTH_FAILURE, pWifiDppSta);
                pWifiDppSta->Activate = FALSE;
                log_dpp_diagnostics("%s MAC: %s\n", "Wifi DPP: RESPONDER_STATUS_AUTH_FAILURE", pWifiDppSta->ClientMac);
                end_device_provisioning(ctx);
            }
}

bool is_matching_easy_connect_event(wifi_device_dpp_context_t *ctx, void *ptr)
{
    wifi_easy_connect_event_match_criteria_t *criteria = (wifi_easy_connect_event_match_criteria_t *) ptr;
    if ((ctx->ap_index != criteria->apIndex) || (memcmp(ctx->session_data.sta_mac, criteria->sta_mac, sizeof(mac_address_t)) != 0)) {
        return false;
        }
    if (ctx->session_data.state != criteria->state) {
        return false;
    }
    return true;
}

void dppAuthResponse_callback(UINT apIndex, mac_address_t sta, unsigned char *frame, unsigned int len)
{
    wifi_device_dpp_context_t *ctx = NULL;
    wifi_easy_connect_event_match_criteria_t criteria;

    criteria.apIndex = apIndex;
    memcpy(criteria.sta_mac, sta, sizeof(mac_address_t));
    criteria.state = STATE_DPP_AUTH_RSP_PENDING;

    wifi_easy_connect_dbg_print(1, "%s:%d apIndex=%d mac=%02x:%02x:%02x:%02x:%02x:%02x len=%d\n", __func__, __LINE__, apIndex, sta[0], sta[1], sta[2], sta[3], sta[4], sta[5], len);
    ctx = (wifi_device_dpp_context_t *)data_plane_queue_remove_event(wifi_data_plane_event_type_dpp, &criteria);

    if (ctx == NULL) {
        wifi_easy_connect_dbg_print(1, "%s:%d ctx NULL\n", __func__, __LINE__);
        return;
    }

    ctx->type = dpp_context_type_received_frame_auth_rsp;
    ctx->received_frame.frame = malloc(len);
    memcpy(ctx->received_frame.frame, frame, len);

    ctx->received_frame.length = len;

    data_plane_queue_push(data_plane_queue_create_event(ctx,wifi_data_plane_event_type_dpp, TRUE));
}

void dppConfigRequest_callback(UINT apIndex, mac_address_t sta, UCHAR token, UCHAR *configAttributes, UINT len)
{
    wifi_device_dpp_context_t *ctx = NULL;
    wifi_easy_connect_event_match_criteria_t criteria;

    criteria.apIndex = apIndex;
    memcpy(criteria.sta_mac, sta, sizeof(mac_address_t));
    criteria.state = STATE_DPP_AUTHENTICATED;

    wifi_easy_connect_dbg_print(1, "%s:%d apIndex=%d mac=%02x:%02x:%02x:%02x:%02x:%02x len=%d\n", __func__, __LINE__, apIndex, sta[0], sta[1], sta[2], sta[3], sta[4], sta[5], len);
    ctx = (wifi_device_dpp_context_t *)data_plane_queue_remove_event(wifi_data_plane_event_type_dpp, &criteria);

    if (ctx == NULL) {
        wifi_easy_connect_dbg_print(1, "%s:%d ctx NULL\n", __func__, __LINE__);
        return;
    }

    ctx->type = dpp_context_type_received_frame_cfg_req;
    ctx->received_frame.frame = malloc(len);
    memcpy(ctx->received_frame.frame, configAttributes, len);
    ctx->received_frame.length = len; // add length
    ctx->token = token;

    data_plane_queue_push(data_plane_queue_create_event(ctx,wifi_data_plane_event_type_dpp, TRUE));
}

void dppConfigResult_callback(UINT apIndex, mac_address_t sta, UCHAR *frame, UINT len)
{
    wifi_device_dpp_context_t *ctx = NULL;
    wifi_easy_connect_event_match_criteria_t criteria;

    criteria.apIndex = apIndex;
    memcpy(criteria.sta_mac, sta, sizeof(mac_address_t));
    criteria.state = STATE_DPP_CFG_RSP_SENT;

    wifi_easy_connect_dbg_print(1, "%s:%d apIndex=%d mac=%02x:%02x:%02x:%02x:%02x:%02x len=%d\n", __func__, __LINE__, apIndex, sta[0], sta[1], sta[2], sta[3], sta[4], sta[5], len);
    ctx = (wifi_device_dpp_context_t *)data_plane_queue_remove_event(wifi_data_plane_event_type_dpp, &criteria);

    if (ctx == NULL) {
        wifi_easy_connect_dbg_print(1, "%s:%d ctx NULL\n", __func__, __LINE__);
        return;
    }

    ctx->type = dpp_context_type_received_frame_cfg_result;
    ctx->received_frame.frame = malloc(len);
    memcpy(ctx->received_frame.frame, frame, len);

    ctx->received_frame.length = len;

    data_plane_queue_push(data_plane_queue_create_event(ctx,wifi_data_plane_event_type_dpp, TRUE));
}

void dppReconfigAnnounce_callback(UINT apIndex, mac_address_t sta, UCHAR *frame, UINT len)
{
    wifi_device_dpp_context_t *ctx = NULL;
    PCOSA_DML_WIFI_DPP_STA_CFG  pWifiDppSta;
    PCOSA_DML_WIFI_DPP_CFG pWifiDppCfg;
    mac_addr_str_t	mac_str;
    wifi_easy_connect_event_match_criteria_t criteria;
    errno_t rc = -1;

    pWifiDppCfg = find_dpp_dml_wifi_ap(apIndex);
    if (pWifiDppCfg == NULL) {
        wifi_easy_connect_dbg_print(1, "%s:%d: Could not find dpp config in database\n", __func__, __LINE__);
        return;
    }

    wifi_easy_connect_dbg_print(1, "%s:%d: Found dpp config in database\n", __func__, __LINE__);

    // check if the STA was provisioned by us
    pWifiDppSta = find_dpp_sta_dml_wifi_ap(apIndex, sta);
    if (pWifiDppSta == NULL) {
        wifi_easy_connect_dbg_print(1, "%s:%d: Could not find station:%s in database\n", __func__, __LINE__, to_mac_str(sta, mac_str));
        return;
    }

    wifi_easy_connect_dbg_print(1, "%s:%d: Found dpp station:%s config in database\n", __func__, __LINE__, to_mac_str(sta, mac_str));

    // check the sctivation status
    //if (strcmp(pWifiDppSta->ActivationStatus, enum_str(ActStatus_OK)) != 0) {
    //    wifi_easy_connect_dbg_print(1, "%s:%d: The station was never activated in database\n", __func__, __LINE__);
    //		return;

    //}

    // check if the request for reconfiguring is in the queue already
    criteria.apIndex = apIndex;
    memcpy(criteria.sta_mac, sta, sizeof(mac_address_t));
    criteria.state = STATE_DPP_PROVISIONED;

    if (data_plane_queue_check_event(wifi_data_plane_event_type_dpp, &criteria) == true) {
            wifi_easy_connect_dbg_print(1, "%s:%d: The station is already in queue\n", __func__, __LINE__);	
            return;
        }

    if (wifi_dppProcessReconfigAnnouncement(frame, len, g_easy_connect.csign[apIndex].sign_key_hash) != RETURN_OK) {
        wifi_easy_connect_dbg_print(1, "%s:%d: C-sign-key hash does not match\n", __func__, __LINE__);
        return;

    }

    wifi_easy_connect_dbg_print(1, "%s:%d: Creating device context\n", __func__, __LINE__);

    // create context and push in queue
    ctx = (wifi_device_dpp_context_t *)malloc(sizeof(wifi_device_dpp_context_t));
    if (ctx == NULL) {
        wifi_easy_connect_dbg_print(1, "%s:%d: Exit. Allocation was unsuccessful.\n", __func__, __LINE__);
        return;
    }
    memset(ctx, 0, sizeof(wifi_device_dpp_context_t));

    ctx->ap_index = apIndex;

    // set the reconfig ctx
    ctx->config.reconfigCtx = g_easy_connect.reconfig[ctx->ap_index].reconf_ctx;

    // set the csign instance
    ctx->config.cSignInstance = g_easy_connect.csign[ctx->ap_index].csign_inst;

    wifi_easy_connect_dbg_print(1, "%s:%d: ap index: %d recfg ctx:%p csign instance:%p\n", __func__, __LINE__, ctx->ap_index,
            ctx->config.reconfigCtx, ctx->config.cSignInstance);

    ctx->configurator_version = pWifiDppCfg->Version;
    memcpy(ctx->session_data.sta_mac, sta, sizeof(mac_address_t));
    ctx->max_retries = pWifiDppSta->MaxRetryCount;

    ctx->session_data.session = wifi_dpp_session_type_reconfig;
    ctx->session_data.state = STATE_DPP_PROVISIONED;
    ctx->type = dpp_context_type_received_frame_recfg_announce;

#ifdef WIFI_HAL_VERSION_3
    wifi_getRadioChannel(getRadioIndexFromAp(ctx->ap_index), (ULONG *)&ctx->session_data.channel);
#else
    wifi_getRadioChannel(ctx->ap_index%2, (ULONG *)&ctx->session_data.channel);
#endif
    rc = strcpy_s(ctx->session_data.u.reconfig_data.iPubKey, sizeof(ctx->session_data.u.reconfig_data.iPubKey), g_easy_connect.reconfig[ctx->ap_index].reconf_pub_key);
    ERR_CHK(rc);

    if (strcmp(pWifiDppSta->Cred.KeyManagement, "Common-PSK") == 0) {                 
        ctx->config.credentials.keyManagement = WIFI_DPP_KEY_MGMT_PSK;
    } else if (strcmp(pWifiDppSta->Cred.KeyManagement, "DPPPSKSAE") == 0) {
        ctx->config.credentials.keyManagement = WIFI_DPP_KEY_MGMT_DPPPSKSAE;
    }

    wifi_easy_connect_dbg_print(1, "%s:%d: Pushing event for processing\n", __func__, __LINE__);
    data_plane_queue_push(data_plane_queue_create_event(ctx,wifi_data_plane_event_type_dpp, TRUE));
}

void dppReconfigAuthResponse_callback(UINT apIndex, mac_address_t sta, unsigned char *frame, unsigned int len)
{
    wifi_device_dpp_context_t *ctx = NULL;
    wifi_easy_connect_event_match_criteria_t criteria;

    criteria.apIndex = apIndex;
    memcpy(criteria.sta_mac, sta, sizeof(mac_address_t));
    criteria.state = STATE_DPP_RECFG_AUTH_RSP_PENDING;

    wifi_easy_connect_dbg_print(1, "%s:%d apIndex=%d mac=%02x:%02x:%02x:%02x:%02x:%02x len=%d\n", __func__, __LINE__, apIndex, sta[0], sta[1], sta[2], sta[3], sta[4], sta[5], len);
    ctx = (wifi_device_dpp_context_t *)data_plane_queue_remove_event(wifi_data_plane_event_type_dpp, &criteria);

    if (ctx == NULL) {
        wifi_easy_connect_dbg_print(1, "%s:%d ctx NULL\n", __func__, __LINE__);
        return;
    }

    wifi_easy_connect_dbg_print(1, "%s:%d setting up context\n", __func__, __LINE__);

    ctx->type = dpp_context_type_received_frame_recfg_auth_rsp;
    ctx->received_frame.frame = malloc(len);
    memcpy(ctx->received_frame.frame, frame, len);

    ctx->received_frame.length = len;

    data_plane_queue_push(data_plane_queue_create_event(ctx,wifi_data_plane_event_type_dpp, TRUE));
}

int find_best_dpp_channel(wifi_device_dpp_context_t *ctx)
{
    unsigned int ch;

    wifi_easy_connect_dbg_print(1, "%s: ctx->current_attempts = %d, ctx->num_channels=%d, %d\n", __func__, ctx->current_attempts, ctx->num_channels, __LINE__);    
    if (ctx->current_attempts >= ctx->num_channels) {
        wifi_easy_connect_dbg_print(1, "%s:%d: Exit\n", __func__, __LINE__);
        return -1;
    } 
    ch = ctx->channels_list[ctx->current_attempts];
    ctx->current_attempts++;

    return ch;
}

int start_device_provisioning (PCOSA_DML_WIFI_AP pWiFiAP, ULONG staIndex)
{
    wifi_device_dpp_context_t *ctx = NULL;
    unsigned int i;
    errno_t rc = -1;

    wifi_easy_connect_dbg_print(1, "%s:%d: Enter\n", __func__, __LINE__);
    if(pWiFiAP == NULL)
    {
        wifi_easy_connect_dbg_print(1, "%s:%d: PCOSA_DML_WIFI_AP is NULL\n", __func__, __LINE__);
        return RETURN_ERR;
    }
    PCOSA_DML_WIFI_DPP_STA_CFG pWifiDppSta = &pWiFiAP->DPP.Cfg[staIndex-1]; 
    ULONG apIndex = pWiFiAP->AP.Cfg.InstanceNumber -1;
    UCHAR dppVersion = pWiFiAP->DPP.Version;

    if(pWifiDppSta == NULL)
    {
        wifi_easy_connect_dbg_print(1, "%s:%d: PCOSA_DML_WIFI_DPP_STA_CFG is NULL\n", __func__, __LINE__);
        return RETURN_ERR;
    }
    // create context and push in queue
    ctx = (wifi_device_dpp_context_t *)malloc(sizeof(wifi_device_dpp_context_t));
    if(ctx == NULL)
    {
        wifi_easy_connect_dbg_print(1, "%s:%d: Exit. Allocation was unsuccessful.\n", __func__, __LINE__);
        return RETURN_ERR;
    }
    memset(ctx, 0, sizeof(wifi_device_dpp_context_t));

    ctx->ap_index = apIndex;
    ctx->configurator_version = dppVersion;

    // set the reconfig ctx
    ctx->config.reconfigCtx = g_easy_connect.reconfig[ctx->ap_index].reconf_ctx;

    // set the csign instance
    ctx->config.cSignInstance = g_easy_connect.csign[ctx->ap_index].csign_inst;

    ctx->session_data.session = wifi_dpp_session_type_config;
    memset(ctx->session_data.u.config_data.iPubKey, 0x0, sizeof(char)*256);
    memset(ctx->session_data.u.config_data.rPubKey, 0x0, sizeof(char)*256);

    to_mac_bytes(pWifiDppSta->ClientMac, ctx->session_data.sta_mac);
    rc = strcpy_s(ctx->session_data.u.config_data.iPubKey, sizeof(ctx->session_data.u.config_data.iPubKey), pWifiDppSta->InitiatorBootstrapSubjectPublicKeyInfo);
    ERR_CHK(rc);
    rc = strcpy_s(ctx->session_data.u.config_data.rPubKey, sizeof(ctx->session_data.u.config_data.rPubKey), pWifiDppSta->ResponderBootstrapSubjectPublicKeyInfo);
    ERR_CHK(rc);

    for (i = 0; i < pWifiDppSta->NumChannels; i++) {
        ctx->channels_list[i] = pWifiDppSta->Channels[i];
        wifi_easy_connect_dbg_print(1, "%s:%d: ctx->channels_list[%d] = %d\n", __func__, __LINE__, i, ctx->channels_list[i]);
    }

    ctx->num_channels = pWifiDppSta->NumChannels;
    wifi_easy_connect_dbg_print(1, "%s:%d: ctx->num_channels = %d pWifiDppSta->NumChannels = %d\n", __func__, __LINE__, ctx->num_channels, pWifiDppSta->NumChannels);
    ctx->session_data.state = STATE_DPP_UNPROVISIONED;
    ctx->max_retries = pWifiDppSta->MaxRetryCount;
    ctx->session_data.channel = find_best_dpp_channel(ctx);
    wifi_easy_connect_dbg_print(1, "%s:%d: After find_best_dpp_channel\n", __func__, __LINE__);
  
    if (strcmp(pWifiDppSta->Cred.KeyManagement, "Common-PSK") == 0) {                 
        ctx->config.credentials.keyManagement = WIFI_DPP_KEY_MGMT_PSK;
    } else if (strcmp(pWifiDppSta->Cred.KeyManagement, "DPPPSKSAE") == 0) {
        ctx->config.credentials.keyManagement = WIFI_DPP_KEY_MGMT_DPPPSKSAE;
    }

    data_plane_queue_push(data_plane_queue_create_event(ctx,wifi_data_plane_event_type_dpp, TRUE));

    wifi_easy_connect_dbg_print(1, "%s:%d: DPP Activate started thread and Exit\n", __func__, __LINE__);

    return RETURN_OK;
}

void destroy_easy_connect (void)
{
}

PCOSA_DML_WIFI_DPP_CFG find_dpp_dml_wifi_ap(unsigned int apIndex)
{

    PCOSA_DATAMODEL_WIFI pMyObject;
    PSINGLE_LINK_ENTRY  pSLinkEntry  = NULL;
    PCOSA_DML_WIFI_AP           pWifiAp     = NULL;

    pMyObject = g_easy_connect.wifi_dml;
    if (pMyObject == NULL) {
        wifi_easy_connect_dbg_print(1, "%s:%d: wifi data model not found\n", __func__, __LINE__);
        return NULL;
    }

    if ((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->AccessPointQueue, apIndex)) == NULL) {
        wifi_easy_connect_dbg_print(1, "%s:%d Data Model object not found!\n", __func__, __LINE__);
        return NULL;
    }

    if ((pWifiAp = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext) == NULL) {
        wifi_easy_connect_dbg_print(1, "%s:%d Data Model object not found!\n", __func__, __LINE__);
        return NULL;
    }

    return &pWifiAp->DPP;
}

PCOSA_DML_WIFI_DPP_STA_CFG find_dpp_sta_dml_wifi_ap(unsigned int ap_index, mac_address_t sta_mac) 
{
    unsigned int i;
    bool found = false;
    PCOSA_DML_WIFI_DPP_CFG pWifiApDpp;
    PCOSA_DML_WIFI_DPP_STA_CFG	pWifiApDppSta = NULL;
    mac_address_t bmac;

    if ((pWifiApDpp = find_dpp_dml_wifi_ap(ap_index)) == NULL) {
        wifi_easy_connect_dbg_print(1, "%s:%d: DPP config not found\n", __func__, __LINE__);
        return NULL;
    }

    for (i = 0; i < COSA_DML_WIFI_DPP_STA_MAX; i++) {
        pWifiApDppSta = &pWifiApDpp->Cfg[i];
        if (pWifiApDppSta == NULL) {
            wifi_easy_connect_dbg_print(1, "%s:%d: DPP STA config not found\n", __func__, __LINE__);
            continue;
        }

        wifi_easy_connect_dbg_print(1, "%s:%d: DPP config for STA:%s\n", __func__, __LINE__, pWifiApDppSta->ClientMac);
        to_mac_bytes (pWifiApDppSta->ClientMac, bmac);
        if (memcmp(bmac, sta_mac, sizeof(mac_address_t)) == 0) {
            found = true;
            break;
        }
    }	

    wifi_easy_connect_dbg_print(1, "%s:%d: data found:%d\n", __func__, __LINE__, found);
    return (found == true)?pWifiApDppSta:NULL;
}

int init_easy_connect (PCOSA_DATAMODEL_WIFI pWifiDataModel)
{
    int i;
    PCOSA_DML_WIFI_DPP_CFG pWifiApDPP;

    wifi_easy_connect_dbg_print(1, "%s:%d: Enter\n", __func__, __LINE__);

    g_easy_connect.wifi_dml = pWifiDataModel;

    g_easy_connect.channels_on_ap[0].num = 5;
    g_easy_connect.channels_on_ap[0].channels[0] = 1;
    g_easy_connect.channels_on_ap[0].channels[1] = 6;
    g_easy_connect.channels_on_ap[0].channels[2] = 11;
    g_easy_connect.channels_on_ap[0].channels[3] = 3;
    g_easy_connect.channels_on_ap[0].channels[4] = 9;

    g_easy_connect.channels_on_ap[1].num = 12;
    g_easy_connect.channels_on_ap[1].channels[0] = 36;
    g_easy_connect.channels_on_ap[1].channels[1] = 40;
    g_easy_connect.channels_on_ap[1].channels[2] = 44;
    g_easy_connect.channels_on_ap[1].channels[3] = 48;

    g_easy_connect.channels_on_ap[1].channels[4] = 136;
    g_easy_connect.channels_on_ap[1].channels[5] = 140;
    g_easy_connect.channels_on_ap[1].channels[6] = 144;

    g_easy_connect.channels_on_ap[1].channels[7] = 149;
    g_easy_connect.channels_on_ap[1].channels[8] = 153;
    g_easy_connect.channels_on_ap[1].channels[9] = 157;
    g_easy_connect.channels_on_ap[1].channels[10] = 161;
    g_easy_connect.channels_on_ap[1].channels[11] = 165;

    for (i = 0; i < MAX_DPP_VAP; i++) {
        pWifiApDPP = find_dpp_dml_wifi_ap(i);
        if (pWifiApDPP != NULL) {
            wifi_dppCreateReconfigContext(i, pWifiApDPP->Recfg.PrivateReconfigAccessKey, (void*)&g_easy_connect.reconfig[i].reconf_ctx,
                    g_easy_connect.reconfig[i].reconf_pub_key);
            wifi_dppCreateCSignIntance(i, pWifiApDPP->Recfg.PrivateSigningKey, (void*)&g_easy_connect.csign[i].csign_inst, 
                    g_easy_connect.csign[i].sign_key_hash);

            wifi_easy_connect_dbg_print(1, "%s:%d: ap:%d reconfig context:%p csign instance: %p\n", __func__, __LINE__, i,
                    g_easy_connect.reconfig[i].reconf_ctx, g_easy_connect.csign[i].csign_inst);
        }
    }

    wifi_dpp_frame_received_callbacks_register(dppAuthResponse_callback, dppConfigRequest_callback, 
            dppConfigResult_callback, dppReconfigAnnounce_callback, dppReconfigAuthResponse_callback);

    wifi_dppStartReceivingTestFrame(g_easy_connect.csign[0].sign_key_hash, g_easy_connect.csign[1].sign_key_hash);
    return 0;
}

    wifi_easy_connect_best_enrollee_channels_t *
get_easy_connect_best_enrollee_channels	(unsigned int ap_index)
{
    return &g_easy_connect.channels_on_ap[ap_index];
}

#endif //#if !defined(_XF3_PRODUCT_REQ_) && !defined(_CBR_PRODUCT_REQ_) && !defined(_HUB4_PRODUCT_REQ_) && !defined(_XB7_PRODUCT_REQ_) && !defined(_PLATFORM_TURRIS_) && !defined(_PLATFORM_RASPBERRYPI_)
#endif //#if !defined(_BWG_PRODUCT_REQ_)
