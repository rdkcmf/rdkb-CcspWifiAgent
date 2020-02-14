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
#include "cosa_apis.h"
#include "cosa_dbus_api.h"
#include "cosa_wifi_apis.h"
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
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include "collection.h"
#include "wifi_hal.h"
#include "wifi_easy_connect.h"
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/un.h>
#include <assert.h>
#include "ansc_status.h"
#include <sysevent/sysevent.h>


#if !defined(_BWG_PRODUCT_REQ_) && defined (ENABLE_FEATURE_MESHWIFI)
#if !defined (_XB6_PRODUCT_REQ_) && !defined (_COSA_BCM_ARM_) && !defined(_XF3_PRODUCT_REQ_) && !defined(_CBR_PRODUCT_REQ_) && !defined(_HUB4_PRODUCT_REQ_) && !defined (_ARRIS_XB6_PRODUCT_REQ_) && !defined(_PLATFORM_TURRIS_)

static const char *wifi_health_log = "/rdklogs/logs/wifihealth.txt";

extern bool wifi_api_is_device_associated(int ap_index, char *mac);

extern bool is_device_associated(int ap_index, char *mac);

static wifi_easy_connect_t g_easy_connect = {0};

static void wifi_dpp_dbg_print(int level, char *format, ...)
{
    char buff[2048] = {0};
    va_list list;
    static FILE *fpg = NULL;

    if ((access("/nvram/wifiDppDbg", R_OK)) != 0) {
        return;
    }

    get_formatted_time(buff);
    strcat(buff, " ");

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
    queue_pop(g_easy_connect.queue);

    free(ctx);
    ctx = NULL;

}

void *dpp_frame_exchange_func  (void *arg)
{
#define BUFF_SIZE 512
    bool exit = false;
    struct timespec time_to_wait;
    struct timeval tv_now;
    int rc;
    unsigned int count;
    wifi_DppConfigurationObject_t config;
    wifi_device_dpp_context_t *ctx = NULL;
    ssid_t ssid;
    PCOSA_DML_WIFI_DPP_STA_CFG pWifiDppSta = NULL;
    char buff[BUFF_SIZE] = {0};
    mac_addr_str_t mac_str;
    char passphrase[64] = {0x0};
    pWifiDppSta = ((PCOSA_DML_WIFI_DPP_STA_CFG) arg);
    FILE *TimeFile = NULL;
    int timer = 0, next_ch = 0;
 
    while (exit == false) {
        gettimeofday(&tv_now, NULL);

        /*read Custom time*/
        TimeFile = fopen("/tmp/time.txt", "r");
        if (TimeFile != NULL)
        {
            fscanf(TimeFile, "%d", &timer );
            fclose(TimeFile);TimeFile = NULL;
        }
        timer = ((timer > 30) || (timer < 3)) ? 3:timer;
        time_to_wait.tv_nsec = 0;
        time_to_wait.tv_sec = tv_now.tv_sec + timer; //set timeout to 3 sec

        pthread_mutex_lock(&g_easy_connect.lock);

        rc = pthread_cond_timedwait(&g_easy_connect.cond, &g_easy_connect.lock, &time_to_wait);
        if ((count = queue_count(g_easy_connect.queue)) == 0) {
            wifi_dpp_dbg_print(1, "%s:%d: queue is empty returning from thread\n", __func__, __LINE__);
            pthread_mutex_unlock(&g_easy_connect.lock);
            exit = true;
            continue;
        }

        ctx = queue_peek(g_easy_connect.queue, count - 1);
        if (ctx == NULL) {
            wifi_dpp_dbg_print(1, "%s:%d: could not find ctx in queue so returning from thread\n", __func__, __LINE__);
            pthread_mutex_unlock(&g_easy_connect.lock);
            exit = true;
            continue;
        }
        wifi_dpp_dbg_print(1, "%s:%d: ctx->max_retries = %d\n", __func__, __LINE__, ctx->max_retries);

        if (rc == 0) {
           
            if (ctx->session_data.state == STATE_DPP_UNPROVISIONED) { 
                    
                if (wifi_dppInitiate(ctx) == RETURN_OK) {
                   wifi_dpp_dbg_print(1, "%s:%d: DPP Authentication Request Frame send success\n", __func__, __LINE__);
                    ctx->session_data.state == STATE_DPP_AUTH_RSP_PENDING;
                    ctx->activation_status == ActStatus_In_Progress;
                    memset(buff, 0, BUFF_SIZE);
                    snprintf(buff, BUFF_SIZE, "%s\n", "Wifi DPP: STATE_DPP_AUTH_RSP_PENDING");
                    write_to_file(wifi_health_log, buff);
                    strcpy(pWifiDppSta->ActivationStatus, enum_str(ActStatus_In_Progress));
                    strcpy(pWifiDppSta->EnrolleeResponderStatus, enum_str(STATE_DPP_AUTH_RSP_PENDING));
                } else {
                    wifi_dpp_dbg_print(1, "%s:%d: DPP Authentication Request Frame send failed\n", __func__, __LINE__);
                    ctx->dpp_init_retries++;
                } 
            } else if (ctx->session_data.state == STATE_DPP_AUTH_RSP_PENDING) {
                if ((ctx->type == dpp_context_type_received_frame_auth_rsp) && (wifi_dppProcessAuthResponse(ctx) == RETURN_OK)) {
                    wifi_dpp_dbg_print(1, "%s:%d: Sending DPP Authentication Cnf ... \n", __func__, __LINE__);
                    rc = wifi_dppSendAuthCnf(ctx);
                    ctx->type = dpp_context_type_session_data;
                    free(ctx->received_frame.frame);
                    ctx->received_frame.length = 0;

                    if (rc == RETURN_OK) {
                        ctx->session_data.state = STATE_DPP_AUTHENTICATED;
                        ctx->activation_status == ActStatus_In_Progress;
                        strcpy(pWifiDppSta->ActivationStatus, enum_str(ActStatus_In_Progress));
                        strcpy(pWifiDppSta->EnrolleeResponderStatus, enum_str(STATE_DPP_AUTHENTICATED));
                        memset(buff, 0, BUFF_SIZE);
                        snprintf(buff, BUFF_SIZE, "%s MAC: %s\n", "Wifi DPP: STATE_DPP_AUTHENTICATED", pWifiDppSta->ClientMac);
                        write_to_file(wifi_health_log, buff);
                    } else {
                        ctx->session_data.state = RESPONDER_STATUS_AUTH_FAILURE;
                        ctx->activation_status == ActStatus_No_Response;
                        strcpy(pWifiDppSta->ActivationStatus, enum_str(ActStatus_No_Response));
                        strcpy(pWifiDppSta->EnrolleeResponderStatus, enum_str(RESPONDER_STATUS_AUTH_FAILURE));
                        end_device_provisioning(ctx);
                        memset(buff, 0, BUFF_SIZE);
                        snprintf(buff, BUFF_SIZE, "%s MAC: %s\n", "Wifi DPP: RESPONSE PENDING FAILURE", pWifiDppSta->ClientMac);
                        write_to_file(wifi_health_log, buff);
                    }
                }
            } else if (ctx->session_data.state == STATE_DPP_AUTHENTICATED) {
                if ((ctx->type == dpp_context_type_received_frame_cfg_req) && (wifi_dppProcessConfigRequest(ctx) == RETURN_OK)) {

                    ctx->config.wifiTech = WIFI_DPP_TECH_INFRA;

                    wifi_getSSIDName(ctx->ap_index, ssid);
                    strncpy(ctx->config.discovery, ssid, sizeof(ssid_t));
                    ctx->config.credentials.keyManagement = WIFI_DPP_KEY_MGMT_PSK;
					wifi_getApSecurityKeyPassphrase(ctx->ap_index, passphrase);
					strncpy(ctx->config.credentials.creds.passPhrase, passphrase, sizeof(ctx->config.credentials.creds.passPhrase));

					wifi_dpp_dbg_print(1, "%s:%d: Sending DPP Config Rsp ... ssid: %s passphrase: %s\n", __func__, __LINE__,
						ctx->config.discovery, ctx->config.credentials.creds.passPhrase);

					rc = wifi_dppSendConfigResponse(ctx);
				
					ctx->type = dpp_context_type_session_data;
					free(ctx->received_frame.frame);
					ctx->received_frame.length = 0;
                	
					if (rc == RETURN_OK) {

						ctx->session_data.state = STATE_DPP_CFG_RSP_SENT;
                        ctx->activation_status = ActStatus_In_Progress;
                        strcpy(pWifiDppSta->ActivationStatus, enum_str(ActStatus_In_Progress));
                        strcpy(pWifiDppSta->EnrolleeResponderStatus, enum_str(STATE_DPP_CFG_RSP_SENT));
                        memset(buff, 0, BUFF_SIZE);
                        snprintf(buff, BUFF_SIZE, "%s\n", "Wifi DPP: STATE_DPP_CFG_RSP_SENT");
                        write_to_file(wifi_health_log, buff);
					} else {
                        ctx->session_data.state = RESPONDER_STATUS_CONFIGURATION_FAILURE;
                        ctx->activation_status = ActStatus_Config_Error;
                        strcpy(pWifiDppSta->ActivationStatus, enum_str(ActStatus_Config_Error));
                        strcpy(pWifiDppSta->EnrolleeResponderStatus, enum_str(RESPONDER_STATUS_CONFIGURATION_FAILURE));
                        memset(buff, 0, BUFF_SIZE);
                        snprintf(buff, BUFF_SIZE, "%s MAC: %s\n", "Wifi DPP: RESPONDER_STATUS_CONFIGURATION_FAILURE", pWifiDppSta->ClientMac);
                        write_to_file(wifi_health_log, buff);
                        pWifiDppSta->Activate = FALSE;
                        end_device_provisioning(ctx);
                    }
                }
            }
        } else if (rc == ETIMEDOUT) {
            wifi_dpp_dbg_print(1, "%s:%d: DPP context state:%d DPP init retries:%d Max retries:%d\n", __func__, __LINE__,
					ctx->session_data.state, ctx->dpp_init_retries, ctx->max_retries);
            if ((ctx->session_data.state == STATE_DPP_AUTH_RSP_PENDING) || (ctx->session_data.state == STATE_DPP_UNPROVISIONED)) {
				if (ctx->dpp_init_retries < ctx->max_retries) {
                    wifi_dpp_dbg_print(1, "%s:%d: Trying to send DPP Authentication Request Frame ... \n", __func__, __LINE__);
                    if (wifi_dppInitiate(ctx) == RETURN_OK) {
                        ctx->session_data.state == STATE_DPP_AUTH_RSP_PENDING;
                        ctx->activation_status == ActStatus_No_Response;
                        strcpy(pWifiDppSta->ActivationStatus, enum_str(ActStatus_No_Response));
                        strcpy(pWifiDppSta->EnrolleeResponderStatus, enum_str(STATE_DPP_AUTH_RSP_PENDING));
                        memset(buff, 0, BUFF_SIZE);
                        snprintf(buff, BUFF_SIZE, "%s\n", "Wifi DPP: STATE_DPP_AUTH_RSP_PENDING");
                        write_to_file(wifi_health_log, buff);
                        wifi_dpp_dbg_print(1, "%s:%d: DPP Authentication Request Frame send success\n", __func__, __LINE__);
                    } else {
                        wifi_dpp_dbg_print(1, "%s:%d: DPP Authentication Request Frame send failed\n", __func__, __LINE__);
                    } 
                   
					ctx->dpp_init_retries++;
                } else if ((next_ch = find_best_dpp_channel(ctx)) != -1) {
					ctx->dpp_init_retries = 0;
					ctx->session_data.channel = next_ch;
					if (wifi_dppInitiate(ctx) == RETURN_OK) {
                        ctx->session_data.state == STATE_DPP_AUTH_RSP_PENDING;
                        ctx->activation_status == ActStatus_No_Response;
                        strcpy(pWifiDppSta->ActivationStatus, enum_str(ActStatus_No_Response));
                        strcpy(pWifiDppSta->EnrolleeResponderStatus, enum_str(STATE_DPP_AUTH_RSP_PENDING));
                        memset(buff, 0, BUFF_SIZE);
                        snprintf(buff, BUFF_SIZE, "%s\n", "Wifi DPP: STATE_DPP_AUTH_RSP_PENDING");
                        write_to_file(wifi_health_log, buff);
                        wifi_dpp_dbg_print(1, "%s:%d: DPP Authentication Request Frame send success\n", __func__, __LINE__);
                    } else {
                        wifi_dpp_dbg_print(1, "%s:%d: DPP Authentication Request Frame send failed\n", __func__, __LINE__);
                    }
                   
                    ctx->dpp_init_retries++;
				} else {
					ctx->enrollee_status = RESPONDER_STATUS_AUTH_FAILURE;
					ctx->activation_status = ActStatus_Failed;
                    strcpy(pWifiDppSta->ActivationStatus, enum_str(ActStatus_Failed));
                    strcpy(pWifiDppSta->EnrolleeResponderStatus, enum_str(RESPONDER_STATUS_AUTH_FAILURE));
                    memset(buff, 0, BUFF_SIZE);
                    snprintf(buff, BUFF_SIZE, "%s MAC: %s\n", "Wifi DPP: RESPONDER_STATUS_AUTH_FAILURE", pWifiDppSta->ClientMac);
                    write_to_file(wifi_health_log, buff);
                    pWifiDppSta->Activate = FALSE;
                    end_device_provisioning(ctx);
                }
            } else if (ctx->session_data.state == STATE_DPP_AUTHENTICATED) {
				// DPP Config Request never arrived
				if (ctx->check_for_config_requested >= ctx->max_retries/*5*/) {
					ctx->enrollee_status = RESPONDER_STATUS_AUTH_FAILURE;
					ctx->activation_status = ActStatus_Failed;
                    strcpy(pWifiDppSta->ActivationStatus, enum_str(ActStatus_Failed));
                    strcpy(pWifiDppSta->EnrolleeResponderStatus, enum_str(RESPONDER_STATUS_AUTH_FAILURE));
                    memset(buff, 0, BUFF_SIZE);
                    snprintf(buff, BUFF_SIZE, "%s MAC: %s\n", "Wifi DPP: RESPONDER_STATUS_AUTH_FAILURE", pWifiDppSta->ClientMac);
                    write_to_file(wifi_health_log, buff);
                    pWifiDppSta->Activate = FALSE;
                    end_device_provisioning(ctx);
				} else {
					ctx->check_for_config_requested++;
				}

			} else if (ctx->session_data.state == STATE_DPP_CFG_RSP_SENT) {
                // now start checking for associated state on the vap index
                to_mac_str(ctx->session_data.sta_mac, mac_str);
                if (wifi_api_is_device_associated(ctx->ap_index, mac_str) == true) {
                    ctx->enrollee_status = RESPONDER_STATUS_OK;
                    ctx->activation_status = ActStatus_OK;
                    strcpy(pWifiDppSta->ActivationStatus, enum_str(ActStatus_OK));
                    strcpy(pWifiDppSta->EnrolleeResponderStatus, enum_str(RESPONDER_STATUS_OK)); 
                    pWifiDppSta->Activate = FALSE;
                    memset(buff, 0, BUFF_SIZE);
                    snprintf(buff, BUFF_SIZE, "%s\n", "Wifi DPP: RESPONDER_STATUS_OK");
                    write_to_file(wifi_health_log, buff);
                    end_device_provisioning(ctx);

				} else if (ctx->check_for_associated >= ctx->max_retries/*5*/) {
					ctx->enrollee_status = RESPONDER_STATUS_CONFIG_REJECTED;
					ctx->activation_status = ActStatus_Config_Error;
                    strcpy(pWifiDppSta->ActivationStatus, enum_str(ActStatus_Config_Error));
                    strcpy(pWifiDppSta->EnrolleeResponderStatus, enum_str(RESPONDER_STATUS_CONFIG_REJECTED));
                    pWifiDppSta->Activate = FALSE;
                    memset(buff, 0, BUFF_SIZE);
                    snprintf(buff, BUFF_SIZE, "%s MAC: %s\n", "Wifi DPP: RESPONDER_STATUS_CONFIG_REJECTED", pWifiDppSta->ClientMac);
                    write_to_file(wifi_health_log, buff);
                    end_device_provisioning(ctx);
				} else {
					ctx->check_for_associated++;
				}
			} else if (ctx->session_data.state == STATE_DPP_AUTH_FAILED) {
				// Authentication Cnf send failure
				ctx->enrollee_status = RESPONDER_STATUS_AUTH_FAILURE;
				ctx->activation_status = ActStatus_Failed;
                strcpy(pWifiDppSta->ActivationStatus, enum_str(ActStatus_Failed));
                strcpy(pWifiDppSta->EnrolleeResponderStatus, enum_str(RESPONDER_STATUS_AUTH_FAILURE));
                pWifiDppSta->Activate = FALSE;
                memset(buff, 0, BUFF_SIZE);
                snprintf(buff, BUFF_SIZE, "%s MAC: %s\n", "Wifi DPP: RESPONDER_STATUS_AUTH_FAILURE", pWifiDppSta->ClientMac);
                write_to_file(wifi_health_log, buff);
                end_device_provisioning(ctx);
            }
        }
        pthread_mutex_unlock(&g_easy_connect.lock);
    }

    g_easy_connect.tid = -1;
    return NULL;
}


void dppAuthResponse_callback(UINT apIndex, mac_address_t sta, unsigned char *frame, unsigned int len)
{
    wifi_device_dpp_context_t *ctx = NULL;
    unsigned int count;

    wifi_dpp_dbg_print(1, "%s:%d apIndex=%d mac=%02x:%02x:%02x:%02x:%02x:%02x len=%d\n", __func__, __LINE__, apIndex, sta[0], sta[1], sta[2], sta[3], sta[4], sta[5], len);
    pthread_mutex_lock(&g_easy_connect.lock);
	if ((count = queue_count(g_easy_connect.queue)) == 0) {
		pthread_mutex_unlock(&g_easy_connect.lock);
        wifi_dpp_dbg_print(1, "%s:%d count NULL\n", __func__, __LINE__);
		return;
	}
	ctx = queue_peek(g_easy_connect.queue, count - 1);

	if ((ctx == NULL) || (ctx->ap_index != apIndex) || (memcmp(ctx->session_data.sta_mac, sta, sizeof(mac_address_t)) != 0)) {
        wifi_dpp_dbg_print(1, "%s:%d ctx NULL\n", __func__, __LINE__);
		pthread_mutex_unlock(&g_easy_connect.lock);
		return;
	}

	if (ctx->session_data.state != STATE_DPP_AUTH_RSP_PENDING) {
        wifi_dpp_dbg_print(1, "%s:%d wrong state %d\n", __func__, __LINE__, ctx->session_data.state);
		pthread_mutex_unlock(&g_easy_connect.lock);
		return;
	}

	ctx->type = dpp_context_type_received_frame_auth_rsp;
	ctx->received_frame.frame = malloc(len);
	memcpy(ctx->received_frame.frame, frame, len);

	ctx->received_frame.length = len;

    pthread_cond_signal(&g_easy_connect.cond);
    pthread_mutex_unlock(&g_easy_connect.lock);
    wifi_dpp_dbg_print(1, "%s:%d EXIT ALL DONE\n", __func__, __LINE__);

}

void dppConfigRequest_callback(UINT apIndex, mac_address_t sta, UCHAR token, UCHAR *configAttributes, UINT len)
{
    wifi_device_dpp_context_t *ctx = NULL;
	unsigned int count;

    pthread_mutex_lock(&g_easy_connect.lock);
	if ((count = queue_count(g_easy_connect.queue)) == 0) {
		pthread_mutex_unlock(&g_easy_connect.lock);
		return;
	}
	ctx = queue_peek(g_easy_connect.queue, count - 1);
	if ((ctx == NULL) || (ctx->ap_index != apIndex) || (memcmp(ctx->session_data.sta_mac, sta, sizeof(mac_address_t)) != 0)) {
		pthread_mutex_unlock(&g_easy_connect.lock);
		return;
	}
	
	if (ctx->session_data.state != STATE_DPP_AUTHENTICATED) {
		pthread_mutex_unlock(&g_easy_connect.lock);
		return;
	}

    ctx->type = dpp_context_type_received_frame_cfg_req;
    ctx->received_frame.frame = malloc(len);
    memcpy(ctx->received_frame.frame, configAttributes, len);
    ctx->received_frame.length = len; // add length
    ctx->token = token;

    pthread_cond_signal(&g_easy_connect.cond);
    pthread_mutex_unlock(&g_easy_connect.lock);

}

int find_best_dpp_channel(wifi_device_dpp_context_t *ctx)
{
    unsigned int ch;

    wifi_dpp_dbg_print(1, "%s: ctx->current_attempts = %d, ctx->num_channels=%d, %d\n", __func__, ctx->current_attempts, ctx->num_channels, __LINE__);    
    if (ctx->current_attempts >= ctx->num_channels) {
        wifi_dpp_dbg_print(1, "%s:%d: Exit\n", __func__, __LINE__);
        return -1;
    } 
    ch = ctx->channels_list[ctx->current_attempts];
    ctx->current_attempts++;

	return ch;
}

int start_device_provisioning (ULONG apIndex, PCOSA_DML_WIFI_DPP_STA_CFG pWifiDppSta)
{
    wifi_device_dpp_context_t *ctx = NULL;
    mac_address_t    sta_mac;
	unsigned int i;

    wifi_dpp_dbg_print(1, "%s:%d: Enter\n", __func__, __LINE__);
    if(pWifiDppSta == NULL)
    {
        wifi_dpp_dbg_print(1, "%s:%d: PCOSA_DML_WIFI_DPP_STA_CFG is NULL\n", __func__, __LINE__);
        return RETURN_ERR;
    }
    
    // create context and push in queue
    ctx = (wifi_device_dpp_context_t *)malloc(sizeof(wifi_device_dpp_context_t));
    memset(ctx, 0, sizeof(wifi_device_dpp_context_t));

    memset(ctx->session_data.iPubKey, 0x0, sizeof(char)*256);
    memset(ctx->session_data.rPubKey, 0x0, sizeof(char)*256);
    ctx->ap_index = apIndex;

    to_mac_bytes(pWifiDppSta->ClientMac, ctx->session_data.sta_mac);
    strcpy(ctx->session_data.iPubKey, pWifiDppSta->InitiatorBootstrapSubjectPublicKeyInfo);
    strcpy(ctx->session_data.rPubKey, pWifiDppSta->ResponderBootstrapSubjectPublicKeyInfo);
    
    for (i = 0; i < pWifiDppSta->NumChannels; i++) {
        ctx->channels_list[i] = pWifiDppSta->Channels[i];
        wifi_dpp_dbg_print(1, "%s:%d: ctx->channels_list[%d] = %d\n", __func__, __LINE__, i, ctx->channels_list[i]);
    }

    ctx->num_channels = pWifiDppSta->NumChannels;
    wifi_dpp_dbg_print(1, "%s:%d: ctx->num_channels = %d pWifiDppSta->NumChannels = %d\n", __func__, __LINE__, ctx->num_channels, pWifiDppSta->NumChannels);
    ctx->session_data.state = STATE_DPP_UNPROVISIONED;
    ctx->max_retries = pWifiDppSta->MaxRetryCount;
    ctx->session_data.channel = find_best_dpp_channel(ctx);

    // if the provisioning thread does not exist create
    if ((g_easy_connect.tid == -1) && (pthread_create(&g_easy_connect.tid, NULL, dpp_frame_exchange_func, (void *) pWifiDppSta) != 0)) {
        wifi_dpp_dbg_print(1, "%s:%d: DPP monitor thread create error\n", __func__, __LINE__);
        queue_push(g_easy_connect.queue, ctx);
    } else {

        pthread_mutex_lock(&g_easy_connect.lock);
        queue_push(g_easy_connect.queue, ctx);
        pthread_cond_signal(&g_easy_connect.cond);
        pthread_mutex_unlock(&g_easy_connect.lock);
    }
     wifi_dpp_dbg_print(1, "%s:%d: DPP Activate started thread and Exit\n", __func__, __LINE__);
    return RETURN_OK;
}

void destroy_easy_connect (void)
{
    queue_destroy(g_easy_connect.queue);
    pthread_mutex_destroy(&g_easy_connect.lock);
    pthread_cond_destroy(&g_easy_connect.cond);
}

int init_easy_connect (void)
{
        
    wifi_dpp_dbg_print(1, "%s:%d: Enter\n", __func__, __LINE__);

    pthread_cond_init(&g_easy_connect.cond, NULL);
    pthread_mutex_init(&g_easy_connect.lock, NULL);
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
    
	g_easy_connect.queue = queue_create();
    g_easy_connect.tid = -1;

    wifi_dpp_frame_received_callbacks_register(dppAuthResponse_callback, dppConfigRequest_callback);
}

wifi_easy_connect_best_enrollee_channels_t *
get_easy_connect_best_enrollee_channels	(unsigned int ap_index)
{
	return &g_easy_connect.channels_on_ap[ap_index];
}

#endif// !defined(_BWG_PRODUCT_REQ_) && defined (ENABLE_FEATURE_MESHWIFI)
#endif// !defined (_XB6_PRODUCT_REQ_) && !defined(_XF3_PRODUCT_REQ_) && !defined(_CBR_PRODUCT_REQ_) && !defined(_HUB4_PRODUCT_REQ_)
