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

#include "wifi_data_plane.h"
#include "wifi_monitor.h"
#include "plugin_main_apis.h"
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/un.h>
#include <assert.h>
#include "ansc_status.h"
#include <sysevent/sysevent.h>
#include <cJSON.h>
#include <dirent.h>
#include <errno.h>

#define GAS_CFG_TYPE_SUPPORTED 1
#define GAS_STATS_FIXED_WINDOW_SIZE 10
#define GAS_STATS_TIME_OUT 60

static cosa_wifi_anqp_data_t g_anqp_data[16];
static cosa_wifi_hs2_data_t g_hs2_data[16];
static COSA_DML_WIFI_GASSTATS gasStats[GAS_CFG_TYPE_SUPPORTED];
 
extern ANSC_HANDLE bus_handle;
extern char        g_Subsystem[32];
extern wifi_data_plane_t g_data_plane_module;
extern PCOSA_BACKEND_MANAGER_OBJECT g_pCosaBEManager;

void destroy_passpoint (void);

INT wifi_setGASConfiguration(UINT advertisementID, wifi_GASConfiguration_t *input_struct);

static wifi_passpoint_t g_passpoint;

static void wifi_passpoint_dbg_print(int level, char *format, ...)
{
    char buff[2048] = {0};
    va_list list;
    static FILE *fpg = NULL;

    if ((access("/nvram/wifiPasspointDbg", R_OK)) != 0) {
        return;
    }

    get_formatted_time(buff);
    strcat(buff, " ");

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
    	wifi_passpoint_dbg_print(1," Current instance matches. \n");
    } else {
        wifi_passpoint_dbg_print(1," Current instance does not match. Resetting\n");
        g_passpoint.wifi_dml = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    }

    if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->AccessPointQueue, apIndex)) == NULL){
        wifi_passpoint_dbg_print(1," pSLinkEntry is NULL. \n");
        return NULL;
    }

    if((pAPLinkObj = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)) == NULL){
        wifi_passpoint_dbg_print(1," pAPLinkObj is NULL. \n");
        return NULL;
    }

    if((pWifiAp = pAPLinkObj->hContext) == NULL){
        wifi_passpoint_dbg_print(1," pWifiAp is NULL. \n");
        return NULL;
    }

    if((pEntry = &pWifiAp->AP) == NULL){
        wifi_passpoint_dbg_print(1," pEntry is NULL. \n");
        return NULL;
    }
    wifi_passpoint_dbg_print(1," returning %x. \n",&pEntry->Cfg);
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
            wifi_passpoint_dbg_print(1,"Failed to Create Passpoint Configuration directory.\n");
            return 0;
        }
    }else{
        wifi_passpoint_dbg_print(1,"Error opening Passpoint Configuration directory.\n");
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
        wifi_passpoint_dbg_print(1,"Error in getting the num of bytes\n");
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
       wifi_passpoint_dbg_print(1,"Failed to read the buffer.\n");
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
    int rc;
    int count = 0;
    mac_addr_str_t mac_str;
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
        wifi_passpoint_dbg_print(1, "%s:%d: Invalid AP Index: %d.\n", __func__, __LINE__,apIns);
        return;
    }
      
    //A gas query received increase the stats.
    gasStats[GAS_CFG_TYPE_SUPPORTED - 1].Queries++;

    //Check RFC value. Return NUll is not enabled
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.WiFi-Passpoint.Enable", NULL, &strValue);
    if ((retPsmGet != CCSP_SUCCESS) || (false == _ansc_atoi(strValue))){
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
        wifi_passpoint_dbg_print(1, "%s:%d: Received ANQP Request. RFC Disabled\n", __func__, __LINE__);
    }else if(g_hs2_data[apIns].hs2Status != true){
        wifi_passpoint_dbg_print(1, "%s:%d: Received ANQP Request. Passpoint is Disabled on AP: %d\n", __func__, __LINE__,apIns);
    }else{
        anqpList = anqpReq->head;
    }

#if defined (FEATURE_SUPPORT_PASSPOINT)
    UINT prevRealmCnt = g_hs2_data[apIns].realmRespCount;
    UINT prevDomainCnt = g_hs2_data[apIns].domainRespCount;
    UINT prev3gppCnt = g_hs2_data[apIns].gppRespCount;
#endif

    while(anqpList){
        anqpList->value->len = 0;
        if(anqpList->value->data){
            free(anqpList->value->data);
            anqpList->value->data = NULL;
        }
        if(anqpList->value->type == wifi_anqp_id_type_anqp){
            wifi_passpoint_dbg_print(1, "%s:%d: Received ANQP Request\n", __func__, __LINE__);
            switch (anqpList->value->u.anqp_elem_id){
                //CapabilityListANQPElement
                case wifi_anqp_element_name_capability_list:
                    capLen = (g_anqp_data[apIns].capabilityInfoLength * sizeof(USHORT)) + sizeof(wifi_vendor_specific_anqp_capabilities_t) + g_hs2_data[apIns].capabilityInfoLength;
                    wifi_passpoint_dbg_print(1, "%s:%d: Received CapabilityListANQPElement Request\n", __func__, __LINE__);
                    anqpList->value->data = malloc(capLen);//To be freed in wifi_anqpSendResponse()
                    if(NULL == anqpList->value->data){
                        wifi_passpoint_dbg_print(1, "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                        if(mallocRetryCount > 5){
                            break;
                        }
                        mallocRetryCount++;
                        anqpList = anqpList->next;
                        continue;
                    }
                    data_pos = anqpList->value->data;
                    wifi_passpoint_dbg_print(1, "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,capLen);
                    memset(data_pos,0,capLen);
                    memcpy(data_pos,&g_anqp_data[apIns].capabilityInfo,(g_anqp_data[apIns].capabilityInfoLength * sizeof(USHORT)));
                    data_pos += (g_anqp_data[apIns].capabilityInfoLength * sizeof(USHORT));
                    wifi_vendor_specific_anqp_capabilities_t *vendorInfo = (wifi_vendor_specific_anqp_capabilities_t *)data_pos;
                    vendorInfo->info_id = wifi_anqp_element_name_vendor_specific;
                    vendorInfo->len = g_hs2_data[apIns].capabilityInfoLength + sizeof(vendorInfo->oi) + sizeof(vendorInfo->wfa_type);
                    memcpy(vendorInfo->oi, wfa_oui, sizeof(wfa_oui));
                    vendorInfo->wfa_type = 0x11;
                    data_pos += sizeof(wifi_vendor_specific_anqp_capabilities_t);
                    memcpy(data_pos, &g_hs2_data[apIns].capabilityInfo, g_hs2_data[apIns].capabilityInfoLength);
                    anqpList->value->len = capLen;
                    respLength += anqpList->value->len;
                    wifi_passpoint_dbg_print(1, "%s:%d: Copied CapabilityListANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                    break;
                //IPAddressTypeAvailabilityANQPElement
                case wifi_anqp_element_name_ip_address_availabality:
                    wifi_passpoint_dbg_print(1, "%s:%d: Received IPAddressTypeAvailabilityANQPElement Request\n", __func__, __LINE__);
                    if(g_anqp_data[apIns].ipAddressInfo){
                        anqpList->value->data = malloc(sizeof(wifi_ipAddressAvailabality_t));//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                            wifi_passpoint_dbg_print(1, "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                            if(mallocRetryCount > 5){
                                break;
                            }
                            mallocRetryCount++;
                            anqpList = anqpList->next;
                            continue;
                        }
                        mallocRetryCount = 0;
                        wifi_passpoint_dbg_print(1, "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,sizeof(wifi_ipAddressAvailabality_t));
                        memset(anqpList->value->data,0,sizeof(wifi_ipAddressAvailabality_t));
                        memcpy(anqpList->value->data,g_anqp_data[apIns].ipAddressInfo,sizeof(wifi_ipAddressAvailabality_t));
                        anqpList->value->len = sizeof(wifi_ipAddressAvailabality_t);
                        respLength += anqpList->value->len;
                        wifi_passpoint_dbg_print(1, "%s:%d: Copied IPAddressTypeAvailabilityANQPElement Data. Length: %d. Data: %02X\n", __func__, __LINE__,anqpList->value->len, ((wifi_ipAddressAvailabality_t *)anqpList->value->data)->field_format);
                    }
                    break;
                //NAIRealmANQPElement
                case wifi_anqp_element_name_nai_realm:
                    wifi_passpoint_dbg_print(1, "%s:%d: Received NAIRealmANQPElement Request\n", __func__, __LINE__);
                    if(g_anqp_data[apIns].realmInfoLength && g_anqp_data[apIns].realmInfo){
                        anqpList->value->data = malloc(g_anqp_data[apIns].realmInfoLength);//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                            wifi_passpoint_dbg_print(1, "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                            if(mallocRetryCount > 5){
                                break;
                            }
                            mallocRetryCount++;
                            anqpList = anqpList->next;
                            continue;
                        }
                        wifi_passpoint_dbg_print(1, "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,g_anqp_data[apIns].realmInfoLength);
                        memset(anqpList->value->data,0,g_anqp_data[apIns].realmInfoLength);
                        memcpy(anqpList->value->data,g_anqp_data[apIns].realmInfo,g_anqp_data[apIns].realmInfoLength);
                        anqpList->value->len = g_anqp_data[apIns].realmInfoLength;
                        respLength += anqpList->value->len;
                        wifi_passpoint_dbg_print(1, "%s:%d: Copied NAIRealmANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
              
                        g_hs2_data[apIns].realmRespCount++;
                    }
                    break;
                //VenueNameANQPElement
                case wifi_anqp_element_name_venue_name:
                    wifi_passpoint_dbg_print(1, "%s:%d: Received VenueNameANQPElement Request\n", __func__, __LINE__);
                    if(g_anqp_data[apIns].venueInfoLength && g_anqp_data[apIns].venueInfo){
                        anqpList->value->data = malloc(g_anqp_data[apIns].venueInfoLength);//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                            wifi_passpoint_dbg_print(1, "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                            if(mallocRetryCount > 5){
                                break;
                            }
                            mallocRetryCount++;
                            anqpList = anqpList->next;
                            continue;
                        }
                        wifi_passpoint_dbg_print(1, "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,g_anqp_data[apIns].venueInfoLength);
                        memset(anqpList->value->data,0,g_anqp_data[apIns].venueInfoLength);
                        memcpy(anqpList->value->data,g_anqp_data[apIns].venueInfo,g_anqp_data[apIns].venueInfoLength);
                        anqpList->value->len = g_anqp_data[apIns].venueInfoLength;
                        respLength += anqpList->value->len;
                        wifi_passpoint_dbg_print(1, "%s:%d: Copied VenueNameANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                    }
                    break;
                //3GPPCellularANQPElement
                case wifi_anqp_element_name_3gpp_cellular_network:
                    wifi_passpoint_dbg_print(1, "%s:%d: Received 3GPPCellularANQPElement Request\n", __func__, __LINE__);
                    if(g_anqp_data[apIns].gppInfoLength && g_anqp_data[apIns].gppInfo){
                        anqpList->value->data = malloc(g_anqp_data[apIns].gppInfoLength);//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                            wifi_passpoint_dbg_print(1, "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                            if(mallocRetryCount > 5){
                                break;
                            }
                            mallocRetryCount++;
                            anqpList = anqpList->next;
                            continue;
                        }
                        wifi_passpoint_dbg_print(1, "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,g_anqp_data[apIns].gppInfoLength);
                        memset(anqpList->value->data,0,g_anqp_data[apIns].gppInfoLength);
                        memcpy(anqpList->value->data,g_anqp_data[apIns].gppInfo,g_anqp_data[apIns].gppInfoLength);
                        anqpList->value->len = g_anqp_data[apIns].gppInfoLength;
                        respLength += anqpList->value->len;
                        wifi_passpoint_dbg_print(1, "%s:%d: Copied 3GPPCellularANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                        g_hs2_data[apIns].gppRespCount++;
                    }
                    break;
                //RoamingConsortiumANQPElement
                case wifi_anqp_element_name_roaming_consortium:
                    wifi_passpoint_dbg_print(1, "%s:%d: Received RoamingConsortiumANQPElement Request\n", __func__, __LINE__);
                    if(g_anqp_data[apIns].roamInfoLength && g_anqp_data[apIns].roamInfo){
                        anqpList->value->data = malloc(g_anqp_data[apIns].roamInfoLength);//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                            wifi_passpoint_dbg_print(1, "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                            if(mallocRetryCount > 5){
                                break;
                            }
                            mallocRetryCount++;
                            anqpList = anqpList->next;
                            continue;
                        }
                        wifi_passpoint_dbg_print(1, "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,g_anqp_data[apIns].roamInfoLength);
                        memset(anqpList->value->data,0,g_anqp_data[apIns].roamInfoLength);
                        memcpy(anqpList->value->data,g_anqp_data[apIns].roamInfo,g_anqp_data[apIns].roamInfoLength);
                        anqpList->value->len = g_anqp_data[apIns].roamInfoLength;
                        respLength += anqpList->value->len;
                        wifi_passpoint_dbg_print(1, "%s:%d: Copied RoamingConsortiumANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                    }
                    break;
                //DomainANQPElement
                case wifi_anqp_element_name_domain_name:
                    wifi_passpoint_dbg_print(1, "%s:%d: Received DomainANQPElement Request\n", __func__, __LINE__);
                    if(g_anqp_data[apIns].domainInfoLength && g_anqp_data[apIns].domainNameInfo){
                        anqpList->value->data = malloc(g_anqp_data[apIns].domainInfoLength);//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                            wifi_passpoint_dbg_print(1, "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                            if(mallocRetryCount > 5){
                                break;
                            }
                            mallocRetryCount++;
                            anqpList = anqpList->next;
                            continue;
                        }
                        wifi_passpoint_dbg_print(1, "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,g_anqp_data[apIns].domainInfoLength);
                        memset(anqpList->value->data,0,g_anqp_data[apIns].domainInfoLength);
                        memcpy(anqpList->value->data,g_anqp_data[apIns].domainNameInfo,g_anqp_data[apIns].domainInfoLength);
                        anqpList->value->len = g_anqp_data[apIns].domainInfoLength;
                        respLength += anqpList->value->len;
                        wifi_passpoint_dbg_print(1, "%s:%d: Copied DomainANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                        g_hs2_data[apIns].domainRespCount++;
                    }
                    break;
               default:
                    wifi_passpoint_dbg_print(1, "%s:%d: Received Unsupported ANQPElement Request: %d\n", __func__, __LINE__,anqpList->value->u.anqp_elem_id);
                    break;
            }     
        } else if (anqpList->value->type == wifi_anqp_id_type_hs){
            wifi_passpoint_dbg_print(1, "%s:%d: Received HS2 ANQP Request\n", __func__, __LINE__);
            switch (anqpList->value->u.anqp_hs_id){
                //CapabilityListANQPElement
                case wifi_anqp_element_hs_subtype_hs_capability_list:
                    wifi_passpoint_dbg_print(1, "%s:%d: Received CapabilityListANQPElement Request\n", __func__, __LINE__);
                    if(g_hs2_data[apIns].capabilityInfoLength){
                        anqpList->value->data = malloc(g_hs2_data[apIns].capabilityInfoLength);//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                            wifi_passpoint_dbg_print(1, "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                            if(mallocRetryCount > 5){
                                break;
                            }
                            mallocRetryCount++;
                            anqpList = anqpList->next;
                            continue;
                        }
                        wifi_passpoint_dbg_print(1, "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,g_hs2_data[apIns].capabilityInfoLength);
                        memset(anqpList->value->data,0,g_hs2_data[apIns].capabilityInfoLength);
                        memcpy(anqpList->value->data,&g_hs2_data[apIns].capabilityInfo,g_hs2_data[apIns].capabilityInfoLength);
                        anqpList->value->len = g_hs2_data[apIns].capabilityInfoLength;
                        respLength += anqpList->value->len;
                        wifi_passpoint_dbg_print(1, "%s:%d: Copied CapabilityListANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                    }
                    break;
                //OperatorFriendlyNameANQPElement
                case wifi_anqp_element_hs_subtype_operator_friendly_name:
                    wifi_passpoint_dbg_print(1, "%s:%d: Received OperatorFriendlyNameANQPElement Request\n", __func__, __LINE__);
                    if(g_hs2_data[apIns].opFriendlyNameInfoLength && g_hs2_data[apIns].opFriendlyNameInfo){
                        anqpList->value->data = malloc(g_hs2_data[apIns].opFriendlyNameInfoLength);//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                            wifi_passpoint_dbg_print(1, "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                            if(mallocRetryCount > 5){
                                break;
                            }
                            mallocRetryCount++;
                            anqpList = anqpList->next;
                            continue;
                        }
                        wifi_passpoint_dbg_print(1, "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,g_hs2_data[apIns].opFriendlyNameInfoLength);
                        memset(anqpList->value->data,0,g_hs2_data[apIns].opFriendlyNameInfoLength);
                        memcpy(anqpList->value->data,g_hs2_data[apIns].opFriendlyNameInfo,g_hs2_data[apIns].opFriendlyNameInfoLength);
                        anqpList->value->len = g_hs2_data[apIns].opFriendlyNameInfoLength;
                        respLength += anqpList->value->len;
                        wifi_passpoint_dbg_print(1, "%s:%d: Copied OperatorFriendlyNameANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                    }
                    break;
                //ConnectionCapabilityListANQPElement
                case wifi_anqp_element_hs_subtype_conn_capability:
                    wifi_passpoint_dbg_print(1, "%s:%d: Received ConnectionCapabilityListANQPElement Request\n", __func__, __LINE__);
                    if(g_hs2_data[apIns].connCapabilityLength && g_hs2_data[apIns].connCapabilityInfo){
                        anqpList->value->data = malloc(g_hs2_data[apIns].connCapabilityLength);//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                            wifi_passpoint_dbg_print(1, "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                            if(mallocRetryCount > 5){
                                break;
                            }
                            mallocRetryCount++;
                            anqpList = anqpList->next;
                            continue;
                        }
                        wifi_passpoint_dbg_print(1, "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,g_hs2_data[apIns].connCapabilityLength);
                        memset(anqpList->value->data,0,g_hs2_data[apIns].connCapabilityLength);
                        memcpy(anqpList->value->data,g_hs2_data[apIns].connCapabilityInfo,g_hs2_data[apIns].connCapabilityLength);
                        anqpList->value->len = g_hs2_data[apIns].connCapabilityLength;
                        respLength += anqpList->value->len;
                        wifi_passpoint_dbg_print(1, "%s:%d: Copied ConnectionCapabilityListANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                    }
                    break;
                //NAIHomeRealmANQPElement
                case wifi_anqp_element_hs_subtype_nai_home_realm_query:
                    wifi_passpoint_dbg_print(1, "%s:%d: Received NAIHomeRealmANQPElement Request\n", __func__, __LINE__);
                    if(g_hs2_data[apIns].realmInfoLength && g_hs2_data[apIns].realmInfo){
                        anqpList->value->data = malloc(g_hs2_data[apIns].realmInfoLength);//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                            wifi_passpoint_dbg_print(1, "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                            if(mallocRetryCount > 5){
                                break;
                            }
                            mallocRetryCount++;
                            anqpList = anqpList->next;
                            continue;
                        }
                        wifi_passpoint_dbg_print(1, "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,g_hs2_data[apIns].realmInfoLength);
                        memset(anqpList->value->data,0,g_hs2_data[apIns].realmInfoLength);
                        memcpy(anqpList->value->data,g_hs2_data[apIns].realmInfo,g_hs2_data[apIns].realmInfoLength);
                        anqpList->value->len = g_hs2_data[apIns].realmInfoLength;
                        respLength += anqpList->value->len;
                        wifi_passpoint_dbg_print(1, "%s:%d: Copied NAIHomeRealmANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                    }
                    break;
                //WANMetricsANQPElement
                case wifi_anqp_element_hs_subtype_wan_metrics:
                    wifi_passpoint_dbg_print(1, "%s:%d: Received WANMetricsANQPElement Request\n", __func__, __LINE__);
                    anqpList->value->data = malloc(sizeof(wifi_HS2_WANMetrics_t));//To be freed in wifi_anqpSendResponse()
                        if(NULL == anqpList->value->data){
                        wifi_passpoint_dbg_print(1, "%s:%d: Failed to allocate memory\n", __func__, __LINE__);
                        if(mallocRetryCount > 5){
                            break;
                        }
                        mallocRetryCount++;
                        anqpList = anqpList->next;
                        continue;
                    }
                    wifi_passpoint_dbg_print(1, "%s:%d: Preparing to Copy Data. Length: %d\n", __func__, __LINE__,sizeof(wifi_HS2_WANMetrics_t));
                    memset(anqpList->value->data,0,sizeof(wifi_HS2_WANMetrics_t));
                    memcpy(anqpList->value->data,&g_hs2_data[apIns].wanMetricsInfo,sizeof(wifi_HS2_WANMetrics_t));
                    anqpList->value->len = sizeof(wifi_HS2_WANMetrics_t);
                    respLength += anqpList->value->len;
                    wifi_passpoint_dbg_print(1, "%s:%d: Copied WANMetricsANQPElement Data. Length: %d\n", __func__, __LINE__,anqpList->value->len);
                    break;
               default:
                    wifi_passpoint_dbg_print(1, "%s:%d: Received Unsupported HS2ANQPElement Request: %d\n", __func__, __LINE__,anqpList->value->u.anqp_hs_id);
                    break;
            }
        }else{
            wifi_passpoint_dbg_print(1, "%s:%d: Invalid Request Type\n", __func__, __LINE__);
        }
        anqpList = anqpList->next;
    }
#if defined (FEATURE_SUPPORT_PASSPOINT)
    if(respLength == 0){
           wifi_passpoint_dbg_print(1, "%s:%d: Requested ANQP parameter is NULL\n", __func__, __LINE__);
    }
    if(RETURN_OK != (wifi_anqpSendResponse(anqpReq->apIndex, anqpReq->sta, anqpReq->token,  anqpReq->head))){
        //We have failed to send a gas response increase the stats
        gasStats[GAS_CFG_TYPE_SUPPORTED - 1].FailedResponses++;

        if(prevRealmCnt != g_hs2_data[apIns].realmRespCount){
            g_hs2_data[apIns].realmRespCount = prevRealmCnt;
            g_hs2_data[apIns].realmFailedCount++;
        }
        if(prevDomainCnt != g_hs2_data[apIns].domainRespCount){
            g_hs2_data[apIns].domainRespCount = prevDomainCnt;
            g_hs2_data[apIns].domainFailedCount++;
        }
        if(prev3gppCnt != g_hs2_data[apIns].gppRespCount){
            g_hs2_data[apIns].gppRespCount = prev3gppCnt;
            g_hs2_data[apIns].gppFailedCount++;
        }
        wifi_passpoint_dbg_print(1, "%s:%d: Failed to send ANQP Response. Clear Request and Continue\n", __func__, __LINE__);
    }else{
        //We have sent a gas response increase the stats
        gasStats[GAS_CFG_TYPE_SUPPORTED - 1].Responses++;
        wifi_passpoint_dbg_print(1, "%s:%d: Successfully sent ANQP Response.\n", __func__, __LINE__);
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
    wifi_passpoint_dbg_print(1, "%s:%d: Received ANQP Request. Pushing to Data Plane Queue.\n", __func__, __LINE__);
    data_plane_queue_push(data_plane_queue_create_event(anqpReq,wifi_data_plane_event_type_anqp, true));
}

int init_passpoint (void)
{
#if defined (FEATURE_SUPPORT_PASSPOINT)
    wifi_passpoint_dbg_print(1, "%s:%d: Initializing Passpoint\n", __func__, __LINE__);

    if(RETURN_OK != wifi_anqp_request_callback_register(anqpRequest_callback)) {
        wifi_passpoint_dbg_print(1, "%s:%d: Failed to Initialize ANQP Callback\n", __func__, __LINE__);
        return RETURN_ERR;
    }
#endif
    return RETURN_OK;
}

ANSC_STATUS CosaDmlWiFi_initPasspoint(void)
{
    if ((init_passpoint() < 0)) {
        wifi_passpoint_dbg_print("%s %d: init_passpoint Failed\n", __func__, __LINE__);
        return ANSC_STATUS_FAILURE;
    }
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS CosaDmlWiFi_SetGasConfig(PANSC_HANDLE phContext, char *JSON_STR)
{
    PCOSA_DATAMODEL_WIFI    pMyObject               = ( PCOSA_DATAMODEL_WIFI )phContext;
    wifi_GASConfiguration_t gasConfig_struct;

    if(!pMyObject){
        wifi_passpoint_dbg_print(1,"Wifi Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    PCOSA_DML_WIFI_GASCFG  pGASconf = NULL;

    cJSON *gasList = NULL;
    cJSON *gasEntry = NULL;
    cJSON *gasParam = NULL;

    if(!JSON_STR){
        wifi_passpoint_dbg_print(1,"Failed to read JSON\n");
        return ANSC_STATUS_FAILURE;
    }

    cJSON *passPointCfg = cJSON_Parse(JSON_STR);

    if (NULL == passPointCfg) {
        wifi_passpoint_dbg_print(1,"Failed to parse JSON\n");
        return ANSC_STATUS_FAILURE;
    }

    gasList = cJSON_GetObjectItem(passPointCfg,"gasConfig");
    if(NULL == gasList){
        wifi_passpoint_dbg_print(1,"gasList is NULL\n");
        cJSON_Delete(passPointCfg);
        return ANSC_STATUS_FAILURE;
    }  
    cJSON_ArrayForEach(gasEntry, gasList) {
        gasParam = cJSON_GetObjectItem(gasEntry,"advertId");
        if((!gasParam) || (0 != gasParam->valuedouble)){
            wifi_passpoint_dbg_print(1,"Invalid Configuration. Only Advertisement ID 0 - ANQP is Supported\n");
            cJSON_Delete(passPointCfg);
            return ANSC_STATUS_FAILURE;
        }
        gasConfig_struct.AdvertisementID = 0;
        pGASconf = &pMyObject->GASCfg[gasConfig_struct.AdvertisementID];
        gasParam = cJSON_GetObjectItem(gasEntry,"pauseForServerResp");
        gasConfig_struct.PauseForServerResponse = gasParam ? (((gasParam->type & cJSON_True) !=0) ? true:false) : pGASconf->PauseForServerResponse;
        gasParam = cJSON_GetObjectItem(gasEntry,"respTimeout");
        if((gasParam) && ((gasParam->valuedouble < 1000) || (gasParam->valuedouble > 65535))){
            wifi_passpoint_dbg_print(1,"Invalid Configuration. ResponseTimeout should be between 1000 and 65535\n");
            cJSON_Delete(passPointCfg);
            return ANSC_STATUS_FAILURE;
        }
        gasConfig_struct.ResponseTimeout = gasParam ? gasParam->valuedouble : pGASconf->ResponseTimeout;
        gasParam = cJSON_GetObjectItem(gasEntry,"comebackDelay");
        if((gasParam) && ((gasParam->valuedouble < 0) || (gasParam->valuedouble > 65535))){
            wifi_passpoint_dbg_print(1,"Invalid Configuration. comebackDelay should be between 0 and 65535\n");
            cJSON_Delete(passPointCfg);
            return ANSC_STATUS_FAILURE;
        }
        gasConfig_struct.ComeBackDelay = gasParam ? gasParam->valuedouble : pGASconf->ComeBackDelay;
        gasParam = cJSON_GetObjectItem(gasEntry,"respBufferTime");
        if((gasParam) && ((gasParam->valuedouble < 0) || (gasParam->valuedouble > 65535))){
            wifi_passpoint_dbg_print(1,"Invalid Configuration. respBufferTime should be between 0 and 65535\n");
            cJSON_Delete(passPointCfg);
            return ANSC_STATUS_FAILURE;
        }
        gasConfig_struct.ResponseBufferingTime = gasParam ? gasParam->valuedouble : pGASconf->ResponseBufferingTime;
        gasParam = cJSON_GetObjectItem(gasEntry,"queryRespLengthLimit");
        if((gasParam) && ((gasParam->valuedouble < 1) || (gasParam->valuedouble > 127))){
            wifi_passpoint_dbg_print(1,"Invalid Configuration. queryRespLengthLimit should be between 1 and 127\n");
            cJSON_Delete(passPointCfg);
            return ANSC_STATUS_FAILURE;
        }
        gasConfig_struct.QueryResponseLengthLimit = gasParam ? gasParam->valuedouble : pGASconf->QueryResponseLengthLimit;
    }

#if defined (FEATURE_SUPPORT_PASSPOINT)
    if(RETURN_OK == wifi_setGASConfiguration(gasConfig_struct.AdvertisementID, &gasConfig_struct)){
#endif 
        pGASconf->AdvertisementID = gasConfig_struct.AdvertisementID; 
        pGASconf->PauseForServerResponse = gasConfig_struct.PauseForServerResponse;
        pGASconf->ResponseTimeout = gasConfig_struct.ResponseTimeout;
        pGASconf->ComeBackDelay = gasConfig_struct.ComeBackDelay;
        pGASconf->ResponseBufferingTime = gasConfig_struct.ResponseBufferingTime;
        pGASconf->QueryResponseLengthLimit = gasConfig_struct.QueryResponseLengthLimit;
        cJSON_Delete(passPointCfg);
        return ANSC_STATUS_SUCCESS;
#if defined (FEATURE_SUPPORT_PASSPOINT)      
      }
#endif 
    wifi_passpoint_dbg_print(1,"Failed to update HAL with GAS Config. Adv-ID:%d\n",gasConfig_struct.AdvertisementID);
    cJSON_Delete(passPointCfg);
    return ANSC_STATUS_FAILURE;
}

ANSC_STATUS CosaDmlWiFi_DefaultGasConfig(PANSC_HANDLE phContext)
{
    PCOSA_DATAMODEL_WIFI    pMyObject               = ( PCOSA_DATAMODEL_WIFI )phContext;

    if(!pMyObject){
        wifi_passpoint_dbg_print(1,"Wifi Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    char *JSON_STR = malloc(strlen(WIFI_PASSPOINT_DEFAULT_GAS_CFG)+1);
    /*CID: 121790 Dereference before null check*/
    if (JSON_STR == NULL) {
        wifi_passpoint_dbg_print(1,"malloc failure\n");
        return ANSC_STATUS_FAILURE;
    }
    memset(JSON_STR,0,(strlen(WIFI_PASSPOINT_DEFAULT_GAS_CFG)+1));
    AnscCopyString(JSON_STR, WIFI_PASSPOINT_DEFAULT_GAS_CFG);

    if(!JSON_STR || (ANSC_STATUS_SUCCESS != CosaDmlWiFi_SetGasConfig(phContext,JSON_STR))){
        if(JSON_STR){
            free(JSON_STR);
            JSON_STR = NULL;
        }
        wifi_passpoint_dbg_print(1,"Failed to update HAL with default GAS Config.\n");
        return ANSC_STATUS_FAILURE;
    }
    pMyObject->GASConfiguration = JSON_STR;
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
            wifi_passpoint_dbg_print(1,"Failed to Create Passpoint Configuration directory. Setting Default\n");
            return ANSC_STATUS_FAILURE;;
        }
    }else{
        wifi_passpoint_dbg_print(1,"Error opening Passpoint Configuration directory. Setting Default\n");
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

ANSC_STATUS CosaDmlWiFi_InitGasConfig(PANSC_HANDLE phContext)
{
    PCOSA_DATAMODEL_WIFI    pMyObject               = ( PCOSA_DATAMODEL_WIFI )phContext;

    if(!pMyObject){
        wifi_passpoint_dbg_print(1,"Wifi Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    g_passpoint.wifi_dml = pMyObject; 
    pMyObject->GASConfiguration = NULL;
    char *JSON_STR = NULL;
   
    long confSize = readFileToBuffer(WIFI_PASSPOINT_GAS_CFG_FILE,&JSON_STR);

    if(!confSize || !JSON_STR || (ANSC_STATUS_SUCCESS != CosaDmlWiFi_SetGasConfig(phContext,JSON_STR))){
        if(JSON_STR){
            free(JSON_STR);
            JSON_STR = NULL;
        }
        wifi_passpoint_dbg_print(1,"Failed to Initialize GAS Configuration from memory. Setting Default\n");
        return CosaDmlWiFi_DefaultGasConfig(phContext);
    }
    pMyObject->GASConfiguration = JSON_STR;
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS CosaDmlWiFi_GetGasStats(PANSC_HANDLE phContext)
{
    PCOSA_DML_WIFI_GASSTATS  pGASStats   = (PCOSA_DML_WIFI_GASSTATS)phContext;
    if(!pGASStats){
        wifi_passpoint_dbg_print(1,"Wifi GAS Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }

    memset(pGASStats,0,sizeof(COSA_DML_WIFI_GASSTATS));
    memcpy(pGASStats,&gasStats[GAS_CFG_TYPE_SUPPORTED - 1],sizeof(gasStats));
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS CosaDmlWiFi_SetANQPConfig(PCOSA_DML_WIFI_AP_CFG pCfg, char *JSON_STR)
{
    if(!pCfg){
        wifi_passpoint_dbg_print(1,"AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    int apIns = pCfg->InstanceNumber -1;
    if((apIns < 0) || (apIns > 15)){
        wifi_passpoint_dbg_print(1, "%s:%d: Invalid AP Index. Setting to 1\n", __func__, __LINE__);
        apIns = 0;
    }

    cJSON *mainEntry = NULL;
    cJSON *anqpElement = NULL;
    cJSON *anqpList = NULL;
    cJSON *anqpEntry = NULL;
    cJSON *anqpParam = NULL;
    cJSON *subList = NULL;
    cJSON *subEntry = NULL;
    cJSON *subParam = NULL;

    if(!JSON_STR){
        wifi_passpoint_dbg_print(1,"JSON String is NULL\n");
        return ANSC_STATUS_FAILURE;
    }

    cJSON *passPointCfg = cJSON_Parse(JSON_STR);
    cJSON *passPointStats = cJSON_CreateObject();//root object for Passpoint Stats
    cJSON *statsMainEntry = cJSON_AddObjectToObject(passPointStats,"PassPointStats");
    cJSON *statsList = cJSON_AddArrayToObject(statsMainEntry, "ANQPResponse");

    if (NULL == passPointCfg) {
        wifi_passpoint_dbg_print(1,"Failed to parse JSON\n");
        return ANSC_STATUS_FAILURE;
    }

    mainEntry = cJSON_GetObjectItem(passPointCfg,"InterworkingService");
    if(NULL == mainEntry){
        wifi_passpoint_dbg_print(1,"ANQP entry is NULL\n");
        cJSON_Delete(passPointCfg);
        return ANSC_STATUS_FAILURE;
    }
   
    pthread_mutex_lock(&g_data_plane_module.lock);//Take lock in case requests come during update.

    //CapabilityListANQPElement
    memset(&g_anqp_data[apIns].capabilityInfo, 0, sizeof(wifi_capabilityListANQP_t));
    g_anqp_data[apIns].capabilityInfoLength = 0;
    g_anqp_data[apIns].capabilityInfo.capabilityList[g_anqp_data[apIns].capabilityInfoLength++] = wifi_anqp_element_name_query_list;
    g_anqp_data[apIns].capabilityInfo.capabilityList[g_anqp_data[apIns].capabilityInfoLength++] = wifi_anqp_element_name_capability_list;
 
    UCHAR *next_pos = NULL;

    //VenueNameANQPElement
    if(g_anqp_data[apIns].venueInfo){
        free(g_anqp_data[apIns].venueInfo);
        g_anqp_data[apIns].venueInfo = NULL;
        g_anqp_data[apIns].venueInfoLength = 0;
    }
    anqpElement = cJSON_GetObjectItem(mainEntry,"VenueNameANQPElement");
    if(anqpElement){
        g_anqp_data[apIns].venueInfo = malloc(sizeof(wifi_venueNameElement_t));
        memset(g_anqp_data[apIns].venueInfo,0,sizeof(wifi_venueNameElement_t));
        next_pos = g_anqp_data[apIns].venueInfo;
        wifi_venueNameElement_t *venueElem = (wifi_venueNameElement_t *)next_pos;
        venueElem->venueGroup = pCfg->IEEE80211uCfg.IntwrkCfg.iVenueGroup;
        next_pos += sizeof(venueElem->venueGroup);
        venueElem->venueType = pCfg->IEEE80211uCfg.IntwrkCfg.iVenueType;
        next_pos += sizeof(venueElem->venueType);
        anqpList    = cJSON_GetObjectItem(anqpElement,"VenueInfo");
        if(anqpList){
            g_anqp_data[apIns].venueCount = cJSON_GetArraySize(anqpList);
            if(cJSON_GetArraySize(anqpList) > 16){
                wifi_passpoint_dbg_print(1, "%s:%d: Venue entries cannot be more than 16. Discarding Configuration\n", __func__, __LINE__);
                free(g_anqp_data[apIns].venueInfo);
                g_anqp_data[apIns].venueInfo = NULL;
                g_anqp_data[apIns].venueInfoLength = 0;
                cJSON_Delete(passPointCfg);
                pthread_mutex_unlock(&g_data_plane_module.lock);
                return ANSC_STATUS_FAILURE;
            }
            if(g_anqp_data[apIns].venueCount){
                cJSON_ArrayForEach(anqpEntry, anqpList){
                    wifi_venueName_t *venueBuf = (wifi_venueName_t *)next_pos;
                    next_pos += sizeof(venueBuf->length); //Will be filled at the end
                    anqpParam = cJSON_GetObjectItem(anqpEntry,"Language");
                    AnscCopyString(next_pos,anqpParam->valuestring);
                    next_pos += AnscSizeOfString(anqpParam->valuestring);
                    anqpParam = cJSON_GetObjectItem(anqpEntry,"Name");
                    if(AnscSizeOfString(anqpParam->valuestring) > 255){
                        wifi_passpoint_dbg_print(1, "%s:%d: Venue name cannot be more than 255. Discarding Configuration\n", __func__, __LINE__);
                        free(g_anqp_data[apIns].realmInfo);
                        g_anqp_data[apIns].venueInfo = NULL;
                        g_anqp_data[apIns].venueInfoLength = 0;
                        cJSON_Delete(passPointCfg);
                        pthread_mutex_unlock(&g_data_plane_module.lock);
                        return ANSC_STATUS_FAILURE;
                    }
                    AnscCopyString(next_pos,anqpParam->valuestring);
                    next_pos += AnscSizeOfString(anqpParam->valuestring);
                    venueBuf->length = next_pos - &venueBuf->language[0]; 
               }
            }
        }
        g_anqp_data[apIns].venueInfoLength = next_pos - g_anqp_data[apIns].venueInfo;
        g_anqp_data[apIns].capabilityInfo.capabilityList[g_anqp_data[apIns].capabilityInfoLength++] = wifi_anqp_element_name_venue_name;
        wifi_passpoint_dbg_print(1, "%s:%d: Copied VenueNameANQPElement Data. Length: %d\n", __func__, __LINE__,g_anqp_data[apIns].venueInfoLength);
    }

    //RoamingConsortiumANQPElement
    if(g_anqp_data[apIns].roamInfo){
        free(g_anqp_data[apIns].roamInfo);
        g_anqp_data[apIns].roamInfo = NULL;
        g_anqp_data[apIns].roamInfoLength = 0;
    }
    anqpElement = cJSON_GetObjectItem(mainEntry,"RoamingConsortiumANQPElement");
    memset(&pCfg->IEEE80211uCfg.RoamCfg, 0, sizeof(pCfg->IEEE80211uCfg.RoamCfg));
    if(anqpElement){
        g_anqp_data[apIns].roamInfo = malloc(sizeof(wifi_roamingConsortium_t));
        memset(g_anqp_data[apIns].roamInfo,0,sizeof(wifi_roamingConsortium_t));
        next_pos = g_anqp_data[apIns].roamInfo;
        anqpList = cJSON_GetObjectItem(anqpElement,"OI");
        int ouiCount = 0;

        if(cJSON_GetArraySize(anqpList) > 32){
            wifi_passpoint_dbg_print(1, "%s:%d: Only 32 OUI supported in RoamingConsortiumANQPElement Data. Discarding Configuration\n", __func__, __LINE__);
            free(g_anqp_data[apIns].roamInfo);
            g_anqp_data[apIns].roamInfo = NULL;
            g_anqp_data[apIns].roamInfoLength = 0;
            cJSON_Delete(passPointCfg);
            pthread_mutex_unlock(&g_data_plane_module.lock);
            return ANSC_STATUS_FAILURE;
        }

        cJSON_ArrayForEach(anqpEntry, anqpList){
            wifi_ouiDuple_t *ouiBuf = (wifi_ouiDuple_t *)next_pos;
            UCHAR ouiStr[30+1];
            int i, ouiStrLen;
            memset(ouiStr,0,sizeof(ouiStr));
            anqpParam = cJSON_GetObjectItem(anqpEntry,"OI");
            if(anqpParam){
                ouiStrLen = AnscSizeOfString(anqpParam->valuestring);
                if((ouiStrLen < 6) || (ouiStrLen > 30) || (ouiStrLen % 2)){
                    wifi_passpoint_dbg_print(1, "%s:%d: Invalid OUI Length in RoamingConsortiumANQPElement Data. Discarding Configuration\n", __func__, __LINE__);
                    free(g_anqp_data[apIns].roamInfo);
                    g_anqp_data[apIns].roamInfo = NULL;
                    g_anqp_data[apIns].roamInfoLength = 0;
                    cJSON_Delete(passPointCfg);
                    pthread_mutex_unlock(&g_data_plane_module.lock);
                    return ANSC_STATUS_FAILURE;
                }
                AnscCopyString(ouiStr,anqpParam->valuestring);
	    }
            //Covert the incoming string to HEX 
            for(i = 0; i < ouiStrLen; i++){
                if((ouiStr[i] >= '0') && (ouiStr[i] <= '9')){
                    ouiStr[i] -= '0';
                }else if((ouiStr[i] >= 'a') && (ouiStr[i] <= 'f')){
                    ouiStr[i] -= ('a' - 10);//a=10
                }else if((ouiStr[i] >= 'A') && (ouiStr[i] <= 'F')){
                    ouiStr[i] -= ('A' - 10);//A=10
                }else{
                    wifi_passpoint_dbg_print(1, "%s:%d: Invalid OUI in RoamingConsortiumANQPElement Data. Discarding Configuration\n", __func__, __LINE__);
                    free(g_anqp_data[apIns].roamInfo);
                    g_anqp_data[apIns].roamInfo = NULL;
                    g_anqp_data[apIns].roamInfoLength = 0;
                    cJSON_Delete(passPointCfg);
                    pthread_mutex_unlock(&g_data_plane_module.lock);
                    return ANSC_STATUS_FAILURE;
                }
                if(i%2){
                    ouiBuf->oui[(i/2)] = ouiStr[i] | (ouiStr[i-1] << 4);
                }
            }
            ouiBuf->length = i/2;
            next_pos += sizeof(ouiBuf->length);
            next_pos += ouiBuf->length;
            if(ouiCount < 3){
                memcpy(&pCfg->IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumOui[ouiCount][0],&ouiBuf->oui[0],ouiBuf->length);
                pCfg->IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumLen[ouiCount++] = ouiBuf->length;
            }
        }
       
        pCfg->IEEE80211uCfg.RoamCfg.iWIFIRoamingConsortiumCount = cJSON_GetArraySize(anqpList);
        //Push Interworkoing Element to HAL
        if(ANSC_STATUS_SUCCESS != CosaDmlWiFi_ApplyRoamingConsortiumElement(pCfg)){
            wifi_passpoint_dbg_print(1, "%s:%d: CosaDmlWiFi_ApplyRoamingConsortiumElement Failed.\n", __func__, __LINE__);
        } 
        g_anqp_data[apIns].roamInfoLength = next_pos - g_anqp_data[apIns].roamInfo;
        g_anqp_data[apIns].capabilityInfo.capabilityList[g_anqp_data[apIns].capabilityInfoLength++] = wifi_anqp_element_name_roaming_consortium;
        wifi_passpoint_dbg_print(1, "%s:%d: Copied RoamingConsortiumANQPElement Data. Length: %d\n", __func__, __LINE__,g_anqp_data[apIns].roamInfoLength);
    }

    //IPAddressTypeAvailabilityANQPElement
    if(g_anqp_data[apIns].ipAddressInfo){
        free(g_anqp_data[apIns].ipAddressInfo);
        g_anqp_data[apIns].ipAddressInfo = NULL;
    }
    anqpEntry = cJSON_GetObjectItem(mainEntry,"IPAddressTypeAvailabilityANQPElement");
    if(anqpEntry){
        g_anqp_data[apIns].ipAddressInfo = malloc(sizeof(wifi_ipAddressAvailabality_t));
        wifi_ipAddressAvailabality_t *ipInfoBuf = (wifi_ipAddressAvailabality_t *)g_anqp_data[apIns].ipAddressInfo;
        ipInfoBuf->field_format = 0;
        anqpParam = cJSON_GetObjectItem(anqpEntry,"IPv6AddressType");
        if(anqpParam){
            if((0 > anqpParam->valuedouble) || (2 < anqpParam->valuedouble)){
                wifi_passpoint_dbg_print(1, "%s:%d: Invalid IPAddressTypeAvailabilityANQPElement. Discarding Configuration\n", __func__, __LINE__);
                free(g_anqp_data[apIns].ipAddressInfo);
                g_anqp_data[apIns].ipAddressInfo = NULL;
                cJSON_Delete(passPointCfg);
                pthread_mutex_unlock(&g_data_plane_module.lock);
                return ANSC_STATUS_FAILURE;
            }
            ipInfoBuf->field_format = (UCHAR)anqpParam->valuedouble;
        }
        anqpParam = cJSON_GetObjectItem(anqpEntry,"IPv4AddressType");
        if(anqpParam){
            if((0 > anqpParam->valuedouble) || (7 < anqpParam->valuedouble)){
                wifi_passpoint_dbg_print(1, "%s:%d: Invalid IPAddressTypeAvailabilityANQPElement. Discarding Configuration\n", __func__, __LINE__);
                free(g_anqp_data[apIns].ipAddressInfo);
                g_anqp_data[apIns].ipAddressInfo = NULL;
                cJSON_Delete(passPointCfg);
                pthread_mutex_unlock(&g_data_plane_module.lock);
                return ANSC_STATUS_FAILURE;
            }
            UCHAR ipv4Field = (UCHAR)anqpParam->valuedouble;
            ipInfoBuf->field_format |= (ipv4Field << 2);
        }
        g_anqp_data[apIns].capabilityInfo.capabilityList[g_anqp_data[apIns].capabilityInfoLength++] = wifi_anqp_element_name_ip_address_availabality;
        wifi_passpoint_dbg_print(1, "%s:%d: Copied IPAddressTypeAvailabilityANQPElement Data. Length: %d. Data: %02X\n", __func__, __LINE__,sizeof(wifi_ipAddressAvailabality_t),ipInfoBuf->field_format);
    }
   
    //NAIRealmANQPElement
    if(g_anqp_data[apIns].realmInfo){
        free(g_anqp_data[apIns].realmInfo);
        g_anqp_data[apIns].realmInfo = NULL;
        g_anqp_data[apIns].realmInfoLength = 0;
    }
    anqpElement = cJSON_GetObjectItem(mainEntry,"NAIRealmANQPElement");
    if(anqpElement){
        anqpList    = cJSON_GetObjectItem(anqpElement,"Realm");
        if(anqpList){
            g_anqp_data[apIns].realmCount = cJSON_GetArraySize(anqpList);
            if(g_anqp_data[apIns].realmCount){
                if(g_anqp_data[apIns].realmCount > 20){
                    wifi_passpoint_dbg_print(1, "%s:%d: Only 20 Realm Entries are supported. Discarding Configuration\n", __func__, __LINE__);
                    cJSON_Delete(passPointCfg);
                    pthread_mutex_unlock(&g_data_plane_module.lock);
                    return ANSC_STATUS_FAILURE;
                }
                g_anqp_data[apIns].realmInfo = malloc(sizeof(wifi_naiRealmElement_t));
                memset(g_anqp_data[apIns].realmInfo,0,sizeof(wifi_naiRealmElement_t));
                next_pos = g_anqp_data[apIns].realmInfo;
                wifi_naiRealmElement_t *naiElem = (wifi_naiRealmElement_t *)next_pos;
                naiElem->nai_realm_count = g_anqp_data[apIns].realmCount;
                next_pos += sizeof(naiElem->nai_realm_count);
                cJSON_ArrayForEach(anqpEntry, anqpList){
                    wifi_naiRealm_t *realmInfoBuf = (wifi_naiRealm_t *)next_pos;
                    next_pos += sizeof(realmInfoBuf->data_field_length);
                    anqpParam = cJSON_GetObjectItem(anqpEntry,"RealmEncoding");
                    realmInfoBuf->encoding = anqpParam ? anqpParam->valuedouble : 0;
                    next_pos += sizeof(realmInfoBuf->encoding);
                    anqpParam = cJSON_GetObjectItem(anqpEntry,"Realms");
                    realmInfoBuf->realm_length = AnscSizeOfString(anqpParam->valuestring);
                    if(realmInfoBuf->realm_length > 255){
                        wifi_passpoint_dbg_print(1, "%s:%d: Realm Length cannot be more than 255. Discarding Configuration\n", __func__, __LINE__);
                        free(g_anqp_data[apIns].realmInfo);
                        g_anqp_data[apIns].realmInfo = NULL; 
                        g_anqp_data[apIns].realmInfoLength = 0;
                        cJSON_Delete(passPointCfg);
                        pthread_mutex_unlock(&g_data_plane_module.lock);
                        return ANSC_STATUS_FAILURE;
                    }
                    next_pos += sizeof(realmInfoBuf->realm_length);
                    AnscCopyString(next_pos,anqpParam->valuestring);
                    next_pos += realmInfoBuf->realm_length;

                    cJSON *realmStats = cJSON_CreateObject();//Create a stats Entry here for each Realm
                    cJSON_AddStringToObject(realmStats, "Name", anqpParam->valuestring);
                    cJSON_AddNumberToObject(realmStats, "EntryType", 1);//1-NAI Realm
                    cJSON_AddNumberToObject(realmStats, "Sent", 0);
                    cJSON_AddNumberToObject(realmStats, "Failed", 0);
                    cJSON_AddNumberToObject(realmStats, "Timeout", 0);
                    cJSON_AddItemToArray(statsList, realmStats);

                    subList = cJSON_GetObjectItem(anqpEntry,"EAP");
                    if(cJSON_GetArraySize(subList) > 16){
                        wifi_passpoint_dbg_print(1, "%s:%d: EAP entries cannot be more than 16. Discarding Configuration\n", __func__, __LINE__);
                        free(g_anqp_data[apIns].realmInfo);
                        g_anqp_data[apIns].realmInfo = NULL;
                        g_anqp_data[apIns].realmInfoLength = 0;
                        cJSON_Delete(passPointCfg);
                        pthread_mutex_unlock(&g_data_plane_module.lock);
                        return ANSC_STATUS_FAILURE;
                    }
                    *next_pos = cJSON_GetArraySize(subList);//eap_method_count
                    next_pos += sizeof(realmInfoBuf->eap_method_count);
                    cJSON_ArrayForEach(subEntry, subList){
                        wifi_eapMethod_t *eapBuf = (wifi_eapMethod_t *)next_pos;
                        next_pos += sizeof(eapBuf->length);
                        subParam = cJSON_GetObjectItem(subEntry,"Method");
                        eapBuf->method = subParam ? subParam->valuedouble : 0; 
                        next_pos += sizeof(eapBuf->method);
                        cJSON *subList_1  = NULL;
                        cJSON *subEntry_1 = NULL;
                        cJSON *subParam_1 = NULL;
                        subList_1 = cJSON_GetObjectItem(subEntry,"AuthenticationParameter");
                        eapBuf->auth_param_count = cJSON_GetArraySize(subList_1);
                        if(eapBuf->auth_param_count > 16){
                            wifi_passpoint_dbg_print(1, "%s:%d: Auth entries cannot be more than 16. Discarding Configuration\n", __func__, __LINE__);
                            free(g_anqp_data[apIns].realmInfo);
                            g_anqp_data[apIns].realmInfo = NULL;
                            g_anqp_data[apIns].realmInfoLength = 0;
                            cJSON_Delete(passPointCfg);
                            pthread_mutex_unlock(&g_data_plane_module.lock);
                            return ANSC_STATUS_FAILURE;
                        }
                        next_pos += sizeof(eapBuf->auth_param_count);
                        cJSON_ArrayForEach(subEntry_1, subList_1){
                            int i,authStrLen;
                            UCHAR authStr[14+1];
                            wifi_authMethod_t *authBuf = (wifi_authMethod_t *)next_pos;
                            subParam_1 = cJSON_GetObjectItem(subEntry_1,"ID");
                            authBuf->id = subParam_1 ? subParam_1->valuedouble:0;
                            next_pos += sizeof(authBuf->id);
                            subParam_1 = cJSON_GetObjectItem(subEntry_1,"Value");

                            if(!subParam_1){
                                wifi_passpoint_dbg_print(1, "%s:%d: Auth Parameter Value not prensent in NAIRealmANQPElement EAP Data. Discarding Configuration\n", __func__, __LINE__);
                                free(g_anqp_data[apIns].realmInfo);
                                g_anqp_data[apIns].realmInfo = NULL;
                                g_anqp_data[apIns].realmInfoLength = 0;
                                cJSON_Delete(passPointCfg);
                                pthread_mutex_unlock(&g_data_plane_module.lock);
                                return ANSC_STATUS_FAILURE;
                            } else if (subParam_1->valuedouble) {
                                authBuf->length = 1;
                                authBuf->val[0] = subParam_1->valuedouble;
                            } else {
                                authStrLen = AnscSizeOfString(subParam_1->valuestring);
                                if((authStrLen != 2) && (authStrLen != 14)){
                                    wifi_passpoint_dbg_print(1, "%s:%d: Invalid EAP Value Length in NAIRealmANQPElement Data. Has to be 1 to 7 bytes Long. Discarding Configuration\n", __func__, __LINE__);
                                    free(g_anqp_data[apIns].realmInfo);
                                    g_anqp_data[apIns].realmInfo = NULL;
                                    g_anqp_data[apIns].realmInfoLength = 0;
                                    cJSON_Delete(passPointCfg);
                                    pthread_mutex_unlock(&g_data_plane_module.lock);
                                    return ANSC_STATUS_FAILURE;
                                }

                                AnscCopyString(authStr,subParam_1->valuestring);

                                //Covert the incoming string to HEX
                                for(i = 0; i < authStrLen; i++){ 
                                    if((authStr[i] >= '0') && (authStr[i] <= '9')){
                                        authStr[i] -= '0'; 
                                    }else if((authStr[i] >= 'a') && (authStr[i] <= 'f')){
                                        authStr[i] -= ('a' - 10);//a=10
                                    }else if((authStr[i] >= 'A') && (authStr[i] <= 'F')){
                                        authStr[i] -= ('A' - 10);//A=10
                                    }else{
                                        wifi_passpoint_dbg_print(1, "%s:%d: Invalid EAP val in NAIRealmANQPElement Data. Discarding Configuration\n", __func__, __LINE__);
                                        free(g_anqp_data[apIns].realmInfo);
                                        g_anqp_data[apIns].realmInfo = NULL;
                                        g_anqp_data[apIns].realmInfoLength = 0;
                                        cJSON_Delete(passPointCfg);
                                        pthread_mutex_unlock(&g_data_plane_module.lock);
                                        return ANSC_STATUS_FAILURE;
                                    }
                                    if(i%2){
                                        authBuf->val[(i/2)] = authStr[i] | (authStr[i-1] << 4);
                                    }
                                }
                                authBuf->length = i/2;
                            }
                            next_pos += sizeof(authBuf->length);
                            next_pos += authBuf->length;
                        }
                        eapBuf->length = next_pos - &eapBuf->method;
                    }
                    realmInfoBuf->data_field_length = next_pos - &realmInfoBuf->encoding;
                }
                g_anqp_data[apIns].realmInfoLength = next_pos - g_anqp_data[apIns].realmInfo;
            }
        }
        g_anqp_data[apIns].capabilityInfo.capabilityList[g_anqp_data[apIns].capabilityInfoLength++] = wifi_anqp_element_name_nai_realm;
        wifi_passpoint_dbg_print(1, "%s:%d: Copied NAIRealmANQPElement Data. Length: %d\n", __func__, __LINE__,g_anqp_data[apIns].realmInfoLength);
    }

    //3GPPCellularANQPElement
    if(g_anqp_data[apIns].gppInfo){
        free(g_anqp_data[apIns].gppInfo);
        g_anqp_data[apIns].gppInfo = NULL;
        g_anqp_data[apIns].gppInfoLength = 0;
    }
    anqpElement = cJSON_GetObjectItem(mainEntry,"3GPPCellularANQPElement");
    if(anqpElement){
        g_anqp_data[apIns].gppInfo = malloc(sizeof(wifi_3gppCellularNetwork_t));
        memset(g_anqp_data[apIns].gppInfo,0,sizeof(wifi_3gppCellularNetwork_t));
        next_pos = g_anqp_data[apIns].gppInfo;
        wifi_3gppCellularNetwork_t *gppBuf = (wifi_3gppCellularNetwork_t *)next_pos;
        anqpParam = cJSON_GetObjectItem(anqpElement,"GUD");
        gppBuf->gud = anqpParam ? anqpParam->valuedouble:0;
        next_pos += sizeof(gppBuf->gud);
        next_pos += sizeof(gppBuf->uhdLength);//Skip over UHD length to be filled at the end
        UCHAR *uhd_pos = next_pos;//Beginning of UHD data
    
        wifi_3gpp_plmn_list_information_element_t *plmnInfoBuf = (wifi_3gpp_plmn_list_information_element_t *)next_pos;
        plmnInfoBuf->iei = 0;
        next_pos += sizeof(plmnInfoBuf->iei);
        next_pos += sizeof(plmnInfoBuf->plmn_length);//skip through the length field that will be filled at the end
        UCHAR *plmn_pos = next_pos;//beginnig of PLMN data
        
        anqpList    = cJSON_GetObjectItem(anqpElement,"PLMN");
        plmnInfoBuf->number_of_plmns = cJSON_GetArraySize(anqpList);
        next_pos += sizeof(plmnInfoBuf->number_of_plmns);

        if(plmnInfoBuf->number_of_plmns > 16){
            wifi_passpoint_dbg_print(1, "%s:%d: 3GPP entries cannot be more than 16. Discarding Configuration\n", __func__, __LINE__);
            free(g_anqp_data[apIns].gppInfo);
            g_anqp_data[apIns].gppInfo = NULL;
            g_anqp_data[apIns].gppInfoLength = 0;
            cJSON_Delete(passPointCfg);
            pthread_mutex_unlock(&g_data_plane_module.lock);
            return ANSC_STATUS_FAILURE;
        }

        cJSON_ArrayForEach(anqpEntry, anqpList){
            UCHAR mccStr[3+1];
            UCHAR mncStr[3+1];
            memset(mccStr,0,sizeof(mccStr));
            memset(mncStr,0,sizeof(mncStr));
             
            anqpParam = cJSON_GetObjectItem(anqpEntry,"MCC");
            if(anqpParam && anqpParam->valuestring && (AnscSizeOfString(anqpParam->valuestring) == (sizeof(mccStr) -1))){
                AnscCopyString(mccStr,anqpParam->valuestring);
            }else if(anqpParam && anqpParam->valuestring && (AnscSizeOfString(anqpParam->valuestring) == (sizeof(mccStr) -2))){
                mccStr[0] = '0';
                AnscCopyString(&mccStr[1],anqpParam->valuestring);
            }else{
                wifi_passpoint_dbg_print(1, "%s:%d: Invalid MCC in 3GPPCellularANQPElement Data. Discarding Configuration\n", __func__, __LINE__);
                free(g_anqp_data[apIns].gppInfo);
                g_anqp_data[apIns].gppInfo = NULL;
                g_anqp_data[apIns].gppInfoLength = 0;
                cJSON_Delete(passPointCfg);
                pthread_mutex_unlock(&g_data_plane_module.lock);
                return ANSC_STATUS_FAILURE;
            }
            anqpParam = cJSON_GetObjectItem(anqpEntry,"MNC");
            if(anqpParam && anqpParam->valuestring && (AnscSizeOfString(anqpParam->valuestring) == (sizeof(mccStr) -1))){
                AnscCopyString(mncStr,anqpParam->valuestring);
            }else if(anqpParam && anqpParam->valuestring && (AnscSizeOfString(anqpParam->valuestring) ==  (sizeof(mccStr) -2))){
                mncStr[0] = '0';
                AnscCopyString(&mncStr[1],anqpParam->valuestring);
            }else{
                wifi_passpoint_dbg_print(1, "%s:%d: Invalid MNC in 3GPPCellularANQPElement Data. Discarding Configuration\n", __func__, __LINE__);
                free(g_anqp_data[apIns].gppInfo);
                g_anqp_data[apIns].gppInfo = NULL;
                g_anqp_data[apIns].gppInfoLength = 0;
                cJSON_Delete(passPointCfg);
                pthread_mutex_unlock(&g_data_plane_module.lock);
                return ANSC_STATUS_FAILURE;
            }
            wifi_plmn_t *plmnBuf = (wifi_plmn_t *)next_pos;
            plmnBuf->PLMN[0] = (UCHAR)((mccStr[0] - '0') | ((mccStr[1] - '0') << 4));
            plmnBuf->PLMN[1] = (UCHAR)((mccStr[2] - '0') | ((mncStr[2] - '0') << 4));
            plmnBuf->PLMN[2] = (UCHAR)((mncStr[0] - '0') | ((mncStr[1] - '0') << 4));
            next_pos += sizeof(wifi_plmn_t);

            char  nameStr[8];
            snprintf(nameStr, sizeof(nameStr), "%s:%s", mccStr, mncStr);
            cJSON *realmStats = cJSON_CreateObject();//Create a stats Entry here for each Realm
            cJSON_AddStringToObject(realmStats, "Name", nameStr);
            cJSON_AddNumberToObject(realmStats, "EntryType", 3);//3-3GPP
            cJSON_AddNumberToObject(realmStats, "Sent", 0);
            cJSON_AddNumberToObject(realmStats, "Failed", 0);
            cJSON_AddNumberToObject(realmStats, "Timeout", 0);
            cJSON_AddItemToArray(statsList, realmStats);
        }
        gppBuf->uhdLength = next_pos - uhd_pos;
        plmnInfoBuf->plmn_length = next_pos - plmn_pos;
        g_anqp_data[apIns].gppInfoLength = next_pos - g_anqp_data[apIns].gppInfo;
        g_anqp_data[apIns].capabilityInfo.capabilityList[g_anqp_data[apIns].capabilityInfoLength++] = wifi_anqp_element_name_3gpp_cellular_network;
        wifi_passpoint_dbg_print(1, "%s:%d: Copied 3GPPCellularANQPElement Data. Length: %d\n", __func__, __LINE__,g_anqp_data[apIns].gppInfoLength);
    }

    //DomainANQPElement
    if(g_anqp_data[apIns].domainNameInfo){
        free(g_anqp_data[apIns].domainNameInfo);
        g_anqp_data[apIns].domainNameInfo = NULL;
        g_anqp_data[apIns].domainInfoLength = 0;
    }
    anqpElement = cJSON_GetObjectItem(mainEntry,"DomainANQPElement");
    if(anqpElement){
        g_anqp_data[apIns].domainNameInfo = malloc(sizeof(wifi_domainName_t));
        memset(g_anqp_data[apIns].domainNameInfo,0,sizeof(wifi_domainName_t));
        next_pos = g_anqp_data[apIns].domainNameInfo;
        wifi_domainName_t *domainBuf = (wifi_domainName_t *)next_pos;
        anqpList = cJSON_GetObjectItem(anqpElement,"DomainName");

        if(cJSON_GetArraySize(anqpList) > 4){
            wifi_passpoint_dbg_print(1, "%s:%d: Only 4 Entries supported in DomainNameANQPElement Data. Discarding Configuration\n", __func__, __LINE__);
            free(g_anqp_data[apIns].domainNameInfo);
            g_anqp_data[apIns].domainNameInfo = NULL;
            g_anqp_data[apIns].domainInfoLength = 0;
        }

        cJSON_ArrayForEach(anqpEntry, anqpList){
            wifi_domainNameTuple_t *nameBuf = (wifi_domainNameTuple_t *)next_pos;
            anqpParam = cJSON_GetObjectItem(anqpEntry,"Name");
            if(AnscSizeOfString(anqpParam->valuestring) > 255){
                wifi_passpoint_dbg_print(1, "%s:%d: Domain name length cannot be more than 255. Discarding Configuration\n", __func__, __LINE__);
                free(g_anqp_data[apIns].domainNameInfo);
                g_anqp_data[apIns].domainNameInfo = NULL;
                g_anqp_data[apIns].domainInfoLength = 0;
                cJSON_Delete(passPointCfg);
                pthread_mutex_unlock(&g_data_plane_module.lock);
                return ANSC_STATUS_FAILURE;
            }
            nameBuf->length = AnscSizeOfString(anqpParam->valuestring);
            next_pos += sizeof(nameBuf->length);
            AnscCopyString(next_pos,anqpParam->valuestring);
            next_pos += nameBuf->length;

            cJSON *realmStats = cJSON_CreateObject();//Create a stats Entry here for each Realm
            cJSON_AddStringToObject(realmStats, "Name", anqpParam->valuestring);
            cJSON_AddNumberToObject(realmStats, "EntryType", 2);//2-Domain
            cJSON_AddNumberToObject(realmStats, "Sent", 0);
            cJSON_AddNumberToObject(realmStats, "Failed", 0);
            cJSON_AddNumberToObject(realmStats, "Timeout", 0);
            cJSON_AddItemToArray(statsList, realmStats);
        }
        g_anqp_data[apIns].domainInfoLength = next_pos - g_anqp_data[apIns].domainNameInfo;
        g_anqp_data[apIns].capabilityInfo.capabilityList[g_anqp_data[apIns].capabilityInfoLength++] = wifi_anqp_element_name_domain_name;
        wifi_passpoint_dbg_print(1, "%s:%d: Copied DomainANQPElement Data. Length: %d\n", __func__, __LINE__,g_anqp_data[apIns].domainInfoLength);
    }

    pthread_mutex_unlock(&g_data_plane_module.lock);
    cJSON_Delete(passPointCfg);

    //Reset the response Counters
    g_hs2_data[apIns].realmRespCount = 0;
    g_hs2_data[apIns].realmFailedCount = 0;
    g_hs2_data[apIns].domainRespCount = 0;
    g_hs2_data[apIns].domainFailedCount = 0;
    g_hs2_data[apIns].gppRespCount = 0;
    g_hs2_data[apIns].gppFailedCount = 0;

    //Update the stats JSON
    cJSON_PrintPreallocated(passPointStats,(char *)&g_hs2_data[apIns].passpointStats, sizeof(g_hs2_data[apIns].passpointStats), false);
    cJSON_Delete(passPointStats);

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS CosaDmlWiFi_DefaultANQPConfig(PCOSA_DML_WIFI_AP_CFG pCfg)
{
    if(!pCfg){
        wifi_passpoint_dbg_print(1,"AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    char *JSON_STR = malloc(strlen(WIFI_PASSPOINT_DEFAULT_ANQP_CFG)+1);
    /*CID: 132395 Dereference before null check*/
    if(JSON_STR == NULL) {
       wifi_passpoint_dbg_print(1,"malloc failure\n");
        return ANSC_STATUS_FAILURE;
    }
    memset(JSON_STR,0,(strlen(WIFI_PASSPOINT_DEFAULT_ANQP_CFG)+1));
    AnscCopyString(JSON_STR, WIFI_PASSPOINT_DEFAULT_ANQP_CFG);

    if(!JSON_STR || (ANSC_STATUS_SUCCESS != CosaDmlWiFi_SetANQPConfig(pCfg,JSON_STR))){
        if(JSON_STR){
            free(JSON_STR);
            JSON_STR = NULL;
        }
        wifi_passpoint_dbg_print(1,"Failed to update default ANQP Config.\n");
        return ANSC_STATUS_FAILURE;
    }
    pCfg->IEEE80211uCfg.PasspointCfg.ANQPConfigParameters = JSON_STR;
    return ANSC_STATUS_SUCCESS; 
}

ANSC_STATUS CosaDmlWiFi_SaveANQPCfg(PCOSA_DML_WIFI_AP_CFG pCfg, char *buffer, int len)
{
    char cfgFile[64];
    DIR     *passPointDir = NULL;
    int apIns = 0;

    passPointDir = opendir(WIFI_PASSPOINT_DIR);
    if(passPointDir){
        closedir(passPointDir);
    }else if(ENOENT == errno){
        if(0 != mkdir(WIFI_PASSPOINT_DIR, 0777)){
            wifi_passpoint_dbg_print(1,"Failed to Create Passpoint Configuration directory.\n");
            return ANSC_STATUS_FAILURE;
        }
    }else{
        wifi_passpoint_dbg_print(1,"Error opening Passpoint Configuration directory.\n");
        return ANSC_STATUS_FAILURE;
    } 
 
    if(!pCfg){
        wifi_passpoint_dbg_print(1,"AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    apIns = pCfg->InstanceNumber;
    sprintf(cfgFile,"%s.%d",WIFI_PASSPOINT_ANQP_CFG_FILE,apIns);
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

    if(!pCfg){
        wifi_passpoint_dbg_print(1,"AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    pCfg->IEEE80211uCfg.PasspointCfg.ANQPConfigParameters = NULL;
    apIns = pCfg->InstanceNumber;
    if((apIns < 1) || (apIns > 16)){
        wifi_passpoint_dbg_print(1, "%s:%d: Invalid AP Index. Return\n", __func__, __LINE__);
        return ANSC_STATUS_FAILURE;
    }

    sprintf(cfgFile,"%s.%d",WIFI_PASSPOINT_ANQP_CFG_FILE,apIns);
   
    confSize = readFileToBuffer(cfgFile,&JSON_STR);

    //Initialize global buffer
    /*TODO RDKB-34680 CID:143567,143565,140458,140472,140466,140461,140459,140476,140470,140475,140462 Data race condition */
    g_anqp_data[apIns-1].venueCount = 0;
    g_anqp_data[apIns-1].venueInfoLength = 0;
    g_anqp_data[apIns-1].venueInfo = NULL;
    g_anqp_data[apIns-1].ipAddressInfo = NULL;
    g_anqp_data[apIns-1].realmCount = 0;
    g_anqp_data[apIns-1].realmInfoLength = 0;
    g_anqp_data[apIns-1].realmInfo = NULL;
    g_anqp_data[apIns-1].gppInfoLength = 0;
    g_anqp_data[apIns-1].gppInfo = NULL;
    g_anqp_data[apIns-1].roamInfoLength = 0;
    g_anqp_data[apIns-1].roamInfo = NULL;
    g_anqp_data[apIns-1].domainInfoLength = 0;
    g_anqp_data[apIns-1].domainNameInfo = NULL;

    if(!confSize || !JSON_STR || (ANSC_STATUS_SUCCESS != CosaDmlWiFi_SetANQPConfig(pCfg,JSON_STR))){
        if(JSON_STR){
            free(JSON_STR);
            JSON_STR = NULL;
        }
        wifi_passpoint_dbg_print(1,"Failed to Initialize ANQP Configuration from memory for AP: %d. Setting Default\n",apIns);
        return CosaDmlWiFi_DefaultANQPConfig(pCfg);
    }
    wifi_passpoint_dbg_print(1,"Initialized ANQP Configuration from memory for AP: %d.\n",apIns);
    pCfg->IEEE80211uCfg.PasspointCfg.ANQPConfigParameters = JSON_STR;
    return ANSC_STATUS_SUCCESS;
}

void CosaDmlWiFi_UpdateANQPVenueInfo(PCOSA_DML_WIFI_AP_CFG pCfg)
{
    int apIns = pCfg->InstanceNumber - 1;

    if((apIns < 0) || (apIns > 15)){
        wifi_passpoint_dbg_print(1, "%s:%d: Invalid AP Index. Return\n", __func__, __LINE__);
        return;
    }

    if(g_anqp_data[apIns].venueInfoLength && g_anqp_data[apIns].venueInfo){
        //Copy Venue Group and Type from Interworking Structure
        wifi_venueNameElement_t *venueElem = (wifi_venueNameElement_t *)g_anqp_data[apIns].venueInfo;
        venueElem->venueGroup = pCfg->IEEE80211uCfg.IntwrkCfg.iVenueGroup;
        venueElem->venueType = pCfg->IEEE80211uCfg.IntwrkCfg.iVenueType;
        wifi_passpoint_dbg_print(1, "%s:%d: Updated VenueNameANQPElement from Interworking\n", __func__, __LINE__);

    }
}

ANSC_STATUS CosaDmlWiFi_SetHS2Config(PCOSA_DML_WIFI_AP_CFG pCfg, char *JSON_STR)
{
    if(!pCfg){
        wifi_passpoint_dbg_print(1,"AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    int apIns = pCfg->InstanceNumber -1;
    if((apIns < 0) || (apIns > 15)){
        wifi_passpoint_dbg_print(1, "%s:%d: Invalid AP Index. Setting to 1\n", __func__, __LINE__);
        apIns = 0;
    }
    
    cJSON *mainEntry = NULL;
    cJSON *anqpElement = NULL;
    cJSON *anqpList = NULL;
    cJSON *anqpEntry = NULL;
    cJSON *anqpParam = NULL;
    cJSON *subList = NULL;
    cJSON *subEntry = NULL;
    cJSON *subParam = NULL;
    UCHAR *next_pos = NULL;
    
    if(!JSON_STR){
        wifi_passpoint_dbg_print(1,"JSON String is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    
    cJSON *passPointCfg = cJSON_Parse(JSON_STR);
    
    if (NULL == passPointCfg) {
        wifi_passpoint_dbg_print(1,"Failed to parse JSON\n");
        return ANSC_STATUS_FAILURE;
    }
    
    mainEntry = cJSON_GetObjectItem(passPointCfg,"Passpoint");
    if(NULL == mainEntry){
        wifi_passpoint_dbg_print(1,"Passpoint entry is NULL\n");
        cJSON_Delete(passPointCfg);
        return ANSC_STATUS_FAILURE;
    }
   
    pthread_mutex_lock(&g_data_plane_module.lock);//Take lock in case requests come during update.

    //groupAddressedForwardingDisable
    anqpParam = cJSON_GetObjectItem(mainEntry,"groupAddressedForwardingDisable");
    if(anqpParam){
        BOOL prevVal = g_hs2_data[apIns].gafDisable;//store current value to check whether it has changed.
      
        g_hs2_data[apIns].gafDisable = ((anqpParam->type & cJSON_True) !=0) ? true:false;
#if defined (FEATURE_SUPPORT_PASSPOINT)
        if((g_hs2_data[apIns].hs2Status) && (g_hs2_data[apIns].gafDisable != prevVal)){ //HS is enabled and DGAF value has changed. Update IE value in HAL
            BOOL l2tif = false;
            if((pCfg->IEEE80211uCfg.IntwrkCfg.iAccessNetworkType == 2) || (pCfg->IEEE80211uCfg.IntwrkCfg.iAccessNetworkType == 3)){
                l2tif = true;
            }
            enablePassPointSettings (apIns, g_hs2_data[apIns].hs2Status, g_hs2_data[apIns].gafDisable,g_hs2_data[apIns].p2pDisable,l2tif);
        }
#endif
    }else if(!g_hs2_data[apIns].gafDisable){ //Restore Default Value
       g_hs2_data[apIns].gafDisable = true;
#if defined (FEATURE_SUPPORT_PASSPOINT)
       if(g_hs2_data[apIns].hs2Status){ //HS is enabled and P2P value has changed. Update IE value in HAL
            BOOL l2tif = false;
            if((pCfg->IEEE80211uCfg.IntwrkCfg.iAccessNetworkType == 2) || (pCfg->IEEE80211uCfg.IntwrkCfg.iAccessNetworkType == 3)){
                l2tif = true;
            }
            enablePassPointSettings (apIns, g_hs2_data[apIns].hs2Status, g_hs2_data[apIns].gafDisable,g_hs2_data[apIns].p2pDisable,l2tif);
        }
#endif
    }

    //p2pCrossConnectionDisable
    anqpParam = cJSON_GetObjectItem(mainEntry,"p2pCrossConnectionDisable");
    if(anqpParam){
        BOOL prevVal = g_hs2_data[apIns].p2pDisable;//store current value to check whether it has changed.
      
        g_hs2_data[apIns].p2pDisable = ((anqpParam->type & cJSON_True) !=0) ? true:false;
#if defined (FEATURE_SUPPORT_PASSPOINT)        
        if((g_hs2_data[apIns].hs2Status) && (g_hs2_data[apIns].p2pDisable != prevVal)){ //HS is enabled and P2P value has changed. Update IE value in HAL
            BOOL l2tif = false;
            if((pCfg->IEEE80211uCfg.IntwrkCfg.iAccessNetworkType == 2) || (pCfg->IEEE80211uCfg.IntwrkCfg.iAccessNetworkType == 3)){
                l2tif = true;
            }
            enablePassPointSettings (apIns, g_hs2_data[apIns].hs2Status, g_hs2_data[apIns].gafDisable,g_hs2_data[apIns].p2pDisable,l2tif);
        }
#endif
    }else if(g_hs2_data[apIns].p2pDisable){ //Restore Default Value
       g_hs2_data[apIns].p2pDisable = false;
#if defined (FEATURE_SUPPORT_PASSPOINT)       
       if(g_hs2_data[apIns].hs2Status){ //HS is enabled and P2P value has changed. Update IE value in HAL
            BOOL l2tif = false;
            if((pCfg->IEEE80211uCfg.IntwrkCfg.iAccessNetworkType == 2) || (pCfg->IEEE80211uCfg.IntwrkCfg.iAccessNetworkType == 3)){
                l2tif = true;
            }
            enablePassPointSettings (apIns, g_hs2_data[apIns].hs2Status, g_hs2_data[apIns].gafDisable,g_hs2_data[apIns].p2pDisable,l2tif);
        }
#endif
    }

    //CapabilityListANQPElement
    memset(&g_hs2_data[apIns].capabilityInfo, 0, sizeof(wifi_HS2_CapabilityList_t));
    g_hs2_data[apIns].capabilityInfoLength = 0;
    g_hs2_data[apIns].capabilityInfo.capabilityList[g_hs2_data[apIns].capabilityInfoLength++] = wifi_anqp_element_hs_subtype_hs_query_list;
    g_hs2_data[apIns].capabilityInfo.capabilityList[g_hs2_data[apIns].capabilityInfoLength++] = wifi_anqp_element_hs_subtype_hs_capability_list;
 
    //OperatorFriendlyNameANQPElement
    if(g_hs2_data[apIns].opFriendlyNameInfo){
        free(g_hs2_data[apIns].opFriendlyNameInfo);
        g_hs2_data[apIns].opFriendlyNameInfo = NULL;
        g_hs2_data[apIns].opFriendlyNameInfoLength = 0;
    }
    anqpElement = cJSON_GetObjectItem(mainEntry,"OperatorFriendlyNameANQPElement");

    if(anqpElement){
        anqpList    = cJSON_GetObjectItem(anqpElement,"Name");
        if(anqpList){
            if(cJSON_GetArraySize(anqpList) > 16){
                wifi_passpoint_dbg_print(1, "%s:%d: Invalid OperatorFriendlyName Length > 252. Discarding Configuration\n", __func__, __LINE__);
                pthread_mutex_unlock(&g_data_plane_module.lock);
                cJSON_Delete(passPointCfg);
                return ANSC_STATUS_FAILURE;
            }
            g_hs2_data[apIns].opFriendlyNameInfo = malloc(sizeof(wifi_HS2_OperatorFriendlyName_t));
            memset(g_hs2_data[apIns].opFriendlyNameInfo, 0, sizeof(wifi_HS2_OperatorFriendlyName_t));
            next_pos = g_hs2_data[apIns].opFriendlyNameInfo;
            cJSON_ArrayForEach(anqpEntry, anqpList){
                wifi_HS2_OperatorNameDuple_t *opNameBuf = (wifi_HS2_OperatorNameDuple_t *)next_pos;
                next_pos += sizeof(opNameBuf->length);//Fill length after reading the remaining fields
                anqpParam = cJSON_GetObjectItem(anqpEntry,"LanguageCode");
                if(!anqpParam || (AnscSizeOfString(anqpParam->valuestring) > 3)){
                    wifi_passpoint_dbg_print(1, "%s:%d: Invalid Language Code. Discarding Configuration\n", __func__, __LINE__);
                    free(g_hs2_data[apIns].opFriendlyNameInfo);
                    g_hs2_data[apIns].opFriendlyNameInfo = NULL;
                    g_hs2_data[apIns].opFriendlyNameInfoLength = 0;
                    pthread_mutex_unlock(&g_data_plane_module.lock);
                    cJSON_Delete(passPointCfg);
                    return ANSC_STATUS_FAILURE;
                }
                AnscCopyString(next_pos,anqpParam->valuestring);
                next_pos += sizeof(opNameBuf->languageCode);
                anqpParam = cJSON_GetObjectItem(anqpEntry,"OperatorName");
                if(!anqpParam || (AnscSizeOfString(anqpParam->valuestring) > 252)){
                    wifi_passpoint_dbg_print(1, "%s:%d: Invalid OperatorFriendlyName. Discarding Configuration\n", __func__, __LINE__);
                    free(g_hs2_data[apIns].opFriendlyNameInfo);
                    g_hs2_data[apIns].opFriendlyNameInfo = NULL;
                    g_hs2_data[apIns].opFriendlyNameInfoLength = 0;
                    pthread_mutex_unlock(&g_data_plane_module.lock);
                    cJSON_Delete(passPointCfg);
                    return ANSC_STATUS_FAILURE;
                }
                AnscCopyString(next_pos,anqpParam->valuestring);
                next_pos += AnscSizeOfString(anqpParam->valuestring);
                opNameBuf->length = AnscSizeOfString(anqpParam->valuestring) +  sizeof(opNameBuf->languageCode);
            }
        }
        g_hs2_data[apIns].opFriendlyNameInfoLength = next_pos - g_hs2_data[apIns].opFriendlyNameInfo;
        wifi_passpoint_dbg_print(1, "%s:%d: Copied OperatorFriendlyNameANQPElement Data. Length: %d\n", __func__, __LINE__,g_hs2_data[apIns].opFriendlyNameInfoLength);
        g_hs2_data[apIns].capabilityInfo.capabilityList[g_hs2_data[apIns].capabilityInfoLength++] = wifi_anqp_element_hs_subtype_operator_friendly_name;
    }
      
    //WANMetricsANQPElement
    memset(&g_hs2_data[apIns].wanMetricsInfo, 0, sizeof(wifi_HS2_WANMetrics_t));
    //wifi_getHS2WanMetrics(&g_hs2_data[apIns].wanMetricsInfo);
    g_hs2_data[apIns].wanMetricsInfo.wanInfo = 0b00000001;
    g_hs2_data[apIns].wanMetricsInfo.downLinkSpeed = 25000;
    g_hs2_data[apIns].wanMetricsInfo.upLinkSpeed = 5000;
    g_hs2_data[apIns].wanMetricsInfo.downLinkLoad = 0;
    g_hs2_data[apIns].wanMetricsInfo.upLinkLoad = 0;
    g_hs2_data[apIns].wanMetricsInfo.lmd = 0;
    g_hs2_data[apIns].capabilityInfo.capabilityList[g_hs2_data[apIns].capabilityInfoLength++] = wifi_anqp_element_hs_subtype_wan_metrics;

    //ConnectionCapabilityListANQPElement
    if(g_hs2_data[apIns].connCapabilityInfo){
        free(g_hs2_data[apIns].connCapabilityInfo);
        g_hs2_data[apIns].connCapabilityInfo = NULL;
        g_hs2_data[apIns].connCapabilityLength = 0;
    }
    anqpElement = cJSON_GetObjectItem(mainEntry,"ConnectionCapabilityListANQPElement");
    if(anqpElement){
        g_hs2_data[apIns].connCapabilityInfo = malloc(sizeof(wifi_HS2_ConnectionCapability_t));
        memset(g_hs2_data[apIns].connCapabilityInfo,0,sizeof(wifi_HS2_ConnectionCapability_t));
        next_pos = g_hs2_data[apIns].connCapabilityInfo;
        anqpList    = cJSON_GetObjectItem(anqpElement,"ProtoPort");
        if(anqpList){
            if(cJSON_GetArraySize(anqpList) > 16){
                wifi_passpoint_dbg_print(1, "%s:%d: Connection Capability count cannot be more than 16. Discarding Configuration\n", __func__, __LINE__);
                free(g_hs2_data[apIns].connCapabilityInfo);
                g_hs2_data[apIns].connCapabilityInfo = NULL;
                g_hs2_data[apIns].connCapabilityLength = 0;
                pthread_mutex_unlock(&g_data_plane_module.lock);
                cJSON_Delete(passPointCfg);
                return ANSC_STATUS_FAILURE;
            }
            cJSON_ArrayForEach(anqpEntry, anqpList){
                wifi_HS2_Proto_Port_Tuple_t *connCapBuf = (wifi_HS2_Proto_Port_Tuple_t *)next_pos;
                anqpParam = cJSON_GetObjectItem(anqpEntry,"IPProtocol");
                connCapBuf->ipProtocol = anqpParam ? anqpParam->valuedouble : 0;
                next_pos += sizeof(connCapBuf->ipProtocol);
                anqpParam = cJSON_GetObjectItem(anqpEntry,"PortNumber");
                connCapBuf->portNumber = anqpParam ? anqpParam->valuedouble : 0;
                next_pos += sizeof(connCapBuf->portNumber);
                anqpParam = cJSON_GetObjectItem(anqpEntry,"Status");
                connCapBuf->status = anqpParam ? anqpParam->valuedouble : 0;
                next_pos += sizeof(connCapBuf->status);
            }
        }
        g_hs2_data[apIns].connCapabilityLength = next_pos - g_hs2_data[apIns].connCapabilityInfo;
        wifi_passpoint_dbg_print(1, "%s:%d: Copied ConnectionCapabilityListANQPElement Data. Length: %d\n", __func__, __LINE__,g_hs2_data[apIns].connCapabilityLength);
        g_hs2_data[apIns].capabilityInfo.capabilityList[g_hs2_data[apIns].capabilityInfoLength++] = wifi_anqp_element_hs_subtype_conn_capability;
    }

    //NAIHomeRealmANQPElement
    if(g_hs2_data[apIns].realmInfo){
        free(g_hs2_data[apIns].realmInfo);
        g_hs2_data[apIns].realmInfo = NULL;
        g_hs2_data[apIns].realmInfoLength = 0;
    }
    anqpElement = cJSON_GetObjectItem(mainEntry,"NAIHomeRealmANQPElement");
    if(anqpElement){
        anqpList    = cJSON_GetObjectItem(anqpElement,"Realms");
        if(anqpList){
            if(cJSON_GetArraySize(anqpList) > 20){
                wifi_passpoint_dbg_print(1, "%s:%d:NAI Home Realm count cannot be more than 20. Discarding Configuration\n", __func__, __LINE__);
                pthread_mutex_unlock(&g_data_plane_module.lock);
                cJSON_Delete(passPointCfg);
                return ANSC_STATUS_FAILURE;
            }
            g_hs2_data[apIns].realmInfo = malloc(sizeof(wifi_HS2_NAI_Home_Realm_Query_t));
            memset(g_hs2_data[apIns].realmInfo,0,sizeof(wifi_HS2_NAI_Home_Realm_Query_t));
            next_pos = g_hs2_data[apIns].realmInfo;
            wifi_HS2_NAI_Home_Realm_Query_t *naiElem = (wifi_HS2_NAI_Home_Realm_Query_t *)next_pos;
            naiElem->realmCount = cJSON_GetArraySize(anqpList);
            next_pos += sizeof(naiElem->realmCount);
            cJSON_ArrayForEach(anqpEntry, anqpList){
                wifi_HS2_NAI_Home_Realm_Data_t *realmInfoBuf = (wifi_HS2_NAI_Home_Realm_Data_t *)next_pos;
                anqpParam = cJSON_GetObjectItem(anqpEntry,"Encoding");
                realmInfoBuf->encoding = anqpParam ? anqpParam->valuedouble : 0;
                next_pos += sizeof(realmInfoBuf->encoding);
                anqpParam = cJSON_GetObjectItem(anqpEntry,"Name");
                if(!anqpParam || (AnscSizeOfString(anqpParam->valuestring) > 255)){
                    wifi_passpoint_dbg_print(1, "%s:%d: Invalid NAI Home Realm Name. Discarding Configuration\n", __func__, __LINE__);
                    free(g_hs2_data[apIns].realmInfo);
                    g_hs2_data[apIns].realmInfo = NULL;
                    g_hs2_data[apIns].realmInfoLength = 0;
                    pthread_mutex_unlock(&g_data_plane_module.lock);
                    cJSON_Delete(passPointCfg);
                    return ANSC_STATUS_FAILURE;
                }
                realmInfoBuf->length = AnscSizeOfString(anqpParam->valuestring);
                next_pos += sizeof(realmInfoBuf->length);
                AnscCopyString(next_pos,anqpParam->valuestring);
                next_pos += realmInfoBuf->length;
            }
            g_hs2_data[apIns].realmInfoLength = next_pos - g_hs2_data[apIns].realmInfo;
            g_hs2_data[apIns].capabilityInfo.capabilityList[g_hs2_data[apIns].capabilityInfoLength++] = wifi_anqp_element_hs_subtype_nai_home_realm_query;
        }
        wifi_passpoint_dbg_print(1, "%s:%d: Copied NAIHomeRealmANQPElement Data. Length: %d\n", __func__, __LINE__,g_hs2_data[apIns].realmInfoLength);
    }
    
    pthread_mutex_unlock(&g_data_plane_module.lock);
    cJSON_Delete(passPointCfg);
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS CosaDmlWiFi_SetHS2Status(PCOSA_DML_WIFI_AP_CFG pCfg, BOOL bValue, BOOL setToPSM)
{

    if(!pCfg){
        wifi_passpoint_dbg_print(1,"AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }

    int apIns = pCfg->InstanceNumber;
    if((apIns < 1) || (apIns > 16)){
        wifi_passpoint_dbg_print(1, "%s:%d: Invalid AP Index. Return\n", __func__, __LINE__);
        return ANSC_STATUS_FAILURE;
    }
#if defined (FEATURE_SUPPORT_PASSPOINT)    
    int retPsmSet;
    CHAR strValue[32], recName[256];
    memset(recName, 0, 256);
    memset(strValue,0,32);
    snprintf(recName, sizeof(recName), PasspointEnable, apIns);
    if (bValue)
       sprintf(strValue,"%s","true");
    else 
       sprintf(strValue,"%s","false");
    
    if(setToPSM){ 
        retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue); 
        if (retPsmSet != CCSP_SUCCESS) {
            wifi_passpoint_dbg_print(1, "%s:%d: PSM Set Failed for Passpoint Status on AP: %d. Return\n", __func__, __LINE__,apIns);
            return ANSC_STATUS_FAILURE;
        }    
    }
    BOOL l2tif = false;
    if((pCfg->IEEE80211uCfg.IntwrkCfg.iAccessNetworkType == 2) || (pCfg->IEEE80211uCfg.IntwrkCfg.iAccessNetworkType == 3)){
        l2tif = true;
    }

    if(RETURN_OK == enablePassPointSettings (apIns-1, bValue, g_hs2_data[apIns-1].gafDisable, g_hs2_data[apIns-1].p2pDisable, l2tif)){
        pCfg->IEEE80211uCfg.PasspointCfg.Status = g_hs2_data[apIns-1].hs2Status = bValue;
    }else{
      wifi_passpoint_dbg_print(1, "%s:%d: Error Setting Passpoint Enable Status on AP: %d\n", __func__, __LINE__,apIns);
      return ANSC_STATUS_FAILURE;
    }

    if(true == bValue){
        wifi_passpoint_dbg_print(1, "%s:%d: Passpoint is Enabled on AP: %d\n", __func__, __LINE__,apIns);
    }else{
        wifi_passpoint_dbg_print(1, "%s:%d: Passpoint is Disabled on AP: %d\n", __func__, __LINE__,apIns);
    }
#endif

    return ANSC_STATUS_SUCCESS;
}
    
        
ANSC_STATUS CosaDmlWiFi_DefaultHS2Config(PCOSA_DML_WIFI_AP_CFG pCfg)
{
    if(!pCfg){
        wifi_passpoint_dbg_print(1,"AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    char *JSON_STR = malloc(strlen(WIFI_PASSPOINT_DEFAULT_HS2_CFG)+1);
    /*CID: 140457 Dereference before null check*/
    if (JSON_STR == NULL) {
        wifi_passpoint_dbg_print(1,"malloc failure\n");
        return ANSC_STATUS_FAILURE;
    }
    memset(JSON_STR,0,(strlen(WIFI_PASSPOINT_DEFAULT_HS2_CFG)+1));
    AnscCopyString(JSON_STR, WIFI_PASSPOINT_DEFAULT_HS2_CFG);
    
    if(!JSON_STR || (ANSC_STATUS_SUCCESS != CosaDmlWiFi_SetHS2Config(pCfg,JSON_STR))){
        if(JSON_STR){
            free(JSON_STR);
            JSON_STR = NULL;
        }
        wifi_passpoint_dbg_print(1,"Failed to update default HS2.0 Config.\n");
        return ANSC_STATUS_FAILURE;
    }
    pCfg->IEEE80211uCfg.PasspointCfg.HS2Parameters = JSON_STR;
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS CosaDmlWiFi_SaveHS2Cfg(PCOSA_DML_WIFI_AP_CFG pCfg, char *buffer, int len)
{
    char cfgFile[64];
    DIR     *passPointDir = NULL;
    int apIns = 0;
    
    passPointDir = opendir(WIFI_PASSPOINT_DIR);
    if(passPointDir){
        closedir(passPointDir);
    }else if(ENOENT == errno){
        if(0 != mkdir(WIFI_PASSPOINT_DIR, 0777)){
            wifi_passpoint_dbg_print(1,"Failed to Create Passpoint Configuration directory.\n");
            return ANSC_STATUS_FAILURE;
        }
    }else{
        wifi_passpoint_dbg_print(1,"Error opening Passpoint Configuration directory.\n");
        return ANSC_STATUS_FAILURE;
    }
    
    if(!pCfg){
        wifi_passpoint_dbg_print(1,"AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    apIns = pCfg->InstanceNumber;
    sprintf(cfgFile,"%s.%d",WIFI_PASSPOINT_HS2_CFG_FILE,apIns);
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
    UCHAR recName[256], *strValue=NULL;
    int retPsmGet;
    char cfgFile[64];
    char *JSON_STR = NULL;
    int apIns = 0;
    long confSize = 0;
    BOOL apEnable = FALSE;
    
    if(!pCfg){
        wifi_passpoint_dbg_print(1,"AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    pCfg->IEEE80211uCfg.PasspointCfg.HS2Parameters = NULL;
    apIns = pCfg->InstanceNumber;
    if((apIns < 1) || (apIns > 16)){
        wifi_passpoint_dbg_print(1, "%s:%d: Invalid AP Index. Return\n", __func__, __LINE__);
        return ANSC_STATUS_FAILURE;
    }
    
    sprintf(cfgFile,"%s.%d",WIFI_PASSPOINT_HS2_CFG_FILE,apIns);
    
    confSize = readFileToBuffer(cfgFile,&JSON_STR);
    
    //Initialize global buffer
    /*TODO RDKB-34680 CID: 140463,140471,140465 Data race condition*/
    g_hs2_data[apIns-1].hs2Status = false;
    g_hs2_data[apIns-1].gafDisable = true;
    g_hs2_data[apIns-1].p2pDisable = false;
    g_hs2_data[apIns-1].capabilityInfoLength = 0;
    g_hs2_data[apIns-1].opFriendlyNameInfoLength = 0;
    g_hs2_data[apIns-1].opFriendlyNameInfo = NULL;
    g_hs2_data[apIns-1].connCapabilityLength = 0;
    g_hs2_data[apIns-1].connCapabilityInfo = NULL;
    g_hs2_data[apIns-1].realmInfoLength = 0;
    g_hs2_data[apIns-1].realmInfo = NULL;

    memset(recName, 0, 256);
    snprintf(recName, sizeof(recName), PasspointEnable, apIns);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if ((retPsmGet == CCSP_SUCCESS) && ((0 == strcmp(strValue, "true")) || (0 == strcmp (strValue, "TRUE")))) {
        pCfg->IEEE80211uCfg.PasspointCfg.Status = g_hs2_data[apIns-1].hs2Status = true;

        wifi_getApEnable(apIns-1, &apEnable);
        wifi_passpoint_dbg_print(1, "%s:%d: Enable flag of AP Index: %d is %d \n", __func__, __LINE__,apIns, apEnable);
        if(apEnable)
        {
            if((g_hs2_data[apIns-1].hs2Status) && (ANSC_STATUS_SUCCESS != CosaDmlWiFi_SetHS2Status(pCfg,true,false))){
                wifi_passpoint_dbg_print(1, "%s:%d: Error Setting Passpoint Enable Status on AP: %d\n", __func__, __LINE__,apIns);
            }
        } else {
            wifi_passpoint_dbg_print(1, "%s:%d: VAP is disabled. Not Initializing Passpoint Enable Status on AP: %d\n", __func__, __LINE__,apIns);
        }
    }
    
    if(!confSize || !JSON_STR || (ANSC_STATUS_SUCCESS != CosaDmlWiFi_SetHS2Config(pCfg,JSON_STR))){
        if(JSON_STR){
            free(JSON_STR);
            JSON_STR = NULL;
        }
        wifi_passpoint_dbg_print(1,"Failed to Initialize HS2.0 Configuration from memory for AP: %d. Setting Default\n",apIns);
        return CosaDmlWiFi_DefaultHS2Config(pCfg);
    }
    wifi_passpoint_dbg_print(1,"Initialized HS2.0 Configuration from memory for AP: %d.\n",apIns);
    pCfg->IEEE80211uCfg.PasspointCfg.HS2Parameters = JSON_STR;
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS CosaDmlWiFi_GetWANMetrics(PCOSA_DML_WIFI_AP_CFG pCfg)
{
    cJSON *mainEntry = NULL;
    int apIns; 

    if(!pCfg){
        wifi_passpoint_dbg_print(1,"AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }

    apIns = pCfg->InstanceNumber -1;
    if((apIns < 0) || (apIns > 15)){
        wifi_passpoint_dbg_print(1, "%s:%d: Invalid AP Index. Setting to 1\n", __func__, __LINE__);
        apIns = 0;
    }

    memset(&pCfg->IEEE80211uCfg.PasspointCfg.WANMetrics, 0, sizeof(pCfg->IEEE80211uCfg.PasspointCfg.WANMetrics));

    cJSON *passPointCfg = cJSON_CreateObject();
    if (NULL == passPointCfg) {
        wifi_passpoint_dbg_print(1,"Failed to create JSON\n");
        return ANSC_STATUS_FAILURE;
    }

    mainEntry = cJSON_AddObjectToObject(passPointCfg,"WANMetrics");

    cJSON_AddNumberToObject(mainEntry,"WANInfo",g_hs2_data[apIns].wanMetricsInfo.wanInfo); 
    cJSON_AddNumberToObject(mainEntry,"DownlinkSpeed",g_hs2_data[apIns].wanMetricsInfo.downLinkSpeed); 
    cJSON_AddNumberToObject(mainEntry,"UplinkSpeed",g_hs2_data[apIns].wanMetricsInfo.upLinkSpeed);
    cJSON_AddNumberToObject(mainEntry,"DownlinkLoad",g_hs2_data[apIns].wanMetricsInfo.downLinkLoad); 
    cJSON_AddNumberToObject(mainEntry,"UplinkLoad",g_hs2_data[apIns].wanMetricsInfo.upLinkLoad); 
    cJSON_AddNumberToObject(mainEntry,"LMD",g_hs2_data[apIns].wanMetricsInfo.lmd); 

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
        wifi_passpoint_dbg_print(1,"AP Context is NULL\n");
        return;
    }

    apIns = pCfg->InstanceNumber -1;
    if((apIns < 0) || (apIns > 15)){
        wifi_passpoint_dbg_print(1, "%s:%d: Invalid AP Index.\n", __func__, __LINE__);
        return;
    }

    memset(&pCfg->IEEE80211uCfg.PasspointCfg.Stats, 0, sizeof(pCfg->IEEE80211uCfg.PasspointCfg.Stats));

    cJSON *passPointStats = cJSON_Parse(g_hs2_data[apIns].passpointStats);
    if (NULL == passPointStats) {
        wifi_passpoint_dbg_print(1,"Failed to parse JSON\n");
        return;
    }

    mainEntry = cJSON_GetObjectItem(passPointStats,"PassPointStats");
    if(NULL == mainEntry){
        wifi_passpoint_dbg_print(1,"PassPointStats entry is NULL\n");
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
                    cJSON_SetIntValue(cJSON_GetObjectItem(statsEntry,"Sent"),g_hs2_data[apIns].realmRespCount);
                    cJSON_SetIntValue(cJSON_GetObjectItem(statsEntry,"Failed"),g_hs2_data[apIns].realmFailedCount);
                    break;
                case 2:
                    cJSON_SetIntValue(cJSON_GetObjectItem(statsEntry,"Sent"),g_hs2_data[apIns].domainRespCount);
                    cJSON_SetIntValue(cJSON_GetObjectItem(statsEntry,"Failed"),g_hs2_data[apIns].domainFailedCount);
                    break;
                case 3:
                    cJSON_SetIntValue(cJSON_GetObjectItem(statsEntry,"Sent"),g_hs2_data[apIns].gppRespCount);
                    cJSON_SetIntValue(cJSON_GetObjectItem(statsEntry,"Failed"),g_hs2_data[apIns].gppFailedCount);
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
        wifi_passpoint_dbg_print(1, "%s:%d: Invalid AP Index: %d.\n", __func__, __LINE__,apIndex);
        return ANSC_STATUS_FAILURE;
    }

    BOOL l2tif = false;
    
    pCfg = find_ap_wifi_dml(apIndex);
  
    if (pCfg == NULL)
    {
        return ANSC_STATUS_FAILURE;
    }
  
    if(!pCfg->InterworkingEnable){
        wifi_passpoint_dbg_print(1, "%s:%d: Interworking is disabled on AP: %d. Returning Success\n", __func__, __LINE__, apIndex);
        return ANSC_STATUS_SUCCESS;
    }
  
    if ((CosaDmlWiFi_ApplyInterworkingElement(pCfg)) != ANSC_STATUS_SUCCESS)
    {
        wifi_passpoint_dbg_print(1, "%s:%d: CosaDmlWiFi_ApplyInterworkingElement Failed.\n", __func__, __LINE__);
    }
  
    if(ANSC_STATUS_SUCCESS != CosaDmlWiFi_ApplyRoamingConsortiumElement(pCfg)){
        wifi_passpoint_dbg_print(1, "%s:%d: CosaDmlWiFi_ApplyRoamingConsortiumElement Failed.\n", __func__, __LINE__);
    }

    if((pCfg->IEEE80211uCfg.IntwrkCfg.iAccessNetworkType == 2) || (pCfg->IEEE80211uCfg.IntwrkCfg.iAccessNetworkType == 3)){
        l2tif = true;
    }
    
    if ((g_hs2_data[apIndex].hs2Status) && (RETURN_OK != enablePassPointSettings (apIndex, g_hs2_data[apIndex].hs2Status, g_hs2_data[apIndex].gafDisable, g_hs2_data[apIndex].p2pDisable, l2tif))){
      wifi_passpoint_dbg_print(1, "%s:%d: Error Setting Passpoint Enable Status on AP: %d\n", __func__, __LINE__,apIndex);
      return ANSC_STATUS_FAILURE;
    }
    wifi_passpoint_dbg_print(1, "%s:%d: Set Passpoint Enable Status on AP: %d\n", __func__, __LINE__,apIndex);
#else
    UNREFERENCED_PARAMETER(apIndex);
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
    if(!pCfg){
        wifi_passpoint_dbg_print(1,"AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }

    int apIns = pCfg->InstanceNumber -1;
    if((apIns < 0) || (apIns > 15)){
        wifi_passpoint_dbg_print(1, "%s:%d: Invalid AP Index. Setting to 1\n", __func__, __LINE__);
        apIns = 0;
    }

    cJSON *mainEntry = NULL;
    cJSON *InterworkingElement = NULL;
    cJSON *venueParam = NULL;

    if(!JSON_STR){
        wifi_passpoint_dbg_print(1,"JSON String is NULL\n");
        return ANSC_STATUS_FAILURE;
    }

    cJSON *interworkingCfg = cJSON_Parse(JSON_STR);

    if (NULL == interworkingCfg) {
        wifi_passpoint_dbg_print(1,"Failed to parse JSON\n");
        return ANSC_STATUS_FAILURE;
    }

    mainEntry = cJSON_GetObjectItem(interworkingCfg,"Interworking_Element");
    if(NULL == mainEntry){
        wifi_passpoint_dbg_print(1,"Interworking entry is NULL\n");
        cJSON_Delete(interworkingCfg);
        return ANSC_STATUS_FAILURE;
    }

//Interworking Status
    InterworkingElement = cJSON_GetObjectItem(mainEntry,"InterworkingServiceEnable");
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
        AnscCopyString(pCfg->IEEE80211uCfg.IntwrkCfg.iHESSID, InterworkingElement->valuestring);
    } else {
        AnscCopyString(pCfg->IEEE80211uCfg.IntwrkCfg.iHESSID, "11:22:33:44:55:66");
    }

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
Funtion     : CosaDmlWiFi_DefaultInterworkingConfig
Input       : Pointer to vAP object
Description : Populates the vAP object pCfg with default values for 
              Interworking parameters
***********************************************************************/
ANSC_STATUS CosaDmlWiFi_DefaultInterworkingConfig(PCOSA_DML_WIFI_AP_CFG pCfg)
{       
    if(!pCfg){ 
        wifi_passpoint_dbg_print(1,"AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }

    pCfg->InterworkingEnable = 0;
    pCfg->IEEE80211uCfg.IntwrkCfg.iASRA = 0;
    pCfg->IEEE80211uCfg.IntwrkCfg.iESR = 0;
    pCfg->IEEE80211uCfg.IntwrkCfg.iUESA = 0;
    pCfg->IEEE80211uCfg.IntwrkCfg.iHESSOptionPresent = 1;
    strcpy(pCfg->IEEE80211uCfg.IntwrkCfg.iHESSID,"11:22:33:44:55:66");
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
Description : Check for Saved Configuration in JSON format.
              If not present, call CosaDmlWiFi_DefaultInterworkingConfig
              to populate default values
***********************************************************************/
ANSC_STATUS CosaDmlWiFi_InitInterworkingElement (PCOSA_DML_WIFI_AP_CFG pCfg)
{
    char cfgFile[64];
    char *JSON_STR = NULL;
    int apIns = 0;
    long confSize = 0;

    if(!pCfg){
        wifi_passpoint_dbg_print(1,"AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }

    apIns = pCfg->InstanceNumber;
    if((apIns < 1) || (apIns > 16)){
        wifi_passpoint_dbg_print(1, "%s:%d: Invalid AP Index. Return\n", __func__, __LINE__);
        return ANSC_STATUS_FAILURE;
    }

    sprintf(cfgFile,WIFI_INTERWORKING_CFG_FILE,apIns);

    confSize = readFileToBuffer(cfgFile,&JSON_STR);

    if(!confSize || !JSON_STR || (ANSC_STATUS_SUCCESS != CosaDmlWiFi_ReadInterworkingConfig(pCfg,JSON_STR))){
        if(JSON_STR){
            free(JSON_STR);
            JSON_STR = NULL;
        }
        wifi_passpoint_dbg_print(1,"Failed to Initialize Interwokring Configuration from memory for AP: %d. Setting Default\n",apIns);
        return CosaDmlWiFi_DefaultInterworkingConfig(pCfg);
    }
    wifi_passpoint_dbg_print(1,"Initialized Interworking Configuration from memory for AP: %d.\n",apIns);
    return ANSC_STATUS_SUCCESS;
}

/***********************************************************************
Funtion     : CosaDmlWiFi_ReadInterworkingConfig
Input       : Pointer to vAP object, JSON String, length of string
Description : Saves Interworking coinfiguration as JSON String into file.
              File is saved for each vap in format InterworkingCfg_<apIns>.json
***********************************************************************/
ANSC_STATUS CosaDmlWiFi_SaveInterworkingCfg(PCOSA_DML_WIFI_AP_CFG pCfg, char *buffer, int len)
{   
    char cfgFile[64];
    DIR     *passPointDir = NULL;
    int apIns = 0;
    
    passPointDir = opendir(WIFI_PASSPOINT_DIR);
    if(passPointDir){
        closedir(passPointDir);
    }else if(ENOENT == errno){
        if(0 != mkdir(WIFI_PASSPOINT_DIR, 0777)){ 
            wifi_passpoint_dbg_print(1,"Failed to Create Passpoint Configuration directory.\n");
            return ANSC_STATUS_FAILURE;
        }
    }else{
        wifi_passpoint_dbg_print(1,"Error opening Passpoint Configuration directory.\n");
        return ANSC_STATUS_FAILURE;
    }
    
    if(!pCfg){
        wifi_passpoint_dbg_print(1,"AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    apIns = pCfg->InstanceNumber;
    sprintf(cfgFile,WIFI_INTERWORKING_CFG_FILE,apIns);
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
        wifi_passpoint_dbg_print(1,"AP Context is NULL\n");
        return ANSC_STATUS_FAILURE;
    }
    
    apIns = pCfg->InstanceNumber;
    if((apIns < 1) || (apIns > 16)){
        return ANSC_STATUS_FAILURE;
    }
    
    cJSON *interworkingCfg = cJSON_CreateObject();
    if (NULL == interworkingCfg) {
        wifi_passpoint_dbg_print(1,"Failed to create JSON\n");
        return ANSC_STATUS_FAILURE;
    }
    
    memset(JSON_STR, 0, sizeof(JSON_STR));

    mainEntry = cJSON_AddObjectToObject(interworkingCfg,"Interworking_Element");
    
    cJSON_AddNumberToObject(mainEntry,"InterworkingServiceEnable",pCfg->InterworkingEnable); 
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

