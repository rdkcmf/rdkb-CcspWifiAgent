/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2015 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/**********************************************************************
   Copyright [2014] [Cisco Systems, Inc.]
 
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at
 
       http://www.apache.org/licenses/LICENSE-2.0
 
   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
**********************************************************************/


/**************************************************************************

    module: cosa_wifi_dml.c

        For COSA Data Model Library Development

    -------------------------------------------------------------------

    description:

        This file implementes back-end apis for the COSA Data Model Library

    -------------------------------------------------------------------

    environment:

        platform independent

    -------------------------------------------------------------------

    author:

        COSA XML TOOL CODE GENERATOR 1.0

    -------------------------------------------------------------------

    revision:

        01/18/2011    initial revision.

**************************************************************************/
#include "ctype.h"
#include "ansc_platform.h"
#include "cosa_wifi_dml.h"
#include "cosa_wifi_internal.h"
#include "plugin_main_apis.h"
#include "secure_wrapper.h"

extern void* g_pDslhDmlAgent;
static BOOL isHotspotSSIDIpdated = FALSE;//LNT_EMU
static int isAllow=0;
static int isChanged=0;
static    CHAR              Operation[20]="Allow-ALL";

 //PSM ACCESS- LNT_EMU
char *param_value;
char param_name[256] = {0};
extern ANSC_HANDLE bus_handle;//lnt
extern char g_Subsystem[32];//lnt
static char *BssSsid ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.SSID.%d.SSID";
static char *HideSsid ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.SSID.%d.HideSSID";
static char *Passphrase ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.SSID.%d.Passphrase";
static char *ChannelNumber ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.%d.Channel";
static BOOL isBeaconRateUpdate[16] = { FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE };
static BOOL AutoChannel_Enable_2G = true;
static BOOL AutoChannel_Enable_5G = true;
/***********************************************************************
 IMPORTANT NOTE:

 According to TR69 spec:
 On successful receipt of a SetParameterValues RPC, the CPE MUST apply 
 the changes to all of the specified Parameters atomically. That is, either 
 all of the value changes are applied together, or none of the changes are 
 applied at all. In the latter case, the CPE MUST return a fault response 
 indicating the reason for the failure to apply the changes. 
 
 The CPE MUST NOT apply any of the specified changes without applying all 
 of them.

 In order to set parameter values correctly, the back-end is required to
 hold the updated values until "Validate" and "Commit" are called. Only after
 all the "Validate" passed in different objects, the "Commit" will be called.
 Otherwise, "Rollback" will be called instead.

 The sequence in COSA Data Model will be:

 SetParamBoolValue/SetParamIntValue/SetParamUlongValue/SetParamStringValue
 -- Backup the updated values;

 if( Validate_XXX())
 {
     Commit_XXX();    -- Commit the update all together in the same object
 }
 else
 {
     Rollback_XXX();  -- Remove the update at backup;
 }
 
***********************************************************************/
//RDKB-EMU
unsigned long get_AutoChannelEnable_value(int InstanceNumber)
{
	FILE *fp = NULL;
	char path[256] = {0},channel_value[256] = {0};
	int count = 0;
	ulong channel = 0;
	if((InstanceNumber == 1) || (InstanceNumber == 5))
                fp = popen("cat /var/prevchanval2G_AutoChannelEnable","r");
        else if(InstanceNumber == 2)
                fp = popen("cat /var/prevchanval5G_AutoChannelEnable","r");
	if ( fp == NULL)
	{
		printf("Failed to run command in Function %s \n",__FUNCTION__);
		return 0;
	}
	fgets(path,sizeof(path),fp);
	for(count=0;path[count]!='\n';count++)
		channel_value[count] = path[count];
	channel_value[count] = '\0';
	channel = atol(channel_value);
	pclose(fp);
	return channel;
}

static ANSC_STATUS
GetInsNumsByWEPKey64(PCOSA_DML_WEPKEY_64BIT pWEPKey, ULONG *apIns, ULONG *wepKeyIdx)
{
    PCOSA_DATAMODEL_WIFI        pWiFi       = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PSINGLE_LINK_ENTRY          pAPLink     = NULL;
    PCOSA_CONTEXT_LINK_OBJECT   pAPLinkObj  = NULL;
    PCOSA_DML_WIFI_AP           pWiFiAP     = NULL;
    int                         i;

    /* for each Device.WiFi.AccessPoint.{i}. */
    for (   pAPLink = AnscSListGetFirstEntry(&pWiFi->AccessPointQueue);
            pAPLink != NULL;
            pAPLink = AnscSListGetNextEntry(pAPLink)
        )
    {
        pAPLinkObj = ACCESS_COSA_CONTEXT_LINK_OBJECT(pAPLink);
        if (!pAPLinkObj)
            continue;
        pWiFiAP = (PCOSA_DML_WIFI_AP)pAPLinkObj->hContext;

        /* for each Device.WiFi.AccessPoint.{i}.Security.X_CISCO_COM_WEPKey64Bit.{i}. */
        for (i = 0; i < COSA_DML_WEP_KEY_NUM; i++)
        {
            if ((ANSC_HANDLE)pWEPKey == (ANSC_HANDLE)&pWiFiAP->SEC.WEPKey64Bit[i])
            {
                /* found */
                *apIns = pWiFiAP->AP.Cfg.InstanceNumber;
                *wepKeyIdx = i;
                return ANSC_STATUS_SUCCESS;
            }
        }
    }
    return ANSC_STATUS_FAILURE;
}

static ANSC_STATUS
GetInsNumsByWEPKey128(PCOSA_DML_WEPKEY_128BIT pWEPKey, ULONG *apIns, ULONG *wepKeyIdx)
{
    PCOSA_DATAMODEL_WIFI        pWiFi       = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PSINGLE_LINK_ENTRY          pAPLink     = NULL;
    PCOSA_CONTEXT_LINK_OBJECT   pAPLinkObj  = NULL;
    PCOSA_DML_WIFI_AP           pWiFiAP     = NULL;
    int                         i;

    /* for each Device.WiFi.AccessPoint.{i}. */
    for (   pAPLink = AnscSListGetFirstEntry(&pWiFi->AccessPointQueue);
            pAPLink != NULL;
            pAPLink = AnscSListGetNextEntry(pAPLink)
        )
    {
        pAPLinkObj = ACCESS_COSA_CONTEXT_LINK_OBJECT(pAPLink);
        if (!pAPLinkObj)
            continue;
        pWiFiAP = (PCOSA_DML_WIFI_AP)pAPLinkObj->hContext;

        /* for each Device.WiFi.AccessPoint.{i}.Security.X_CISCO_COM_WEPKey128Bit.{i}. */
        for (i = 0; i < COSA_DML_WEP_KEY_NUM; i++)
        {
            if ((ANSC_HANDLE)pWEPKey == (ANSC_HANDLE)&pWiFiAP->SEC.WEPKey128Bit[i])
            {
                /* found */
                *apIns = pWiFiAP->AP.Cfg.InstanceNumber;
                *wepKeyIdx = i;
                return ANSC_STATUS_SUCCESS;
            }
        }
    }
    return ANSC_STATUS_FAILURE;
}

static BOOL IsSsidHotspot(ULONG ins)
{
    char rec[128];
    char *sval = NULL;
    BOOL bval;
    extern ANSC_HANDLE bus_handle;
    extern char        g_Subsystem[32];
    snprintf(rec, sizeof(rec), "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.HotSpot", (int)ins);
    if (PSM_Get_Record_Value2(bus_handle, g_Subsystem, rec, NULL, &sval) != CCSP_SUCCESS) {
        AnscTraceError(("%s: fail to get PSM record !\n", __FUNCTION__));
        return FALSE;
    }
    bval = (atoi(sval) == 1) ? TRUE : FALSE;
    ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(sval);
    return bval;
}
#define MAX_SUPP_STD_STR     5

/**********************************************************************  

    caller:     Radio_SetParamStringValue

    prototype: 

      static BOOL 
      ValidateOpStd
      (
          ULONG               sstd_bitmask, 
          const char          *pOpStd
      )

    description:

        Validates user input for wifi operting standards

    argument:  
          ULONG               sstd_bitmask, 
                Bitmask of supported standards

          const char          *pOpStd
                User input for operating standard


    return:     TRUE if user input is valid

**********************************************************************/
typedef struct _sstd_db_entry
{
    ULONG       entry_bitmask;
    char        entry_name[4];
} sstd_db_entry_t;

static BOOL ValidateOpStd(ULONG sstd_bitmask, const char *pOpStd)
{

    char *tok;
    char *end                         = NULL;
    int  idx                          = 0;
    int  sstd_arr_len                 = 0;
    char buf_op_str[16];
    char *supp_substring_arr[MAX_SUPP_STD_STR];

    sstd_db_entry_t sstd_db[MAX_SUPP_STD_STR] = {
        { COSA_DML_WIFI_STD_b,    "b" } ,
        { COSA_DML_WIFI_STD_g,    "g" } ,
        { COSA_DML_WIFI_STD_n,    "n" } ,
        { COSA_DML_WIFI_STD_a,    "a" } ,
        { COSA_DML_WIFI_STD_ac,   "ac" }
    };

    // 1. Build a list of supported standard strings
    memset(supp_substring_arr, 0, sizeof(supp_substring_arr));

    for (idx=0; idx < MAX_SUPP_STD_STR; idx++)
    {
        if (sstd_bitmask & sstd_db[idx].entry_bitmask)
            supp_substring_arr[sstd_arr_len++] = sstd_db[idx].entry_name;
    }

    // 2. Check user input string against the list

    // Get a working buffer first
    strncpy(buf_op_str, pOpStd, sizeof(buf_op_str));
    tok = strtok_r(buf_op_str, ",", &end);
    if (!tok)
    {
        return FALSE;
    }

    while (tok)
    {
        for (idx=0; idx < sstd_arr_len; idx++)
        {
            if (strcasecmp(tok, supp_substring_arr[idx]) == 0) {
                break;  //found
            }
        }
        // Check for lookup failure
        if (idx == sstd_arr_len)
        return FALSE;

        tok = strtok_r(NULL, ",", &end);
    }

    return TRUE;
}

/***********************************************************************

 APIs for Object:

    WiFi.

    *  WiFi_GetParamBoolValue
    *  WiFi_GetParamIntValue
    *  WiFi_GetParamUlongValue
    *  WiFi_GetParamStringValue

***********************************************************************/
/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        WiFi_GetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL*                       pBool
            );

    description:

        This function is called to retrieve Boolean parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL*                       pBool
                The buffer of returned boolean value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
WiFi_GetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL*                       pBool
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;

    if (AnscEqualString(ParamName, "X_CISCO_COM_FactoryReset", TRUE))
    {
        /* always return false when get */
        *pBool = FALSE;
        return TRUE;
    }

    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        WiFi_GetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                int*                        pInt
            );

    description:

        This function is called to retrieve integer parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                int*                        pInt
                The buffer of returned integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
WiFi_GetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int*                        pInt
    )
{
    /* check the parameter name and return the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        WiFi_GetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG*                      puLong
            );

    description:

        This function is called to retrieve ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG*                      puLong
                The buffer of returned ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
WiFi_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG*                      puLong
    )
{
    /* check the parameter name and return the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        WiFi_GetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pValue,
                ULONG*                      pUlSize
            );

    description:

        This function is called to retrieve string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pValue,
                The string value buffer;

                ULONG*                      pUlSize
                The buffer of length of string value;
                Usually size of 1023 will be used.
                If it's not big enough, put required size here and return 1;

    return:     0 if succeeded;
                1 if short of buffer size; (*pUlSize = required size)
                -1 if not supported.

**********************************************************************/
ULONG
WiFi_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
    char *binConf;
    ULONG binSize;
    char *base64Conf;

    if (!ParamName || !pValue || !pUlSize || *pUlSize < 1)
        return -1;

    /* at lest support 32786 after base64 config */
    if (*pUlSize < 32786)
    {
        *pUlSize = 32786;
        return 1;
    }

    if (!AnscEqualString(ParamName, "X_CISCO_COM_ConfigFileBase64", TRUE))
        return -1;
 
    /* 
     * let's translate config file to base64 format since 
     * XXX_SetParamStringValue only support ASCII string
     */
 
    /* note base64 need 4 bytes for every 3 octets, 
     * we use a smaller buffer to save config file.
     * and save one more byte for debbuging */
    binSize = (*pUlSize - 1) * 3 / 4;
    binConf = AnscAllocateMemory(binSize);
    if (binConf == NULL)
    {
        AnscTraceError(("%s: no memory\n", __FUNCTION__));
        return -1;
    }
   
    /* get binary (whatever format) config and it's size from backend */
    if (CosaDmlWiFi_GetConfigFile(binConf, &binSize) != ANSC_STATUS_SUCCESS)
        return -1;

    base64Conf = AnscBase64Encode(binConf, binSize);
    if (base64Conf == NULL)
    {
        AnscTraceError(("%s: base64 encoding error\n", __FUNCTION__));
        AnscFreeMemory(binConf);
        return -1;
    }

    snprintf(pValue, *pUlSize, "%s", base64Conf);

    AnscFreeMemory(base64Conf);
    AnscFreeMemory(binConf);

    return 0;
}

BOOL
WiFi_SetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL                        bValue
    )
{
    if(AnscEqualString(ParamName, "X_CISCO_COM_FactoryReset", TRUE))
    {
        CosaDmlWiFi_FactoryReset();
        return TRUE;
    }

    return FALSE;
}
static void wifiFactoryReset(void *frArgs)
{

        ULONG indexes=(ULONG)frArgs;
        ULONG radioIndex   =(indexes>>24) & 0xff;
        ULONG radioIndex_2 =(indexes>>16) & 0xff;
        ULONG apIndex      =(indexes>>8) & 0xff;
        ULONG apIndex_2    =indexes & 0xff;
	CosaDmlWiFi_FactoryReset();
}
BOOL
WiFi_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pString
    )
{
    char *binConf;
    int nRet=0;
    ULONG radioIndex=0, apIndex=0, radioIndex_2=0, apIndex_2=0;
    ULONG binSize;
    ULONG indexes=0;	

    if (!ParamName || !pString)
        return FALSE;

    if( AnscEqualString(ParamName, "X_CISCO_COM_FactoryResetRadioAndAp", TRUE))
    {
            printf("-- %s X_CISCO_COM_FactoryResetRadioAndAp %s\n", __func__, pString);
            if(!pString || strlen(pString)<3 || strchr(pString, ';')==NULL)
                    return FALSE;
            if(strchr(pString, ',')) { //1,2;1,2
                    nRet = _ansc_sscanf(pString, "%lu,%lu;%lu,%lu",  &radioIndex, &radioIndex_2, &apIndex, &apIndex_2);
                    if ( nRet != 4 || radioIndex>2 || radioIndex_2>2 || apIndex>16 || apIndex_2>16)
                            return FALSE;
            } else {
                    nRet = _ansc_sscanf(pString, "%lu;%lu",  &radioIndex, &apIndex);
                    if ( nRet != 2 || radioIndex>2 || apIndex>16)
                            return FALSE;
            }
        indexes=(radioIndex<<24) + (radioIndex_2<<16) + (apIndex<<8) + apIndex_2;
        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_t *attrp = NULL;

        attrp = &attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );
        pthread_create(&tid, attrp, &wifiFactoryReset, (void*)indexes);
        if(attrp != NULL)
            pthread_attr_destroy( attrp );
	
            //CosaDmlWiFi_FactoryReset();
            return TRUE;
    }

    if (!AnscEqualString(ParamName, "X_CISCO_COM_ConfigFileBase64", TRUE))
        return FALSE;

    binConf = AnscBase64Decode(pString, &binSize);
    if (binConf == NULL)
    {
        AnscTraceError(("%s: base64 decode error\n", __FUNCTION__));
        return FALSE;
    }
    
    if (CosaDmlWiFi_SetConfigFile(binConf, binSize) != ANSC_STATUS_SUCCESS)
    {
        AnscFreeMemory(binConf);
        return FALSE;
    }

    AnscFreeMemory(binConf);
    return TRUE;
}
 
/***********************************************************************

 APIs for Object:

    WiFi.Radio.{i}.

    *  Radio_GetEntryCount
    *  Radio_GetEntry
    *  Radio_GetParamBoolValue
    *  Radio_GetParamIntValue
    *  Radio_GetParamUlongValue
    *  Radio_GetParamStringValue
    *  Radio_SetParamBoolValue
    *  Radio_SetParamIntValue
    *  Radio_SetParamUlongValue
    *  Radio_SetParamStringValue
    *  Radio_Validate
    *  Radio_Commit
    *  Radio_Rollback

***********************************************************************/
/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        Radio_GetEntryCount
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to retrieve the count of the table.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The count of the table

**********************************************************************/
ULONG
Radio_GetEntryCount
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    ULONG                           count         = 0;
    
    return pMyObject->RadioCount;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ANSC_HANDLE
        Radio_GetEntry
            (
                ANSC_HANDLE                 hInsContext,
                ULONG                       nIndex,
                ULONG*                      pInsNumber
            );

    description:

        This function is called to retrieve the entry specified by the index.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                ULONG                       nIndex,
                The index of this entry;

                ULONG*                      pInsNumber
                The output instance number;

    return:     The handle to identify the entry

**********************************************************************/
ANSC_HANDLE
Radio_GetEntry
    (
        ANSC_HANDLE                 hInsContext,
        ULONG                       nIndex,
        ULONG*                      pInsNumber
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_RADIO            pWifiRadio    = NULL;
    
    if ( pMyObject->pRadio && nIndex < pMyObject->RadioCount )
    {
        pWifiRadio = pMyObject->pRadio+nIndex;

        *pInsNumber = pWifiRadio->Radio.Cfg.InstanceNumber;

        return pWifiRadio;
    }
    
    return NULL; /* return the handle */
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Radio_GetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL*                       pBool
            );

    description:

        This function is called to retrieve Boolean parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL*                       pBool
                The buffer of returned boolean value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Radio_GetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL*                       pBool
    )
{
    PCOSA_DML_WIFI_RADIO            pWifiRadio     = hInsContext;
    PCOSA_DML_WIFI_RADIO_FULL       pWifiRadioFull = &pWifiRadio->Radio;

    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "Enable", TRUE))
    {
        /* collect value */
	int wlanIndex = pWifiRadioFull->Cfg.InstanceNumber - 1;
	wifi_getRadioEnable(wlanIndex,&pWifiRadioFull->Cfg.bEnabled);//RDKB-EMU
        *pBool = pWifiRadioFull->Cfg.bEnabled;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "Upstream", TRUE))
    {
        /* collect value */
        *pBool = pWifiRadioFull->StaticInfo.bUpstream;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "AutoChannelSupported", TRUE))
    {
        /* collect value */
        *pBool = pWifiRadioFull->StaticInfo.AutoChannelSupported;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "AutoChannelEnable", TRUE))
    {
        /* collect value */
        *pBool = pWifiRadioFull->Cfg.AutoChannelEnable;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "IEEE80211hSupported", TRUE))
    {
        /* collect value */
        *pBool = pWifiRadioFull->StaticInfo.IEEE80211hSupported;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "IEEE80211hEnabled", TRUE))
    {
        /* collect value */
        *pBool = pWifiRadioFull->Cfg.IEEE80211hEnabled;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_APIsolation", TRUE))
    {
        /* collect value */
        *pBool = pWifiRadioFull->Cfg.APIsolation;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_FrameBurst", TRUE))
    {
        /* collect value */
        *pBool = pWifiRadioFull->Cfg.FrameBurst;

        return TRUE;
    }

    if (AnscEqualString(ParamName, "X_CISCO_COM_ApplySetting", TRUE))
    {
        *pBool = pWifiRadioFull->Cfg.ApplySetting; 
        return TRUE;
    }

    if (AnscEqualString(ParamName, "X_CISCO_COM_ReverseDirectionGrant", TRUE))
    {
        *pBool = pWifiRadioFull->Cfg.X_CISCO_COM_ReverseDirectionGrant; 
        return TRUE;
    }

    if (AnscEqualString(ParamName, "X_CISCO_COM_AggregationMSDU", TRUE))
    {
        *pBool = pWifiRadioFull->Cfg.X_CISCO_COM_AggregationMSDU; 
        return TRUE;
    }

    if (AnscEqualString(ParamName, "X_CISCO_COM_AutoBlockAck", TRUE))
    {
        *pBool = pWifiRadioFull->Cfg.X_CISCO_COM_AutoBlockAck; 
        return TRUE;
    }

    if (AnscEqualString(ParamName, "X_CISCO_COM_DeclineBARequest", TRUE))
    {
        *pBool = pWifiRadioFull->Cfg.X_CISCO_COM_DeclineBARequest; 
        return TRUE;
    }

    if (AnscEqualString(ParamName, "X_CISCO_COM_STBCEnable", TRUE))
    {
        *pBool = pWifiRadioFull->Cfg.X_CISCO_COM_STBCEnable; 
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Radio_GetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                int*                        pInt
            );

    description:

        This function is called to retrieve integer parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                int*                        pInt
                The buffer of returned integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Radio_GetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int*                        pInt
    )
{
    PCOSA_DML_WIFI_RADIO            pWifiRadio     = hInsContext;
    PCOSA_DML_WIFI_RADIO_FULL       pWifiRadioFull = &pWifiRadio->Radio;
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "MCS", TRUE))
    {
        /* collect value */
        *pInt = pWifiRadioFull->Cfg.MCS;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "TransmitPower", TRUE))
    {
        /* collect value */
        *pInt = pWifiRadioFull->Cfg.TransmitPower;
        
        return TRUE;
    }

    if (AnscEqualString(ParamName, "X_CISCO_COM_MbssUserControl", TRUE))
    {
        *pInt = pWifiRadioFull->Cfg.MbssUserControl; 
        return TRUE;
    }
    if (AnscEqualString(ParamName, "X_CISCO_COM_AdminControl", TRUE))
    {
        *pInt = pWifiRadioFull->Cfg.AdminControl; 
        return TRUE;
    }
    if (AnscEqualString(ParamName, "X_CISCO_COM_OnOffPushButtonTime", TRUE))
    {
        *pInt = pWifiRadioFull->Cfg.OnOffPushButtonTime; 
        return TRUE;
    }
    if (AnscEqualString(ParamName, "X_CISCO_COM_ObssCoex", TRUE))
    {
        *pInt = pWifiRadioFull->Cfg.ObssCoex; 
        return TRUE;
    }
    if (AnscEqualString(ParamName, "X_CISCO_COM_MulticastRate", TRUE))
    {
        *pInt = pWifiRadioFull->Cfg.MulticastRate; 
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Radio_GetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG*                      puLong
            );

    description:

        This function is called to retrieve ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG*                      puLong
                The buffer of returned ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Radio_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG*                      puLong
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_RADIO            pWifiRadio     = hInsContext;
    PCOSA_DML_WIFI_RADIO_FULL       pWifiRadioFull = &pWifiRadio->Radio;
    int wlanIndex = pWifiRadioFull->Cfg.InstanceNumber - 1;
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "Status", TRUE))
    {
        CosaDmlWiFiRadioGetDinfo((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiRadio->Radio.Cfg.InstanceNumber, &pWifiRadio->Radio.DynamicInfo);
    
        /* collect value */
	if( pWifiRadioFull->Cfg.bEnabled == TRUE)
		pWifiRadioFull->DynamicInfo.Status = COSA_DML_IF_STATUS_Up;
	else
		pWifiRadioFull->DynamicInfo.Status = COSA_DML_IF_STATUS_Down;
        *puLong = pWifiRadioFull->DynamicInfo.Status;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "LastChange", TRUE))
    {
        //CosaDmlWiFiRadioGetDinfo((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiRadio->Radio.Cfg.InstanceNumber, &pWifiRadio->Radio.DynamicInfo);
    
        /* collect value */
        *puLong = AnscGetTimeIntervalInSeconds(pWifiRadioFull->Cfg.LastChange, AnscGetTickInSeconds()); 
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "MaxBitRate", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioFull->StaticInfo.MaxBitRate;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "SupportedFrequencyBands", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioFull->StaticInfo.SupportedFrequencyBands;
        
        return TRUE;
    }
    if( AnscEqualString(ParamName, "Channel", TRUE))
    {
	    /* collect value */
	    *puLong = pWifiRadioFull->Cfg.Channel;
#if 1//RDKB_EMULATOR
		    wifi_getRadioChannel(wlanIndex,&pWifiRadioFull->Cfg.Channel);
		    wifi_setAutoChannelEnableVal(wlanIndex,pWifiRadioFull->Cfg.Channel);
		    *puLong = pWifiRadioFull->Cfg.Channel;
	    if (pWifiRadioFull->Cfg.AutoChannelEnable == TRUE )
	    {
		    if((AutoChannel_Enable_2G == true) || (AutoChannel_Enable_5G == true))
		    {
			    FILE *fp = NULL;
			    if((pWifiRadioFull->Cfg.InstanceNumber == 1) || (pWifiRadioFull->Cfg.InstanceNumber == 5))
			    {
				    fp = fopen("/var/prevchanval2G_AutoChannelEnable","r");
				    if(fp == NULL)
				    {
                                            v_secure_system("echo %ld > /var/prevchanval2G_AutoChannelEnable", pWifiRadioFull->Cfg.Channel);
				    }
				    else
				    {
					    pWifiRadioFull->Cfg.Channel = get_AutoChannelEnable_value(pWifiRadioFull->Cfg.InstanceNumber);
					    wifi_setAutoChannelEnableVal(wlanIndex,pWifiRadioFull->Cfg.Channel);
					    fclose(fp);
				    }
                                    AutoChannel_Enable_2G = false;
			    }
			    else if(pWifiRadioFull->Cfg.InstanceNumber == 2)
			    {
				    fp = fopen("/var/prevchanval5G_AutoChannelEnable","r");
				    if(fp == NULL)
				    {
                                            v_secure_system("echo %ld > /var/prevchanval5G_AutoChannelEnable", pWifiRadioFull->Cfg.Channel);
				    }
				    else
				    {
					    pWifiRadioFull->Cfg.Channel = get_AutoChannelEnable_value(pWifiRadioFull->Cfg.InstanceNumber);
					    wifi_setAutoChannelEnableVal(pWifiRadioFull->Cfg.InstanceNumber,pWifiRadioFull->Cfg.Channel);
					    fclose(fp);
				    }
                                    AutoChannel_Enable_5G = false;
			    }

		    }
		    else
		    {
			    pWifiRadioFull->Cfg.Channel = get_AutoChannelEnable_value(pWifiRadioFull->Cfg.InstanceNumber);
			    wifi_setAutoChannelEnableVal(wlanIndex,pWifiRadioFull->Cfg.Channel);
		    }
                    *puLong = pWifiRadioFull->Cfg.Channel;
		    char recName[256] = {0} ;
		    char param_value[50] = {0};

		    memset(recName, 0, sizeof(recName));//PSM ACCESS
		    sprintf(recName, ChannelNumber, pWifiRadioFull->Cfg.InstanceNumber);
		    sprintf(param_value,"%d",pWifiRadioFull->Cfg.Channel);
		    PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, param_value);

	    }
#endif
	    return TRUE;
    }

    if( AnscEqualString(ParamName, "AutoChannelRefreshPeriod", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioFull->Cfg.AutoChannelRefreshPeriod;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "OperatingChannelBandwidth", TRUE))
    {
        /* collect value */
	char bandwidth[50] ={0};
	wifi_getRadioOperatingChannelBandwidth(wlanIndex,bandwidth);
	if(strcmp(bandwidth,"20MHz") == 0)
		 pWifiRadioFull->Cfg.OperatingChannelBandwidth = COSA_DML_WIFI_CHAN_BW_20M;
	else if(strcmp(bandwidth,"40MHz") == 0)
		 pWifiRadioFull->Cfg.OperatingChannelBandwidth = COSA_DML_WIFI_CHAN_BW_40M;
	else if(strcmp(bandwidth,"80MHz") == 0)
		 pWifiRadioFull->Cfg.OperatingChannelBandwidth = COSA_DML_WIFI_CHAN_BW_80M;
	else if(strcmp(bandwidth,"160MHz") == 0)
		 pWifiRadioFull->Cfg.OperatingChannelBandwidth = COSA_DML_WIFI_CHAN_BW_160M;
        *puLong = pWifiRadioFull->Cfg.OperatingChannelBandwidth;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "ExtensionChannel", TRUE))
    {
        /* collect value */
        char bandwidth[64];
        char extchan[64];
        wifi_getRadioOperatingChannelBandwidth(wlanIndex, bandwidth);

        if (strcmp(bandwidth, "40MHz") == 0) {
             wifi_getRadioExtChannel(wlanIndex, extchan);
             if (strcmp(extchan, "AboveControlChannel") == 0)
                 pWifiRadioFull->Cfg.ExtensionChannel  = COSA_DML_WIFI_EXT_CHAN_Above;
              else if (strcmp(extchan, "BelowControlChannel") == 0)
                 pWifiRadioFull->Cfg.ExtensionChannel  = COSA_DML_WIFI_EXT_CHAN_Below;
              else
                 pWifiRadioFull->Cfg.ExtensionChannel  = COSA_DML_WIFI_EXT_CHAN_Auto;
         }
         else
               pWifiRadioFull->Cfg.ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Auto;

        *puLong = pWifiRadioFull->Cfg.ExtensionChannel;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "GuardInterval", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioFull->Cfg.GuardInterval;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_RTSThreshold", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioFull->Cfg.RTSThreshold;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_FragmentationThreshold", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioFull->Cfg.FragmentationThreshold;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_DTIMInterval", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioFull->Cfg.DTIMInterval;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_BeaconInterval", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioFull->Cfg.BeaconInterval;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_TxRate", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioFull->Cfg.TxRate;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_BasicRate", TRUE))
    {
        /* collect value */

       *puLong = pWifiRadioFull->Cfg.BasicRate; 
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_CTSProtectionMode", TRUE))
    {
        /* collect value */
        *puLong = (FALSE == pWifiRadioFull->Cfg.CTSProtectionMode) ? 0 : 1;

        return TRUE;
    }

    if (AnscEqualString(ParamName, "X_CISCO_COM_LongRetryLimit", TRUE))
    {
        *puLong = pWifiRadioFull->Cfg.LongRetryLimit; 
        return TRUE;
    }
  
    if (AnscEqualString(ParamName, "X_CISCO_COM_HTTxStream", TRUE))
    {
        *puLong = pWifiRadioFull->Cfg.X_CISCO_COM_HTTxStream; 
        return TRUE;
    }
  
    if (AnscEqualString(ParamName, "X_CISCO_COM_HTRxStream", TRUE))
    {
        *puLong = pWifiRadioFull->Cfg.X_CISCO_COM_HTRxStream; 
        return TRUE;
    }
  
    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        Radio_GetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pValue,
                ULONG*                      pUlSize
            );

    description:

        This function is called to retrieve string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pValue,
                The string value buffer;

                ULONG*                      pUlSize
                The buffer of length of string value;
                Usually size of 1023 will be used.
                If it's not big enough, put required size here and return 1;

    return:     0 if succeeded;
                1 if short of buffer size; (*pUlSize = required size)
                -1 if not supported.

**********************************************************************/
ULONG
Radio_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
    PCOSA_DML_WIFI_RADIO            pWifiRadio     = hInsContext;
    PCOSA_DML_WIFI_RADIO_FULL       pWifiRadioFull = &pWifiRadio->Radio;
    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;

    int wlanIndex = pWifiRadioFull->Cfg.InstanceNumber - 1;
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "Alias", TRUE))
    {
        /* collect value */
        if ( AnscSizeOfString(pWifiRadioFull->Cfg.Alias) < *pUlSize)
        {
            AnscCopyString(pValue, pWifiRadioFull->Cfg.Alias);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiRadioFull->Cfg.Alias)+1;
            return 1;
        }
        return 0;
    }

    if( AnscEqualString(ParamName, "Name", TRUE))
    {
        /* collect value */
        if ( AnscSizeOfString(pWifiRadioFull->StaticInfo.Name) < *pUlSize)
        {
            AnscCopyString(pValue, pWifiRadioFull->StaticInfo.Name);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiRadioFull->StaticInfo.Name)+1;
            return 1;
        }
        return 0;
    }

    if( AnscEqualString(ParamName, "LowerLayers", TRUE))
    {
        /*TR-181: Since Radio is a layer 1 interface, 
          it is expected that LowerLayers will not be used
         */
         /* collect value */
        AnscCopyString(pValue, "Not Applicable");
        return 0;
    }

    if( AnscEqualString(ParamName, "OperatingFrequencyBand", TRUE))
    {
        /* collect value */
        if ( 5 < *pUlSize)
        {
            if ( pWifiRadioFull->Cfg.OperatingFrequencyBand == COSA_DML_WIFI_FREQ_BAND_2_4G )
            {
                AnscCopyString(pValue, "2.4GHz");
            }
            else if ( pWifiRadioFull->Cfg.OperatingFrequencyBand == COSA_DML_WIFI_FREQ_BAND_5G )
            {
                AnscCopyString(pValue, "5GHz");
            }
            return 0;
        }
        else
        {
            *pUlSize = 6;
            
            return 1;
        }
        return 0;
    }

    if( AnscEqualString(ParamName, "OperatingStandards", TRUE))
    {
        /* collect value */
        char buf[512] = {0};
        if (pWifiRadioFull->Cfg.OperatingStandards & COSA_DML_WIFI_STD_a )
        {
            strcat(buf, "a");
        }
        
        if (pWifiRadioFull->Cfg.OperatingStandards & COSA_DML_WIFI_STD_b )
        {
            if (AnscSizeOfString(buf) != 0)
            {
                strcat(buf, ",b");
            }
            else
            {
                strcat(buf, "b");
            }
        }
        
        if (pWifiRadioFull->Cfg.OperatingStandards & COSA_DML_WIFI_STD_g )
        {
            if (AnscSizeOfString(buf) != 0)
            {
                strcat(buf, ",g");
            }
            else
            {
                strcat(buf, "g");
            }
        }
        
        if (pWifiRadioFull->Cfg.OperatingStandards & COSA_DML_WIFI_STD_n )
        {
            if (AnscSizeOfString(buf) != 0)
            {
                strcat(buf, ",n");
            }
            else
            {
                strcat(buf, "n");
            }
        }
	
	//RDKB-EMU
	if (pWifiRadioFull->Cfg.OperatingStandards & COSA_DML_WIFI_STD_ac )
        {
            if (AnscSizeOfString(buf) != 0)
            {
                strcat(buf, ",ac");
            }
            else
            {
                strcat(buf, "ac");
            }
        }


        if ( AnscSizeOfString(buf) < *pUlSize)
        {
            AnscCopyString(pValue, buf);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(buf)+1;
            
            return 1;
        }
        return 0;
    }

    if( AnscEqualString(ParamName, "PossibleChannels", TRUE))
    {
        /* collect value */
        if ( AnscSizeOfString(pWifiRadioFull->StaticInfo.PossibleChannels) < *pUlSize)
        {
            AnscCopyString(pValue, pWifiRadioFull->StaticInfo.PossibleChannels);
            
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiRadioFull->StaticInfo.PossibleChannels)+1;
            
            return 1;
        }
        return 0;
    }

    if( AnscEqualString(ParamName, "ChannelsInUse", TRUE))
    {
        CosaDmlWiFiRadioGetDinfo((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiRadio->Radio.Cfg.InstanceNumber, &pWifiRadio->Radio.DynamicInfo);
        /* collect value */
	if(pWifiRadioFull->Cfg.bEnabled == TRUE)
                wifi_getRadioChannelsInUse(wlanIndex,&pWifiRadioFull->DynamicInfo.ChannelsInUse);
        else
                AnscCopyString(pWifiRadioFull->DynamicInfo.ChannelsInUse, "0");

        if ( AnscSizeOfString(pWifiRadioFull->DynamicInfo.ChannelsInUse) < *pUlSize)
        {
            AnscCopyString(pValue, pWifiRadioFull->DynamicInfo.ChannelsInUse);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiRadioFull->DynamicInfo.ChannelsInUse)+1;
            return 1;
        }
        return 0;
    }

    if( AnscEqualString(ParamName, "TransmitPowerSupported", TRUE))
    {
        /* collect value */
        if ( AnscSizeOfString(pWifiRadioFull->StaticInfo.TransmitPowerSupported) < *pUlSize)
        {
            AnscCopyString(pValue, pWifiRadioFull->StaticInfo.TransmitPowerSupported);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiRadioFull->StaticInfo.TransmitPowerSupported)+1;
            return 1;
        }
        return 0;
    }

    if( AnscEqualString(ParamName, "RegulatoryDomain", TRUE))
    {
        /* collect value */
        if ( AnscSizeOfString(pWifiRadioFull->Cfg.RegulatoryDomain) < *pUlSize)
        {
            AnscCopyString(pValue, pWifiRadioFull->Cfg.RegulatoryDomain);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiRadioFull->Cfg.RegulatoryDomain)+1;
            return 1;
        }
        return 0;
    }

    if( AnscEqualString(ParamName, "SupportedStandards", TRUE))
    {
        /* collect value */
        char buf[512] = {0};
        if (pWifiRadioFull->StaticInfo.SupportedStandards & COSA_DML_WIFI_STD_a )
        {
            strcat(buf, "a");
        }
        
        if (pWifiRadioFull->StaticInfo.SupportedStandards & COSA_DML_WIFI_STD_b )
        {
            if (AnscSizeOfString(buf) != 0)
            {
                strcat(buf, ",b");
            }
            else
            {
                strcat(buf, "b");
            }
        }
        
        if (pWifiRadioFull->StaticInfo.SupportedStandards & COSA_DML_WIFI_STD_g )
        {
            if (AnscSizeOfString(buf) != 0)
            {
                strcat(buf, ",g");
            }
            else
            {
                strcat(buf, "g");
            }
        }
        
        if (pWifiRadioFull->StaticInfo.SupportedStandards & COSA_DML_WIFI_STD_n )
        {
            if (AnscSizeOfString(buf) != 0)
            {
                strcat(buf, ",n");
            }
            else
            {
                strcat(buf, "n");
            }
        }

	if (pWifiRadioFull->StaticInfo.SupportedStandards & COSA_DML_WIFI_STD_ac )
        {
            if (AnscSizeOfString(buf) != 0)
            {
                strcat(buf, ",ac");
            }
            else
            {
                strcat(buf, "ac");
            }
        }

        
        if ( AnscSizeOfString(buf) < *pUlSize)
        {
            AnscCopyString(pValue, buf);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(buf)+1;
            
            return 1;
        }
        return 0;
    }


    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return -1;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Radio_SetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL                        bValue
            );

    description:

        This function is called to set BOOL parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL                        bValue
                The updated BOOL value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Radio_SetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL                        bValue
    )
{
    PCOSA_DML_WIFI_RADIO            pWifiRadio     = hInsContext;
    PCOSA_DML_WIFI_RADIO_FULL       pWifiRadioFull = &pWifiRadio->Radio;

    int wlanIndex = pWifiRadioFull->Cfg.InstanceNumber -  1;
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "Enable", TRUE))
    {
        /* save update to backup */
        /* Currently Radio is always UP - Dont allow it to be disabled */
        /* pWifiRadioFull->Cfg.bEnabled = bValue; */
        /* return TRUE; */
        if ( pWifiRadioFull->Cfg.bEnabled == bValue )
        {
            return  TRUE;
        }

        /* save update to backup */
        pWifiRadioFull->Cfg.bEnabled = bValue;
        pWifiRadio->bRadioChanged = TRUE;
	
        wifi_setRadioEnable(wlanIndex,pWifiRadioFull->Cfg.bEnabled);//RDKB-EMU
        return TRUE;
        //return FALSE;
    }

    if( AnscEqualString(ParamName, "AutoChannelEnable", TRUE))
    {
        if ( pWifiRadioFull->Cfg.AutoChannelEnable == bValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.AutoChannelEnable = bValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "IEEE80211hEnabled", TRUE))
    {
        if ( pWifiRadioFull->Cfg.IEEE80211hEnabled == bValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.IEEE80211hEnabled = bValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_APIsolation", TRUE))
    {
        if ( pWifiRadioFull->Cfg.APIsolation == bValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.APIsolation = bValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_FrameBurst", TRUE))
    {
        if ( pWifiRadioFull->Cfg.FrameBurst == bValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.FrameBurst = bValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_ApplySetting", TRUE))
    {
        if ( pWifiRadioFull->Cfg.ApplySetting == bValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.ApplySetting = bValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_ReverseDirectionGrant", TRUE))
    {
        if ( pWifiRadioFull->Cfg.X_CISCO_COM_ReverseDirectionGrant == bValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.X_CISCO_COM_ReverseDirectionGrant = bValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_AggregationMSDU", TRUE))
    {
        if ( pWifiRadioFull->Cfg.X_CISCO_COM_AggregationMSDU == bValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.X_CISCO_COM_AggregationMSDU = bValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_AutoBlockAck", TRUE))
    {
        if ( pWifiRadioFull->Cfg.X_CISCO_COM_AutoBlockAck == bValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.X_CISCO_COM_AutoBlockAck = bValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_DeclineBARequest", TRUE))
    {
        if ( pWifiRadioFull->Cfg.X_CISCO_COM_DeclineBARequest == bValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.X_CISCO_COM_DeclineBARequest = bValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_STBCEnable", TRUE))
    {
        if ( pWifiRadioFull->Cfg.X_CISCO_COM_STBCEnable == bValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.X_CISCO_COM_STBCEnable = bValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Radio_SetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                int                         iValue
            );

    description:

        This function is called to set integer parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                int                         iValue
                The updated integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Radio_SetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int                         iValue
    )
{
    PCOSA_DML_WIFI_RADIO            pWifiRadio     = hInsContext;
    PCOSA_DML_WIFI_RADIO_FULL       pWifiRadioFull = &pWifiRadio->Radio;
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "MCS", TRUE))
    {
        if ( pWifiRadioFull->Cfg.MCS == iValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.MCS = iValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "TransmitPower", TRUE))
    {
        if ( pWifiRadioFull->Cfg.TransmitPower == iValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.TransmitPower = iValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_MbssUserControl", TRUE))
    {
        if ( pWifiRadioFull->Cfg.MbssUserControl == iValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.MbssUserControl = iValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }
    if( AnscEqualString(ParamName, "X_CISCO_COM_AdminControl", TRUE))
    {
        if ( pWifiRadioFull->Cfg.AdminControl == iValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.AdminControl = iValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }
    if( AnscEqualString(ParamName, "X_CISCO_COM_OnOffPushButtonTime", TRUE))
    {
        if ( pWifiRadioFull->Cfg.OnOffPushButtonTime == iValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.OnOffPushButtonTime = iValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }
    if( AnscEqualString(ParamName, "X_CISCO_COM_ObssCoex", TRUE))
    {
        if ( pWifiRadioFull->Cfg.ObssCoex == iValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.ObssCoex = iValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }
    if( AnscEqualString(ParamName, "X_CISCO_COM_MulticastRate", TRUE))
    {
        if ( pWifiRadioFull->Cfg.MulticastRate == iValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.MulticastRate = iValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Radio_SetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG                       uValue
            );

    description:

        This function is called to set ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG                       uValue
                The updated ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Radio_SetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG                       uValue
    )
{
	PCOSA_DML_WIFI_RADIO            pWifiRadio     = hInsContext;
	PCOSA_DML_WIFI_RADIO_FULL       pWifiRadioFull = &pWifiRadio->Radio;
	int wlanIndex = pWifiRadioFull->Cfg.InstanceNumber - 1;
	/* check the parameter name and set the corresponding value */
	if( AnscEqualString(ParamName, "Channel", TRUE))
	{
		if ( pWifiRadioFull->Cfg.Channel == uValue )
		{
			return  TRUE;
		}
		/* save update to backup */
		pWifiRadioFull->Cfg.Channel = uValue;
#if 1//RDKB_EMULATOR
		char recName[256] = {0} ;
		char param_value[50] = {0};

		memset(recName, 0, sizeof(recName));//PSM ACCESS
		sprintf(recName, ChannelNumber, pWifiRadioFull->Cfg.InstanceNumber);
		sprintf(param_value,"%d",pWifiRadioFull->Cfg.Channel);
		PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, param_value); 
		wifi_setRadioChannel(wlanIndex,  pWifiRadioFull->Cfg.Channel);
		pWifiRadioFull->Cfg.AutoChannelEnable = FALSE; /* User has manually set a channel */
		pWifiRadio->bRadioChanged = TRUE;
#endif
        return TRUE;
    }

    if( AnscEqualString(ParamName, "AutoChannelRefreshPeriod", TRUE))
    {
        if ( pWifiRadioFull->Cfg.AutoChannelRefreshPeriod == uValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.AutoChannelRefreshPeriod = uValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "OperatingChannelBandwidth", TRUE))
    {
        if ( pWifiRadioFull->Cfg.OperatingChannelBandwidth == uValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.OperatingChannelBandwidth = uValue;
        pWifiRadio->bRadioChanged = TRUE;
        if(pWifiRadioFull->Cfg.OperatingChannelBandwidth == 1)
		wifi_setRadioOperatingChannelBandwidth(wlanIndex,"20MHz");
        else if(pWifiRadioFull->Cfg.OperatingChannelBandwidth == 2)
		wifi_setRadioOperatingChannelBandwidth(wlanIndex,"40MHz");
        else if(pWifiRadioFull->Cfg.OperatingChannelBandwidth == 3)
		wifi_setRadioOperatingChannelBandwidth(wlanIndex,"Auto");
        return TRUE;
    }

    if( AnscEqualString(ParamName, "ExtensionChannel", TRUE))
    {
        if ( pWifiRadioFull->Cfg.ExtensionChannel == uValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.ExtensionChannel = uValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "GuardInterval", TRUE))
    {
        if ( pWifiRadioFull->Cfg.GuardInterval == uValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.GuardInterval = uValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_RTSThreshold", TRUE))
    {
        if ( pWifiRadioFull->Cfg.RTSThreshold == uValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.RTSThreshold = uValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_FragmentationThreshold", TRUE))
    {
        if ( pWifiRadioFull->Cfg.FragmentationThreshold == uValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.FragmentationThreshold = uValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_DTIMInterval", TRUE))
    {
        if ( pWifiRadioFull->Cfg.DTIMInterval == uValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.DTIMInterval = uValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_BeaconInterval", TRUE))
    {
        if ( pWifiRadioFull->Cfg.BeaconInterval == uValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.BeaconInterval = uValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_TxRate", TRUE))
    {
        if ( pWifiRadioFull->Cfg.TxRate == uValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.TxRate = uValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_BasicRate", TRUE))
    {
        if ( pWifiRadioFull->Cfg.BasicRate == uValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.BasicRate = uValue; 
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_CTSProtectionMode", TRUE))
    {
        if ( pWifiRadioFull->Cfg.CTSProtectionMode == (0 != uValue) )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.CTSProtectionMode = (0 == uValue) ? FALSE : TRUE;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_LongRetryLimit", TRUE))
    {
        if ( pWifiRadioFull->Cfg.LongRetryLimit == uValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.LongRetryLimit = uValue; 
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_HTTxStream", TRUE))
    {
        if ( pWifiRadioFull->Cfg.X_CISCO_COM_HTTxStream == uValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.X_CISCO_COM_HTTxStream = uValue; 
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_HTRxStream", TRUE))
    {
        if ( pWifiRadioFull->Cfg.X_CISCO_COM_HTRxStream == uValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.X_CISCO_COM_HTRxStream = uValue; 
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Radio_SetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pString
            );

    description:

        This function is called to set string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pString
                The updated string value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Radio_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pString
    )
{
    PCOSA_DML_WIFI_RADIO            pWifiRadio     = hInsContext;
    PCOSA_DML_WIFI_RADIO_FULL       pWifiRadioFull = &pWifiRadio->Radio;
    
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "Alias", TRUE))
    {
        /* save update to backup */
        AnscCopyString( pWifiRadioFull->Cfg.Alias, pString );
        return TRUE;
    }

    if( AnscEqualString(ParamName, "LowerLayers", TRUE))
    {
        /*TR-181: Since Radio is a layer 1 interface, 
          it is expected that LowerLayers will not be used
         */
        /* User shouldnt be able to set a value for this */
        return FALSE;
    }

    if( AnscEqualString(ParamName, "OperatingFrequencyBand", TRUE))
    {
        COSA_DML_WIFI_FREQ_BAND     TmpFreq;
        
        /* save update to backup */
        if (( AnscEqualString(pString, "2.4GHz", TRUE) ) && (1 == pWifiRadioFull->Cfg.InstanceNumber))
        {
            TmpFreq = COSA_DML_WIFI_FREQ_BAND_2_4G;
        }
        else if (( AnscEqualString(pString, "5GHz", TRUE) ) && (2 == pWifiRadioFull->Cfg.InstanceNumber))
        {
            TmpFreq = COSA_DML_WIFI_FREQ_BAND_5G;
        }
        else
    	{
    	    return FALSE; /* Radio can only support these two frequency bands and can not change dynamically */
    	}

        if ( pWifiRadioFull->Cfg.OperatingFrequencyBand == TmpFreq )
        {
            return  TRUE;
        }
        
    	pWifiRadioFull->Cfg.OperatingFrequencyBand = TmpFreq; 
        pWifiRadio->bRadioChanged = TRUE;
    	
        return TRUE;
    }
#if 0 //RDKB-EMU
    if( AnscEqualString(ParamName, "OperatingStandards", TRUE))
    {
        ULONG                       TmpOpStd;
        
        /* save update to backup */
        TmpOpStd = 0;
        if ( strchr(pString, 'a') != NULL )
        {
            TmpOpStd |= COSA_DML_WIFI_STD_a;
        }
        if ( strchr(pString, 'b') != NULL )
        {
            TmpOpStd |= COSA_DML_WIFI_STD_b;
        }
        if ( strchr(pString, 'g') != NULL )
        {
            TmpOpStd |= COSA_DML_WIFI_STD_g;
        }
        if ( strchr(pString, 'n') != NULL )
        {
            TmpOpStd |= COSA_DML_WIFI_STD_n;
        }

        if ( pWifiRadioFull->Cfg.OperatingStandards == TmpOpStd )
        {
            return  TRUE;
        }
        
        pWifiRadioFull->Cfg.OperatingStandards = TmpOpStd;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }
#endif
    if( AnscEqualString(ParamName, "OperatingStandards", TRUE))
    {
        ULONG                       TmpOpStd;
        char *a = _ansc_strchr(pString, 'a');
        char *ac = _ansc_strstr(pString, "ac");

        if (!ValidateOpStd(pWifiRadioFull->StaticInfo.SupportedStandards,  pString)) {
            AnscTraceError(("%s! wifi standard not supported %s\n", __FUNCTION__, pString));
            return FALSE;
        }

                //zqiu
                if( (a!=NULL) && (ac==NULL) )
                        return FALSE;

        /* save update to backup */
        TmpOpStd = 0;

        // if a and ac are not NULL and they are the same string, then move past the ac and search for an a by itself
        if (a && ac && (a  == ac)) {
            a = a+1;
            a = _ansc_strchr(a,'a');
        }

        if ( a != NULL )
        {
            TmpOpStd |= COSA_DML_WIFI_STD_a;
        }
        if ( ac != NULL )
        {
            TmpOpStd |= COSA_DML_WIFI_STD_ac;
        }
        if ( strchr(pString, 'b') != NULL )
        {
            TmpOpStd |= COSA_DML_WIFI_STD_b;
        }
        if ( strchr(pString, 'g') != NULL )
        {
            TmpOpStd |= COSA_DML_WIFI_STD_g;
        }
        if ( strchr(pString, 'n') != NULL )
        {
            TmpOpStd |= COSA_DML_WIFI_STD_n;
        }

	if ( pWifiRadioFull->Cfg.OperatingStandards == TmpOpStd )
        {
            return  TRUE;
        }

        pWifiRadioFull->Cfg.OperatingStandards = TmpOpStd;

/*        if(pWifiRadioFull->Cfg.OperatingStandards == (COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n) ) {

                isBeaconRateUpdate[0] = isBeaconRateUpdate[2] = isBeaconRateUpdate[4] =  isBeaconRateUpdate[6] = isBeaconRateUpdate[8] = isBeaconRateUpdate[10] = TRUE;
                CosaWifiAdjustBeaconRate(1, "6Mbps");
                CcspTraceWarning(("WIFI OperatingStandards = g/n  Beacon Rate 6Mbps  Function %s \n",__FUNCTION__));
        }
        else if (pWifiRadioFull->Cfg.OperatingStandards == (COSA_DML_WIFI_STD_b |COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n) ) {

                isBeaconRateUpdate[0] = isBeaconRateUpdate[2] = isBeaconRateUpdate[4] =  isBeaconRateUpdate[6] = isBeaconRateUpdate[8] = isBeaconRateUpdate[10] = TRUE;
                CosaWifiAdjustBeaconRate(1, "1Mbps");
                CcspTraceWarning(("WIFI OperatingStandards = b/g/n  Beacon Rate 1Mbps %s \n",__FUNCTION__));
        }*/

        pWifiRadio->bRadioChanged = TRUE;

        return TRUE;
    }

    if( AnscEqualString(ParamName, "RegulatoryDomain", TRUE))
    {
        if ( AnscEqualString(pWifiRadioFull->Cfg.RegulatoryDomain, pString, TRUE) )
        {
            return  TRUE;
        }
         
        /* save update to backup */
        AnscCopyString( pWifiRadioFull->Cfg.RegulatoryDomain, pString );
        pWifiRadio->bRadioChanged = TRUE;
        return TRUE;
    }


    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Radio_Validate
            (
                ANSC_HANDLE                 hInsContext,
                char*                       pReturnParamName,
                ULONG*                      puLength
            );

    description:

        This function is called to finally commit all the update.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       pReturnParamName,
                The buffer (128 bytes) of parameter name if there's a validation. 

                ULONG*                      puLength
                The output length of the param name. 

    return:     TRUE if there's no validation.

**********************************************************************/
BOOL
Radio_Validate
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pReturnParamName,
        ULONG*                      puLength
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject      = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_RADIO            pWifiRadio     = hInsContext;
    PCOSA_DML_WIFI_RADIO_FULL       pWifiRadioFull = &pWifiRadio->Radio;
    PCOSA_DML_WIFI_RADIO            pWifiRadio2    = NULL;
    PCOSA_DML_WIFI_RADIO_FULL       pWifiRadioFull2= NULL;
    ULONG                           idx            = 0;
    ULONG                           maxCount       = 0;
 
    /*Alias should be non-empty*/
    if (AnscSizeOfString(pWifiRadio->Radio.Cfg.Alias) == 0)
    {
        CcspTraceWarning(("********Radio Validate:Failed Alias \n"));
        AnscCopyString(pReturnParamName, "Alias");
        *puLength = AnscSizeOfString("Alias");
        return FALSE;
    }
 
    for ( idx = 0; idx < pMyObject->RadioCount; idx++)
    {
        if ( pWifiRadio == (pMyObject->pRadio + idx) )
        {
            continue;
        }
        
        pWifiRadio2     = pMyObject->pRadio + idx;
        pWifiRadioFull2 = &pWifiRadio2->Radio;
        
        if ( AnscEqualString(pWifiRadio->Radio.Cfg.Alias, pWifiRadio2->Radio.Cfg.Alias, TRUE) )
        {
            CcspTraceWarning(("********Radio Validate:Failed Alias \n"));
            AnscCopyString(pReturnParamName, "Alias");
            *puLength = AnscSizeOfString("Alias");
            return FALSE;
        }
    }

    if (!(CosaUtilChannelValidate(pWifiRadioFull->Cfg.OperatingFrequencyBand,pWifiRadioFull->Cfg.Channel)))
        {
            CcspTraceWarning(("********Radio Validate:Failed Channel\n"));
            AnscCopyString(pReturnParamName, "Channel");
            *puLength = AnscSizeOfString("Channel");
            return FALSE;
        }

    
    if ( pWifiRadioFull->Cfg.OperatingFrequencyBand == COSA_DML_WIFI_FREQ_BAND_2_4G )
    {
        if (pWifiRadioFull->Cfg.OperatingStandards & COSA_DML_WIFI_STD_a)
        {
            CcspTraceWarning(("********Radio Validate:Failed OperatingStandards\n"));
            AnscCopyString(pReturnParamName, "OperatingStandards");
            *puLength = AnscSizeOfString("OperatingStandards");
            return FALSE;
        }
    }
    
    if ( pWifiRadioFull->Cfg.OperatingFrequencyBand == COSA_DML_WIFI_FREQ_BAND_5G )
    {
        if (pWifiRadioFull->Cfg.OperatingStandards & COSA_DML_WIFI_STD_b)
        {
            CcspTraceWarning(("********Radio Validate:Failed OperatingStandards\n"));
            AnscCopyString(pReturnParamName, "OperatingStandards");
            *puLength = AnscSizeOfString("OperatingStandards");
            return FALSE;
        }
        
        if (pWifiRadioFull->Cfg.OperatingStandards & COSA_DML_WIFI_STD_g)
        {
            CcspTraceWarning(("********Radio Validate:Failed OperatingStandards\n"));
            AnscCopyString(pReturnParamName, "OperatingStandards");
            *puLength = AnscSizeOfString("OperatingStandards");
            return FALSE;
        }
    }

    if( (pWifiRadioFull->Cfg.TransmitPower != 25) && (pWifiRadioFull->Cfg.TransmitPower != 50) && (pWifiRadioFull->Cfg.TransmitPower != 100) )
    {
         CcspTraceWarning(("********Radio Validate:Failed Transmit Power\n"));
         AnscCopyString(pReturnParamName, "TransmitPower");
         *puLength = AnscSizeOfString("TransmitPower");
         return FALSE;
    }    

    if( (pWifiRadioFull->Cfg.BeaconInterval < 1) || (pWifiRadioFull->Cfg.BeaconInterval > 65535) )
    {
         CcspTraceWarning(("********Radio Validate:Failed BeaconInterval\n"));
         AnscCopyString(pReturnParamName, "BeaconInterval");
         *puLength = AnscSizeOfString("BeaconInterval");
         return FALSE;
    }

    if( (pWifiRadioFull->Cfg.DTIMInterval < 1) || (pWifiRadioFull->Cfg.DTIMInterval > 255) )
    {
         CcspTraceWarning(("********Radio Validate:Failed DTIMInterval\n"));
         AnscCopyString(pReturnParamName, "DTIMInterval");
         *puLength = AnscSizeOfString("DTIMInterval");
         return FALSE;
    }

    if( (pWifiRadioFull->Cfg.FragmentationThreshold < 256) || (pWifiRadioFull->Cfg.FragmentationThreshold > 2346) )
    {
         CcspTraceWarning(("********Radio Validate:Failed FragThreshhold\n"));
         AnscCopyString(pReturnParamName, "FragmentationThreshold");
         *puLength = AnscSizeOfString("FragmentationThreshold");
         return FALSE;
    }

    if( (pWifiRadioFull->Cfg.RTSThreshold > 2347) )
    {
         CcspTraceWarning(("********Radio Validate:Failed RTSThreshhold\n"));
         AnscCopyString(pReturnParamName, "RTSThreshold");
         *puLength = AnscSizeOfString("RTSThreshold");
         return FALSE;
    }

    maxCount = (60 * 71582787);
    if(pWifiRadioFull->Cfg.AutoChannelRefreshPeriod > maxCount)
    {
         CcspTraceWarning(("********Radio Validate:Failed AutoChannelRefreshPeriod\n"));
         AnscCopyString(pReturnParamName, "AutoChannelRefreshPeriod");
         *puLength = AnscSizeOfString("AutoChannelRefreshPeriod");
         return FALSE;
    }

#if 0
    if(pWifiRadioFull->Cfg.RegulatoryDomain[2] != 'I')
    {
         /* Currently driver supports only Inside a country code */
         CcspTraceWarning(("********Radio Validate:Failed Regulatory Domain \n"));
         AnscCopyString(pReturnParamName, "RegulatoryDomain");
         *puLength = AnscSizeOfString("RegulatoryDomain");
         return FALSE;
    }
#endif

    return TRUE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        Radio_Commit
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to finally commit all the update.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
Radio_Commit
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_RADIO            pWifiRadio     = hInsContext;
    PCOSA_DML_WIFI_RADIO_FULL       pWifiRadioFull = &pWifiRadio->Radio;
    PCOSA_DML_WIFI_RADIO_CFG        pWifiRadioCfg  = &pWifiRadioFull->Cfg;

    if ( !pWifiRadio->bRadioChanged )
    {
        return  ANSC_STATUS_SUCCESS;
    }
    else
    {
        pWifiRadio->bRadioChanged = FALSE;
        CcspTraceInfo(("WiFi Radio -- apply the change...\n"));
    }
    
    return CosaDmlWiFiRadioSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiRadioCfg);
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        Radio_Rollback
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to roll back the update whenever there's a 
        validation found.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
Radio_Rollback
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject      = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_RADIO            pWifiRadio     = pMyObject->pRadio;
    PCOSA_DML_WIFI_RADIO_FULL       pWifiRadioFull = &pWifiRadio->Radio;
    ULONG                           idx            = 0;    
    PCOSA_DML_WIFI_RADIO            pWifiRadio2    = hInsContext;
    
    for (idx=0; idx < pMyObject->RadioCount; idx++)
    {
        if ( pWifiRadio2 == pWifiRadio+idx )
        {
            return CosaDmlWiFiRadioGetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, &pWifiRadio2->Radio.Cfg) ;
        }
    }
    
    return ANSC_STATUS_SUCCESS;
}

/***********************************************************************

 APIs for Object:

    WiFi.Radio.{i}.Stats.

    *  Stats3_GetParamBoolValue
    *  Stats3_GetParamIntValue
    *  Stats3_GetParamUlongValue
    *  Stats3_GetParamStringValue

***********************************************************************/
/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Stats3_GetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL*                       pBool
            );

    description:

        This function is called to retrieve Boolean parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL*                       pBool
                The buffer of returned boolean value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Stats3_GetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL*                       pBool
    )
{
    /* check the parameter name and return the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Stats3_GetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                int*                        pInt
            );

    description:

        This function is called to retrieve integer parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                int*                        pInt
                The buffer of returned integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Stats3_GetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int*                        pInt
    )
{
    /* check the parameter name and return the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Stats3_GetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG*                      puLong
            );

    description:

        This function is called to retrieve ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG*                      puLong
                The buffer of returned ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Stats3_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG*                      puLong
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject       = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_RADIO            pWifiRadio      = hInsContext;
    PCOSA_DML_WIFI_RADIO_STATS      pWifiRadioStats = &pWifiRadio->Stats;
    
    CosaDmlWiFiRadioGetStats((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiRadio->Radio.Cfg.InstanceNumber, pWifiRadioStats);
    
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "BytesSent", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioStats->BytesSent;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "BytesReceived", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioStats->BytesReceived;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "PacketsSent", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioStats->PacketsSent;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "PacketsReceived", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioStats->PacketsReceived;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "ErrorsSent", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioStats->ErrorsSent;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "ErrorsReceived", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioStats->ErrorsReceived;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "DiscardPacketsSent", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioStats->DiscardPacketsSent;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "DiscardPacketsReceived", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioStats->DiscardPacketsReceived;
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        Stats3_GetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pValue,
                ULONG*                      pUlSize
            );

    description:

        This function is called to retrieve string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pValue,
                The string value buffer;

                ULONG*                      pUlSize
                The buffer of length of string value;
                Usually size of 1023 will be used.
                If it's not big enough, put required size here and return 1;

    return:     0 if succeeded;
                1 if short of buffer size; (*pUlSize = required size)
                -1 if not supported.

**********************************************************************/
ULONG
Stats3_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
    /* check the parameter name and return the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return -1;
}

/***********************************************************************

 APIs for Object:

    WiFi.SSID.{i}.

    *  SSID_GetEntryCount
    *  SSID_GetEntry
    *  SSID_AddEntry
    *  SSID_DelEntry
    *  SSID_GetParamBoolValue
    *  SSID_GetParamIntValue
    *  SSID_GetParamUlongValue
    *  SSID_GetParamStringValue
    *  SSID_SetParamBoolValue
    *  SSID_SetParamIntValue
    *  SSID_SetParamUlongValue
    *  SSID_SetParamStringValue
    *  SSID_Validate
    *  SSID_Commit
    *  SSID_Rollback

***********************************************************************/
/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        SSID_GetEntryCount
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to retrieve the count of the table.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The count of the table

**********************************************************************/
ULONG
SSID_GetEntryCount
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    ULONG                           entryCount    = 0;
    
    entryCount = AnscSListQueryDepth(&pMyObject->SsidQueue);
    
    return entryCount;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ANSC_HANDLE
        SSID_GetEntry
            (
                ANSC_HANDLE                 hInsContext,
                ULONG                       nIndex,
                ULONG*                      pInsNumber
            );

    description:

        This function is called to retrieve the entry specified by the index.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                ULONG                       nIndex,
                The index of this entry;

                ULONG*                      pInsNumber
                The output instance number;

    return:     The handle to identify the entry

**********************************************************************/
ANSC_HANDLE
SSID_GetEntry
    (
        ANSC_HANDLE                 hInsContext,
        ULONG                       nIndex,
        ULONG*                      pInsNumber
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj      = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntry   = (PSINGLE_LINK_ENTRY       )NULL;

    pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->SsidQueue, nIndex);
    
    if (pSLinkEntry)
    {
        pLinkObj = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);
        
        *pInsNumber = pLinkObj->InstanceNumber;
    }
    
    return pLinkObj; /* return the handle */
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ANSC_HANDLE
        SSID_AddEntry
            (
                ANSC_HANDLE                 hInsContext,
                ULONG*                      pInsNumber
            );

    description:

        This function is called to add a new entry.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                ULONG*                      pInsNumber
                The output instance number;

    return:     The handle of new added entry.

**********************************************************************/
ANSC_HANDLE
SSID_AddEntry
    (
        ANSC_HANDLE                 hInsContext,
        ULONG*                      pInsNumber
    )
{

    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj      = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid     = (PCOSA_DML_WIFI_SSID      )NULL;
    PCOSA_DML_WIFI_SSID_FULL        pWifiSsidFull = (PCOSA_DML_WIFI_SSID_FULL )NULL;

    pLinkObj                   = (PCOSA_CONTEXT_LINK_OBJECT)AnscAllocateMemory(sizeof(COSA_CONTEXT_LINK_OBJECT));
    if (!pLinkObj)
    {
        return NULL;
    }
    
    pWifiSsid                  = AnscAllocateMemory(sizeof(COSA_DML_WIFI_SSID));
    if (!pWifiSsid)
    {
        AnscFreeMemory(pLinkObj);
        return NULL;
    }
    
    if (TRUE)
    {
        pLinkObj->InstanceNumber           = pMyObject->ulSsidNextInstance;
    
        pWifiSsid->SSID.Cfg.InstanceNumber = pMyObject->ulSsidNextInstance;
    
        pMyObject->ulSsidNextInstance++;

        if ( pMyObject->ulSsidNextInstance == 0 )
        {
            pMyObject->ulSsidNextInstance = 1;
        }
    
        /*Set default Name, SSID & Alias*/
        _ansc_sprintf(pWifiSsid->SSID.StaticInfo.Name, "SSID%d", *pInsNumber);
        _ansc_sprintf(pWifiSsid->SSID.Cfg.Alias, "SSID%d", *pInsNumber);
        _ansc_sprintf(pWifiSsid->SSID.Cfg.SSID, "Cisco-SSID-%d", *pInsNumber);
    
        pLinkObj->hContext         = (ANSC_HANDLE)pWifiSsid;
        pLinkObj->hParentTable     = NULL;
        pLinkObj->bNew             = TRUE;
        
        pWifiSsidFull              = &pWifiSsid->SSID;
        
        CosaSListPushEntryByInsNum((PSLIST_HEADER)&pMyObject->SsidQueue, pLinkObj);

        CosaWifiRegAddSsidInfo((ANSC_HANDLE)pMyObject, (ANSC_HANDLE)pLinkObj);
    }
    
    *pInsNumber = pLinkObj->InstanceNumber;
    
    return (ANSC_HANDLE)pLinkObj; /* return the handle */
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        SSID_DelEntry
            (
                ANSC_HANDLE                 hInsContext,
                ANSC_HANDLE                 hInstance
            );

    description:

        This function is called to delete an exist entry.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                ANSC_HANDLE                 hInstance
                The exist entry handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
SSID_DelEntry
    (
        ANSC_HANDLE                 hInsContext,
        ANSC_HANDLE                 hInstance
    )
{

    PCOSA_DATAMODEL_WIFI            pMyObject    = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInstance;
    PCOSA_DML_WIFI_SSID             pWifiSsid    = (PCOSA_DML_WIFI_SSID      )NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntry  = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pAPLinkObj   = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )NULL;
    UCHAR                           PathName[64] = {0};
   
    if(pLinkObj)
        pWifiSsid    = (PCOSA_DML_WIFI_SSID      )pLinkObj->hContext;
    if ( CosaDmlWiFiSsidDelEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.InstanceNumber) != ANSC_STATUS_SUCCESS )
    {
        return ANSC_STATUS_FAILURE;
    }
    
    AnscQueuePopEntryByLink(&pMyObject->SsidQueue, &pLinkObj->Linkage);
    
    /*Reset the SSIDReference in AccessPoint table*/
    sprintf(PathName, "Device.WiFi.SSID.%d.", pLinkObj->InstanceNumber);
    
    pSLinkEntry = AnscQueueGetFirstEntry(&pMyObject->AccessPointQueue);
    while ( pSLinkEntry )
    {
        pAPLinkObj   = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);
        pWifiAp      = pAPLinkObj->hContext;
        
        if (AnscEqualString(pWifiAp->AP.Cfg.SSID, PathName, TRUE))
        {
            memset(pWifiAp->AP.Cfg.SSID, 0, sizeof(pWifiAp->AP.Cfg.SSID));
            
            pAPLinkObj->bNew = TRUE;
            
            CosaWifiRegAddAPInfo((ANSC_HANDLE)pMyObject, (ANSC_HANDLE)pAPLinkObj);
        }
        
        pSLinkEntry             = AnscQueueGetNextEntry(pSLinkEntry);
    }
    
    if (pLinkObj->bNew)
    {
        CosaWifiRegDelSsidInfo((ANSC_HANDLE)pMyObject, (ANSC_HANDLE)pLinkObj);
    }
    
    if (pLinkObj->hContext)
    {
        AnscFreeMemory(pLinkObj->hContext);
    }
    
    if (pLinkObj)
    {
        AnscFreeMemory(pLinkObj);
    }

    return 0; /* succeeded */
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        SSID_GetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL*                       pBool
            );

    description:

        This function is called to retrieve Boolean parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL*                       pBool
                The buffer of returned boolean value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
SSID_GetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL*                       pBool
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_SSID             pWifiSsid    = (PCOSA_DML_WIFI_SSID      )pLinkObj->hContext;

    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "Enable", TRUE))
    {
	int wlanIndex = pWifiSsid->SSID.Cfg.InstanceNumber - 1;
        /* collect value */
	/*memset(param_name, 0, sizeof(param_name));//LNT_EMU
        sprintf(param_name, HideSsid, pWifiSsid->SSID.Cfg.InstanceNumber);
        PSM_Get_Record_Value2(bus_handle,g_Subsystem, param_name, NULL, &param_value);
        pWifiSsid->SSID.Cfg.bEnabled = (atoi(param_value) == 1) ? TRUE : FALSE;*/
	//wifi_getSSIDEnable(pWifiSsid->SSID.Cfg.InstanceNumber,&pWifiSsid->SSID.Cfg.bEnabled);
	wifi_getSSIDEnable(wlanIndex,&pWifiSsid->SSID.Cfg.bEnabled);
        *pBool = pWifiSsid->SSID.Cfg.bEnabled;
        return TRUE;
    }


    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        SSID_GetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                int*                        pInt
            );

    description:

        This function is called to retrieve integer parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                int*                        pInt
                The buffer of returned integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
SSID_GetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int*                        pInt
    )
{
    /* check the parameter name and return the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        SSID_GetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG*                      puLong
            );

    description:

        This function is called to retrieve ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG*                      puLong
                The buffer of returned ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
SSID_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG*                      puLong
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject    = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_SSID             pWifiSsid    = (PCOSA_DML_WIFI_SSID      )pLinkObj->hContext;

    CosaDmlWiFiSsidGetDinfo((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.InstanceNumber, &pWifiSsid->SSID.DynamicInfo);
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "Status", TRUE))
    {
        /* collect value */
        *puLong  = pWifiSsid->SSID.DynamicInfo.Status;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "LastChange", TRUE))
    {
        /* collect value */
        *puLong  = AnscGetTimeIntervalInSeconds(pWifiSsid->SSID.DynamicInfo.LastChange, AnscGetTickInSeconds());
        return TRUE;
    }


    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        SSID_GetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pValue,
                ULONG*                      pUlSize
            );

    description:

        This function is called to retrieve string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pValue,
                The string value buffer;

                ULONG*                      pUlSize
                The buffer of length of string value;
                Usually size of 1023 will be used.
                If it's not big enough, put required size here and return 1;

    return:     0 if succeeded;
                1 if short of buffer size; (*pUlSize = required size)
                -1 if not supported.

**********************************************************************/
ULONG
SSID_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_SSID             pWifiSsid    = (PCOSA_DML_WIFI_SSID      )pLinkObj->hContext;
    PUCHAR                          pLowerLayer  = NULL;
    int wlanIndex = pWifiSsid->SSID.Cfg.InstanceNumber - 1;
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "Alias", TRUE))
    {
        /* collect value */
        if ( AnscSizeOfString(pWifiSsid->SSID.Cfg.Alias) < *pUlSize)
        {
            AnscCopyString(pValue, pWifiSsid->SSID.Cfg.Alias);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiSsid->SSID.Cfg.Alias)+1;
            return 1;
        }
        return 0;
    }

    if( AnscEqualString(ParamName, "Name", TRUE))
    {
        /* collect value */
        if ( AnscSizeOfString(pWifiSsid->SSID.StaticInfo.Name) < *pUlSize)
        {
            AnscCopyString(pValue, pWifiSsid->SSID.StaticInfo.Name);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiSsid->SSID.StaticInfo.Name)+1;
            return 1;
        }
        return 0;
    }

    if( AnscEqualString(ParamName, "LowerLayers", TRUE))
    {
        /* collect value */
        pLowerLayer = CosaUtilGetLowerLayers("Device.WiFi.Radio.", pWifiSsid->SSID.Cfg.WiFiRadioName);
        
        if (pLowerLayer != NULL)
        {
            AnscCopyString(pValue, pLowerLayer);
            
            AnscFreeMemory(pLowerLayer);
        }
        return 0;
    }

    if( AnscEqualString(ParamName, "BSSID", TRUE))
    {
        /* collect value */
        if ( AnscSizeOfString(pWifiSsid->SSID.StaticInfo.BSSID) < *pUlSize)
        {
	    _ansc_sprintf
            (
                pValue,
                "%02X:%02X:%02X:%02X:%02X:%02X",
		pWifiSsid->SSID.StaticInfo.BSSID[0],
                pWifiSsid->SSID.StaticInfo.BSSID[1],
                pWifiSsid->SSID.StaticInfo.BSSID[2],
                pWifiSsid->SSID.StaticInfo.BSSID[3],
                pWifiSsid->SSID.StaticInfo.BSSID[4],
                pWifiSsid->SSID.StaticInfo.BSSID[5]
            );
            *pUlSize = AnscSizeOfString(pValue);

            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiSsid->SSID.StaticInfo.BSSID)+1;
            return 1;
        }
        return 0;
    }

    if( AnscEqualString(ParamName, "MACAddress", TRUE))
    {
        /* collect value */
        if ( AnscSizeOfString(pWifiSsid->SSID.StaticInfo.MacAddress) < *pUlSize)
        {
	    _ansc_sprintf
            (
                pValue,
                "%02X:%02X:%02X:%02X:%02X:%02X",
		pWifiSsid->SSID.StaticInfo.MacAddress[0],
                pWifiSsid->SSID.StaticInfo.MacAddress[1],
                pWifiSsid->SSID.StaticInfo.MacAddress[2],
                pWifiSsid->SSID.StaticInfo.MacAddress[3],
                pWifiSsid->SSID.StaticInfo.MacAddress[4],
                pWifiSsid->SSID.StaticInfo.MacAddress[5]
            );

            *pUlSize = AnscSizeOfString(pValue);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiSsid->SSID.StaticInfo.MacAddress)+1;
            return 1;
        }
        return 0;
    }

    if( AnscEqualString(ParamName, "SSID", TRUE))
    {
        /* collect value */
        if ( AnscSizeOfString(pWifiSsid->SSID.Cfg.SSID) < *pUlSize)
        {
	    if(pWifiSsid->SSID.Cfg.bEnabled == true)//LNT_EMU
                {
			wifi_getSSIDName(wlanIndex,&pWifiSsid->SSID.Cfg.SSID);
                }
                else
                {
	                strcpy(pWifiSsid->SSID.Cfg.SSID,"OutOfService");
	        }
            AnscCopyString(pValue, pWifiSsid->SSID.Cfg.SSID);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiSsid->SSID.Cfg.SSID)+1;
            return 1;
        }
        return 0;
    }


    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return -1;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        SSID_SetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL                        bValue
            );

    description:

        This function is called to set BOOL parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL                        bValue
                The updated BOOL value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
SSID_SetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL                        bValue
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_SSID             pWifiSsid    = (PCOSA_DML_WIFI_SSID      )pLinkObj->hContext;
   
    char recName[256] = {0};//LNT_EMU
    char str[100] ={0},buf[100] = {0};
    INT wlanIndex = pWifiSsid->SSID.Cfg.InstanceNumber - 1;
 
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "Enable", TRUE))
    {
     /*   if ( pWifiSsid->SSID.Cfg.bEnabled == bValue )
        {
            return  TRUE;
        }*/
        /* save update to backup */
        pWifiSsid->SSID.Cfg.bEnabled = bValue;
        pWifiSsid->bSsidChanged = TRUE; 

#if 1//LNT_EMU
        //PSM Access
        memset(recName, 0, sizeof(recName));
        sprintf(recName, HideSsid, pWifiSsid->SSID.Cfg.InstanceNumber);
        sprintf(str,"%d",pWifiSsid->SSID.Cfg.bEnabled);
        PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, str);

        //wifi_setSSIDEnable(pWifiSsid->SSID.Cfg.InstanceNumber,pWifiSsid->SSID.Cfg.bEnabled);
        wifi_setSSIDEnable(wlanIndex,pWifiSsid->SSID.Cfg.bEnabled);
#endif
        return TRUE;
    }


    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        SSID_SetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                int                         iValue
            );

    description:

        This function is called to set integer parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                int                         iValue
                The updated integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
SSID_SetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int                         iValue
    )
{
    /* check the parameter name and set the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        SSID_SetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG                       uValue
            );

    description:

        This function is called to set ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG                       uValue
                The updated ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
SSID_SetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG                       uValue
    )
{
    /* check the parameter name and set the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        SSID_SetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pString
            );

    description:

        This function is called to set string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pString
                The updated string value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
SSID_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pString
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj              = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_SSID             pWifiSsid             = (PCOSA_DML_WIFI_SSID      )pLinkObj->hContext;
    ULONG                           ulEntryNameLen        = 256;
    UCHAR                           ucEntryParamName[256] = {0};
    UCHAR                           ucEntryNameValue[256] = {0};

    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "Alias", TRUE))
    {
        /* save update to backup */
        AnscCopyString( pWifiSsid->SSID.Cfg.Alias, pString );

        return TRUE;
    }

    if( AnscEqualString(ParamName, "LowerLayers", TRUE))
    {
        /* save update to backup */
    #ifdef _COSA_SIM_
        _ansc_sprintf(ucEntryParamName, "%s%s", pString, "Name");
        
        if ( ( 0 == CosaGetParamValueString(ucEntryParamName, ucEntryNameValue, &ulEntryNameLen)) &&
             (AnscSizeOfString(ucEntryNameValue) != 0) )
        {
            AnscCopyString(pWifiSsid->SSID.Cfg.WiFiRadioName, ucEntryNameValue);
            
            return TRUE;
        }
    #else
    	if(0 == strlen(pWifiSsid->SSID.Cfg.WiFiRadioName)) { /*Lower layers can only be specified during creation */
    
                _ansc_sprintf(ucEntryParamName, "%s%s", pString, "Name");
            
                if ( ( 0 == g_GetParamValueString(g_pDslhDmlAgent, ucEntryParamName, ucEntryNameValue, &ulEntryNameLen)) &&
                     (AnscSizeOfString(ucEntryNameValue) != 0) )
                {
                    AnscCopyString(pWifiSsid->SSID.Cfg.WiFiRadioName, ucEntryNameValue);
                    return TRUE;
                }
        } else
            return FALSE;
    #endif
    }
#if 0//LNT_EMU
    if ( AnscEqualString(ParamName, "SSID", TRUE) )
    {
        if ( AnscEqualString(pWifiSsid->SSID.Cfg.SSID, pString, TRUE) )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        AnscCopyString( pWifiSsid->SSID.Cfg.SSID, pString );
        pWifiSsid->bSsidChanged = TRUE; 
        return TRUE;
    }
#endif
#if 1//LNT_EMU
    if ( AnscEqualString(ParamName, "SSID", TRUE) )
    {
        if ( AnscEqualString(pWifiSsid->SSID.Cfg.SSID, pString, TRUE) )
        {
            return  TRUE;
        }

    if (IsSsidHotspot(pWifiSsid->SSID.Cfg.InstanceNumber) )
        {

                if(AnscEqualString(pString, "OutOfService", FALSE)) /* case insensitive */
                {
                    pWifiSsid->SSID.Cfg.bEnabled = FALSE;
                    fprintf(stderr, "%s: Disable HHS SSID since it's set to OutOfService\n", __FUNCTION__);
                }
             else
                {

                    isHotspotSSIDIpdated = TRUE;
                }
        }

//RDKB-20043 - We should not restrict here
#if 0
        if ( (pWifiSsid->SSID.Cfg.InstanceNumber == 1) || (pWifiSsid->SSID.Cfg.InstanceNumber == 2) )
        {

                if ( AnscEqualString(pWifiSsid->SSID.Cfg.DefaultSSID, pString, TRUE) )
                {
                    return  FALSE;
                }

        }
#endif /* 0 */

        /* save update to backup */
        AnscCopyString( pWifiSsid->SSID.Cfg.SSID, pString );
#if 1//LNT_EMU
	char recName[256];
        memset(recName, 0, sizeof(recName));
        sprintf(recName, BssSsid, pWifiSsid->SSID.Cfg.InstanceNumber);
        if(pWifiSsid->SSID.Cfg.bEnabled == true)
        {
        PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, pWifiSsid->SSID.Cfg.SSID);
	int wlanIndex = pWifiSsid->SSID.Cfg.InstanceNumber - 1;
        wifi_setSSIDName(wlanIndex,&pWifiSsid->SSID.Cfg.SSID);
        }
        else
        {
        strcpy(pWifiSsid->SSID.Cfg.SSID,"OutOfService");
        }
#endif
        pWifiSsid->bSsidChanged = TRUE;
        return TRUE;
    }

#endif


    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        SSID_Validate
            (
                ANSC_HANDLE                 hInsContext,
                char*                       pReturnParamName,
                ULONG*                      puLength
            );

    description:

        This function is called to finally commit all the update.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       pReturnParamName,
                The buffer (128 bytes) of parameter name if there's a validation. 

                ULONG*                      puLength
                The output length of the param name. 

    return:     TRUE if there's no validation.

**********************************************************************/
BOOL
SSID_Validate
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pReturnParamName,
        ULONG*                      puLength
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj      = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_SSID             pWifiSsid     = (PCOSA_DML_WIFI_SSID      )pLinkObj->hContext;
    PSINGLE_LINK_ENTRY              pSLinkEntry   = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pSSIDLinkObj  = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid2    = (PCOSA_DML_WIFI_SSID      )NULL;
 
     pWifiSsid->SSID.DynamicInfo.LastChange             = AnscGetTickInSeconds(); 
    /*Alias should be non-empty*/
    if (AnscSizeOfString(pWifiSsid->SSID.Cfg.Alias) == 0)
    {
        AnscCopyString(pReturnParamName, "Alias");
        *puLength = AnscSizeOfString("Alias");
       // return FALSE;//LNT_EMU
	return TRUE;
    }

    /* Lower Layers has to be non-empty */
    if (AnscSizeOfString(pWifiSsid->SSID.Cfg.WiFiRadioName) == 0)
    {
        AnscCopyString(pReturnParamName, "LowerLayers");
        *puLength = AnscSizeOfString("LowerLayers");
        //return FALSE;//LNT_EMU
	return TRUE;
    }
 
    pSLinkEntry = AnscQueueGetFirstEntry(&pMyObject->SsidQueue);
    
    while ( pSLinkEntry )
    {
        pSSIDLinkObj = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);
        pSLinkEntry  = AnscQueueGetNextEntry(pSLinkEntry);
        pWifiSsid2   = pSSIDLinkObj->hContext;

        if (pSSIDLinkObj == pLinkObj)
        {
            continue;
        }
        
        if (AnscEqualString(pWifiSsid->SSID.Cfg.SSID, pWifiSsid2->SSID.Cfg.SSID, TRUE))
        {
            AnscCopyString(pReturnParamName, "SSID");

            *puLength = AnscSizeOfString("SSID");
           // return FALSE;//LNT_EMU
	    return TRUE;
        }
    }
    
    return TRUE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        SSID_Commit
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to finally commit all the update.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
SSID_Commit
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject    = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_SSID             pWifiSsid    = (PCOSA_DML_WIFI_SSID      )pLinkObj->hContext;
    ANSC_STATUS                     returnStatus = ANSC_STATUS_SUCCESS;
    
    if (pLinkObj->bNew)
    {
        pLinkObj->bNew = FALSE;
        
        returnStatus = CosaDmlWiFiSsidAddEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, &pWifiSsid->SSID);
        
        if (returnStatus != ANSC_STATUS_SUCCESS)
        {
            return ANSC_STATUS_FAILURE;
        }
        
        CosaWifiRegDelSsidInfo((ANSC_HANDLE)pMyObject, (ANSC_HANDLE)pLinkObj);
    }
    else
    {
        if ( !pWifiSsid->bSsidChanged )
        {
            return  ANSC_STATUS_SUCCESS;
        }
        else
        {
            pWifiSsid->bSsidChanged = FALSE;
            CcspTraceInfo(("WiFi SSID -- apply the changes...\n"));
        }
        return CosaDmlWiFiSsidSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, &pWifiSsid->SSID.Cfg);
    }
    
    return 0;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        SSID_Rollback
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to roll back the update whenever there's a 
        validation found.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
SSID_Rollback
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj      = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PSINGLE_LINK_ENTRY              pSLinkEntry   = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pSSIDLinkObj  = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid     = (PCOSA_DML_WIFI_SSID      )NULL;
    ULONG                           idx           = 0;
    
    pSLinkEntry = AnscQueueGetFirstEntry(&pMyObject->SsidQueue);
    
    while ( pSLinkEntry )
    {
        pSSIDLinkObj = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);
        
        if (pSSIDLinkObj == pLinkObj)
        {
            break;
        }
        idx++;
        
        pSLinkEntry             = AnscQueueGetNextEntry(pSLinkEntry);
    }
    
    pWifiSsid   = (PCOSA_DML_WIFI_SSID)pLinkObj->hContext;
    
    return CosaDmlWiFiSsidGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, idx, &pWifiSsid->SSID);  
}

/***********************************************************************

 APIs for Object:

    WiFi.SSID.{i}.Stats.

    *  Stats4_GetParamBoolValue
    *  Stats4_GetParamIntValue
    *  Stats4_GetParamUlongValue
    *  Stats4_GetParamStringValue

***********************************************************************/
/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Stats4_GetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL*                       pBool
            );

    description:

        This function is called to retrieve Boolean parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL*                       pBool
                The buffer of returned boolean value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Stats4_GetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL*                       pBool
    )
{
    /* check the parameter name and return the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Stats4_GetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                int*                        pInt
            );

    description:

        This function is called to retrieve integer parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                int*                        pInt
                The buffer of returned integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Stats4_GetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int*                        pInt
    )
{
    /* check the parameter name and return the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Stats4_GetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG*                      puLong
            );

    description:

        This function is called to retrieve ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG*                      puLong
                The buffer of returned ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Stats4_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG*                      puLong
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject      = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj       = (PCOSA_CONTEXT_LINK_OBJECT )hInsContext;
    PCOSA_DML_WIFI_SSID             pWifiSsid      = (PCOSA_DML_WIFI_SSID       )pLinkObj->hContext;
    PCOSA_DML_WIFI_SSID_STATS       pWifiSsidStats = (PCOSA_DML_WIFI_SSID_STATS )&pWifiSsid->Stats;
    
    CosaDmlWiFiSsidGetStats((ANSC_HANDLE)pMyObject->hPoamWiFiDm,pWifiSsid->SSID.Cfg.InstanceNumber, pWifiSsidStats);

    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "BytesSent", TRUE))
    {
        /* collect value */
        *puLong = pWifiSsidStats->BytesSent;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "BytesReceived", TRUE))
    {
        /* collect value */
        *puLong = pWifiSsidStats->BytesReceived;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "PacketsSent", TRUE))
    {
        /* collect value */
        *puLong = pWifiSsidStats->PacketsSent;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "PacketsReceived", TRUE))
    {
        /* collect value */
        *puLong = pWifiSsidStats->PacketsReceived;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "ErrorsSent", TRUE))
    {
        /* collect value */
        *puLong = pWifiSsidStats->ErrorsSent;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "ErrorsReceived", TRUE))
    {
        /* collect value */
        *puLong = pWifiSsidStats->ErrorsReceived;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "UnicastPacketsSent", TRUE))
    {
        /* collect value */
        *puLong = pWifiSsidStats->UnicastPacketsSent;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "UnicastPacketsReceived", TRUE))
    {
        /* collect value */
        *puLong = pWifiSsidStats->UnicastPacketsReceived;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "DiscardPacketsSent", TRUE))
    {
        /* collect value */
        *puLong = pWifiSsidStats->DiscardPacketsSent;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "DiscardPacketsReceived", TRUE))
    {
        /* collect value */
        *puLong = pWifiSsidStats->DiscardPacketsReceived;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "MulticastPacketsSent", TRUE))
    {
        /* collect value */
        *puLong = pWifiSsidStats->MulticastPacketsSent;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "MulticastPacketsReceived", TRUE))
    {
        /* collect value */
        *puLong = pWifiSsidStats->MulticastPacketsReceived;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "BroadcastPacketsSent", TRUE))
    {
        /* collect value */
        *puLong = pWifiSsidStats->BroadcastPacketsSent;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "BroadcastPacketsReceived", TRUE))
    {
        /* collect value */
        *puLong = pWifiSsidStats->BroadcastPacketsReceived;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "UnknownProtoPacketsReceived", TRUE))
    {
        /* collect value */
        *puLong = pWifiSsidStats->UnknownProtoPacketsReceived;
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        Stats4_GetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pValue,
                ULONG*                      pUlSize
            );

    description:

        This function is called to retrieve string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pValue,
                The string value buffer;

                ULONG*                      pUlSize
                The buffer of length of string value;
                Usually size of 1023 will be used.
                If it's not big enough, put required size here and return 1;

    return:     0 if succeeded;
                1 if short of buffer size; (*pUlSize = required size)
                -1 if not supported.

**********************************************************************/
ULONG
Stats4_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
    /* check the parameter name and return the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return -1;
}

/***********************************************************************

 APIs for Object:

    WiFi.AccessPoint.{i}.

    *  AccessPoint_GetEntryCount
    *  AccessPoint_GetEntry
    *  AccessPoint_AddEntry
    *  AccessPoint_DelEntry
    *  AccessPoint_GetParamBoolValue
    *  AccessPoint_GetParamIntValue
    *  AccessPoint_GetParamUlongValue
    *  AccessPoint_GetParamStringValue
    *  AccessPoint_SetParamBoolValue
    *  AccessPoint_SetParamIntValue
    *  AccessPoint_SetParamUlongValue
    *  AccessPoint_SetParamStringValue
    *  AccessPoint_Validate
    *  AccessPoint_Commit
    *  AccessPoint_Rollback

***********************************************************************/
/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        AccessPoint_GetEntryCount
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to retrieve the count of the table.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The count of the table

**********************************************************************/
ULONG
AccessPoint_GetEntryCount
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    ULONG                           entryCount    = 0;
    
    entryCount = AnscSListQueryDepth(&pMyObject->AccessPointQueue);
    return entryCount;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ANSC_HANDLE
        AccessPoint_GetEntry
            (
                ANSC_HANDLE                 hInsContext,
                ULONG                       nIndex,
                ULONG*                      pInsNumber
            );

    description:

        This function is called to retrieve the entry specified by the index.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                ULONG                       nIndex,
                The index of this entry;

                ULONG*                      pInsNumber
                The output instance number;

    return:     The handle to identify the entry

**********************************************************************/
ANSC_HANDLE
AccessPoint_GetEntry
    (
        ANSC_HANDLE                 hInsContext,
        ULONG                       nIndex,
        ULONG*                      pInsNumber
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject       = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObjAp      = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntryAp   = (PSINGLE_LINK_ENTRY       )NULL;

    pSLinkEntryAp = AnscQueueGetEntryByIndex(&pMyObject->AccessPointQueue, nIndex);
    
    if ( pSLinkEntryAp )
    {
        pLinkObjAp  = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntryAp);

        *pInsNumber = pLinkObjAp->InstanceNumber;
    }

    return pLinkObjAp; /* return the handle */
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ANSC_HANDLE
        AccessPoint_AddEntry
            (
                ANSC_HANDLE                 hInsContext,
                ULONG*                      pInsNumber
            );

    description:

        This function is called to add a new entry.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                ULONG*                      pInsNumber
                The output instance number;

    return:     The handle of new added entry.

**********************************************************************/
ANSC_HANDLE
AccessPoint_AddEntry
    (
        ANSC_HANDLE                 hInsContext,
        ULONG*                      pInsNumber
    )
{

    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj      = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PCOSA_DML_WIFI_AP               pWifiAp       = (PCOSA_DML_WIFI_AP        )NULL;
    
    return NULL; /* Temporarily dont allow addition/deletion of AccessPoints */
    pLinkObj   = (PCOSA_CONTEXT_LINK_OBJECT)AnscAllocateMemory(sizeof(COSA_CONTEXT_LINK_OBJECT));
    
    if (!pLinkObj)
    {
        return NULL;
    }
    
    pWifiAp        = AnscAllocateMemory(sizeof(COSA_DML_WIFI_AP));
    
    if (!pWifiAp)
    {
        AnscFreeMemory(pLinkObj);
        
        return NULL;
    }
    
    if (TRUE)
    {
        pLinkObj->InstanceNumber       = pMyObject->ulAPNextInstance;
    
        pWifiAp->AP.Cfg.InstanceNumber = pMyObject->ulAPNextInstance;
    
        pMyObject->ulAPNextInstance++;

        if ( pMyObject->ulAPNextInstance == 0 )
        {
            pMyObject->ulAPNextInstance = 1;
        }
        
        pLinkObj->hContext     = pWifiAp;
        pLinkObj->hParentTable = NULL;
        pLinkObj->bNew         = TRUE;

        /*Set default Alias*/
        _ansc_sprintf(pWifiAp->AP.Cfg.Alias, "AccessPoint%d", *pInsNumber);
    
        CosaSListPushEntryByInsNum((PSLIST_HEADER)&pMyObject->AccessPointQueue, pLinkObj);
    
        CosaWifiRegAddAPInfo((ANSC_HANDLE)pMyObject, (ANSC_HANDLE)pLinkObj);
    }
    
    *pInsNumber = pLinkObj->InstanceNumber;
    
    return (ANSC_HANDLE)pLinkObj; /* return the handle */
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        AccessPoint_DelEntry
            (
                ANSC_HANDLE                 hInsContext,
                ANSC_HANDLE                 hInstance
            );

    description:

        This function is called to delete an exist entry.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                ANSC_HANDLE                 hInstance
                The exist entry handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
AccessPoint_DelEntry
    (
        ANSC_HANDLE                 hInsContext,
        ANSC_HANDLE                 hInstance
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject    = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInstance;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntry   = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pSSIDLinkObj  = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid     = (PCOSA_DML_WIFI_SSID      )NULL;
    UCHAR                           PathName[64]  = {0};

    return ANSC_STATUS_FAILURE; /*Temporarily we dont allow addition/deletion of AccessPoint entries */
    
    /*
      When an AP is deleted, if it is an orphan one, DM adapter deletes it internally. 
      If it is associated with a SSID, CosaDmlWiFiApSetCfg() is called to pass the configuration to backend,
      with all AP configuration set to default values. 
    */
    pSLinkEntry = AnscQueueGetFirstEntry(&pMyObject->SsidQueue);
    
    pWifiAp = pLinkObj->hContext;

    while ( pSLinkEntry )
    {
        pSSIDLinkObj = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);
        
        sprintf(PathName, "Device.WiFi.SSID.%d.", pSSIDLinkObj->InstanceNumber);
        
        if ( AnscEqualString(pWifiAp->AP.Cfg.SSID, PathName, TRUE) )
        {
            break;
        }    
        
        pSLinkEntry             = AnscQueueGetNextEntry(pSLinkEntry);
    }

    if ( pSLinkEntry )
    {
        pWifiSsid = pSSIDLinkObj->hContext;
        
        /*Set the default Cfg to backend*/
        pWifiAp->AP.Cfg.bEnabled = FALSE;
        pWifiAp->AP.Cfg.SSID[0]  = 0;
        
        CosaDmlWiFiApSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->AP.Cfg);
    }
    
    AnscQueuePopEntryByLink(&pMyObject->AccessPointQueue, &pLinkObj->Linkage);
    
    if (pLinkObj->bNew)
    {
        CosaWifiRegDelAPInfo((ANSC_HANDLE)pMyObject, (ANSC_HANDLE)pLinkObj);
    }
    
    if (pWifiAp->AssocDevices)
    {
        AnscFreeMemory(pWifiAp->AssocDevices);
    }
    if (pLinkObj->hContext)
    {
        AnscFreeMemory(pLinkObj->hContext);
    }
    if (pLinkObj)
    {
        AnscFreeMemory(pLinkObj);
    }
    
    return ANSC_STATUS_SUCCESS;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        AccessPoint_GetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL*                       pBool
            );

    description:

        This function is called to retrieve Boolean parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL*                       pBool
                The buffer of returned boolean value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
AccessPoint_GetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL*                       pBool
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject    = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;	
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PSINGLE_LINK_ENTRY              pSLinkEntry  = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pSSIDLinkObj = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid    = (PCOSA_DML_WIFI_SSID      )NULL;
    UCHAR                           PathName[64] = {0};	
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "Enable", TRUE))
    {
	/* collect value */
        int wlanIndex;
        BOOL enabled = FALSE;

        pSLinkEntry = AnscQueueGetFirstEntry(&pMyObject->SsidQueue);
        while ( pSLinkEntry )
        {
            pSSIDLinkObj = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);
            pWifiSsid    = pSSIDLinkObj->hContext;
            sprintf(PathName, "Device.WiFi.SSID.%d.", pSSIDLinkObj->InstanceNumber);

            if ( AnscEqualString(pWifiAp->AP.Cfg.SSID, PathName, TRUE) )
            {
                break;
            }
            pSLinkEntry = AnscQueueGetNextEntry(pSLinkEntry);
        }
        if (pSLinkEntry)
        {
	    int wlanIndex = pWifiAp->AP.Cfg.InstanceNumber - 1;
            wifi_getApEnable(wlanIndex,&enabled);
        }

        *pBool = enabled;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "SSIDAdvertisementEnabled", TRUE))
    {
        /* collect value */
        *pBool = pWifiAp->AP.Cfg.SSIDAdvertisementEnabled;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "WMMCapability", TRUE))
    {
        /* collect value */
        *pBool = pWifiAp->AP.Info.WMMCapability;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "UAPSDCapability", TRUE))
    {
        /* collect value */
        *pBool = pWifiAp->AP.Info.UAPSDCapability;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "WMMEnable", TRUE))
    {
        /* collect value */
        *pBool = pWifiAp->AP.Cfg.WMMEnable;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "UAPSDEnable", TRUE))
    {
        /* collect value */
        *pBool = pWifiAp->AP.Cfg.UAPSDEnable;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_BssCountStaAsCpe", TRUE))
    {
        /* collect value */
        *pBool = pWifiAp->AP.Cfg.BssCountStaAsCpe;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "X_CISCO_COM_BssHotSpot", TRUE))
    {
        /* collect value */
        *pBool = pWifiAp->AP.Cfg.BssHotSpot;
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        AccessPoint_GetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                int*                        pInt
            );

    description:

        This function is called to retrieve integer parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                int*                        pInt
                The buffer of returned integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
AccessPoint_GetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int*                        pInt
    )
{
    /* check the parameter name and return the corresponding value */
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;

    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "X_CISCO_COM_WmmNoAck", TRUE))
    {
        *pInt = pWifiAp->AP.Cfg.WmmNoAck;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "X_CISCO_COM_MulticastRate", TRUE))
    {
        *pInt = pWifiAp->AP.Cfg.MulticastRate;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "X_CISCO_COM_BssMaxNumSta", TRUE))
    {
        *pInt = pWifiAp->AP.Cfg.BssMaxNumSta;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "X_CISCO_COM_BssUserStatus", TRUE))
    {
        *pInt = pWifiAp->AP.Info.BssUserStatus;
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        AccessPoint_GetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG*                      puLong
            );

    description:

        This function is called to retrieve ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG*                      puLong
                The buffer of returned ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
AccessPoint_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG*                      puLong
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject    = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PSINGLE_LINK_ENTRY              pSLinkEntry  = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pSSIDLinkObj = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid    = (PCOSA_DML_WIFI_SSID      )NULL;
    UCHAR                           PathName[64] = {0};
    
    pSLinkEntry = AnscQueueGetFirstEntry(&pMyObject->SsidQueue);
    
    while ( pSLinkEntry )
    {
        pSSIDLinkObj = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);
        pWifiSsid    = pSSIDLinkObj->hContext;
        
        sprintf(PathName, "Device.WiFi.SSID.%d.", pSSIDLinkObj->InstanceNumber);
        
        if ( AnscEqualString(pWifiAp->AP.Cfg.SSID, PathName, TRUE) )
        {
            break;
        }    
        
        pSLinkEntry             = AnscQueueGetNextEntry(pSLinkEntry);
    }
    
    if (pSLinkEntry)
    {
        CosaDmlWiFiApGetInfo((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->AP.Info);
    }
    
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "Status", TRUE))
    {
        /* collect value */
        *puLong = pWifiAp->AP.Info.Status;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "RetryLimit", TRUE))
    {
        /* collect value */
        *puLong = pWifiAp->AP.Cfg.RetryLimit;
        return TRUE;
    }


    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        AccessPoint_GetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pValue,
                ULONG*                      pUlSize
            );

    description:

        This function is called to retrieve string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pValue,
                The string value buffer;

                ULONG*                      pUlSize
                The buffer of length of string value;
                Usually size of 1023 will be used.
                If it's not big enough, put required size here and return 1;

    return:     0 if succeeded;
                1 if short of buffer size; (*pUlSize = required size)
                -1 if not supported.

**********************************************************************/
ULONG
AccessPoint_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj      = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp       = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_AP_MF_CFG        pWifiApMf     = (PCOSA_DML_WIFI_APWPS_FULL)&pWifiAp->MF;

    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "Alias", TRUE))
    {
        /* collect value */
        if ( AnscSizeOfString(pWifiAp->AP.Cfg.Alias) < *pUlSize)
        {
            AnscCopyString(pValue, pWifiAp->AP.Cfg.Alias);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiAp->AP.Cfg.Alias)+1;
            return 1;
        }
        return 0;
    }

    if( AnscEqualString(ParamName, "SSIDReference", TRUE))
    {
        /* collect value */
        if ( AnscSizeOfString(pWifiAp->AP.Cfg.SSID) < *pUlSize)
        {
            AnscCopyString(pValue, pWifiAp->AP.Cfg.SSID);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiAp->AP.Cfg.SSID)+1;
            return 1;
        }
        return 0;
    }
    if(AnscEqualString(ParamName, "X_COMCAST-COM_MAC_FilteringMode", TRUE))
    {
       AnscCopyString(pValue,pWifiAp->AP.Cfg.MacFilterMode);
        /* collect value */

       return TRUE;
    }


    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return -1;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        AccessPoint_SetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL                        bValue
            );

    description:

        This function is called to set BOOL parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL                        bValue
                The updated BOOL value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
AccessPoint_SetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL                        bValue
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    int wlanIndex = pWifiAp->AP.Cfg.InstanceNumber - 1;
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "Enable", TRUE))
    {
        /* Currently APs are always enabled */
        /* pWifiAp->AP.Cfg.bEnabled = bValue; */
        /* return TRUE; */
        return FALSE;
    }

    if( AnscEqualString(ParamName, "SSIDAdvertisementEnabled", TRUE))
    {
        if ( pWifiAp->AP.Cfg.SSIDAdvertisementEnabled == bValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiAp->AP.Cfg.SSIDAdvertisementEnabled = bValue;
	wifi_setApSsidAdvertisementEnable(wlanIndex,pWifiAp->AP.Cfg.SSIDAdvertisementEnabled);//RDKB-EMU
        pWifiAp->bApChanged = TRUE;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "WMMEnable", TRUE))
    {
        if ( pWifiAp->AP.Cfg.WMMEnable == bValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiAp->AP.Cfg.WMMEnable = bValue;
        pWifiAp->bApChanged = TRUE;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "UAPSDEnable", TRUE))
    {
        if ( pWifiAp->AP.Cfg.UAPSDEnable == bValue )
        {
            return  TRUE;
        }
        /* save update to backup */
        pWifiAp->AP.Cfg.UAPSDEnable = bValue;
        pWifiAp->bApChanged = TRUE;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_BssCountStaAsCpe", TRUE))
    {
        if ( pWifiAp->AP.Cfg.BssCountStaAsCpe == bValue )
        {
            return  TRUE;
        }
        /* save update to backup */
        pWifiAp->AP.Cfg.BssCountStaAsCpe = bValue;
        pWifiAp->bApChanged = TRUE;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "X_CISCO_COM_BssHotSpot", TRUE))
    {
        if ( pWifiAp->AP.Cfg.BssHotSpot == bValue )
        {
            return  TRUE;
        }
        /* save update to backup */
        pWifiAp->AP.Cfg.BssHotSpot = bValue;
        pWifiAp->bApChanged = TRUE;
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        AccessPoint_SetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                int                         iValue
            );

    description:

        This function is called to set integer parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                int                         iValue
                The updated integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
AccessPoint_SetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int                         iValue
    )
{
    /* check the parameter name and set the corresponding value */
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;

    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "X_CISCO_COM_WmmNoAck", TRUE))
    {
        if ( pWifiAp->AP.Cfg.WmmNoAck == iValue )
        {
            return  TRUE;
        }
        /* save update to backup */
        pWifiAp->AP.Cfg.WmmNoAck = iValue;
        pWifiAp->bApChanged = TRUE;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "X_CISCO_COM_MulticastRate", TRUE))
    {
        if ( pWifiAp->AP.Cfg.MulticastRate == iValue )
        {
            return  TRUE;
        }
        /* save update to backup */
        pWifiAp->AP.Cfg.MulticastRate = iValue;
        pWifiAp->bApChanged = TRUE;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_BssMaxNumSta", TRUE))
    {
        if ( pWifiAp->AP.Cfg.BssMaxNumSta == iValue )
        {
            return  TRUE;
        }
        /* save update to backup */
        pWifiAp->AP.Cfg.BssMaxNumSta = iValue;
        pWifiAp->bApChanged = TRUE;
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        AccessPoint_SetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG                       uValue
            );

    description:

        This function is called to set ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG                       uValue
                The updated ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
AccessPoint_SetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG                       uValue
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "RetryLimit", TRUE))
    {
        if ( pWifiAp->AP.Cfg.RetryLimit == uValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiAp->AP.Cfg.RetryLimit = uValue;
        pWifiAp->bApChanged = TRUE;
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        AccessPoint_SetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pString
            );

    description:

        This function is called to set string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pString
                The updated string value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
AccessPoint_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pString
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_AP_MF_CFG        pWifiApMf     = (PCOSA_DML_WIFI_APWPS_FULL)&pWifiAp->MF;
    
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "Alias", TRUE))
    {
        /* save update to backup */
        AnscCopyString(pWifiAp->AP.Cfg.Alias, pString);
        return TRUE;
    }

    if( AnscEqualString(ParamName, "SSIDReference", TRUE))
    {
        /* save update to backup */
    #ifdef _COSA_SIM_
        AnscCopyString(pWifiAp->AP.Cfg.SSID, pString);
        return TRUE;
    #else
        /* Currently we dont allow to change this - May be when multi-SSID comes in */
        return FALSE;
    #endif
    }

    if(AnscEqualString(ParamName, "X_COMCAST-COM_MAC_FilteringMode", TRUE))
    {
       AnscCopyString(pWifiAp->AP.Cfg.MacFilterMode,pString);
       isChanged = 1;
        /* collect value */

       return TRUE;

    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        AccessPoint_Validate
            (
                ANSC_HANDLE                 hInsContext,
                char*                       pReturnParamName,
                ULONG*                      puLength
            );

    description:

        This function is called to finally commit all the update.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       pReturnParamName,
                The buffer (128 bytes) of parameter name if there's a validation. 

                ULONG*                      puLength
                The output length of the param name. 

    return:     TRUE if there's no validation.

**********************************************************************/
BOOL
AccessPoint_Validate
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pReturnParamName,
        ULONG*                      puLength
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj      = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp       = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_AP               pWifiApE      = (PCOSA_DML_WIFI_AP        )NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntry   = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pSSIDLinkObj  = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pAPLinkObj    = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    UCHAR                           PathName[64]  = {0};
  
    /*Alias should be non-empty*/
    if (AnscSizeOfString(pWifiAp->AP.Cfg.Alias) == 0)
    {
        AnscCopyString(pReturnParamName, "Alias");
        *puLength = AnscSizeOfString("Alias");

       // return FALSE;
          return TRUE;
    }
 
    /* Retry Limit should be between 0 and 7 */
    if((pWifiAp->AP.Cfg.RetryLimit < 0) || (pWifiAp->AP.Cfg.RetryLimit > 7))
    {
        AnscCopyString(pReturnParamName, "RetryLimit");
        *puLength = sizeof("RetryLimit");
        goto EXIT;
    }

    /* UAPSD can not be enabled when WMM is disabled */
    if((FALSE == pWifiAp->AP.Cfg.WMMEnable) && (TRUE == pWifiAp->AP.Cfg.UAPSDEnable))
    {
       AnscCopyString(pReturnParamName, "UAPSDEnable");
       *puLength = sizeof("UAPSDEnable");
        goto EXIT;
    }
 
    /*SSIDRefence should be unique*/
    pSLinkEntry = AnscQueueGetFirstEntry(&pMyObject->AccessPointQueue);
    while ( pSLinkEntry )
    {
        pAPLinkObj   = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);
        pWifiApE     = pAPLinkObj->hContext;
        pSLinkEntry  = AnscQueueGetNextEntry(pSLinkEntry);
        
        if (pWifiApE == pWifiAp)
        {
            continue;
        }
        
        if (AnscEqualString(pWifiAp->AP.Cfg.SSID, pWifiApE->AP.Cfg.SSID, TRUE))
        {
            memset(pWifiAp->AP.Cfg.SSID, 0, sizeof(pWifiAp->AP.Cfg.SSID));
    
            AnscCopyString(pReturnParamName, "SSIDReference");
            *puLength = sizeof("SSIDReference");
    
            goto EXIT;
        }
    }
    
    /*SSIDReference should be valid*/
    pSLinkEntry = AnscQueueGetFirstEntry(&pMyObject->SsidQueue);
    
    while ( pSLinkEntry )
    {
        pSSIDLinkObj = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);
        
        sprintf(PathName, "Device.WiFi.SSID.%d.", pSSIDLinkObj->InstanceNumber);
        
        /*see whether the corresponding SSID entry exists*/
        if ( AnscEqualString(pWifiAp->AP.Cfg.SSID, PathName, TRUE) )
        {
            return TRUE;
        }
        
        pSLinkEntry             = AnscQueueGetNextEntry(pSLinkEntry);
    }
    
    memset(pWifiAp->AP.Cfg.SSID, 0, sizeof(pWifiAp->AP.Cfg.SSID));
    
    AnscCopyString(pReturnParamName, "SSIDReference");
    *puLength = sizeof("SSIDReference");
    
    goto EXIT;
    
EXIT:
//    return FALSE;
      return TRUE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        AccessPoint_Commit
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to finally commit all the update.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
AccessPoint_Commit
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj      = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp       = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PSINGLE_LINK_ENTRY              pSLinkEntry   = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pSSIDLinkObj  = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid     = (PCOSA_DML_WIFI_SSID      )NULL;
    UCHAR                           PathName[64]  = {0};
    ANSC_STATUS                     returnStatus  = ANSC_STATUS_SUCCESS;
    
    pSLinkEntry = AnscQueueGetFirstEntry(&pMyObject->SsidQueue);
    
    while ( pSLinkEntry )
    {
        pSSIDLinkObj = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);
        
        sprintf(PathName, "Device.WiFi.SSID.%d.", pSSIDLinkObj->InstanceNumber);
        
        if ( AnscEqualString(pWifiAp->AP.Cfg.SSID, PathName, TRUE) )
        {
            break;
        }    
        
        pSLinkEntry             = AnscQueueGetNextEntry(pSLinkEntry);
    }
    
    if ( pSLinkEntry )
    {
        pWifiSsid = pSSIDLinkObj->hContext;

        if ( !pWifiAp->bApChanged )
        {
            return  ANSC_STATUS_SUCCESS;
        }
        else
        {
            pWifiAp->bApChanged = FALSE;
            CcspTraceInfo(("WiFi AP -- apply the changes...\n"));
        }  
        /*Set to backend*/
        returnStatus = CosaDmlWiFiApSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->AP.Cfg);

        if (returnStatus != ANSC_STATUS_SUCCESS)
        {
            return ANSC_STATUS_FAILURE;
        }
    
        /*This is not an orphan entry*/
        if (pLinkObj->bNew == TRUE)
        {
            pLinkObj->bNew = FALSE;
        
            CosaWifiRegDelAPInfo((ANSC_HANDLE)pMyObject, (ANSC_HANDLE)pLinkObj);
        }
        
        return ANSC_STATUS_SUCCESS;
    }
    else
    {
        /*This is an orphan entry*/
        if (pLinkObj->bNew == FALSE)
        {
            pLinkObj->bNew = TRUE;
        
            CosaWifiRegAddAPInfo((ANSC_HANDLE)pMyObject, (ANSC_HANDLE)pLinkObj);
        }
    
        return ANSC_STATUS_FAILURE;
    }
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        AccessPoint_Rollback
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to roll back the update whenever there's a 
        validation found.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
AccessPoint_Rollback
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject       = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObjAp      = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp         = (PCOSA_DML_WIFI_AP        )pLinkObjAp->hContext;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObjSsid    = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntrySsid = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid       = (PCOSA_DML_WIFI_SSID      )NULL;
    UCHAR                           PathName[64]    = {0};
    
    /*Get the corresponding SSID entry*/
    pSLinkEntrySsid = AnscQueueGetFirstEntry(&pMyObject->SsidQueue);
    while ( pSLinkEntrySsid )
    {
        pLinkObjSsid = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntrySsid);
        
        sprintf(PathName, "Device.WiFi.SSID.%d.", pLinkObjSsid->InstanceNumber);
        
        if ( AnscEqualString(pWifiAp->AP.Cfg.SSID, PathName, TRUE) )
        {
            break;
        }
                   
        pSLinkEntrySsid             = AnscQueueGetNextEntry(pSLinkEntrySsid);
    }
    
    if ( pSLinkEntrySsid )
    {
        pWifiSsid = pLinkObjSsid->hContext;
        
        CosaDmlWiFiApGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->AP); 
        
        return ANSC_STATUS_SUCCESS;
    }
    
    return ANSC_STATUS_SUCCESS;
}


/***********************************************************************

 APIs for Object:

    WiFi.AccessPoint.{i}.Security.

    *  Security_GetParamBoolValue
    *  Security_GetParamIntValue
    *  Security_GetParamUlongValue
    *  Security_GetParamStringValue
    *  Security_SetParamBoolValue
    *  Security_SetParamIntValue
    *  Security_SetParamUlongValue
    *  Security_SetParamStringValue
    *  Security_Validate
    *  Security_Commit
    *  Security_Rollback

***********************************************************************/
/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Security_GetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL*                       pBool
            );

    description:

        This function is called to retrieve Boolean parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL*                       pBool
                The buffer of returned boolean value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Security_GetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL*                       pBool
    )
{
    /* check the parameter name and return the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Security_GetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                int*                        pInt
            );

    description:

        This function is called to retrieve integer parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                int*                        pInt
                The buffer of returned integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Security_GetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int*                        pInt
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_APSEC_FULL       pWifiApSec   = (PCOSA_DML_WIFI_APSEC_FULL)&pWifiAp->SEC;
    /* check the parameter name and return the corresponding value */

    if( AnscEqualString(ParamName, "X_CISCO_COM_RadiusReAuthInterval", TRUE))
    {
        /* collect value */
        *pInt = pWifiApSec->Cfg.RadiusReAuthInterval;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "X_CISCO_COM_DefaultKey", TRUE))
    {
        /* collect value */
        *pInt = pWifiApSec->Cfg.DefaultKey;
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************
    caller:     owner of this object 

    prototype: 

        BOOL
        Security_GetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG*                      puLong
            );

    description:

        This function is called to retrieve ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG*                      puLong
                The buffer of returned ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Security_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG*                      puLong
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_APSEC_FULL       pWifiApSec   = (PCOSA_DML_WIFI_APSEC_FULL)&pWifiAp->SEC;
    /* check the parameter name and return the corresponding value */

    if( AnscEqualString(ParamName, "RekeyingInterval", TRUE))
    {
        /* collect value */
        *puLong = pWifiApSec->Cfg.RekeyingInterval;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_EncryptionMethod", TRUE))
    {
        /* collect value */
        *puLong = pWifiApSec->Cfg.EncryptionMethod;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "RadiusServerIPAddr", TRUE))
    {
        /* collect value */
        *puLong = pWifiApSec->Cfg.RadiusServerIPAddr.Value;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "RadiusServerPort", TRUE))
    {
        /* collect value */
        *puLong = pWifiApSec->Cfg.RadiusServerPort;
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        Security_GetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pValue,
                ULONG*                      pUlSize
            );

    description:

        This function is called to retrieve string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pValue,
                The string value buffer;

                ULONG*                      pUlSize
                The buffer of length of string value;
                Usually size of 1023 will be used.
                If it's not big enough, put required size here and return 1;

    return:     0 if succeeded;
                1 if short of buffer size; (*pUlSize = required size)
                -1 if not supported.

**********************************************************************/
ULONG
Security_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_APSEC_FULL       pWifiApSec   = (PCOSA_DML_WIFI_APSEC_FULL)&pWifiAp->SEC;
    char                            password[50] = {0};
    char 			    SecurityMode[50] = {0};
    int wlanIndex = pWifiAp->AP.Cfg.InstanceNumber - 1;
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "ModesSupported", TRUE))
    {
        /* collect value */
        char buf[512] = {0};
        if (pWifiApSec->Info.ModesSupported & COSA_DML_WIFI_SECURITY_None )
        {
            strcat(buf, "None");
        }

        if ( pWifiApSec->Info.ModesSupported & COSA_DML_WIFI_SECURITY_WEP_64 )
        {
            if (AnscSizeOfString(buf) != 0)
            {
                strcat(buf, ",WEP-64");
            }
            else
            {
                strcat(buf, "WEP-64");
            }
        }

        if (pWifiApSec->Info.ModesSupported & COSA_DML_WIFI_SECURITY_WEP_128 )
        {
            if (AnscSizeOfString(buf) != 0)
            {
                strcat(buf, ",WEP-128");
            }
            else
            {
                strcat(buf, "WEP-128");
            }
        }

        if ( pWifiApSec->Info.ModesSupported & COSA_DML_WIFI_SECURITY_WPA_Personal)
        {
            if (AnscSizeOfString(buf) != 0)
            {
                strcat(buf, ",WPA-Personal");
            }
            else
            {
                strcat(buf, "WPA-Personal");
            }
        }

        if ( pWifiApSec->Info.ModesSupported & COSA_DML_WIFI_SECURITY_WPA2_Personal)
        {
            if (AnscSizeOfString(buf) != 0)
            {
                strcat(buf, ",WPA2-Personal");
            }
            else
            {
                strcat(buf, "WPA2-Personal");
            }
        }

        if ( pWifiApSec->Info.ModesSupported & COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal)
        {
            if (AnscSizeOfString(buf) != 0)
            {
                strcat(buf, ",WPA-WPA2-Personal");
            }
            else
            {
                strcat(buf, "WPA-WPA2-Personal");
            }
        }

        if ( pWifiApSec->Info.ModesSupported & COSA_DML_WIFI_SECURITY_WPA_Enterprise)
        {
            if (AnscSizeOfString(buf) != 0)
            {
                strcat(buf, ",WPA-Enterprise");
            }
            else
            {
                strcat(buf, "WPA-Enterprise");
            }
        }

        if ( pWifiApSec->Info.ModesSupported & COSA_DML_WIFI_SECURITY_WPA2_Enterprise)
        {
            if (AnscSizeOfString(buf) != 0)
            {
                strcat(buf, ",WPA2-Enterprise");
            }
            else
            {
                strcat(buf, "WPA2-Enterprise");
            }
        }

        if ( pWifiApSec->Info.ModesSupported & COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise)
        {
            if (AnscSizeOfString(buf) != 0)
            {
                strcat(buf, ",WPA-WPA2-Enterprise");
            }
            else
            {
                strcat(buf, "WPA-WPA2-Enterprise");
            }
        }
        if ( AnscSizeOfString(buf) < *pUlSize)
        {
            AnscCopyString(pValue, buf);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(buf)+1;

            return 1;
        }
        return 0;
    }
 
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "ModeEnabled", TRUE))
    {
        /* collect value */
		    //wifi_getApSecurityModeEnabled(pWifiAp->AP.Cfg.InstanceNumber,SecurityMode);//RDKB-EMU
		    wifi_getApSecurityModeEnabled(wlanIndex,SecurityMode);//RDKB-EMU
                    if(strcmp(SecurityMode,"WPA-Personal") == 0)
                                pWifiApSec->Cfg.ModeEnabled = COSA_DML_WIFI_SECURITY_WPA_Personal;
                    else if(strcmp(SecurityMode,"WPA2-Personal") == 0)
                                pWifiApSec->Cfg.ModeEnabled = COSA_DML_WIFI_SECURITY_WPA2_Personal;
                    else if(strcmp(SecurityMode,"WPA-WPA2-Personal") == 0)
                                pWifiApSec->Cfg.ModeEnabled = COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal;
                    else if(strcmp(SecurityMode,"None") == 0)
                                pWifiApSec->Cfg.ModeEnabled = COSA_DML_WIFI_SECURITY_None;


        if ( 20 < *pUlSize)
        {
            if (pWifiApSec->Cfg.ModeEnabled & COSA_DML_WIFI_SECURITY_None )
            {
                AnscCopyString(pValue, "None");
            }
            else if (pWifiApSec->Cfg.ModeEnabled & COSA_DML_WIFI_SECURITY_WEP_64 )
            {
                AnscCopyString(pValue, "WEP-64");
            }
            else if (pWifiApSec->Cfg.ModeEnabled & COSA_DML_WIFI_SECURITY_WEP_128 )
            {
                AnscCopyString(pValue, "WEP-128");
            }
            else if (pWifiApSec->Cfg.ModeEnabled & COSA_DML_WIFI_SECURITY_WPA_Personal )
            {
                AnscCopyString(pValue, "WPA-Personal");
            }
            else if (pWifiApSec->Cfg.ModeEnabled & COSA_DML_WIFI_SECURITY_WPA2_Personal )
            {
                AnscCopyString(pValue, "WPA2-Personal");
            }
            else if (pWifiApSec->Cfg.ModeEnabled & COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal )
            {
                AnscCopyString(pValue, "WPA-WPA2-Personal");
            }
            else if (pWifiApSec->Cfg.ModeEnabled & COSA_DML_WIFI_SECURITY_WPA_Enterprise )
            {
                AnscCopyString(pValue, "WPA-Enterprise");
            }
            else if (pWifiApSec->Cfg.ModeEnabled & COSA_DML_WIFI_SECURITY_WPA2_Enterprise )
            {
                AnscCopyString(pValue, "WPA2-Enterprise");
            }
            else if (pWifiApSec->Cfg.ModeEnabled & COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise )
            {
                AnscCopyString(pValue, "WPA-WPA2-Enterprise");
            }
            return 0;
        }
        else
        {
            *pUlSize = 20;
            return 1;
        }
        return 0;
    }

    if( AnscEqualString(ParamName, "WEPKey", TRUE))
    {
#ifdef _COSA_SIM_
        if (pWifiApSec->Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_64 )
        {
            _ansc_sprintf
            (
                pValue,
                "%02X%02X%02X%02X%02X",
                pWifiApSec->Cfg.WEPKeyp[0],
                pWifiApSec->Cfg.WEPKeyp[1],
                pWifiApSec->Cfg.WEPKeyp[2],
                pWifiApSec->Cfg.WEPKeyp[3],
                pWifiApSec->Cfg.WEPKeyp[4]
            );
        }
        else if ( pWifiApSec->Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_128 )
        {
            _ansc_sprintf
            (
                pValue,
                "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
                pWifiApSec->Cfg.WEPKeyp[0],
                pWifiApSec->Cfg.WEPKeyp[1],
                pWifiApSec->Cfg.WEPKeyp[2],
                pWifiApSec->Cfg.WEPKeyp[3],
                pWifiApSec->Cfg.WEPKeyp[4],
                pWifiApSec->Cfg.WEPKeyp[5],
                pWifiApSec->Cfg.WEPKeyp[6],
                pWifiApSec->Cfg.WEPKeyp[7],
                pWifiApSec->Cfg.WEPKeyp[8],
                pWifiApSec->Cfg.WEPKeyp[9],
                pWifiApSec->Cfg.WEPKeyp[10],
                pWifiApSec->Cfg.WEPKeyp[11],
                pWifiApSec->Cfg.WEPKeyp[12]
            );
        }
        else
        {
            return -1;
        }
#else
        /* WEP Key should always return empty string when read */
        AnscCopyString(pValue, "");
#endif        
        /* collect value */
        return 0;
    }

    if( AnscEqualString(ParamName, "PreSharedKey", TRUE))
    {
#ifdef _COSA_SIM_
        /* collect value */
        if ( AnscSizeOfString(pWifiApSec->Cfg.PreSharedKey) < *pUlSize)
        {
	    wifi_getApSecurityPreSharedKey(wlanIndex,pWifiApSec->Cfg.PreSharedKey);
            AnscCopyString(pValue, pWifiApSec->Cfg.PreSharedKey);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiApSec->Cfg.PreSharedKey)+1;
            return 1;
        }
#else
        /* PresharedKey should always return empty string when read */
        AnscCopyString(pValue, "");
#endif
        return 0;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_KeyPassphrase", TRUE) || AnscEqualString(ParamName, "KeyPassphrase", TRUE))
    {
#ifdef _COSA_SIM_
#if 1 
	    int wlanIndex = pWifiAp->AP.Cfg.InstanceNumber - 1;
	    if((pWifiAp->AP.Cfg.InstanceNumber == 1) || (pWifiAp->AP.Cfg.InstanceNumber == 2))
	    {
	    wifi_getApSecurityPreSharedKey(wlanIndex,password);
	    if(strcmp(password,"")==0)
	    {
		    pWifiApSec->Cfg.ModeEnabled = COSA_DML_WIFI_SECURITY_None;
		    AnscCopyString(pWifiApSec->Cfg.KeyPassphrase, "");
	    }
	    else
	    {
		    AnscCopyString(pWifiApSec->Cfg.KeyPassphrase, password);
	    }
	    }
#endif

	    /* collect value */
	    if ( AnscSizeOfString(pWifiApSec->Cfg.KeyPassphrase) < *pUlSize)
	    {
		    AnscCopyString(pValue, pWifiApSec->Cfg.KeyPassphrase);
		    return 0;
	    }
	    else
	    {
		    *pUlSize = AnscSizeOfString(pWifiApSec->Cfg.KeyPassphrase)+1;
		    return 1;
	    }
#else
	    /* Key Passphrase should always return empty string when read */
	    AnscCopyString(pValue, "");
#endif
	    return 0;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_WEPKey", TRUE))
    {
        if (pWifiApSec->Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_64 )
        {
            _ansc_sprintf
            (
                pValue,
                "%02X%02X%02X%02X%02X",
                pWifiApSec->Cfg.WEPKeyp[0],
                pWifiApSec->Cfg.WEPKeyp[1],
                pWifiApSec->Cfg.WEPKeyp[2],
                pWifiApSec->Cfg.WEPKeyp[3],
                pWifiApSec->Cfg.WEPKeyp[4]
            );
        }
        else if ( pWifiApSec->Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_128 )
        {
            _ansc_sprintf
            (
                pValue,
                "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
                pWifiApSec->Cfg.WEPKeyp[0],
                pWifiApSec->Cfg.WEPKeyp[1],
                pWifiApSec->Cfg.WEPKeyp[2],
                pWifiApSec->Cfg.WEPKeyp[3],
                pWifiApSec->Cfg.WEPKeyp[4],
                pWifiApSec->Cfg.WEPKeyp[5],
                pWifiApSec->Cfg.WEPKeyp[6],
                pWifiApSec->Cfg.WEPKeyp[7],
                pWifiApSec->Cfg.WEPKeyp[8],
                pWifiApSec->Cfg.WEPKeyp[9],
                pWifiApSec->Cfg.WEPKeyp[10],
                pWifiApSec->Cfg.WEPKeyp[11],
                pWifiApSec->Cfg.WEPKeyp[12]
            );
        }
        else
        {
            return -1;
        }
        
        /* collect value */
        return 0;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_KeyPassphrase", TRUE))
    {
        /* collect value */
        if ( AnscSizeOfString(pWifiApSec->Cfg.KeyPassphrase) < *pUlSize)
        {
            AnscCopyString(pValue, pWifiApSec->Cfg.KeyPassphrase);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiApSec->Cfg.KeyPassphrase)+1;
            return 1;
        }
        return 0;
    }

    if( AnscEqualString(ParamName, "RadiusSecret", TRUE))
    {
        /* Radius Secret should always return empty string when read */
        AnscCopyString(pValue, "");
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return -1;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Security_SetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL                        bValue
            );

    description:

        This function is called to set BOOL parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL                        bValue
                The updated BOOL value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Security_SetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL                        bValue
    )
{
    /* check the parameter name and set the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Security_SetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                int                         iValue
            );

    description:

        This function is called to set integer parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                int                         iValue
                The updated integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Security_SetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int                         iValue
    )
{
    /* check the parameter name and set the corresponding value */
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_APSEC_FULL       pWifiApSec   = (PCOSA_DML_WIFI_APSEC_FULL)&pWifiAp->SEC;
    
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "X_CISCO_COM_RadiusReAuthInterval", TRUE))
    {
        if ( pWifiApSec->Cfg.RadiusReAuthInterval != iValue )
        {
            /* save update to backup */
            pWifiApSec->Cfg.RadiusReAuthInterval = iValue;
            pWifiAp->bSecChanged             = TRUE;
        }

        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_DefaultKey", TRUE))
    {
        if ( pWifiApSec->Cfg.DefaultKey != iValue )
        {
            /* save update to backup */
            pWifiApSec->Cfg.DefaultKey = iValue;
            pWifiAp->bSecChanged             = TRUE;
        }

        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Security_SetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG                       uValue
            );

    description:

        This function is called to set ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG                       uValue
                The updated ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Security_SetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG                       uValue
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_APSEC_FULL       pWifiApSec   = (PCOSA_DML_WIFI_APSEC_FULL)&pWifiAp->SEC;
    
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "RekeyingInterval", TRUE))
    {
        if ( pWifiApSec->Cfg.RekeyingInterval != uValue )
        {
            /* save update to backup */
            pWifiApSec->Cfg.RekeyingInterval = uValue;
            pWifiAp->bSecChanged             = TRUE;
        }

        return TRUE;
    }
  
    if( AnscEqualString(ParamName, "X_CISCO_COM_EncryptionMethod", TRUE))
    {
        if ( pWifiApSec->Cfg.EncryptionMethod != uValue )
        {
            /* collect value */
            pWifiApSec->Cfg.EncryptionMethod = uValue;
            pWifiAp->bSecChanged             = TRUE;
        }
        return TRUE;
    }

    if( AnscEqualString(ParamName, "RadiusServerIPAddr", TRUE))
    {
        if ( pWifiApSec->Cfg.RadiusServerIPAddr.Value != uValue )
        {
            /* save update to backup */
            pWifiApSec->Cfg.RadiusServerIPAddr.Value = uValue;
            pWifiAp->bSecChanged             = TRUE;
        }
        return TRUE;
    }

    if( AnscEqualString(ParamName, "RadiusServerPort", TRUE))
    {
        if ( pWifiApSec->Cfg.RadiusServerPort != uValue )
        {
            /* save update to backup */
            pWifiApSec->Cfg.RadiusServerPort = uValue;
            pWifiAp->bSecChanged             = TRUE;
        }
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Security_SetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pString
            );

    description:

        This function is called to set string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pString
                The updated string value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Security_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pString
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_APSEC_FULL       pWifiApSec   = (PCOSA_DML_WIFI_APSEC_FULL)&pWifiAp->SEC;
    UCHAR                           tmpWEPKey[14]= {'\0'};
    
    char recName[256] = {0};//LNT_EMU
    char str[100] ={0};
    int  wlanIndex = pWifiAp->AP.Cfg.InstanceNumber - 1;
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "ModeEnabled", TRUE))
    {
        COSA_DML_WIFI_SECURITY      TmpMode;
        
        /* save update to backup */
        if ( AnscEqualString(pString, "None", TRUE) )
        {
            TmpMode = COSA_DML_WIFI_SECURITY_None;
	    //wifi_setApSecurityModeEnabled(pWifiAp->AP.Cfg.InstanceNumber,pString);//RDKB-EMU
	    wifi_setApSecurityModeEnabled(wlanIndex,pString);//RDKB-EMU

        }
        else if ( AnscEqualString(pString, "WEP-64", TRUE) )
        {
            TmpMode  = COSA_DML_WIFI_SECURITY_WEP_64;
        }
        else if ( AnscEqualString(pString, "WEP-128", TRUE) )
        {
            TmpMode  = COSA_DML_WIFI_SECURITY_WEP_128;
 	}
        else if ( AnscEqualString(pString, "WPA-Personal", TRUE) )
        {
            TmpMode  = COSA_DML_WIFI_SECURITY_WPA_Personal;
	    //wifi_setApSecurityModeEnabled(pWifiAp->AP.Cfg.InstanceNumber,pString);//RDKB-EMU
	    wifi_setApSecurityModeEnabled(wlanIndex,pString);//RDKB-EMU
        }
        else if ( AnscEqualString(pString, "WPA2-Personal", TRUE) )
        {
            TmpMode  = COSA_DML_WIFI_SECURITY_WPA2_Personal;
	    //wifi_setApSecurityModeEnabled(pWifiAp->AP.Cfg.InstanceNumber,pString);//RDKB-EMU
	    wifi_setApSecurityModeEnabled(wlanIndex,pString);//RDKB-EMU
        }
        else if ( AnscEqualString(pString, "WPA-WPA2-Personal", TRUE) )
        {
            TmpMode  = COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal;
	    //wifi_setApSecurityModeEnabled(pWifiAp->AP.Cfg.InstanceNumber,pString);//RDKB-EMU
	    wifi_setApSecurityModeEnabled(wlanIndex,pString);//RDKB-EMU
        }
        else if ( AnscEqualString(pString, "WPA-Enterprise", TRUE) )
        {
            TmpMode  = COSA_DML_WIFI_SECURITY_WPA_Enterprise;
        }
        else if ( AnscEqualString(pString, "WPA2-Enterprise", TRUE) )
        {
            TmpMode  = COSA_DML_WIFI_SECURITY_WPA2_Enterprise;
        }
        else if ( AnscEqualString(pString, "WPA-WPA2-Enterprise", TRUE) )
        {
            TmpMode  = COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise;
        }
        else
        {
            return FALSE;
        }
        
        if ( TmpMode == pWifiApSec->Cfg.ModeEnabled )
        {
            return  TRUE;
        }
        else
        {
            pWifiApSec->Cfg.ModeEnabled = TmpMode;
            pWifiAp->bSecChanged        = TRUE;
        }
                
        return TRUE;
    }

    if( AnscEqualString(ParamName, "WEPKey", TRUE)
        || AnscEqualString(ParamName, "X_CISCO_COM_WEPKey", TRUE) )
    {
        pWifiAp->bSecChanged = TRUE;
        
        /* save update to backup */
        if ( pString && AnscSizeOfString(pString) == 10)
        {
            _ansc_sscanf
            (
                pString,
                "%02X%02X%02X%02X%02X",
                &tmpWEPKey[0],
                &tmpWEPKey[1],
                &tmpWEPKey[2],
                &tmpWEPKey[3],
                &tmpWEPKey[4]
            );

            if ( _ansc_memcmp(pWifiApSec->Cfg.WEPKeyp, tmpWEPKey, 5) != 0 )
            {
                pWifiApSec->Cfg.WEPKeyp[0] = tmpWEPKey[0];
                pWifiApSec->Cfg.WEPKeyp[1] = tmpWEPKey[1];
                pWifiApSec->Cfg.WEPKeyp[2] = tmpWEPKey[2];
                pWifiApSec->Cfg.WEPKeyp[3] = tmpWEPKey[3];
                pWifiApSec->Cfg.WEPKeyp[4] = tmpWEPKey[4];

                pWifiAp->bSecChanged = TRUE;
            }
        }
        else if ( pString && AnscSizeOfString(pString) == 26 )
        {
            /* Need to do sscanf to a temp value here as it puts a NULL at the end */

            _ansc_sscanf
            (
                pString,
                "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
                &tmpWEPKey[0],
                &tmpWEPKey[1],
                &tmpWEPKey[2],
                &tmpWEPKey[3],
                &tmpWEPKey[4],
                &tmpWEPKey[5],
                &tmpWEPKey[6],
                &tmpWEPKey[7],
                &tmpWEPKey[8],
                &tmpWEPKey[9],
                &tmpWEPKey[10],
                &tmpWEPKey[11],
                &tmpWEPKey[12]
             );

            if ( _ansc_memcmp(pWifiApSec->Cfg.WEPKeyp, tmpWEPKey, 13) != 0 )
            {
                 pWifiApSec->Cfg.WEPKeyp[0] = tmpWEPKey[0];
                 pWifiApSec->Cfg.WEPKeyp[1] = tmpWEPKey[1];
                 pWifiApSec->Cfg.WEPKeyp[2] = tmpWEPKey[2];
                 pWifiApSec->Cfg.WEPKeyp[3] = tmpWEPKey[3];
                 pWifiApSec->Cfg.WEPKeyp[4] = tmpWEPKey[4];
                 pWifiApSec->Cfg.WEPKeyp[5] = tmpWEPKey[5];
                 pWifiApSec->Cfg.WEPKeyp[6] = tmpWEPKey[6];
                 pWifiApSec->Cfg.WEPKeyp[7] = tmpWEPKey[7];
                 pWifiApSec->Cfg.WEPKeyp[8] = tmpWEPKey[8];
                 pWifiApSec->Cfg.WEPKeyp[9] = tmpWEPKey[9];
                 pWifiApSec->Cfg.WEPKeyp[10] = tmpWEPKey[10];
                 pWifiApSec->Cfg.WEPKeyp[11] = tmpWEPKey[11];
                 pWifiApSec->Cfg.WEPKeyp[12] = tmpWEPKey[12];
                 
                pWifiAp->bSecChanged = TRUE;
            }
        }
        else
        {
            if((pWifiApSec->Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_64) || 
               (pWifiApSec->Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_128))
                return FALSE; /* Return an error only if the security mode enabled is WEP - For UI */
        }
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "PreSharedKey", TRUE))
    {
        if ( AnscEqualString(pString, pWifiApSec->Cfg.PreSharedKey, TRUE) )
        {
		return TRUE;
	}
            /* save update to backup */
        AnscCopyString(pWifiApSec->Cfg.PreSharedKey, pString );
	wifi_setApSecurityPreSharedKey(wlanIndex,&pWifiApSec->Cfg.PreSharedKey);
        pWifiAp->bSecChanged = TRUE;
        return TRUE;
    }
#if 0//LNT_EMU
    if( AnscEqualString(ParamName, "KeyPassphrase", TRUE)
        || AnscEqualString(ParamName, "X_CISCO_COM_KeyPassphrase", TRUE) )
    {
        if ( AnscEqualString(pString, pWifiApSec->Cfg.KeyPassphrase, TRUE) )
        {
            /* save update to backup */
            AnscCopyString(pWifiApSec->Cfg.KeyPassphrase, pString );
            pWifiAp->bSecChanged = TRUE;
        }
        return TRUE;
    }
#endif
#if 1 //LNT_EMU
        if( AnscEqualString(ParamName, "KeyPassphrase", TRUE) ||
        ( ( AnscEqualString(ParamName, "X_CISCO_COM_KeyPassphrase", TRUE)) &&
          ( AnscSizeOfString(pString) != 64) ) ||  ( ( AnscEqualString(ParamName, "X_COMCAST-COM_KeyPassphrase", TRUE)) && ( AnscSizeOfString(pString) != 64) ) )

    {
        if ( AnscEqualString(pString, pWifiApSec->Cfg.KeyPassphrase, TRUE) )
        {
            return TRUE;
        }

//RDKB-20043 - We should not restrict here
#if 0
        if ( (pWifiAp->AP.Cfg.InstanceNumber == 1 ) || (pWifiAp->AP.Cfg.InstanceNumber == 2 ) )
        {

                if ( AnscEqualString(pString, pWifiApSec->Cfg.DefaultKeyPassphrase, TRUE) )
                {
                return FALSE;
                }

        }
#endif /* 0 */

        /* save update to backup */
        AnscCopyString(pWifiApSec->Cfg.KeyPassphrase, pString );
        //zqiu: reason for change: Change 2.4G wifi password not work for the first time
	
        AnscCopyString(pWifiApSec->Cfg.PreSharedKey, pWifiApSec->Cfg.KeyPassphrase );
	if( (pWifiAp->AP.Cfg.InstanceNumber == 1) || (pWifiAp->AP.Cfg.InstanceNumber == 2))
	{
        wifi_setApSecurityPreSharedKey(wlanIndex,&pWifiApSec->Cfg.PreSharedKey);
//PSM_ACCESS
	memset(recName, 0, sizeof(recName));
        sprintf(recName, Passphrase, pWifiAp->AP.Cfg.InstanceNumber);
        AnscCopyString(str,pWifiApSec->Cfg.PreSharedKey);
        PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, str);
	}
        pWifiAp->bSecChanged = TRUE;
        return TRUE;
    }
#endif

    if( AnscEqualString(ParamName, "RadiusSecret", TRUE))
    {
        if ( AnscEqualString(pString, pWifiApSec->Cfg.RadiusSecret, TRUE) )
        {
            /* save update to backup */
            AnscCopyString( pWifiApSec->Cfg.RadiusSecret, pString );
            pWifiAp->bSecChanged = TRUE;
        }
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Security_Validate
            (
                ANSC_HANDLE                 hInsContext,
                char*                       pReturnParamName,
                ULONG*                      puLength
            );

    description:

        This function is called to finally commit all the update.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       pReturnParamName,
                The buffer (128 bytes) of parameter name if there's a validation. 

                ULONG*                      puLength
                The output length of the param name. 

    return:     TRUE if there's no validation.

**********************************************************************/
BOOL
Security_Validate
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pReturnParamName,
        ULONG*                      puLength
    )
{
    return TRUE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        Security_Commit
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to finally commit all the update.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
Security_Commit
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj      = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp       = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_APSEC_FULL       pWifiApSec    = (PCOSA_DML_WIFI_APSEC_FULL)&pWifiAp->SEC;
    PCOSA_DML_WIFI_APSEC_CFG        pWifiApSecCfg = (PCOSA_DML_WIFI_APSEC_CFG )&pWifiApSec->Cfg;
    PSINGLE_LINK_ENTRY              pSLinkEntry   = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pSSIDLinkObj  = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid     = (PCOSA_DML_WIFI_SSID      )NULL;
    UCHAR                           PathName[64]  = {0};

    if ( !pWifiAp->bSecChanged )
    {
        return  ANSC_STATUS_SUCCESS;
    }
    else
    {
        pWifiAp->bSecChanged = FALSE;
        CcspTraceInfo(("WiFi AP Security commit -- apply the changes...\n"));
    }
    
    pSLinkEntry = AnscQueueGetFirstEntry(&pMyObject->SsidQueue);
    
    while ( pSLinkEntry )
    {
        pSSIDLinkObj = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);
        
        sprintf(PathName, "Device.WiFi.SSID.%d.", pSSIDLinkObj->InstanceNumber);
        
        if ( AnscEqualString(pWifiAp->AP.Cfg.SSID, PathName, TRUE) )
        {
            break;
        }
        
        pSLinkEntry             = AnscQueueGetNextEntry(pSLinkEntry);
    }
    
    if ( pSLinkEntry )
    {
        pWifiSsid = pSSIDLinkObj->hContext;
        
        //return CosaDmlWiFiApSecSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, pWifiApSecCfg);
        return CosaDmlWiFiApSecSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.Alias, pWifiApSecCfg);
    }
    
    return ANSC_STATUS_FAILURE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        Security_Rollback
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to roll back the update whenever there's a 
        validation found.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
Security_Rollback
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject       = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObjAp      = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp         = (PCOSA_DML_WIFI_AP        )pLinkObjAp->hContext;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObjSsid    = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntrySsid = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid       = (PCOSA_DML_WIFI_SSID      )NULL;
    UCHAR                           PathName[64]    = {0};

    /*Get the corresponding SSID entry*/
    pSLinkEntrySsid = AnscQueueGetFirstEntry(&pMyObject->SsidQueue);
    while ( pSLinkEntrySsid )
    {
        pLinkObjSsid = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntrySsid);
        
        sprintf(PathName, "Device.WiFi.SSID.%d.", pLinkObjSsid->InstanceNumber);
        
        if ( AnscEqualString(pWifiAp->AP.Cfg.SSID, PathName, TRUE) )
        {
            break;
        }
                
        pSLinkEntrySsid             = AnscQueueGetNextEntry(pSLinkEntrySsid);
    }
    
    if ( pSLinkEntrySsid )
    {
        pWifiSsid = pLinkObjSsid->hContext;
        
        CosaDmlWiFiApSecGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->SEC);
    }
    
    return ANSC_STATUS_SUCCESS;
}

/***********************************************************************

 APIs for Object:

    WiFi.AccessPoint.{i}.WPS.

    *  WPS_GetParamBoolValue
    *  WPS_GetParamIntValue
    *  WPS_GetParamUlongValue
    *  WPS_GetParamStringValue
    *  WPS_SetParamBoolValue
    *  WPS_SetParamIntValue
    *  WPS_SetParamUlongValue
    *  WPS_SetParamStringValue
    *  WPS_Validate
    *  WPS_Commit
    *  WPS_Rollback

***********************************************************************/
/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        WPS_GetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL*                       pBool
            );

    description:

        This function is called to retrieve Boolean parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL*                       pBool
                The buffer of returned boolean value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
WPS_GetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL*                       pBool
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_APWPS_FULL       pWifiApWps   = (PCOSA_DML_WIFI_APWPS_FULL)&pWifiAp->WPS;
    
	int wlanIndex = pWifiAp->AP.Cfg.InstanceNumber - 1;
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "Enable", TRUE))
    {
        /* collect value */
	wifi_getApWpsEnable(wlanIndex, &pWifiApWps->Cfg.bEnabled);
        *pBool = pWifiApWps->Cfg.bEnabled;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_ActivatePushButton", TRUE))
    {
        /* collect value */
        *pBool = pWifiApWps->Cfg.X_CISCO_COM_ActivatePushButton;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_Comcast_com_Configured", TRUE))
    {
	    /* collect value */
	    char  configState[32] = {0};
	    wifi_getApWpsConfigurationState(wlanIndex, configState);
	    if (strstr(configState,"Not configured") != NULL) {
		    pWifiApWps->Info.X_Comcast_com_Configured = FALSE;
	    } else {
		    pWifiApWps->Info.X_Comcast_com_Configured = TRUE;
	    }

	    *pBool = pWifiApWps->Info.X_Comcast_com_Configured;
	    return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_CancelSession", TRUE))
    {
        /* collect value */
        *pBool = pWifiApWps->Cfg.X_CISCO_COM_CancelSession;
        return TRUE;
    }


    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        WPS_GetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                int*                        pInt
            );

    description:

        This function is called to retrieve integer parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                int*                        pInt
                The buffer of returned integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
WPS_GetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int*                        pInt
    )
{
    /* check the parameter name and return the corresponding value */
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_APWPS_FULL       pWifiApWps   = (PCOSA_DML_WIFI_APWPS_FULL)&pWifiAp->WPS;
    

    if (AnscEqualString(ParamName, "X_CISCO_COM_WpsPushButton", TRUE))
    {
        *pInt = pWifiApWps->Cfg.WpsPushButton;
        return TRUE;
    }
 
    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        WPS_GetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG*                      puLong
            );

    description:

        This function is called to retrieve ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG*                      puLong
                The buffer of returned ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
WPS_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG*                      puLong
    )
{
    /* check the parameter name and return the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        WPS_GetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pValue,
                ULONG*                      pUlSize
            );

    description:

        This function is called to retrieve string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pValue,
                The string value buffer;

                ULONG*                      pUlSize
                The buffer of length of string value;
                Usually size of 1023 will be used.
                If it's not big enough, put required size here and return 1;

    return:     0 if succeeded;
                1 if short of buffer size; (*pUlSize = required size)
                -1 if not supported.

**********************************************************************/
ULONG
WPS_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_APWPS_FULL       pWifiApWps   = (PCOSA_DML_WIFI_APWPS_FULL)&pWifiAp->WPS;
    int wlanIndex = pWifiAp->AP.Cfg.InstanceNumber - 1;
    char methodsEnabled[64] = {0};

    wifi_getApWpsConfigMethodsEnabled(wlanIndex,methodsEnabled);
    if (strstr(methodsEnabled,"PushButton") != NULL) {
	    pWifiApWps->Cfg.ConfigMethodsEnabled |= COSA_DML_WIFI_WPS_METHOD_PushButton;
    }
    if (strstr(methodsEnabled,"Keypad") != NULL) {
	    pWifiApWps->Cfg.ConfigMethodsEnabled |= COSA_DML_WIFI_WPS_METHOD_Pin;
    }

    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "ConfigMethodsEnabled", TRUE))
    {
        /* collect value */
        char buf[512] = {0};
        if (pWifiApWps->Cfg.ConfigMethodsEnabled & COSA_DML_WIFI_WPS_METHOD_UsbFlashDrive )
        {   
            strcat(buf, "USBFlashDrive");
        }
        if (pWifiApWps->Cfg.ConfigMethodsEnabled & COSA_DML_WIFI_WPS_METHOD_Ethernet )
        {   
            if (AnscSizeOfString(buf) != 0)
            {
               strcat(buf, ",Ethernet");
            }
            else
            {
               strcat(buf, "Ethernet");
            }

        }
        if (pWifiApWps->Cfg.ConfigMethodsEnabled & COSA_DML_WIFI_WPS_METHOD_ExternalNFCToken )
        {
            if (AnscSizeOfString(buf) != 0)
            {
               strcat(buf, ",ExternalNFCToken");
            }
            else
            {
               strcat(buf, "ExternalNFCToken");
            }
        }
        if (pWifiApWps->Cfg.ConfigMethodsEnabled & COSA_DML_WIFI_WPS_METHOD_IntgratedNFCToken )
        {
            if (AnscSizeOfString(buf) != 0)
            {
               strcat(buf, ",IntegratedNFCToken");
            }
            else
            {
               strcat(buf, "IntegratedNFCToken");
            }
        }
        if (pWifiApWps->Cfg.ConfigMethodsEnabled & COSA_DML_WIFI_WPS_METHOD_NFCInterface )
        {
            if (AnscSizeOfString(buf) != 0)
            {
               strcat(buf, ",NFCInterface");
            }
            else
            {
               strcat(buf, "NFCInterface");
            }
        }
        if (pWifiApWps->Cfg.ConfigMethodsEnabled & COSA_DML_WIFI_WPS_METHOD_PushButton )
        {
            if (AnscSizeOfString(buf) != 0)
            {
               strcat(buf, ",PushButton");
            }
            else
            {
               strcat(buf, "PushButton");
            }

         }
         if (pWifiApWps->Cfg.ConfigMethodsEnabled & COSA_DML_WIFI_WPS_METHOD_Pin )
         {
         
            if (AnscSizeOfString(buf) != 0)
            {
               strcat(buf, ",PIN");
            }
            else
            {
               strcat(buf, "PIN");
            }
        }
        if ( AnscSizeOfString(buf) < *pUlSize)
        {
            AnscCopyString(pValue, buf);
            return 0;
        }
        else
        {
           *pUlSize = AnscSizeOfString(buf)+1;
           return 1;
        }
    }
    if( AnscEqualString(ParamName, "ConfigMethodsSupported", TRUE))
    {
        /* collect value */
        char buf[512] = {0};
        if (pWifiApWps->Info.ConfigMethodsSupported & COSA_DML_WIFI_WPS_METHOD_UsbFlashDrive )
        {   
            strcat(buf, "USBFlashDrive");
        }
        if (pWifiApWps->Info.ConfigMethodsSupported & COSA_DML_WIFI_WPS_METHOD_Ethernet )
        {   
            if (AnscSizeOfString(buf) != 0)
            {
               strcat(buf, ",Ethernet");
            }
            else
            {
               strcat(buf, "Ethernet");
            }

        }
        if (pWifiApWps->Info.ConfigMethodsSupported & COSA_DML_WIFI_WPS_METHOD_ExternalNFCToken )
        {
            if (AnscSizeOfString(buf) != 0)
            {
               strcat(buf, ",ExternalNFCToken");
            }
            else
            {
               strcat(buf, "ExternalNFCToken");
            }
        }
        if (pWifiApWps->Info.ConfigMethodsSupported & COSA_DML_WIFI_WPS_METHOD_IntgratedNFCToken )
        {
            if (AnscSizeOfString(buf) != 0)
            {
               strcat(buf, ",IntegratedNFCToken");
            }
            else
            {
               strcat(buf, "IntegratedNFCToken");
            }
        }
        if (pWifiApWps->Info.ConfigMethodsSupported & COSA_DML_WIFI_WPS_METHOD_NFCInterface )
        {
            if (AnscSizeOfString(buf) != 0)
            {
               strcat(buf, ",NFCInterface");
            }
            else
            {
               strcat(buf, "NFCInterface");
            }
        }
        if (pWifiApWps->Info.ConfigMethodsSupported & COSA_DML_WIFI_WPS_METHOD_PushButton )
        {
            if (AnscSizeOfString(buf) != 0)
            {
               strcat(buf, ",PushButton");
            }
            else
            {
               strcat(buf, "PushButton");
            }

         }
         if (pWifiApWps->Info.ConfigMethodsSupported & COSA_DML_WIFI_WPS_METHOD_Pin )
         {
            if (AnscSizeOfString(buf) != 0)
            {
               strcat(buf, ",PIN");
            }
            else
            {
               strcat(buf, "PIN");
            }
        }
        if ( AnscSizeOfString(buf) < *pUlSize)
        {
            AnscCopyString(pValue, buf);
            return 0;
        }
        else
        {
           *pUlSize = AnscSizeOfString(buf)+1;
           return 1;
        }
    }

    if (AnscEqualString(ParamName, "X_CISCO_COM_Pin", TRUE))
    {
        if (*pUlSize <= AnscSizeOfString(pWifiApWps->Info.X_CISCO_COM_Pin))
        {
            *pUlSize = AnscSizeOfString(pWifiApWps->Info.X_CISCO_COM_Pin) + 1;
            return 1;
        }

        AnscCopyString(pValue, pWifiApWps->Info.X_CISCO_COM_Pin);
        return 0;
    }

    if (AnscEqualString(ParamName, "X_CISCO_COM_ClientPin", TRUE))
    {
        if (*pUlSize <= AnscSizeOfString(pWifiApWps->Cfg.X_CISCO_COM_ClientPin))
        {
            *pUlSize = AnscSizeOfString(pWifiApWps->Cfg.X_CISCO_COM_ClientPin) + 1;
            return 1;
        }

        AnscCopyString(pValue, pWifiApWps->Cfg.X_CISCO_COM_ClientPin);
        return 0;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return -1;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        WPS_SetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL                        bValue
            );

    description:

        This function is called to set BOOL parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL                        bValue
                The updated BOOL value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
WPS_SetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL                        bValue
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_APWPS_FULL       pWifiApWps   = (PCOSA_DML_WIFI_APWPS_FULL)&pWifiAp->WPS;
    int wlanIndex = pWifiAp->AP.Cfg.InstanceNumber - 1;
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "Enable", TRUE))
    {
        /* save update to backup */
        pWifiApWps->Cfg.bEnabled = bValue;
	wifi_setApWpsEnable(wlanIndex, pWifiApWps->Cfg.bEnabled);
        return TRUE;
    }
    if( AnscEqualString(ParamName, "X_CISCO_COM_ActivatePushButton", TRUE))
    {
        /* save update to backup */
        pWifiApWps->Cfg.X_CISCO_COM_ActivatePushButton = bValue;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "X_CISCO_COM_CancelSession", TRUE))
    {
        /* save update to backup */
        pWifiApWps->Cfg.X_CISCO_COM_CancelSession = bValue;
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        WPS_SetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                int                         iValue
            );

    description:

        This function is called to set integer parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                int                         iValue
                The updated integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
WPS_SetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int                         iValue
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_APWPS_FULL       pWifiApWps   = (PCOSA_DML_WIFI_APWPS_FULL)&pWifiAp->WPS;
 
    /* check the parameter name and set the corresponding value */
    if (AnscEqualString(ParamName, "X_CISCO_COM_WpsPushButton", TRUE))
    {
        pWifiApWps->Cfg.WpsPushButton = iValue;
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        WPS_SetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG                       uValue
            );

    description:

        This function is called to set ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG                       uValue
                The updated ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
WPS_SetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG                       uValue
    )
{
    /* check the parameter name and set the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        WPS_SetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pString
            );

    description:

        This function is called to set string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pString
                The updated string value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
WPS_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pString
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_APWPS_FULL       pWifiApWps   = (PCOSA_DML_WIFI_APWPS_FULL)&pWifiAp->WPS;
    int wlanIndex = pWifiAp->AP.Cfg.InstanceNumber-1;
    
    /* check the parameter name and set the corresponding value */

    if( AnscEqualString(ParamName, "ConfigMethodsEnabled", TRUE))
    {
        pWifiApWps->Cfg.ConfigMethodsEnabled = 0;
        /* save update to backup */
        if (_ansc_strstr(pString, "USBFlashDrive"))
        {
            pWifiApWps->Cfg.ConfigMethodsEnabled = (pWifiApWps->Cfg.ConfigMethodsEnabled | COSA_DML_WIFI_WPS_METHOD_UsbFlashDrive);
        }
        if (_ansc_strstr(pString, "Ethernet"))
        {
            pWifiApWps->Cfg.ConfigMethodsEnabled = (pWifiApWps->Cfg.ConfigMethodsEnabled | COSA_DML_WIFI_WPS_METHOD_Ethernet);
        }
        if (_ansc_strstr(pString, "ExternalNFCToken"))
        {
            pWifiApWps->Cfg.ConfigMethodsEnabled = (pWifiApWps->Cfg.ConfigMethodsEnabled | COSA_DML_WIFI_WPS_METHOD_ExternalNFCToken);
        }
        if (_ansc_strstr(pString, "IntegratedNFCToken"))
        {
            pWifiApWps->Cfg.ConfigMethodsEnabled = (pWifiApWps->Cfg.ConfigMethodsEnabled | COSA_DML_WIFI_WPS_METHOD_IntgratedNFCToken);
        }
        if (_ansc_strstr(pString, "NFCInterface"))
        {
            pWifiApWps->Cfg.ConfigMethodsEnabled = (pWifiApWps->Cfg.ConfigMethodsEnabled | COSA_DML_WIFI_WPS_METHOD_NFCInterface);
        }
        if (_ansc_strstr(pString, "PushButton"))
        {
            pWifiApWps->Cfg.ConfigMethodsEnabled = (pWifiApWps->Cfg.ConfigMethodsEnabled | COSA_DML_WIFI_WPS_METHOD_PushButton);
	     wifi_setApWpsConfigMethodsEnabled(wlanIndex,"PushButton");
        }
        if (_ansc_strstr(pString, "PIN"))
        {
            pWifiApWps->Cfg.ConfigMethodsEnabled = (pWifiApWps->Cfg.ConfigMethodsEnabled | COSA_DML_WIFI_WPS_METHOD_Pin);
	    wifi_setApWpsConfigMethodsEnabled(wlanIndex,"Keypad,Label,Display");
        }
        if (_ansc_strstr(pString, "PushButton,PIN"))
        {
            pWifiApWps->Cfg.ConfigMethodsEnabled == (COSA_DML_WIFI_WPS_METHOD_PushButton|COSA_DML_WIFI_WPS_METHOD_Pin);
	    wifi_setApWpsConfigMethodsEnabled(wlanIndex,"PushButton,Keypad,Label,Display");
        }
        return TRUE;
    }

    if (AnscEqualString(ParamName, "X_CISCO_COM_ClientPin", TRUE))
    {
        if (AnscSizeOfString(pString) > sizeof(pWifiApWps->Cfg.X_CISCO_COM_ClientPin) - 1)
            return FALSE;

        AnscCopyString(pWifiApWps->Cfg.X_CISCO_COM_ClientPin, pString);
        return TRUE;
    }


    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        WPS_Validate
            (
                ANSC_HANDLE                 hInsContext,
                char*                       pReturnParamName,
                ULONG*                      puLength
            );

    description:

        This function is called to finally commit all the update.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       pReturnParamName,
                The buffer (128 bytes) of parameter name if there's a validation. 

                ULONG*                      puLength
                The output length of the param name. 

    return:     TRUE if there's no validation.

**********************************************************************/
BOOL
WPS_Validate
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pReturnParamName,
        ULONG*                      puLength
    )
{
    return TRUE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        WPS_Commit
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to finally commit all the update.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
WPS_Commit
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj      = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp       = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PSINGLE_LINK_ENTRY              pSLinkEntry   = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pSSIDLinkObj  = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid     = (PCOSA_DML_WIFI_SSID      )NULL;
    UCHAR                           PathName[64]  = {0};
   
    pSLinkEntry = AnscQueueGetFirstEntry(&pMyObject->SsidQueue);
    
    while ( pSLinkEntry )
    {
        pSSIDLinkObj = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);
        
        sprintf(PathName, "Device.WiFi.SSID.%d.", pSSIDLinkObj->InstanceNumber);
        
        if ( AnscEqualString(pWifiAp->AP.Cfg.SSID, PathName, TRUE) )
        {
            break;
        }
                
        pSLinkEntry             = AnscQueueGetNextEntry(pSLinkEntry);
    }
    
    if ( pSLinkEntry )
    {
        pWifiSsid = pSSIDLinkObj->hContext;
         //return CosaDmlWiFiApWpsSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->WPS.Cfg);
        return CosaDmlWiFiApWpsSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.Alias, &pWifiAp->WPS.Cfg);
    }
    
    return ANSC_STATUS_FAILURE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        WPS_Rollback
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to roll back the update whenever there's a 
        validation found.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
WPS_Rollback
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject       = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObjAp      = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp         = (PCOSA_DML_WIFI_AP        )pLinkObjAp->hContext;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObjSsid    = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntrySsid = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid       = (PCOSA_DML_WIFI_SSID      )NULL;
    UCHAR                           PathName[64]    = {0};

    /*Get the corresponding SSID entry*/
    pSLinkEntrySsid = AnscQueueGetFirstEntry(&pMyObject->SsidQueue);
    while ( pSLinkEntrySsid )
    {
        pLinkObjSsid = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntrySsid);
        
        sprintf(PathName, "Device.WiFi.SSID.%d.", pLinkObjSsid->InstanceNumber);
        
        if ( AnscEqualString(pWifiAp->AP.Cfg.SSID, PathName, TRUE) )
        {
            break;
        }
                
        pSLinkEntrySsid             = AnscQueueGetNextEntry(pSLinkEntrySsid);
    }
    
    if ( pSLinkEntrySsid )
    {
        pWifiSsid = pLinkObjSsid->hContext;
        CosaDmlWiFiApWpsGetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->WPS.Cfg);
    }
    
    return ANSC_STATUS_SUCCESS;
}


/***********************************************************************

 APIs for Object:

    WiFi.AccessPoint.{i}.X_CISCO_COM_MACFilter.

    *  MacFilter_GetParamBoolValue
    *  MacFilter_GetParamIntValue
    *  MacFilter_GetParamUlongValue
    *  MacFilter_GetParamStringValue
    *  MacFilter_SetParamBoolValue
    *  Macfilter_SetParamIntValue
    *  MacFilter_SetParamUlongValue
    *  MacFilter_SetParamStringValue
    *  MacFilter_Validate
    *  MacFilter_Commit
    *  MacFilter_Rollback

***********************************************************************/
/**********************************************************************  

    caller:     owner of this object

    prototype:

        BOOL
        MacFilter_GetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL*                       pBool
            );

    description:

        This function is called to retrieve Boolean parameter value;

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL*                       pBool
                The buffer of returned boolean value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
MacFilter_GetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL*                       pBool
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject    = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_AP_MF_CFG        pWifiApMf    = (PCOSA_DML_WIFI_APWPS_FULL)&pWifiAp->MF;
    PCOSA_DML_WIFI_SSID             pWifiSsid    = (PCOSA_DML_WIFI_SSID      )pLinkObj->hContext;

    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "Enable", TRUE))
    {
        /* collect value */
        *pBool = pWifiApMf->bEnabled;
        return TRUE;
    }
    
    if( AnscEqualString(ParamName, "FilterAsBlackList", TRUE))
    {
        /* collect value */
        *pBool = pWifiApMf->FilterAsBlackList;
        return TRUE;
    }


    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}


/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Macfilter_GetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                int*                        pInt
            );

    description:

        This function is called to retrieve integer parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                int*                        pInt
                The buffer of returned integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
MacFilter_GetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int*                        pInt
    )
{
    /* check the parameter name and return the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        MacFilter_GetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG*                      puLong
            );

    description:

        This function is called to retrieve ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG*                      puLong
                The buffer of returned ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
MacFilter_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG*                      puLong
    )
{
    /* check the parameter name and return the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        MacFilter_GetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pValue,
                ULONG*                      pUlSize
            );

    description:

        This function is called to retrieve string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pValue,
                The string value buffer;

                ULONG*                      pUlSize
                The buffer of length of string value;
                Usually size of 1023 will be used.
                If it's not big enough, put required size here and return 1;

    return:     0 if succeeded;
                1 if short of buffer size; (*pUlSize = required size)
                -1 if not supported.

**********************************************************************/
ULONG
MacFilter_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject    = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_AP_MF_CFG        pWifiApMf    = (PCOSA_DML_WIFI_APWPS_FULL)&pWifiAp->MF;
    PCOSA_DML_WIFI_SSID             pWifiSsid    = (PCOSA_DML_WIFI_SSID      )pLinkObj->hContext;

    
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "MACAddress", TRUE))
    {
        /* collect value */
        char maclist[1024] = {'\0'};
        CosaDmlWiFiApMfGetMacList(pWifiApMf->MacAddrList, maclist, pWifiApMf->NumberMacAddrList);
        AnscCopyString(pValue, maclist);
        *pUlSize = AnscSizeOfString(pValue); 

        return 0;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return -1;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        MacFilter_SetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL                        bValue
            );

    description:

        This function is called to set BOOL parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL                        bValue
                The updated BOOL value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
MacFilter_SetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL                        bValue
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_AP_MF_CFG        pWifiApMf    = (PCOSA_DML_WIFI_APWPS_FULL)&pWifiAp->MF;
    
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "Enable", TRUE))
    {
       /* save update to backup */
       pWifiApMf->bEnabled = bValue;
        if(!pWifiApMf->bEnabled)
        {
                snprintf(Operation,sizeof(Operation),"%s","ALLOW-ALL");
		isAllow=0;
        }
	else
		isAllow = 1;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "FilterAsBlackList", TRUE))
    {
        /* save update to backup */
        if(pWifiApMf->bEnabled == FALSE)
        {
            return FALSE;
        }
        else
        {
            pWifiApMf->FilterAsBlackList = bValue;
            if(pWifiApMf->FilterAsBlackList)
                   snprintf(Operation,sizeof(Operation),"%s","DROP");
            else
                   snprintf(Operation,sizeof(Operation),"%s","ACCEPT");
            return TRUE;
        }
    }


    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        MacFilter_SetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                int                         iValue
            );

    description:

        This function is called to set integer parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                int                         iValue
                The updated integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
MacFilter_SetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int                         iValue
    )
{
    /* check the parameter name and set the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        MacFilter_SetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG                       uValue
            );

    description:

        This function is called to set ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG                       uValue
                The updated ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
MacFilter_SetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG                       uValue
    )
{
    /* check the parameter name and set the corresponding value */

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        MacFilter_SetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pString
            );

    description:

        This function is called to set string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pString
                The updated string value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
MacFilter_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pString
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_AP_MF_CFG        pWifiApMf    = (PCOSA_DML_WIFI_APWPS_FULL)&pWifiAp->MF;
    
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "MACAddress", TRUE))
    {
        if(pWifiApMf->bEnabled == TRUE)
	{
            CosaDmlWiFiApMfSetMacList(pString, pWifiApMf->MacAddrList, &pWifiApMf->NumberMacAddrList);
            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        MacFilter_Validate
            (
                ANSC_HANDLE                 hInsContext,
                char*                       pReturnParamName,
                ULONG*                      puLength
            );

    description:

        This function is called to finally commit all the update.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       pReturnParamName,
                The buffer (128 bytes) of parameter name if there's a validation. 

                ULONG*                      puLength
                The output length of the param name. 

    return:     TRUE if there's no validation.

**********************************************************************/
BOOL
MacFilter_Validate
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pReturnParamName,
        ULONG*                      puLength
    )
{
    return TRUE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        MacFilter_Commit
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to finally commit all the update.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
MacFilter_Commit
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj      = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp       = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PSINGLE_LINK_ENTRY              pSLinkEntry   = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pSSIDLinkObj  = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid     = (PCOSA_DML_WIFI_SSID      )NULL;
    UCHAR                           PathName[64]  = {0};
    int ret;   
    pSLinkEntry = AnscQueueGetFirstEntry(&pMyObject->SsidQueue);
    
    while ( pSLinkEntry )
    {
        pSSIDLinkObj = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);
        
        sprintf(PathName, "Device.WiFi.SSID.%d.", pSSIDLinkObj->InstanceNumber);
        
        if ( AnscEqualString(pWifiAp->AP.Cfg.SSID, PathName, TRUE) )
        {
            break;
        }
                
        pSLinkEntry             = AnscQueueGetNextEntry(pSLinkEntry);
    }
    
    if ( pSLinkEntry )
    {
        pWifiSsid = pSSIDLinkObj->hContext;
        ret = CosaDmlWiFiApMfSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->MF);
        updateMacFilter(Operation);
	return ANSC_STATUS_SUCCESS;
    }
    
    return ANSC_STATUS_FAILURE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        MacFilter_Rollback
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to roll back the update whenever there's a 
        validation found.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
MacFilter_Rollback
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject       = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObjAp      = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp         = (PCOSA_DML_WIFI_AP        )pLinkObjAp->hContext;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObjSsid    = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntrySsid = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid       = (PCOSA_DML_WIFI_SSID      )NULL;
    UCHAR                           PathName[64]    = {0};

    /*Get the corresponding SSID entry*/
    pSLinkEntrySsid = AnscQueueGetFirstEntry(&pMyObject->SsidQueue);
    while ( pSLinkEntrySsid )
    {
        pLinkObjSsid = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntrySsid);
        
        sprintf(PathName, "Device.WiFi.SSID.%d.", pLinkObjSsid->InstanceNumber);
        
        if ( AnscEqualString(pWifiAp->AP.Cfg.SSID, PathName, TRUE) )
        {
            break;
        }
                
        pSLinkEntrySsid             = AnscQueueGetNextEntry(pSLinkEntrySsid);
    }
    
    if ( pSLinkEntrySsid )
    {
        pWifiSsid = pLinkObjSsid->hContext;
        CosaDmlWiFiApMfGetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->MF);
    }
    
    return ANSC_STATUS_SUCCESS;
}



/***********************************************************************

 APIs for Object:

    WiFi.AccessPoint.{i}.Associated{i}.

    *  AssociatedDevice1_GetEntryCount
    *  AssociatedDevice1_GetEntry
    *  AssociatedDevice1_IsUpdated
    *  AssociatedDevice1_Synchronize
    *  AssociatedDevice1_GetParamBoolValue
    *  AssociatedDevice1_GetParamIntValue
    *  AssociatedDevice1_GetParamUlongValue
    *  AssociatedDevice1_GetParamStringValue

***********************************************************************/
/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        AssociatedDevice1_GetEntryCount
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to retrieve the count of the table.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The count of the table

**********************************************************************/
ULONG
AssociatedDevice1_GetEntryCount
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
   
    return pWifiAp->AssocDeviceCount;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ANSC_HANDLE
        AssociatedDevice1_GetEntry
            (
                ANSC_HANDLE                 hInsContext,
                ULONG                       nIndex,
                ULONG*                      pInsNumber
            );

    description:

        This function is called to retrieve the entry specified by the index.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                ULONG                       nIndex,
                The index of this entry;

                ULONG*                      pInsNumber
                The output instance number;

    return:     The handle to identify the entry

**********************************************************************/
ANSC_HANDLE
AssociatedDevice1_GetEntry
    (
        ANSC_HANDLE                 hInsContext,
        ULONG                       nIndex,
        ULONG*                      pInsNumber
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT     )hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP             )pLinkObj->hContext;
    PCOSA_DML_WIFI_AP_ASSOC_DEVICE  pWifiApDev   = (PCOSA_DML_WIFI_AP_ASSOC_DEVICE)pWifiAp->AssocDevices;
    
    if (nIndex > pWifiAp->AssocDeviceCount)
    {
        return NULL;
    }
    else
    {
        *pInsNumber  = nIndex + 1; 
        return pWifiApDev+nIndex;
    }
    return NULL; /* return the handle */
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        AssociatedDevice1_IsUpdated
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is checking whether the table is updated or not.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     TRUE or FALSE.

**********************************************************************/
static ULONG AssociatedDevice1PreviousVisitTime;

#define WIFI_AssociatedDevice_TIMEOUT   120 /*unit is second*/

#if 0
BOOL
AssociatedDevice1_IsUpdated
    (
        ANSC_HANDLE                 hInsContext
    )
{
	printf( " ============================= %s =============== \n",__FUNCTION__);
    if ( ( AnscGetTickInSeconds() - AssociatedDevice1PreviousVisitTime ) < WIFI_AssociatedDevice_TIMEOUT )
        return FALSE;
    else
    {
	printf( " In Updated function ................ \n");
        AssociatedDevice1PreviousVisitTime =  AnscGetTickInSeconds();
        return TRUE;
    }
}
#endif
//RDKB-EMU
BOOL
AssociatedDevice1_IsUpdated
    (
        ANSC_HANDLE                 hInsContext
    )
{
    // This function is called once per SSID.  The old implementation always reported the second call as 
    // false and hence the second SSID would not appear to need updating.  This table is very dynamic and
    // clients come and go, so always update it.
    //return TRUE;

        //zqiu: remember AssociatedDevice1PreviousVisitTime for each AP.
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT     )hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP             )pLinkObj->hContext;
    ULONG ticket;
//>>zqiu
// remove index restriction
//    if(pWifiAp->AP.Cfg.InstanceNumber>12) //skip unused ssid 13-16
//      return FALSE;
//<<
    ticket=AnscGetTickInSeconds();
//>>zqiu
//remove WIFI_AssociatedDevice_TIMEOUT restriction
//    if ( ticket < (pWifiAp->AssociatedDevice1PreviousVisitTime + WIFI_AssociatedDevice_TIMEOUT ) )
//      return FALSE;
//    else {
        pWifiAp->AssociatedDevice1PreviousVisitTime =  ticket;
        return TRUE;
}
/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        AssociatedDevice1_Synchronize
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to synchronize the table.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
AssociatedDevice1_Synchronize
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT     )hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP             )pLinkObj->hContext;
    PCOSA_DATAMODEL_WIFI            pMyObject    = (PCOSA_DATAMODEL_WIFI          )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pSSIDLinkObj = (PCOSA_CONTEXT_LINK_OBJECT     )NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid    = (PCOSA_DML_WIFI_SSID           )NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntry  = (PSINGLE_LINK_ENTRY            )NULL;
    UCHAR                           PathName[64] = {0};

    /*release data allocated previous time*/
    if (pWifiAp->AssocDeviceCount)
    {
        AnscFreeMemory(pWifiAp->AssocDevices);
        pWifiAp->AssocDevices = NULL;
        pWifiAp->AssocDeviceCount = 0;
    }
    
    pSLinkEntry = AnscQueueGetFirstEntry(&pMyObject->SsidQueue);
    
    while ( pSLinkEntry )
    {
        pSSIDLinkObj = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);

        
        sprintf(PathName, "Device.WiFi.SSID.%d.", pSSIDLinkObj->InstanceNumber);
        
        if ( AnscEqualString(pWifiAp->AP.Cfg.SSID, PathName, TRUE) )
        {
            break;
        }
        
        pSLinkEntry             = AnscQueueGetNextEntry(pSLinkEntry);
    }
    
     if ( pSLinkEntry )
       {
        pWifiSsid = pSSIDLinkObj->hContext;
       /* pWifiAp->AssocDevices = CosaDmlWiFiApGetAssocDevices
        (
            (ANSC_HANDLE)pMyObject->hPoamWiFiDm, 
            pWifiSsid->SSID.Cfg.SSID, 
            &pWifiAp->AssocDeviceCount
        );*/ //RDKB-EMU
        pWifiAp->AssocDevices = CosaDmlWiFiApGetAssocDevices
        (
            (ANSC_HANDLE)pMyObject->hPoamWiFiDm, 
            pWifiSsid->SSID.Cfg.Alias, 
            &pWifiAp->AssocDeviceCount
        );
        
        return ANSC_STATUS_SUCCESS;
    }
    
    return ANSC_STATUS_SUCCESS;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        AssociatedDevice1_GetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL*                       pBool
            );

    description:

        This function is called to retrieve Boolean parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL*                       pBool
                The buffer of returned boolean value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
AssociatedDevice1_GetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL*                       pBool
    )
{
    PCOSA_DML_WIFI_AP_ASSOC_DEVICE  pWifiApDev   = (PCOSA_DML_WIFI_AP_ASSOC_DEVICE)hInsContext;
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "AuthenticationState", TRUE))
    {
        /* collect value */
        *pBool = pWifiApDev->AuthenticationState;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "Active", TRUE))
    {
        /* collect value */
        *pBool = pWifiApDev->Active;
        return TRUE;
    }


    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        AssociatedDevice1_GetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                int*                        pInt
            );

    description:

        This function is called to retrieve integer parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                int*                        pInt
                The buffer of returned integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
AssociatedDevice1_GetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int*                        pInt
    )
{
    PCOSA_DML_WIFI_AP_ASSOC_DEVICE  pWifiApDev   = (PCOSA_DML_WIFI_AP_ASSOC_DEVICE)hInsContext;
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "SignalStrength", TRUE))
    {
        /* collect value */
        *pInt = pWifiApDev->SignalStrength;
        return TRUE;
    }
    
    if( AnscEqualString(ParamName, "X_COMCAST-COM_SNR", TRUE))
    {
        /* collect value */
        *pInt = pWifiApDev->SNR;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_COMCAST-COM_RSSI", TRUE))
    {
        /* collect value */
        *pInt = pWifiApDev->RSSI;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_COMCAST-COM_MinRSSI", TRUE))
    {
        /* collect value */
        *pInt = pWifiApDev->MinRSSI;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_COMCAST-COM_MaxRSSI", TRUE))
    {
        /* collect value */
        *pInt = pWifiApDev->MaxRSSI;
        return TRUE;
    }


    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        AssociatedDevice1_GetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG*                      puLong
            );

    description:

        This function is called to retrieve ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG*                      puLong
                The buffer of returned ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
AssociatedDevice1_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG*                      puLong
    )
{
    PCOSA_DML_WIFI_AP_ASSOC_DEVICE  pWifiApDev   = (PCOSA_DML_WIFI_AP_ASSOC_DEVICE)hInsContext;
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "LastDataDownlinkRate", TRUE))
    {
        /* collect value */
        *puLong = pWifiApDev->LastDataDownlinkRate;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "LastDataUplinkRate", TRUE))
    {
        /* collect value */
        *puLong = pWifiApDev->LastDataUplinkRate;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "Retransmissions", TRUE))
    {
        /* collect value */
        *puLong = pWifiApDev->Retransmissions;
        return TRUE;
    }
	
    if( AnscEqualString(ParamName, "X_COMCAST-COM_DataFramesSentAck", TRUE))
    {
        /* collect value */
        *puLong = pWifiApDev->DataFramesSentAck;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_COMCAST-COM_DataFramesSentNoAck", TRUE))
    {
        /* collect value */
        *puLong = pWifiApDev->DataFramesSentNoAck;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_COMCAST-COM_BytesSent", TRUE))
    {
        /* collect value */
        *puLong = pWifiApDev->BytesSent;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_COMCAST-COM_BytesReceived", TRUE))
    {
        /* collect value */
        *puLong = pWifiApDev->LastDataDownlinkRate;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_COMCAST-COM_Disassociations", TRUE))
    {
        /* collect value */
        *puLong = pWifiApDev->Disassociations;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_COMCAST-COM_AuthenticationFailures", TRUE))
    {
        /* collect value */
        *puLong = pWifiApDev->AuthenticationFailures;
        return TRUE;
    }


    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        AssociatedDevice1_GetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pValue,
                ULONG*                      pUlSize
            );

    description:

        This function is called to retrieve string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pValue,
                The string value buffer;

                ULONG*                      pUlSize
                The buffer of length of string value;
                Usually size of 1023 will be used.
                If it's not big enough, put required size here and return 1;

    return:     0 if succeeded;
                1 if short of buffer size; (*pUlSize = required size)
                -1 if not supported.

**********************************************************************/
ULONG
AssociatedDevice1_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
    PCOSA_DML_WIFI_AP_ASSOC_DEVICE  pWifiApDev   = (PCOSA_DML_WIFI_AP_ASSOC_DEVICE)hInsContext;
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "MACAddress", TRUE))
    {
        /* collect value */
        if ( AnscSizeOfString(pWifiApDev->MacAddress) < *pUlSize)
        {
             _ansc_sprintf
            (
                pValue,
                "%02X:%02X:%02X:%02X:%02X:%02X",
                pWifiApDev->MacAddress[0],
                pWifiApDev->MacAddress[1],
                pWifiApDev->MacAddress[2],
                pWifiApDev->MacAddress[3],
                pWifiApDev->MacAddress[4],
                pWifiApDev->MacAddress[5]
            );

            *pUlSize = AnscSizeOfString(pValue);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiApDev->MacAddress)+1;
            return 1;
        }
        return 0;
    }

    if( AnscEqualString(ParamName, "X_COMCAST-COM_OperatingStandard", TRUE))
    {
        /* collect value */
        AnscCopyString(pValue, pWifiApDev->OperatingStandard);
        return 0;
    }

    if( AnscEqualString(ParamName, "X_COMCAST-COM_OperatingChannelBandwidth", TRUE))
    {
        /* collect value */
        AnscCopyString(pValue, pWifiApDev->OperatingChannelBandwidth);
        return 0;
    }

    if( AnscEqualString(ParamName, "X_COMCAST-COM_InterferenceSources", TRUE))
    {
        /* collect value */
        AnscCopyString(pValue, pWifiApDev->InterferenceSources);
        return 0;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return -1;
}

ULONG
WEPKey64Bit_GetEntryCount
    (
        ANSC_HANDLE                 hInsContext
    )
{
    return COSA_DML_WEP_KEY_NUM;
}

ANSC_HANDLE
WEPKey64Bit_GetEntry
    (
        ANSC_HANDLE                 hInsContext,
        ULONG                       nIndex,
        ULONG*                      pInsNumber
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_APSEC_FULL       pWifiApSec   = (PCOSA_DML_WIFI_APSEC_FULL)&pWifiAp->SEC;

    if (nIndex < 0 || nIndex >= COSA_DML_WEP_KEY_NUM)
        return (ANSC_HANDLE)NULL;

    *pInsNumber = nIndex + 1;
    return (ANSC_HANDLE)&pWifiApSec->WEPKey64Bit[nIndex];
}

ULONG
WEPKey64Bit_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
    PCOSA_DML_WEPKEY_64BIT          pWEPKey64 = (PCOSA_DML_WEPKEY_64BIT)hInsContext;

    if (AnscEqualString(ParamName, "WEPKey", TRUE))
    {
        if (*pUlSize <= AnscSizeOfString(pWEPKey64->WEPKey))
        {
            *pUlSize = AnscSizeOfString(pWEPKey64->WEPKey) + 1;
            return 1;
        }

        AnscCopyString(pValue, pWEPKey64->WEPKey);
        return 0;
    }

    return -1;
}

BOOL
WEPKey64Bit_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pString
    )
{
    PCOSA_DML_WEPKEY_64BIT          pWEPKey64 = (PCOSA_DML_WEPKEY_64BIT)hInsContext;

    if (AnscEqualString(ParamName, "WEPKey", TRUE))
    {
        if (AnscSizeOfString(pString) > sizeof(pWEPKey64->WEPKey) - 1)
            return FALSE;

        AnscZeroMemory(pWEPKey64->WEPKey, sizeof(pWEPKey64->WEPKey));
        AnscCopyString(pWEPKey64->WEPKey, pString);
        return TRUE;
    }

    return FALSE;
}

BOOL
WEPKey64Bit_Validate
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pReturnParamName,
        ULONG*                      puLength
    )
{
    PCOSA_DML_WEPKEY_64BIT          pWEPKey64 = (PCOSA_DML_WEPKEY_64BIT)hInsContext;

    /* isxdigital and mod(2) is already checked by frame work */
    if (AnscSizeOfString(pWEPKey64->WEPKey) != 40 / 8 * 2)
    {
        AnscCopyString(pReturnParamName, "WEPKey");
        *puLength = AnscSizeOfString("WEPKey");
        return FALSE;
    }

    return TRUE;
}

ULONG
WEPKey64Bit_Commit
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DML_WEPKEY_64BIT          pWEPKey64 = (PCOSA_DML_WEPKEY_64BIT)hInsContext;
    ULONG                           apIns, wepKeyIdx;

    if (GetInsNumsByWEPKey64(pWEPKey64, &apIns, &wepKeyIdx) != ANSC_STATUS_SUCCESS)
        return -1;

    return CosaDmlWiFi_SetWEPKey64ByIndex(apIns, wepKeyIdx, pWEPKey64);
}

ULONG
WEPKey64Bit_Rollback
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DML_WEPKEY_64BIT          pWEPKey64 = (PCOSA_DML_WEPKEY_64BIT)hInsContext;
    ULONG                           apIns, wepKeyIdx;

    if (GetInsNumsByWEPKey64(pWEPKey64, &apIns, &wepKeyIdx) != ANSC_STATUS_SUCCESS)
        return -1;

    return CosaDmlWiFi_GetWEPKey64ByIndex(apIns, wepKeyIdx, pWEPKey64);
}

ULONG
WEPKey128Bit_GetEntryCount
    (
        ANSC_HANDLE                 hInsContext
    )
{
    return COSA_DML_WEP_KEY_NUM;
}

ANSC_HANDLE
WEPKey128Bit_GetEntry
    (
        ANSC_HANDLE                 hInsContext,
        ULONG                       nIndex,
        ULONG*                      pInsNumber
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_APSEC_FULL       pWifiApSec   = (PCOSA_DML_WIFI_APSEC_FULL)&pWifiAp->SEC;

    if (nIndex < 0 || nIndex >= COSA_DML_WEP_KEY_NUM)
        return (ANSC_HANDLE)NULL;

    *pInsNumber = nIndex + 1;
    return (ANSC_HANDLE)&pWifiApSec->WEPKey128Bit[nIndex];
}

ULONG
WEPKey128Bit_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
    PCOSA_DML_WEPKEY_128BIT          pWEPKey128 = (PCOSA_DML_WEPKEY_128BIT)hInsContext;

    if (AnscEqualString(ParamName, "WEPKey", TRUE))
    {
        if (*pUlSize <= AnscSizeOfString(pWEPKey128->WEPKey))
        {
            *pUlSize = AnscSizeOfString(pWEPKey128->WEPKey) + 1;
            return 1;
        }

        AnscCopyString(pValue, pWEPKey128->WEPKey);
        return 0;
    }

    return -1;
}

BOOL
WEPKey128Bit_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pString
    )
{
    PCOSA_DML_WEPKEY_128BIT          pWEPKey128 = (PCOSA_DML_WEPKEY_128BIT)hInsContext;

    if (AnscEqualString(ParamName, "WEPKey", TRUE))
    {
        if (AnscSizeOfString(pString) > sizeof(pWEPKey128->WEPKey) - 1)
            return FALSE;

        AnscZeroMemory(pWEPKey128->WEPKey, sizeof(pWEPKey128->WEPKey));
        AnscCopyString(pWEPKey128->WEPKey, pString);
        return TRUE;
    }

    return FALSE;
}

BOOL
WEPKey128Bit_Validate
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pReturnParamName,
        ULONG*                      puLength
    )
{
    PCOSA_DML_WEPKEY_128BIT          pWEPKey128 = (PCOSA_DML_WEPKEY_128BIT)hInsContext;

    /* isxdigital and mod(2) is already checked by frame work */
    if (AnscSizeOfString(pWEPKey128->WEPKey) != 104 / 8 * 2)
    {
        AnscCopyString(pReturnParamName, "WEPKey");
        *puLength = AnscSizeOfString("WEPKey");
        return FALSE;
    }

    return TRUE;
}

ULONG
WEPKey128Bit_Commit
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DML_WEPKEY_128BIT          pWEPKey128 = (PCOSA_DML_WEPKEY_128BIT)hInsContext;
    ULONG                           apIns, wepKeyIdx;

    if (GetInsNumsByWEPKey128(pWEPKey128, &apIns, &wepKeyIdx) != ANSC_STATUS_SUCCESS)
        return -1;

    return CosaDmlWiFi_SetWEPKey128ByIndex(apIns, wepKeyIdx, pWEPKey128);
}

ULONG
WEPKey128Bit_Rollback
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DML_WEPKEY_128BIT          pWEPKey128 = (PCOSA_DML_WEPKEY_128BIT)hInsContext;
    ULONG                           apIns, wepKeyIdx;

    if (GetInsNumsByWEPKey128(pWEPKey128, &apIns, &wepKeyIdx) != ANSC_STATUS_SUCCESS)
        return -1;

    return CosaDmlWiFi_GetWEPKey128ByIndex(apIns, wepKeyIdx, pWEPKey128);
}

ULONG
MacFiltTab_GetEntryCount
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pParentLinkObj  = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP_FULL          pWiFiAP         = (PCOSA_DML_WIFI_AP_FULL)pParentLinkObj->hContext;
    PSLIST_HEADER                   pListHead       = (PSLIST_HEADER)&pWiFiAP->MacFilterList;

    return AnscSListQueryDepth(pListHead);
}

ANSC_HANDLE
MacFiltTab_GetEntry
    (
        ANSC_HANDLE                 hInsContext,
        ULONG                       nIndex,
        ULONG*                      pInsNumber
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pCosaContext    = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP_FULL          pWiFiAP         = (PCOSA_DML_WIFI_AP_FULL)pCosaContext->hContext;

    PSINGLE_LINK_ENTRY              pLink           = (PSINGLE_LINK_ENTRY)NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pSubCosaContext = (PCOSA_CONTEXT_LINK_OBJECT)NULL;

    pLink = AnscSListGetEntryByIndex((PSLIST_HEADER)&pWiFiAP->MacFilterList, nIndex);

    if ( pLink )
    {
        pSubCosaContext = ACCESS_COSA_CONTEXT_LINK_OBJECT(pLink);
        *pInsNumber = pSubCosaContext->InstanceNumber;
    }
    
    return pSubCosaContext;
}

ANSC_HANDLE
MacFiltTab_AddEntry
    (
        ANSC_HANDLE                 hInsContext,
        ULONG*                      pInsNumber
    )
{
    PCOSA_DATAMODEL_WIFI            pWiFi           = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pParentLinkObj  = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP_FULL          pWiFiAP         = (PCOSA_DML_WIFI_AP_FULL)pParentLinkObj->hContext;
    PCOSA_CONTEXT_LINK_OBJECT       pSubCosaContext = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PCOSA_DML_WIFI_AP_MAC_FILTER    pMacFilt        = (PCOSA_DML_WIFI_AP_MAC_FILTER)NULL;

    pMacFilt = AnscAllocateMemory(sizeof(COSA_DML_WIFI_AP_MAC_FILTER));
    if ( !pMacFilt )
    {
        return NULL;
    }

    _ansc_sprintf(pMacFilt->Alias, "MacFilterTable%lu", pWiFiAP->ulMacFilterNextInsNum);
    pMacFilt->InstanceNumber = pWiFiAP->ulMacFilterNextInsNum;

    /* Update the middle layer data */
    pSubCosaContext = (PCOSA_CONTEXT_LINK_OBJECT)AnscAllocateMemory(sizeof(COSA_CONTEXT_LINK_OBJECT));
    if ( !pSubCosaContext )
    {
        AnscFreeMemory(pMacFilt);
        return NULL;
    }

    pSubCosaContext->InstanceNumber = pWiFiAP->ulMacFilterNextInsNum;
    pWiFiAP->ulMacFilterNextInsNum++;
    if ( 0 == pWiFiAP->ulMacFilterNextInsNum )
    {
        pWiFiAP->ulMacFilterNextInsNum = 1;
    }

    pSubCosaContext->hContext         = (ANSC_HANDLE)pMacFilt;
    pSubCosaContext->hParentTable     = pWiFiAP;
    pSubCosaContext->hPoamIrepUpperFo = pWiFi->hIrepFolderWifiAP;
    pSubCosaContext->bNew             = TRUE;

    CosaSListPushEntryByInsNum((PSLIST_HEADER)&pWiFiAP->MacFilterList, pSubCosaContext);

    CosaWifiRegAddMacFiltInfo
        (
         (ANSC_HANDLE)pWiFiAP,
         (ANSC_HANDLE)pSubCosaContext
        );

    *pInsNumber = pSubCosaContext->InstanceNumber;

    return pSubCosaContext;
}

ULONG
MacFiltTab_DelEntry
    (
        ANSC_HANDLE                 hInsContext,
        ANSC_HANDLE                 hInstance
    )
{
    ANSC_STATUS                     returnStatus    = ANSC_STATUS_SUCCESS;
    PCOSA_DATAMODEL_WIFI            pWiFi           = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi; 
    PCOSA_CONTEXT_LINK_OBJECT       pParentLinkObj  = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP_FULL          pWiFiAP         = (PCOSA_DML_WIFI_AP_FULL)pParentLinkObj->hContext;
    PCOSA_CONTEXT_LINK_OBJECT       pSubCosaContext = (PCOSA_CONTEXT_LINK_OBJECT)hInstance;
    PCOSA_DML_WIFI_AP_MAC_FILTER    pMacFilt        = (PCOSA_DML_WIFI_AP_MAC_FILTER)pSubCosaContext->hContext;

    CosaDmlMacFilt_DelEntry(pParentLinkObj->InstanceNumber, pSubCosaContext->InstanceNumber);

    AnscSListPopEntryByLink((PSLIST_HEADER)&pWiFiAP->MacFilterList, &pSubCosaContext->Linkage);

    if ( pSubCosaContext->bNew )
    {
        CosaWifiRegDelMacFiltInfo(pWiFiAP, (ANSC_HANDLE)pSubCosaContext);
    }

    AnscFreeMemory(pMacFilt);
    AnscFreeMemory(pSubCosaContext);

    return 0;
}

ULONG
MacFiltTab_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pCosaContext    = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP_MAC_FILTER    pMacFilt        = (PCOSA_DML_WIFI_AP_MAC_FILTER)pCosaContext->hContext;
    
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "MACAddress", TRUE))
    {
        /* collect value */
        AnscCopyString(pValue, pMacFilt->MACAddress);
        return 0;
    }
    if( AnscEqualString(ParamName, "DeviceName", TRUE))
    {
        /* collect value */
        AnscCopyString(pValue, pMacFilt->DeviceName);
        return 0;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return -1;
}

BOOL
MacFiltTab_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pString
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pCosaContext    = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP_MAC_FILTER    pMacFilt        = (PCOSA_DML_WIFI_AP_MAC_FILTER)pCosaContext->hContext;
    
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "MACAddress", TRUE))
    {
        /* save update to backup */
        if (AnscSizeOfString(pString) >= sizeof(pMacFilt->MACAddress))
            return FALSE;
        AnscCopyString(pMacFilt->MACAddress, pString);
        return TRUE;
    }
    if( AnscEqualString(ParamName, "DeviceName", TRUE))
    {
        /* save update to backup */
        if (AnscSizeOfString(pString) >= sizeof(pMacFilt->DeviceName))
            return FALSE;
        AnscCopyString(pMacFilt->DeviceName, pString);
        return TRUE;
    }


    return FALSE;
}

BOOL
MacFiltTab_Validate
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pReturnParamName,
        ULONG*                      puLength
    )
{
    return TRUE;
}

ULONG
MacFiltTab_Commit
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI            pWiFi           = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pCosaContext    = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP_MAC_FILTER    pMacFilt        = (PCOSA_DML_WIFI_AP_MAC_FILTER)pCosaContext->hContext;
    PCOSA_DML_WIFI_AP_FULL          pWiFiAP         = (PCOSA_DML_WIFI_AP_FULL)pCosaContext->hParentTable;

    if ( pCosaContext->bNew )
    {
        pCosaContext->bNew = FALSE;

        CosaDmlMacFilt_AddEntry(pWiFiAP->Cfg.InstanceNumber, pMacFilt);

        CosaWifiRegDelMacFiltInfo(pWiFiAP, (ANSC_HANDLE)pCosaContext);
    }
    else
    {
        CosaDmlMacFilt_SetConf(pWiFiAP->Cfg.InstanceNumber, pMacFilt->InstanceNumber, pMacFilt);
    }
    
    return 0;
}

ULONG
MacFiltTab_Rollback
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI            pWiFi           = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pCosaContext    = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP_MAC_FILTER    pMacFilt        = (PCOSA_DML_WIFI_AP_MAC_FILTER)pCosaContext->hContext;
    PCOSA_DML_WIFI_AP_FULL          pWiFiAP         = (PCOSA_DML_WIFI_AP_FULL)pCosaContext->hParentTable;

    CosaDmlMacFilt_GetConf(pWiFiAP->Cfg.InstanceNumber, pMacFilt->InstanceNumber, pMacFilt);

    return 0;
}

BOOL
NeighboringWiFiDiagnostic_GetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL*                       pBool
    )
{

    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;

    if (AnscEqualString(ParamName, "Enable", TRUE))
    {
        *pBool = pMyObject->Diagnostics.bEnable;
        return TRUE;
    }

        return FALSE;
}

ULONG
NeighboringWiFiDiagnostic_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{

    PCOSA_DATAMODEL_WIFI            pMyObject      = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_NEIGHTBOURING_WIFI_DIAG_CFG  pDiagnostics = (PCOSA_DML_NEIGHTBOURING_WIFI_DIAG_CFG)hInsContext;
    if(AnscEqualString(ParamName, "DiagnosticsState", TRUE))
    {
        AnscCopyString(pValue,pMyObject->Diagnostics.DiagnosticsState);
        return 0;
    }
    return -1;
}

BOOL
NeighboringWiFiDiagnostic_SetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL                        bValue
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    if(AnscEqualString(ParamName, "Enable", TRUE))
    {
// Set WiFi Neighbour Diagnostic switch value
    CosaDmlSetNeighbouringDiagnosticEnable(bValue);
        pMyObject->Diagnostics.bEnable = bValue;
        return TRUE;
    }
        return FALSE;
}


BOOL
NeighboringWiFiDiagnostic_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pString
    )
{

    PCOSA_DATAMODEL_WIFI            pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_NEIGHTBOURING_WIFI_DIAG_CFG  pDiagnostics = (PCOSA_DML_NEIGHTBOURING_WIFI_DIAG_CFG)hInsContext;

    if( AnscEqualString(ParamName, "DiagnosticsState", TRUE))   {
        if( AnscEqualString(pString, "Requested", TRUE)) {
                 if( pMyObject->Diagnostics.bEnable )   {
                        if(AnscEqualString(pMyObject->Diagnostics.DiagnosticsState, "Requested", TRUE))
                                return TRUE;
                        CosaDmlWiFi_doNeighbouringScan(&pMyObject->Diagnostics);
                        return TRUE;
                 }
                 else
                 {
                        return FALSE;
                 }
        }
    }
        return FALSE;
 }



ULONG
NeighboringScanResult_GetEntryCount
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject      = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    return pMyObject->Diagnostics.ResultCount;
}
ANSC_HANDLE
NeighboringScanResult_GetEntry
    (
        ANSC_HANDLE                 hInsContext,
        ULONG                       nIndex,
        ULONG*                      pInsNumber
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject      = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    //PCOSA_DML_NEIGHTBOURING_WIFI_RESULT pNeighbourResult = (PCOSA_DML_NEIGHTBOURING_WIFI_RESULT)pMyObject->Diagnostics.pResult;

    if ( nIndex >= pMyObject->Diagnostics.ResultCount )
                return NULL;

        *pInsNumber  = nIndex + 1;
        if(nIndex < pMyObject->Diagnostics.ResultCount_2)
                return (ANSC_HANDLE)&pMyObject->Diagnostics.pResult_2[nIndex];
        else
                return (ANSC_HANDLE)&pMyObject->Diagnostics.pResult_5[nIndex-pMyObject->Diagnostics.ResultCount_2];
}

BOOL
NeighboringScanResult_IsUpdated
    (
        ANSC_HANDLE                 hInsContext
    )
{
        return TRUE;

}



BOOL
NeighboringScanResult_GetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int*                        pInt
    )
{
    PCOSA_DML_NEIGHTBOURING_WIFI_RESULT       pResult       = (PCOSA_DML_NEIGHTBOURING_WIFI_RESULT)hInsContext;

    if( AnscEqualString(ParamName, "SignalStrength", TRUE))    {
        *pInt = pResult->SignalStrength;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "Noise", TRUE))    {
        *pInt = pResult->Noise;
        return TRUE;
    }
        return FALSE;
}

BOOL
NeighboringScanResult_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG*                      puLong
    )
{
    PCOSA_DML_NEIGHTBOURING_WIFI_RESULT       pResult       = (PCOSA_DML_NEIGHTBOURING_WIFI_RESULT)hInsContext;

    if( AnscEqualString(ParamName, "DTIMPeriod", TRUE))    {
        *puLong = pResult->DTIMPeriod;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "X_COMCAST-COM_ChannelUtilization", TRUE))    {
        *puLong = pResult->ChannelUtilization;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "Channel", TRUE))    {
                *puLong = pResult->Channel;
        return TRUE;
    }
    if(AnscEqualString(ParamName, "BeaconPeriod", TRUE))   {
       *puLong = pResult->BeaconPeriod;
       return TRUE;
    }

    return FALSE;
}

ULONG
NeighboringScanResult_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
        PCOSA_DML_NEIGHTBOURING_WIFI_RESULT       pResult       = (PCOSA_DML_NEIGHTBOURING_WIFI_RESULT)hInsContext;

        if( AnscEqualString(ParamName, "Radio", TRUE))    {
                if(AnscEqualString(pResult->OperatingFrequencyBand, "5GHz", TRUE))
                        AnscCopyString(pValue, "Device.WiFi.Radio.2");
                else
                        AnscCopyString(pValue, "Device.WiFi.Radio.1");
        return 0;
    }
    if(AnscEqualString(ParamName, "EncryptionMode", TRUE))    {
      AnscCopyString(pValue,pResult->EncryptionMode);
        return 0;
    }
    if( AnscEqualString(ParamName, "Mode", TRUE))    {
        AnscCopyString(pValue,pResult->Mode);
        return 0;
    }
    if( AnscEqualString(ParamName, "SecurityModeEnabled", TRUE))    {
        AnscCopyString(pValue,pResult->SecurityModeEnabled);
        return 0;
    }
    if( AnscEqualString(ParamName, "BasicDataTransferRates", TRUE))    {
        AnscCopyString(pValue,pResult->BasicDataTransferRates);
        return 0;
    }
    if( AnscEqualString(ParamName, "SupportedDataTransferRates", TRUE))    {
       AnscCopyString(pValue,pResult->SupportedDataTransferRates);
        return 0;
    }
    if( AnscEqualString(ParamName, "OperatingChannelBandwidth", TRUE))    {
        AnscCopyString(pValue,pResult->OperatingChannelBandwidth);
        return 0;
    }
    if( AnscEqualString(ParamName, "OperatingStandards", TRUE))    {
        AnscCopyString(pValue,pResult->OperatingStandards);
        return 0;
    }
    if( AnscEqualString(ParamName, "SupportedStandards", TRUE))    {
       AnscCopyString(pValue,pResult->SupportedStandards);
       return 0;
    }
    if( AnscEqualString(ParamName, "BSSID", TRUE))    {
        AnscCopyString(pValue,pResult->BSSID);
        return 0;
    }
    if(AnscEqualString(ParamName, "SSID", TRUE))     {
        AnscCopyString(pValue,pResult->SSID);
        return 0;
    }
    if( AnscEqualString(ParamName, "OperatingFrequencyBand", TRUE))    {
        AnscCopyString(pValue,pResult->OperatingFrequencyBand);
        return 0;
    }

    return -1;

 }

/***********************************************************************

 APIs for Object:

    WiFi.X_RDKCENTRAL-COM_ATM

    *  ATM_GetParamBoolValue
    *  ATM_GetParamUlongValue
        *  ATM_SetParamBoolValue
        
***********************************************************************/
/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        ATM_GetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL*                       pBool
            );

    description:

        This function is called to retrieve Boolean parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL*                       pBool
                The buffer of returned boolean value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
ATM_GetParamBoolValue
(
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL*                       pBool
)
{
        PCOSA_DATAMODEL_WIFI    pMyObject       = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
        PCOSA_DML_WIFI_ATM              pATM   = pMyObject->pATM;
        if (AnscEqualString(ParamName, "Capable", TRUE)) {
                 *pBool = pATM->Capable;
                return TRUE;
        }

    if (AnscEqualString(ParamName, "Enable", TRUE)) {
                *pBool = pATM->Enable;
                return TRUE;
        }

    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        ATM_SetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                BOOL                        bValue
            );

    description:

        This function is called to set BOOL parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                BOOL                        bValue
                The updated BOOL value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
ATM_SetParamBoolValue
(
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL                        bValue
)
{
 PCOSA_DATAMODEL_WIFI    pMyObject       = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
        PCOSA_DML_WIFI_ATM              pATM   = pMyObject->pATM;
    if( AnscEqualString(ParamName, "Enable", TRUE)) {
                CosaDmlWiFi_SetATMEnable(pATM, bValue); //RDKB-EMU
                return TRUE;
        }

        return FALSE;
}

BOOL
ATM_Validate
(
        ANSC_HANDLE                             hInsContext,
        char*                                   pReturnParamName,
        ULONG*                                  puLength
)
{
        PCOSA_DATAMODEL_WIFI    pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
        PCOSA_DML_WIFI_ATM              pATM   = pMyObject->pATM;

        return TRUE;
}


ULONG
ATM_Commit
(
        ANSC_HANDLE                              hInsContext
)
{
        PCOSA_DATAMODEL_WIFI    pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
        PCOSA_DML_WIFI_ATM              pATM   = pMyObject->pATM;

        //CosaDmlWiFi_SetATMEnable(pATM, bValue);
        return ANSC_STATUS_SUCCESS;
}

ULONG
ATM_Rollback
(
        ANSC_HANDLE                              hInsContext
)
{
        PCOSA_DATAMODEL_WIFI    pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
        PCOSA_DML_WIFI_ATM              pATM   = pMyObject->pATM;

        //CosaDmlWiFi_GetATMEnable(pATM, &bValue);
        return ANSC_STATUS_SUCCESS;
}

/***********************************************************************

 APIs for Object:

    WiFi.APGroup.{i}.

    *  APGroup_GetEntryCount
    *  APGroup_GetEntry
    *  APGroup_AddEntry
    *  APGroup_DelEntry
    *  APGroup_GetParamUlongValue
    *  APGroup_GetParamStringValue
    *  APGroup_Validate
    *  APGroup_Commit
    *  APGroup_Rollback

***********************************************************************/

ULONG
APGroup_GetEntryCount
(
        ANSC_HANDLE                 hInsContext
)
{
        CcspTraceInfo(("APGroup_GetEntryCount \n"));

        PCOSA_DATAMODEL_WIFI            pWiFi     = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
        PCOSA_DML_WIFI_ATM                              pATM = pWiFi->pATM;

    return pATM->grpCount;


}


ANSC_HANDLE
APGroup_GetEntry
(
        ANSC_HANDLE                 hInsContext,
        ULONG                       nIndex,
        ULONG*                      pInsNumber
)
{
        CcspTraceInfo(("APGroup_GetEntry '%d'\n", nIndex));
        PCOSA_DATAMODEL_WIFI            pWiFi     = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
        PCOSA_DML_WIFI_ATM                              pATM = pWiFi->pATM;
        //PCOSA_DML_WIFI_ATM_APGROUP            pATMApGroup=&pATM->APGroup;

        if(0<=nIndex && nIndex < pATM->grpCount) {
                *pInsNumber=nIndex+1;
                return pATM->APGroup+nIndex;
        }
                return NULL;
}


/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        APGroup_GetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pValue,
                ULONG*                      pUlSize
            );

    description:

        This function is called to retrieve string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pValue,
                The string value buffer;

                ULONG*                      pUlSize
                The buffer of length of string value;
                Usually size of 1023 will be used.
                If it's not big enough, put required size here and return 1;

    return:     0 if succeeded;
                1 if short of buffer size; (*pUlSize = required size)
                -1 if not supported.

**********************************************************************/
ULONG
APGroup_GetParamStringValue

    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
        CcspTraceInfo(("APGroup_GetParamStringValue parameter '%s'\n", ParamName));
         PCOSA_DML_WIFI_ATM_APGROUP      pWifiApGrp    = (PCOSA_DML_WIFI_ATM_APGROUP)hInsContext;

        if( AnscEqualString(ParamName, "APList", TRUE)) {
        AnscCopyString(pValue, pWifiApGrp->APList);
        return 0;
    }

    return -1;
}


/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        APGroup_GetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG*                      puLong
            );

    description:

        This function is called to retrieve ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG*                      puLong
                The buffer of returned ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
APGroup_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG*                      puLong
    )
{
    CcspTraceInfo(("APGroup_GetParamUlongValue parameter '%s'\n", ParamName));
        PCOSA_DML_WIFI_ATM_APGROUP      pWifiApGrp    = (PCOSA_DML_WIFI_ATM_APGROUP)hInsContext;

    if( AnscEqualString(ParamName, "AirTimePercent", TRUE)) {
        *puLong = pWifiApGrp->AirTimePercent; // collect from corresponding AP object
                return TRUE;
    }

    return FALSE;
}

BOOL
APGroup_SetParamUlongValue (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG                       uValue
)
   {
    	CcspTraceInfo(("APGroup_SetParamUlongValue parameter '%s'\n", ParamName));
	CcspTraceInfo(("---- %s %s \n", __func__,       ParamName));
        PCOSA_DML_WIFI_ATM_APGROUP      pWifiApGrp    = (PCOSA_DML_WIFI_ATM_APGROUP)hInsContext;

        if( AnscEqualString(ParamName, "AirTimePercent", TRUE))   {
                pWifiApGrp->AirTimePercent= uValue;
                CosaDmlWiFi_SetATMAirTimePercent(pWifiApGrp->APList, pWifiApGrp->AirTimePercent);
        return TRUE;
    }

    return FALSE;
}


/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        APGroup_Validate
            (
                ANSC_HANDLE                 hInsContext,
                char*                       pReturnParamName,
                ULONG*                      puLength
            );

    description:

        This function is called to finally commit all the update.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;
  
                char*                       pReturnParamName,
                The buffer (128 bytes) of parameter name if there's a validation. 

                ULONG*                      puLength
                The output length of the param name. 

    return:     TRUE if there's no validation.

**********************************************************************/
BOOL
APGroup_Validate
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pReturnParamName,
        ULONG*                      puLength
    )
{
        CcspTraceInfo(("APGroup_Validate parameter '%s'\n", pReturnParamName));
    	return TRUE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        APGroup_Commit
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to finally commit all the update.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
APGroup_Commit
    (
        ANSC_HANDLE                 hInsContext
    )
{
        CcspTraceInfo(("APGroup_Commit parameter \n"));
    	return 0;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        APGroup_Rollback
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to roll back the update whenever there's a 
        validation found.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
APGroup_Rollback
    (
        ANSC_HANDLE                 hInsContext
    )
{
        CcspTraceInfo(("APGroup_Rollback parameter \n"));
    	return ANSC_STATUS_SUCCESS;
}

/***********************************************************************

 APIs for Object:

    WiFi.X_RDKCENTRAL-COM_ATM.APGroup.{i}.Sta.{j}.

    *  Sta_GetEntryCount
    *  Sta_GetEntry
    *  Sta_AddEntry
    *  Sta_DelEntry
    *  Sta_GetParamUlongValue
    *  Sta_GetParamStringValue
    *  Sta_SetParamUlongValue
    *  Sta_SetParamStringValue
    *  Sta_Validate
    *  Sta_Commit
    *  Sta_Rollback

***********************************************************************/
/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        Sta_GetEntryCount
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to retrieve the count of the table.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The count of the table

**********************************************************************/
ULONG
Sta_GetEntryCount
    (
        ANSC_HANDLE                 hInsContext
    )
{
        PCOSA_DML_WIFI_ATM_APGROUP      pATMApGroup=(PCOSA_DML_WIFI_ATM_APGROUP)hInsContext;
        return pATMApGroup->NumberSta;
}

/**********************************************************************  

       caller:     owner of this object 

    prototype: 

        ANSC_HANDLE
        Sta_GetEntry
            (
                ANSC_HANDLE                 hInsContext,
                ULONG                       nIndex,
                ULONG*                      pInsNumber
            );

    description:

        This function is called to retrieve the entry specified by the index.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                ULONG                       nIndex,
                The index of this entry;

                ULONG*                      pInsNumber
                The output instance number;

    return:     The handle to identify the entry

**********************************************************************/
ANSC_HANDLE
Sta_GetEntry
    (
        ANSC_HANDLE                 hInsContext,
        ULONG                       nIndex,
        ULONG*                      pInsNumber
    )
{
        CcspTraceInfo(("Sta_GetEntry parameter '%d'\n", nIndex));
        PCOSA_DML_WIFI_ATM_APGROUP      pATMApGroup=(PCOSA_DML_WIFI_ATM_APGROUP)hInsContext;
        if(0<=nIndex && nIndex<COSA_DML_WIFI_ATM_MAX_STA_NUM) {
                *pInsNumber=nIndex+1;
                return (ANSC_HANDLE)&pATMApGroup->StaList[nIndex];
        }
        return NULL;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

          ANSC_HANDLE
        Sta_AddEntry
            (
                ANSC_HANDLE                 hInsContext,
                ULONG*                      pInsNumber
            );

    description:

        This function is called to add a new entry.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                ULONG*                      pInsNumber
                The output instance number;

    return:     The handle of new added entry.

**********************************************************************/
ANSC_HANDLE
Sta_AddEntry
    (
        ANSC_HANDLE                 hInsContext,
        ULONG*                      pInsNumber
    )
{
        CcspTraceInfo(("Sta_AddEntry parameter '%d'\n", pInsNumber));
        PCOSA_DML_WIFI_ATM_APGROUP              pATMApGroup= (PCOSA_DML_WIFI_ATM_APGROUP)hInsContext;
        PCOSA_DML_WIFI_ATM_APGROUP_STA  pATMApSta=NULL;
        if(pATMApGroup->NumberSta < (COSA_DML_WIFI_ATM_MAX_STA_NUM-1)) {
                pATMApGroup->NumberSta+=1;
                *pInsNumber=pATMApGroup->NumberSta;
                pATMApSta=&pATMApGroup->StaList[pATMApGroup->NumberSta-1];
                memset(pATMApSta, 0, sizeof(COSA_DML_WIFI_ATM_APGROUP_STA));
                pATMApSta->pAPList=pATMApGroup->APList;
                return (ANSC_HANDLE)pATMApSta;
        }
        return NULL;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        Sta_DelEntry
            (
                ANSC_HANDLE                 hInsContext,
                ANSC_HANDLE                 hInstance
            );

    description:

        This function is called to delete an exist entry.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                ANSC_HANDLE                 hInstance
                The exist entry handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
Sta_DelEntry
    (
        ANSC_HANDLE                 hInsContext,
        ANSC_HANDLE                 hInstance
    )
{
        CcspTraceInfo(("Sta_DelEntry  \n"));
        PCOSA_DML_WIFI_ATM_APGROUP              pATMApGroup= (PCOSA_DML_WIFI_ATM_APGROUP)hInsContext;
        PCOSA_DML_WIFI_ATM_APGROUP_STA  pATMApGroupSta=(PCOSA_DML_WIFI_ATM_APGROUP_STA)hInstance;
        int             uInstance=pATMApGroupSta-pATMApGroup->StaList;
        //MACAddress=pATMApGroup->StaList[hInstance-1].MACAddress; 
        //APList=pATMApGroup->APList;   
        CosaDmlWiFi_SetATMSta(pATMApGroup->APList, pATMApGroupSta->MACAddress, 0); //Delete sta

        //shift 
        int mvcount=pATMApGroup->NumberSta-uInstance;
        if(mvcount>0)
                memmove(pATMApGroupSta, pATMApGroupSta+1, mvcount*sizeof(COSA_DML_WIFI_ATM_APGROUP_STA));
        pATMApGroup->NumberSta-=1;

        return ANSC_STATUS_SUCCESS;

}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        Sta_GetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pValue,
                ULONG*                      pUlSize
            );

    description:

        This function is called to retrieve string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pValue,
                The string value buffer;

                ULONG*                      pUlSize
                The buffer of length of string value;
                Usually size of 1023 will be used.
                If it's not big enough, put required size here and return 1;

    return:     0 if succeeded;
                1 if short of buffer size; (*pUlSize = required size)
                -1 if not supported.

**********************************************************************/
ULONG
Sta_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
    CcspTraceInfo(("Sta_GetParamStringValue parameter '%s'\n", ParamName));
        PCOSA_DML_WIFI_ATM_APGROUP_STA pATMApSta = (PCOSA_DML_WIFI_ATM_APGROUP_STA)hInsContext;

        if( AnscEqualString(ParamName, "MACAddress", TRUE)) {
        /* collect value */
                AnscCopyString(pValue, pATMApSta->MACAddress);
                *pUlSize = AnscSizeOfString(pValue);
        return 0;
    }

    return FALSE;
}
/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Sta_GetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG*                      puLong
            );

    description:

        This function is called to retrieve ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG*                      puLong
                The buffer of returned ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Sta_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG*                      puLong
    )
{
    CcspTraceInfo(("Sta_GetParamUlongValue parameter '%s'\n", ParamName));
        PCOSA_DML_WIFI_ATM_APGROUP_STA pATMApSta   = (PCOSA_DML_WIFI_ATM_APGROUP_STA)hInsContext;
        if( AnscEqualString(ParamName, "AirTimePercent", TRUE))  {
        /* collect value */
        *puLong = pATMApSta->AirTimePercent; // collect from corresponding AP->Sta object
        return TRUE;
    }
    return FALSE;
}
/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Sta_SetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                char*                       pString
            );

    description:

        This function is called to set string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                char*                       pString
                The updated string value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Sta_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pString
    )
{
    CcspTraceInfo(("Sta_SetParamStringValue parameter '%s'\n", ParamName));
    PCOSA_DML_WIFI_ATM_APGROUP_STA pWifiApGrpSta   = (PCOSA_DML_WIFI_ATM_APGROUP_STA)hInsContext;
    if( AnscEqualString(ParamName, "MACAddress", TRUE)) {
                if (AnscSizeOfString(pString) >= sizeof(pWifiApGrpSta->MACAddress))
                        return FALSE;
                if(0==strcasecmp(pWifiApGrpSta->MACAddress, pString))
                        return TRUE;

                AnscCopyString(pWifiApGrpSta->MACAddress, pString);
                return TRUE;
    }
    return FALSE;
}
/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Sta_SetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                ULONG                       uValue
            );

    description:

        This function is called to set ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                ULONG                       uValue
                The updated ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Sta_SetParamUlongValue
        (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG                       uValue
    )
{
    CcspTraceInfo(("Sta_SetParamIntValue parameter '%s'\n", ParamName));

        PCOSA_DML_WIFI_ATM_APGROUP_STA pWifiApGrpSta   = (PCOSA_DML_WIFI_ATM_APGROUP_STA)hInsContext;
        if( AnscEqualString(ParamName, "AirTimePercent", TRUE)) {

                pWifiApGrpSta->AirTimePercent = uValue;

                if(pWifiApGrpSta->MACAddress[0]!=0)
                        CosaDmlWiFi_SetATMSta(pWifiApGrpSta->pAPList, pWifiApGrpSta->MACAddress, pWifiApGrpSta->AirTimePercent);

                return TRUE;
        }
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Sta_Validate
            (
                ANSC_HANDLE                 hInsContext,
                char*                       pReturnParamName,
                ULONG*                      puLength
            );

    description:

        This function is called to finally commit all the update.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       pReturnParamName,
                The buffer (128 bytes) of parameter name if there's a validation. 

                ULONG*                      puLength
                The output length of the param name. 

    return:     TRUE if there's no validation.

**********************************************************************/
BOOL
Sta_Validate
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pReturnParamName,
        ULONG*                      puLength
    )
{
        CcspTraceInfo(("Sta_Validate parameter '%s'\n",pReturnParamName));
    	return TRUE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 
          ULONG
        Sta_Commit
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to finally commit all the update.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
Sta_Commit
    (
        ANSC_HANDLE                 hInsContext
    )
{
        CcspTraceInfo(("Sta_Commit parameter \n"));
    	return TRUE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        ULONG
        Sta_Rollback
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to roll back the update whenever there's a 
        validation found.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.
**********************************************************************/
ULONG
Sta_Rollback
    (
        ANSC_HANDLE                 hInsContext
    )
{
        CcspTraceInfo(("Sta_Rollback parameter \n"));
    	return ANSC_STATUS_SUCCESS;
}

