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
#include "wifi_webconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "wifi_hal.h"
#include <cjson/cJSON.h>
#include "cosa_wifi_passpoint.h"
#include "ctype.h"
#include "safec_lib_common.h"

UINT g_interworking_RFC;
UINT g_passpoint_RFC;

#define validate_param_string(json, key, value) \
{	\
    value = cJSON_GetObjectItem(json, key); 	\
    if ((value == NULL) || (cJSON_IsString(value) == false) ||	\
            (value->valuestring == NULL) || (strcmp(value->valuestring, "") == 0)) {	\
        wifi_passpoint_dbg_print("%s:%d: Validation failed for key:%s\n", __func__, __LINE__, key);	\
        if (execRetVal) { \
            snprintf(execRetVal->ErrorMsg,sizeof(execRetVal->ErrorMsg)-1, "Missing or Invalid Value for key: %s",key); \
        } \
        return RETURN_ERR;	\
    }	\
}	\

#define validate_param_integer(json, key, value) \
{	\
    value = cJSON_GetObjectItem(json, key); 	\
    if ((value == NULL) || (cJSON_IsNumber(value) == false)) {	\
        wifi_passpoint_dbg_print("%s:%d: Validation failed for key:%s\n", __func__, __LINE__, key);	\
        if (execRetVal) { \
            snprintf(execRetVal->ErrorMsg,sizeof(execRetVal->ErrorMsg)-1, "Missing or Invalid Value for key: %s",key); \
        } \
        return RETURN_ERR;	\
    }	\
}	\

#define validate_param_bool(json, key, value) \
{	\
    value = cJSON_GetObjectItem(json, key); 	\
    if ((value == NULL) || (cJSON_IsBool(value) == false)) {	\
        wifi_passpoint_dbg_print("%s:%d: Validation failed for key:%s\n", __func__, __LINE__, key);	\
        if (execRetVal) { \
            snprintf(execRetVal->ErrorMsg,sizeof(execRetVal->ErrorMsg)-1, "Missing or Invalid Value for key: %s",key); \
        } \
        return RETURN_ERR;	\
    }	\
}	\


#define validate_param_array(json, key, value) \
{	\
    value = cJSON_GetObjectItem(json, key); 	\
    if ((value == NULL) || (cJSON_IsArray(value) == false)) {	\
        wifi_passpoint_dbg_print("%s:%d: Validation failed for key:%s\n", __func__, __LINE__, key);	\
        if (execRetVal) { \
            snprintf(execRetVal->ErrorMsg,sizeof(execRetVal->ErrorMsg)-1, "Missing or Invalid Value for key: %s",key); \
        } \
        return RETURN_ERR;	\
    }	\
}	\


#define validate_param_object(json, key, value) \
{	\
    value = cJSON_GetObjectItem(json, key); 	\
    if ((value == NULL) || (cJSON_IsObject(value) == false)) {	\
        wifi_passpoint_dbg_print("%s:%d: Validation failed for key:%s\n", __func__, __LINE__, key);	\
        if (execRetVal) { \
            snprintf(execRetVal->ErrorMsg,sizeof(execRetVal->ErrorMsg)-1, "Missing or Invalid Value for key: %s",key); \
        } \
        return RETURN_ERR;	\
    }	\
}	\


int validate_ipv4_address(char *ip) {
    struct sockaddr_in sa;

    if (inet_pton(AF_INET,ip, &(sa.sin_addr)) != 1 ) {
        CcspTraceError(("%s: Invalid IP address: %s\n",__FUNCTION__,ip));
        return RETURN_ERR;
    }
    return RETURN_OK;
}

int validate_anqp(const cJSON *anqp, wifi_interworking_t *vap_info, pErr execRetVal)
{
    cJSON *mainEntry = NULL;
    cJSON *anqpElement = NULL;
    cJSON *anqpList = NULL;
    cJSON *anqpEntry = NULL;
    cJSON *anqpParam = NULL;
    cJSON *subList = NULL;
    cJSON *subEntry = NULL;
    cJSON *subParam = NULL;
    UCHAR *next_pos = NULL;
    errno_t rc = -1;

    
    cJSON *passPointStats = cJSON_CreateObject();//root object for Passpoint Stats
    cJSON *statsMainEntry = cJSON_AddObjectToObject(passPointStats,"PassPointStats");
    cJSON *statsList = cJSON_AddArrayToObject(statsMainEntry, "ANQPResponse");
    
    if(!anqp || !vap_info || !execRetVal){
        wifi_passpoint_dbg_print("ANQP entry is NULL\n");
        if(execRetVal) {
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Empty ANQP Entry");
        }
        cJSON_Delete(passPointStats);
        return RETURN_ERR;
    }
    mainEntry = (cJSON *) anqp;

    //CapabilityListANQPElement
    vap_info->anqp.capabilityInfoLength = 0;
    vap_info->anqp.capabilityInfo.capabilityList[vap_info->anqp.capabilityInfoLength++] = wifi_anqp_element_name_query_list;
    vap_info->anqp.capabilityInfo.capabilityList[vap_info->anqp.capabilityInfoLength++] = wifi_anqp_element_name_capability_list;
    
    //VenueNameANQPElement
    validate_param_object(mainEntry,"VenueNameANQPElement",anqpElement);
 
    next_pos = (UCHAR *)&vap_info->anqp.venueInfo;

    validate_param_array(anqpElement,"VenueInfo",anqpList);
    if(cJSON_GetArraySize(anqpList) > 16){
        wifi_passpoint_dbg_print( "%s:%d: Venue entries cannot be more than 16. Discarding Configuration\n", __func__, __LINE__);
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Exceeded Max number of Venue entries");
        cJSON_Delete(passPointStats);
        return RETURN_ERR;
    } else if (cJSON_GetArraySize(anqpList)) {
        //Venue List is non-empty. Update capability List
        vap_info->anqp.capabilityInfo.capabilityList[vap_info->anqp.capabilityInfoLength++] = wifi_anqp_element_name_venue_name;

        //Fill in Venue Group and Type from Interworking Config
        wifi_venueNameElement_t *venueElem = (wifi_venueNameElement_t *)next_pos;
        venueElem->venueGroup = vap_info->interworking.venueGroup;
        next_pos += sizeof(venueElem->venueGroup);
        venueElem->venueType = vap_info->interworking.venueType;
        next_pos += sizeof(venueElem->venueType);
    }

    cJSON_ArrayForEach(anqpEntry, anqpList){
        wifi_venueName_t *venueBuf = (wifi_venueName_t *)next_pos;
        next_pos += sizeof(venueBuf->length); //Will be filled at the end
        validate_param_string(anqpEntry,"Language",anqpParam);
        /*
             * LIMITATION
             * Following AnscCopyString() can't modified to safec strcpy_s() api
             * Because, safec has the limitation of copying only 4k ( RSIZE_MAX ) to destination pointer
             * And here, we have detination size more than 4k
            */
        AnscCopyString((char*)next_pos, anqpParam->valuestring);
        next_pos += AnscSizeOfString(anqpParam->valuestring);
        anqpParam = cJSON_GetObjectItem(anqpEntry,"Name");
        if(AnscSizeOfString(anqpParam->valuestring) > 255){
            wifi_passpoint_dbg_print( "%s:%d: Venue name cannot be more than 255. Discarding Configuration\n", __func__, __LINE__);
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid size for Venue name");
            cJSON_Delete(passPointStats);
            return RETURN_ERR;
        }
        AnscCopyString((char*)next_pos, anqpParam->valuestring);
        next_pos += AnscSizeOfString(anqpParam->valuestring);
        venueBuf->length = next_pos - &venueBuf->language[0];
    }
    vap_info->anqp.venueInfoLength = next_pos - (UCHAR *)&vap_info->anqp.venueInfo;

    //RoamingConsortiumANQPElement
    validate_param_object(mainEntry,"RoamingConsortiumANQPElement", anqpElement);
    next_pos = (UCHAR *)&vap_info->anqp.roamInfo;

    validate_param_array(anqpElement,"OI",anqpList);
    if(cJSON_GetArraySize(anqpList) > 32){
        wifi_passpoint_dbg_print( "%s:%d: Only 32 OUI supported in RoamingConsortiumANQPElement Data. Discarding Configuration\n", __func__, __LINE__); 
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid number of OUIs");
        cJSON_Delete(passPointStats);
        return RETURN_ERR;
    }
    int ouiCount = 0;
    cJSON_ArrayForEach(anqpEntry, anqpList){
        wifi_ouiDuple_t *ouiBuf = (wifi_ouiDuple_t *)next_pos;
        UCHAR ouiStr[30+1];
        int i, ouiStrLen = 0;
        memset(ouiStr,0,sizeof(ouiStr));
        anqpParam = cJSON_GetObjectItem(anqpEntry,"OI");
        if(anqpParam){
            ouiStrLen = AnscSizeOfString(anqpParam->valuestring);
            if((ouiStrLen < 6) || (ouiStrLen > 30) || (ouiStrLen % 2)){
                wifi_passpoint_dbg_print( "%s:%d: Invalid OUI Length in RoamingConsortiumANQPElement Data. Discarding Configuration\n", __func__, __LINE__);
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid OUI Length");
                cJSON_Delete(passPointStats);
                return RETURN_ERR;
            }
            rc = strcpy_s((char*)ouiStr, sizeof(ouiStr), anqpParam->valuestring);
            ERR_CHK(rc);
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
                wifi_passpoint_dbg_print( "%s:%d: Invalid OUI in RoamingConsortiumANQPElement Data. Discarding Configuration\n", __func__, __LINE__);
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid  character in OUI");
                cJSON_Delete(passPointStats);
                return RETURN_ERR;
            }
            if(i%2){
                ouiBuf->oui[(i/2)] = ouiStr[i] | (ouiStr[i-1] << 4);
            }
        }
        ouiBuf->length = i/2;
        next_pos += sizeof(ouiBuf->length);
        next_pos += ouiBuf->length;
        if(ouiCount < 3){
            memcpy(&vap_info->roamingConsortium.wifiRoamingConsortiumOui[ouiCount][0],&ouiBuf->oui[0],ouiBuf->length);
            vap_info->roamingConsortium.wifiRoamingConsortiumLen[ouiCount] = ouiBuf->length;
        }
        ouiCount++;
    }
    vap_info->roamingConsortium.wifiRoamingConsortiumCount = ouiCount;

    if(ouiCount) {
        vap_info->anqp.capabilityInfo.capabilityList[vap_info->anqp.capabilityInfoLength++] = wifi_anqp_element_name_roaming_consortium;
    }

    vap_info->anqp.roamInfoLength = next_pos - (UCHAR *)&vap_info->anqp.roamInfo;

    //IPAddressTypeAvailabilityANQPElement
    validate_param_object(mainEntry,"IPAddressTypeAvailabilityANQPElement",anqpElement);
    vap_info->anqp.ipAddressInfo.field_format = 0;

    validate_param_integer(anqpElement,"IPv6AddressType",anqpParam);
    if((0 > anqpParam->valuedouble) || (2 < anqpParam->valuedouble)){
        wifi_passpoint_dbg_print( "%s:%d: Invalid IPAddressTypeAvailabilityANQPElement. Discarding Configuration\n", __func__, __LINE__);
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid IPAddressTypeAvailabilityANQPElement");
        cJSON_Delete(passPointStats);
        return RETURN_ERR;
    }
    vap_info->anqp.ipAddressInfo.field_format = (UCHAR)anqpParam->valuedouble;

    validate_param_integer(anqpElement,"IPv4AddressType",anqpParam);
    if((0 > anqpParam->valuedouble) || (7 < anqpParam->valuedouble)){
        wifi_passpoint_dbg_print( "%s:%d: Invalid IPAddressTypeAvailabilityANQPElement. Discarding Configuration\n", __func__, __LINE__);
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid IPAddressTypeAvailabilityANQPElement");
        cJSON_Delete(passPointStats);
        return RETURN_ERR;
    }
    vap_info->anqp.ipAddressInfo.field_format |= ((UCHAR)anqpParam->valuedouble << 2);
    vap_info->anqp.capabilityInfo.capabilityList[vap_info->anqp.capabilityInfoLength++] = wifi_anqp_element_name_ip_address_availabality;

    //NAIRealmANQPElement
    validate_param_object(mainEntry, "NAIRealmANQPElement", anqpElement);

    validate_param_array(anqpElement, "Realm", anqpList);

    wifi_naiRealmElement_t *naiElem = &vap_info->anqp.realmInfo;
    naiElem->nai_realm_count = cJSON_GetArraySize(anqpList);
    if(naiElem->nai_realm_count > 20) {
        wifi_passpoint_dbg_print( "%s:%d: Only 20 Realm Entries are supported. Discarding Configuration\n", __func__, __LINE__);
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Exceeded max number of Realm entries");
        cJSON_Delete(passPointStats);
        return RETURN_ERR;
    }
    next_pos = (UCHAR *)naiElem;
    next_pos += sizeof(naiElem->nai_realm_count);

    if(naiElem->nai_realm_count) {
        vap_info->anqp.capabilityInfo.capabilityList[vap_info->anqp.capabilityInfoLength++] = wifi_anqp_element_name_nai_realm;
    }

    cJSON_ArrayForEach(anqpEntry, anqpList){
        wifi_naiRealm_t *realmInfoBuf = (wifi_naiRealm_t *)next_pos;
        next_pos += sizeof(realmInfoBuf->data_field_length);

        validate_param_integer(anqpEntry,"RealmEncoding",anqpParam);
        realmInfoBuf->encoding = anqpParam->valuedouble;
        next_pos += sizeof(realmInfoBuf->encoding);

        validate_param_string(anqpEntry,"Realms",anqpParam);
        if(AnscSizeOfString(anqpParam->valuestring) > 255){
            wifi_passpoint_dbg_print( "%s:%d: Realm Length cannot be more than 255. Discarding Configuration\n", __func__, __LINE__);
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Realm Length");
            cJSON_Delete(passPointStats);
            return RETURN_ERR;
        }
        realmInfoBuf->realm_length = AnscSizeOfString(anqpParam->valuestring);
        next_pos += sizeof(realmInfoBuf->realm_length);
        AnscCopyString((char*)next_pos, anqpParam->valuestring);
        next_pos += realmInfoBuf->realm_length;

        cJSON *realmStats = cJSON_CreateObject();//Create a stats Entry here for each Realm
        cJSON_AddStringToObject(realmStats, "Name", anqpParam->valuestring);
        cJSON_AddNumberToObject(realmStats, "EntryType", 1);//1-NAI Realm
        cJSON_AddNumberToObject(realmStats, "Sent", 0);
        cJSON_AddNumberToObject(realmStats, "Failed", 0);
        cJSON_AddNumberToObject(realmStats, "Timeout", 0);
        cJSON_AddItemToArray(statsList, realmStats);

        validate_param_array(anqpEntry,"EAP",subList);
        realmInfoBuf->eap_method_count = cJSON_GetArraySize(subList);
        if(realmInfoBuf->eap_method_count > 16){
            wifi_passpoint_dbg_print( "%s:%d: EAP entries cannot be more than 16. Discarding Configuration\n", __func__, __LINE__);
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid number of EAP entries in realm");
            cJSON_Delete(passPointStats);
            return RETURN_ERR;
        }
        next_pos += sizeof(realmInfoBuf->eap_method_count);

        cJSON_ArrayForEach(subEntry, subList){
            wifi_eapMethod_t *eapBuf = (wifi_eapMethod_t *)next_pos;
            validate_param_integer(subEntry,"Method",subParam);
            eapBuf->method = subParam->valuedouble;
            next_pos += sizeof(eapBuf->method);
            cJSON *subList_1  = NULL;
            cJSON *subEntry_1 = NULL;
            cJSON *subParam_1 = NULL;
            
            validate_param_array(subEntry,"AuthenticationParameter",subList_1);
            eapBuf->auth_param_count = cJSON_GetArraySize(subList_1);
            if(eapBuf->auth_param_count > 16){
                wifi_passpoint_dbg_print( "%s:%d: Auth entries cannot be more than 16. Discarding Configuration\n", __func__, __LINE__);
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid number of Auth entries in EAP Method");
                cJSON_Delete(passPointStats);
                return RETURN_ERR;
            }
            next_pos += sizeof(eapBuf->auth_param_count);
            cJSON_ArrayForEach(subEntry_1, subList_1){
                int i,authStrLen;
                UCHAR authStr[14+1];
                wifi_authMethod_t *authBuf = (wifi_authMethod_t *)next_pos;

                validate_param_integer(subEntry_1,"ID",subParam_1);
                authBuf->id = subParam_1->valuedouble;
                next_pos += sizeof(authBuf->id);

                subParam_1 = cJSON_GetObjectItem(subEntry_1,"Value");
                if(!subParam_1){
                    wifi_passpoint_dbg_print( "%s:%d: Auth Parameter Value not prensent in NAIRealmANQPElement EAP Data. Discarding Configuration\n", __func__, __LINE__);
                    snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Auth param missing in RealANQP EAP Data");
                    cJSON_Delete(passPointStats);
                    return RETURN_ERR;
                } else if (subParam_1->valuedouble) {
                    authBuf->length = 1;
                    authBuf->val[0] = subParam_1->valuedouble;
                } else {
                    authStrLen = AnscSizeOfString(subParam_1->valuestring);
                    if((authStrLen != 2) && (authStrLen != 14)){
                        wifi_passpoint_dbg_print( "%s:%d: Invalid EAP Value Length in NAIRealmANQPElement Data. Has to be 1 to 7 bytes Long. Discarding Configuration\n", __func__, __LINE__);
                        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid EAP Length in NAIRealmANQPElement Data");
                        cJSON_Delete(passPointStats);
                        return RETURN_ERR;
                    }
                    AnscCopyString((char*)authStr,subParam_1->valuestring);
                                
                    //Covert the incoming string to HEX
                    for(i = 0; i < authStrLen; i++){ 
                        if((authStr[i] >= '0') && (authStr[i] <= '9')){
                            authStr[i] -= '0';  
                        }else if((authStr[i] >= 'a') && (authStr[i] <= 'f')){
                            authStr[i] -= ('a' - 10);//a=10
                        }else if((authStr[i] >= 'A') && (authStr[i] <= 'F')){
                            authStr[i] -= ('A' - 10);//A=10
                        }else{
                            wifi_passpoint_dbg_print( "%s:%d: Invalid EAP val in NAIRealmANQPElement Data. Discarding Configuration\n", __func__, __LINE__); 
                            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid EAP value in NAIRealmANQPElement Data");
                            cJSON_Delete(passPointStats);
                            return RETURN_ERR;
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
    vap_info->anqp.realmInfoLength = next_pos - (UCHAR *)&vap_info->anqp.realmInfo;

    //3GPPCellularANQPElement
    validate_param_object(mainEntry, "3GPPCellularANQPElement", anqpElement);
    wifi_3gppCellularNetwork_t *gppBuf = &vap_info->anqp.gppInfo;
    next_pos = (UCHAR *)gppBuf;

    validate_param_integer(anqpElement,"GUD",anqpParam);
    gppBuf->gud = anqpParam->valuedouble;
    next_pos += sizeof(gppBuf->gud);

    next_pos += sizeof(gppBuf->uhdLength);//Skip over UHD length to be filled at the end
    UCHAR *uhd_pos = next_pos;//Beginning of UHD data

    wifi_3gpp_plmn_list_information_element_t *plmnInfoBuf = (wifi_3gpp_plmn_list_information_element_t *)next_pos;
    plmnInfoBuf->iei = 0;
    next_pos += sizeof(plmnInfoBuf->iei);
    next_pos += sizeof(plmnInfoBuf->plmn_length);//skip through the length field that will be filled at the end
    UCHAR *plmn_pos = next_pos;//beginnig of PLMN data

    validate_param_array(anqpElement,"PLMN",anqpList);
    plmnInfoBuf->number_of_plmns = cJSON_GetArraySize(anqpList);
    next_pos += sizeof(plmnInfoBuf->number_of_plmns);  
    if(plmnInfoBuf->number_of_plmns > 16){
        wifi_passpoint_dbg_print( "%s:%d: 3GPP entries cannot be more than 16. Discarding Configuration\n", __func__, __LINE__);
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Exceeded max number of 3GPP entries");
        cJSON_Delete(passPointStats);
        return RETURN_ERR; 
     }

    cJSON_ArrayForEach(anqpEntry, anqpList){
        UCHAR mccStr[3+1];
        UCHAR mncStr[3+1];
        memset(mccStr,0,sizeof(mccStr));
        memset(mncStr,0,sizeof(mncStr));

        validate_param_string(anqpEntry,"MCC",anqpParam);
        if(AnscSizeOfString(anqpParam->valuestring) == (sizeof(mccStr) -1)){
            rc = strcpy_s((char*)mccStr, sizeof(mccStr), anqpParam->valuestring);
            ERR_CHK(rc);
        }else if(AnscSizeOfString(anqpParam->valuestring) == (sizeof(mccStr) -2)){
            mccStr[0] = '0';
            rc = strcpy_s((char*)&mccStr[1], (sizeof(mccStr) - 1),anqpParam->valuestring);
            ERR_CHK(rc);
        }else{
            wifi_passpoint_dbg_print( "%s:%d: Invalid MCC in 3GPPCellularANQPElement Data. Discarding Configuration\n", __func__, __LINE__);
            rc = strcpy_s(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg), "Invalid MCC in 3GPP Element");
            ERR_CHK(rc);
            cJSON_Delete(passPointStats);
            return RETURN_ERR;
        }

        validate_param_string(anqpEntry,"MNC",anqpParam);
        if(AnscSizeOfString(anqpParam->valuestring) == (sizeof(mccStr) -1)){
            rc = strcpy_s((char*)mncStr, sizeof(mncStr), anqpParam->valuestring);
            ERR_CHK(rc);
        }else if(AnscSizeOfString(anqpParam->valuestring) ==  (sizeof(mccStr) -2)){
            mncStr[0] = '0';
            rc = strcpy_s((char*)&mncStr[1], (sizeof(mncStr) - 1), anqpParam->valuestring);
            ERR_CHK(rc);
        }else{
            wifi_passpoint_dbg_print( "%s:%d: Invalid MNC in 3GPPCellularANQPElement Data. Discarding Configuration\n", __func__, __LINE__);
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid MNC in 3GPP Element");
            cJSON_Delete(passPointStats);
            return RETURN_ERR;
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
    vap_info->anqp.gppInfoLength = next_pos - (UCHAR *)&vap_info->anqp.gppInfo;
    vap_info->anqp.capabilityInfo.capabilityList[vap_info->anqp.capabilityInfoLength++] = wifi_anqp_element_name_3gpp_cellular_network;            
    
    //DomainANQPElement
    validate_param_object(mainEntry, "DomainANQPElement", anqpElement);
    validate_param_array(anqpElement, "DomainName", anqpList);

    if(cJSON_GetArraySize(anqpList) > 4){
        wifi_passpoint_dbg_print( "%s:%d: Only 4 Entries supported in DomainNameANQPElement Data. Discarding Configuration\n", __func__, __LINE__);
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Exceeded max no of entries in DomainNameANQPElement Data");
        cJSON_Delete(passPointStats);
        return RETURN_ERR;
    }
    next_pos = (UCHAR *)&vap_info->anqp.domainNameInfo;

    cJSON_ArrayForEach(anqpEntry, anqpList){
        wifi_domainNameTuple_t *nameBuf = (wifi_domainNameTuple_t *)next_pos;
        validate_param_string(anqpEntry,"Name",anqpParam);
        if(AnscSizeOfString(anqpParam->valuestring) > 255){ 
            wifi_passpoint_dbg_print( "%s:%d: Domain name length cannot be more than 255. Discarding Configuration\n", __func__, __LINE__);
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Domain name length");
            cJSON_Delete(passPointStats);
            return RETURN_ERR;
        }
        nameBuf->length = AnscSizeOfString(anqpParam->valuestring);
        next_pos += sizeof(nameBuf->length);
        AnscCopyString((char*)next_pos, anqpParam->valuestring);
        next_pos += nameBuf->length;

        cJSON *realmStats = cJSON_CreateObject();//Create a stats Entry here for each Realm
        cJSON_AddStringToObject(realmStats, "Name", anqpParam->valuestring);
        cJSON_AddNumberToObject(realmStats, "EntryType", 2);//2-Domain
        cJSON_AddNumberToObject(realmStats, "Sent", 0);
        cJSON_AddNumberToObject(realmStats, "Failed", 0);
        cJSON_AddNumberToObject(realmStats, "Timeout", 0);
        cJSON_AddItemToArray(statsList, realmStats);
    }
    
    vap_info->anqp.domainInfoLength = next_pos - (UCHAR *)&vap_info->anqp.domainNameInfo;
    if (vap_info->anqp.domainInfoLength) {
        vap_info->anqp.capabilityInfo.capabilityList[vap_info->anqp.capabilityInfoLength++] = wifi_anqp_element_name_domain_name;
    }

    //Update the stats JSON
    cJSON_PrintPreallocated(passPointStats,(char *)&vap_info->anqp.passpointStats, sizeof(vap_info->anqp.passpointStats), false);
    cJSON_Delete(passPointStats);

    return RETURN_OK;
}

int validate_passpoint(const cJSON *passpoint, wifi_interworking_t *vap_info, pErr execRetVal) 
{
    cJSON *mainEntry = NULL;
    cJSON *anqpElement = NULL;
    cJSON *anqpList = NULL;
    cJSON *anqpEntry = NULL;
    cJSON *anqpParam = NULL;
    UCHAR *next_pos = NULL;

    if(!passpoint || !vap_info || !execRetVal){
        wifi_passpoint_dbg_print("Passpoint entry is NULL\n");
        return RETURN_ERR;
    }
    mainEntry = (cJSON *)passpoint;

    validate_param_bool(mainEntry, "PasspointEnable", anqpParam);
    vap_info->passpoint.enable = (anqpParam->type & cJSON_True) ? true:false;

    if(vap_info->passpoint.enable) {
        if(!g_passpoint_RFC) {
            wifi_passpoint_dbg_print("%s:%d: Passpoint cannot be enable when RFC is disabled\n", __func__, __LINE__);
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "PasspointEnable: Cannot Enable Passpoint. RFC Disabled");
            return RETURN_ERR;
        } else if(vap_info->interworking.interworkingEnabled == FALSE) {
            wifi_passpoint_dbg_print("%s:%d: Passpoint cannot be enable when Interworking is disabled\n", __func__, __LINE__);
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Cannot Enable Passpoint. Interworking Disabled");
            return RETURN_ERR;
        }
    }	
    validate_param_bool(mainEntry, "GroupAddressedForwardingDisable", anqpParam);
    vap_info->passpoint.gafDisable = (anqpParam->type & cJSON_True) ? true:false;

    validate_param_bool(mainEntry, "P2pCrossConnectionDisable", anqpParam);
    vap_info->passpoint.p2pDisable = (anqpParam->type & cJSON_True) ? true:false;

    if((vap_info->interworking.accessNetworkType == 2) || (vap_info->interworking.accessNetworkType == 3)) {
        vap_info->passpoint.l2tif = true;
    }

    if(vap_info->passpoint.enable) {
        vap_info->passpoint.bssLoad = true;
        vap_info->passpoint.countryIE = true;
        vap_info->passpoint.proxyArp = true;
    }

    //HS2CapabilityListANQPElement
    vap_info->passpoint.capabilityInfoLength = 0;
    vap_info->passpoint.capabilityInfo.capabilityList[vap_info->passpoint.capabilityInfoLength++] = wifi_anqp_element_hs_subtype_hs_query_list;
    vap_info->passpoint.capabilityInfo.capabilityList[vap_info->passpoint.capabilityInfoLength++] = wifi_anqp_element_hs_subtype_hs_capability_list;
    
    //OperatorFriendlyNameANQPElement
    validate_param_object(mainEntry,"OperatorFriendlyNameANQPElement",anqpElement);
    validate_param_array(anqpElement,"Name",anqpList);

    if(cJSON_GetArraySize(anqpList) > 16){
        wifi_passpoint_dbg_print( "%s:%d: OperatorFriendlyName cannot have more than 16 entiries. Discarding Configuration\n", __func__, __LINE__);
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid no of entries in OperatorFriendlyName");
        return RETURN_ERR;
    }

    next_pos = (UCHAR *)&vap_info->passpoint.opFriendlyNameInfo;
    cJSON_ArrayForEach(anqpEntry, anqpList){
        wifi_HS2_OperatorNameDuple_t *opNameBuf = (wifi_HS2_OperatorNameDuple_t *)next_pos;
        next_pos += sizeof(opNameBuf->length);//Fill length after reading the remaining fields

        validate_param_string(anqpEntry,"LanguageCode",anqpParam);
        if(AnscSizeOfString(anqpParam->valuestring) > 3){
            wifi_passpoint_dbg_print( "%s:%d: Invalid Language Code. Discarding Configuration\n", __func__, __LINE__);
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Language Code");
            return RETURN_ERR;
        }
        AnscCopyString((char*)next_pos, anqpParam->valuestring);
        next_pos += sizeof(opNameBuf->languageCode);

        validate_param_string(anqpEntry,"OperatorName",anqpParam);
        if(AnscSizeOfString(anqpParam->valuestring) > 252){
            wifi_passpoint_dbg_print( "%s:%d: Invalid OperatorFriendlyName. Discarding Configuration\n", __func__, __LINE__);
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid OperatorFriendlyName");
            return RETURN_ERR;
        }
        AnscCopyString((char*)next_pos, anqpParam->valuestring);
        next_pos += AnscSizeOfString(anqpParam->valuestring);
        opNameBuf->length = AnscSizeOfString(anqpParam->valuestring) +  sizeof(opNameBuf->languageCode);
    }
    vap_info->passpoint.opFriendlyNameInfoLength = next_pos - (UCHAR *)&vap_info->passpoint.opFriendlyNameInfo;
    if(vap_info->passpoint.opFriendlyNameInfoLength) {
        vap_info->passpoint.capabilityInfo.capabilityList[vap_info->passpoint.capabilityInfoLength++] = wifi_anqp_element_hs_subtype_operator_friendly_name;
    }

    //ConnectionCapabilityListANQPElement
    validate_param_object(mainEntry,"ConnectionCapabilityListANQPElement",anqpElement);
    validate_param_array(anqpElement,"ProtoPort",anqpList);
    if(cJSON_GetArraySize(anqpList) > 16){
        wifi_passpoint_dbg_print( "%s:%d: Connection Capability count cannot be more than 16. Discarding Configuration\n", __func__, __LINE__);
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Exceeded max count of Connection Capability");
        return RETURN_ERR;
    }
    next_pos = (UCHAR *)&vap_info->passpoint.connCapabilityInfo;
    cJSON_ArrayForEach(anqpEntry, anqpList){
        wifi_HS2_Proto_Port_Tuple_t *connCapBuf = (wifi_HS2_Proto_Port_Tuple_t *)next_pos;
        validate_param_integer(anqpEntry,"IPProtocol",anqpParam);
        connCapBuf->ipProtocol = anqpParam->valuedouble;
        next_pos += sizeof(connCapBuf->ipProtocol);
        validate_param_integer(anqpEntry,"PortNumber",anqpParam);
        connCapBuf->portNumber = anqpParam->valuedouble;
        next_pos += sizeof(connCapBuf->portNumber);
        validate_param_integer(anqpEntry,"Status",anqpParam);
        connCapBuf->status = anqpParam->valuedouble;
        next_pos += sizeof(connCapBuf->status);
    }
    vap_info->passpoint.connCapabilityLength = next_pos - (UCHAR *)&vap_info->passpoint.connCapabilityInfo;
    if(vap_info->passpoint.connCapabilityLength) {
        vap_info->passpoint.capabilityInfo.capabilityList[vap_info->passpoint.capabilityInfoLength++] = wifi_anqp_element_hs_subtype_conn_capability;
    }

    //NAIHomeRealmANQPElement
    validate_param_object(mainEntry,"NAIHomeRealmANQPElement",anqpElement);
    validate_param_array(anqpElement,"Realms",anqpList);
    if(cJSON_GetArraySize(anqpList) > 20){
        wifi_passpoint_dbg_print( "%s:%d: NAI Realm count cannot be more than 20. Discarding Configuration\n", __func__, __LINE__);
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Exceeded max count of NAI Realm");
        return RETURN_ERR;
    }
    next_pos = (UCHAR *)&vap_info->passpoint.realmInfo;
    wifi_HS2_NAI_Home_Realm_Query_t *naiElem = (wifi_HS2_NAI_Home_Realm_Query_t *)next_pos;
    naiElem->realmCount = cJSON_GetArraySize(anqpList);
    next_pos += sizeof(naiElem->realmCount);
    cJSON_ArrayForEach(anqpEntry, anqpList){
        wifi_HS2_NAI_Home_Realm_Data_t *realmInfoBuf = (wifi_HS2_NAI_Home_Realm_Data_t *)next_pos;
        validate_param_integer(anqpEntry,"Encoding",anqpParam);
        realmInfoBuf->encoding = anqpParam->valuedouble;
        next_pos += sizeof(realmInfoBuf->encoding);
        validate_param_string(anqpEntry,"Name",anqpParam);
        if(AnscSizeOfString(anqpParam->valuestring) > 255){
            wifi_passpoint_dbg_print( "%s:%d: Invalid NAI Home Realm Name. Discarding Configuration\n", __func__, __LINE__);
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid NAI Home Realm Name");
            return RETURN_ERR;
        }
        realmInfoBuf->length = AnscSizeOfString(anqpParam->valuestring);
        next_pos += sizeof(realmInfoBuf->length);
        AnscCopyString((char*)next_pos, anqpParam->valuestring);
        next_pos += realmInfoBuf->length;
    }
    vap_info->passpoint.realmInfoLength = next_pos - (UCHAR *)&vap_info->passpoint.realmInfo;
    if(vap_info->passpoint.realmInfoLength) {
        vap_info->passpoint.capabilityInfo.capabilityList[vap_info->passpoint.capabilityInfoLength++] = wifi_anqp_element_hs_subtype_nai_home_realm_query;
    }
   
    //WANMetricsANQPElement
    //wifi_getHS2WanMetrics(&g_hs2_data[apIns].wanMetricsInfo);
    vap_info->passpoint.wanMetricsInfo.wanInfo = 0b00000001;
    vap_info->passpoint.wanMetricsInfo.downLinkSpeed = 25000;
    vap_info->passpoint.wanMetricsInfo.upLinkSpeed = 5000;
    vap_info->passpoint.wanMetricsInfo.downLinkLoad = 0;
    vap_info->passpoint.wanMetricsInfo.upLinkLoad = 0;
    vap_info->passpoint.wanMetricsInfo.lmd = 0;
    vap_info->passpoint.capabilityInfo.capabilityList[vap_info->passpoint.capabilityInfoLength++] = wifi_anqp_element_hs_subtype_wan_metrics;
 
    return RETURN_OK;
}

int validate_interworking(const cJSON *interworking, wifi_vap_info_t *vap_info, pErr execRetVal)
{
    const cJSON *param, *venue, *passpoint, *anqp;
    errno_t rc = -1;

    if(!interworking || !vap_info || !execRetVal){
        wifi_passpoint_dbg_print("Interworking entry is NULL\n");
        return RETURN_ERR;
    }
    validate_param_bool(interworking, "InterworkingEnable", param);
    vap_info->u.bss_info.interworking.interworking.interworkingEnabled = (param->type & cJSON_True) ? true:false;

    if((!g_interworking_RFC) && (vap_info->u.bss_info.interworking.interworking.interworkingEnabled)) {
        wifi_passpoint_dbg_print("%s:%d: Interworking cannot be enable when RFC is disabled\n", __func__, __LINE__);	
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "InterworkingEnable: Cannot Enable Interworking. RFC Disabled");
        return RETURN_ERR;
    }
	
    validate_param_integer(interworking, "AccessNetworkType", param);
    vap_info->u.bss_info.interworking.interworking.accessNetworkType = param->valuedouble;
    if (vap_info->u.bss_info.interworking.interworking.accessNetworkType > 5) {
        wifi_passpoint_dbg_print("%s:%d: Validation failed for AccessNetworkType\n", __func__, __LINE__);	
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Access Network type");
        return RETURN_ERR;
    }
	
    validate_param_bool(interworking, "Internet", param);
    vap_info->u.bss_info.interworking.interworking.internetAvailable = (param->type & cJSON_True) ? true:false;

    validate_param_bool(interworking, "ASRA", param);
    vap_info->u.bss_info.interworking.interworking.asra = (param->type & cJSON_True) ? true:false;

    validate_param_bool(interworking, "ESR", param);
    vap_info->u.bss_info.interworking.interworking.esr = (param->type & cJSON_True) ? true:false;

    validate_param_bool(interworking, "UESA", param);
    vap_info->u.bss_info.interworking.interworking.uesa = (param->type & cJSON_True) ? true:false;

    validate_param_bool(interworking, "HESSOptionPresent", param);
    vap_info->u.bss_info.interworking.interworking.hessOptionPresent = (param->type & cJSON_True) ? true:false;

    validate_param_string(interworking, "HESSID", param);
    rc = strcpy_s(vap_info->u.bss_info.interworking.interworking.hessid, sizeof(vap_info->u.bss_info.interworking.interworking.hessid), param->valuestring);
    ERR_CHK(rc);
    if (CosaDmlWiFi_IsValidMacAddr(vap_info->u.bss_info.interworking.interworking.hessid) != TRUE) {
        wifi_passpoint_dbg_print("%s:%d: Validation failed for HESSID\n", __func__, __LINE__);
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid HESSID");
        return RETURN_ERR;
    }

    validate_param_object(interworking, "Venue", venue);
        
    validate_param_integer(venue, "VenueType", param);
    vap_info->u.bss_info.interworking.interworking.venueType = param->valuedouble;
    if (vap_info->u.bss_info.interworking.interworking.venueType > 15) {
        wifi_passpoint_dbg_print("%s:%d: Validation failed for VenueGroup\n", __func__, __LINE__);
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Venue Group");
	return RETURN_ERR;
    } 
	
    validate_param_integer(venue, "VenueGroup", param);
    vap_info->u.bss_info.interworking.interworking.venueGroup = param->valuedouble;
    if (vap_info->u.bss_info.interworking.interworking.venueGroup > 11) {
        wifi_passpoint_dbg_print("%s:%d: Validation failed for VenueGroup\n", __func__, __LINE__);	
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Venue Group");
	return RETURN_ERR;
    } 

    switch (vap_info->u.bss_info.interworking.interworking.venueGroup) {
	case 0:
	    if (vap_info->u.bss_info.interworking.interworking.venueType > 0) {
        	wifi_passpoint_dbg_print("%s:%d: Validation failed for VenueGroup and VenueType\n", __func__, __LINE__);	
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Venue Group and type combination");
		return RETURN_ERR;
	    }
	    break;
		
	case 1:
	    if (vap_info->u.bss_info.interworking.interworking.venueType > 15) {
        	wifi_passpoint_dbg_print("%s:%d: Validation failed for VenueGroup and VenueType\n", __func__, __LINE__);	
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Venue Group and type combination");
		return RETURN_ERR;
	    }
	    break;
		
	case 2:
	    if (vap_info->u.bss_info.interworking.interworking.venueType > 9) {
        	wifi_passpoint_dbg_print("%s:%d: Validation failed for VenueGroup and VenueType\n", __func__, __LINE__);
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Venue Group and type combination");
		return RETURN_ERR;
	    }
	    break;
		
	case 3:
	    if (vap_info->u.bss_info.interworking.interworking.venueType > 3) {
        	wifi_passpoint_dbg_print("%s:%d: Validation failed for VenueGroup and VenueType\n", __func__, __LINE__);
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Venue Group and type combination");
		return RETURN_ERR;
	    }
	    break;
		
	case 4:
	    if (vap_info->u.bss_info.interworking.interworking.venueType > 1) {
        	wifi_passpoint_dbg_print("%s:%d: Validation failed for VenueGroup and VenueType\n", __func__, __LINE__);
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Venue Group and type combination");
		return RETURN_ERR;
	    }
	    break;
		
	case 5:
	    if (vap_info->u.bss_info.interworking.interworking.venueType > 5) {
        	wifi_passpoint_dbg_print("%s:%d: Validation failed for VenueGroup and VenueType\n", __func__, __LINE__);
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Venue Group and type combination");
		return RETURN_ERR;
	    }
	    break;
		
	case 6:
	    if (vap_info->u.bss_info.interworking.interworking.venueType > 5) {
        	wifi_passpoint_dbg_print("%s:%d: Validation failed for VenueGroup and VenueType\n", __func__, __LINE__);
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Venue Group and type combination");
		return RETURN_ERR;
	    }
	    break;
		
	case 7:
	    if (vap_info->u.bss_info.interworking.interworking.venueType > 4) {
        	wifi_passpoint_dbg_print("%s:%d: Validation failed for VenueGroup and VenueType\n", __func__, __LINE__);	
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Venue Group and type combination");
		return RETURN_ERR;
	    }
	    break;
		
	case 8:
	    if (vap_info->u.bss_info.interworking.interworking.venueType > 0) {
                wifi_passpoint_dbg_print("%s:%d: Validation failed for VenueGroup and VenueType\n", __func__, __LINE__);
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Venue Group and type combination");
		return RETURN_ERR;
	    }
	    break;
		
	case 9:
	    if (vap_info->u.bss_info.interworking.interworking.venueType > 0) {
        	wifi_passpoint_dbg_print("%s:%d: Validation failed for VenueGroup and VenueType\n", __func__, __LINE__);
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Venue Group and type combination");
		return RETURN_ERR;
            }
	    break;
		
	case 10:
	    if (vap_info->u.bss_info.interworking.interworking.venueType > 7) {
        	wifi_passpoint_dbg_print("%s:%d: Validation failed for VenueGroup and VenueType\n", __func__, __LINE__);
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Venue Group and type combination");
		return RETURN_ERR;
	    }
	    break;
		
	case 11:
	    if (vap_info->u.bss_info.interworking.interworking.venueType > 6) {
        	wifi_passpoint_dbg_print("%s:%d: Validation failed for VenueGroup and VenueType\n", __func__, __LINE__);
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Venue Group and type combination");
		return RETURN_ERR;
	    }
	    break;
    }

    validate_param_object(interworking, "ANQP",anqp);

    if (validate_anqp(anqp, &vap_info->u.bss_info.interworking, execRetVal) != 0) {
        wifi_passpoint_dbg_print("%s:%d: Validation failed\n", __func__, __LINE__);
	    return RETURN_ERR;
    } else {
        cJSON *anqpString = cJSON_CreateObject();
        cJSON_AddItemReferenceToObject(anqpString, "ANQP", (cJSON *)anqp);
        cJSON_PrintPreallocated(anqpString, (char *)&vap_info->u.bss_info.interworking.anqp.anqpParameters, sizeof(vap_info->u.bss_info.interworking.anqp.anqpParameters),false);
        cJSON_Delete(anqpString);
    }

    validate_param_object(interworking, "Passpoint",passpoint);
	
    if (validate_passpoint(passpoint, &vap_info->u.bss_info.interworking, execRetVal) != 0) {
        wifi_passpoint_dbg_print("%s:%d: Validation failed\n", __func__, __LINE__);
        return RETURN_ERR;
    } else {
        cJSON *hs2String = cJSON_CreateObject();
        cJSON_AddItemReferenceToObject(hs2String, "Passpoint", (cJSON *)passpoint);
        cJSON_PrintPreallocated(hs2String, (char *)&vap_info->u.bss_info.interworking.passpoint.hs2Parameters, sizeof(vap_info->u.bss_info.interworking.passpoint.hs2Parameters),false);
        cJSON_Delete(hs2String);
    }

    return RETURN_OK;
}

int validate_radius_settings(const cJSON *radius, wifi_vap_info_t *vap_info, pErr execRetVal)
{
	const cJSON *param;
    errno_t rc = -1;

        if(!radius || !vap_info || !execRetVal){
            wifi_passpoint_dbg_print("Radius entry is NULL\n");
            return RETURN_ERR;
        }

	validate_param_string(radius, "RadiusServerIPAddr", param);
	if (validate_ipv4_address(param->valuestring) != RETURN_OK) {
            wifi_passpoint_dbg_print("%s:%d: Validation failed for RadiusServerIPAddr\n", __func__, __LINE__);	
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Radius server IP");
		return RETURN_ERR;
	}
#ifndef WIFI_HAL_VERSION_3_PHASE2
	rc = strcpy_s((char *)vap_info->u.bss_info.security.u.radius.ip, sizeof(vap_info->u.bss_info.security.u.radius.ip), param->valuestring);
    ERR_CHK(rc);
#else
    /* check the INET family and update the radius ip address */
    if(inet_pton(AF_INET, param->valuestring, &(vap_info->u.bss_info.security.u.radius.ip.u.IPv4addr)) > 0) {
       vap_info->u.bss_info.security.u.radius.ip.family = wifi_ip_family_ipv4;
    } else if(inet_pton(AF_INET6, param->valuestring, &(vap_info->u.bss_info.security.u.radius.ip.u.IPv6addr)) > 0) {
       vap_info->u.bss_info.security.u.radius.ip.family = wifi_ip_family_ipv6;
    } else {
       CcspTraceError(("<%s> <%d> : inet_pton falied for primary radius IP\n", __FUNCTION__, __LINE__));
       return RETURN_ERR;
    }
#endif

	validate_param_integer(radius, "RadiusServerPort", param);
	vap_info->u.bss_info.security.u.radius.port = param->valuedouble;

	validate_param_string(radius, "RadiusSecret", param);
	rc = strcpy_s(vap_info->u.bss_info.security.u.radius.key, sizeof(vap_info->u.bss_info.security.u.radius.key), param->valuestring);
    ERR_CHK(rc);

	validate_param_string(radius, "SecondaryRadiusServerIPAddr", param);
	if (validate_ipv4_address(param->valuestring) != RETURN_OK) {
            wifi_passpoint_dbg_print("%s:%d: Validation failed for SecondaryRadiusServerIPAddr\n", __func__, __LINE__);
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Secondary Radius server IP");
		return RETURN_ERR;
	}
#ifndef WIFI_HAL_VERSION_3_PHASE2
        rc = strcpy_s((char *)vap_info->u.bss_info.security.u.radius.s_ip, sizeof(vap_info->u.bss_info.security.u.radius.s_ip), param->valuestring);
        ERR_CHK(rc);
#else
        /* check the INET family and update the radius ip address */
        if(inet_pton(AF_INET, param->valuestring, &(vap_info->u.bss_info.security.u.radius.s_ip.u.IPv4addr)) > 0) {
           vap_info->u.bss_info.security.u.radius.s_ip.family = wifi_ip_family_ipv4;
        } else if(inet_pton(AF_INET6, param->valuestring, &(vap_info->u.bss_info.security.u.radius.s_ip.u.IPv6addr)) > 0) {
           vap_info->u.bss_info.security.u.radius.s_ip.family = wifi_ip_family_ipv6;
        } else {
          CcspTraceError(("<%s> <%d> : inet_pton falied for primary radius IP\n", __FUNCTION__, __LINE__));
          return RETURN_ERR;
        }
#endif

	validate_param_integer(radius, "SecondaryRadiusServerPort", param);
	vap_info->u.bss_info.security.u.radius.s_port = param->valuedouble;
	validate_param_string(radius, "SecondaryRadiusSecret", param);
	rc = strcpy_s(vap_info->u.bss_info.security.u.radius.s_key, sizeof(vap_info->u.bss_info.security.u.radius.s_key), param->valuestring);
    ERR_CHK(rc);
	
	return RETURN_OK;

}

int validate_enterprise_security(const cJSON *security, wifi_vap_info_t *vap_info, pErr execRetVal)
{
	const cJSON *param;

        if(!security || !vap_info || !execRetVal){
            wifi_passpoint_dbg_print("Interworking entry is NULL\n");
            return RETURN_ERR;
        }
        validate_param_string(security, "Mode", param);
        if ((strcmp(param->valuestring, "WPA2-Enterprise") != 0) && (strcmp(param->valuestring, "WPA-WPA2-Enterprise") != 0)) {
          wifi_passpoint_dbg_print("%s:%d: Xfinity WiFi VAP security is not WPA2 Eneterprise, value:%s\n", 
                                   __func__, __LINE__, param->valuestring);
          snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid sec mode for hotspot secure vap");
          return RETURN_ERR;
        }
        if (strcmp(param->valuestring, "WPA2-Enterprise") == 0) { 
          vap_info->u.bss_info.security.mode = wifi_security_mode_wpa2_enterprise;
        } else {
          vap_info->u.bss_info.security.mode = wifi_security_mode_wpa_wpa2_enterprise;
        }
        validate_param_string(security, "EncryptionMethod", param);
        if ((strcmp(param->valuestring, "AES") != 0) && (strcmp(param->valuestring, "AES+TKIP") != 0)) {
          wifi_passpoint_dbg_print("%s:%d: Xfinity WiFi VAP Encrytpion mode is Invalid:%s\n", 
                                   __func__, __LINE__, param->valuestring);
          snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid enc mode for hotspot secure vap");
          return RETURN_ERR;
        }
        if (strcmp(param->valuestring, "AES") == 0) {
          vap_info->u.bss_info.security.encr = wifi_encryption_aes;	
        } else {
          vap_info->u.bss_info.security.encr = wifi_encryption_aes_tkip;
        }
        // MFPConfig
        validate_param_string(security, "MFPConfig", param);
        if ((strcmp(param->valuestring, "Disabled") != 0) 
             && (strcmp(param->valuestring, "Required") != 0) 
             && (strcmp(param->valuestring, "Optional") != 0)) {
                wifi_passpoint_dbg_print("%s:%d: MFPConfig not valid, value:%s\n",
                        __func__, __LINE__, param->valuestring);
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid  MFPConfig for hotspot secure vap");
                return RETURN_ERR;
        }
#if defined(WIFI_HAL_VERSION_3)
        if (strstr(param->valuestring, "Disabled")) {
            vap_info->u.bss_info.security.mfp = wifi_mfp_cfg_disabled;
        } else if (strstr(param->valuestring, "Required")) {
            vap_info->u.bss_info.security.mfp = wifi_mfp_cfg_required;
        } else if (strstr(param->valuestring, "Optional")) {
            vap_info->u.bss_info.security.mfp = wifi_mfp_cfg_optional;
        }
#else
        errno_t rc = -1;
        rc = strcpy_s(vap_info->u.bss_info.security.mfpConfig, sizeof(vap_info->u.bss_info.security.mfpConfig), param->valuestring);
        ERR_CHK(rc);
#endif
	
        validate_param_object(security, "RadiusSettings",param);

	if (validate_radius_settings(param, vap_info, execRetVal) != 0) {
        wifi_passpoint_dbg_print("%s:%d: Validation failed\n", __func__, __LINE__);
		return RETURN_ERR;

	}

	return RETURN_OK;
}

int validate_personal_security(const cJSON *security, wifi_vap_info_t *vap_info, pErr execRetVal)
{
        if(!security || !vap_info || !execRetVal){
            wifi_passpoint_dbg_print("Interworking entry is NULL\n");
            return RETURN_ERR;
        }
        
        const cJSON *param;
        validate_param_string(security, "EncryptionMethod", param);

        if (strcmp(param->valuestring, "TKIP") == 0) {
            vap_info->u.bss_info.security.encr = wifi_encryption_tkip;
        } else if(strcmp(param->valuestring, "AES") == 0) {
            vap_info->u.bss_info.security.encr = wifi_encryption_aes;
        } else if(strcmp(param->valuestring, "AES+TKIP") == 0) {
            vap_info->u.bss_info.security.encr = wifi_encryption_aes_tkip;
        } else {
            CcspTraceError(("%s: Invalid Encryption method for private vap\n", __FUNCTION__));
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Encryption method");
            return RETURN_ERR;
        }

        if ((vap_info->u.bss_info.security.mode == wifi_security_mode_wpa_wpa2_personal) &&
            (vap_info->u.bss_info.security.encr == wifi_encryption_tkip)) {
            CcspTraceError(("%s: Invalid Encryption method combination for private vap\n",__FUNCTION__));
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Encryption method combinaiton");
            return RETURN_ERR;
        }

        validate_param_string(security, "Passphrase", param);

        if ((strlen(param->valuestring) < MIN_PWD_LEN) || (strlen(param->valuestring) > MAX_PWD_LEN)) {
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Key passphrase length");
            CcspTraceError(("%s: Invalid Key passphrase length\n",__FUNCTION__));
            return RETURN_ERR;
        }
        strncpy(vap_info->u.bss_info.security.u.key.key, param->valuestring,
                sizeof(vap_info->u.bss_info.security.u.key.key) - 1);

        return RETURN_OK;
}

int validate_ssid_name(char *ssid_name, pErr execRetVal) 
{
    int i =0, ssid_len;

    if(!ssid_name ||!execRetVal){
        wifi_passpoint_dbg_print("Interworking entry is NULL\n");
        return RETURN_ERR;
    }
        
    ssid_len = strlen(ssid_name);
    if ((ssid_len == 0) || (ssid_len > COSA_DML_WIFI_MAX_SSID_NAME_LEN)) {
        CcspTraceError(("%s: Invalid SSID size \n",__FUNCTION__));
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid SSID Size");
        return RETURN_ERR;
    }


    for (i = 0; i < ssid_len; i++) {
        if (!((ssid_name[i] >= ' ') && (ssid_name[i] <= '~'))) {
            CcspTraceError(("%s: Invalid character present in SSID Name \n",__FUNCTION__));
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid character in SSID");
            return RETURN_ERR;
        }
    }
    return RETURN_OK;
}

int validate_xfinity_secure_vap(const cJSON *vap, wifi_vap_info_t *vap_info, pErr execRetVal)
{
        if(!vap || !vap_info || !execRetVal){
            wifi_passpoint_dbg_print("VAP entry is NULL\n");
            return RETURN_ERR;
        }
        
	const cJSON *security, *interworking;

	validate_param_object(vap, "Security",security);
	if (vap_info->u.bss_info.enabled) {
            if (validate_enterprise_security(security, vap_info, execRetVal) != RETURN_OK) {
            wifi_passpoint_dbg_print("%s:%d: Validation failed\n", __func__, __LINE__);
                    return RETURN_ERR;
            }
        }
	validate_param_object(vap, "Interworking",interworking);

	if (validate_interworking(interworking, vap_info, execRetVal) != RETURN_OK) {
            return RETURN_ERR;
	}

	return RETURN_OK;
}

int validate_xfinity_open_vap(const cJSON *vap, wifi_vap_info_t *vap_info, pErr execRetVal)
{
        const cJSON *security, *param, *interworking;
        
        validate_param_object(vap, "Security",security);

        validate_param_string(security, "Mode", param);
        if (strcmp(param->valuestring, "None") != 0) {
            CcspTraceError(("%s:Xfinity open security is not OPEN\n", __FUNCTION__));
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid security for hotspot open vap");
            return RETURN_ERR;
        }
        vap_info->u.bss_info.security.mode = wifi_security_mode_none;
        
        // MFPConfig
        validate_param_string(security, "MFPConfig", param);
        if ((strcmp(param->valuestring, "Disabled") != 0) 
             && (strcmp(param->valuestring, "Required") != 0) 
             && (strcmp(param->valuestring, "Optional") != 0)) {
                wifi_passpoint_dbg_print("%s:%d: MFPConfig not valid, value:%s\n",
                        __func__, __LINE__, param->valuestring);
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid  MFPConfig for hotspot secure vap");
                return RETURN_ERR;
        }
#if defined(WIFI_HAL_VERSION_3)
        if (strstr(param->valuestring, "Disabled")) {
            vap_info->u.bss_info.security.mfp = wifi_mfp_cfg_disabled;
        } else if (strstr(param->valuestring, "Required")) {
            vap_info->u.bss_info.security.mfp = wifi_mfp_cfg_required;
        } else if (strstr(param->valuestring, "Optional")) {
            vap_info->u.bss_info.security.mfp = wifi_mfp_cfg_optional;
        }
#else
        errno_t rc = -1;
        rc = strcpy_s(vap_info->u.bss_info.security.mfpConfig, sizeof(vap_info->u.bss_info.security.mfpConfig), param->valuestring);
        ERR_CHK(rc);
#endif

        validate_param_object(vap, "Interworking",interworking);

        if (validate_interworking(interworking, vap_info, execRetVal) != RETURN_OK) {
            return RETURN_ERR;
        }

        if (vap_info->u.bss_info.interworking.passpoint.enable) {
            CcspTraceError(("%s:Passpoint cannot be enabled on hotspot open vap\n", __FUNCTION__));
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Passpoint cannot be enabled on hotspot open vap");
            return RETURN_ERR;
        }

	return RETURN_OK;
}

int validate_private_vap(const cJSON *vap, wifi_vap_info_t *vap_info, pErr execRetVal)
{
        const cJSON *security, *param, *interworking;

        validate_param_object(vap, "Security",security);

        validate_param_string(security, "Mode", param);

        if (strcmp(param->valuestring, "None") == 0) {
            vap_info->u.bss_info.security.mode = wifi_security_mode_none;
        } else if (strcmp(param->valuestring, "WPA-Personal") == 0) {
            vap_info->u.bss_info.security.mode = wifi_security_mode_wpa_personal;
        } else if (strcmp(param->valuestring, "WPA2-Personal") == 0) {
            vap_info->u.bss_info.security.mode = wifi_security_mode_wpa2_personal;
        } else if (strcmp(param->valuestring, "WPA-WPA2-Personal") == 0) {
            vap_info->u.bss_info.security.mode = wifi_security_mode_wpa_wpa2_personal;
#ifdef WIFI_HAL_VERSION_3
        } else if (strcmp(param->valuestring, "WPA3-Personal") == 0) {
            vap_info->u.bss_info.security.mode = wifi_security_mode_wpa3_personal;
            vap_info->u.bss_info.security.u.key.type = wifi_security_key_type_sae;
        } else if (strcmp(param->valuestring, "WPA3-Personal-Transition") == 0) {
            vap_info->u.bss_info.security.mode = wifi_security_mode_wpa3_transition;
            vap_info->u.bss_info.security.u.key.type = wifi_security_key_type_psk_sae;
#endif
        } else {
            CcspTraceError(("%s: Invalid Authentication mode for private vap", __FUNCTION__));
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Authentication mode for private vap");
            return RETURN_ERR;
        }


        // MFPConfig
        validate_param_string(security, "MFPConfig", param);
        if ((strcmp(param->valuestring, "Disabled") != 0) 
             && (strcmp(param->valuestring, "Required") != 0) 
             && (strcmp(param->valuestring, "Optional") != 0)) {
                wifi_passpoint_dbg_print("%s:%d: MFPConfig not valid, value:%s\n",
                        __func__, __LINE__, param->valuestring);
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid  MFPConfig for hotspot secure vap");
                return RETURN_ERR;
        }
#if defined(WIFI_HAL_VERSION_3)
        if (strstr(param->valuestring, "Disabled")) {
            vap_info->u.bss_info.security.mfp = wifi_mfp_cfg_disabled;
        } else if (strstr(param->valuestring, "Required")) {
            vap_info->u.bss_info.security.mfp = wifi_mfp_cfg_required;
        } else if (strstr(param->valuestring, "Optional")) {
            vap_info->u.bss_info.security.mfp = wifi_mfp_cfg_optional;
        }
#else
        errno_t rc = -1;
        rc = strcpy_s(vap_info->u.bss_info.security.mfpConfig, sizeof(vap_info->u.bss_info.security.mfpConfig), param->valuestring);
        ERR_CHK(rc);
#endif

        if ((vap_info->u.bss_info.security.mode != wifi_security_mode_none) &&
            (validate_personal_security(security, vap_info, execRetVal) != RETURN_OK)) {
            CcspTraceError(("%s: Failed to validate security for vap %s", __FUNCTION__, vap_info->vap_name));
            return RETURN_ERR;
        } 
         
        validate_param_object(vap, "Interworking",interworking);

        if (validate_interworking(interworking, vap_info, execRetVal) != RETURN_OK) {
            return RETURN_ERR;
        }

        if (vap_info->u.bss_info.interworking.passpoint.enable) {
            CcspTraceError(("%s:Passpoint cannot be enabled on private vap\n", __FUNCTION__));
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Passpoint cannot be enabled on private vap");
            return RETURN_ERR;
        }

	return RETURN_OK;
}

int validate_xhome_vap(const cJSON *vap, wifi_vap_info_t *vap_info, pErr execRetVal)
{
        const cJSON *security, *param, *interworking;

        validate_param_object(vap, "Security",security);

        validate_param_string(security, "Mode", param);

        if (strcmp(param->valuestring, "None") == 0) {
            vap_info->u.bss_info.security.mode = wifi_security_mode_none;
        } else if (strcmp(param->valuestring, "WPA-Personal") == 0) {
            vap_info->u.bss_info.security.mode = wifi_security_mode_wpa_personal;
        } else if (strcmp(param->valuestring, "WPA2-Personal") == 0) {
            vap_info->u.bss_info.security.mode = wifi_security_mode_wpa2_personal;
        } else if (strcmp(param->valuestring, "WPA-WPA2-Personal") == 0) {
            vap_info->u.bss_info.security.mode = wifi_security_mode_wpa_wpa2_personal;
#if defined(WIFI_HAL_VERSION_3)
        } else if (strcmp(param->valuestring, "WPA3-Personal") == 0) {
            vap_info->u.bss_info.security.mode = wifi_security_mode_wpa3_personal;
            vap_info->u.bss_info.security.u.key.type = wifi_security_key_type_sae;
        } else if (strcmp(param->valuestring, "WPA3-Personal-Transition") == 0) {
            vap_info->u.bss_info.security.mode = wifi_security_mode_wpa3_transition;
            vap_info->u.bss_info.security.u.key.type = wifi_security_key_type_psk_sae;
#endif
        } else {
            CcspTraceError(("%s: Invalid Authentication mode for vap %s", __FUNCTION__, vap_info->vap_name));
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid Authentication mode for xhome vap");
            return RETURN_ERR;
        }

        // MFPConfig
        validate_param_string(security, "MFPConfig", param);
        if ((strcmp(param->valuestring, "Disabled") != 0)
             && (strcmp(param->valuestring, "Required") != 0)
             && (strcmp(param->valuestring, "Optional") != 0)) {
                wifi_passpoint_dbg_print("%s:%d: MFPConfig not valid, value:%s\n",
                        __func__, __LINE__, param->valuestring);
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid  MFPConfig for hotspot secure vap");
                return RETURN_ERR;
        }
#if defined(WIFI_HAL_VERSION_3)
        if (strstr(param->valuestring, "Disabled")) {
            vap_info->u.bss_info.security.mfp = wifi_mfp_cfg_disabled;
        } else if (strstr(param->valuestring, "Required")) {
            vap_info->u.bss_info.security.mfp = wifi_mfp_cfg_required;
        } else if (strstr(param->valuestring, "Optional")) {
            vap_info->u.bss_info.security.mfp = wifi_mfp_cfg_optional;
        }
#else
        errno_t rc = -1;
        rc = strcpy_s(vap_info->u.bss_info.security.mfpConfig, sizeof(vap_info->u.bss_info.security.mfpConfig), param->valuestring);
        ERR_CHK(rc);
#endif

        if ((vap_info->u.bss_info.security.mode != wifi_security_mode_none) &&
            (validate_personal_security(security, vap_info, execRetVal) != RETURN_OK)) {
            CcspTraceError(("%s: Failed to validate security for vap %s", __FUNCTION__, vap_info->vap_name));
            return RETURN_ERR;
        }

        validate_param_object(vap, "Interworking",interworking);

        if (validate_interworking(interworking, vap_info, execRetVal) != RETURN_OK) {
            return RETURN_ERR;
        }

        if (vap_info->u.bss_info.interworking.passpoint.enable) {
            CcspTraceError(("%s:Passpoint cannot be enabled on private vap\n", __FUNCTION__));
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Passpoint cannot be enabled on private vap");
            return RETURN_ERR;
        }

	return RETURN_OK;
}

int validate_vap(const cJSON *vap, wifi_vap_info_t *vap_info, pErr execRetVal)
{
	const cJSON  *param;
	int ret=RETURN_OK;
    errno_t rc = -1;

    //VAP Name
	validate_param_string(vap, "VapName",param);
	rc = strcpy_s(vap_info->vap_name, sizeof(vap_info->vap_name), param->valuestring);
    ERR_CHK(rc);

    // Enabled
    validate_param_bool(vap, "Enabled", param);
    vap_info->u.bss_info.enabled = (param->type & cJSON_True) ? true:false;

	// SSID
    if (vap_info->u.bss_info.enabled) {
        validate_param_string(vap, "SSID", param);
	rc = strcpy_s(vap_info->u.bss_info.ssid, sizeof(vap_info->u.bss_info.ssid), param->valuestring);
    ERR_CHK(rc);

        if (validate_ssid_name(vap_info->u.bss_info.ssid, execRetVal) != RETURN_OK) {
            CcspTraceError(("%s : Ssid name validation failed for %s\n",__FUNCTION__, vap_info->vap_name));
            return RETURN_ERR;
        } 
    }
    // Broadcast SSID
	validate_param_bool(vap, "SSIDAdvertisementEnabled", param);
	vap_info->u.bss_info.showSsid = (param->type & cJSON_True) ? true:false;

	// Isolation
	validate_param_bool(vap, "IsolationEnable", param);
	vap_info->u.bss_info.isolation = (param->type & cJSON_True) ? true:false;

	// ManagementFramePowerControl
	validate_param_integer(vap, "ManagementFramePowerControl", param);
	vap_info->u.bss_info.mgmtPowerControl = param->valuedouble;

	// BssMaxNumSta
	validate_param_integer(vap, "BssMaxNumSta", param);
	vap_info->u.bss_info.bssMaxSta = param->valuedouble;

        // BSSTransitionActivated
        validate_param_bool(vap, "BSSTransitionActivated", param);
        vap_info->u.bss_info.bssTransitionActivated = (param->type & cJSON_True) ? true:false;

        // NeighborReportActivated
        validate_param_bool(vap, "NeighborReportActivated", param);
        vap_info->u.bss_info.nbrReportActivated = (param->type & cJSON_True) ? true:false;

        // RapidReconnCountEnable
        validate_param_bool(vap, "RapidReconnCountEnable", param);
        vap_info->u.bss_info.rapidReconnectEnable = (param->type & cJSON_True) ? true:false;

	// RapidReconnThreshold
	validate_param_integer(vap, "RapidReconnThreshold", param);
	vap_info->u.bss_info.rapidReconnThreshold = param->valuedouble;

        // VapStatsEnable
        validate_param_bool(vap, "VapStatsEnable", param);
        vap_info->u.bss_info.vapStatsEnable = (param->type & cJSON_True) ? true:false;

#ifdef WIFI_HAL_VERSION_3
        UINT apIndex = 0;
        if ( (getVAPIndexFromName(vap_info->vap_name, &apIndex) == ANSC_STATUS_SUCCESS))
        {
            vap_info->vap_index = apIndex;
            if (isVapHotspot(apIndex))
            {
                if (isVapHotspotSecure(apIndex))
                {
                   ret = validate_xfinity_secure_vap(vap, vap_info, execRetVal);
                }
                else
                {
                    ret = validate_xfinity_open_vap(vap, vap_info, execRetVal);
                }

            }else if(isVapPrivate(apIndex))
            {
                ret = validate_private_vap(vap, vap_info, execRetVal);
            }
            else if (isVapXhs(apIndex))
            {
                ret = validate_xhome_vap(vap, vap_info, execRetVal);
            }
        }
#else
	if (strcmp(vap_info->vap_name, "hotspot_open_2g") == 0) {
		vap_info->vap_index = 4;
		ret = validate_xfinity_open_vap(vap, vap_info, execRetVal);
	} else if (strcmp(vap_info->vap_name, "hotspot_secure_2g") == 0) {
		vap_info->vap_index = 8;
		ret = validate_xfinity_secure_vap(vap, vap_info, execRetVal);
	} else if (strcmp(vap_info->vap_name, "hotspot_open_5g") == 0) {
		vap_info->vap_index = 5;
		ret = validate_xfinity_open_vap(vap, vap_info, execRetVal);
	} else if (strcmp(vap_info->vap_name, "hotspot_secure_5g") == 0) {
		vap_info->vap_index = 9;
		ret = validate_xfinity_secure_vap(vap, vap_info, execRetVal);
	} else if (strcmp(vap_info->vap_name, "private_ssid_2g") == 0) {
		vap_info->vap_index = 0;
		ret = validate_private_vap(vap, vap_info, execRetVal);
	} else if (strcmp(vap_info->vap_name, "private_ssid_5g") == 0) {
		vap_info->vap_index = 1;
		ret = validate_private_vap(vap, vap_info, execRetVal);
	} else if (strcmp(vap_info->vap_name, "iot_ssid_2g") == 0) {
		vap_info->vap_index = 2;
		ret = validate_xhome_vap(vap, vap_info, execRetVal);
	} else if (strcmp(vap_info->vap_name, "iot_ssid_5g") == 0) {
		vap_info->vap_index = 3;
		ret = validate_xhome_vap(vap, vap_info, execRetVal);
	} 
#endif

    else {
                CcspTraceError(("%s Validation failed: Invalid vap name",__FUNCTION__));
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid vap name");
		return RETURN_ERR;
	}

        if (ret != RETURN_OK) {
            CcspTraceError(("%s Validation of %s vap failed\n", __FUNCTION__, vap_info->vap_name));
        } 
	return ret;
}

#ifdef FEATURE_RADIO_WEBCONFIG
int validate_wifi_radio_config(const cJSON *radio, wifi_radio_operationParam_t *radio_info, pErr execRetVal)
{
        if (!radio || !radio_info || !execRetVal) {
            CcspTraceError(("%s: Wifi Radio entry is NULL\n", __FUNCTION__));
            return RETURN_ERR;
        }

        const cJSON  *param;
        errno_t rc = -1;
        wifi_radio_index_t radio_index = 0;
        char temp_radio_name[64] = {0};
        UINT temp_band = 0, i = 0;
        int ret = RETURN_ERR;
#ifdef WIFI_HAL_VERSION_3
        wifi_radio_operationParam_t *wifiRadioOperParam = NULL;
#endif
        //Copy RadioName
        validate_param_string(radio, "RadioName", param);
        rc = strcpy_s(temp_radio_name, sizeof(temp_radio_name), param->valuestring);
        ERR_CHK(rc);

        //Copy FreqBand
        validate_param_integer(radio, "FreqBand", param);
        temp_band = param->valuedouble;

        //Index
        ret = getRadioIndexFromRadioName(temp_radio_name, &radio_index);
        if ( (ret != RETURN_OK) ||  (radio_index <0) || (radio_index >= MAX_NUM_RADIOS) )
            return RETURN_ERR;

#ifdef WIFI_HAL_VERSION_3
        wifiRadioOperParam = getRadioOperationParam(radio_index);
        if (wifiRadioOperParam == NULL) {
            CcspTraceError(("%s Input radioIndex = %d not found for wifiRadioOperParam \n",__FUNCTION__, radio_index));
            return RETURN_ERR;
        }
        //Validate the band
        if ((wifiRadioOperParam->band != temp_band)) {
            CcspTraceError(("%s: Input radioIndex = %d Band=%d is  not found\n",__FUNCTION__, radio_index, temp_band));
            return RETURN_ERR;
        }
#else
        switch(radio_index)
        {
            case 0:
                if (temp_band != WIFI_FREQUENCY_2_4_BAND) {
                    CcspTraceInfo(("%s: Input radioIndex = %d Band=%d is  not found\n",__FUNCTION__, radio_index, temp_band));
                    return RETURN_ERR;
                }
                break;
            case 1:
                if (temp_band != WIFI_FREQUENCY_5_BAND) {
                    CcspTraceInfo(("%s: Input radioIndex = %d Band=%d is  not found\n",__FUNCTION__, radio_index, temp_band));
                    return RETURN_ERR;
                }
                break;
            default:
                CcspTraceInfo(("%s: Invalid radioIndex : %d \n",__FUNCTION__, radio_index));
                return RETURN_ERR;
        }
#endif
        //Check if two Radios of same band exists in the blob
        for (i = 0;i < MAX_NUM_RADIOS; i++) {
            if (temp_band == radio_info[i].band) {
                CcspTraceError(("%s: Two Radios of same band %d exists in the blob\n",__FUNCTION__, radio_info[i].band));
                snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "More than one existence of a Radio");
                return RETURN_ERR;
            }
        }
        // Update Enabled
        validate_param_bool(radio, "Enabled", param);
        radio_info[radio_index].enable = (param->type & cJSON_True) ? true:false;

        // Update Band
        radio_info[radio_index].band = temp_band;

        CcspTraceInfo(("%s Parsed Radio Index %d\n",__FUNCTION__, radio_index));

        return RETURN_OK;
}
#endif

int validate_gas_config(const cJSON *gas, wifi_GASConfiguration_t *gas_info, pErr execRetVal)
{
        if(!gas || !gas_info || !execRetVal){
            wifi_passpoint_dbg_print("GAS entry is NULL\n");
            return RETURN_ERR;
        }
        
        const cJSON  *param;
 
        //AdvertisementId
        validate_param_integer(gas, "AdvertisementId", param);
        gas_info->AdvertisementID = param->valuedouble;
        if (gas_info->AdvertisementID != 0) { //ANQP
            wifi_passpoint_dbg_print("Invalid Configuration. Only Advertisement ID 0 - ANQP is Supported\n");
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid AdvertisementId. Only ANQP(0) Supported");
            return RETURN_ERR;
        }
        
        // PauseForServerResp
        validate_param_bool(gas, "PauseForServerResp", param);
        gas_info->PauseForServerResponse = (param->type & cJSON_True) ? true:false;

        //ResponseTimeout
        validate_param_integer(gas, "RespTimeout", param);
        gas_info->ResponseTimeout = param->valuedouble;
        if ((gas_info->ResponseTimeout < 1000) || (gas_info->ResponseTimeout > 65535)) {
            wifi_passpoint_dbg_print("Invalid Configuration. ResponseTimeout should be between 1000 and 65535\n");
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid RespTimeout 1000..65535");
            return RETURN_ERR;
        }
        
        //ComebackDelay
        validate_param_integer(gas, "ComebackDelay", param);
        gas_info->ComeBackDelay = param->valuedouble;
        if (gas_info->ComeBackDelay > 65535) {
            wifi_passpoint_dbg_print("Invalid Configuration. ComeBackDelay should be between 0 and 65535\n");
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid ComebackDelay 0..65535");
            return RETURN_ERR;
        }
        
        //ResponseBufferingTime
        validate_param_integer(gas, "RespBufferTime", param);
        gas_info->ResponseBufferingTime = param->valuedouble;
        
        //QueryResponseLengthLimit
        validate_param_integer(gas, "QueryRespLengthLimit", param);
        gas_info->QueryResponseLengthLimit = param->valuedouble;
        if ((gas_info->QueryResponseLengthLimit < 1) || (gas_info->QueryResponseLengthLimit > 127)) {
            wifi_passpoint_dbg_print("Invalid Configuration. QueryResponseLengthLimit should be between 1 and 127\n");
            snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Invalid QueryRespLengthLimit 1..127");
            return RETURN_ERR;
        }

        return RETURN_OK;
}

int validate_wifi_config(const cJSON *wifi, wifi_config_t *wifi_info, pErr execRetVal)
{
    const cJSON  *param,*gas_entry;
    int ret;

    if(!wifi || !wifi_info || !execRetVal){
        wifi_passpoint_dbg_print("WiFi Global entry is NULL\n");
        return RETURN_ERR;
    }
                
    validate_param_array(wifi, "GASConfig", param);
    cJSON_ArrayForEach(gas_entry, param) {
        ret = validate_gas_config(gas_entry,&wifi_info->gas_config,execRetVal);
        if (ret != RETURN_OK) {
            CcspTraceError(("%s Validation of GAS Configuration Failed\n",__FUNCTION__));
            return RETURN_ERR;
        }
    } 
    return RETURN_OK;
}

int wifi_validate_config(const char *buff, wifi_config_t *wifi_config, wifi_vap_info_map_t *vap_map, pErr execRetVal) 
{
    const cJSON *vaps, *vap, *wifi;
    cJSON *root_json;
    const char *err = NULL;
    FILE *fpw = NULL;
    unsigned int i =0;

    if (!buff || !vap_map || !execRetVal) {
        return RETURN_ERR;
    }

    fpw = fopen("/tmp/wifiWebconf", "w+");
    if (fpw != NULL) {
        fputs(buff, fpw);
        fclose(fpw);
    }

    root_json = cJSON_Parse(buff);
    if(root_json == NULL) {
        CcspTraceError(("%s: Json parse fail\n", __FUNCTION__));
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Json parse fail");
	err = cJSON_GetErrorPtr();
	if (err) {
            CcspTraceError(("%s: Json parse error %s\n", __FUNCTION__, err));
	}
	return RETURN_ERR;
    }

    //Parse Wifi Global Config
    wifi = cJSON_GetObjectItem(root_json, "WifiConfig");
    if (wifi == NULL) {
        CcspTraceError(("%s: Getting WifiConfig json objet fail\n",__FUNCTION__));
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "WifiConfig object get fail");
	cJSON_Delete(root_json);
        err = cJSON_GetErrorPtr();
        if (err) {
            CcspTraceError(("%s: Json delete error %s\n",__FUNCTION__,err));
        }
	return RETURN_ERR;
    }

    if (validate_wifi_config(wifi, wifi_config, execRetVal) != RETURN_OK) {
        cJSON_Delete(root_json);
        return RETURN_ERR;
    }

    //Parse VAP Config
    vaps = cJSON_GetObjectItem(root_json, "WifiVapConfig");
    if (vaps == NULL) {
        CcspTraceError(("%s: Getting WifiVapConfig json objet fail\n",__FUNCTION__));
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "WifiVapConfig object get fail");
	cJSON_Delete(root_json);
        err = cJSON_GetErrorPtr();
        if (err) {
            CcspTraceError(("%s: Json delete error %s\n",__FUNCTION__,err));
        }
	return RETURN_ERR;
    }

    vap_map->num_vaps = 0;

    cJSON_ArrayForEach(vap, vaps) {
        if (validate_vap(vap, &vap_map->vap_array[vap_map->num_vaps], execRetVal) != RETURN_OK) {
            cJSON_Delete(root_json);
	    return RETURN_ERR;
        }
	for (i = 0;i < vap_map->num_vaps;i++) {
	    if (strcmp(vap_map->vap_array[vap_map->num_vaps].vap_name, 
		  vap_map->vap_array[i].vap_name) == 0) {
                CcspTraceError(("%s: Two vaps of same name %s exists in the blob\n",__FUNCTION__,
					vap_map->vap_array[vap_map->num_vaps].vap_name));
		snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "More than one existence of a VAP");
		return RETURN_ERR;
	    }
	}
        vap_map->num_vaps++;
    }

    cJSON_Delete(root_json);
	
    return RETURN_OK;
}

#ifdef FEATURE_RADIO_WEBCONFIG
int wifi_validate_radio_config(const char *buff, wifi_radio_operationParam_t *radio_map, pErr execRetVal)
{
    const cJSON *radios, *radio;
    cJSON *root_json;
    const char *err = NULL;
    FILE *fpw = NULL;

    if (!buff || !radio_map || !execRetVal) {
        return RETURN_ERR;
    }

    fpw = fopen("/tmp/wifiWebconf", "w+");
    if (fpw != NULL) {
        fputs(buff, fpw);
        fclose(fpw);
    }

    root_json = cJSON_Parse(buff);
    if(root_json == NULL) {
        CcspTraceError(("%s: Json parse fail\n", __FUNCTION__));
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "Json parse fail");
        err = cJSON_GetErrorPtr();
        if (err) {
                CcspTraceError(("%s: Json parse error %s\n", __FUNCTION__, err));
        }
        return RETURN_ERR;
    }

    //Parse Wifi Radio Config
    radios = cJSON_GetObjectItem(root_json, "WifiRadioConfig");
    if (radios == NULL) {
        CcspTraceError(("%s: Getting WifiRadioConfig json object fail\n",__FUNCTION__));
        snprintf(execRetVal->ErrorMsg, sizeof(execRetVal->ErrorMsg)-1, "%s", "WifiRadioConfig object get fail");
        cJSON_Delete(root_json);
        err = cJSON_GetErrorPtr();
        if (err) {
            CcspTraceError(("%s: Json delete error %s\n",__FUNCTION__,err));
        }
        return RETURN_ERR;
    }

    cJSON_ArrayForEach(radio, radios) {
        if (validate_wifi_radio_config(radio, radio_map, execRetVal)!= RETURN_OK) {
            CcspTraceError(("%s Validation of Wifi Radio Configuration Failed\n",__FUNCTION__));
            cJSON_Delete(root_json);
            return RETURN_ERR;
        }
    }

    cJSON_Delete(root_json);

    return RETURN_OK;
}
#endif