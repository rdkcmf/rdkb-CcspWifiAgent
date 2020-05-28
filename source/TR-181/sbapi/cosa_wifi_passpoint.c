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
#include "cosa_wifi_passpoint.h"
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

static COSA_DML_WIFI_GASSTATS gasStats[GAS_CFG_TYPE_SUPPORTED];
 
extern ANSC_HANDLE bus_handle;
extern char        g_Subsystem[32];

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
    fread(*buffer, sizeof(char), numbytes, infile);
    fclose(infile);
    return numbytes;
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
        gasConfig_struct.ResponseTimeout = gasParam ? gasParam->valuedouble : pGASconf->ResponseTimeout;
        gasParam = cJSON_GetObjectItem(gasEntry,"comebackDelay");
        gasConfig_struct.ComeBackDelay = gasParam ? gasParam->valuedouble : pGASconf->ComeBackDelay;
        gasParam = cJSON_GetObjectItem(gasEntry,"respBufferTime");
        gasConfig_struct.ResponseBufferingTime = gasParam ? gasParam->valuedouble : pGASconf->ResponseBufferingTime;
        gasParam = cJSON_GetObjectItem(gasEntry,"queryRespLengthLimit");
        gasConfig_struct.QueryResponseLengthLimit = gasParam ? gasParam->valuedouble : pGASconf->QueryResponseLengthLimit;
    }

#ifdef DUAL_CORE_XB3
    if(RETURN_OK == wifi_setGASConfiguration(gasConfig_struct.AdvertisementID, &gasConfig_struct)){
#endif //DUAL_CORE_XB3
        pGASconf->AdvertisementID = gasConfig_struct.AdvertisementID; 
        pGASconf->PauseForServerResponse = gasConfig_struct.PauseForServerResponse;
        pGASconf->ResponseTimeout = gasConfig_struct.ResponseTimeout;
        pGASconf->ComeBackDelay = gasConfig_struct.ComeBackDelay;
        pGASconf->ResponseBufferingTime = gasConfig_struct.ResponseBufferingTime;
        pGASconf->QueryResponseLengthLimit = gasConfig_struct.QueryResponseLengthLimit;
        cJSON_Delete(passPointCfg);
        return ANSC_STATUS_SUCCESS;
#ifdef DUAL_CORE_XB3
      }
#endif //DUAL_CORE_XB3
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

