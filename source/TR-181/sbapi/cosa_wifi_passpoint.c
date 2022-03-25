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

#include "wifi_data_plane.h"
#include "wifi_monitor.h"
#include "plugin_main_apis.h"
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/un.h>
#include <assert.h>
#include "ansc_status.h"
#include <sysevent/sysevent.h>
#include <cjson/cJSON.h>
#include <dirent.h>
#include <errno.h>
#include "ccsp_psm_helper.h"
#include "webconfig_framework.h"
#include "safec_lib_common.h"

#define GAS_CFG_TYPE_SUPPORTED 1
#define GAS_STATS_FIXED_WINDOW_SIZE 10
#define GAS_STATS_TIME_OUT 60

static wifi_interworking_t g_interworking_data[16];
static COSA_DML_WIFI_GASSTATS gasStats[GAS_CFG_TYPE_SUPPORTED];
 
extern ANSC_HANDLE bus_handle;
extern char        g_Subsystem[32];
extern wifi_data_plane_t g_data_plane_module;
extern PCOSA_BACKEND_MANAGER_OBJECT g_pCosaBEManager;

extern UINT g_interworking_RFC;
extern UINT g_passpoint_RFC;

void destroy_passpoint (void);

INT wifi_setGASConfiguration(UINT advertisementID, wifi_GASConfiguration_t *input_struct);

static wifi_passpoint_t g_passpoint;


void wifi_passpoint_dbg_print(char *format, ...)
{
    char buff[2048] = {0};
    va_list list;
    static FILE *fpg = NULL;
    errno_t rc = -1;

    if ((access("/nvram/wifiPasspointDbg", R_OK)) != 0) {
        return;
    }

    get_formatted_time(buff);
    rc = strcat_s(buff, sizeof(buff), " ");
    ERR_CHK(rc);

    va_start(list, format);
    vsprintf(&buff[strlen(buff)], format, list);
    va_end(list);

    if (fpg == NULL) {
        fpg = fopen("/tmp/wifiPasspoint", "a+");
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

PCOSA_DML_WIFI_AP_CFG find_ap_wifi_dml(unsigned int apIndex)
{
    PCOSA_DATAMODEL_WIFI pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PSINGLE_LINK_ENTRY  pSLinkEntry = NULL;
    PCOSA_CONTEXT_LINK_OBJECT pAPLinkObj = NULL;
    PCOSA_DML_WIFI_AP pWifiAp = NULL;
    PCOSA_DML_WIFI_AP_FULL pEntry;

    if((pMyObject = g_passpoint.wifi_dml) == NULL){
        return NULL;
    }
    if(((PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi) == g_passpoint.wifi_dml){
    	wifi_passpoint_dbg_print(" Current instance matches. \n");
    } else {
        wifi_passpoint_dbg_print(" Current instance does not match. Resetting\n");
        g_passpoint.wifi_dml = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    }

    if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->AccessPointQueue, apIndex)) == NULL){
        wifi_passpoint_dbg_print(" pSLinkEntry is NULL. \n");
        return NULL;
    }

    if((pAPLinkObj = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)) == NULL){
        wifi_passpoint_dbg_print(" pAPLinkObj is NULL. \n");
        return NULL;
    }

    if((pWifiAp = pAPLinkObj->hContext) == NULL){
        wifi_passpoint_dbg_print(" pWifiAp is NULL. \n");
        return NULL;
    }

    if((pEntry = &pWifiAp->AP) == NULL){
        wifi_passpoint_dbg_print(" pEntry is NULL. \n");
        return NULL;
    }
    wifi_passpoint_dbg_print(" returning %x. \n",&pEntry->Cfg);
    return &pEntry->Cfg;
}

static long readFileToBuffer(const char *fileName, char **buffer)
{
    FILE    *infile = NULL;
    long    numbytes;
    DIR     *passPointDir = NULL;
   
    passPointDir = opendir(WIFI_PASSPOINT_DIR);
    if(passPointDir){
        closedir(passPointDir);
    }else if(ENOENT == errno){
        if(0 != mkdir(WIFI_PASSPOINT_DIR, 0777)){
            wifi_passpoint_dbg_print("Failed to Create Passpoint Configuration directory.\n");
            return 0;
        }
    }else{
        wifi_passpoint_dbg_print("Error opening Passpoint Configuration directory.\n");
        return 0;
    } 
 
    infile = fopen(fileName, "r");
 
    /* quit if the file does not exist */
    if(infile == NULL)
        return 0;
 
    /* Get the number of bytes */
    fseek(infile, 0L, SEEK_END);
    numbytes = ftell(infile);
    /*CID: 121788 Argument cannot be negative*/
    if (numbytes < 0) {
        wifi_passpoint_dbg_print("Error in getting the num of bytes\n");
        fclose(infile);
        return 0;
    } 
    /* reset the file position indicator to 
    the beginning of the file */
    fseek(infile, 0L, SEEK_SET);	
 
    /* grab sufficient memory for the 
    buffer to hold the text */
    *buffer = (char*)calloc(numbytes+1, sizeof(char));	
 
    /* memory error */
    if(*buffer == NULL){
        fclose(infile);
        return 0;
    }
 
    /* copy all the text into the buffer */
    /*CID:121787 Ignoring number of bytes read*/
    if(1 != fread(*buffer, numbytes, 1, infile)) {
       wifi_passpoint_dbg_print("Failed to read the buffer.\n");
       fclose(infile);
       return 0;
    }
    return numbytes;
}

void process_passpoint_timeout()
{
//GAS Rate computation variables
    static struct timeval last_clean_time;
    static BOOL firstTime  = true;

    static UINT gas_query_rate_queue[GAS_STATS_FIXED_WINDOW_SIZE];
    static UINT gas_response_rate_queue[GAS_STATS_FIXED_WINDOW_SIZE];
    
    static UINT gas_query_rate_window_sum;
    static UINT gas_response_rate_window_sum;
  
    static UCHAR gas_rate_head;

    static UINT gas_queries_per_minute_old;
    static UINT gas_responses_per_minute_old;

    static UCHAR gas_rate_divisor;

    if (firstTime){
        firstTime = false;
        gettimeofday(&last_clean_time, NULL);
    }

    struct timeval curr_time;
    gettimeofday(&curr_time, NULL);

    if ((GAS_STATS_TIME_OUT <= (curr_time.tv_sec - last_clean_time.tv_sec)) ||
        ((curr_time.tv_sec > GAS_STATS_TIME_OUT) &&
         (last_clean_time.tv_sec > curr_time.tv_sec)))
    {
        UCHAR gas_rate_tail = (gas_rate_head + 1) % GAS_STATS_FIXED_WINDOW_SIZE;
      
        UINT gas_queries_per_minute_new = gasStats[GAS_CFG_TYPE_SUPPORTED - 1].Queries - gas_queries_per_minute_old;
        gas_queries_per_minute_old = gasStats[GAS_CFG_TYPE_SUPPORTED - 1].Queries;
      
        gas_query_rate_window_sum = gas_query_rate_window_sum - gas_query_rate_queue[gas_rate_tail] + gas_queries_per_minute_new;
            
        UINT gas_responses_per_minute_new = gasStats[GAS_CFG_TYPE_SUPPORTED - 1].Responses - gas_responses_per_minute_old;
        gas_responses_per_minute_old = gasStats[GAS_CFG_TYPE_SUPPORTED - 1].Responses;
          
        gas_response_rate_window_sum = gas_response_rate_window_sum - gas_response_rate_queue[gas_rate_tail] + gas_responses_per_minute_new;
            
        //move the head
        gas_rate_head = (gas_rate_head + 1) % GAS_STATS_FIXED_WINDOW_SIZE;
        gas_query_rate_queue[gas_rate_head] = gas_queries_per_minute_new;
        gas_response_rate_queue[gas_rate_head] = gas_responses_per_minute_new;
          
        if (gas_rate_divisor < GAS_STATS_FIXED_WINDOW_SIZE)
        {
            gas_rate_divisor++;//Increment the divisor for the first 10 minutes.
        }

        if (gas_rate_divisor)
        {
            //update stats with calculated values
            gasStats[GAS_CFG_TYPE_SUPPORTED - 1].QueryRate = gas_query_rate_window_sum / gas_rate_divisor;
            gasStats[GAS_CFG_TYPE_SUPPORTED - 1].ResponseRate = gas_response_rate_window_sum / gas_rate_divisor;
        }
      
        last_clean_time.tv_sec = curr_time.tv_sec;
    }
}

void process_passpoint_event(cosa_wifi_anqp_context_t *anqpReq)
{
    char *strValue = NULL;
    int retPsmGet = CCSP_SUCCESS;
    wifi_anqp_node_t *anqpList = NULL;
    int respLength = 0;
    int apIns;
    int mallocRetryCount = 0;
    int capLen;
    UCHAR wfa_oui[3] = {0x50, 0x6f, 0x9a};
    UCHAR *data_pos = NULL;

    respLength = 0;
    /*CID: 159997 Dereference before null check*/
    if(!anqpReq)
       return;
    apIns = anqpReq->apIndex;
    /*CID: 159998,159995  Out-of-bounds read*/
    if((apIns < 0) || (apIns > 15)){
        wifi_passpoint_dbg_print( "%s:%d: Invalid AP Index: %d.\n", __func__, __LINE__,apIns);
        return;
    }
      
    //A gas query received increase the stats.
    gasStats[GAS_CFG_TYPE_SUPPORTED - 1].Queries++;

    //Check RFC value. Return NUll is not enabled
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.WiFi-Passpoint.Enable", NULL, &strValue);
    if ((retPsmGet != CCSP_SUCCESS) || (false == _ansc_atoi(strValue))){
        if(strValue) {
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
        }
        wifi_passpoint_dbg_print( "%s:%d: Received ANQP Request. RFC Disabled\n", __func__, __LINE__);
    }else if(g_interworking_data[apIns].passpoint.enable != true){
        wifi_passpoint_dbg_print( "%s:%d: Received ANQP Request. Passpoint is Disabled on AP: %d\n", __func__, __LINE__,apIns);
    }else{
        anqpList = anqpReq->head;
    }

    //Free the RFC string
    ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);

#if defined (FEATURE_SUPPORT_PASSPOINT)
    UINT prevRealmCnt = g_interworking_data[apIns].anqp.realmRespCount;
    UINT prevDomainCnt = g_interworking_data[apIns].anqp.domainRespCount;
    UINT prev3gppCnt = g_interworking_data[apIns].anqp.gppRespCount;
#endif

    while(anqpList){
        anqpList->value->len = 0;
        if(anqpList->value->data){
            free(anqpList->value->data);
            anqpList->value->data = NULL;
        }
        if(anqpList->value->type == wifi_anqp_id_type_anqp){
            wifi_passpoint_dbg_print( "%s:%d: Received ANQP Request\n", __func__, __LINE__);
            switch (anqpList->value->u.anqp_elem_id){
                //CapabilityListANQPElement
                case wifi_anqp_element_name_capability_list:
                    capLen = (g_interworking_data[apIns].anqp.capabilityInfoLength * sizeof(USHORT)) + sizeof(wifi_vendor_specific_anqp_capabilities_t) + g_interworking_data[apIns].passpoint.capabilityInfoLength;
                    wifi_passpoint_dbg_print( "%s:%d: Received CapabilityListANQPElement Request\n", __func__, __LINE__);
                    anqpList->value->data = malloc(capLen);//To be freed in wifi_anqpSendResponse()
                    if(NULL == anqpList->value->data){
                        wifi_passpoint_dbg_print( "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                        if(mallocRetryCount > 5){
                            break;
                        }
                        mallocRetryCount++;
                        anqpList = anqpList->next;
                        continue;
                    }
                    data_pos = anqpList->value->data;
                    wifi_passpoint_dbg_print( "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,capLen);
                    memset(data_pos,0,capLen);
                    memcpy(data_pos,&g_interworking_data[apIns].anqp.capabilityInfo,(g_interworking_data[apIns].anqp.capabilityInfoLength * sizeof(USHORT)));
                    data_pos += (g_interworking_data[apIns].anqp.capabilityInfoLength * sizeof(USHORT));
                    wifi_vendor_specific_anqp_capabilities_t *vendorInfo = (wifi_vendor_specific_anqp_capabilities_t *)data_pos;
                    vendorInfo->info_id = wifi_anqp_element_name_vendor_specific;
                    vendorInfo->len = g_interworking_data[apIns].passpoint.capabilityInfoLength + sizeof(vendorInfo->oi) + sizeof(vendorInfo->wfa_type);
                    memcpy(vendorInfo->oi, wfa_oui, sizeof(wfa_oui));
                    vendorInfo->wfa_type = 0x11;
                    data_pos += sizeof(wifi_vendor_specific_anqp_capabilities_t);
                    memcpy(data_pos, &g_interworking_data[apIns].passpoint.capabilityInfo, g_interworking_data[apIns].passpoint.capabilityInfoLength);
                    anqpList->value->len = capLen;
                    respLength += anqpList->value->len;
                    wifi_passpoint_dbg_print( "%s:%d: Copied CapabilityListANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                    break;
                //IPAddressTypeAvailabilityANQPElement
                case wifi_anqp_element_name_ip_address_availabality:
                    wifi_passpoint_dbg_print( "%s:%d: Received IPAddressTypeAvailabilityANQPElement Request\n", __func__, __LINE__);
                    anqpList->value->data = malloc(sizeof(wifi_ipAddressAvailabality_t));//To be freed in wifi_anqpSendResponse()
                    if(NULL == anqpList->value->data){
                        wifi_passpoint_dbg_print( "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                        if(mallocRetryCount > 5){
                            break;
                        }
                        mallocRetryCount++;
                        anqpList = anqpList->next;
                        continue;
                    }
                    mallocRetryCount = 0;
                    wifi_passpoint_dbg_print( "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,sizeof(wifi_ipAddressAvailabality_t));
                    memset(anqpList->value->data,0,sizeof(wifi_ipAddressAvailabality_t));
                    memcpy(anqpList->value->data,&g_interworking_data[apIns].anqp.ipAddressInfo,sizeof(wifi_ipAddressAvailabality_t));
                    anqpList->value->len = sizeof(wifi_ipAddressAvailabality_t);
                    respLength += anqpList->value->len;
                    wifi_passpoint_dbg_print( "%s:%d: Copied IPAddressTypeAvailabilityANQPElement Data. Length: %d. Data: %02X\n", __func__, __LINE__,anqpList->value->len, ((wifi_ipAddressAvailabality_t *)anqpList->value->data)->field_format);
                    break;
                //NAIRealmANQPElement
                case wifi_anqp_element_name_nai_realm:
                    wifi_passpoint_dbg_print( "%s:%d: Received NAIRealmANQPElement Request\n", __func__, __LINE__);
                    if(g_interworking_data[apIns].anqp.realmInfoLength){
                        anqpList->value->data = malloc(g_interworking_data[apIns].anqp.realmInfoLength);//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                            wifi_passpoint_dbg_print( "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                            if(mallocRetryCount > 5){
                                break;
                            }
                            mallocRetryCount++;
                            anqpList = anqpList->next;
                            continue;
                        }
                        wifi_passpoint_dbg_print( "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,g_interworking_data[apIns].anqp.realmInfoLength);
                        memset(anqpList->value->data,0,g_interworking_data[apIns].anqp.realmInfoLength);
                        memcpy(anqpList->value->data,&g_interworking_data[apIns].anqp.realmInfo,g_interworking_data[apIns].anqp.realmInfoLength);
                        anqpList->value->len = g_interworking_data[apIns].anqp.realmInfoLength;
                        respLength += anqpList->value->len;
                        wifi_passpoint_dbg_print( "%s:%d: Copied NAIRealmANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
              
                        g_interworking_data[apIns].anqp.realmRespCount++;
                    }
                    break;
                //VenueNameANQPElement
                case wifi_anqp_element_name_venue_name:
                    wifi_passpoint_dbg_print( "%s:%d: Received VenueNameANQPElement Request\n", __func__, __LINE__);
                    if(g_interworking_data[apIns].anqp.venueInfoLength){
                        anqpList->value->data = malloc(g_interworking_data[apIns].anqp.venueInfoLength);//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                            wifi_passpoint_dbg_print( "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                            if(mallocRetryCount > 5){
                                break;
                            }
                            mallocRetryCount++;
                            anqpList = anqpList->next;
                            continue;
                        }
                        wifi_passpoint_dbg_print( "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,g_interworking_data[apIns].anqp.venueInfoLength);
                        memset(anqpList->value->data,0,g_interworking_data[apIns].anqp.venueInfoLength);
                        memcpy(anqpList->value->data,&g_interworking_data[apIns].anqp.venueInfo,g_interworking_data[apIns].anqp.venueInfoLength);
                        anqpList->value->len = g_interworking_data[apIns].anqp.venueInfoLength;
                        respLength += anqpList->value->len;
                        wifi_passpoint_dbg_print( "%s:%d: Copied VenueNameANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                    }
                    break;
                //3GPPCellularANQPElement
                case wifi_anqp_element_name_3gpp_cellular_network:
                    wifi_passpoint_dbg_print( "%s:%d: Received 3GPPCellularANQPElement Request\n", __func__, __LINE__);
                    if(g_interworking_data[apIns].anqp.gppInfoLength){
                        anqpList->value->data = malloc(g_interworking_data[apIns].anqp.gppInfoLength);//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                            wifi_passpoint_dbg_print( "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                            if(mallocRetryCount > 5){
                                break;
                            }
                            mallocRetryCount++;
                            anqpList = anqpList->next;
                            continue;
                        }
                        wifi_passpoint_dbg_print( "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,g_interworking_data[apIns].anqp.gppInfoLength);
                        memset(anqpList->value->data,0,g_interworking_data[apIns].anqp.gppInfoLength);
                        memcpy(anqpList->value->data,&g_interworking_data[apIns].anqp.gppInfo,g_interworking_data[apIns].anqp.gppInfoLength);
                        anqpList->value->len = g_interworking_data[apIns].anqp.gppInfoLength;
                        respLength += anqpList->value->len;
                        wifi_passpoint_dbg_print( "%s:%d: Copied 3GPPCellularANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                        g_interworking_data[apIns].anqp.gppRespCount++;
                    }
                    break;
                //RoamingConsortiumANQPElement
                case wifi_anqp_element_name_roaming_consortium:
                    wifi_passpoint_dbg_print( "%s:%d: Received RoamingConsortiumANQPElement Request\n", __func__, __LINE__);
                    if(g_interworking_data[apIns].anqp.roamInfoLength){
                        anqpList->value->data = malloc(g_interworking_data[apIns].anqp.roamInfoLength);//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                            wifi_passpoint_dbg_print( "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                            if(mallocRetryCount > 5){
                                break;
                            }
                            mallocRetryCount++;
                            anqpList = anqpList->next;
                            continue;
                        }
                        wifi_passpoint_dbg_print( "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,g_interworking_data[apIns].anqp.roamInfoLength);
                        memset(anqpList->value->data,0,g_interworking_data[apIns].anqp.roamInfoLength);
                        memcpy(anqpList->value->data,&g_interworking_data[apIns].anqp.roamInfo,g_interworking_data[apIns].anqp.roamInfoLength);
                        anqpList->value->len = g_interworking_data[apIns].anqp.roamInfoLength;
                        respLength += anqpList->value->len;
                        wifi_passpoint_dbg_print( "%s:%d: Copied RoamingConsortiumANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                    }
                    break;
                //DomainANQPElement
                case wifi_anqp_element_name_domain_name:
                    wifi_passpoint_dbg_print( "%s:%d: Received DomainANQPElement Request\n", __func__, __LINE__);
                    if(g_interworking_data[apIns].anqp.domainInfoLength){
                        anqpList->value->data = malloc(g_interworking_data[apIns].anqp.domainInfoLength);//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                            wifi_passpoint_dbg_print( "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                            if(mallocRetryCount > 5){
                                break;
                            }
                            mallocRetryCount++;
                            anqpList = anqpList->next;
                            continue;
                        }
                        wifi_passpoint_dbg_print( "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,g_interworking_data[apIns].anqp.domainInfoLength);
                        memset(anqpList->value->data,0,g_interworking_data[apIns].anqp.domainInfoLength);
                        memcpy(anqpList->value->data,&g_interworking_data[apIns].anqp.domainNameInfo,g_interworking_data[apIns].anqp.domainInfoLength);
                        anqpList->value->len = g_interworking_data[apIns].anqp.domainInfoLength;
                        respLength += anqpList->value->len;
                        wifi_passpoint_dbg_print( "%s:%d: Copied DomainANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                        g_interworking_data[apIns].anqp.domainRespCount++;
                    }
                    break;
               default:
                    wifi_passpoint_dbg_print( "%s:%d: Received Unsupported ANQPElement Request: %d\n", __func__, __LINE__,anqpList->value->u.anqp_elem_id);
                    break;
            }     
        } else if (anqpList->value->type == wifi_anqp_id_type_hs){
            wifi_passpoint_dbg_print( "%s:%d: Received HS2 ANQP Request\n", __func__, __LINE__);
            switch (anqpList->value->u.anqp_hs_id){
                //CapabilityListANQPElement
                case wifi_anqp_element_hs_subtype_hs_capability_list:
                    wifi_passpoint_dbg_print( "%s:%d: Received CapabilityListANQPElement Request\n", __func__, __LINE__);
                    if(g_interworking_data[apIns].passpoint.capabilityInfoLength){
                        anqpList->value->data = malloc(g_interworking_data[apIns].passpoint.capabilityInfoLength);//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                            wifi_passpoint_dbg_print( "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                            if(mallocRetryCount > 5){
                                break;
                            }
                            mallocRetryCount++;
                            anqpList = anqpList->next;
                            continue;
                        }
                        wifi_passpoint_dbg_print( "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,g_interworking_data[apIns].passpoint.capabilityInfoLength);
                        memset(anqpList->value->data,0,g_interworking_data[apIns].passpoint.capabilityInfoLength);
                        memcpy(anqpList->value->data,&g_interworking_data[apIns].passpoint.capabilityInfo,g_interworking_data[apIns].passpoint.capabilityInfoLength);
                        anqpList->value->len = g_interworking_data[apIns].passpoint.capabilityInfoLength;
                        respLength += anqpList->value->len;
                        wifi_passpoint_dbg_print( "%s:%d: Copied CapabilityListANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                    }
                    break;
                //OperatorFriendlyNameANQPElement
                case wifi_anqp_element_hs_subtype_operator_friendly_name:
                    wifi_passpoint_dbg_print( "%s:%d: Received OperatorFriendlyNameANQPElement Request\n", __func__, __LINE__);
                    if(g_interworking_data[apIns].passpoint.opFriendlyNameInfoLength){
                        anqpList->value->data = malloc(g_interworking_data[apIns].passpoint.opFriendlyNameInfoLength);//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                            wifi_passpoint_dbg_print( "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                            if(mallocRetryCount > 5){
                                break;
                            }
                            mallocRetryCount++;
                            anqpList = anqpList->next;
                            continue;
                        }
                        wifi_passpoint_dbg_print( "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,g_interworking_data[apIns].passpoint.opFriendlyNameInfoLength);
                        memset(anqpList->value->data,0,g_interworking_data[apIns].passpoint.opFriendlyNameInfoLength);
                        memcpy(anqpList->value->data,&g_interworking_data[apIns].passpoint.opFriendlyNameInfo,g_interworking_data[apIns].passpoint.opFriendlyNameInfoLength);
                        anqpList->value->len = g_interworking_data[apIns].passpoint.opFriendlyNameInfoLength;
                        respLength += anqpList->value->len;
                        wifi_passpoint_dbg_print( "%s:%d: Copied OperatorFriendlyNameANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                    }
                    break;
                //ConnectionCapabilityListANQPElement
                case wifi_anqp_element_hs_subtype_conn_capability:
                    wifi_passpoint_dbg_print( "%s:%d: Received ConnectionCapabilityListANQPElement Request\n", __func__, __LINE__);
                    if(g_interworking_data[apIns].passpoint.connCapabilityLength){
                        anqpList->value->data = malloc(g_interworking_data[apIns].passpoint.connCapabilityLength);//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                            wifi_passpoint_dbg_print( "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                            if(mallocRetryCount > 5){
                                break;
                            }
                            mallocRetryCount++;
                            anqpList = anqpList->next;
                            continue;
                        }
                        wifi_passpoint_dbg_print( "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,g_interworking_data[apIns].passpoint.connCapabilityLength);
                        memset(anqpList->value->data,0,g_interworking_data[apIns].passpoint.connCapabilityLength);
                        memcpy(anqpList->value->data,&g_interworking_data[apIns].passpoint.connCapabilityInfo,g_interworking_data[apIns].passpoint.connCapabilityLength);
                        anqpList->value->len = g_interworking_data[apIns].passpoint.connCapabilityLength;
                        respLength += anqpList->value->len;
                        wifi_passpoint_dbg_print( "%s:%d: Copied ConnectionCapabilityListANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                    }
                    break;
                //NAIHomeRealmANQPElement
                case wifi_anqp_element_hs_subtype_nai_home_realm_query:
                    wifi_passpoint_dbg_print( "%s:%d: Received NAIHomeRealmANQPElement Request\n", __func__, __LINE__);
                    if(g_interworking_data[apIns].passpoint.realmInfoLength){
                        anqpList->value->data = malloc(g_interworking_data[apIns].passpoint.realmInfoLength);//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                            wifi_passpoint_dbg_print( "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                            if(mallocRetryCount > 5){
                                break;
                            }
                            mallocRetryCount++;
                            anqpList = anqpList->next;
                            continue;
                        }
                        wifi_passpoint_dbg_print( "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,g_interworking_data[apIns].passpoint.realmInfoLength);
                        memset(anqpList->value->data,0,g_interworking_data[apIns].passpoint.realmInfoLength);
                        memcpy(anqpList->value->data,&g_interworking_data[apIns].passpoint.realmInfo,g_interworking_data[apIns].passpoint.realmInfoLength);
                        anqpList->value->len = g_interworking_data[apIns].passpoint.realmInfoLength;
                        respLength += anqpList->value->len;
                        wifi_passpoint_dbg_print( "%s:%d: Copied NAIHomeRealmANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                    }
                    break;
                //WANMetricsANQPElement
                case wifi_anqp_element_hs_subtype_wan_metrics:
                    wifi_passpoint_dbg_print( "%s:%d: Received WANMetricsANQPElement Request\n", __func__, __LINE__);
                    anqpList->value->data = malloc(sizeof(wifi_HS2_WANMetrics_t));//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                        wifi_passpoint_dbg_print( "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                        if(mallocRetryCount > 5){
                            break;
                        }
                        mallocRetryCount++;
                        anqpList = anqpList->next;
                        continue;
                    }
                    wifi_passpoint_dbg_print( "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,sizeof(wifi_HS2_WANMetrics_t));
                    memset(anqpList->value->data,0,sizeof(wifi_HS2_WANMetrics_t));
                    memcpy(anqpList->value->data,&g_interworking_data[apIns].passpoint.wanMetricsInfo,sizeof(wifi_HS2_WANMetrics_t));
                    anqpList->value->len = sizeof(wifi_HS2_WANMetrics_t);
                    respLength += anqpList->value->len;
                    wifi_passpoint_dbg_print( "%s:%d: Copied WANMetricsANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                    break;
               default:
                    wifi_passpoint_dbg_print( "%s:%d: Received Unsupported HS2ANQPElement Request: %d\n", __func__, __LINE__,anqpList->value->u.anqp_hs_id);
                    break;
            }
        }else{
            wifi_passpoint_dbg_print( "%s:%d: Invalid Request Type\n", __func__, __LINE__);
        }
        anqpList = anqpList->next;
    }
#if defined (FEATURE_SUPPORT_PASSPOINT)
    if(respLength == 0){
           wifi_passpoint_dbg_print( "%s:%d: Requested ANQP parameter is NULL\n", __func__, __LINE__);
    }
    if(RETURN_OK != (wifi_anqpSendResponse(anqpReq->apIndex, anqpReq->sta, anqpReq->token,  anqpReq->head))){
        //We have failed to send a gas response increase the stats
        gasStats[GAS_CFG_TYPE_SUPPORTED - 1].FailedResponses++;

        if(prevRealmCnt != g_interworking_data[apIns].anqp.realmRespCount){
            g_interworking_data[apIns].anqp.realmRespCount = prevRealmCnt;
            g_interworking_data[apIns].anqp.realmFailedCount++;
        }
        if(prevDomainCnt != g_interworking_data[apIns].anqp.domainRespCount){
            g_interworking_data[apIns].anqp.domainRespCount = prevDomainCnt;
            g_interworking_data[apIns].anqp.domainFailedCount++;
        }
        if(prev3gppCnt != g_interworking_data[apIns].anqp.gppRespCount){
            g_interworking_data[apIns].anqp.gppRespCount = prev3gppCnt;
            g_interworking_data[apIns].anqp.gppFailedCount++;
        }
        wifi_passpoint_dbg_print( "%s:%d: Failed to send ANQP Response. Clear Request and Continue\n", __func__, __LINE__);
    }else{
        //We have sent a gas response increase the stats
        gasStats[GAS_CFG_TYPE_SUPPORTED - 1].Responses++;
        wifi_passpoint_dbg_print( "%s:%d: Successfully sent ANQP Response.\n", __func__, __LINE__);
    }
#endif 
    if(anqpReq){
        free(anqpReq);
        anqpReq = NULL;
    }
}

void anqpRequest_callback(UINT apIndex, mac_address_t sta, unsigned char token,  wifi_anqp_node_t *head)
{
    cosa_wifi_anqp_context_t *anqpReq = malloc(sizeof(cosa_wifi_anqp_context_t));
    memset(anqpReq,0,sizeof(cosa_wifi_anqp_context_t));
    anqpReq->apIndex = apIndex;
    memcpy(anqpReq->sta, sta, sizeof(mac_address_t));
    anqpReq->head = head;
    anqpReq->token = token;
    wifi_passpoint_dbg_print( "%s:%d: Received ANQP Request. Pushing to Data Plane Queue.\n", __func__, __LINE__);
    data_plane_queue_push(data_plane_queue_create_event(anqpReq,wifi_data_plane_event_type_anqp, true));
}

int init_passpoint (void)
{
#if defined (FEATURE_SUPPORT_PASSPOINT)
    wifi_passpoint_dbg_print( "%s:%d: Initializing Passpoint\n", __func__, __LINE__);

    if(RETURN_OK != wifi_anqp_request_callback_register((wifi_anqp_request_callback_t)anqpRequest_callback)) {
        wifi_passpoint_dbg_print( "%s:%d: Failed to Initialize ANQP Callback\n", __func__, __LINE__);
        return RETURN_ERR;
    }
#endif
    return RETURN_OK;
}

ANSC_STATUS CosaDmlWiFi_initPasspoint(void)
{
    if ((init_passpoint() < 0)) {
        wifi_passpoint_dbg_print( "%s %d: init_passpoint Failed\n", __func__, __LINE__);
        return ANSC_STATUS_FAILURE;
    }
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS CosaDmlWiFi_SaveGasCfg(char *buffer, int len)
{
    DIR     *passPointDir = NULL;
   
    passPointDir = opendir(WIFI_PASSPOINT_DIR);
    if(passPointDir){
        closedir(passPointDir);
    }else if(ENOENT == errno){
        if(0 != mkdir(WIFI_PASSPOINT_DIR, 0777)){
            wifi_passpoint_dbg_print("Failed to Create Passpoint Configuration directory. Setting Default\n");
            return ANSC_STATUS_FAILURE;;
        }
    }else{
        wifi_passpoint_dbg_print("Error opening Passpoint Configuration directory. Setting Default\n");
        return ANSC_STATUS_FAILURE;;
    } 
 
    FILE *fPasspointGasCfg = fopen(WIFI_PASSPOINT_GAS_CFG_FILE, "w");
    if(0 == fwrite(buffer, len,1, fPasspointGasCfg)){
        fclose(fPasspointGasCfg);
        return ANSC_STATUS_FAILURE;
    }else{
        fclose(fPasspointGasCfg);
        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS CosaDmlWiFi_SetGasConfig(PANSC_HANDLE phContext, char *JSON_STR)
{
#if defined (FEATURE_SUPPORT_PASSPOINT)
    PCOSA_DATAMODEL_WIFI    pMyObject               = ( PCOSA_DATAMODEL_WIFI )phContext;
    wifi_GASConfiguration_t gasConfig_struct = {0, 0, 0, 0, 0, 0};
    Err execRetVal;
    errno_t rc = -1;

    if(!pMyObject){
        wifi_passpoint_dbg_print("Wifi Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    PCOSA_DML_WIFI_GASCFG  pGASconf = NULL;

    cJSON *gasList = NULL;
    cJSON *gasEntry = NULL;

    if(!JSON_STR){
        wifi_passpoint_dbg_print("Failed to read JSON\n");
        return ANSC_STATUS_FAILURE;
    }

    cJSON *passPointCfg = cJSON_Parse(JSON_STR);

    if (NULL == passPointCfg) {
        wifi_passpoint_dbg_print("Failed to parse JSON\n");
        return ANSC_STATUS_FAILURE;
    }

    gasList = cJSON_GetObjectItem(passPointCfg,"GASConfig");
    if(NULL == gasList){
        wifi_passpoint_dbg_print("gasList is NULL\n");
        cJSON_Delete(passPointCfg);
        return ANSC_STATUS_FAILURE;
    } 

    memset((char *)&gasConfig_struct,0,sizeof(gasConfig_struct));

   cJSON_ArrayForEach(gasEntry, gasList) {
 
        if(RETURN_OK!= validate_gas_config(gasEntry, &gasConfig_struct, &execRetVal)) {
            wifi_passpoint_dbg_print("Invalid GAS Configuration. %s\n",execRetVal.ErrorMsg);
            cJSON_Delete(passPointCfg);
            return ANSC_STATUS_FAILURE;
        }
    }

    pGASconf = &pMyObject->GASCfg[gasConfig_struct.AdvertisementID];

    if(RETURN_OK == wifi_setGASConfiguration(gasConfig_struct.AdvertisementID, &gasConfig_struct)){
        pGASconf->AdvertisementID = gasConfig_struct.AdvertisementID; 
        pGASconf->PauseForServerResponse = gasConfig_struct.PauseForServerResponse;
        pGASconf->ResponseTimeout = gasConfig_struct.ResponseTimeout;
        pGASconf->ComeBackDelay = gasConfig_struct.ComeBackDelay;
        pGASconf->ResponseBufferingTime = gasConfig_struct.ResponseBufferingTime;
        pGASconf->QueryResponseLengthLimit = gasConfig_struct.QueryResponseLengthLimit;
        cJSON_Delete(passPointCfg);

#if defined(ENABLE_FEATURE_MESHWIFI)        
        //Update OVSDB
        if(RETURN_OK != update_ovsdb_gas_config(gasConfig_struct.AdvertisementID, &gasConfig_struct))
        {
            wifi_passpoint_dbg_print("Failed to update OVSDB with GAS Config. Adv-ID:%d\n",gasConfig_struct.AdvertisementID);
        }
#else
        if (ANSC_STATUS_SUCCESS != CosaDmlWiFi_SaveGasCfg (JSON_STR, strlen(JSON_STR))) {
            wifi_passpoint_dbg_print("Failed to update OVSDB with GAS Config. Adv-ID:%d\n",gasConfig_struct.AdvertisementID);
        }
#endif
        if (pMyObject->GASConfiguration) {
            free(pMyObject->GASConfiguration);
            pMyObject->GASConfiguration = NULL;
        }
        pMyObject->GASConfiguration = malloc(strlen(JSON_STR) + 1);
       
        if(pMyObject->GASConfiguration) {
            rc = strcpy_s(pMyObject->GASConfiguration, (strlen(JSON_STR) + 1), JSON_STR);
            ERR_CHK(rc);
        } else {
            wifi_passpoint_dbg_print("Failed to allocate memory for GAS Config. Adv-ID:%d\n",gasConfig_struct.AdvertisementID);
        }
        
        return ANSC_STATUS_SUCCESS;
      }
      wifi_passpoint_dbg_print("Failed to update HAL with GAS Config. Adv-ID:%d\n",gasConfig_struct.AdvertisementID);
      return ANSC_STATUS_FAILURE;
#else
    UNREFERENCED_PARAMETER(JSON_STR);
    UNREFERENCED_PARAMETER(phContext);
#endif 
    return ANSC_STATUS_FAILURE;
}

ANSC_STATUS CosaDmlWiFi_DefaultGasConfig(PANSC_HANDLE phContext)
{
    if (ANSC_STATUS_SUCCESS != CosaDmlWiFi_SetGasConfig(phContext, WIFI_PASSPOINT_DEFAULT_GAS_CFG))
    {
        wifi_passpoint_dbg_print("Failed to update HAL with default GAS Config.\n");
        return ANSC_STATUS_FAILURE;
    }
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS CosaDmlWiFi_InitGasConfig(PANSC_HANDLE phContext)
{
#if defined (FEATURE_SUPPORT_PASSPOINT)  
    PCOSA_DATAMODEL_WIFI    pMyObject               = ( PCOSA_DATAMODEL_WIFI )phContext;
    char *JSON_STR = NULL;

    if(!pMyObject){
        wifi_passpoint_dbg_print("Wifi Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    g_passpoint.wifi_dml = pMyObject; 

    pMyObject->GASConfiguration = NULL;
   
#if defined(ENABLE_FEATURE_MESHWIFI)
    wifi_GASConfiguration_t gasConfig_struct = {0};
    cJSON *gasCfg = NULL;
    cJSON *mainEntry = NULL;

    if(RETURN_OK != get_ovsdb_gas_config(0,&gasConfig_struct)){
        return CosaDmlWiFi_DefaultGasConfig(phContext);
    }

    gasCfg = cJSON_CreateObject();
    if (NULL == gasCfg) {
        wifi_passpoint_dbg_print("Failed to create GAS JSON Object\n");
        return CosaDmlWiFi_DefaultGasConfig(phContext);
    }

    mainEntry = cJSON_AddObjectToObject(gasCfg,"GasConfig");    
    cJSON_AddNumberToObject(mainEntry,"AdvertisementId",gasConfig_struct.AdvertisementID);
    cJSON_AddBoolToObject(mainEntry,"PauseForServerResp",gasConfig_struct.PauseForServerResponse);
    cJSON_AddNumberToObject(mainEntry,"RespTimeout",gasConfig_struct.ResponseTimeout);
    cJSON_AddNumberToObject(mainEntry,"ComebackDelay",gasConfig_struct.ComeBackDelay);
    cJSON_AddNumberToObject(mainEntry,"RespBufferTime",gasConfig_struct.ResponseBufferingTime);
    cJSON_AddNumberToObject(mainEntry,"QueryRespLengthLimit",gasConfig_struct.QueryResponseLengthLimit);
    
    JSON_STR = malloc(512);
    memset(JSON_STR, 0, 512);

    cJSON_PrintPreallocated(gasCfg, JSON_STR,512,false);
    cJSON_Delete(gasCfg);
#else
    long confSize = readFileToBuffer(WIFI_PASSPOINT_GAS_CFG_FILE,&JSON_STR);

    if(!confSize || !JSON_STR) { 
        if(JSON_STR){
            free(JSON_STR);
            JSON_STR = NULL;
        }
        wifi_passpoint_dbg_print("Failed to Initialize GAS Configuration from memory. Setting Default\n");
        return CosaDmlWiFi_DefaultGasConfig(phContext);
    }
#endif

    if((ANSC_STATUS_SUCCESS != CosaDmlWiFi_SetGasConfig(phContext,JSON_STR))){
        wifi_passpoint_dbg_print("Failed to Initialize GAS Configuration from memory. Setting Default\n");
        return CosaDmlWiFi_DefaultGasConfig(phContext);
    }

    if(JSON_STR){
        free(JSON_STR);
        JSON_STR = NULL;
    }
#else
    UNREFERENCED_PARAMETER(phContext);
#endif
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS CosaDmlWiFi_GetGasStats(PANSC_HANDLE phContext)
{
    PCOSA_DML_WIFI_GASSTATS  pGASStats   = (PCOSA_DML_WIFI_GASSTATS)phContext;
    if(!pGASStats){
        wifi_passpoint_dbg_print("Wifi GAS Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }

    memset(pGASStats,0,sizeof(COSA_DML_WIFI_GASSTATS));
    memcpy(pGASStats,&gasStats[GAS_CFG_TYPE_SUPPORTED - 1],sizeof(gasStats));
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS CosaDmlWiFi_SetANQPConfig(PCOSA_DML_WIFI_AP_CFG pCfg, char *JSON_STR)
{
    Err execRetVal;
    void* anqpDataInterworking = NULL;
    if(!pCfg){
        wifi_passpoint_dbg_print("AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    int apIns = pCfg->InstanceNumber -1;
    if((apIns < 0) || (apIns > 15)){
        wifi_passpoint_dbg_print( "%s:%d: Invalid AP Index. Setting to 1\n", __func__, __LINE__);
        apIns = 0;
    }
    wifi_interworking_t * anqpData;
	anqpData = (wifi_interworking_t *) malloc(sizeof(wifi_interworking_t));
	
	if(anqpData == NULL){
        wifi_passpoint_dbg_print("Failed to allocate memory\n");
        return ANSC_STATUS_FAILURE;
    }
	
    cJSON *mainEntry = NULL;
    if(!JSON_STR){
        wifi_passpoint_dbg_print("JSON String is NULL\n");
        if (anqpData)
            free(anqpData);
        return ANSC_STATUS_FAILURE;
    }
    cJSON *passPointCfg = cJSON_Parse(JSON_STR);
    if (NULL == passPointCfg) {
        wifi_passpoint_dbg_print("Failed to parse JSON\n");
        if (anqpData)
            free(anqpData);
        return ANSC_STATUS_FAILURE;
    }
    mainEntry = cJSON_GetObjectItem(passPointCfg,"ANQP");
    if(NULL == mainEntry){
        wifi_passpoint_dbg_print("ANQP entry is NULL\n");
        cJSON_Delete(passPointCfg);
        if (anqpData)
            free(anqpData);
        return ANSC_STATUS_FAILURE;
    }
   
    memset((char *)anqpData,0,sizeof(wifi_interworking_t));
    anqpDataInterworking = &(anqpData->interworking);
    wifi_getApInterworkingElement(apIns,anqpDataInterworking);   
 
    if (validate_anqp(mainEntry, anqpData, &execRetVal) != 0) {
        wifi_passpoint_dbg_print("%s:%d: Validation failed. Error: %s\n", __func__, __LINE__,execRetVal.ErrorMsg);
        cJSON_Delete(passPointCfg);
        if (anqpData)
            free(anqpData);
      
        return ANSC_STATUS_FAILURE;
    }
    if (memcmp(&g_interworking_data[apIns].roamingConsortium,
            &(anqpData->roamingConsortium),
        sizeof(wifi_roamingConsortiumElement_t)) != 0) {
#if defined (FEATURE_SUPPORT_PASSPOINT)
        if (RETURN_OK != wifi_pushApRoamingConsortiumElement(apIns, 
                     &(anqpData->roamingConsortium))) {
            wifi_passpoint_dbg_print( "%s: Failed to push Roaming Consotrium to hal for wlan %d\n",
                            __FUNCTION__, apIns);
            cJSON_Delete(passPointCfg);    
            if(anqpData)
                free(anqpData);
            
          return ANSC_STATUS_FAILURE;
        }
        wifi_passpoint_dbg_print( "%s: Applied Roaming Consortium configuration successfully for wlan %d\n",
                   __FUNCTION__, apIns);
#endif
        memcpy(&g_interworking_data[apIns].roamingConsortium,&(anqpData->roamingConsortium),
               sizeof(wifi_roamingConsortiumElement_t));
        //Update TR-181
        pCfg->IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumCount = anqpData->roamingConsortium.wifiRoamingConsortiumCount;
        memcpy(&pCfg->IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumOui,&(anqpData->roamingConsortium.wifiRoamingConsortiumOui),
               sizeof(pCfg->IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumOui));
        memcpy(&pCfg->IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumLen,&(anqpData->roamingConsortium.wifiRoamingConsortiumLen),
               sizeof(pCfg->IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumLen));
    }
    memcpy(&g_interworking_data[apIns].anqp, &(anqpData->anqp), sizeof(wifi_anqp_settings_t));
    wifi_passpoint_dbg_print("%s:%d: Validation Success. Updating ANQP Config\n", __func__, __LINE__);
    cJSON_Delete(passPointCfg);
    free(anqpData);
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS CosaDmlWiFi_SaveANQPCfg(PCOSA_DML_WIFI_AP_CFG pCfg)
{
    char cfgFile[64];
    DIR     *passPointDir = NULL;
    int apIns = 0;
    char *buffer = NULL;
    int len = 0;
    errno_t rc = -1;

    if(!pCfg){
        wifi_passpoint_dbg_print("AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }

    buffer = pCfg->IEEE80211uCfg.PasspointCfg.ANQPConfigParameters;
    if (!buffer) {
        wifi_passpoint_dbg_print("ANQP Parameters is NULL.\n");
        return ANSC_STATUS_FAILURE;
    }

    len = AnscSizeOfString(pCfg->IEEE80211uCfg.PasspointCfg.ANQPConfigParameters);
    if (!len) {
        wifi_passpoint_dbg_print("ANQP Parameters Length is 0.\n");
        return ANSC_STATUS_FAILURE;
    }
 
    passPointDir = opendir(WIFI_PASSPOINT_DIR);
    if(passPointDir){
        closedir(passPointDir);
    }else if(ENOENT == errno){
        if(0 != mkdir(WIFI_PASSPOINT_DIR, 0777)){
            wifi_passpoint_dbg_print("Failed to Create Passpoint Configuration directory.\n");
            return ANSC_STATUS_FAILURE;
        }
    }else{
        wifi_passpoint_dbg_print("Error opening Passpoint Configuration directory.\n");
        return ANSC_STATUS_FAILURE;
    } 
 
    apIns = pCfg->InstanceNumber;
    rc = sprintf_s(cfgFile, sizeof(cfgFile), "%s.%d",WIFI_PASSPOINT_ANQP_CFG_FILE,apIns);
    if(rc < EOK)
    {
        ERR_CHK(rc);
    }
    FILE *fPasspointAnqpCfg = fopen(cfgFile, "w");
    if(0 == fwrite(buffer, len,1, fPasspointAnqpCfg)){
        fclose(fPasspointAnqpCfg);
        return ANSC_STATUS_FAILURE;
    }else{
        fclose(fPasspointAnqpCfg);
        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS CosaDmlWiFi_InitANQPConfig(PCOSA_DML_WIFI_AP_CFG pCfg)
{
    char cfgFile[64];
    char *JSON_STR = NULL;
    int apIns = 0;
    long confSize = 0;
    errno_t rc = -1;

    if(!pCfg){
        wifi_passpoint_dbg_print("AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    pCfg->IEEE80211uCfg.PasspointCfg.ANQPConfigParameters = NULL;
    apIns = pCfg->InstanceNumber;
    if((apIns < 1) || (apIns > 16)){
        wifi_passpoint_dbg_print( "%s:%d: Invalid AP Index. Return\n", __func__, __LINE__);
        return ANSC_STATUS_FAILURE;
    }

    rc = sprintf_s(cfgFile, sizeof(cfgFile), "%s.%d",WIFI_PASSPOINT_ANQP_CFG_FILE,apIns);
    if(rc < EOK)
    {
        ERR_CHK(rc);
    }
   
    confSize = readFileToBuffer(cfgFile,&JSON_STR);
    
    if(!confSize || !JSON_STR || (ANSC_STATUS_SUCCESS != CosaDmlWiFi_SetANQPConfig(pCfg,JSON_STR))){
        if(JSON_STR){
            free(JSON_STR);
            JSON_STR = NULL;
        }
        wifi_passpoint_dbg_print("Failed to Initialize ANQP Configuration from memory for AP: %d.\n",apIns);
        pCfg->IEEE80211uCfg.PasspointCfg.ANQPConfigParameters = NULL;
    } else {
        wifi_passpoint_dbg_print("Initialized ANQP Configuration from memory for AP: %d.\n",apIns);
        pCfg->IEEE80211uCfg.PasspointCfg.ANQPConfigParameters = JSON_STR;
    }
    return ANSC_STATUS_SUCCESS;
}

void CosaDmlWiFi_UpdateANQPVenueInfo(PCOSA_DML_WIFI_AP_CFG pCfg)
{
    int apIns = pCfg->InstanceNumber - 1;

    if((apIns < 0) || (apIns > 15)){
        wifi_passpoint_dbg_print( "%s:%d: Invalid AP Index. Return\n", __func__, __LINE__);
        return;
    }

    //Copy Venue Group and Type from Interworking Structure
    g_interworking_data[apIns].anqp.venueInfo.venueGroup = pCfg->IEEE80211uCfg.IntwrkCfg.iVenueGroup;
    g_interworking_data[apIns].anqp.venueInfo.venueType = pCfg->IEEE80211uCfg.IntwrkCfg.iVenueType;
    wifi_passpoint_dbg_print( "%s:%d: Updated VenueNameANQPElement from Interworking\n", __func__, __LINE__);

}

ANSC_STATUS CosaDmlWiFi_SetHS2Config(PCOSA_DML_WIFI_AP_CFG pCfg, char *JSON_STR)
{
#if defined (FEATURE_SUPPORT_PASSPOINT)
    Err execRetVal;
    BOOL apEnable = FALSE;
    char *strValue = NULL;
    int   retPsmGet  = CCSP_SUCCESS;
    void* pPasspointCfgInterworking = NULL;

    if(!pCfg){
        wifi_passpoint_dbg_print("AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    int apIns = pCfg->InstanceNumber -1;
    if((apIns < 0) || (apIns > 15)){
        wifi_passpoint_dbg_print( "%s:%d: Invalid AP Index. Setting to 1\n", __func__, __LINE__);
        apIns = 0;
    }
    
    wifi_interworking_t * passpointCfg;
    passpointCfg = (wifi_interworking_t *) malloc(sizeof(wifi_interworking_t));
    if(passpointCfg == NULL){
        wifi_passpoint_dbg_print("Failed to allocate memory\n");
        return ANSC_STATUS_FAILURE;
    }

    cJSON *mainEntry = NULL;
    
    if(!JSON_STR){
        wifi_passpoint_dbg_print("JSON String is NULL\n");
        if (passpointCfg)
           free(passpointCfg);
        return ANSC_STATUS_FAILURE;
    }
    
    cJSON *passPointObj = cJSON_Parse(JSON_STR);
    
    if (NULL == passPointObj) {
        wifi_passpoint_dbg_print("Failed to parse JSON\n");
        if (passpointCfg)
           free(passpointCfg);
        return ANSC_STATUS_FAILURE;
    }
    
    mainEntry = cJSON_GetObjectItem(passPointObj,"Passpoint");
    if(NULL == mainEntry){
        wifi_passpoint_dbg_print("Passpoint entry is NULL\n");
        cJSON_Delete(passPointObj);
        if (passpointCfg)
           free(passpointCfg);
        return ANSC_STATUS_FAILURE;
    }
  
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
 
    memset((char *)passpointCfg,0,sizeof(wifi_interworking_t));
    pPasspointCfgInterworking = &(passpointCfg->interworking);
    wifi_getApInterworkingElement(apIns,pPasspointCfgInterworking);
    if (validate_passpoint(mainEntry, passpointCfg, &execRetVal) != 0) {   
       wifi_passpoint_dbg_print("%s:%d: Validation failed. Error: %s\n", __func__, __LINE__,execRetVal.ErrorMsg);
       cJSON_Delete(passPointObj);
       if (passpointCfg)
           free(passpointCfg);
       return ANSC_STATUS_FAILURE;
    }
   
    wifi_passpoint_dbg_print("%s:%d: Validation Success. Updating Passpoint Config\n", __func__, __LINE__);

    wifi_getApEnable(apIns, &apEnable);
    wifi_passpoint_dbg_print( "%s:%d: Enable flag of AP Index: %d is %d \n", __func__, __LINE__,apIns, apEnable);
    if(apEnable) {
        if(RETURN_OK == enablePassPointSettings(apIns, passpointCfg->passpoint.enable,
                                                       passpointCfg->passpoint.gafDisable,
                                                       passpointCfg->passpoint.p2pDisable,
                                                       passpointCfg->passpoint.l2tif)) {
             wifi_passpoint_dbg_print("%s:%d: Successfully set Passpoint Config\n", __func__, __LINE__);
             pCfg->IEEE80211uCfg.PasspointCfg.Status = passpointCfg->passpoint.enable;
             pCfg->IEEE80211uCfg.PasspointCfg.gafDisable = passpointCfg->passpoint.gafDisable;
             pCfg->IEEE80211uCfg.PasspointCfg.p2pDisable = passpointCfg->passpoint.p2pDisable;
             pCfg->IEEE80211uCfg.PasspointCfg.l2tif = passpointCfg->passpoint.l2tif;
         }else{
             wifi_passpoint_dbg_print( "%s:%d: Error Setting Passpoint Enable Status on AP: %d\n", __func__, __LINE__,apIns);
             cJSON_Delete(passPointObj);
             if (passpointCfg)
                 free(passpointCfg);
             return ANSC_STATUS_FAILURE;
        }
    } else {
        wifi_passpoint_dbg_print( "%s:%d: VAP is disabled. Not Initializing Passpoint Enable Status on AP: %d\n", __func__, __LINE__,apIns);
    }
    memcpy((char *)&g_interworking_data[apIns].passpoint,&(passpointCfg->passpoint),sizeof(wifi_passpoint_settings_t));

    cJSON_Delete(passPointObj);
    if (passpointCfg)
        free(passpointCfg);
#else
    UNREFERENCED_PARAMETER(pCfg);
    UNREFERENCED_PARAMETER(JSON_STR);
#endif

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS CosaDmlWiFi_SetHS2Status(PCOSA_DML_WIFI_AP_CFG pCfg, BOOL bValue, BOOL setToPSM)
{

    if(!pCfg){
        wifi_passpoint_dbg_print("AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }

    int apIns = pCfg->InstanceNumber;
    if((apIns < 1) || (apIns > 16)){
        wifi_passpoint_dbg_print( "%s:%d: Invalid AP Index. Return\n", __func__, __LINE__);
        return ANSC_STATUS_FAILURE;
    }
#if defined (FEATURE_SUPPORT_PASSPOINT)    
    
    if(RETURN_OK == enablePassPointSettings (apIns-1, bValue, g_interworking_data[apIns-1].passpoint.gafDisable, g_interworking_data[apIns-1].passpoint.p2pDisable, g_interworking_data[apIns-1].passpoint.l2tif)){
        pCfg->IEEE80211uCfg.PasspointCfg.Status = g_interworking_data[apIns-1].passpoint.enable = bValue;
    }else{
      wifi_passpoint_dbg_print( "%s:%d: Error Setting Passpoint Enable Status on AP: %d\n", __func__, __LINE__,apIns);
      return ANSC_STATUS_FAILURE;
    }

    if(true == bValue){
        wifi_passpoint_dbg_print( "%s:%d: Passpoint is Enabled on AP: %d\n", __func__, __LINE__,apIns);
    }else{
        wifi_passpoint_dbg_print( "%s:%d: Passpoint is Disabled on AP: %d\n", __func__, __LINE__,apIns);
    }
#else
    UNREFERENCED_PARAMETER(bValue);
#endif
    UNREFERENCED_PARAMETER(setToPSM);
    return ANSC_STATUS_SUCCESS;
}
    
        
ANSC_STATUS CosaDmlWiFi_SaveHS2Cfg(PCOSA_DML_WIFI_AP_CFG pCfg)
{
    char cfgFile[64];
    DIR     *passPointDir = NULL;
    char *buffer = NULL;
    int len = 0;
    errno_t rc = -1;

    if(!pCfg){
        wifi_passpoint_dbg_print("AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    
    buffer = pCfg->IEEE80211uCfg.PasspointCfg.HS2Parameters;
    if (!buffer) {
        wifi_passpoint_dbg_print("Passpoint Parameters is NULL.\n");
        return ANSC_STATUS_FAILURE;
    }

    len = AnscSizeOfString(pCfg->IEEE80211uCfg.PasspointCfg.HS2Parameters);
    if (!len) {
        wifi_passpoint_dbg_print("Passpoint Parameters Length is 0.\n");
        return ANSC_STATUS_FAILURE;
    }
 
    
    passPointDir = opendir(WIFI_PASSPOINT_DIR);
    if(passPointDir){
        closedir(passPointDir);
    }else if(ENOENT == errno){
        if(0 != mkdir(WIFI_PASSPOINT_DIR, 0777)){
            wifi_passpoint_dbg_print("Failed to Create Passpoint Configuration directory.\n");
            return ANSC_STATUS_FAILURE;
        }
    }else{
        wifi_passpoint_dbg_print("Error opening Passpoint Configuration directory.\n");
        return ANSC_STATUS_FAILURE;
    }
    
    rc = sprintf_s(cfgFile, sizeof(cfgFile), "%s.%d",WIFI_PASSPOINT_HS2_CFG_FILE, pCfg->InstanceNumber);
    if(rc < EOK)
    {
        ERR_CHK(rc);
    }
    FILE *fPasspointCfg = fopen(cfgFile, "w");
    if(0 == fwrite(buffer, len,1, fPasspointCfg)){
        fclose(fPasspointCfg);
        return ANSC_STATUS_FAILURE;
    }else{
        fclose(fPasspointCfg);
        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS CosaDmlWiFi_InitHS2Config(PCOSA_DML_WIFI_AP_CFG pCfg)
{
    char cfgFile[64];
    char *JSON_STR = NULL;
    int apIns = 0;
    long confSize = 0;
    errno_t rc = -1;
    
    if(!pCfg){
        wifi_passpoint_dbg_print("AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    pCfg->IEEE80211uCfg.PasspointCfg.HS2Parameters = NULL;
    apIns = pCfg->InstanceNumber;
    if((apIns < 1) || (apIns > 16)){
        wifi_passpoint_dbg_print( "%s:%d: Invalid AP Index. Return\n", __func__, __LINE__);
        return ANSC_STATUS_FAILURE;
    }
   
    //Set Default DGAF value to true
    pCfg->IEEE80211uCfg.PasspointCfg.gafDisable = true;
 
    rc = sprintf_s(cfgFile, sizeof(cfgFile), "%s.%d",WIFI_PASSPOINT_HS2_CFG_FILE,apIns);
    if(rc < EOK)
    {
        ERR_CHK(rc);
    }
    
    confSize = readFileToBuffer(cfgFile,&JSON_STR);
    if(!confSize || !JSON_STR || (ANSC_STATUS_SUCCESS != CosaDmlWiFi_SetHS2Config(pCfg,JSON_STR))){
        if(JSON_STR){
            free(JSON_STR);
            JSON_STR = NULL;
        }
        wifi_passpoint_dbg_print("Failed to Initialize HS2.0 Configuration from memory for AP: %d. Setting Default\n",apIns);
        pCfg->IEEE80211uCfg.PasspointCfg.HS2Parameters = NULL;
    } else {
        wifi_passpoint_dbg_print("Initialized HS2.0 Configuration from memory for AP: %d.\n",apIns);
        pCfg->IEEE80211uCfg.PasspointCfg.HS2Parameters = JSON_STR;

    }

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS CosaDmlWiFi_GetWANMetrics(PCOSA_DML_WIFI_AP_CFG pCfg)
{
    cJSON *mainEntry = NULL;
    int apIns; 

    if(!pCfg){
        wifi_passpoint_dbg_print("AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }

    apIns = pCfg->InstanceNumber -1;
    if((apIns < 0) || (apIns > 15)){
        wifi_passpoint_dbg_print( "%s:%d: Invalid AP Index. Setting to 1\n", __func__, __LINE__);
        apIns = 0;
    }

    memset(&pCfg->IEEE80211uCfg.PasspointCfg.WANMetrics, 0, sizeof(pCfg->IEEE80211uCfg.PasspointCfg.WANMetrics));

    cJSON *passPointCfg = cJSON_CreateObject();
    if (NULL == passPointCfg) {
        wifi_passpoint_dbg_print("Failed to create JSON\n");
        return ANSC_STATUS_FAILURE;
    }

    mainEntry = cJSON_AddObjectToObject(passPointCfg,"WANMetrics");

    cJSON_AddNumberToObject(mainEntry,"WANInfo",g_interworking_data[apIns].passpoint.wanMetricsInfo.wanInfo); 
    cJSON_AddNumberToObject(mainEntry,"DownlinkSpeed",g_interworking_data[apIns].passpoint.wanMetricsInfo.downLinkSpeed); 
    cJSON_AddNumberToObject(mainEntry,"UplinkSpeed",g_interworking_data[apIns].passpoint.wanMetricsInfo.upLinkSpeed);
    cJSON_AddNumberToObject(mainEntry,"DownlinkLoad",g_interworking_data[apIns].passpoint.wanMetricsInfo.downLinkLoad); 
    cJSON_AddNumberToObject(mainEntry,"UplinkLoad",g_interworking_data[apIns].passpoint.wanMetricsInfo.upLinkLoad); 
    cJSON_AddNumberToObject(mainEntry,"LMD",g_interworking_data[apIns].passpoint.wanMetricsInfo.lmd); 

    cJSON_PrintPreallocated(passPointCfg, (char *)&pCfg->IEEE80211uCfg.PasspointCfg.WANMetrics, sizeof(pCfg->IEEE80211uCfg.PasspointCfg.WANMetrics),false);
    cJSON_Delete(passPointCfg);
    return ANSC_STATUS_SUCCESS;
}

void CosaDmlWiFi_GetHS2Stats(PCOSA_DML_WIFI_AP_CFG pCfg)
{
    cJSON *mainEntry = NULL;
    cJSON *statsParam = NULL;
    cJSON *statsList = NULL;
    cJSON *statsEntry = NULL;
    int apIns; 

    if(!pCfg){
        wifi_passpoint_dbg_print("AP Context is NULL\n");
        return;
    }

    apIns = pCfg->InstanceNumber -1;
    if((apIns < 0) || (apIns > 15)){
        wifi_passpoint_dbg_print( "%s:%d: Invalid AP Index.\n", __func__, __LINE__);
        return;
    }

    memset(&pCfg->IEEE80211uCfg.PasspointCfg.Stats, 0, sizeof(pCfg->IEEE80211uCfg.PasspointCfg.Stats));

    cJSON *passPointStats = cJSON_Parse((char*)g_interworking_data[apIns].anqp.passpointStats);
    if (NULL == passPointStats) {
        wifi_passpoint_dbg_print("Failed to parse JSON\n");
        return;
    }

    mainEntry = cJSON_GetObjectItem(passPointStats,"PassPointStats");
    if(NULL == mainEntry){
        wifi_passpoint_dbg_print("PassPointStats entry is NULL\n");
        cJSON_Delete(passPointStats);
        return;
    }
  
    //Set EAP stats to zero. TBD
    cJSON_AddNumberToObject(mainEntry, "EAPOLStartSuccess", 0);
    cJSON_AddNumberToObject(mainEntry, "EAPOLStartFailed", 0);
    cJSON_AddNumberToObject(mainEntry, "EAPOLStartTimeouts", 0);
    cJSON_AddNumberToObject(mainEntry, "EAPOLStartRetries", 0);
    cJSON_AddNumberToObject(mainEntry, "EAPOLSuccessSent", 0);
    cJSON_AddNumberToObject(mainEntry, "EAPFailedSent", 0);
  
    statsList = cJSON_GetObjectItem(mainEntry, "ANQPResponse");

    cJSON_ArrayForEach(statsEntry, statsList) {
        if(NULL != (statsParam = cJSON_GetObjectItem(statsEntry,"EntryType"))){
            switch((int)statsParam->valuedouble){
                case 1:
                    cJSON_SetIntValue(cJSON_GetObjectItem(statsEntry,"Sent"),g_interworking_data[apIns].anqp.realmRespCount);
                    cJSON_SetIntValue(cJSON_GetObjectItem(statsEntry,"Failed"),g_interworking_data[apIns].anqp.realmFailedCount);
                    break;
                case 2:
                    cJSON_SetIntValue(cJSON_GetObjectItem(statsEntry,"Sent"),g_interworking_data[apIns].anqp.domainRespCount);
                    cJSON_SetIntValue(cJSON_GetObjectItem(statsEntry,"Failed"),g_interworking_data[apIns].anqp.domainFailedCount);
                    break;
                case 3:
                    cJSON_SetIntValue(cJSON_GetObjectItem(statsEntry,"Sent"),g_interworking_data[apIns].anqp.gppRespCount);
                    cJSON_SetIntValue(cJSON_GetObjectItem(statsEntry,"Failed"),g_interworking_data[apIns].anqp.gppFailedCount);
                    break;
            }
        }
    }

    cJSON_PrintPreallocated(passPointStats, (char *)&pCfg->IEEE80211uCfg.PasspointCfg.Stats, sizeof(pCfg->IEEE80211uCfg.PasspointCfg.Stats),false);
    cJSON_Delete(passPointStats);
    return;
}

ANSC_STATUS CosaDmlWiFi_RestoreAPInterworking (int apIndex)
{
#if defined (FEATURE_SUPPORT_PASSPOINT)
    PCOSA_DML_WIFI_AP_CFG pCfg; 
    if((apIndex < 0) || (apIndex> 15)){
        wifi_passpoint_dbg_print( "%s:%d: Invalid AP Index: %d.\n", __func__, __LINE__,apIndex);
        return ANSC_STATUS_FAILURE;
    }

    pCfg = find_ap_wifi_dml(apIndex);
  
    if (pCfg == NULL)
    {
        return ANSC_STATUS_FAILURE;
    }
  
    if(!pCfg->InterworkingEnable){
        wifi_passpoint_dbg_print( "%s:%d: Interworking is disabled on AP: %d. Returning Success\n", __func__, __LINE__, apIndex);
        return ANSC_STATUS_SUCCESS;
    }
  
    if ((CosaDmlWiFi_ApplyInterworkingElement(pCfg)) != ANSC_STATUS_SUCCESS)
    {
        wifi_passpoint_dbg_print( "%s:%d: CosaDmlWiFi_ApplyInterworkingElement Failed.\n", __func__, __LINE__);
    }
  
    if(ANSC_STATUS_SUCCESS != CosaDmlWiFi_ApplyRoamingConsortiumElement(pCfg)){
        wifi_passpoint_dbg_print( "%s:%d: CosaDmlWiFi_ApplyRoamingConsortiumElement Failed.\n", __func__, __LINE__);
    }
    
    if ((g_interworking_data[apIndex].passpoint.enable) && (RETURN_OK != enablePassPointSettings (apIndex, g_interworking_data[apIndex].passpoint.enable, g_interworking_data[apIndex].passpoint.gafDisable, g_interworking_data[apIndex].passpoint.p2pDisable, g_interworking_data[apIndex].passpoint.l2tif))){
      wifi_passpoint_dbg_print( "%s:%d: Error Setting Passpoint Enable Status on AP: %d\n", __func__, __LINE__,apIndex);
      return ANSC_STATUS_FAILURE;
    }
    pCfg->IEEE80211uCfg.PasspointCfg.Status = g_interworking_data[apIndex].passpoint.enable;
    wifi_passpoint_dbg_print( "%s:%d: Set Passpoint Enable Status on AP: %d\n", __func__, __LINE__,apIndex);
#else
    UNREFERENCED_PARAMETER(apIndex);
#endif
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS CosaDmlWiFi_SaveInterworkingWebconfig(PCOSA_DML_WIFI_AP_CFG pCfg, wifi_interworking_t *interworking_data, int apIns) 
{
#if defined (FEATURE_SUPPORT_PASSPOINT)
    if(!interworking_data || !pCfg) {
        wifi_passpoint_dbg_print("NULL Interworking Configuration\n");
        return ANSC_STATUS_FAILURE;
    }

    //Copy ANQP Parameters.
    if(pCfg->IEEE80211uCfg.PasspointCfg.ANQPConfigParameters){
        free(pCfg->IEEE80211uCfg.PasspointCfg.ANQPConfigParameters);
    }
    pCfg->IEEE80211uCfg.PasspointCfg.ANQPConfigParameters = strdup((char *)interworking_data->anqp.anqpParameters);
    if (! pCfg->IEEE80211uCfg.PasspointCfg.ANQPConfigParameters) {
        return ANSC_STATUS_FAILURE;
    }

    if(ANSC_STATUS_FAILURE == CosaDmlWiFi_SaveANQPCfg(pCfg)){
        wifi_passpoint_dbg_print("Failed to Save ANQP Configuration\n");
    }
    
    //Copy Passpoint Parameters.
    if(pCfg->IEEE80211uCfg.PasspointCfg.HS2Parameters){
        free(pCfg->IEEE80211uCfg.PasspointCfg.HS2Parameters);
    }
    pCfg->IEEE80211uCfg.PasspointCfg.HS2Parameters = strdup((char *)interworking_data->passpoint.hs2Parameters);
    if (! pCfg->IEEE80211uCfg.PasspointCfg.HS2Parameters) {
        return ANSC_STATUS_FAILURE;
    }

    if(ANSC_STATUS_FAILURE == CosaDmlWiFi_SaveHS2Cfg(pCfg)){
        wifi_passpoint_dbg_print("Failed to Save  Configuration\n");
    }

    pCfg->IEEE80211uCfg.PasspointCfg.Status = interworking_data->passpoint.enable;
    pCfg->IEEE80211uCfg.PasspointCfg.gafDisable = interworking_data->passpoint.gafDisable;
    pCfg->IEEE80211uCfg.PasspointCfg.p2pDisable = interworking_data->passpoint.p2pDisable;
    pCfg->IEEE80211uCfg.PasspointCfg.l2tif = interworking_data->passpoint.l2tif;
  
    //Copy the Data for message responses
    memcpy((char *)&g_interworking_data[apIns], interworking_data, sizeof(wifi_interworking_t));

    memcpy(pCfg->IEEE80211uCfg.PasspointCfg.Stats,g_interworking_data[apIns].anqp.passpointStats, sizeof(pCfg->IEEE80211uCfg.PasspointCfg.Stats)); 
#else
    UNREFERENCED_PARAMETER(apIns);
    UNREFERENCED_PARAMETER(pCfg);
    UNREFERENCED_PARAMETER(interworking_data);
#endif

    return ANSC_STATUS_SUCCESS;
}

/***********************************************************************
Funtion     : CosaDmlWiFi_ReadInterworkingConfig
Input       : Pointer to vAP object, JSON String
Description : Parses JSON String JSON_STR, and populates the vAP object pCfg 
***********************************************************************/
ANSC_STATUS CosaDmlWiFi_ReadInterworkingConfig (PCOSA_DML_WIFI_AP_CFG pCfg, char *JSON_STR)     
{
    errno_t rc = -1;
    if(!pCfg){
        wifi_passpoint_dbg_print("AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }

    int apIns = pCfg->InstanceNumber -1;
    if((apIns < 0) || (apIns > 15)){
        wifi_passpoint_dbg_print( "%s:%d: Invalid AP Index. Setting to 1\n", __func__, __LINE__);
        apIns = 0;
    }

    cJSON *mainEntry = NULL;
    cJSON *InterworkingElement = NULL;
    cJSON *venueParam = NULL;

    if(!JSON_STR){
        wifi_passpoint_dbg_print("JSON String is NULL\n");
        return ANSC_STATUS_FAILURE;
    }

    cJSON *interworkingCfg = cJSON_Parse(JSON_STR);

    if (NULL == interworkingCfg) {
        wifi_passpoint_dbg_print("Failed to parse JSON\n");
        return ANSC_STATUS_FAILURE;
    }

    mainEntry = cJSON_GetObjectItem(interworkingCfg,"Interworking");
    if(NULL == mainEntry){
        wifi_passpoint_dbg_print("Interworking entry is NULL\n");
        cJSON_Delete(interworkingCfg);
        return ANSC_STATUS_FAILURE;
    }

//Interworking Status
    InterworkingElement = cJSON_GetObjectItem(mainEntry,"InterworkingEnable");
    pCfg->InterworkingEnable = InterworkingElement ? InterworkingElement->valuedouble : 0;

//AccessNetworkType
    InterworkingElement = cJSON_GetObjectItem(mainEntry,"AccessNetworkType");
    if ( (pCfg->InstanceNumber == 5) || (pCfg->InstanceNumber == 6) || (pCfg->InstanceNumber == 9) || (pCfg->InstanceNumber == 10) )        //Xfinity hotspot vaps
    {
        pCfg->IEEE80211uCfg.IntwrkCfg.iAccessNetworkType = InterworkingElement ? InterworkingElement->valuedouble :2;
    } else {
        pCfg->IEEE80211uCfg.IntwrkCfg.iAccessNetworkType = InterworkingElement ? InterworkingElement->valuedouble :0;
    }

//ASRA
    InterworkingElement = cJSON_GetObjectItem(mainEntry,"ASRA");
    pCfg->IEEE80211uCfg.IntwrkCfg.iASRA = InterworkingElement ? InterworkingElement->valuedouble : 0;

//ESR
    InterworkingElement = cJSON_GetObjectItem(mainEntry,"ESR");
    pCfg->IEEE80211uCfg.IntwrkCfg.iESR = InterworkingElement ? InterworkingElement->valuedouble : 0;

//UESA
    InterworkingElement = cJSON_GetObjectItem(mainEntry,"UESA");
    pCfg->IEEE80211uCfg.IntwrkCfg.iUESA = InterworkingElement ? InterworkingElement->valuedouble : 0;

//HESSOptionPresent
    InterworkingElement = cJSON_GetObjectItem(mainEntry,"HESSOptionPresent");
    pCfg->IEEE80211uCfg.IntwrkCfg.iHESSOptionPresent = InterworkingElement ? InterworkingElement->valuedouble : 0;
        
//HESSID
    InterworkingElement = cJSON_GetObjectItem(mainEntry,"HESSID");
    if(InterworkingElement && InterworkingElement->valuestring){
        rc = strcpy_s(pCfg->IEEE80211uCfg.IntwrkCfg.iHESSID, sizeof(pCfg->IEEE80211uCfg.IntwrkCfg.iHESSID), InterworkingElement->valuestring);
    } else {
        rc = strcpy_s(pCfg->IEEE80211uCfg.IntwrkCfg.iHESSID, sizeof(pCfg->IEEE80211uCfg.IntwrkCfg.iHESSID), "11:22:33:44:55:66");
    }
    ERR_CHK(rc);

//VenueOptionPresent
    InterworkingElement = cJSON_GetObjectItem(mainEntry,"VenueOptionPresent");
    pCfg->IEEE80211uCfg.IntwrkCfg.iVenueOptionPresent = InterworkingElement ? InterworkingElement->valuedouble : 0;

//Venue
    InterworkingElement = cJSON_GetObjectItem(mainEntry,"Venue");
    if (InterworkingElement){
        //VenueGroup
        venueParam = cJSON_GetObjectItem(InterworkingElement,"VenueGroup");
        pCfg->IEEE80211uCfg.IntwrkCfg.iVenueGroup = venueParam ? venueParam->valuedouble : 0;

        //VenueType
        venueParam = cJSON_GetObjectItem(InterworkingElement,"VenueType");
        pCfg->IEEE80211uCfg.IntwrkCfg.iVenueType = venueParam ? venueParam->valuedouble : 0;
    }
    cJSON_Delete(interworkingCfg);
    return ANSC_STATUS_SUCCESS;
}

/***********************************************************************
Funtion     : CosaDmlWiFi_SaveInterworkingConfig
Input       : Pointer to vAP object, JSON String, length of string
Description : Saves Interworking coinfiguration as JSON String into file.
              File is saved for each vap in format InterworkingCfg_<apIns>.json
***********************************************************************/
ANSC_STATUS CosaDmlWiFi_SaveInterworkingCfg(PCOSA_DML_WIFI_AP_CFG pCfg, char *buffer, int len)
{   
    char cfgFile[64];
    DIR     *passPointDir = NULL;
    errno_t rc = -1;
    
    passPointDir = opendir(WIFI_PASSPOINT_DIR);
    if(passPointDir){
        closedir(passPointDir);
    }else if(ENOENT == errno){
        if(0 != mkdir(WIFI_PASSPOINT_DIR, 0777)){ 
            wifi_passpoint_dbg_print("Failed to Create Passpoint Configuration directory.\n");
            return ANSC_STATUS_FAILURE;
        }
    }else{
        wifi_passpoint_dbg_print("Error opening Passpoint Configuration directory.\n");
        return ANSC_STATUS_FAILURE;
    }
    
    if(!pCfg){
        wifi_passpoint_dbg_print("AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    rc = sprintf_s(cfgFile, sizeof(cfgFile), WIFI_INTERWORKING_CFG_FILE,pCfg->InstanceNumber);
    if(rc < EOK)
    {
        ERR_CHK(rc);
    }
    FILE *fPasspointCfg = fopen(cfgFile, "w");
    if(0 == fwrite(buffer, len,1, fPasspointCfg)){
        fclose(fPasspointCfg);
        return ANSC_STATUS_FAILURE;
    }else{
        fclose(fPasspointCfg);
        return ANSC_STATUS_SUCCESS;
    }
}

/***********************************************************************
Funtion     : CosaDmlWiFi_WriteInterworkingConfig
Input       : Pointer to vAP object
Description : Convert Interworking parameters to JSON String JSON_STR, 
              and pass it to CosaDmlWiFi_SaveInterworkingCfg for persistent
              storage
***********************************************************************/
ANSC_STATUS CosaDmlWiFi_WriteInterworkingConfig (PCOSA_DML_WIFI_AP_CFG pCfg)
{
    cJSON *mainEntry = NULL;
    cJSON *venueEntry = NULL;
    char JSON_STR[2048];
    int apIns;
    
    if(!pCfg){
        wifi_passpoint_dbg_print("AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    
    apIns = pCfg->InstanceNumber;
    if((apIns < 1) || (apIns > 16)){
        return ANSC_STATUS_FAILURE;
    }
    
    cJSON *interworkingCfg = cJSON_CreateObject();
    if (NULL == interworkingCfg) {
        wifi_passpoint_dbg_print("Failed to create JSON\n");
        return ANSC_STATUS_FAILURE;
    }
    
    memset(JSON_STR, 0, sizeof(JSON_STR));

    mainEntry = cJSON_AddObjectToObject(interworkingCfg,"Interworking");
    
    cJSON_AddNumberToObject(mainEntry,"InterworkingEnable",pCfg->InterworkingEnable); 
    cJSON_AddNumberToObject(mainEntry,"AccessNetworkType",pCfg->IEEE80211uCfg.IntwrkCfg.iAccessNetworkType); 
    cJSON_AddNumberToObject(mainEntry,"ASRA",pCfg->IEEE80211uCfg.IntwrkCfg.iASRA); 
    cJSON_AddNumberToObject(mainEntry,"ESR",pCfg->IEEE80211uCfg.IntwrkCfg.iESR); 
    cJSON_AddNumberToObject(mainEntry,"UESA",pCfg->IEEE80211uCfg.IntwrkCfg.iUESA); 
    cJSON_AddNumberToObject(mainEntry,"HESSOptionPresent",pCfg->IEEE80211uCfg.IntwrkCfg.iHESSOptionPresent); 
    cJSON_AddStringToObject(mainEntry, "HESSID", pCfg->IEEE80211uCfg.IntwrkCfg.iHESSID);
    venueEntry = cJSON_AddObjectToObject(mainEntry,"Venue");
    cJSON_AddNumberToObject(venueEntry,"VenueGroup",pCfg->IEEE80211uCfg.IntwrkCfg.iVenueGroup); 
    cJSON_AddNumberToObject(venueEntry,"VenueType",pCfg->IEEE80211uCfg.IntwrkCfg.iVenueType); 
    
    cJSON_PrintPreallocated(interworkingCfg, JSON_STR, sizeof(JSON_STR),false);
    cJSON_Delete(interworkingCfg);
 
    return CosaDmlWiFi_SaveInterworkingCfg(pCfg, JSON_STR, sizeof(JSON_STR));
}

/***********************************************************************
Funtion     : CosaDmlWiFi_DefaultInterworkingConfig
Input       : Pointer to vAP object
Description : Populates the vAP object pCfg with default values for 
              Interworking parameters
***********************************************************************/
ANSC_STATUS CosaDmlWiFi_DefaultInterworkingConfig(PCOSA_DML_WIFI_AP_CFG pCfg)
{       
    errno_t rc = -1;
    if(!pCfg){ 
        wifi_passpoint_dbg_print("AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }

    pCfg->InterworkingEnable = 0;
    pCfg->IEEE80211uCfg.IntwrkCfg.iASRA = 0;
    pCfg->IEEE80211uCfg.IntwrkCfg.iESR = 0;
    pCfg->IEEE80211uCfg.IntwrkCfg.iUESA = 0;
    pCfg->IEEE80211uCfg.IntwrkCfg.iHESSOptionPresent = 1;
    if( (pCfg->InstanceNumber == 1) || (pCfg->InstanceNumber == 2))
    {
	    pCfg->IEEE80211uCfg.IntwrkCfg.iInternetAvailable = 1;
    }
    rc = strcpy_s(pCfg->IEEE80211uCfg.IntwrkCfg.iHESSID, sizeof(pCfg->IEEE80211uCfg.IntwrkCfg.iHESSID), "11:22:33:44:55:66");
    ERR_CHK(rc);
    if ( (pCfg->InstanceNumber == 5) || (pCfg->InstanceNumber == 6) || (pCfg->InstanceNumber == 9) || (pCfg->InstanceNumber == 10) )	//Xfinity hotspot vaps
    {
         pCfg->IEEE80211uCfg.IntwrkCfg.iAccessNetworkType = 2;
    } else {
         pCfg->IEEE80211uCfg.IntwrkCfg.iAccessNetworkType = 0;
    }
    pCfg->IEEE80211uCfg.IntwrkCfg.iVenueOptionPresent = 1;
    pCfg->IEEE80211uCfg.IntwrkCfg.iVenueGroup = 0;
    pCfg->IEEE80211uCfg.IntwrkCfg.iVenueType = 0;
    
    return ANSC_STATUS_SUCCESS;
}

/***********************************************************************
Funtion     : CosaDmlWiFi_InitInterworkingElement
Input       : Pointer to vAP object
Description : Check for Saved Configuration.
              If not present, call CosaDmlWiFi_DefaultInterworkingConfig
              to populate default values
***********************************************************************/
ANSC_STATUS CosaDmlWiFi_InitInterworkingElement (PCOSA_DML_WIFI_AP_CFG pCfg)
{
#if defined (FEATURE_SUPPORT_PASSPOINT)

#if defined(ENABLE_FEATURE_MESHWIFI)        
    wifi_InterworkingElement_t  elem;
    errno_t rc = -1;
    memset((char *)&elem, 0, sizeof(wifi_InterworkingElement_t));
    //Update OVS DB
#ifdef WIFI_HAL_VERSION_3
    if(-1 == get_ovsdb_interworking_config(getVAPName(pCfg->InstanceNumber - 1),&elem)) {
#else
    char *vap_name[] = {"private_ssid_2g", "private_ssid_5g", "iot_ssid_2g", "iot_ssid_5g", "hotspot_open_2g", "hotspot_open_5g", "lnf_psk_2g", "lnf_psk_5g", "hotspot_secure_2g", "hotspot_secure_5g"};
    if(-1 == get_ovsdb_interworking_config(vap_name[pCfg->InstanceNumber - 1],&elem)) {
#endif
        wifi_passpoint_dbg_print("Failed to Initialize Interworking Configuration from DB for AP: %d. Setting Default\n",pCfg->InstanceNumber);
        return CosaDmlWiFi_DefaultInterworkingConfig(pCfg);
    }
    
    pCfg->InterworkingEnable = elem.interworkingEnabled;
    pCfg->IEEE80211uCfg.IntwrkCfg.iAccessNetworkType = elem.accessNetworkType;
    pCfg->IEEE80211uCfg.IntwrkCfg.iInternetAvailable = elem.internetAvailable;
    pCfg->IEEE80211uCfg.IntwrkCfg.iASRA = elem.asra;
    pCfg->IEEE80211uCfg.IntwrkCfg.iESR = elem.esr;
    pCfg->IEEE80211uCfg.IntwrkCfg.iUESA = elem.uesa;
    pCfg->IEEE80211uCfg.IntwrkCfg.iVenueOptionPresent = elem.venueOptionPresent;
    pCfg->IEEE80211uCfg.IntwrkCfg.iVenueType = elem.venueType;
    pCfg->IEEE80211uCfg.IntwrkCfg.iVenueGroup = elem.venueGroup;
    pCfg->IEEE80211uCfg.IntwrkCfg.iHESSOptionPresent = elem.hessOptionPresent;
    rc = strcpy_s(pCfg->IEEE80211uCfg.IntwrkCfg.iHESSID, sizeof(pCfg->IEEE80211uCfg.IntwrkCfg.iHESSID), elem.hessid);
    ERR_CHK(rc);

#else
    char cfgFile[64];
    char *JSON_STR = NULL;
    int apIns = 0; 
    long confSize = 0; 
    errno_t rc = -1;

    if(!pCfg){
        wifi_passpoint_dbg_print("AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }    

    apIns = pCfg->InstanceNumber;
    if((apIns < 1) || (apIns > 16)){
        wifi_passpoint_dbg_print( "%s:%d: Invalid AP Index. Return\n", __func__, __LINE__);
        return ANSC_STATUS_FAILURE;
    }    

    rc = sprintf_s(cfgFile, sizeof(cfgFile), WIFI_INTERWORKING_CFG_FILE,apIns);
    if(rc < EOK)
    {
        ERR_CHK(rc);
    }

    confSize = readFileToBuffer(cfgFile,&JSON_STR);

    if(!confSize || !JSON_STR || (ANSC_STATUS_SUCCESS != CosaDmlWiFi_ReadInterworkingConfig(pCfg,JSON_STR))){
        if(JSON_STR){
            free(JSON_STR);
            JSON_STR = NULL;
        }    
        wifi_passpoint_dbg_print("Failed to Initialize Interworking Configuration from memory for AP: %d. Setting Default\n",apIns);
        return CosaDmlWiFi_DefaultInterworkingConfig(pCfg);
    }    
    wifi_passpoint_dbg_print("Initialized Interworking Configuration from memory for AP: %d.\n",apIns);

#endif
    return ANSC_STATUS_SUCCESS;
#else
    return CosaDmlWiFi_DefaultInterworkingConfig(pCfg);
#endif
}

void update_json_gas_config(wifi_GASConfiguration_t *gasConfig_struct) {
    cJSON *gasCfg = NULL;
    cJSON *mainEntry = NULL;
    char *JSON_STR = NULL;

    gasCfg = cJSON_CreateObject();
    if (NULL == gasCfg) {
        wifi_passpoint_dbg_print("Failed to create GAS JSON Object\n");
        return;
    }

    mainEntry = cJSON_AddObjectToObject(gasCfg,"GasConfig");    
    cJSON_AddNumberToObject(mainEntry,"AdvertisementId",gasConfig_struct->AdvertisementID);
    cJSON_AddBoolToObject(mainEntry,"PauseForServerResp",gasConfig_struct->PauseForServerResponse);
    cJSON_AddNumberToObject(mainEntry,"RespTimeout",gasConfig_struct->ResponseTimeout);
    cJSON_AddNumberToObject(mainEntry,"ComebackDelay",gasConfig_struct->ComeBackDelay);
    cJSON_AddNumberToObject(mainEntry,"RespBufferTime",gasConfig_struct->ResponseBufferingTime);
    cJSON_AddNumberToObject(mainEntry,"QueryRespLengthLimit",gasConfig_struct->QueryResponseLengthLimit);

    JSON_STR = malloc(512);
    memset(JSON_STR, 0, 512);
    cJSON_PrintPreallocated(gasCfg, JSON_STR,512,false);

    if (ANSC_STATUS_SUCCESS != CosaDmlWiFi_SaveGasCfg(JSON_STR, strlen(JSON_STR))) {
        wifi_passpoint_dbg_print("Failed to update OVSDB with GAS Config. Adv-ID:%d\n",gasConfig_struct->AdvertisementID);
    }
    cJSON_Delete(gasCfg);

    if(JSON_STR){
        free(JSON_STR);
        JSON_STR = NULL;
    }
}
