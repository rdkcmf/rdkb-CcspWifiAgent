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

    copyright:

        Cisco Systems, Inc.
        All Rights Reserved.

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

extern void* g_pDslhDmlAgent;
extern int gChannelSwitchingCount;
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

static int isHex (char *string);
static BOOL isHotspotSSIDIpdated = FALSE;
static BOOL isBeaconRateUpdate[16] = { FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE };
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

static BOOL isValidSSID(char *SSID)
{
    // "alphabet, digit, underscore, hyphen and dot"
    char *validChars = "_-."; 
    int i = 0;
    BOOL result = true;

    for (i = 0; i < AnscSizeOfString(SSID); i++) {
        if ( (isalnum(SSID[i]) == 0) &&
              (strchr(validChars, SSID[i]) == NULL)) {
            result = false;
            break;
        }
    }
    return result;
}
//zqiu>>
//#define WIFIEXT_DM_HOTSPOTSSID_UPDATE  "Device.X_COMCAST_COM_GRE.Interface.1.DHCPCircuitIDSSID"
#define WIFIEXT_DM_HOTSPOTSSID_UPDATE  "Device.X_COMCAST-COM_GRE.Tunnel.1.EnableCircuitID"
//zqiu<<
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

BOOL UpdateCircuitId()
{

    char *dstComponent = NULL;
	char *dstPath = NULL;
	parameterValStruct_t    **valStructs = NULL;
	char                    *paramNameList[1];
	char                    tmpPath[64];
	extern ANSC_HANDLE bus_handle;
    extern char        g_Subsystem[32];
	int                     valNum = 0;
	pthread_detach(pthread_self());
	sleep(7);

	if (!Cosa_FindDestComp(WIFIEXT_DM_HOTSPOTSSID_UPDATE, &dstComponent, &dstPath) || !dstComponent || !dstPath)
	{
		if(dstComponent)
		{
			AnscFreeMemory(dstComponent);
		}
		if(dstPath)
		{
			AnscFreeMemory(dstPath);
		}
		return FALSE;
	}

	sprintf(tmpPath,"%s",WIFIEXT_DM_HOTSPOTSSID_UPDATE);
    paramNameList[0] = tmpPath;
    if(CcspBaseIf_getParameterValues(bus_handle, dstComponent, dstPath,
                paramNameList, 1, &valNum, &valStructs) != CCSP_SUCCESS)
    {
	    free_parameterValStruct_t(bus_handle, valNum, valStructs);
		if(dstComponent)
		{
			AnscFreeMemory(dstComponent);
		}
		if(dstPath)
		{
			AnscFreeMemory(dstPath);
		}
		return FALSE;
	}

	if(dstComponent)
	{
		AnscFreeMemory(dstComponent);
	}
	if(dstPath)
	{
		AnscFreeMemory(dstPath);
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
    if (AnscEqualString(ParamName, "X_CISCO_COM_EnableTelnet", TRUE))
    {
        *pBool = pMyObject->bTelnetEnabled;
	    return TRUE;
    }
    if (AnscEqualString(ParamName, "X_CISCO_COM_ResetRadios", TRUE))
    {
        /* always return false when get */
        *pBool = FALSE;
        return TRUE;
    }
    if (AnscEqualString(ParamName, "X_RDKCENTRAL-COM_WiFiHost_Sync", TRUE))
    {
	
        *pBool = FALSE;
        return TRUE;
    }

    if (AnscEqualString(ParamName, "X_RDKCENTRAL-COM_PreferPrivate", TRUE))
    {
        CosaDmlWiFi_GetPreferPrivatePsmData(pBool);
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

    if( AnscEqualString(ParamName, "X_CISCO_COM_RadioPower", TRUE))
    { 
        COSA_DML_WIFI_RADIO_POWER power;
        CosaDmlWiFi_GetRadioPower(&power);

        if ( power == COSA_DML_WIFI_POWER_LOW )
        {
            AnscCopyString(pValue, "PowerLow");
        } else if ( power == COSA_DML_WIFI_POWER_DOWN )
        { 
            AnscCopyString(pValue, "PowerDown");
        } else if ( power == COSA_DML_WIFI_POWER_UP )
        { 
            AnscCopyString(pValue, "PowerUp");
        } 
        return 0;
    }

    if (AnscEqualString(ParamName, "X_CISCO_COM_ConfigFileBase64", TRUE))
    {
        /* at lest support 32786 after base64 config */
        if (*pUlSize < 32786)
        {
            *pUlSize = 32786;
            return 1;
        }

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
        {
            AnscFreeMemory(binConf); /*RDKB-6905, CID-32900, free unused resource before exit */
            return -1;
        }

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
    }

        return 0;
    }
void* WiFi_HostSyncThread()
{
	CcspTraceWarning(("RDK_LOG_WARN, %s-%d \n",__FUNCTION__,__LINE__));
	pthread_detach(pthread_self());
	Wifi_Hosts_Sync_Func(NULL,0, NULL, 1);
}
BOOL
WiFi_SetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL                        bValue
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject     = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;

    if(AnscEqualString(ParamName, "X_CISCO_COM_FactoryReset", TRUE))
    {
        CosaDmlWiFi_FactoryReset();
        return TRUE;
    }

    if(AnscEqualString(ParamName, "X_CISCO_COM_EnableTelnet", TRUE))
    {
        if ( CosaDmlWiFi_EnableTelnet(bValue) == ANSC_STATUS_SUCCESS ) {
	    // Set parameter value only if the command was successful
	    pMyObject->bTelnetEnabled = bValue;
	}
	        return TRUE;
    }

    if(AnscEqualString(ParamName, "X_CISCO_COM_ResetRadios", TRUE))
    {
	CosaDmlWiFi_ResetRadios();
        return TRUE;
    }
    if (AnscEqualString(ParamName, "X_RDKCENTRAL-COM_WiFiHost_Sync", TRUE))
    {
		pthread_t WiFi_HostSync_Thread;
    	int res;
        res = pthread_create(&WiFi_HostSync_Thread, NULL, WiFi_HostSyncThread, NULL);		
		if(res != 0)
			CcspTraceWarning(("Create Send_Notification_Thread error %d ", res));
        //Wifi_Hosts_Sync_Func(NULL,0, NULL, 1);
        return TRUE;
    }

    if (AnscEqualString(ParamName, "X_RDKCENTRAL-COM_PreferPrivate", TRUE))
    {
        if(CosaDmlWiFi_SetPreferPrivatePsmData(bValue) == ANSC_STATUS_SUCCESS)
            return TRUE;
    }

    return FALSE;
}

void CosaDmlWiFi_UpdateMfCfg( void )
{
        PCOSA_DATAMODEL_WIFI            pMyObject       = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
        PCOSA_CONTEXT_LINK_OBJECT       pLinkObjAp      = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
        PSINGLE_LINK_ENTRY              pSLinkEntryAp   = (PSINGLE_LINK_ENTRY       )NULL;
        int  idx[4] = {5,6,9,10};
        int  i;

        for(i=0; i<4; i++)
        {
                pSLinkEntryAp = AnscQueueGetEntryByIndex( &pMyObject->AccessPointQueue, idx[i]-1);

                if ( pSLinkEntryAp )
                {
                        pLinkObjAp      = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntryAp);

                        if( pLinkObjAp )
                        {
                                PCOSA_CONTEXT_LINK_OBJECT               pLinkObj           = (PCOSA_CONTEXT_LINK_OBJECT)pLinkObjAp;
                                PCOSA_DML_WIFI_AP                       pWifiAp            = (PCOSA_DML_WIFI_AP           )pLinkObj->hContext;
                                PCOSA_DML_WIFI_AP_MF_CFG                pWifiApMf          = (PCOSA_DML_WIFI_APWPS_FULL)&pWifiAp->MF;

                                PSINGLE_LINK_ENTRY                      pSLinkEntry    = (PSINGLE_LINK_ENTRY             )NULL;
                                PCOSA_CONTEXT_LINK_OBJECT               pSSIDLinkObj   = (PCOSA_CONTEXT_LINK_OBJECT      )NULL;
                                PCOSA_DML_WIFI_SSID                     pWifiSsid      = (PCOSA_DML_WIFI_SSID            )NULL;
                                UCHAR                                   PathName[64] = {0};

                                pSLinkEntry = AnscQueueGetFirstEntry(&pMyObject->SsidQueue);

                                while ( pSLinkEntry )
                                {
                                        pSSIDLinkObj = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);
                                        pWifiSsid        = pSSIDLinkObj->hContext;

                                        sprintf(PathName, "Device.WiFi.SSID.%d.", pSSIDLinkObj->InstanceNumber);

					if ( AnscEqualString(pWifiAp->AP.Cfg.SSID, PathName, TRUE) )
                                        {
                                                break;
                                        }

                                        pSLinkEntry = AnscQueueGetNextEntry(pSLinkEntry);
                                }

                                #if !defined(_COSA_INTEL_USG_ATOM_) && !defined(_COSA_BCM_MIPS_)  && !defined(_COSA_BCM_ARM_)
                                        CosaDmlWiFiApMfGetCfg( NULL, pWifiSsid->SSID.Cfg.SSID, pWifiApMf );
                                #else
                                        CosaDmlWiFiApMfGetCfg( NULL, pWifiSsid->SSID.StaticInfo.Name, pWifiApMf );
                                #endif
                        }
                }
        }
}

static void wifiFactoryReset(void *frArgs)
{

	ULONG indexes=(ULONG)frArgs;
	ULONG radioIndex   =(indexes>>24) & 0xff;
	ULONG radioIndex_2 =(indexes>>16) & 0xff;
	ULONG apIndex      =(indexes>>8) & 0xff;
	ULONG apIndex_2    =indexes & 0xff;

	CosaDmlWiFi_FactoryResetRadioAndAp(radioIndex,radioIndex_2, apIndex, apIndex_2);
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
    ULONG binSize;
	int nRet=0;
	ULONG radioIndex=0, apIndex=0, radioIndex_2=0, apIndex_2=0;
	ULONG indexes=0;

#ifdef USE_NOTIFY_COMPONENT
	char* p_write_id;
	char* p_new_val;
	char* p_old_val;
	char* p_notify_param_name;
	char* st;
	char* p_val_type;
	UINT value_type,write_id;
	parameterSigStruct_t param = {0};
#endif

    if (!ParamName || !pString)
        return FALSE;

	if( AnscEqualString(ParamName, "X_RDKCENTRAL-COM_WiFi_Notification", TRUE))
	{
#ifdef USE_NOTIFY_COMPONENT
	
			printf(" \n WIFI : Notification Received \n");
			p_notify_param_name = strtok_r(pString, ",", &st);
			p_write_id = strtok_r(NULL, ",", &st);
			p_new_val = strtok_r(NULL, ",", &st);
			p_old_val = strtok_r(NULL, ",", &st);
			p_val_type = strtok_r(NULL, ",", &st);
	
			value_type = atoi(p_val_type);
			write_id = atoi(p_write_id);

			printf(" \n Notification : Parameter Name = %s \n", p_notify_param_name);
			printf(" \n Notification : New Value = %s \n", p_new_val);
			printf(" \n Notification : Old Value = %s \n", p_old_val);
			printf(" \n Notification : Value Type = %d \n", value_type);
			printf(" \n Notification : Component ID = %d \n", write_id);

			param.parameterName = p_notify_param_name;
			param.oldValue = p_old_val;
			param.newValue = p_new_val;
			param.type = value_type;
			param.writeID = write_id;

			WiFiPramValueChangedCB(&param,0,NULL);
#endif
			return TRUE;
		}	 
	
		if( AnscEqualString(ParamName, "X_RDKCENTRAL-COM_Connected-Client", TRUE))
		{
#ifdef USE_NOTIFY_COMPONENT
		
			printf(" \n WIFI : Connected-Client Received \n");
			p_notify_param_name = strtok_r(pString, ",", &st);
			p_write_id = strtok_r(NULL, ",", &st);
			p_new_val = strtok_r(NULL, ",", &st);
			p_old_val = strtok_r(NULL, ",", &st);
	
			printf(" \n Notification : Parameter Name = %s \n", p_notify_param_name);
			printf(" \n Notification : Interface = %s \n", p_write_id);
			printf(" \n Notification : MAC = %s \n", p_new_val);
			printf(" \n Notification : Status = %s \n", p_old_val);
			
#endif
			return TRUE;
		}	 

	if( AnscEqualString(ParamName, "X_CISCO_COM_FactoryResetRadioAndAp", TRUE))
    {
		fprintf(stderr, "-- %s X_CISCO_COM_FactoryResetRadioAndAp %s\n", __func__, pString);	
        if(!pString || strlen(pString)<3 || strchr(pString, ';')==NULL)
			return FALSE;
		if(strchr(pString, ',')) { //1,2;1,3	
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
    pthread_create(&tid, NULL, &wifiFactoryReset, (void*)indexes);
	//	CosaDmlWiFi_FactoryResetRadioAndAp(radioIndex,radioIndex_2, apIndex, apIndex_2);        
        return TRUE;
    }
	
    if( AnscEqualString(ParamName, "X_CISCO_COM_RadioPower", TRUE))
    {
        COSA_DML_WIFI_RADIO_POWER TmpPower; 
        if ( AnscEqualString(pString, "PowerLow", TRUE ) )
        {
            TmpPower = COSA_DML_WIFI_POWER_LOW;
        } else if ( AnscEqualString(pString, "PowerDown", TRUE ) )
        { 
            TmpPower = COSA_DML_WIFI_POWER_DOWN;
        } else if ( AnscEqualString(pString, "PowerUp", TRUE ) )
        { 
            TmpPower = COSA_DML_WIFI_POWER_UP;
        } else {
            return FALSE;
        }

        /* save update to backup */
        CosaDmlWiFi_SetRadioPower(TmpPower);
        
        return TRUE;
    }
	
    if (AnscEqualString(ParamName, "X_CISCO_COM_ConfigFileBase64", TRUE)) {
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

		return TRUE;
	}
	
	if (AnscEqualString(ParamName, "X_RDKCENTRAL-COM_Br0_Sync", TRUE))
    {        
		char buf[128]={0};
		char *pt=NULL;
		
		strncpy(buf, pString, sizeof(buf));		
		if ((pt=strstr(buf,","))) {
			*pt=0;
			CosaDmlWiFiGetBridge0PsmData(buf, pt+1);
		}
        return TRUE;
    }	
	
	return FALSE;	
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
	
	if (AnscEqualString(ParamName, "X_COMCAST_COM_DFSSupport", TRUE))
    {
        *pBool = pWifiRadioFull->Cfg.X_COMCAST_COM_DFSSupport;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_COMCAST_COM_DFSEnable", TRUE))
    {
        /* collect value */
        *pBool = pWifiRadioFull->Cfg.X_COMCAST_COM_DFSEnable;
        
        return TRUE;
    }
	
	if (AnscEqualString(ParamName, "X_COMCAST-COM_DCSSupported", TRUE))
    {
        *pBool = pWifiRadioFull->Cfg.X_COMCAST_COM_DCSSupported;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_COMCAST-COM_DCSEnable", TRUE))
    {
        *pBool = pWifiRadioFull->Cfg.X_COMCAST_COM_DCSEnable;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_COMCAST_COM_IGMPSnoopingEnable", TRUE))
    {
        /* collect value */
        *pBool = pWifiRadioFull->Cfg.X_COMCAST_COM_IGMPSnoopingEnable;
        
        return TRUE;
    }
    
	if( AnscEqualString(ParamName, "X_COMCAST-COM_AutoChannelRefreshPeriodSupported", TRUE))
    {
	
        *pBool = pWifiRadioFull->Cfg.X_COMCAST_COM_AutoChannelRefreshPeriodSupported;
       
        return TRUE;
    }

	if( AnscEqualString(ParamName, "X_COMCAST-COM_IEEE80211hSupported", TRUE))
    {
        *pBool = pWifiRadioFull->Cfg.X_COMCAST_COM_IEEE80211hSupported;
        return TRUE;
    }
	
	if( AnscEqualString(ParamName, "X_COMCAST-COM_ReverseDirectionGrantSupported", TRUE))
    {
        *pBool = pWifiRadioFull->Cfg.X_COMCAST_COM_ReverseDirectionGrantSupported;
        return TRUE;
    }
	
	if( AnscEqualString(ParamName, "X_COMCAST-COM_RtsThresholdSupported", TRUE))
    {
	
        *pBool = pWifiRadioFull->Cfg.X_COMCAST_COM_RtsThresholdSupported;
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
    if (AnscEqualString(ParamName, "X_RDKCENTRAL-COM_AutoChannelRefreshPeriodSupported", TRUE))
    {
        *pBool = pWifiRadioFull->Cfg.AutoChannelRefreshPeriodSupported; 
        return TRUE;
    }

    if (AnscEqualString(ParamName, "X_RDKCENTRAL-COM_RtsThresholdSupported", TRUE))
    {
        *pBool = pWifiRadioFull->Cfg.RtsThresholdSupported; 
        return TRUE;
    }

    if (AnscEqualString(ParamName, "X_RDKCENTRAL-COM_ReverseDirectionGrantSupported", TRUE))
    {
        *pBool = pWifiRadioFull->Cfg.ReverseDirectionGrantSupported; 
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

    if (AnscEqualString(ParamName, "X_CISCO_COM_11nGreenfieldEnabled", TRUE))
    {
        *pBool = pWifiRadioFull->Cfg.X_CISCO_COM_11nGreenfieldEnabled; 
        return TRUE;
    }

    if (AnscEqualString(ParamName, "X_CISCO_COM_WirelessOnOffButton", TRUE))
    {
        *pBool = pWifiRadioFull->Cfg.X_CISCO_COM_WirelessOnOffButton; 
        return TRUE;
    }
    if( AnscEqualString(ParamName, "X_RDKCENTRAL-COM_DCSEnable", TRUE))
    {
   	 *pBool = pWifiRadioFull->Cfg.X_COMCAST_COM_DCSEnable;
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
    if (AnscEqualString(ParamName, "X_CISCO_COM_ApplySettingSSID", TRUE))
    {
        *pInt = pWifiRadioFull->Cfg.ApplySettingSSID; 
        return TRUE;
    }
	if (AnscEqualString(ParamName, "X_COMCAST-COM_CarrierSenseThresholdRange", TRUE))
    {
        CosaDmlWiFi_getRadioCarrierSenseThresholdRange((pWifiRadio->Radio.Cfg.InstanceNumber - 1),pInt);
	 return TRUE;
    }
    if (AnscEqualString(ParamName, "X_COMCAST-COM_CarrierSenseThresholdInUse", TRUE))
    {
        CosaDmlWiFi_getRadioCarrierSenseThresholdInUse((pWifiRadio->Radio.Cfg.InstanceNumber - 1),pInt); 
        return TRUE;
    }
    if (AnscEqualString(ParamName, "X_COMCAST-COM_ChannelSwitchingCount", TRUE))
    {

		*pInt = gChannelSwitchingCount;
        return TRUE;
    }
   if (AnscEqualString(ParamName, "X_RDKCENTRAL-COM_DCSDwelltime", TRUE))
    {
            CosaDmlWiFi_getRadioDcsDwelltime(pWifiRadio->Radio.Cfg.InstanceNumber,pInt);
        return TRUE;
    }
	//if (AnscEqualString(ParamName, "X_RDKCENTRAL-COM_DCSHighChannelUsageThreshold", TRUE))
    //{
	//	CosaDmlWiFi_getRadioDCSHighChannelUsageThreshold(pWifiRadio->Radio.Cfg.InstanceNumber,pInt);
    //    return TRUE;
    //}


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
    

    /* check the parameter name and return the corresponding value */
	if( AnscEqualString(ParamName, "X_COMCAST_COM_RadioUpTime", TRUE))
    {
        /* collect value */
		int TimeInSecs = 0;
        if(ANSC_STATUS_SUCCESS == CosaDmlWiFi_RadioUpTime(&TimeInSecs, (pWifiRadio->Radio.Cfg.InstanceNumber - 1)))	{
			*puLong = (ULONG )TimeInSecs;
		}
        return TRUE;
    }
    if( AnscEqualString(ParamName, "Status", TRUE))
    {
        CosaDmlWiFiRadioGetDinfo((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiRadio->Radio.Cfg.InstanceNumber, &pWifiRadio->Radio.DynamicInfo);
    
        /* collect value */
        *puLong = pWifiRadioFull->DynamicInfo.Status;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "LastChange", TRUE))
    {
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
        CosaDmlWiFiRadioGetDCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, &pWifiRadio->Radio.Cfg);
        *puLong = pWifiRadioFull->Cfg.Channel;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "AutoChannelRefreshPeriod", TRUE))
    {
		//zqiu:  Reason for change: Device.WiFi.Radio.10000.AutoChannelRefreshPeriod parameter is getting updated even after Device.WiFi.Radio.10000.AutoChannelEnable is disabled.
		if(pWifiRadioFull->Cfg.AutoChannelEnable == TRUE)
		{
			/* collect value */
			*puLong = pWifiRadioFull->Cfg.AutoChannelRefreshPeriod;
        }
        return TRUE;
    }

    if( AnscEqualString(ParamName, "OperatingChannelBandwidth", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioFull->Cfg.OperatingChannelBandwidth;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "ExtensionChannel", TRUE))
    {
        /* collect value */
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

    if( AnscEqualString(ParamName, "X_COMCAST-COM_BeaconInterval", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioFull->Cfg.BeaconInterval;
        
        return TRUE;
    }
    if(AnscEqualString(ParamName, "BeaconPeriod", TRUE))
    {
        /* collect value */
        *puLong = pWifiRadioFull->Cfg.BeaconInterval;
        CosaDmlWiFi_getRadioBeaconPeriod((pWifiRadio->Radio.Cfg.InstanceNumber -1), puLong);
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
#if 0
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
 #endif   
    if( AnscEqualString(ParamName, "RadioResetCount", TRUE) )
	{
	    printf(" **** wifi_dml RadioResetCount : Entry **** \n");
		if (CosaDmlWiFi_RadioGetResetCount((pWifiRadio->Radio.Cfg.InstanceNumber -1),puLong) != ANSC_STATUS_SUCCESS)
			return FALSE;

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
        CosaDmlWiFiRadioGetChannelsInUse((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiRadio->Radio.Cfg.InstanceNumber, &pWifiRadio->Radio.DynamicInfo);
        /* collect value */
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

    if( AnscEqualString(ParamName, "X_CISCO_COM_ApChannelScan", TRUE))
    {
        CosaDmlWiFiRadioGetApChannelScan((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiRadio->Radio.Cfg.InstanceNumber, &pWifiRadio->Radio.DynamicInfo);
        /* collect value */
        if ( AnscSizeOfString(pWifiRadioFull->DynamicInfo.ApChannelScan) < *pUlSize)
        {
            AnscCopyString(pValue, pWifiRadioFull->DynamicInfo.ApChannelScan);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiRadioFull->DynamicInfo.ApChannelScan)+1;
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

	if( AnscEqualString(ParamName, "BasicDataTransmitRates", TRUE))
    {
        /* collect value */
        CosaDmlWiFiRadioGetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, &pWifiRadio->Radio.Cfg) ;
        if ( AnscSizeOfString(pWifiRadioFull->Cfg.BasicDataTransmitRates) < *pUlSize)
        {
            AnscCopyString(pValue, pWifiRadioFull->Cfg.BasicDataTransmitRates);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiRadioFull->Cfg.BasicDataTransmitRates)+1;
            return 1;
        }
        return 0;
    }
    	if( AnscEqualString(ParamName, "SupportedDataTransmitRates", TRUE))
    {
        /* collect value */
        if ( AnscSizeOfString(pWifiRadioFull->Cfg.SupportedDataTransmitRates) < *pUlSize)
        {
            AnscCopyString(pValue, pWifiRadioFull->Cfg.SupportedDataTransmitRates);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiRadioFull->Cfg.SupportedDataTransmitRates)+1;
            return 1;
        }
        return 0;
    }
    	if( AnscEqualString(ParamName, "OperationalDataTransmitRates", TRUE))
    {
        /* collect value */
        CosaDmlWiFiRadioGetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, &pWifiRadio->Radio.Cfg) ;
        if ( AnscSizeOfString(pWifiRadioFull->Cfg.OperationalDataTransmitRates) < *pUlSize)
        {
            AnscCopyString(pValue, pWifiRadioFull->Cfg.OperationalDataTransmitRates);
            return 0;
        }
        else
        {
            *pUlSize = AnscSizeOfString(pWifiRadioFull->Cfg.OperationalDataTransmitRates)+1;
            return 1;
        }
        return 0;
    }
    if( AnscEqualString(ParamName, "X_RDKCENTRAL-COM_DCSChannelPool", TRUE)) {
		if ( AnscSizeOfString(pWifiRadioFull->Cfg.DCSChannelPool) < *pUlSize) {
			AnscCopyString(pValue, pWifiRadioFull->Cfg.DCSChannelPool);
			return 0;
		} else {
			*pUlSize = AnscSizeOfString(pWifiRadioFull->Cfg.DCSChannelPool)+1;
			return 1;
		}
	}	

	if( AnscEqualString(ParamName, "X_RDKCENTRAL-COM_DCSChannelScore", TRUE)) {
		/* collect value */
		CosaDmlWiFi_getDCSChanScore(pWifiRadioFull->Cfg.InstanceNumber, pWifiRadioFull->Cfg.DCSChannelScore, *pUlSize);
		if ( AnscSizeOfString(pWifiRadioFull->Cfg.DCSChannelScore) < *pUlSize) {
			AnscCopyString(pValue, pWifiRadioFull->Cfg.DCSChannelScore);
			return 0;
		} else {
			*pUlSize = AnscSizeOfString(pWifiRadioFull->Cfg.DCSChannelScore)+1;
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

    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "Enable", TRUE))
    {
        if ( pWifiRadioFull->Cfg.bEnabled == bValue )
        {
            return  TRUE;
        }

        /* save update to backup */
        pWifiRadioFull->Cfg.bEnabled = bValue;
        pWifiRadio->bRadioChanged = TRUE;

        return TRUE;
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

    if( AnscEqualString(ParamName, "X_COMCAST_COM_DFSEnable", TRUE))
    {
        if ( pWifiRadioFull->Cfg.X_COMCAST_COM_DFSEnable == bValue )
        {
            return  TRUE;
        }

        /* save update to backup */
        pWifiRadioFull->Cfg.X_COMCAST_COM_DFSEnable = bValue;
        pWifiRadio->bRadioChanged = TRUE;

        return TRUE;
    }
	
    if( AnscEqualString(ParamName, "X_COMCAST-COM_DCSEnable", TRUE))
    {
        if ( pWifiRadioFull->Cfg.X_COMCAST_COM_DCSEnable == bValue )
        {
            return  TRUE;
        }

        /* save update to backup */
        pWifiRadioFull->Cfg.X_COMCAST_COM_DCSEnable = bValue;
        pWifiRadio->bRadioChanged = TRUE;

        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_COMCAST_COM_IGMPSnoopingEnable", TRUE))
    {
        if ( pWifiRadioFull->Cfg.X_COMCAST_COM_IGMPSnoopingEnable == bValue )
        {
            return  TRUE;
        }

        /* save update to backup */
        pWifiRadioFull->Cfg.X_COMCAST_COM_IGMPSnoopingEnable = bValue;
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

    if( AnscEqualString(ParamName, "X_CISCO_COM_11nGreenfieldEnabled", TRUE))
    {
        if ( pWifiRadioFull->Cfg.X_CISCO_COM_11nGreenfieldEnabled == bValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.X_CISCO_COM_11nGreenfieldEnabled = bValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_WirelessOnOffButton", TRUE))
    {
        if ( pWifiRadioFull->Cfg.X_CISCO_COM_WirelessOnOffButton == bValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.X_CISCO_COM_WirelessOnOffButton = bValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }
    if( AnscEqualString(ParamName, "X_RDKCENTRAL-COM_DCSEnable", TRUE))
	{
		if ( pWifiRadioFull->Cfg.X_COMCAST_COM_DCSEnable == bValue )
		{
		    return  TRUE;
		}

		/* save update to backup */
		pWifiRadioFull->Cfg.X_COMCAST_COM_DCSEnable = bValue;
		//pWifiRadio->bRadioChanged = TRUE;
		CosaDmlWiFi_setDCSScan(pWifiRadioFull->Cfg.InstanceNumber,bValue);
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
    if( AnscEqualString(ParamName, "X_CISCO_COM_ApplySettingSSID", TRUE))
    {
        if ( pWifiRadioFull->Cfg.ApplySettingSSID == iValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.ApplySettingSSID = iValue;
        pWifiRadio->bRadioChanged = TRUE;
        
        return TRUE;
    }
    if( AnscEqualString(ParamName, "X_COMCAST-COM_CarrierSenseThresholdInUse", TRUE))
    {         
        CosaDmlWiFi_setRadioCarrierSenseThresholdInUse((pWifiRadio->Radio.Cfg.InstanceNumber - 1),iValue);
        return TRUE;
    }
    if( AnscEqualString(ParamName, "X_RDKCENTRAL-COM_DCSDwelltime", TRUE))
    {

	CosaDmlWiFi_setRadioDcsDwelltime(pWifiRadio->Radio.Cfg.InstanceNumber,iValue);
	return TRUE;
    }
    //if( AnscEqualString(ParamName, "X_RDKCENTRAL-COM_DCSHighChannelUsageThreshold", TRUE))
    //{
	//CosaDmlWiFi_setRadioDCSHighChannelUsageThreshold(pWifiRadio->Radio.Cfg.InstanceNumber,iValue);
	//return TRUE;
    //}

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
    
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "Channel", TRUE))
    {
        if ( pWifiRadioFull->Cfg.Channel == uValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiRadioFull->Cfg.Channel = uValue;
        pWifiRadioFull->Cfg.AutoChannelEnable = FALSE; /* User has manually set a channel */
		pWifiRadio->bRadioChanged = TRUE;
       // pWifiRadioFull->Cfg.ChannelSwitchingCount++;
	   gChannelSwitchingCount++;
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

    if( AnscEqualString(ParamName, "X_COMCAST-COM_BeaconInterval", TRUE))
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
	if( AnscEqualString(ParamName,"BeaconPeriod", TRUE))
    {
		CosaDmlWiFi_setRadioBeaconPeriod((pWifiRadio->Radio.Cfg.InstanceNumber - 1),uValue);
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
#if 0
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
#endif
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

    if( AnscEqualString(ParamName, "OperatingStandards", TRUE))
    {
        ULONG                       TmpOpStd;
        char *a = _ansc_strchr(pString, 'a');
        char *ac = _ansc_strstr(pString, "ac");
        
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
        if ( AnscCharInString(pString, 'b') )
        {
            TmpOpStd |= COSA_DML_WIFI_STD_b;
        }
        if ( AnscCharInString(pString, 'g') )
        {
            TmpOpStd |= COSA_DML_WIFI_STD_g;
        }
        if ( AnscCharInString(pString, 'n') )
        {
            TmpOpStd |= COSA_DML_WIFI_STD_n;
        }

        if ( pWifiRadioFull->Cfg.OperatingStandards == TmpOpStd )
        {
            return  TRUE;
        }
        
        pWifiRadioFull->Cfg.OperatingStandards = TmpOpStd;

	if(pWifiRadioFull->Cfg.OperatingStandards == (COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n) ) {

		isBeaconRateUpdate[0] = isBeaconRateUpdate[2] = isBeaconRateUpdate[4] =  isBeaconRateUpdate[6] = isBeaconRateUpdate[8] = isBeaconRateUpdate[10] = TRUE;	
		CosaWifiAdjustBeaconRate(1, "6Mbps");
		CcspTraceWarning(("WIFI OperatingStandards = g/n  Beacon Rate 6Mbps  Function %s \n",__FUNCTION__));
	}
	else if (pWifiRadioFull->Cfg.OperatingStandards == (COSA_DML_WIFI_STD_b |COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n) ) {

		isBeaconRateUpdate[0] = isBeaconRateUpdate[2] = isBeaconRateUpdate[4] =  isBeaconRateUpdate[6] = isBeaconRateUpdate[8] = isBeaconRateUpdate[10] = TRUE;	
		CosaWifiAdjustBeaconRate(1, "1Mbps");
		CcspTraceWarning(("WIFI OperatingStandards = b/g/n  Beacon Rate 1Mbps %s \n",__FUNCTION__));
	}

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

	if(AnscEqualString(ParamName, "BasicDataTransmitRates", TRUE))
    {
		if ( AnscEqualString(pWifiRadioFull->Cfg.BasicDataTransmitRates, pString, TRUE) )
        {
            return  TRUE;
        }
         
        /* save update to backup */
        AnscCopyString( pWifiRadioFull->Cfg.BasicDataTransmitRates, pString );
        pWifiRadio->bRadioChanged = TRUE;
        return TRUE;
    }
    	if(AnscEqualString(ParamName, "OperationalDataTransmitRates", TRUE))
    {
		if ( AnscEqualString(pWifiRadioFull->Cfg.OperationalDataTransmitRates, pString, TRUE) )
        {
            return  TRUE;
        }
         
        /* save update to backup */
        AnscCopyString( pWifiRadioFull->Cfg.OperationalDataTransmitRates, pString );
        pWifiRadio->bRadioChanged = TRUE;
        return TRUE;
    }
	
    if(AnscEqualString(ParamName, "X_RDKCENTRAL-COM_DCSChannelPool", TRUE)) {	
		if ( AnscEqualString(pWifiRadioFull->Cfg.DCSChannelPool, pString, TRUE) )
			return  TRUE;

		/* save update to backup */
		AnscCopyString( pWifiRadioFull->Cfg.DCSChannelPool, pString );
		pWifiRadio->bRadioChanged = TRUE;
		CosaDmlWiFi_setDCSChanPool(pWifiRadioFull->Cfg.InstanceNumber, pString) ;
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
	PCOSA_DML_WIFI_RADIO_STATS      pWifiStats 	   = &pWifiRadio->Stats;
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

    if (!(CosaUtilChannelValidate(pWifiRadioFull->Cfg.OperatingFrequencyBand,pWifiRadioFull->Cfg.Channel,pWifiRadioFull->StaticInfo.PossibleChannels))) {
        CcspTraceWarning(("********Radio Validate:Failed Channel\n"));
        AnscCopyString(pReturnParamName, "Channel");
        *puLength = AnscSizeOfString("Channel");
        return FALSE;
    }

    if ( (pWifiRadioFull->StaticInfo.SupportedStandards & pWifiRadioFull->Cfg.OperatingStandards) !=  pWifiRadioFull->Cfg.OperatingStandards) {
        CcspTraceWarning(("********Radio Validate:Failed OperatingStandards\n"));
        AnscCopyString(pReturnParamName, "OperatingStandards");
        *puLength = AnscSizeOfString("OperatingStandards");
        return FALSE;
    }

    // If the Channel Bandwidth is 80 or 160 MHz then the radio must support 11ac
    if ( (    (pWifiRadioFull->Cfg.OperatingChannelBandwidth == COSA_DML_WIFI_CHAN_BW_80M)
           || (pWifiRadioFull->Cfg.OperatingChannelBandwidth == COSA_DML_WIFI_CHAN_BW_160M)) &&
         !(pWifiRadioFull->StaticInfo.SupportedStandards & COSA_DML_WIFI_STD_ac) ) {
        CcspTraceWarning(("********Radio Validate:Failed OperatingChannelBandwidth\n"));
        AnscCopyString(pReturnParamName, "OperatingChannelBandwidth");
        *puLength = AnscSizeOfString("OperatingChannelBandwidth");
        return FALSE;
    }

    // If the Channel is 165 then Channel Bandwidth must be 20 MHz, otherwise reject the change
    if ((pWifiRadioFull->Cfg.AutoChannelEnable == FALSE) &&
        (pWifiRadioFull->Cfg.Channel == 165) && 
         !(pWifiRadioFull->Cfg.OperatingChannelBandwidth == COSA_DML_WIFI_CHAN_BW_20M) ) {
        CcspTraceWarning(("********Radio Validate:Failed Channel and OperatingChannelBandwidth mismatch\n"));
        AnscCopyString(pReturnParamName, "Channel");
        *puLength = AnscSizeOfString("Channel");
        return FALSE;
    }
    
    if( (pWifiRadioFull->Cfg.TransmitPower != 12) &&  
        (pWifiRadioFull->Cfg.TransmitPower != 25) && 
        (pWifiRadioFull->Cfg.TransmitPower != 50) && 
        (pWifiRadioFull->Cfg.TransmitPower != 75) && 
        (pWifiRadioFull->Cfg.TransmitPower != 100) )
    {
         CcspTraceWarning(("********Radio Validate:Failed Transmit Power\n"));
         AnscCopyString(pReturnParamName, "TransmitPower");
         *puLength = AnscSizeOfString("TransmitPower");
         return FALSE;
    }    

    if( (pWifiRadioFull->Cfg.BeaconInterval < 0) || (pWifiRadioFull->Cfg.BeaconInterval > 65535) )
    {
         CcspTraceWarning(("********Radio Validate:Failed BeaconInterval\n"));
         AnscCopyString(pReturnParamName, "BeaconInterval");
         *puLength = AnscSizeOfString("BeaconInterval");
         return FALSE;
    }

    if( (pWifiRadioFull->Cfg.DTIMInterval < 0) || (pWifiRadioFull->Cfg.DTIMInterval > 255) )
    {
         CcspTraceWarning(("********Radio Validate:Failed DTIMInterval\n"));
         AnscCopyString(pReturnParamName, "DTIMInterval");
         *puLength = AnscSizeOfString("DTIMInterval");
         return FALSE;
    }

// need to fix temporary in order for the set to work
// Value of 0 == off
    if( (pWifiRadioFull->Cfg.FragmentationThreshold > 2346) )
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
	
	if(pWifiStats->RadioStatisticsMeasuringInterval==0 || pWifiStats->RadioStatisticsMeasuringInterval<=pWifiStats->RadioStatisticsMeasuringRate)
	{
		CcspTraceWarning(("********Radio Validate:Failed RadioStatisticsMeasuringInterval \n"));
        AnscCopyString(pReturnParamName, "RadioStatisticsMeasuringInterval");
        *puLength = AnscSizeOfString("RadioStatisticsMeasuringInterval");
		return FALSE;
	}
	
	if(pWifiStats->RadioStatisticsMeasuringRate==0 || (pWifiStats->RadioStatisticsMeasuringInterval%pWifiStats->RadioStatisticsMeasuringRate)!=0)
	{
		CcspTraceWarning(("********Radio Validate:Failed RadioStatisticsMeasuringRate \n"));
        AnscCopyString(pReturnParamName, "RadioStatisticsMeasuringRate");
        *puLength = AnscSizeOfString("RadioStatisticsMeasuringRate");
		return FALSE;
	}
	if((pWifiStats->RadioStatisticsMeasuringInterval/pWifiStats->RadioStatisticsMeasuringRate)>=RSL_MAX)
	{
		CcspTraceWarning(("********Radio Validate:Failed RadioStatisticsMeasuringInterval is too big \n"));
        AnscCopyString(pReturnParamName, "RadioStatisticsMeasuringInterval");
        *puLength = AnscSizeOfString("RadioStatisticsMeasuringInterval");
		return FALSE;
	}
	;

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
	ANSC_STATUS                     returnStatus    = ANSC_STATUS_SUCCESS;
#if defined(_ENABLE_BAND_STEERING_)
    PCOSA_DML_WIFI_BANDSTEERING	 pBandSteering = pMyObject->pBandSteering;
    BOOL radio_0_Enabled=FALSE;
    BOOL radio_1_Enabled=FALSE;
    BOOL ret=FALSE;
#endif
    if ( !pWifiRadio->bRadioChanged )
    {
        return  ANSC_STATUS_SUCCESS;
    }
    else
    {
        pWifiRadio->bRadioChanged = FALSE;
        CcspTraceInfo(("WiFi Radio -- apply the change...\n"));
    }
    
    returnStatus = CosaDmlWiFiRadioSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiRadioCfg);
#if defined(_ENABLE_BAND_STEERING_)
    wifi_getRadioEnable(1, &radio_1_Enabled);
    wifi_getRadioEnable(0, &radio_0_Enabled);
    ret= radio_1_Enabled & radio_0_Enabled;
    if(!ret && NULL !=pBandSteering && NULL != &(pBandSteering->BSOption)) 
    {	
	if(pBandSteering->BSOption.bEnable)
	{
		pBandSteering->BSOption.bLastBSDisableForRadio=true; 
		pBandSteering->BSOption.bEnable=false;
		CosaDmlWiFi_SetBandSteeringOptions( &pBandSteering->BSOption );
    	}	
    }
    if(ret && pBandSteering->BSOption.bLastBSDisableForRadio && NULL !=pBandSteering &&  NULL != &(pBandSteering->BSOption)) 
    {
	pBandSteering->BSOption.bLastBSDisableForRadio=false;
	pBandSteering->BSOption.bEnable=true;
	CosaDmlWiFi_SetBandSteeringOptions( &pBandSteering->BSOption );
    }
#endif

    if (returnStatus == ANSC_STATUS_SUCCESS && isHotspotSSIDIpdated)
    {

	pthread_t tid;
   	pthread_create(&tid, NULL, &UpdateCircuitId, NULL);
	isHotspotSSIDIpdated = FALSE;
    }
    return returnStatus; 
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

ULONG
ReceivedSignalLevel_GetEntryCount
    (
        ANSC_HANDLE                 hInsContext
    )
{
	PCOSA_DML_WIFI_RADIO            pWifiRadio      = hInsContext;
	PCOSA_DML_WIFI_RADIO_STATS      pWifiStats = &pWifiRadio->Stats;
	PCOSA_DML_WIFI_RADIO_STATS_RSL	pRsl = &pWifiStats->RslInfo;
	if(pRsl->Count==0) {
		if(pWifiStats->RadioStatisticsMeasuringRate==0)
			pWifiStats->RadioStatisticsMeasuringRate=30;
		if(pWifiStats->RadioStatisticsMeasuringInterval==0)
			pWifiStats->RadioStatisticsMeasuringInterval=1800;
		pRsl->Count=pWifiStats->RadioStatisticsMeasuringInterval/pWifiStats->RadioStatisticsMeasuringRate;
	}
	return pRsl->Count;
}

ANSC_HANDLE
ReceivedSignalLevel_GetEntry
    (
        ANSC_HANDLE                 hInsContext,
        ULONG                       nIndex,
        ULONG*                      pInsNumber
    )
{
	PCOSA_DML_WIFI_RADIO            pWifiRadio      = hInsContext;
	PCOSA_DML_WIFI_RADIO_STATS      pWifiStats = &pWifiRadio->Stats;
	PCOSA_DML_WIFI_RADIO_STATS_RSL	pRsl = &pWifiStats->RslInfo;

	if (nIndex > pRsl->Count)   {
        return NULL;
    } else {
        *pInsNumber  = nIndex + 1; 
        return pRsl->ReceivedSignalLevel+((pRsl->StartIndex+pRsl->Count-nIndex)%pRsl->Count);
    }
    return NULL; 
}


BOOL
ReceivedSignalLevel_GetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int*                        pInt
    )
{
	INT	*pReceivedSignalLevel   = (INT*)hInsContext;    
	
	if(!hInsContext)
		return FALSE;
	
	if( AnscEqualString(ParamName, "ReceivedSignalLevel", TRUE))   {
		*pInt = *pReceivedSignalLevel;
        return TRUE;
    }
	return FALSE;		
}

/***********************************************************************

 APIs for Object:

    WiFi.Radio.{i}.Stats.

    *  Stats3_SetParamBoolValue
    *  Stats3_GetParamIntValue
    *  Stats3_GetParamUlongValue
    *  Stats3_GetParamStringValue

***********************************************************************/
/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Stats3_SetParamBoolValue
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
Stats3_IsUpdated
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject       = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_RADIO            pWifiRadio      = hInsContext;
    PCOSA_DML_WIFI_RADIO_STATS      pWifiRadioStats = &pWifiRadio->Stats;

	if ( ( AnscGetTickInSeconds() - pWifiRadioStats->LastSampling ) < pWifiRadioStats->RadioStatisticsMeasuringRate )
		return FALSE;
	else {
    	pWifiRadioStats->LastSampling =  AnscGetTickInSeconds();
    	return TRUE;
	}	
	return TRUE;
}

ULONG
Stats3_Synchronize
    (
        ANSC_HANDLE                 hInsContext
    )
{
	//PCOSA_DATAMODEL_WIFI            pMyObject       = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_RADIO            pWifiRadio      = hInsContext;
    PCOSA_DML_WIFI_RADIO_STATS      pWifiRadioStats = &pWifiRadio->Stats;	
	PCOSA_DML_WIFI_RADIO_STATS_RSL	pRsl = &pWifiRadioStats->RslInfo;
	INT								iRSL=0;
	ULONG							uIndex=0;	
		
	CosaDmlWiFi_getRadioStatsReceivedSignalLevel(pWifiRadio->Radio.Cfg.InstanceNumber, &iRSL);
	uIndex=(pRsl->StartIndex+1)%pRsl->Count;
	pRsl->ReceivedSignalLevel[uIndex]=iRSL;
	pRsl->StartIndex=uIndex;
	
	//zqiu: TODO: other noise statistic
	
	return ANSC_STATUS_SUCCESS;
}

BOOL
Stats3_GetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL                        *pBool
    )
{
    //PCOSA_DATAMODEL_WIFI            pMyObject       = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_RADIO            pWifiRadio      = hInsContext;
    PCOSA_DML_WIFI_RADIO_STATS      pWifiRadioStats = &pWifiRadio->Stats;
        
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "X_COMCAST-COM_RadioStatisticsEnable", TRUE))    {
		*pBool = pWifiRadioStats->RadioStatisticsEnable;
         return TRUE;
    }	
    return FALSE;
}

BOOL
Stats3_SetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL                        bValue
    )
{
	PCOSA_DATAMODEL_WIFI            pMyObject       = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
	PCOSA_DML_WIFI_RADIO            pWifiRadio      = hInsContext;
	PCOSA_DML_WIFI_RADIO_FULL       pWifiRadioFull = &pWifiRadio->Radio;
    PCOSA_DML_WIFI_RADIO_CFG        pWifiRadioCfg  = &pWifiRadioFull->Cfg;	
    PCOSA_DML_WIFI_RADIO_STATS      pWifiRadioStats = &pWifiRadio->Stats;	
    if( AnscEqualString(ParamName, "X_COMCAST-COM_RadioStatisticsEnable", TRUE))   {
		if(pWifiRadioStats->RadioStatisticsEnable!=bValue) {
			//apply changes		
			pWifiRadioStats->RadioStatisticsEnable=bValue;
			CosaDmlWiFiRadioStatsSet(pWifiRadioCfg->InstanceNumber, pWifiRadioStats);	
		}			
        return TRUE;
    }
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
    PCOSA_DATAMODEL_WIFI            pMyObject       = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_RADIO            pWifiRadio      = hInsContext;
    PCOSA_DML_WIFI_RADIO_STATS      pWifiRadioStats = &pWifiRadio->Stats;
    
    CosaDmlWiFiRadioGetStats((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiRadio->Radio.Cfg.InstanceNumber, pWifiRadioStats);

    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "X_COMCAST-COM_NoiseFloor", TRUE))    {
		*pInt = pWifiRadioStats->NoiseFloor;
         return TRUE;
    }
	if( AnscEqualString(ParamName, "X_COMCAST-COM_ActivityFactor", TRUE))    {
		*pInt = pWifiRadioStats->ActivityFactor;
        return TRUE;
    }
	if( AnscEqualString(ParamName, "X_COMCAST-COM_CarrierSenseThreshold_Exceeded", TRUE))    {
        *pInt = pWifiRadioStats->CarrierSenseThreshold_Exceeded;
        return TRUE;
    }
	if( AnscEqualString(ParamName, "X_COMCAST-COM_RetransmissionMetric", TRUE))     {
        *pInt = pWifiRadioStats->RetransmissionMetric;
        return TRUE;
    }
	if( AnscEqualString(ParamName, "X_COMCAST-COM_MaximumNoiseFloorOnChannel", TRUE))    {
        *pInt = pWifiRadioStats->MaximumNoiseFloorOnChannel;
        return TRUE;
    }
	if( AnscEqualString(ParamName, "X_COMCAST-COM_MinimumNoiseFloorOnChannel", TRUE))   {
        *pInt = pWifiRadioStats->MinimumNoiseFloorOnChannel;
        return TRUE;
    }
	if( AnscEqualString(ParamName, "X_COMCAST-COM_MedianNoiseFloorOnChannel", TRUE))    {
        *pInt = pWifiRadioStats->MedianNoiseFloorOnChannel;
        return TRUE;
    }
	if( AnscEqualString(ParamName, "X_COMCAST-COM_RadioStatisticsMeasuringRate", TRUE)) {
        //CosaDmlWiFi_getRadioStatsRadioStatisticsMeasuringRate(pWifiRadio->Radio.Cfg.InstanceNumber ,pInt);
		*pInt = pWifiRadioStats->RadioStatisticsMeasuringRate;
        return TRUE;
    }
	if( AnscEqualString(ParamName, "X_COMCAST-COM_RadioStatisticsMeasuringInterval", TRUE))    {
        //CosaDmlWiFi_getRadioStatsRadioStatisticsMeasuringInterval(pWifiRadio->Radio.Cfg.InstanceNumber,pInt);
		*pInt = pWifiRadioStats->RadioStatisticsMeasuringInterval;
        return TRUE;
    }
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
    if( AnscEqualString(ParamName, "BytesSent", TRUE))   {
        *puLong = pWifiRadioStats->BytesSent;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "BytesReceived", TRUE))    {
        *puLong = pWifiRadioStats->BytesReceived;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "PacketsSent", TRUE))    {
        *puLong = pWifiRadioStats->PacketsSent;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "PacketsReceived", TRUE))    {
        *puLong = pWifiRadioStats->PacketsReceived;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "ErrorsSent", TRUE))    {
        *puLong = pWifiRadioStats->ErrorsSent;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "ErrorsReceived", TRUE))    {
        *puLong = pWifiRadioStats->ErrorsReceived;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "DiscardPacketsSent", TRUE))    {
        *puLong = pWifiRadioStats->DiscardPacketsSent;
        return TRUE;
    }
    if( AnscEqualString(ParamName, "DiscardPacketsReceived", TRUE))    {
        *puLong = pWifiRadioStats->DiscardPacketsReceived;
        return TRUE;
    }  
	if( AnscEqualString(ParamName, "PLCPErrorCount", TRUE))    {
        *puLong = pWifiRadioStats->PLCPErrorCount;
        return TRUE;
    }
	if( AnscEqualString(ParamName, "FCSErrorCount", TRUE))    {
        *puLong = pWifiRadioStats->FCSErrorCount;
        return TRUE;
    }
	if( AnscEqualString(ParamName, "InvalidMACCount", TRUE))    {
        *puLong = pWifiRadioStats->InvalidMACCount;
        return TRUE;
    }
	if( AnscEqualString(ParamName, "PacketsOtherReceived", TRUE))    {
        *puLong = pWifiRadioStats->PacketsOtherReceived;
        return TRUE;
    }
	if( AnscEqualString(ParamName, "X_COMCAST-COM_ChannelUtilization", TRUE))    {
        *puLong = pWifiRadioStats->ChannelUtilization;
        return TRUE;
    }
	if( AnscEqualString(ParamName, "X_COMCAST-COM_StatisticsStartTime", TRUE))    {
        *puLong = pWifiRadioStats->StatisticsStartTime;
        return TRUE;
    }

    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        Stats3_SetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       ParamName,
                int                        iValue
            );

    description:

        This function is called to set integer parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       ParamName,
                The parameter name;

                int                        iValue
                The buffer of returned integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
Stats3_SetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int                         iValue
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject       = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_RADIO            pWifiRadio      = hInsContext;
	PCOSA_DML_WIFI_RADIO_STATS      pWifiRadioStats = &pWifiRadio->Stats;
 
	if( AnscEqualString(ParamName, "X_COMCAST-COM_RadioStatisticsMeasuringRate", TRUE))   {
		pWifiRadioStats->RadioStatisticsMeasuringRate=iValue;
		CosaDmlWiFi_setRadioStatsRadioStatisticsMeasuringRate(pWifiRadio->Radio.Cfg.InstanceNumber, pWifiRadioStats->RadioStatisticsMeasuringRate);
	    return TRUE;
    }
	if( AnscEqualString(ParamName, "X_COMCAST-COM_RadioStatisticsMeasuringInterval", TRUE))    {
		pWifiRadioStats->RadioStatisticsMeasuringInterval=iValue;	
		CosaDmlWiFi_setRadioStatsRadioStatisticsMeasuringInterval(pWifiRadio->Radio.Cfg.InstanceNumber, pWifiRadioStats->RadioStatisticsMeasuringInterval);
        return TRUE;
    }
	return FALSE;
}

BOOL
Stats3_Validate
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pReturnParamName,
        ULONG*                      puLength
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject      = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_RADIO            pWifiRadio     = hInsContext;
	PCOSA_DML_WIFI_RADIO_STATS      pWifiStats 	   = &pWifiRadio->Stats;
    	
	if(pWifiStats->RadioStatisticsMeasuringInterval==0 || pWifiStats->RadioStatisticsMeasuringInterval<=pWifiStats->RadioStatisticsMeasuringRate)
	{
		CcspTraceWarning(("********Radio Validate:Failed RadioStatisticsMeasuringInterval \n"));
        AnscCopyString(pReturnParamName, "RadioStatisticsMeasuringInterval");
        *puLength = AnscSizeOfString("RadioStatisticsMeasuringInterval");
		return FALSE;
	}
	
	if(pWifiStats->RadioStatisticsMeasuringRate==0 || (pWifiStats->RadioStatisticsMeasuringInterval%pWifiStats->RadioStatisticsMeasuringRate)!=0)
	{
		CcspTraceWarning(("********Radio Validate:Failed RadioStatisticsMeasuringRate \n"));
        AnscCopyString(pReturnParamName, "RadioStatisticsMeasuringRate");
        *puLength = AnscSizeOfString("RadioStatisticsMeasuringRate");
		return FALSE;
	}
	if((pWifiStats->RadioStatisticsMeasuringInterval/pWifiStats->RadioStatisticsMeasuringRate)>=RSL_MAX)
	{
		CcspTraceWarning(("********Radio Validate:Failed RadioStatisticsMeasuringInterval is too big \n"));
        AnscCopyString(pReturnParamName, "RadioStatisticsMeasuringInterval");
        *puLength = AnscSizeOfString("RadioStatisticsMeasuringInterval");
		return FALSE;
	}

    return TRUE;
}

ULONG
Stats3_Commit
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject      = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_RADIO            pWifiRadio     = hInsContext;
	PCOSA_DML_WIFI_RADIO_STATS      pWifiStats 	   = &pWifiRadio->Stats;
	
	//CosaDmlWiFi_setRadioStatsRadioStatisticsMeasuringRate(pWifiRadio->Radio.Cfg.InstanceNumber, pWifiStats->RadioStatisticsMeasuringRate);
	//CosaDmlWiFi_setRadioStatsRadioStatisticsMeasuringInterval(pWifiRadio->Radio.Cfg.InstanceNumber, pWifiStats->RadioStatisticsMeasuringInterval);
	//CosaDmlWiFi_resetRadioStats(pWifiStats);
    return ANSC_STATUS_SUCCESS; 
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

    if ( WIFI_INDEX_MAX < pMyObject->ulSsidNextInstance)
       {
           return NULL;
       }

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

    /*RDKB-6905, CID-33430, null check before use*/
    if(!pMyObject || !pLinkObj)
    {
        AnscTraceError(("%s: null object passed !\n", __FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }

    pWifiSsid    = (PCOSA_DML_WIFI_SSID      )pLinkObj->hContext;
    if(pWifiSsid)
    {
        AnscTraceError(("%s: null pWifiSsid passed !\n", __FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }

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
        if(pAPLinkObj)
        {
            pWifiAp      = pAPLinkObj->hContext;

            if (pWifiAp && AnscEqualString(pWifiAp->AP.Cfg.SSID, PathName, TRUE))
            {
                memset(pWifiAp->AP.Cfg.SSID, 0, sizeof(pWifiAp->AP.Cfg.SSID));

                pAPLinkObj->bNew = TRUE;

                CosaWifiRegAddAPInfo((ANSC_HANDLE)pMyObject, (ANSC_HANDLE)pAPLinkObj);
            }
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
        /* collect value */
        *pBool = pWifiSsid->SSID.Cfg.bEnabled;

        return TRUE;
    }
    
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "X_CISCO_COM_EnableOnline", TRUE))
    {
        /* collect value */
        *pBool = pWifiSsid->SSID.Cfg.EnableOnline;

        return TRUE;
    }

    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "X_CISCO_COM_RouterEnabled", TRUE))
    {
        /* collect value */
        *pBool = pWifiSsid->SSID.Cfg.RouterEnabled;

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
        *puLong  = AnscGetTimeIntervalInSeconds(pWifiSsid->SSID.Cfg.LastChange, AnscGetTickInSeconds());
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
        if ( ( AnscSizeOfString(pWifiSsid->SSID.Cfg.SSID) < *pUlSize) &&
             ( AnscSizeOfString("OutOfService") < *pUlSize) )
        {
            //zqiu: R5401
            AnscCopyString(pValue, pWifiSsid->SSID.Cfg.SSID);
            if ( (pWifiSsid->SSID.Cfg.InstanceNumber == 5) || (pWifiSsid->SSID.Cfg.InstanceNumber == 6) || 
	         (pWifiSsid->SSID.Cfg.InstanceNumber == 9) || (pWifiSsid->SSID.Cfg.InstanceNumber == 10) ) {
                if ( ( IsSsidHotspot(pWifiSsid->SSID.Cfg.InstanceNumber) == TRUE ) && ( pWifiSsid->SSID.Cfg.bEnabled == FALSE ) ) {
	            AnscCopyString(pValue, "OutOfService");
		    return 0;
                }
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

    if( AnscEqualString(ParamName, "X_COMCAST-COM_DefaultSSID", TRUE))
    {
	AnscCopyString(pValue, pWifiSsid->SSID.Cfg.DefaultSSID);
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
    
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "Enable", TRUE))
    {
        if ( pWifiSsid->SSID.Cfg.bEnabled == bValue )
        {
            return  TRUE;
        }

        /* save update to backup */
        pWifiSsid->SSID.Cfg.bEnabled = bValue;
        pWifiSsid->bSsidChanged = TRUE; 
        return TRUE;
    }

    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "X_CISCO_COM_EnableOnline", TRUE))
    {
        if ( pWifiSsid->SSID.Cfg.EnableOnline == bValue )
        {
            return  TRUE;
        }

        /* save update to backup */
        pWifiSsid->SSID.Cfg.EnableOnline = bValue;
        pWifiSsid->bSsidChanged = TRUE; 
        return TRUE;
    }

    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "X_CISCO_COM_RouterEnabled", TRUE))
    {
        if ( pWifiSsid->SSID.Cfg.RouterEnabled == bValue )
        {
            return  TRUE;
        }

        /* save update to backup */
        pWifiSsid->SSID.Cfg.RouterEnabled = bValue;
        pWifiSsid->bSsidChanged = TRUE; 
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
	
	if ( (pWifiSsid->SSID.Cfg.InstanceNumber == 1) || (pWifiSsid->SSID.Cfg.InstanceNumber == 2) )
	{

	        if ( AnscEqualString(pWifiSsid->SSID.Cfg.DefaultSSID, pString, TRUE) )
	        {
        	    return  FALSE;
        	}

	}        
        /* save update to backup */
        AnscCopyString( pWifiSsid->SSID.Cfg.SSID, pString );
        pWifiSsid->bSsidChanged = TRUE; 
        return TRUE;
    }


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
  
    /*Alias should be non-empty*/
    if (AnscSizeOfString(pWifiSsid->SSID.Cfg.Alias) == 0)
    {
        AnscCopyString(pReturnParamName, "Alias");
        *puLength = AnscSizeOfString("Alias");
        return FALSE;
    }

    /* Lower Layers has to be non-empty */
    if (AnscSizeOfString(pWifiSsid->SSID.Cfg.WiFiRadioName) == 0)
    {
        AnscCopyString(pReturnParamName, "LowerLayers");
        *puLength = AnscSizeOfString("LowerLayers");
        return FALSE;
    }
    /* SSID should be non-empty */
    if (AnscSizeOfString(pWifiSsid->SSID.Cfg.SSID) == 0)
    {
        AnscCopyString(pReturnParamName, "SSID");
        *puLength = AnscSizeOfString("SSID");
        return FALSE;
    }
    // "alphabet, digit, underscore, hyphen and dot" 
/*    if (CosaDmlWiFiSsidValidateSSID() == TRUE) {
        if ( isValidSSID(pWifiSsid->SSID.Cfg.SSID) == false ) {
            // Reset to current value because snmp request will not rollback on invalid values 
            AnscTraceError(("SSID '%s' is invalid.  \n", pWifiSsid->SSID.Cfg.SSID));
            CosaDmlWiFiSsidGetSSID(pWifiSsid->SSID.Cfg.InstanceNumber, pWifiSsid->SSID.Cfg.SSID);
            AnscTraceError(("SSID is treated as a special case and will be rolled back even for snmp to old value '%s' \n", pWifiSsid->SSID.Cfg.SSID));
            AnscCopyString(pReturnParamName, "SSID");
            *puLength = AnscSizeOfString("SSID");
            return FALSE;
        }
    }*/
	//Commenting below validation as this should be done at interface level -RDKB-5203
	/*
    if (!IsSsidHotspot(pWifiSsid->SSID.Cfg.InstanceNumber))
    {
        if (strcasestr(pWifiSsid->SSID.Cfg.SSID, "xfinityWiFi")
                || strcasestr(pWifiSsid->SSID.Cfg.SSID, "xfinity")
                || strcasestr(pWifiSsid->SSID.Cfg.SSID, "CableWiFi"))
        {
            AnscTraceError(("SSID '%s' contains preserved name for Hotspot\n", pWifiSsid->SSID.Cfg.SSID));
            CosaDmlWiFiSsidGetSSID(pWifiSsid->SSID.Cfg.InstanceNumber, pWifiSsid->SSID.Cfg.SSID);
            AnscTraceError(("SSID is treated as a special case and will be rolled back even for snmp to old value '%s' \n", pWifiSsid->SSID.Cfg.SSID));
            AnscCopyString(pReturnParamName, "SSID");
            *puLength = AnscSizeOfString("SSID");
            return FALSE;
        }
    }
    */
 
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

#if !defined(_COSA_INTEL_USG_ATOM_) && !defined(_COSA_BCM_MIPS_) && !defined(_COSA_BCM_ARM_)
        if (AnscEqualString(pWifiSsid->SSID.Cfg.SSID, pWifiSsid2->SSID.Cfg.SSID, TRUE))
        {
            AnscCopyString(pReturnParamName, "SSID");

            *puLength = AnscSizeOfString("SSID");
            return FALSE;
        }
#else
        if (AnscEqualString(pWifiSsid->SSID.StaticInfo.Name, pWifiSsid2->SSID.StaticInfo.Name, TRUE))
        {
        
            AnscCopyString(pReturnParamName, "Name");

            *puLength = AnscSizeOfString("Name");
            return FALSE;
        }
#endif
    }

    if ( (pWifiSsid->SSID.Cfg.bEnabled == TRUE) && 
         IsSsidHotspot(pWifiSsid->SSID.Cfg.InstanceNumber) && 
         AnscEqualString(pWifiSsid->SSID.Cfg.SSID, "OutOfService", FALSE) /* case insensitive */)
    {
        AnscCopyString(pReturnParamName, "SSID");

        *puLength = AnscSizeOfString("SSID");

        fprintf(stderr, "%s: Cannot Enable HHS, SSID is %s\n", 
                __FUNCTION__, pWifiSsid->SSID.Cfg.SSID);

        return FALSE;
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

    if( AnscEqualString(ParamName, "RetransCount", TRUE))
    {
        /* collect value */
        *puLong = pWifiSsidStats->RetransCount;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "FailedRetransCount", TRUE))
    {
       /* collect value */
        *puLong = pWifiSsidStats->FailedRetransCount;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "RetryCount", TRUE))
    {
        /* collect value */
       *puLong = pWifiSsidStats->RetryCount;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "MultipleRetryCount", TRUE))
    {
        /* collect value */
        *puLong = pWifiSsidStats->MultipleRetryCount;
        return TRUE;
    }
    

    if( AnscEqualString(ParamName, "ACKFailureCount", TRUE))
    {
        /* collect value */
        *puLong = pWifiSsidStats->ACKFailureCount;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "AggregatedPacketCount", TRUE))
    {
        /* collect value */
        *puLong = pWifiSsidStats->AggregatedPacketCount;
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
        
#if !defined(_COSA_INTEL_USG_ATOM_) && !defined(_COSA_BCM_MIPS_) && !defined(_COSA_BCM_ARM_)
        CosaDmlWiFiApSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->AP.Cfg);
#else
        CosaDmlWiFiApSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->AP.Cfg);
#endif
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
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "IsolationEnable", TRUE))
    {
        /* collect value */
        *pBool = pWifiAp->AP.Cfg.IsolationEnable;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "Enable", TRUE))
    {
        /* collect value */
        *pBool = pWifiAp->AP.Cfg.bEnabled;
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

    if( AnscEqualString(ParamName, "X_CISCO_COM_KickAssocDevices", TRUE))
    {
        /* collect value */
        *pBool = pWifiAp->AP.Cfg.KickAssocDevices;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_COMCAST-COM_InterworkingServiceCapability", TRUE))
    {
        /* collect value */
        *pBool = pWifiAp->AP.Cfg.InterworkingCapability;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_COMCAST-COM_InterworkingServiceEnable", TRUE))
    {
        /* collect value */
        *pBool = pWifiAp->AP.Cfg.InterworkingEnable;
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
        if( AnscEqualString(ParamName, "X_RDKCENTRAL-COM_ManagementFramePowerControl", TRUE))
    {
        *pInt = pWifiAp->AP.Cfg.ManagementFramePowerControl;
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
#if !defined(_COSA_INTEL_USG_ATOM_) && !defined(_COSA_BCM_MIPS_) && !defined(_COSA_BCM_ARM_)
        CosaDmlWiFiApGetInfo((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->AP.Info);
#else
        CosaDmlWiFiApGetInfo((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->AP.Info);
        CosaDmlWiFiApAssociatedDevicesHighWatermarkGetVal((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->AP.Cfg);
#endif
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

    if (AnscEqualString(ParamName, "X_CISCO_COM_LongRetryLimit", TRUE))
    {
        *puLong = pWifiAp->AP.Cfg.LongRetryLimit; 
        return TRUE;
    }
  
    if (AnscEqualString(ParamName, "MaxAssociatedDevices", TRUE))
    {
        *puLong = pWifiAp->AP.Cfg.MaxAssociatedDevices; 
        return TRUE;
    }

    if (AnscEqualString(ParamName, "X_COMCAST-COM_AssociatedDevicesHighWatermarkThreshold", TRUE))
    {
        *puLong = pWifiAp->AP.Cfg.HighWatermarkThreshold; 
        return TRUE;
    }

    if (AnscEqualString(ParamName, "X_COMCAST-COM_AssociatedDevicesHighWatermarkThresholdReached", TRUE))
    {
        *puLong = pWifiAp->AP.Cfg.HighWatermarkThresholdReached; 
        return TRUE;
    }

    if (AnscEqualString(ParamName, "X_COMCAST-COM_AssociatedDevicesHighWatermark", TRUE))
    {
        *puLong = pWifiAp->AP.Cfg.HighWatermark; 
        return TRUE;
    }
	
	//zqiu
    if( AnscEqualString(ParamName, "X_COMCAST-COM_AssociatedDevicesHighWatermarkDate", TRUE))
    {
		//TODO: need cacultion for the time
		*puLong = AnscGetTickInSeconds();
        //CosaDmlGetHighWatermarkDate(NULL,pWifiSsid->SSID.StaticInfo.Name,pValue);
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
    PCOSA_DATAMODEL_WIFI            pMyObject    = (PCOSA_DATAMODEL_WIFI     )g_pCosaBEManager->hWifi;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj      = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp       = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_AP_MF_CFG        pWifiApMf    = (PCOSA_DML_WIFI_APWPS_FULL)&pWifiAp->MF;

    PSINGLE_LINK_ENTRY              pSLinkEntry  = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pSSIDLinkObj = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid    = (PCOSA_DML_WIFI_SSID      )NULL;
    UCHAR                           PathName[64] = {0};

    pSLinkEntry = AnscQueueGetFirstEntry(&pMyObject->SsidQueue);
    INT wlanIndex;

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

	//zqiu
    //if( AnscEqualString(ParamName, "X_COMCAST-COM_AssociatedDevicesHighWatermarkDate", TRUE))
    //{
    //    CosaDmlGetHighWatermarkDate(NULL,pWifiSsid->SSID.StaticInfo.Name,pValue);
    //    return 0;
    //}
	if( AnscEqualString(ParamName, "X_RDKCENTRAL-COM_BeaconRate", TRUE))
    {
		wlanIndex = pWifiAp->AP.Cfg.InstanceNumber -1 ;

		if(isBeaconRateUpdate[wlanIndex] == TRUE) {
			CosaDmlWiFiGetApBeaconRate(wlanIndex, pWifiAp->AP.Cfg.BeaconRate );
			AnscCopyString(pValue, pWifiAp->AP.Cfg.BeaconRate);
			isBeaconRateUpdate[wlanIndex] = FALSE;
			CcspTraceWarning(("WIFI   wlanIndex  %d  getBeaconRate %s  Function %s \n",wlanIndex,pWifiAp->AP.Cfg.BeaconRate,__FUNCTION__)); 
			return 0;

		}
		else {
				/* collect value */
				if ( AnscSizeOfString(pWifiAp->AP.Cfg.BeaconRate) < *pUlSize)
				{
					AnscCopyString(pValue, pWifiAp->AP.Cfg.BeaconRate);
					return 0;
				}
				else
				{
					*pUlSize = AnscSizeOfString(pWifiAp->AP.Cfg.BeaconRate)+1;
					return 1;
				}
		}
		return 0;
    }

    if( AnscEqualString(ParamName, "X_COMCAST-COM_MAC_FilteringMode", TRUE))
    {

	if ( pWifiApMf->bEnabled == TRUE )
	{
		if ( pWifiApMf->FilterAsBlackList == TRUE )
		{	
			AnscCopyString(pWifiAp->AP.Cfg.MacFilterMode,"Deny");
		}
		else
		{
			AnscCopyString(pWifiAp->AP.Cfg.MacFilterMode,"Allow");
		}
	}
	else
	{
		AnscCopyString(pWifiAp->AP.Cfg.MacFilterMode,"Allow-ALL");
		
	}
        /* collect value */
       
	AnscCopyString(pValue, pWifiAp->AP.Cfg.MacFilterMode);
        return 0;

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
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "IsolationEnable", TRUE))
    {
        if ( pWifiAp->AP.Cfg.IsolationEnable == bValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiAp->AP.Cfg.IsolationEnable = bValue;
        pWifiAp->bApChanged = TRUE;
        
        return TRUE;
    }

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
    if( AnscEqualString(ParamName, "X_CISCO_COM_KickAssocDevices", TRUE))
    {
        if ( pWifiAp->AP.Cfg.KickAssocDevices == bValue )
        {
            return  TRUE;
        }
        /* save update to backup */
        pWifiAp->AP.Cfg.KickAssocDevices = bValue;
        pWifiAp->bApChanged = TRUE;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_COMCAST-COM_InterworkingServiceEnable", TRUE))
    {
        if ( pWifiAp->AP.Cfg.InterworkingEnable == bValue )
        {
            return  TRUE;
        }
        /* save update to backup */
        pWifiAp->AP.Cfg.InterworkingEnable = bValue;
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
        if( AnscEqualString(ParamName, "X_RDKCENTRAL-COM_ManagementFramePowerControl", TRUE))
    {
        if ( pWifiAp->AP.Cfg.ManagementFramePowerControl == iValue )
        {
            return  TRUE;
        }
        /* save update to backup */
        pWifiAp->AP.Cfg.ManagementFramePowerControl = iValue;
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

    if( AnscEqualString(ParamName, "X_CISCO_COM_LongRetryLimit", TRUE))
    {
        if ( pWifiAp->AP.Cfg.LongRetryLimit == uValue )
        {
            return  TRUE;
        }
        
        /* save update to backup */
        pWifiAp->AP.Cfg.LongRetryLimit = uValue; 
        pWifiAp->bApChanged = TRUE;
        
        return TRUE;
    }

    if( AnscEqualString(ParamName, "MaxAssociatedDevices", TRUE))
    {

        if ( pWifiAp->AP.Cfg.MaxAssociatedDevices == uValue )
        {
            return  TRUE;
        }

        /* save update to backup */
        pWifiAp->AP.Cfg.MaxAssociatedDevices = uValue;
        pWifiAp->bApChanged = TRUE;

        return TRUE;
    }

    if( AnscEqualString(ParamName, "X_COMCAST-COM_AssociatedDevicesHighWatermarkThreshold", TRUE))
    {

        if ( pWifiAp->AP.Cfg.HighWatermarkThreshold == uValue )
        {
            return  TRUE;
        }

        if ( uValue <= pWifiAp->AP.Cfg.MaxAssociatedDevices )
	{
        /* save update to backup */
        pWifiAp->AP.Cfg.HighWatermarkThreshold = uValue;
        pWifiAp->bApChanged = TRUE;
	
        return TRUE;
	}

	else
		return FALSE;

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
	
	if( AnscEqualString(ParamName, "X_RDKCENTRAL-COM_BeaconRate", TRUE))
    {
        /* save update to backup */
        AnscCopyString(pWifiAp->AP.Cfg.BeaconRate, pString);
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

        return FALSE;
    }
 
    /* Retry Limit should be between 0 and 7 */
    if((pWifiAp->AP.Cfg.RetryLimit < 0) || (pWifiAp->AP.Cfg.RetryLimit > 255))
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
             /* ManagementFramePowerControl Parameter values higher than 0 shall be converted to value of 0 and Parameter values lower than -20 shall be converted to value of -20 */
    if(pWifiAp->AP.Cfg.ManagementFramePowerControl > 0)
    {
 	pWifiAp->AP.Cfg.ManagementFramePowerControl = 0;
    }
    else if (pWifiAp->AP.Cfg.ManagementFramePowerControl < -20)
    {
    	pWifiAp->AP.Cfg.ManagementFramePowerControl = -20;
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
    return FALSE;
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
#if !defined(_COSA_INTEL_USG_ATOM_) && !defined(_COSA_BCM_MIPS_) && !defined(_COSA_BCM_ARM_) 
        returnStatus = CosaDmlWiFiApSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->AP.Cfg);
#else
        returnStatus = CosaDmlWiFiApSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->AP.Cfg);
#endif

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
        
#if !defined(_COSA_INTEL_USG_ATOM_) && !defined(_COSA_BCM_MIPS_) && !defined(_COSA_BCM_ARM_)
        CosaDmlWiFiApGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->AP);
#else
        CosaDmlWiFiApGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->AP);
#endif
        
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
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_APSEC_FULL       pWifiApSec   = (PCOSA_DML_WIFI_APSEC_FULL)&pWifiAp->SEC;

    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "Reset", TRUE))
    {
		/* 
		* The value of this parameter is not part of the device configuration and is 
		* always false when read. 
		*/
		*pBool = pWifiApSec->Cfg.bReset;

        return TRUE;
    }

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

    if( AnscEqualString(ParamName, "RadiusServerPort", TRUE))
    {
        /* collect value */
        *puLong = pWifiApSec->Cfg.RadiusServerPort;
        return TRUE;
    }
	
	if( AnscEqualString(ParamName, "SecondaryRadiusServerPort", TRUE))
    {
        /* collect value */
        *puLong = pWifiApSec->Cfg.SecondaryRadiusServerPort;
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
   
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "ModesSupported", TRUE))
    {
        /* collect value */
        char buf[512] = {0};

#ifndef _XB6_PRODUCT_REQ_
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
#else
		if (pWifiApSec->Info.ModesSupported & COSA_DML_WIFI_SECURITY_None )
        {
            strcat(buf, "None");
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
#endif
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

        if ( 20 < *pUlSize)
        {
#ifndef _XB6_PRODUCT_REQ_
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
#else
			if (pWifiApSec->Cfg.ModeEnabled & COSA_DML_WIFI_SECURITY_None )
            {
                AnscCopyString(pValue, "None");
            }
			else if (pWifiApSec->Cfg.ModeEnabled & COSA_DML_WIFI_SECURITY_WPA2_Personal )
            {
                AnscCopyString(pValue, "WPA2-Personal");
            }
			else if (pWifiApSec->Cfg.ModeEnabled & COSA_DML_WIFI_SECURITY_WPA2_Enterprise )
            {
                AnscCopyString(pValue, "WPA2-Enterprise");
            }
#endif
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
#if 0
    if( AnscEqualString(ParamName, "KeyPassphrase", TRUE))
    {
#ifdef _COSA_SIM_
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

#endif

    if( AnscEqualString(ParamName, "X_COMCAST-COM_DefaultKeyPassphrase", TRUE))
    {
            AnscCopyString(pValue, pWifiApSec->Cfg.DefaultKeyPassphrase);
            return 0;
    }

    if( AnscEqualString(ParamName, "X_CISCO_COM_WEPKey", TRUE) || AnscEqualString(ParamName, "X_COMCAST-COM_WEPKey", TRUE))
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

    if(AnscEqualString(ParamName, "KeyPassphrase", TRUE) || AnscEqualString(ParamName, "X_COMCAST-COM_KeyPassphrase", TRUE))
    {
        /* collect value */
        if ( AnscSizeOfString(pWifiApSec->Cfg.KeyPassphrase) > 0 ) 
        {
            if  ( AnscSizeOfString(pWifiApSec->Cfg.KeyPassphrase) < *pUlSize) 
	    {
		AnscCopyString(pValue, pWifiApSec->Cfg.KeyPassphrase);
		return 0;
	    }
	    else
	    {
		*pUlSize = AnscSizeOfString(pWifiApSec->Cfg.KeyPassphrase)+1;
		return 1;
	    }
        } else if ( AnscSizeOfString(pWifiApSec->Cfg.PreSharedKey) > 0 )   
        {
            if  ( AnscSizeOfString(pWifiApSec->Cfg.PreSharedKey) < *pUlSize) 
            {   
                AnscCopyString(pValue, pWifiApSec->Cfg.PreSharedKey);
                return 0;
            }   
            else
            {   
                *pUlSize = AnscSizeOfString(pWifiApSec->Cfg.PreSharedKey)+1;
                return 1;
            }   
        } else  {
            // if both PreSharedKey and KeyPassphrase are NULL, set to NULL string
	    AnscCopyString(pValue, "");
	    return 0;
        }
    }

    if( AnscEqualString(ParamName, "RadiusSecret", TRUE))
    {
        /* Radius Secret should always return empty string when read */
        AnscCopyString(pValue, "");
    }
	if( AnscEqualString(ParamName, "SecondaryRadiusSecret", TRUE))
    {
        /* Radius Secret should always return empty string when read */
        AnscCopyString(pValue, "");
    }

    if( AnscEqualString(ParamName, "RadiusServerIPAddr", TRUE))
    {
        /* Radius Secret should always return empty string when read */
        AnscCopyString(pValue, pWifiApSec->Cfg.RadiusServerIPAddr);
    }
	if( AnscEqualString(ParamName, "SecondaryRadiusServerIPAddr", TRUE))
    {
        /* Radius Secret should always return empty string when read */
        AnscCopyString(pValue, pWifiApSec->Cfg.SecondaryRadiusServerIPAddr);
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
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_APSEC_FULL       pWifiApSec   = (PCOSA_DML_WIFI_APSEC_FULL)&pWifiAp->SEC;
	
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "Reset", TRUE))
    {
		/* To set changes made flag */
		pWifiApSec->Cfg.bReset	 = bValue;

		if ( TRUE == pWifiApSec->Cfg.bReset )
		{
			pWifiAp->bSecChanged	 = TRUE;
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

    if( AnscEqualString(ParamName, "SecondaryRadiusServerPort", TRUE))
    {
        if ( pWifiApSec->Cfg.SecondaryRadiusServerPort != uValue )
        {
            /* save update to backup */
            pWifiApSec->Cfg.SecondaryRadiusServerPort = uValue;
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

    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "ModeEnabled", TRUE))
    {
        COSA_DML_WIFI_SECURITY      TmpMode;
        
#ifndef _XB6_PRODUCT_REQ_
        /* save update to backup */
        if ( AnscEqualString(pString, "None", TRUE) )
        {
            TmpMode = COSA_DML_WIFI_SECURITY_None;
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
        }
        else if ( AnscEqualString(pString, "WPA2-Personal", TRUE) )
        {
            TmpMode  = COSA_DML_WIFI_SECURITY_WPA2_Personal;
        }
        else if ( AnscEqualString(pString, "WPA-WPA2-Personal", TRUE) )
        {
            TmpMode  = COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal;
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
#else
		if ( AnscEqualString(pString, "None", TRUE) )
        {
            TmpMode = COSA_DML_WIFI_SECURITY_None;
        }
		else if ( AnscEqualString(pString, "WPA2-Personal", TRUE) )
        {
            TmpMode  = COSA_DML_WIFI_SECURITY_WPA2_Personal;
        }
		else if ( AnscEqualString(pString, "WPA2-Enterprise", TRUE) )
        {
            TmpMode  = COSA_DML_WIFI_SECURITY_WPA2_Enterprise;
        }
#endif
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
        || AnscEqualString(ParamName, "X_CISCO_COM_WEPKey", TRUE) || AnscEqualString(ParamName, "X_COMCAST-COM_WEPKey", TRUE))
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

    if( AnscEqualString(ParamName, "KeyPassphrase", TRUE) || 
		  ( ( AnscEqualString(ParamName, "X_COMCAST-COM_KeyPassphrase", TRUE)) && ( AnscSizeOfString(pString) >= 8 ) && ( AnscSizeOfString(pString) <= 64) ) )

    {
        if ( AnscEqualString(pString, pWifiApSec->Cfg.KeyPassphrase, TRUE) )
        {
	    return TRUE;
        } 

	if ( (pWifiAp->AP.Cfg.InstanceNumber == 1 ) || (pWifiAp->AP.Cfg.InstanceNumber == 2 ) )
	{

		if ( AnscEqualString(pString, pWifiApSec->Cfg.DefaultKeyPassphrase, TRUE) )
        	{
        	return FALSE;
        	}
	
	}
	/* save update to backup */
	AnscCopyString(pWifiApSec->Cfg.KeyPassphrase, pString );
	//zqiu: reason for change: Change 2.4G wifi password not work for the first time
	AnscCopyString(pWifiApSec->Cfg.PreSharedKey, pWifiApSec->Cfg.KeyPassphrase );
	pWifiAp->bSecChanged = TRUE;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "PreSharedKey", TRUE) ||
        ( ( AnscEqualString(ParamName, "X_COMCAST-COM_KeyPassphrase", TRUE)) &&
          ( AnscSizeOfString(pString) == 64) ) ) 
    {
        if ( AnscEqualString(pString, pWifiApSec->Cfg.PreSharedKey, TRUE) )
        {
	    return TRUE;
        } 

	/* save update to backup */
	AnscCopyString(pWifiApSec->Cfg.PreSharedKey, pString );
	AnscCopyString(pWifiApSec->Cfg.KeyPassphrase, "" );
	pWifiAp->bSecChanged = TRUE;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "RadiusSecret", TRUE))
    {
        if ( AnscEqualString(pString, pWifiApSec->Cfg.RadiusSecret, TRUE) )
            return TRUE;
    
		/* save update to backup */
		AnscCopyString( pWifiApSec->Cfg.RadiusSecret, pString );
		pWifiAp->bSecChanged = TRUE;
        return TRUE;
    }
	
	if( AnscEqualString(ParamName, "SecondaryRadiusSecret", TRUE))
    {
        if ( AnscEqualString(pString, pWifiApSec->Cfg.SecondaryRadiusSecret, TRUE) )
            return TRUE;
    
		/* save update to backup */
		AnscCopyString( pWifiApSec->Cfg.SecondaryRadiusSecret, pString );
		pWifiAp->bSecChanged = TRUE;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "RadiusServerIPAddr", TRUE))
    {
        if ( AnscEqualString(pString, pWifiApSec->Cfg.RadiusServerIPAddr, TRUE) )
			return TRUE;

		/* save update to backup */
		AnscCopyString( pWifiApSec->Cfg.RadiusServerIPAddr, pString );
		pWifiAp->bSecChanged = TRUE;
        return TRUE;
    }
	
	if( AnscEqualString(ParamName, "SecondaryRadiusServerIPAddr", TRUE))
    {
        if ( AnscEqualString(pString, pWifiApSec->Cfg.SecondaryRadiusServerIPAddr, TRUE) )
            return TRUE;
        
		/* save update to backup */
		AnscCopyString( pWifiApSec->Cfg.SecondaryRadiusServerIPAddr, pString );
		pWifiAp->bSecChanged = TRUE;
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
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_APSEC_FULL       pWifiApSec   = (PCOSA_DML_WIFI_APSEC_FULL)&pWifiAp->SEC;

    // If PSK is present it must be a 64 hex string
	//zqiu: reason for change: Change 2.4G wifi password not work for the first time
    /*if ( (AnscSizeOfString(pWifiApSec->Cfg.PreSharedKey) > 0) && 
         (!isHex(pWifiApSec->Cfg.PreSharedKey) ||  (AnscSizeOfString(pWifiApSec->Cfg.PreSharedKey) != 64) ) ) {
        AnscCopyString(pReturnParamName, "PreSharedKey");
        *puLength = AnscSizeOfString("PreSharedKey");
        return FALSE;
    }*/

    // WPA is not allowed by itself.  Only in mixed mode WPA/WPA2
    if (pWifiApSec->Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_Enterprise ||
        pWifiApSec->Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_Personal) {
        AnscCopyString(pReturnParamName, "X_CISCO_COM_EncryptionMethod");
        *puLength = AnscSizeOfString("X_CISCO_COM_EncryptionMethod");
        return FALSE;
    }
    //zqiu: R5422 >>
    if ( ( (pWifiApSec->Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise) ||
            (pWifiApSec->Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal) ) &&
          (pWifiApSec->Cfg.EncryptionMethod != COSA_DML_WIFI_AP_SEC_AES_TKIP) &&
	  (pWifiApSec->Cfg.EncryptionMethod != COSA_DML_WIFI_AP_SEC_AES )   ) {
        AnscCopyString(pReturnParamName, "X_CISCO_COM_EncryptionMethod");
        *puLength = AnscSizeOfString("X_CISCO_COM_EncryptionMethod");
        return FALSE;
    }

    //if ( ( pWifiApSec->Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Enterprise ||
    //        pWifiApSec->Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Personal ) &&
    //     (pWifiApSec->Cfg.EncryptionMethod != COSA_DML_WIFI_AP_SEC_AES ) ) {
    //    AnscCopyString(pReturnParamName, "X_CISCO_COM_EncryptionMethod");
    //    *puLength = AnscSizeOfString("X_CISCO_COM_EncryptionMethod");
    //    return FALSE;
    //}
    //zqiu: R5422 <<

	/* 
	  * If the parameter cannot be set, the CPE MUST reject the request as an invalid 
	  * parameter value. Possible failure reasons include a lack of default values or 
	  * if ModeEnabled is an Enterprise type, i.e. WPA-Enterprise, WPA2-Enterprise or 
	  * WPA-WPA2-Enterprise.
	  */
	if( ( pWifiApSec->Cfg.bReset == TRUE ) && 
		 ( ( COSA_DML_WIFI_SECURITY_WPA_Enterprise == pWifiApSec->Cfg.ModeEnabled ) ||
	       ( COSA_DML_WIFI_SECURITY_WPA2_Enterprise == pWifiApSec->Cfg.ModeEnabled ) ||
		   ( COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise == pWifiApSec->Cfg.ModeEnabled )
   	     )
	  )
	{
        AnscCopyString(pReturnParamName, "Reset");
        *puLength = AnscSizeOfString("Reset");
        return FALSE;
	}

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
        
#if !defined(_COSA_INTEL_USG_ATOM_) && !defined(_COSA_BCM_MIPS_) && !defined(_COSA_BCM_ARM_)
        return CosaDmlWiFiApSecSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, pWifiApSecCfg);
#else
        return CosaDmlWiFiApSecSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, pWifiApSecCfg);
#endif
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
        
#if !defined(_COSA_INTEL_USG_ATOM_) && !defined(_COSA_BCM_MIPS_) && !defined(_COSA_BCM_ARM_)
        CosaDmlWiFiApSecGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->SEC);
#else
        CosaDmlWiFiApSecGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->SEC);
#endif
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
    
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "Enable", TRUE))
    {
        /* collect value */
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
    
    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "Enable", TRUE))
    {
        /* save update to backup */
        pWifiApWps->Cfg.bEnabled = bValue;
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
        }
        if (_ansc_strstr(pString, "PIN"))
        {
            pWifiApWps->Cfg.ConfigMethodsEnabled = (pWifiApWps->Cfg.ConfigMethodsEnabled | COSA_DML_WIFI_WPS_METHOD_Pin);
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
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;
    PCOSA_DML_WIFI_APWPS_FULL       pWifiApWps   = (PCOSA_DML_WIFI_APWPS_FULL)&pWifiAp->WPS;
    
    // Only one of these may be set at a given time
    if ( ( (pWifiApWps->Cfg.X_CISCO_COM_ActivatePushButton == TRUE) &&
         ( (strlen(pWifiApWps->Cfg.X_CISCO_COM_ClientPin) > 0) ||
           (pWifiApWps->Cfg.X_CISCO_COM_CancelSession == TRUE) ) ) ||
         ( (strlen(pWifiApWps->Cfg.X_CISCO_COM_ClientPin) > 0) &&
           (pWifiApWps->Cfg.X_CISCO_COM_CancelSession == TRUE) ) ) 
    {
	AnscCopyString(pReturnParamName, "X_CISCO_COM_ActivatePushButton");
	*puLength = AnscSizeOfString("X_CISCO_COM_ActivatePushButton");
        return FALSE;
    }

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
#if !defined(_COSA_INTEL_USG_ATOM_) && !defined(_COSA_BCM_MIPS_) && !defined(_COSA_BCM_ARM_)
        return CosaDmlWiFiApWpsSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->WPS.Cfg);
#else
        return CosaDmlWiFiApWpsSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->WPS.Cfg);
#endif
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
#if !defined(_COSA_INTEL_USG_ATOM_) && !defined(_COSA_BCM_MIPS_) && !defined(_COSA_BCM_ARM_)
        CosaDmlWiFiApWpsGetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->WPS.Cfg);
#else
        CosaDmlWiFiApWpsGetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->WPS.Cfg);
#endif
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
#if 0	
    if( AnscEqualString(ParamName, "MACAddress", TRUE))
    {
        /* collect value */
        char maclist[1024] = {'\0'};
        CosaDmlWiFiApMfGetMacList(pWifiApMf->MacAddrList, maclist, pWifiApMf->NumberMacAddrList);
        AnscCopyString(pValue, maclist);
        *pUlSize = AnscSizeOfString(pValue); 

        return 0;
    }
#endif
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
        return TRUE;
    }
    if( AnscEqualString(ParamName, "FilterAsBlackList", TRUE))
    {
         /* save update to backup */
	     pWifiApMf->FilterAsBlackList = bValue;
         return TRUE;
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
    #if 0
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
  #endif
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
#if !defined(_COSA_INTEL_USG_ATOM_) && !defined(_COSA_BCM_MIPS_) && !defined(_COSA_BCM_ARM_)
        return CosaDmlWiFiApMfSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->MF);
#else
        return CosaDmlWiFiApMfSetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->MF);
#endif
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
#if !defined(_COSA_INTEL_USG_ATOM_) && !defined(_COSA_BCM_MIPS_)  && !defined(_COSA_BCM_ARM_)
        CosaDmlWiFiApMfGetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->MF);
#else
        CosaDmlWiFiApMfGetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->MF);
#endif
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

#define WIFI_AssociatedDevice_TIMEOUT   20 /*unit is second*/

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
//	return FALSE;
//<<
    ticket=AnscGetTickInSeconds();
//>>zqiu
//remove WIFI_AssociatedDevice_TIMEOUT restriction
//    if ( ticket < (pWifiAp->AssociatedDevice1PreviousVisitTime + WIFI_AssociatedDevice_TIMEOUT ) )
//	return FALSE;
//    else {
    	pWifiAp->AssociatedDevice1PreviousVisitTime =  ticket;
    	return TRUE;
//    }
//<<
#if 0
    if ( ( AnscGetTickInSeconds() - AssociatedDevice1PreviousVisitTime ) < WIFI_AssociatedDevice_TIMEOUT )
        return FALSE;
    else
    {
        AssociatedDevice1PreviousVisitTime =  AnscGetTickInSeconds();
        return TRUE;
    }
#endif 
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
        pWifiAp->AssocDevices = CosaDmlWiFiApGetAssocDevices
        (
            (ANSC_HANDLE)pMyObject->hPoamWiFiDm, 
#if !defined(_COSA_INTEL_USG_ATOM_) && !defined(_COSA_BCM_MIPS_) && !defined(_COSA_BCM_ARM_)
            pWifiSsid->SSID.Cfg.SSID, 
#else
            pWifiSsid->SSID.StaticInfo.Name, 
#endif
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

static int isHex (char *string)
{
    int len = strlen(string);
    int i = 0;

    for (i = 0; i < len; i++)
    {
        char c = string[i];
	if (isdigit(c)||
	    c=='a'||c=='b'||c=='c'||c=='d'||c=='e'||c=='f' ||
	    c=='A'||c=='B'||c=='C'||c=='D'||c=='E'||c=='F')
	    continue;
	else
	    return 0;
    }
    return 1;
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

    // Only 40 of the 64 is used for key
    // key must be either a alphanumeric string of length 5
    // or a hexstring of length 10
    if ( AnscSizeOfString(pWEPKey64->WEPKey) != 40 / 8 ) 
    {
	if ( !isHex(pWEPKey64->WEPKey) || ( AnscSizeOfString(pWEPKey64->WEPKey) != 40 / 8 * 2 ) ) 
	{
	    AnscCopyString(pReturnParamName, "WEPKey");
	    *puLength = AnscSizeOfString("WEPKey");
	    return FALSE;
	}
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

    // Only 104 of the 128 is used for key
    // key must be either a alphanumeric string of length 5
    // or a hexstring of length 10
    if ( AnscSizeOfString(pWEPKey128->WEPKey) != 104 / 8 ) 
    {
	if ( !isHex(pWEPKey128->WEPKey) || ( AnscSizeOfString(pWEPKey128->WEPKey) != 104 / 8 * 2 ) ) 
	{
	    AnscCopyString(pReturnParamName, "WEPKey");
	    *puLength = AnscSizeOfString("WEPKey");
	    return FALSE;
	}
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

BOOL
RadiusSettings_GetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL*                       pBool
    )
{
    /* check the parameter name and return the corresponding value */
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

    CosaDmlGetApRadiusSettings(NULL,pWifiSsid->SSID.StaticInfo.Name,&pWifiAp->RadiusSetting);
    //PCOSA_DML_WIFI_RadiusSetting  pRadiusSetting   = (PCOSA_DML_WIFI_RadiusSetting)hInsContext;
    /* check the parameter name and return the corresponding value */
    if( AnscEqualString(ParamName, "PMKCaching", TRUE))
    {
        /* collect value */
        *pBool = pWifiAp->RadiusSetting.bPMKCaching;
        return TRUE;
    }
 
    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

BOOL
RadiusSettings_GetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int*                        pInt
    )
{
//    PCOSA_DML_WIFI_RadiusSetting  pRadiusSetting   = (PCOSA_DML_WIFI_RadiusSetting)hInsContext;
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

    /* check the parameter name and return the corresponding value */
    CosaDmlGetApRadiusSettings(NULL,pWifiSsid->SSID.StaticInfo.Name,&pWifiAp->RadiusSetting);

    if( AnscEqualString(ParamName, "RadiusServerRetries", TRUE))
    {
        /* collect value */
        *pInt = pWifiAp->RadiusSetting.iRadiusServerRetries;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "RadiusServerRequestTimeout", TRUE))
    {
        /* collect value */
        *pInt = pWifiAp->RadiusSetting.iRadiusServerRequestTimeout;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "PMKLifetime", TRUE))	
    {
        /* collect value */
        *pInt = pWifiAp->RadiusSetting.iPMKLifetime;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "PMKCacheInterval", TRUE))
    {
        /* collect value */
        *pInt = pWifiAp->RadiusSetting.iPMKCacheInterval;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "MaxAuthenticationAttempts", TRUE))
    {
        /* collect value */
        *pInt = pWifiAp->RadiusSetting.iMaxAuthenticationAttempts;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "BlacklistTableTimeout", TRUE))
    {
        /* collect value */
        *pInt = pWifiAp->RadiusSetting.iBlacklistTableTimeout;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "IdentityRequestRetryInterval", TRUE))
    {
        /* collect value */
        *pInt = pWifiAp->RadiusSetting.iIdentityRequestRetryInterval;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "QuietPeriodAfterFailedAuthentication", TRUE))
    {
        /* collect value */
        *pInt = pWifiAp->RadiusSetting.iQuietPeriodAfterFailedAuthentication;
        return TRUE;
    }

    return FALSE;
}

BOOL
RadiusSettings_SetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL                        bValue
    )
{
//    PCOSA_DML_WIFI_RadiusSetting  pRadiusSetting   = (PCOSA_DML_WIFI_RadiusSetting)hInsContext;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;

    AnscTraceWarning(("ParamName: %s bvalue\n", ParamName, bValue));

    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "PMKCaching", TRUE))
    {
        /* save update to backup */
        pWifiAp->RadiusSetting.bPMKCaching = bValue;
        return TRUE;
    }
return FALSE;
}

BOOL
RadiusSettings_SetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        int                         iValue
    )
{
    //PCOSA_DML_WIFI_RadiusSetting  pRadiusSetting   = (PCOSA_DML_WIFI_RadiusSetting)hInsContext;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)hInsContext;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )pLinkObj->hContext;

    AnscTraceWarning(("ParamName: %s iValue: %d\n", ParamName, iValue));

    /* check the parameter name and set the corresponding value */
    if( AnscEqualString(ParamName, "RadiusServerRetries", TRUE))
    {
        /* save update to backup */
        pWifiAp->RadiusSetting.iRadiusServerRetries = iValue;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "RadiusServerRequestTimeout", TRUE))
    {
        /* save update to backup */
        pWifiAp->RadiusSetting.iRadiusServerRequestTimeout = iValue;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "PMKLifetime", TRUE))
    {
        /* save update to backup */
        pWifiAp->RadiusSetting.iPMKLifetime = iValue;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "PMKCacheInterval", TRUE))
    {
        /* save update to backup */
        pWifiAp->RadiusSetting.iPMKCacheInterval = iValue;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "MaxAuthenticationAttempts", TRUE))
    {
        /* save update to backup */
        pWifiAp->RadiusSetting.iMaxAuthenticationAttempts = iValue;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "BlacklistTableTimeout", TRUE))
    {
        /* save update to backup */
        pWifiAp->RadiusSetting.iBlacklistTableTimeout = iValue;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "IdentityRequestRetryInterval", TRUE))
    {
        /* save update to backup */
        pWifiAp->RadiusSetting.iIdentityRequestRetryInterval = iValue;
        return TRUE;
    }

    if( AnscEqualString(ParamName, "QuietPeriodAfterFailedAuthentication", TRUE))
    {
        /* save update to backup */
        pWifiAp->RadiusSetting.iQuietPeriodAfterFailedAuthentication = iValue;
        return TRUE;
    }
    /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
    return FALSE;
}

BOOL
RadiusSettings_Validate
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pReturnParamName,
        ULONG*                      puLength
    )
{
    return TRUE;
}

ULONG
RadiusSettings_Commit
    (
        ANSC_HANDLE                 hInsContext
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

    CosaDmlSetApRadiusSettings(NULL,pWifiSsid->SSID.StaticInfo.Name,&pWifiAp->RadiusSetting);

    return 0;
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
    CcspTraceWarning(("pMacFilt->InstanceNumber is %lu\n", pMacFilt->InstanceNumber));
    /* Update the middle layer data */
    pSubCosaContext = (PCOSA_CONTEXT_LINK_OBJECT)AnscAllocateMemory(sizeof(COSA_CONTEXT_LINK_OBJECT));
    if ( !pSubCosaContext )
    {
        AnscFreeMemory(pMacFilt);
        return NULL;
    }
    
    pSubCosaContext->InstanceNumber = pWiFiAP->ulMacFilterNextInsNum;
    pWiFiAP->ulMacFilterNextInsNum++;
    CcspTraceWarning(("Next InstanceNumber is %lu\n", pWiFiAP->ulMacFilterNextInsNum));
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
MacFilterTab_Rollback
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
 
	 WiFi.X_RDKCENTRAL-COM_BandSteering.
 
	 *	BandSteering_GetParamBoolValue
	 *	BandSteering_SetParamBoolValue
	 *    BandSteering_GetParamStringValue
	 *	BandSteering_Validate
	 *	BandSteering_Commit
	 *	BandSteering_Rollback
 
 ***********************************************************************/
 /**********************************************************************  
 
	 caller:	 owner of this object 
 
	 prototype: 
 
		 BOOL
		 BandSteering_GetParamBoolValue
			 (
				 ANSC_HANDLE				 hInsContext,
				 char*						 ParamName,
				 BOOL*						 pBool
			 );
 
	 description:
 
		 This function is called to retrieve Boolean parameter value; 
 
	 argument:	 ANSC_HANDLE				 hInsContext,
				 The instance handle;
 
				 char*						 ParamName,
				 The parameter name;
 
				 BOOL*						 pBool
				 The buffer of returned boolean value;
 
	 return:	 TRUE if succeeded.
 
 **********************************************************************/
 BOOL
 BandSteering_GetParamBoolValue
	 (
		 ANSC_HANDLE				 hInsContext,
		 char*						 ParamName,
		 BOOL*						 pBool
	 )
 {
	 PCOSA_DATAMODEL_WIFI			 pMyObject		 = (PCOSA_DATAMODEL_WIFI	 )g_pCosaBEManager->hWifi;
 	 PCOSA_DML_WIFI_BANDSTEERING	 pBandSteering   = pMyObject->pBandSteering;
	 /* check the parameter name and return the corresponding value */
	 if( AnscEqualString(ParamName, "Enable", TRUE))
	 {
		 *pBool = pBandSteering->BSOption.bEnable;
		 return TRUE;
	 }

	 if( AnscEqualString(ParamName, "Capability", TRUE))
	 {
		 *pBool = pBandSteering->BSOption.bCapability;
		 return TRUE;
	 }

	 /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
	 return FALSE;
 }

 /**********************************************************************  
 
	 caller:	 owner of this object 
 
	 prototype: 
 
		 BOOL
		 BandSteering_SetParamBoolValue
			 (
				 ANSC_HANDLE				 hInsContext,
				 char*						 ParamName,
				 BOOL						 bValue
			 );
 
	 description:
 
		 This function is called to set BOOL parameter value; 
 
	 argument:	 ANSC_HANDLE				 hInsContext,
				 The instance handle;
 
				 char*						 ParamName,
				 The parameter name;
 
				 BOOL						 bValue
				 The updated BOOL value;
 
	 return:	 TRUE if succeeded.
 
 **********************************************************************/
 BOOL
 BandSteering_SetParamBoolValue
	 (
		 ANSC_HANDLE				 hInsContext,
		 char*						 ParamName,
		 BOOL						 bValue
	 )
 {
	 PCOSA_DATAMODEL_WIFI			 pMyObject		 = (PCOSA_DATAMODEL_WIFI	 )g_pCosaBEManager->hWifi;
 	 PCOSA_DML_WIFI_BANDSTEERING	 pBandSteering   = pMyObject->pBandSteering;
	 
	 /* check the parameter name and set the corresponding value */
	 if( AnscEqualString(ParamName, "Enable", TRUE))
	 {
		 if( bValue != pBandSteering->BSOption.bEnable )
	 	 {
			 pBandSteering->BSOption.bEnable = bValue;
			 pBandSteering->bBSOptionChanged = TRUE;
	 	 }
		 return TRUE;		 
	 }

	 /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
	 return FALSE;
 }

 /**********************************************************************  
 
	 caller:	 owner of this object 
 
	 prototype: 
 
		 ULONG
		 BandSteering_GetParamStringValue
			 (
				 ANSC_HANDLE				 hInsContext,
				 char*						 ParamName,
				 char*						 pValue,
				 ULONG* 					 pUlSize
			 );
 
	 description:
 
		 This function is called to retrieve string parameter value; 
 
	 argument:	 ANSC_HANDLE				 hInsContext,
				 The instance handle;
 
				 char*						 ParamName,
				 The parameter name;
 
				 char*						 pValue,
				 The string value buffer;
 
				 ULONG* 					 pUlSize
				 The buffer of length of string value;
				 Usually size of 1023 will be used.
				 If it's not big enough, put required size here and return 1;
 
	 return:	 0 if succeeded;
				 1 if short of buffer size; (*pUlSize = required size)
				 -1 if not supported.
 
 **********************************************************************/
 ULONG
 BandSteering_GetParamStringValue
	 (
		 ANSC_HANDLE				 hInsContext,
		 char*						 ParamName,
		 char*						 pValue,
		 ULONG* 					 pUlSize
	 )
 {
	 PCOSA_DATAMODEL_WIFI			 pMyObject		 = (PCOSA_DATAMODEL_WIFI	 )g_pCosaBEManager->hWifi;
 	 PCOSA_DML_WIFI_BANDSTEERING	 pBandSteering   = pMyObject->pBandSteering;
 
	 /* check the parameter name and return the corresponding value */

	 if( AnscEqualString(ParamName, "APGroup", TRUE))
	 {
		/* collect value */
		 if ( ( sizeof(pBandSteering->BSOption.APGroup ) - 1 ) < *pUlSize)
		 {
			 AnscCopyString(pValue, pBandSteering->BSOption.APGroup);

			 return 0;
		 }
		 else
		 {
			 *pUlSize = sizeof(pBandSteering->BSOption.APGroup);
 
			 return 1;
		 }

		 return 0;
	 }

	 if( AnscEqualString(ParamName, "History", TRUE))
	 {
		 /* collect value */
		 if ( ( sizeof(pBandSteering->BSOption.BandHistory ) - 1 ) < *pUlSize)
		 {
			 CosaDmlWiFi_GetBandSteeringLog( pBandSteering->BSOption.BandHistory, 
			 								 sizeof(pBandSteering->BSOption.BandHistory) );

			 AnscCopyString(pValue, pBandSteering->BSOption.BandHistory);

			 return 0;
		 }
		 else
		 {
			 *pUlSize = sizeof(pBandSteering->BSOption.BandHistory);
			 
			 return 1;
		 }
		 
		 return 0;
	 }

	 /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
	 return -1;
 }

 /**********************************************************************  
 
	 caller:	 owner of this object 
 
	 prototype: 
 
		 ULONG
		 BandSteering_SetParamStringValue
			 (
				 ANSC_HANDLE				 hInsContext,
				 char*				         ParamName,
				 char*					 pString,
			 );
 
	 description:
 
		 This function is called to retrieve string parameter value; 
 
	 argument:	 ANSC_HANDLE				 hInsContext,
			 The instance handle;
 
			 char*					 ParamName,
			 The parameter name;
 
			 char*					 pString,
			 The string value buffer;
 
 
	 return:	 TRUE if succeeded.
 
 **********************************************************************/
 BOOL
 BandSteering_SetParamStringValue
	 (
		 ANSC_HANDLE				hInsContext,
		 char*					ParamName,
		 char*					pString
	 )
 {
	 PCOSA_DATAMODEL_WIFI		 pMyObject	= (PCOSA_DATAMODEL_WIFI	 )g_pCosaBEManager->hWifi;
 	 PCOSA_DML_WIFI_BANDSTEERING	 pBandSteering  = pMyObject->pBandSteering;
 
	 /* check the parameter name and return the corresponding value */

	 if( AnscEqualString(ParamName, "APGroup", TRUE))
	 {
		/* save update to backup */
		//AnscCopyString(pBandSteering->BSOption.APGroup, pString);
		strncpy(pBandSteering->BSOption.APGroup, pString, 64);
		pBandSteering->bBSOptionChanged = TRUE;
	        return TRUE;
	 }

	 /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
	 return FALSE;
 }

 /**********************************************************************  
 
	 caller:	 owner of this object 
 
	 prototype: 
 
		 BOOL
		 BandSteering_Validate
			 (
				 ANSC_HANDLE				 hInsContext,
				 char*						 pReturnParamName,
				 ULONG* 					 puLength
			 );
 
	 description:
 
		 This function is called to finally commit all the update.
 
	 argument:	 ANSC_HANDLE				 hInsContext,
				 The instance handle;
 
				 char*						 pReturnParamName,
				 The buffer (128 bytes) of parameter name if there's a validation. 
 
				 ULONG* 					 puLength
				 The output length of the param name. 
 
	 return:	 TRUE if there's no validation.
 
 **********************************************************************/
 BOOL
 BandSteering_Validate
	 (
		 ANSC_HANDLE				 hInsContext,
		 char*						 pReturnParamName,
		 ULONG* 					 puLength
	 )
 {
	 PCOSA_DATAMODEL_WIFI			 pMyObject		 = (PCOSA_DATAMODEL_WIFI	 )g_pCosaBEManager->hWifi;
 	 PCOSA_DML_WIFI_BANDSTEERING	 pBandSteering   = pMyObject->pBandSteering;

	 return TRUE;
 }
 
 /**********************************************************************  
 
	 caller:	 owner of this object 
 
	 prototype: 
 
		 ULONG
		 BandSteering_Commit
			 (
				 ANSC_HANDLE				 hInsContext
			 );
 
	 description:
 
		 This function is called to finally commit all the update.
 
	 argument:	 ANSC_HANDLE				 hInsContext,
				 The instance handle;
 
	 return:	 The status of the operation.
 
 **********************************************************************/
 ULONG
 BandSteering_Commit
	 (
		 ANSC_HANDLE				 hInsContext
	 )
 {
	 PCOSA_DATAMODEL_WIFI			 pMyObject	   = (PCOSA_DATAMODEL_WIFI	   )g_pCosaBEManager->hWifi;
     PCOSA_DML_WIFI_BANDSTEERING	 pBandSteering = pMyObject->pBandSteering;
 
	 /* Set the Band Steering Current Options */
 	 if ( TRUE == pBandSteering->bBSOptionChanged )
 	 {
		 CosaDmlWiFi_SetBandSteeringOptions( &pBandSteering->BSOption );
		 pBandSteering->bBSOptionChanged = FALSE;


 	 }

	 return ANSC_STATUS_SUCCESS;
 }
 
 /**********************************************************************  
 
	 caller:	 owner of this object 
 
	 prototype: 
 
		 ULONG
		 BandSteering_Rollback
			 (
				 ANSC_HANDLE				 hInsContext
			 );
 
	 description:
 
		 This function is called to roll back the update whenever there's a 
		 validation found.
 
	 argument:	 ANSC_HANDLE				 hInsContext,
				 The instance handle;
 
	 return:	 The status of the operation.
 
 **********************************************************************/
 ULONG
 BandSteering_Rollback
	 (
		 ANSC_HANDLE				 hInsContext
	 )
 {
	 PCOSA_DATAMODEL_WIFI			 pMyObject		 = (PCOSA_DATAMODEL_WIFI	 )g_pCosaBEManager->hWifi;
 	 PCOSA_DML_WIFI_BANDSTEERING	 pBandSteering   = pMyObject->pBandSteering;

	 CosaDmlWiFi_GetBandSteeringOptions( &pBandSteering->BSOption );
	 pBandSteering->bBSOptionChanged = FALSE;

	 return ANSC_STATUS_SUCCESS;
 }

 /***********************************************************************
 
  APIs for Object:
 
	 WiFi.X_RDKCENTRAL-COM_BandSteering.BandSetting.{i}.
 
	 *    BandSetting_GetEntryCount
	 *    BandSetting_GetEntry
	 *	BandSetting_GetParamIntValue
	 *	BandSetting_SetParamIntValue
	 *	BandSteering_Validate
	 *	BandSteering_Commit
	 *	BandSteering_Rollback
 
 ***********************************************************************/
 /**********************************************************************  
 
	 caller:	 owner of this object 
 
	 prototype: 
 
		 ULONG
		 BandSetting_GetEntryCount
			 (
				 ANSC_HANDLE				 hInsContext
			 );
 
	 description:
 
		 This function is called to retrieve the count of the table.
 
	 argument:	 ANSC_HANDLE				 hInsContext,
				 The instance handle;
 
	 return:	 The count of the table
 
 **********************************************************************/
 ULONG
 BandSetting_GetEntryCount
	 (
		 ANSC_HANDLE				 hInsContext
	 )
 {
	 PCOSA_DATAMODEL_WIFI			 pMyObject		 = (PCOSA_DATAMODEL_WIFI	 )g_pCosaBEManager->hWifi;
	 PCOSA_DML_WIFI_BANDSTEERING	 pBandSteering	 = pMyObject->pBandSteering;
	 
	 return pBandSteering->RadioCount;
 }

/**********************************************************************  

	caller: 	owner of this object 

	prototype: 

		ANSC_HANDLE
		BandSetting_GetEntry
			(
				ANSC_HANDLE 				hInsContext,
				ULONG						nIndex,
				ULONG*						pInsNumber
			);

	description:

		This function is called to retrieve the entry specified by the index.

	argument:	ANSC_HANDLE 				hInsContext,
				The instance handle;

				ULONG						nIndex,
				The index of this entry;

				ULONG*						pInsNumber
				The output instance number;

	return: 	The handle to identify the entry

**********************************************************************/
ANSC_HANDLE
BandSetting_GetEntry
	(
		ANSC_HANDLE 				hInsContext,
		ULONG						nIndex,
		ULONG*						pInsNumber
	)
{
	PCOSA_DATAMODEL_WIFI			pMyObject		= (PCOSA_DATAMODEL_WIFI 	)g_pCosaBEManager->hWifi;
	PCOSA_DML_WIFI_BANDSTEERING 	pBandSteering	= pMyObject->pBandSteering;
	if( ( NULL != pBandSteering ) && \
		( nIndex < pBandSteering->RadioCount )
	  )
	{
		*pInsNumber = pBandSteering->pBSSettings[ nIndex ].InstanceNumber;
		
		return ( &pBandSteering->pBSSettings[ nIndex ] ); /* return the handle */
	}
	
	return NULL; /* return the NULL for invalid index */
}

/**********************************************************************  
 
	 caller:	 owner of this object 
 
	 prototype: 
 
		 BOOL
		 BandSetting_GetParamIntValue
			 (
				 ANSC_HANDLE				 hInsContext,
				 char*						 ParamName,
				 int*						 pInt
			 );
 
	 description:
 
		 This function is called to retrieve integer parameter value; 
 
	 argument:	 ANSC_HANDLE				 hInsContext,
				 The instance handle;
 
				 char*						 ParamName,
				 The parameter name;
 
				 int*						 pInt
				 The buffer of returned integer value;
 
	 return:	 TRUE if succeeded.
 
 **********************************************************************/
 BOOL
 BandSetting_GetParamIntValue
	 (
		 ANSC_HANDLE				 hInsContext,
		 char*						 ParamName,
		 int*						 pInt
	 )
 {
	PCOSA_DML_WIFI_BANDSTEERING_SETTINGS pBandSteeringSettings	= ( PCOSA_DML_WIFI_BANDSTEERING_SETTINGS )hInsContext;
	if( AnscEqualString(ParamName, "UtilizationThreshold", TRUE))
	{
		 /* collect value */
		 *pInt = pBandSteeringSettings->UtilizationThreshold;
		 
		 return TRUE;
	}
	
	if( AnscEqualString(ParamName, "RSSIThreshold", TRUE))
	{
		 /* collect value */
		 *pInt = pBandSteeringSettings->RSSIThreshold;
		 
		 return TRUE;
	}

	 if( AnscEqualString(ParamName, "PhyRateThreshold", TRUE))
	 {
		  /* collect value */
		  *pInt = pBandSteeringSettings->PhyRateThreshold;
		  
		  return TRUE;
	 }

	 if( AnscEqualString(ParamName, "OverloadInactiveTime", TRUE))
	 {
		  /* collect value */
		  *pInt = pBandSteeringSettings->OverloadInactiveTime;

		  return TRUE;
	 }

	 if( AnscEqualString(ParamName, "IdleInactiveTime", TRUE))
	 {
		  /* collect value */
		  *pInt = pBandSteeringSettings->IdleInactiveTime;

		  return TRUE;
	 }

 	 /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
	 return FALSE;
 }

 /**********************************************************************  
 
	 caller:	 owner of this object 
 
	 prototype: 
 
		 BOOL
		 BandSetting_SetParamIntValue
			 (
				 ANSC_HANDLE				 hInsContext,
				 char*						 ParamName,
				 int						 iValue
			 );
 
	 description:
 
		 This function is called to set integer parameter value; 
 
	 argument:	 ANSC_HANDLE				 hInsContext,
				 The instance handle;
 
				 char*						 ParamName,
				 The parameter name;
 
				 int						 iValue
				 The updated integer value;
 
	 return:	 TRUE if succeeded.
 
 **********************************************************************/
 BOOL
 BandSetting_SetParamIntValue
	 (
		 ANSC_HANDLE				 hInsContext,
		 char*						 ParamName,
		 int						 iValue
	 )
 {
	 PCOSA_DATAMODEL_WIFI			 	  pMyObject		 		 = (PCOSA_DATAMODEL_WIFI	 )g_pCosaBEManager->hWifi;
	 PCOSA_DML_WIFI_BANDSTEERING	 	  pBandSteering	 		 = pMyObject->pBandSteering;
	 PCOSA_DML_WIFI_BANDSTEERING_SETTINGS pBandSteeringSettings  = ( PCOSA_DML_WIFI_BANDSTEERING_SETTINGS )hInsContext;

	 /* check the parameter name and set the corresponding value */
	 if( AnscEqualString(ParamName, "UtilizationThreshold", TRUE))
	 {
		 /* save update to backup */
		 pBandSteeringSettings->UtilizationThreshold = iValue;
		 pBandSteering->bBSSettingsChanged			 = TRUE;
		 
		 return TRUE;
	 }
 
	 if( AnscEqualString(ParamName, "RSSIThreshold", TRUE))
	 {
		 /* save update to backup */
		 pBandSteeringSettings->RSSIThreshold = iValue;
		 pBandSteering->bBSSettingsChanged	  = TRUE;
		 
		 return TRUE;
	 }

	 if( AnscEqualString(ParamName, "PhyRateThreshold", TRUE))
	 {
		 /* save update to backup */
		 pBandSteeringSettings->PhyRateThreshold = iValue;
		 pBandSteering->bBSSettingsChanged		 = TRUE;
		 
		 return TRUE;
	 }

	 if( AnscEqualString(ParamName, "OverloadInactiveTime", TRUE))
	 {
		 /* save update to backup */
		 pBandSteeringSettings->OverloadInactiveTime = iValue;
		 pBandSteering->bBSSettingsChanged	     = TRUE;

		 return TRUE;
	 }

	 if( AnscEqualString(ParamName, "IdleInactiveTime", TRUE))
	 {
		 /* save update to backup */
		 pBandSteeringSettings->IdleInactiveTime = iValue;
		 pBandSteering->bBSSettingsChanged	 = TRUE;

		 return TRUE;
	 }

	 /* CcspTraceWarning(("Unsupported parameter '%s'\n", ParamName)); */
	 return FALSE;
 }

 /**********************************************************************  
 
	 caller:	 owner of this object 
 
	 prototype: 
 
		 BOOL
		 BandSetting_Validate
			 (
				 ANSC_HANDLE				 hInsContext,
				 char*						 pReturnParamName,
				 ULONG* 					 puLength
			 );
 
	 description:
 
		 This function is called to finally commit all the update.
 
	 argument:	 ANSC_HANDLE				 hInsContext,
				 The instance handle;
 
				 char*						 pReturnParamName,
				 The buffer (128 bytes) of parameter name if there's a validation. 
 
				 ULONG* 					 puLength
				 The output length of the param name. 
 
	 return:	 TRUE if there's no validation.
 
 **********************************************************************/
 BOOL
 BandSetting_Validate
	 (
		 ANSC_HANDLE				 hInsContext,
		 char*						 pReturnParamName,
		 ULONG* 					 puLength
	 )
 {
	 PCOSA_DATAMODEL_WIFI			 	  pMyObject		 		= (PCOSA_DATAMODEL_WIFI	 )g_pCosaBEManager->hWifi;
 	 PCOSA_DML_WIFI_BANDSTEERING	 	  pBandSteering   		= pMyObject->pBandSteering;
	 PCOSA_DML_WIFI_BANDSTEERING_SETTINGS pBandSteeringSettings = ( PCOSA_DML_WIFI_BANDSTEERING_SETTINGS )hInsContext;
	 return TRUE;
 }
 
 /**********************************************************************  
 
	 caller:	 owner of this object 
 
	 prototype: 
 
		 ULONG
		 BandSetting_Commit
			 (
				 ANSC_HANDLE				 hInsContext
			 );
 
	 description:
 
		 This function is called to finally commit all the update.
 
	 argument:	 ANSC_HANDLE				 hInsContext,
				 The instance handle;
 
	 return:	 The status of the operation.
 
 **********************************************************************/
 ULONG
 BandSetting_Commit
	 (
		 ANSC_HANDLE				 hInsContext
	 )
 {
	 PCOSA_DATAMODEL_WIFI			 	  pMyObject		 		= (PCOSA_DATAMODEL_WIFI	 )g_pCosaBEManager->hWifi;
 	 PCOSA_DML_WIFI_BANDSTEERING	 	  pBandSteering   		= pMyObject->pBandSteering;
	 PCOSA_DML_WIFI_BANDSTEERING_SETTINGS pBandSteeringSettings = ( PCOSA_DML_WIFI_BANDSTEERING_SETTINGS )hInsContext;
	 /* Set the Band Steering Current Options */
 	 if ( TRUE == pBandSteering->bBSSettingsChanged )
 	 {
		 CosaDmlWiFi_SetBandSteeringSettings( pBandSteeringSettings->InstanceNumber - 1,
		 									  pBandSteeringSettings );
		 
		 pBandSteering->bBSSettingsChanged = FALSE;
 	 }

	 return ANSC_STATUS_SUCCESS;
 }
 
 /**********************************************************************  
 
	 caller:	 owner of this object 
 
	 prototype: 
 
		 ULONG
		 BandSetting_Rollback
			 (
				 ANSC_HANDLE				 hInsContext
			 );
 
	 description:
 
		 This function is called to roll back the update whenever there's a 
		 validation found.
 
	 argument:	 ANSC_HANDLE				 hInsContext,
				 The instance handle;
 
	 return:	 The status of the operation.
 
 **********************************************************************/
 ULONG
 BandSetting_Rollback
	 (
		 ANSC_HANDLE				 hInsContext
	 )
 {
	 PCOSA_DATAMODEL_WIFI			 pMyObject		 = (PCOSA_DATAMODEL_WIFI	 )g_pCosaBEManager->hWifi;
 	 PCOSA_DML_WIFI_BANDSTEERING	 pBandSteering   = pMyObject->pBandSteering;
	 int 							 iLoopCount;
	 /* Load Previous Values for Band Steering Settings */
	 for ( iLoopCount = 0; iLoopCount < pBandSteering->RadioCount ; ++iLoopCount )
	 {
		 memset( &pBandSteering->pBSSettings[ iLoopCount ], 0, sizeof( COSA_DML_WIFI_BANDSTEERING_SETTINGS ) );

		 /* Instance Number Always from 1 */
		 pBandSteering->pBSSettings[ iLoopCount ].InstanceNumber = iLoopCount + 1;
	 
		 CosaDmlWiFi_GetBandSteeringSettings( iLoopCount, 
											   &pBandSteering->pBSSettings[ iLoopCount ] );
	 }

	 pBandSteering->bBSSettingsChanged = FALSE;

	 return ANSC_STATUS_SUCCESS;
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
	PCOSA_DATAMODEL_WIFI	pMyObject	= (PCOSA_DATAMODEL_WIFI	 )g_pCosaBEManager->hWifi;
	PCOSA_DML_WIFI_ATM	 	pATM   = pMyObject->pATM;
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
	PCOSA_DATAMODEL_WIFI	pMyObject	= (PCOSA_DATAMODEL_WIFI	 )g_pCosaBEManager->hWifi;
	PCOSA_DML_WIFI_ATM	 	pATM   = pMyObject->pATM;
    if( AnscEqualString(ParamName, "Enable", TRUE)) {
		CosaDmlWiFi_SetATMEnable(pATM, bValue);
		return TRUE;
	}

	return FALSE;
}

BOOL
ATM_Validate
(
	ANSC_HANDLE				hInsContext,
	char*					pReturnParamName,
	ULONG* 					puLength
)
{
	PCOSA_DATAMODEL_WIFI	pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
	PCOSA_DML_WIFI_ATM	 	pATM   = pMyObject->pATM;

	return TRUE;
}


ULONG
ATM_Commit
(
	ANSC_HANDLE				 hInsContext
)
{
	PCOSA_DATAMODEL_WIFI	pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
	PCOSA_DML_WIFI_ATM	 	pATM   = pMyObject->pATM;

	//CosaDmlWiFi_SetATMEnable(pATM, bValue);
	return ANSC_STATUS_SUCCESS;
}

ULONG
ATM_Rollback
(
	ANSC_HANDLE				 hInsContext
)
{
	PCOSA_DATAMODEL_WIFI	pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
	PCOSA_DML_WIFI_ATM	 	pATM   = pMyObject->pATM;

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
	PCOSA_DML_WIFI_ATM				pATM = pWiFi->pATM;
	
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
	PCOSA_DML_WIFI_ATM				pATM = pWiFi->pATM;
	//PCOSA_DML_WIFI_ATM_APGROUP		pATMApGroup=&pATM->APGroup;
	
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
CcspTraceInfo(("---- %s %s \n", __func__, 	ParamName));
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
	PCOSA_DML_WIFI_ATM_APGROUP	pATMApGroup=(PCOSA_DML_WIFI_ATM_APGROUP)hInsContext;
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
	PCOSA_DML_WIFI_ATM_APGROUP	pATMApGroup=(PCOSA_DML_WIFI_ATM_APGROUP)hInsContext;
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
	PCOSA_DML_WIFI_ATM_APGROUP		pATMApGroup= (PCOSA_DML_WIFI_ATM_APGROUP)hInsContext;
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
	PCOSA_DML_WIFI_ATM_APGROUP		pATMApGroup= (PCOSA_DML_WIFI_ATM_APGROUP)hInsContext;
	PCOSA_DML_WIFI_ATM_APGROUP_STA  pATMApGroupSta=(PCOSA_DML_WIFI_ATM_APGROUP_STA)hInstance;
	int		uInstance=pATMApGroupSta-pATMApGroup->StaList;
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
	if( AnscEqualString(ParamName, "AirTimePercent", TRUE))	{
		
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


