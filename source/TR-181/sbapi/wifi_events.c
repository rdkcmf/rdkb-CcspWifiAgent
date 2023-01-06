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

#if defined (FEATURE_CSI)
#include <rbus/rbus.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <inttypes.h>

#include "ansc_platform.h"
#include "ccsp_WifiLog_wrapper.h"
#include "wifi_events.h"

#define MAX_EVENT_NAME_SIZE     200
#define CLIENTDIAG_JSON_BUFFER_SIZE 665

typedef struct {
    char name[MAX_EVENT_NAME_SIZE];
    int idx;
    wifi_monitor_event_type_t type;
    BOOL subscribed;
    unsigned int num_subscribers;
} event_element_t;

typedef struct {
    unsigned long csi_session_num;
    bool enabled;
    unsigned int csi_client_count;
    mac_address_t csi_client_list[MAX_CSI_CLIENTS_PER_SESSION];
} csi_data_t;

rbusError_t events_subHandler(rbusHandle_t handle, rbusEventSubAction_t action, const char* eventName, 
                                    rbusFilter_t filter, int32_t interval, bool* autoPublish);
rbusError_t events_APtable_addrowhandler(rbusHandle_t handle, char const* tableName, char const* aliasName, uint32_t* instNum);
rbusError_t events_CSItable_addrowhandler(rbusHandle_t handle, char const* tableName, char const* aliasName, uint32_t* instNum);
rbusError_t events_APtable_removerowhandler(rbusHandle_t handle, char const* rowName);
rbusError_t events_CSItable_removerowhandler(rbusHandle_t handle, char const* rowName);
rbusError_t events_GetHandler(rbusHandle_t handle, rbusProperty_t property, rbusGetHandlerOptions_t* opts);
rbusError_t events_CSIGetHandler(rbusHandle_t handle, rbusProperty_t property, rbusGetHandlerOptions_t* opts);
rbusError_t events_CSISetHandler(rbusHandle_t handle, rbusProperty_t property, rbusSetHandlerOptions_t* opts);

static rbusHandle_t         g_rbus_handle;
static queue_t              *g_rbus_events_queue;
static pthread_mutex_t      g_events_lock;
static BOOL                 g_isRbusAvailable = false;
static char                 *gdiag_events_json_buffer[MAX_VAP];
static queue_t              *g_csi_data_queue;

static queue_t** get_csi_entry_queue()
{
    return &g_csi_data_queue;
}

int events_init(void)
{
    char componentName[] = "WifiEventProvider";
    int rc, i, ap_cnt;
    rbusDataElement_t dataElement[10] = {
        {"Device.WiFi.AccessPoint.{i}.",                            RBUS_ELEMENT_TYPE_TABLE, {NULL, NULL, events_APtable_addrowhandler, events_APtable_removerowhandler, NULL, NULL}},
        {"Device.WiFi.AccessPoint.{i}.X_RDK_deviceConnected",      RBUS_ELEMENT_TYPE_EVENT, {NULL, NULL, NULL, NULL, events_subHandler, NULL}},
        {"Device.WiFi.AccessPoint.{i}.X_RDK_deviceDisconnected",   RBUS_ELEMENT_TYPE_EVENT, {NULL, NULL, NULL, NULL, events_subHandler, NULL}},
        {"Device.WiFi.AccessPoint.{i}.X_RDK_deviceDeauthenticated",RBUS_ELEMENT_TYPE_EVENT, {NULL, NULL, NULL, NULL, events_subHandler, NULL}},
        {"Device.WiFi.AccessPoint.{i}.X_RDK_DiagData",             RBUS_ELEMENT_TYPE_EVENT, {events_GetHandler, NULL, NULL, NULL, events_subHandler, NULL}},
        {"Device.WiFi.X_RDK_CSI.{i}.",                              RBUS_ELEMENT_TYPE_TABLE, {NULL, NULL, events_CSItable_addrowhandler, events_CSItable_removerowhandler, NULL, NULL}},
        {"Device.WiFi.X_RDK_CSI.{i}.data",                         RBUS_ELEMENT_TYPE_EVENT, {NULL, NULL, NULL, NULL, events_subHandler, NULL}},
        {"Device.WiFi.X_RDK_CSI.{i}.ClientMaclist",                RBUS_ELEMENT_TYPE_PROPERTY, {events_CSIGetHandler, events_CSISetHandler, NULL, NULL, NULL, NULL}},
        {"Device.WiFi.X_RDK_CSI.{i}.Enable",                       RBUS_ELEMENT_TYPE_PROPERTY, {events_CSIGetHandler, events_CSISetHandler, NULL, NULL, NULL, NULL}},
        {"Device.WiFi.X_RDK_CSINumberOfEntries",                   RBUS_ELEMENT_TYPE_PROPERTY, {events_CSIGetHandler, NULL, NULL, NULL, NULL, NULL}}
    };
    wifi_dbg_print(1, "%s():\n", __FUNCTION__);

    if(RBUS_ENABLED == rbus_checkStatus())
    {
        g_isRbusAvailable = TRUE;
    }
    else
    {
        wifi_dbg_print(1, "%s(): RBUS not available. WifiEvents is not supported\n", __FUNCTION__);
        return 0;
    }

    pthread_mutex_init(&g_events_lock, NULL);

    rc = rbus_open(&g_rbus_handle, componentName);
    if(rc != RBUS_ERROR_SUCCESS)
    {
        wifi_dbg_print(1, "%s():fail to open rbus_open\n", __FUNCTION__);
        return -1;
    }

    g_rbus_events_queue = queue_create();
    if(g_rbus_events_queue == NULL)
    {
        rbus_close(g_rbus_handle);
        wifi_dbg_print(1, "%s(): fail to create rbus events queue\n", __FUNCTION__);
        return -1;
    }
   
    memset(gdiag_events_json_buffer, 0, MAX_VAP*sizeof(char *));

    rc = rbus_regDataElements(g_rbus_handle, 10, dataElement);
    if(rc != RBUS_ERROR_SUCCESS)
    {
        wifi_dbg_print(1, "%s() rbus_regDataElements failed %d\n", __FUNCTION__, rc);
    }
    else
    {
        wifi_dbg_print(1, "%s() rbus_regDataElements success\n", __FUNCTION__);
    }

    ap_cnt = CosaDmlWiFiAPGetNumberOfEntries(NULL); 
    for(i=1;i<=ap_cnt;i++)
    {
        rc = rbusTable_addRow(g_rbus_handle, "Device.WiFi.AccessPoint.", NULL, NULL);
        if(rc != RBUS_ERROR_SUCCESS)
        {
            wifi_dbg_print(1, "%s() rbusTable_addRow failed %d\n", __FUNCTION__, rc);
        }
    }

    queue_t **csi_queue = (queue_t**)get_csi_entry_queue();
    *csi_queue = queue_create();

    return 0;
}

static BOOL events_getSubscribed(char *eventName)
{
    int i;
    event_element_t *event;
    int count = queue_count(g_rbus_events_queue);

    if (count == 0) {
        return FALSE;
    }
    
    for (i = 0; i < count; i++) {
        event = queue_peek(g_rbus_events_queue, i);
        if ((event != NULL) && (strncmp(event->name, eventName, MAX_EVENT_NAME_SIZE) == 0)) {
            return event->subscribed;
        }
    }
    return FALSE;
}

static event_element_t *events_getEventElement(char *eventName)
{
    int i;
    event_element_t *event;
    int count = queue_count(g_rbus_events_queue);

    if (count == 0) {
        return NULL;
    }
    
    for (i = 0; i < count; i++) {
        event = queue_peek(g_rbus_events_queue, i);
        if ((event != NULL) && (strncmp(event->name, eventName, MAX_EVENT_NAME_SIZE) == 0)) {
            return event;
        }
    }
    return NULL;
}

void events_update_clientdiagdata(unsigned int num_devs, int vap_idx, wifi_associated_dev3_t *dev_array)
{

    unsigned int i =0;
    unsigned int pos = 0;
    unsigned int t_pos = 0;

    if(g_isRbusAvailable == FALSE)
    {
        return;
    }

    pthread_mutex_lock(&g_events_lock);
    if(gdiag_events_json_buffer[vap_idx] != NULL)
    {

        pos = snprintf(gdiag_events_json_buffer[vap_idx], 
                CLIENTDIAG_JSON_BUFFER_SIZE*(sizeof(char))*MAX_ASSOCIATED_WIFI_DEVS,
                "{"
                "\"Version\":\"1.0\","
                "\"AssociatedClientsDiagnostics\":["
                "{"
                "\"VapIndex\":\"%d\","
                "\"AssociatedClientDiagnostics\":[", 
                vap_idx + 1);
        t_pos = pos + 1;
        if(dev_array != NULL) {
            for(i=0; i<num_devs; i++) {
                pos += snprintf(&gdiag_events_json_buffer[vap_idx][pos], 
                        (CLIENTDIAG_JSON_BUFFER_SIZE*(sizeof(char))*MAX_ASSOCIATED_WIFI_DEVS)-pos, "{"
                        "\"MAC\":\"%02x%02x%02x%02x%02x%02x\","
                        "\"DownlinkDataRate\":\"%d\","
                        "\"UplinkDataRate\":\"%d\","
                        "\"BytesSent\":\"%lu\","
                        "\"BytesReceived\":\"%lu\","
                        "\"PacketsSent\":\"%lu\","
                        "\"PacketsRecieved\":\"%lu\","
                        "\"Errors\":\"%lu\","
                        "\"RetransCount\":\"%lu\","
                        "\"Acknowledgements\":\"%lu\","
                        "\"SignalStrength\":\"%d\","
                        "\"SNR\":\"%d\","
                        "\"OperatingStandard\":\"%s\","
                        "\"OperatingChannelBandwidth\":\"%s\","
                        "\"AuthenticationFailures\":\"%d\""
                        "},",     
                        dev_array->cli_MACAddress[0],
                        dev_array->cli_MACAddress[1],
                        dev_array->cli_MACAddress[2],
                        dev_array->cli_MACAddress[3],
                        dev_array->cli_MACAddress[4],
                        dev_array->cli_MACAddress[5],
                        dev_array->cli_MaxDownlinkRate,
                        dev_array->cli_MaxUplinkRate,
                        dev_array->cli_BytesSent,
                        dev_array->cli_BytesReceived,
                        dev_array->cli_PacketsSent,
                        dev_array->cli_PacketsReceived,
                        dev_array->cli_ErrorsSent,
                        dev_array->cli_RetransCount,
                        dev_array->cli_DataFramesSentAck,
                        dev_array->cli_SignalStrength,
                        dev_array->cli_SNR,
                        dev_array->cli_OperatingStandard,
                        dev_array->cli_OperatingChannelBandwidth,
                        dev_array->cli_AuthenticationFailures);
                dev_array++;
            }
            t_pos = pos;
        }
        snprintf(&gdiag_events_json_buffer[vap_idx][t_pos-1], (
                CLIENTDIAG_JSON_BUFFER_SIZE*(sizeof(char))*MAX_ASSOCIATED_WIFI_DEVS)-t_pos-1,"]"
                "}"
                "]"
                "}");
    }
    pthread_mutex_unlock(&g_events_lock);
}

int events_publish(wifi_monitor_data_t data)
{
    rbusEvent_t event;
    rbusObject_t rdata;
    rbusValue_t value;
    bool should_publish = FALSE;
    char eventName[MAX_EVENT_NAME_SIZE];
    int rc;

    if(g_isRbusAvailable == FALSE)
    {
        return 0;
    }

    pthread_mutex_lock(&g_events_lock);
    rbusValue_Init(&value);
    rbusObject_Init(&rdata, NULL);

    wifi_dbg_print(1, "%s(): rbusEvent_Publish Event %d\n", __FUNCTION__, data.event_type);
    switch(data.event_type)
    {
        case monitor_event_type_diagnostics:
            sprintf(eventName, "Device.WiFi.AccessPoint.%d.X_RDK_DiagData", data.ap_index + 1);
            if (gdiag_events_json_buffer[data.ap_index] != NULL)
            {
                rbusValue_SetString(value, gdiag_events_json_buffer[data.ap_index]);
                wifi_dbg_print(1, "%s(): device_diagnostics Event %d %s \n", __FUNCTION__, data.event_type, eventName);
                should_publish = TRUE;
            }
            break;
        case monitor_event_type_connect:
        case monitor_event_type_disconnect:
        case monitor_event_type_deauthenticate:
            if(data.event_type == monitor_event_type_connect) {
                sprintf(eventName, "Device.WiFi.AccessPoint.%d.X_RDK_deviceConnected", data.ap_index + 1);
            }
            else if(data.event_type == monitor_event_type_disconnect) {
                sprintf(eventName, "Device.WiFi.AccessPoint.%d.X_RDK_deviceDisconnected", data.ap_index + 1);
            }
            else {
                sprintf(eventName, "Device.WiFi.AccessPoint.%d.X_RDK_deviceDeauthenticated", data.ap_index + 1);
            }
            if(events_getSubscribed(eventName) == TRUE)
            {
                rbusValue_SetBytes(value, (uint8_t *)&data.u.dev.sta_mac[0], sizeof(data.u.dev.sta_mac));
                wifi_dbg_print(1, "%s(): Event - %d %s \n", __FUNCTION__, data.event_type, eventName);
                should_publish = TRUE;
            }
            break;
        case monitor_event_type_csi:
            {
                char buffer[(strlen("CSI") + 1) + sizeof(unsigned int) + sizeof(time_t) + (sizeof(unsigned int)) + (1 *(sizeof(mac_addr_t) + sizeof(unsigned int) + sizeof(wifi_csi_dev_t)))];
                unsigned int total_length, num_csi_clients, csi_data_lenght;
                time_t datetime;
                char *pbuffer = (char *)buffer;

                sprintf(eventName, "Device.WiFi.X_RDK_CSI.%d.data", data.csi_session);

                //ASCII characters "CSI"
                memcpy(pbuffer,"CSI", (strlen("CSI") + 1));
                pbuffer = pbuffer + (strlen("CSI") + 1);

                //Total length:  <length of this entire data field as an unsigned int>
                total_length = sizeof(time_t) + (sizeof(unsigned int)) + (1 *(sizeof(mac_addr_t) + sizeof(unsigned int) + sizeof(wifi_csi_data_t)));
                memcpy(pbuffer, &total_length, sizeof(unsigned int));
                pbuffer = pbuffer + sizeof(unsigned int);

                //DataTimeStamp:  <date-time, number of seconds since the Epoch>
                datetime = time(NULL);
                memcpy(pbuffer, &datetime, sizeof(time_t));
                pbuffer = pbuffer + sizeof(time_t);

                //NumberOfClients:  <unsigned int number of client devices>
                num_csi_clients = 1;
                memcpy(pbuffer, &num_csi_clients, sizeof(unsigned int));
                pbuffer = pbuffer + sizeof(unsigned int);

                //clientMacAddress:  <client mac address>
                memcpy(pbuffer, &data.u.csi.sta_mac, sizeof(mac_addr_t));
                pbuffer = pbuffer + sizeof(mac_addr_t);

                //length of client CSI data:  <size of the next field in bytes>
                csi_data_lenght = sizeof(wifi_csi_data_t);
                memcpy(pbuffer, &csi_data_lenght, sizeof(unsigned int));
                pbuffer = pbuffer + sizeof(unsigned int);

                //<client device CSI data>
                memcpy(pbuffer, &data.u.csi.csi, sizeof(wifi_csi_data_t));

                rbusValue_SetBytes(value, (uint8_t*)buffer, sizeof(buffer));
                should_publish = TRUE;
            }
            break;
        default:
            wifi_dbg_print(1, "%s(): Invalid event type\n", __FUNCTION__);
            break;
    }
    if(should_publish == TRUE) {
        rbusObject_SetValue(rdata, eventName, value);
        event.name = eventName;
        event.data = rdata;
        event.type = RBUS_EVENT_GENERAL;
        
        rc = rbusEvent_Publish(g_rbus_handle, &event);

        if(rc != RBUS_ERROR_SUCCESS)
        {
            wifi_dbg_print(1, "%s(): rbusEvent_Publish Event failed: %d\n", __FUNCTION__, rc);
        }
    }
    rbusValue_Release(value);
    rbusObject_Release(rdata);
    pthread_mutex_unlock(&g_events_lock);

    return 0;
}

rbusError_t events_subHandler(rbusHandle_t handle, rbusEventSubAction_t action, const char* eventName, rbusFilter_t filter, int32_t interval, bool* autoPublish)
{
    UNREFERENCED_PARAMETER(handle);
    UNREFERENCED_PARAMETER(filter);
    UNREFERENCED_PARAMETER(autoPublish);
    int csi_session = 0;
    unsigned int idx = 0;
    event_element_t *event;
    char *telemetry_start = NULL;
    char *telemetry_cancel = NULL;

    wifi_dbg_print(1, "Entering %s: Event %s\n", __FUNCTION__, eventName);

    pthread_mutex_lock(&g_events_lock);
    event = events_getEventElement((char *)eventName);
    if(event != NULL)
    {
        switch(event->type)
        {
            case monitor_event_type_diagnostics:
                idx = event->idx;
                if(action == RBUS_EVENT_ACTION_SUBSCRIBE)
                {
                    if(interval < MIN_DIAG_INTERVAL)
                    {
                        wifi_dbg_print(1, "WiFi_DiagData_SubscriptionFailed %d\n", idx );
                        CcspWifiTrace(("RDK_LOG_INFO, WiFi_DiagData_SubscriptionFailed %d\n", idx));
                        pthread_mutex_unlock(&g_events_lock);
                        wifi_dbg_print(1, "Exit %s: Event %s\n", __FUNCTION__, eventName);
                        return RBUS_ERROR_BUS_ERROR;
                    }
                    if(gdiag_events_json_buffer[idx-1] == NULL)
                    {
                        gdiag_events_json_buffer[idx-1] = (char *) malloc(CLIENTDIAG_JSON_BUFFER_SIZE*(sizeof(char))*MAX_ASSOCIATED_WIFI_DEVS);
                        if(gdiag_events_json_buffer[idx-1] == NULL)
                        {
                            wifi_dbg_print(1, "WiFi_DiagData_SubscriptionFailed %d\n", idx );
                            CcspWifiTrace(("RDK_LOG_INFO, WiFi_DiagData_SubscriptionFailed %d\n", idx));
                            pthread_mutex_unlock(&g_events_lock);
                            wifi_dbg_print(1, "Exit %s: Event %s\n", __FUNCTION__, eventName);
                            return RBUS_ERROR_BUS_ERROR;
                        }
                        memset(gdiag_events_json_buffer[idx-1], 0, (CLIENTDIAG_JSON_BUFFER_SIZE*(sizeof(char))*MAX_ASSOCIATED_WIFI_DEVS));
                        snprintf(gdiag_events_json_buffer[idx-1], 
                                    CLIENTDIAG_JSON_BUFFER_SIZE*(sizeof(char))*MAX_ASSOCIATED_WIFI_DEVS,
                                    "{"
                                    "\"Version\":\"1.0\","
                                    "\"AssociatedClientsDiagnostics\":["
                                    "{"
                                    "\"VapIndex\":\"%d\","
                                    "\"AssociatedClientDiagnostics\":[]"
                                    "}"
                                    "]"
                                    "}",
                                    idx);
                    }
                    CcspWifiTrace(("RDK_LOG_INFO, WiFi_DiagData_SubscriptionStarted %d\n", idx));

                    event->num_subscribers++;
                    event->subscribed = TRUE;

                    //unlock event mutex before updating monitor data to avoid deadlock
                    pthread_mutex_unlock(&g_events_lock);
                    diagdata_set_interval(interval, idx - 1);
                    
                    wifi_dbg_print(1, "Exit %s: Event %s\n", __FUNCTION__, eventName);
                    return RBUS_ERROR_SUCCESS;
                }
                else
                {
                    CcspWifiTrace(("RDK_LOG_INFO, WiFi_DiagData_SubscriptionCancelled %d\n", idx));
                    event->num_subscribers--;
                    if(event->num_subscribers == 0) {
                        event->subscribed = FALSE;
                        if(gdiag_events_json_buffer[idx-1] != NULL)
                        {
                            free(gdiag_events_json_buffer[idx-1]);
                            gdiag_events_json_buffer[idx-1] = NULL;
                        }
                        //unlock event mutex before updating monitor data to avoid deadlock
                        pthread_mutex_unlock(&g_events_lock);
                        diagdata_set_interval(0, idx - 1);
                        wifi_dbg_print(1, "Exit %s: Event %s\n", __FUNCTION__, eventName);
                        return RBUS_ERROR_SUCCESS;
                    }
                }
                break;

            case monitor_event_type_connect:
            case monitor_event_type_disconnect:
            case monitor_event_type_deauthenticate:
                idx = event->idx;
                if(event->type == monitor_event_type_connect) {
                    telemetry_start = "WiFi_deviceConnected_SubscriptionStarted";
                    telemetry_cancel = "WiFi_deviceConnected_SubscriptionCancelled"; 
                }
                else if( event->type == monitor_event_type_disconnect) {
                    telemetry_start = "WiFi_deviceDisconnected_SubscriptionStarted";
                    telemetry_cancel = "WiFi_deviceDisconnected_SubscriptionCancelled";
                }
                else {
                    telemetry_start = "WiFi_deviceDeauthenticated_SubscriptionStarted";
                    telemetry_cancel = "WiFi_deviceDeauthenticated_SubscriptionCancelled";
                }
                if(action == RBUS_EVENT_ACTION_SUBSCRIBE)
                {
                    event->num_subscribers++;
                    CcspWifiTrace(("RDK_LOG_INFO, %s %d\n", telemetry_start, idx));
                    event->subscribed = TRUE;
                }
                else{
                    CcspWifiTrace(("RDK_LOG_INFO, %s  %d\n",telemetry_cancel, idx));
                    event->num_subscribers--;
                    if(event->num_subscribers == 0) {
                        event->subscribed = FALSE;
                    }
                }
                break;
            
            case monitor_event_type_csi:
                csi_session = event->idx;
                if(action == RBUS_EVENT_ACTION_SUBSCRIBE)
                {
                    /* TODO: interval needs to be multiple of WifiMonitor basic interval */
                    if(interval > MAX_CSI_INTERVAL || interval < MIN_CSI_INTERVAL
                            ||  event->subscribed == TRUE)
                    {
                        //telemetry
                        printf("WiFi_Motion_SubscriptionFailed %d\n", csi_session);
                        wifi_dbg_print(1, "WiFi_Motion_SubscriptionFailed %d\n", csi_session);
                        CcspWifiTrace(("RDK_LOG_INFO, WiFi_CSI_SubscriptionFailed %d\n", csi_session));
                        pthread_mutex_unlock(&g_events_lock);
                        wifi_dbg_print(1, "Exit %s: Event %s\n", __FUNCTION__, eventName);
                        return RBUS_ERROR_BUS_ERROR;
                    }
                    event->subscribed = TRUE;
                    wifi_dbg_print(1, "WiFi_Motion_SubscriptionStarted %d\n", csi_session);
                    CcspWifiTrace(("RDK_LOG_INFO, WiFi_CSI_SubscriptionStarted %d\n", csi_session));

                    //unlock event mutex before updating monitor data to avoid deadlock
                    pthread_mutex_unlock(&g_events_lock);
                    csi_set_interval(interval, csi_session);
                    csi_enable_subscription(TRUE, csi_session);
                    wifi_dbg_print(1, "Exit %s: Event %s\n", __FUNCTION__, eventName);
                    return RBUS_ERROR_SUCCESS;
                }
                else
                {
                    event->subscribed = FALSE;
                    wifi_dbg_print(1, "WiFi_Motion_SubscriptionStopped %d\n", csi_session);
                    CcspWifiTrace(("RDK_LOG_INFO, WiFi_CSI_SubscriptionCancelled %d\n", csi_session));
                    //unlock event mutex before updating monitor data to avoid deadlock
                    pthread_mutex_unlock(&g_events_lock);
                    csi_enable_subscription(FALSE, csi_session);
                    wifi_dbg_print(1, "Exit %s: Event %s\n", __FUNCTION__, eventName);
                    return RBUS_ERROR_SUCCESS;
                }
                break;
            default:
                wifi_dbg_print(1, "%s(): Invalid event type\n", __FUNCTION__);
                break;
        }
    }
    pthread_mutex_unlock(&g_events_lock);
    wifi_dbg_print(1, "Exit %s: Event %s\n", __FUNCTION__, eventName);

    return RBUS_ERROR_SUCCESS;
}

rbusError_t events_APtable_addrowhandler(rbusHandle_t handle, char const* tableName, char const* aliasName, uint32_t* instNum)
{
    static int instanceCounter = 1;
    event_element_t *event;

    *instNum = instanceCounter++;

    wifi_dbg_print(1, "%s(): %s %d\n", __FUNCTION__, tableName, *instNum);

    pthread_mutex_lock(&g_events_lock);

    //Device.WiFi.AccessPoint.{i}.X_RDK_deviceConnected
    event = (event_element_t *) malloc(sizeof(event_element_t));
    if(event != NULL)
    {
        sprintf(event->name, "Device.WiFi.AccessPoint.%d.X_RDK_deviceConnected", *instNum);
        event->idx = *instNum;
        event->type = monitor_event_type_connect;
        event->subscribed = FALSE;
        event->num_subscribers = 0;
        queue_push(g_rbus_events_queue, event);
    }

    //Device.WiFi.AccessPoint.{i}.X_RDK_deviceDisconnected
    event = (event_element_t *) malloc(sizeof(event_element_t));
    if(event != NULL)
    {
        sprintf(event->name, "Device.WiFi.AccessPoint.%d.X_RDK_deviceDisconnected", *instNum);
        event->idx = *instNum;
        event->type = monitor_event_type_disconnect;
        event->subscribed = FALSE;
        event->num_subscribers = 0;
        queue_push(g_rbus_events_queue, event);
    }

    //Device.WiFi.AccessPoint.{i}.X_RDK_deviceDeauthenticated
    event = (event_element_t *) malloc(sizeof(event_element_t));
    if(event != NULL)
    {
        sprintf(event->name, "Device.WiFi.AccessPoint.%d.X_RDK_deviceDeauthenticated", *instNum);
        event->idx = *instNum;
        event->type = monitor_event_type_deauthenticate;
        event->subscribed = FALSE;
        event->num_subscribers = 0;
        queue_push(g_rbus_events_queue, event);
    }

    //Device.WiFi.AccessPoint.{i}.X_RDK_DiagData
    event = (event_element_t *) malloc(sizeof(event_element_t));
    if(event != NULL)
    {
        sprintf(event->name, "Device.WiFi.AccessPoint.%d.X_RDK_DiagData", *instNum);
        event->idx = *instNum;
        event->type = monitor_event_type_diagnostics;
        event->subscribed = FALSE;
        event->num_subscribers = 0;
        queue_push(g_rbus_events_queue, event);
    }

    pthread_mutex_unlock(&g_events_lock);
    wifi_dbg_print(1, "%s(): exit\n", __FUNCTION__);

    UNREFERENCED_PARAMETER(handle);
    UNREFERENCED_PARAMETER(aliasName);
    return RBUS_ERROR_SUCCESS;
}

rbusError_t events_CSItable_addrowhandler(rbusHandle_t handle, char const* tableName, char const* aliasName, uint32_t* instNum)
{
    static int instanceCounter = 1;
    event_element_t *event;
    queue_t** csi_queue = (queue_t**)get_csi_entry_queue();
    csi_data_t *csi_data;

    *instNum = instanceCounter++;

    wifi_dbg_print(1, "%s(): %s %d\n", __FUNCTION__, tableName, *instNum);

    pthread_mutex_lock(&g_events_lock);

    event = (event_element_t *) malloc(sizeof(event_element_t));
    if(event != NULL)
    {
        sprintf(event->name, "Device.WiFi.X_RDK_CSI.%d.data", *instNum);
        event->idx = *instNum;
        event->type = monitor_event_type_csi;
        event->subscribed = FALSE;
    }

    if (*csi_queue == NULL) {
        *csi_queue = queue_create();
        if (*csi_queue == NULL) {
            wifi_dbg_print(1, "%s:%d fail to create csi queue\n", __func__, __LINE__);
            free(event);
            pthread_mutex_unlock(&g_events_lock);
            return RBUS_ERROR_BUS_ERROR;
        }
    }

    csi_data = (csi_data_t *)malloc(sizeof(csi_data_t));
    if (csi_data == NULL) {
        wifi_dbg_print(1, "%s:%d NULL Pointer\n", __func__, __LINE__);
        free(event);
        pthread_mutex_unlock(&g_events_lock);
        return RBUS_ERROR_BUS_ERROR;
    }
    memset(csi_data, 0, sizeof(csi_data_t));
    csi_data->csi_session_num = *instNum;
    
    queue_push(*csi_queue, csi_data);
    queue_push(g_rbus_events_queue, event);

    pthread_mutex_unlock(&g_events_lock);
    
    csi_create_session(csi_data->csi_session_num);

    wifi_dbg_print(1, "%s(): exit\n", __FUNCTION__);

    UNREFERENCED_PARAMETER(handle);
    UNREFERENCED_PARAMETER(aliasName);

    return RBUS_ERROR_SUCCESS;
}

rbusError_t events_APtable_removerowhandler(rbusHandle_t handle, char const* rowName)
{
    int i = 0;
    event_element_t *event;
    int count = queue_count(g_rbus_events_queue);

    wifi_dbg_print(1, "%s(): %s\n", __FUNCTION__, rowName);

    pthread_mutex_lock(&g_events_lock);

    while(i < count)
    {
        event = queue_peek(g_rbus_events_queue, i);
        if ((event != NULL) && (strstr(event->name, rowName) != NULL))
        {
            wifi_dbg_print(1, "%s():event remove from queue %s\n", __FUNCTION__, event->name);
            event = queue_remove(g_rbus_events_queue, i);
            if(event) {
                free(event);
            }
            count--;
        }
        else {
            i++;
        }
    }

    pthread_mutex_unlock(&g_events_lock);

    UNREFERENCED_PARAMETER(handle);

    return RBUS_ERROR_SUCCESS;
}

rbusError_t events_CSItable_removerowhandler(rbusHandle_t handle, char const *rowName)
{
    unsigned int i = 0;
    event_element_t *event = NULL;
    csi_data_t *tmp_csi_data =  NULL;
    unsigned int itr, qcount;
    int csi_sess_number;
    queue_t** csi_queue = (queue_t**)get_csi_entry_queue();

    wifi_dbg_print(1, "%s(): %s\n", __FUNCTION__, rowName);

    pthread_mutex_lock(&g_events_lock);

    if ((*csi_queue == NULL)){
        wifi_dbg_print(1, "%s:%d NULL Pointer\n", __func__, __LINE__);
        pthread_mutex_unlock(&g_events_lock);
        return RBUS_ERROR_BUS_ERROR;
    }

    qcount = queue_count(g_rbus_events_queue);
    while(i < qcount)
    {
        event = queue_peek(g_rbus_events_queue, i);
        if ((event != NULL) && (strstr(event->name, rowName) != NULL))
        {
            wifi_dbg_print(1, "%s():event remove from queue %s\n", __FUNCTION__, event->name);
            event = queue_remove(g_rbus_events_queue, i);
            break;
        }
        else {
            i++;
        }
    }
    if (event == NULL) {
        wifi_dbg_print(1, "%s:%d Could not find entry\n", __func__, __LINE__);
        pthread_mutex_unlock(&g_events_lock);
        return RBUS_ERROR_BUS_ERROR;
    }
    csi_sess_number = event->idx;

    qcount = queue_count(*csi_queue);
    for (itr=0; itr<qcount; itr++) {
        tmp_csi_data = queue_peek(*csi_queue, itr);
        if (tmp_csi_data->csi_session_num == (unsigned long) event->idx) {
            tmp_csi_data = queue_remove(*csi_queue, itr);
            if (tmp_csi_data) {
                free(tmp_csi_data);
            }
            break;
        }
    }
    free(event);

    pthread_mutex_unlock(&g_events_lock);
    csi_del_session(csi_sess_number);

    UNREFERENCED_PARAMETER(handle);

    return RBUS_ERROR_SUCCESS;
}

rbusError_t events_GetHandler(rbusHandle_t handle, rbusProperty_t property, rbusGetHandlerOptions_t* opts)
{
    UNREFERENCED_PARAMETER(handle);
    UNREFERENCED_PARAMETER(opts);
    char const* name;
    rbusValue_t value;
    unsigned int idx = 0;
    int ret;

    pthread_mutex_lock(&g_events_lock);
    name = rbusProperty_GetName(property);
    if (!name)
    {
        pthread_mutex_unlock(&g_events_lock);
        return RBUS_ERROR_INVALID_INPUT;
    }

    wifi_dbg_print(1, "%s(): %s\n", __FUNCTION__, name);

    ret = sscanf(name, "Device.WiFi.AccessPoint.%d.X_RDK_DiagData", &idx);
    if(ret==1 && idx > 0 && idx <= MAX_VAP)
    {
        rbusValue_Init(&value);

        if(gdiag_events_json_buffer[idx-1] != NULL)
        {
            rbusValue_SetString(value, gdiag_events_json_buffer[idx-1]);
        }
        else
        {
            char buffer[500];
            snprintf(buffer,sizeof(buffer),
                                    "{"
                                    "\"Version\":\"1.0\","
                                    "\"AssociatedClientsDiagnostics\":["
                                    "{"
                                    "\"VapIndex\":\"%d\","
                                    "\"AssociatedClientDiagnostics\":[]"
                                    "}"
                                    "]"
                                    "}",
                                    idx);
            rbusValue_SetString(value, buffer);
        }
        rbusProperty_SetValue(property, value);
        rbusValue_Release(value);

        pthread_mutex_unlock(&g_events_lock);
        return RBUS_ERROR_SUCCESS;
    }

    pthread_mutex_unlock(&g_events_lock);
    return RBUS_ERROR_INVALID_INPUT;
}

rbusError_t events_CSIGetHandler(rbusHandle_t handle, rbusProperty_t property, rbusGetHandlerOptions_t* opts)
{
    UNREFERENCED_PARAMETER(handle);
    UNREFERENCED_PARAMETER(opts);
    char const* name;
    rbusValue_t value;
    unsigned int idx = 0;
    int ret;
    char parameter[MAX_EVENT_NAME_SIZE];
    unsigned int itr, count, qcount;
    csi_data_t *csi_data =  NULL;
    queue_t** csi_queue = (queue_t**)get_csi_entry_queue();

    pthread_mutex_lock(&g_events_lock);
    name = rbusProperty_GetName(property);
    if (!name)
    {
        pthread_mutex_unlock(&g_events_lock);
        return RBUS_ERROR_INVALID_INPUT;
    }

    wifi_dbg_print(1,  "%s(): %s\n", __FUNCTION__, name);
    if (strcmp(name, "Device.WiFi.X_RDK_CSINumberOfEntries") == 0) {
        queue_t** csi_queue = (queue_t**)get_csi_entry_queue();
        if ((csi_queue == NULL) || (*csi_queue == NULL)) {
            wifi_dbg_print(1, "%s:%d invalid queue pointer\n", __func__, __LINE__);
            pthread_mutex_unlock(&g_events_lock);
            return RBUS_ERROR_BUS_ERROR;
        }

        count = queue_count(*csi_queue);
        rbusValue_Init(&value);
        rbusValue_SetUInt32(value, count);
        rbusProperty_SetValue(property, value);
        rbusValue_Release(value);

        pthread_mutex_unlock(&g_events_lock);
        return RBUS_ERROR_SUCCESS;
    }

    ret = sscanf(name, "Device.WiFi.X_RDK_CSI.%d.%s", &idx, parameter);
    if(ret==2 && idx > 0 && idx <= MAX_VAP)
    {
        qcount = queue_count(*csi_queue);
        for (itr=0; itr<qcount; itr++) {
            csi_data = queue_peek(*csi_queue, itr);
            if (csi_data->csi_session_num == idx) {
                break;
            }
        }

        if (csi_data == NULL) {
            wifi_dbg_print(1, "%s:%d Could not find entry\n", __func__, __LINE__);
            pthread_mutex_unlock(&g_events_lock);
            return RBUS_ERROR_BUS_ERROR;
        }

        rbusValue_Init(&value);
        if (strcmp(parameter, "ClientMaclist") == 0) {
            char tmp_cli_list[128];
            mac_addr_str_t mac_str;
            memset(tmp_cli_list, 0, sizeof(tmp_cli_list));
            if (csi_data->csi_client_count > 0) {
                for (itr=0; itr<csi_data->csi_client_count; itr++) {
                    to_mac_str(csi_data->csi_client_list[itr], mac_str);
                    strcat(tmp_cli_list, mac_str);
                    strcat(tmp_cli_list, ",");
                }
                int len  = strlen(tmp_cli_list);
                tmp_cli_list[len-1] = '\0';
            }
            rbusValue_SetString(value, tmp_cli_list);
        } else if(strcmp(parameter, "Enable") == 0) {
            rbusValue_SetBoolean(value, csi_data->enabled);
        }
        rbusProperty_SetValue(property, value);
        rbusValue_Release(value);

        pthread_mutex_unlock(&g_events_lock);
        return RBUS_ERROR_SUCCESS;
    }

    pthread_mutex_unlock(&g_events_lock);
    return RBUS_ERROR_INVALID_INPUT;
}

static void str_to_mac_bytes (char *key, mac_addr_t bmac) {
   unsigned int mac[6];
   if(strlen(key) > 12)
       sscanf(key, "%02x:%02x:%02x:%02x:%02x:%02x",
             &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
   else
       sscanf(key, "%02x%02x%02x%02x%02x%02x",
             &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
   bmac[0] = mac[0]; bmac[1] = mac[1]; bmac[2] = mac[2];
   bmac[3] = mac[3]; bmac[4] = mac[4]; bmac[5] = mac[5];

}

rbusError_t events_CSISetHandler(rbusHandle_t handle, rbusProperty_t property, rbusSetHandlerOptions_t* opts)
{
    (void)handle;
    (void)opts;
    char const* name;
    rbusValue_t value;
    rbusValueType_t type;
    unsigned int idx = 0;
    int ret, apply = false;
    char parameter[MAX_EVENT_NAME_SIZE];
    unsigned int itr, i, j, k, qcount, num_unique_mac=0;
    csi_data_t *csi_data =  NULL, *tmp_csi_data;
    mac_address_t unique_mac_list[MAX_NUM_CSI_CLIENTS];
    bool found = false;
    unsigned int csi_client_count;
    mac_address_t csi_client_list[MAX_CSI_CLIENTS_PER_SESSION];
    queue_t** csi_queue = (queue_t**)get_csi_entry_queue();
    
    name = rbusProperty_GetName(property);
    value = rbusProperty_GetValue(property);
    type = rbusValue_GetType(value);

    if (!name)
    {
        return RBUS_ERROR_INVALID_INPUT;
    }
    pthread_mutex_lock(&g_events_lock);

    wifi_dbg_print(1,  "%s(): %s\n", __FUNCTION__, name);

    ret = sscanf(name, "Device.WiFi.X_RDK_CSI.%d.%s", &idx, parameter);
    if(ret==2 && idx > 0 && idx <= MAX_VAP)
    {
        qcount = queue_count(*csi_queue);
        for (itr=0; itr<qcount; itr++) {
            csi_data = queue_peek(*csi_queue, itr);
            if (csi_data->csi_session_num == idx) {
                break;
            }
        }

        if (csi_data == NULL) {
            wifi_dbg_print(1, "%s:%d Could not find entry\n", __func__, __LINE__);
            pthread_mutex_unlock(&g_events_lock);
            return RBUS_ERROR_BUS_ERROR;
        }
        if (strcmp(parameter, "ClientMaclist") == 0) {

            if (type != RBUS_STRING)
            {
                wifi_dbg_print(1, "%s:%d '%s' Called Set handler with wrong data type\n", __func__, __LINE__, name);
                pthread_mutex_unlock(&g_events_lock);
                return RBUS_ERROR_INVALID_INPUT;
            } else {
                char *str, *cptr, *str_dup; 
                mac_address_t l_client_list[MAX_CSI_CLIENTS_PER_SESSION];
                char const* pTmp = NULL;
                int len = 0;
                memset(l_client_list, 0, MAX_CSI_CLIENTS_PER_SESSION*sizeof(mac_address_t));
 
                pTmp = rbusValue_GetString(value, &len);
                str_dup = strdup(pTmp);
                if (str_dup == NULL) {
                    wifi_dbg_print(1, "%s:%d strdup failed\n", __func__, __LINE__);
                    pthread_mutex_unlock(&g_events_lock);
                    return RBUS_ERROR_BUS_ERROR;
                }
                itr = 0;
                str = strtok_r(str_dup, ",", &cptr);
                while (str != NULL) {
                    str_to_mac_bytes(str, l_client_list[itr]);
                    str = strtok_r(NULL, ",", &cptr);
                    itr++;
                    if (itr > MAX_CSI_CLIENTS_PER_SESSION) {
                        wifi_dbg_print(1, "%s:%d client list is big %d\n", __func__, __LINE__, itr);
                        if (str_dup) {
                            free(str_dup);
                        }
                        pthread_mutex_unlock(&g_events_lock);
                        return RBUS_ERROR_BUS_ERROR;
                    }
                }
                
                if (memcmp(csi_data->csi_client_list, l_client_list,  MAX_CSI_CLIENTS_PER_SESSION*sizeof(mac_address_t)) != 0) {
                    //check new configuration did not exceed the max number of csi clients 
                    num_unique_mac = 0;
                    for (i=0; i<qcount; i++) {
                        tmp_csi_data = (csi_data_t *)queue_peek(*csi_queue, i);
                        if ((tmp_csi_data != NULL) && (tmp_csi_data->enabled)) {
                            if (tmp_csi_data->csi_session_num == csi_data->csi_session_num) {
                                csi_client_count = itr;
                                memcpy(csi_client_list, l_client_list,  MAX_CSI_CLIENTS_PER_SESSION*sizeof(mac_address_t));
                            } else {
                                csi_client_count = tmp_csi_data->csi_client_count;
                                memcpy(csi_client_list, tmp_csi_data->csi_client_list,  MAX_CSI_CLIENTS_PER_SESSION*sizeof(mac_address_t));
                            }
                            for (j=0; j < csi_client_count; j++) {
                                found  = false;
                                for (k=0; k < num_unique_mac; k++) {
                                    if (memcmp(csi_client_list[j], unique_mac_list[k], sizeof(mac_address_t)) == 0) {
                                        found  = true;
                                        break;
                                    }
                                }
                                if (!found) {
                                    num_unique_mac++;
                                    if (num_unique_mac > MAX_NUM_CSI_CLIENTS) {
                                        wifi_dbg_print(1, "%s %d MAX_NUM_CSI_CLIENTS reached\n", __func__, __LINE__);
                                        if (str_dup) {
                                            free(str_dup);
                                        }
                                        pthread_mutex_unlock(&g_events_lock);
                                        return RBUS_ERROR_BUS_ERROR;
                                    } else {
                                        memcpy(unique_mac_list[num_unique_mac-1], csi_client_list[j], sizeof(mac_address_t));
                                    }
                                }
                            }
                        }
                    }

                    memcpy(csi_data->csi_client_list, l_client_list,  MAX_CSI_CLIENTS_PER_SESSION*sizeof(mac_address_t));
                    csi_data->csi_client_count = itr;
                    apply = true;
                } else {
                    wifi_dbg_print(1, "%s:%d config not change\n", __func__, __LINE__);
                }
                if (str_dup) {
                    free(str_dup);
                }
                //unlock event mutex before updating monitor data to avoid deadlock
                pthread_mutex_unlock(&g_events_lock);
                if (apply==true) {
                    csi_set_client_mac((char *)pTmp, csi_data->csi_session_num);
                }
                return RBUS_ERROR_SUCCESS;
            }

        } else if(strcmp(parameter, "Enable") == 0) {
            if (type != RBUS_BOOLEAN)
            {
                wifi_dbg_print(1, "%s:%d '%s' Called Set handler with wrong data type\n", __func__, __LINE__, name);
                pthread_mutex_unlock(&g_events_lock);
                return RBUS_ERROR_INVALID_INPUT;
            } else {
                bool enabled = rbusValue_GetBoolean(value);
                if (enabled != csi_data->enabled) {
                    //check new configuration did not exceed the max number of csi clients
                    num_unique_mac = 0;
                    if (enabled == true) {
                        for (i=0; i<qcount; i++) {
                            tmp_csi_data = (csi_data_t *)queue_peek(*csi_queue, i);
                            if (tmp_csi_data != NULL) {
                                if (tmp_csi_data->csi_session_num != csi_data->csi_session_num) {
                                    if (tmp_csi_data->enabled == false) {
                                        continue;
                                    }
                                }
                                for (j=0; j < tmp_csi_data->csi_client_count; j++) {
                                    found  = false;
                                    for (k=0; k < num_unique_mac; k++) {
                                        if (memcmp(tmp_csi_data->csi_client_list[j], unique_mac_list[k], sizeof(mac_address_t)) == 0) {
                                            found  = true;
                                            break;
                                        }
                                    }
                                    if (!found) {
                                        num_unique_mac++;
                                        if (num_unique_mac > MAX_NUM_CSI_CLIENTS) {
                                            wifi_dbg_print(1, "%s %d MAX_NUM_CSI_CLIENTS reached\n", __func__, __LINE__);
                                            pthread_mutex_unlock(&g_events_lock);
                                            return RBUS_ERROR_BUS_ERROR;
                                        } else {
                                            memcpy(unique_mac_list[num_unique_mac-1], tmp_csi_data->csi_client_list[j], sizeof(mac_address_t));
                                        }
                                    }
                                }
                            }
                        }
                    }
                    csi_data->enabled = enabled;
                    apply = true;
                }
                //unlock event mutex before updating monitor data to avoid deadlock
                pthread_mutex_unlock(&g_events_lock);
                if (apply==true) {
                    csi_enable_session(csi_data->enabled, csi_data->csi_session_num);
                }
                return RBUS_ERROR_SUCCESS;
            }
        }

        pthread_mutex_unlock(&g_events_lock);
        return RBUS_ERROR_BUS_ERROR;
    }

    pthread_mutex_unlock(&g_events_lock);
    return RBUS_ERROR_INVALID_INPUT;
}

int events_deinit(void)
{
    event_element_t *event;

    if(g_isRbusAvailable == FALSE)
    {
        return 0;
    }

    wifi_dbg_print(1, "%s():\n", __FUNCTION__);
    pthread_mutex_lock(&g_events_lock);

    do
    {
        event = queue_pop(g_rbus_events_queue);
        free(event);
    } while (event != NULL);

    queue_t** csi_queue = (queue_t **)get_csi_entry_queue();
    if ((csi_queue != NULL) && (*csi_queue != NULL)) {
        queue_destroy(*csi_queue);
    }

    rbus_close(g_rbus_handle);
    pthread_mutex_unlock(&g_events_lock);

    pthread_mutex_destroy(&g_events_lock);

    return 0;
}
#endif
