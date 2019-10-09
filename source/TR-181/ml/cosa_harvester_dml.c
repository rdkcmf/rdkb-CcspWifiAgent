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
#include "ccsp_WifiLog_wrapper.h"
#include "ccsp_trace.h"
#include "ccsp_psm_helper.h"
#include "ccsp_custom_logs.h"

extern ANSC_HANDLE bus_handle;
extern char g_Subsystem[32];

extern void* g_pDslhDmlAgent;
extern int gChannelSwitchingCount;

#if 1
#define MIN_INSTANT_REPORT_TIME   1
#define MAX_INSTANT_REPORT_TIME   900

static char *InstWifiClientEnabled              = "eRT.com.cisco.spvtg.ccsp.Device.WiFi.InstWifiClientEnabled";
static char *InstWifiClientReportingPeriod      = "eRT.com.cisco.spvtg.ccsp.Device.WiFi.InstWifiClientReportingPeriod";
static char *InstWifiClientMacAddress           = "eRT.com.cisco.spvtg.ccsp.Device.WiFi.InstWifiClientMacAddress";
static char *InstWifiClientDefReportingPeriod   = "eRT.com.cisco.spvtg.ccsp.Device.WiFi.InstWifiClientDefReportingPeriod";

ANSC_STATUS GetNVRamULONGConfiguration(char* setting, ULONG* value)
{
    char *strValue = NULL;
    int retPsmGet = CCSP_SUCCESS;

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, setting, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        *value = _ansc_atoi(strValue);
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }
    return retPsmGet;
}

ANSC_STATUS SetNVRamULONGConfiguration(char* setting, ULONG value)
{
    int retPsmSet = CCSP_SUCCESS;
    char psmValue[32] = {};

    sprintf(psmValue,"%d",value);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, setting, ccsp_string, psmValue);
    return retPsmSet;
}


ANSC_STATUS
CosaDmlHarvesterInit
    (
        ANSC_HANDLE                 hThisObject
    )
{
    ANSC_STATUS                     returnStatus        = ANSC_STATUS_SUCCESS;
    int retPsmGet = CCSP_SUCCESS;
    ULONG psmValue = 0;
    char recName[256];
    char *macAddr = NULL;
    char *schemaId = NULL;

    PCOSA_DML_WIFI_HARVESTER    pHarvester = (PCOSA_DML_WIFI_HARVESTER)hThisObject;
    if (!pHarvester)
    {
        CcspTraceWarning(("%s-%d : NULL param\n" , __FUNCTION__, __LINE__ ));
        return ANSC_STATUS_FAILURE;
    }

    retPsmGet = GetNVRamULONGConfiguration(InstWifiClientReportingPeriod, &psmValue);
    if (retPsmGet == CCSP_SUCCESS) {
        pHarvester->uINSTClientReportingPeriod = psmValue;
        pHarvester->bINSTClientReportingPeriodChanged = FALSE;
        SetINSTReportingPeriod(pHarvester->uINSTClientReportingPeriod);
    }

    retPsmGet = GetNVRamULONGConfiguration(InstWifiClientDefReportingPeriod, &psmValue);
    if (retPsmGet == CCSP_SUCCESS) {
        pHarvester->uINSTClientDefReportingPeriod = psmValue;
        pHarvester->bINSTClientDefReportingPeriodChanged = FALSE;
        SetINSTDefReportingPeriod(pHarvester->uINSTClientDefReportingPeriod);
    }

    pHarvester->uINSTClientDefOverrideTTL = 0;
    pHarvester->bINSTClientDefOverrideTTLChanged = FALSE;
    SetINSTOverrideTTL(pHarvester->uINSTClientDefOverrideTTL);


    memset(recName, 0, sizeof(recName));
    sprintf(recName, "%s", InstWifiClientMacAddress);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &macAddr);
    if ((retPsmGet == CCSP_SUCCESS) && (macAddr))
    {
        strncpy(pHarvester->MacAddress, macAddr, MIN_MAC_LEN);
       ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(macAddr);
        
       SetINSTMacAddress(pHarvester->MacAddress);
    }

    retPsmGet = GetNVRamULONGConfiguration(InstWifiClientEnabled, &psmValue);
    if (retPsmGet == CCSP_SUCCESS) {
        pHarvester->bINSTClientEnabled = psmValue;
        CosaDmlWiFiClient_InstantMeasurementsEnable(pHarvester); 
    }

    return returnStatus;
}

/***********************************************************************

 APIs for Object:

    WiFi.X_RDKCENTRAL-COM_Report.WifiClient.

    *  WifiClient_GetParamBoolValue
    *  WifiClient_GetParamUlongValue
    *  WifiClient_GetParamStringValue
    *  WifiClient_SetParamBoolValue
    *  WifiClient_SetParamUlongValue
    *  WifiClient_SetParamStringValue
    *  WifiClient_Validate
    *  WifiClient_Commit
    *  WifiClient_Rollback

***********************************************************************/
BOOL
WifiClient_GetParamBoolValue
(
    ANSC_HANDLE                 hInsContext,
    char*                       ParamName,
    BOOL*                       pBool
)
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if ( AnscEqualString(ParamName, "Enabled", TRUE))
    {
        *pBool    =  CosaDmlWiFi_IsInstantMeasurementsEnable();  
        //*pBool    =  pHarvester->bINSTClientEnabled;
        return TRUE;
    }
    return FALSE;
}

BOOL
WifiClient_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG*                      puLong
    )
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if ( AnscEqualString(ParamName, "ReportingPeriod", TRUE))
    {
        *puLong =  pHarvester->uINSTClientReportingPeriod;
        return TRUE;
    }

    return FALSE;
}

ULONG
WifiClient_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;
    char *buffer;

    if( AnscEqualString(ParamName, "MacAddress", TRUE))
    {
        AnscCopyString(pValue, pHarvester->MacAddress);
        return 0;
    }

   if( AnscEqualString(ParamName, "Schema", TRUE))
    {
#if 1
        AnscCopyString(pValue, "WifiSingleClient.avsc");
#else
        /* collect value */
        ReadInstAssocDevSchemaBuffer();
        buffer = GetInstAssocDevSchemaBuffer();
        int bufsize = GetInstAssocDevSchemaBufferSize();
        if(!bufsize)
        {
            char result[1024] = "Schema Buffer is empty";
            AnscCopyString(pValue, (char*)&result);
            FreeInstAssocDevSchema();
            return -1;
        }
        else
        {
            if (bufsize < *pUlSize){
                AnscCopyString(pValue, GetInstAssocDevSchemaBuffer());
                CcspTraceWarning(("%s-%d : Buffer Size [%d]\n" , __FUNCTION__, __LINE__, (int)strlen(pValue)));
                FreeInstAssocDevSchema();
                return 0;
            }else{
                //CcspTraceWarning(("%s-%d : bufsize is %d, need %d\n" , __FUNCTION__, __LINE__, *pUlSize, bufsize));
                *pUlSize = bufsize + 1;
                FreeInstAssocDevSchema();
                return 1;
            }
        }
#endif
        return 0;
    }

    if( AnscEqualString(ParamName, "SchemaID", TRUE))
    {
#if 0
        AnscCopyString(pValue, "SchemaID Buffer is empty");
        /* collect value */
#else
        int bufsize = GetInstAssocDevSchemaIdBufferSize();
        if(!bufsize)
        {
            char result[1024] = "SchemaID Buffer is empty";
            AnscCopyString(pValue, (char*)&result);
            return -1;
        }
        else
        {
            CcspTraceWarning(("%s-%d : Buffer Size [%d] InputSize [%d]\n" , __FUNCTION__, __LINE__, bufsize, *pUlSize));
            if (bufsize < *pUlSize)
            {
                AnscCopyString(pValue, GetInstAssocDevSchemaIdBuffer());
                CcspTraceWarning(("%s-%d : pValue Buffer Size [%d]\n" , __FUNCTION__, __LINE__, (int)strlen(pValue)));
                return 0;
            }
            else
            {
                *pUlSize = bufsize + 1;
                return 1;
            }
        }
#endif
        return 0;
    }

    return -1;
}

BOOL
WifiClient_SetParamBoolValue
(
    ANSC_HANDLE                 hInsContext,
    char*                       ParamName,
    BOOL                        bValue
)
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    /* check the parameter name and set the corresponding value */

    if ( AnscEqualString(ParamName, "Enabled", TRUE))
    {
        if((bValue == true) && 
               (pHarvester->uINSTClientReportingPeriod > pHarvester->uINSTClientDefOverrideTTL))
        {
             AnscTraceWarning(("Can not start report when PollingPeriod > TTL\n"));
             return FALSE;
	}

        pHarvester->bINSTClientEnabledChanged = true;
        pHarvester->bINSTClientEnabled = bValue;

        return TRUE;
    }

    return FALSE;
}

BOOL
WifiClient_SetParamUlongValue
(
    ANSC_HANDLE                 hInsContext,
    char*                       ParamName,
    ULONG                       uValue
)
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if ( AnscEqualString(ParamName, "ReportingPeriod", TRUE))
    {
        pHarvester->uINSTClientReportingPeriod = uValue;
        pHarvester->bINSTClientReportingPeriodChanged = TRUE;

        return TRUE;
    }

    return FALSE;
}

BOOL
WifiClient_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue
    )
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if( AnscEqualString(ParamName, "MacAddress", TRUE))
    {
        if (Validate_InstClientMac(pValue)){
            AnscCopyString(pHarvester->MacAddress, pValue);
            pHarvester->bINSTClientMacAddressChanged = true;

            return TRUE;
        }else{
            pHarvester->bINSTClientMacAddressChanged = false;
            return FALSE;
        }
	return TRUE;
    }

    return FALSE;
}

BOOL
WifiClient_Validate
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pReturnParamName,
        ULONG*                      puLength
    )
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if(pHarvester->bINSTClientReportingPeriodChanged)
    {
        if (validateDefReportingPeriod(pHarvester->uINSTClientReportingPeriod) != TRUE)
        {
            AnscCopyString(pReturnParamName, "ReportingPeriod");
            *puLength = AnscSizeOfString("ReportingPeriod");
            AnscTraceWarning(("Unsupported parameter value '%s'\n", pReturnParamName));
            return FALSE;
        }

        /*When instant reporting is enabled, TTL must not be less than poll time */ 
        if ((CosaDmlWiFi_IsInstantMeasurementsEnable()) && (pHarvester->uINSTClientReportingPeriod != 0) && 
                  (pHarvester->uINSTClientReportingPeriod > pHarvester->uINSTClientDefOverrideTTL))
        {
            AnscCopyString(pReturnParamName, "ReportingPeriod");
            *puLength = AnscSizeOfString("ReportingPeriod");
            AnscTraceWarning(("Unsupported parameter value '%s'\n", pReturnParamName));
            return FALSE;
        }
    }

    return TRUE;
}

ULONG
WifiClient_Commit
(
    ANSC_HANDLE                 hInsContext
)
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    char recName[256];
    ULONG psmValue = 0;
    if(pHarvester->bINSTClientEnabledChanged)
    {
        psmValue = pHarvester->bINSTClientEnabled;
        SetNVRamULONGConfiguration(InstWifiClientEnabled, psmValue);
        pHarvester->bINSTClientEnabledChanged = false;

        CosaDmlWiFiClient_InstantMeasurementsEnable(pHarvester);
    }

    if(pHarvester->bINSTClientReportingPeriodChanged)
    {
        psmValue = pHarvester->uINSTClientReportingPeriod;
        SetNVRamULONGConfiguration(InstWifiClientReportingPeriod, psmValue);
        pHarvester->bINSTClientReportingPeriodChanged = false;
        CosaDmlWiFiClient_InstantMeasurementsReportingPeriod(pHarvester->uINSTClientReportingPeriod);
    }

    if(pHarvester->bINSTClientMacAddressChanged)
    {
	memset(recName, 0, sizeof(recName));
	sprintf(recName, "%s", InstWifiClientMacAddress);
	PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, pHarvester->MacAddress);

        pHarvester->bINSTClientMacAddressChanged = false;
	CosaDmlWiFiClient_InstantMeasurementsMacAddress(pHarvester->MacAddress);
    }
    return 0;
}

ULONG
WifiClient_Rollback
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if(pHarvester->bINSTClientReportingPeriodChanged)
    {
        pHarvester->uINSTClientReportingPeriod = GetINSTPollingPeriod();
        pHarvester->bINSTClientReportingPeriodChanged = false;
    }

    return 0;
}

/***********************************************************************

 APIs for Object:

    WiFi.X_RDKCENTRAL-COM_Report.WifiClient.Default.

    *  WifiClient_Default_GetParamUlongValue
    *  WifiClient_Default_SetParamUlongValue
    *  WifiClient_Default_Validate
    *  WifiClient_Default_Commit
    *  WifiClient_Default_Rollback

***********************************************************************/

BOOL
WifiClient_Default_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG*                      puLong
    )
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if ( AnscEqualString(ParamName, "OverrideTTL", TRUE))
    {
        *puLong =  GetINSTOverrideTTL();
        return TRUE;
    }

    if ( AnscEqualString(ParamName, "ReportingPeriod", TRUE))
    {
        *puLong =  pHarvester->uINSTClientDefReportingPeriod;
        return TRUE;
    }

    return FALSE;
}

BOOL
WifiClient_Default_SetParamUlongValue
(
    ANSC_HANDLE                 hInsContext,
    char*                       ParamName,
    ULONG                       uValue
)
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if ( AnscEqualString(ParamName, "ReportingPeriod", TRUE))
    {
        pHarvester->uINSTClientDefReportingPeriod = uValue;
        pHarvester->bINSTClientDefReportingPeriodChanged = TRUE;

        return TRUE;
    }

    if ( AnscEqualString(ParamName, "OverrideTTL", TRUE))
    {
        pHarvester->uINSTClientDefOverrideTTL = uValue;
        pHarvester->bINSTClientDefOverrideTTLChanged = TRUE;

        return TRUE;
    }

    return FALSE;
}

BOOL
WifiClient_Default_Validate
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pReturnParamName,
        ULONG*                      puLength
    )
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if(pHarvester->bINSTClientDefReportingPeriodChanged)
    {
        if (validateDefReportingPeriod(pHarvester->uINSTClientDefReportingPeriod) != TRUE)
        {
            AnscCopyString(pReturnParamName, "ReportingPeriod");
            *puLength = AnscSizeOfString("ReportingPeriod");
            AnscTraceWarning(("Unsupported parameter value '%s'\n", pReturnParamName));
            return FALSE;
        }
    }

    /*When instant reporting is enabled, TTL time must not be less than poll time */ 
    if(pHarvester->bINSTClientDefOverrideTTLChanged)
    {
        if ((CosaDmlWiFi_IsInstantMeasurementsEnable()) && (pHarvester->uINSTClientDefOverrideTTL != 0) && 
                  (pHarvester->uINSTClientDefOverrideTTL < pHarvester->uINSTClientReportingPeriod))
        {
            AnscCopyString(pReturnParamName, "OverrideTTL");
            *puLength = AnscSizeOfString("OverrideTTL");
            AnscTraceWarning(("Unsupported parameter value '%s'\n", pReturnParamName));
            return FALSE;
        }

        if (pHarvester->uINSTClientDefOverrideTTL > 900)
        {
            AnscCopyString(pReturnParamName, "OverrideTTL");
            *puLength = AnscSizeOfString("OverrideTTL");
            AnscTraceWarning(("Unsupported parameter value '%s'\n", pReturnParamName));
            return FALSE;
        }
    }

    return TRUE;
}

ULONG
WifiClient_Default_Commit
(
    ANSC_HANDLE                 hInsContext
)
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    char recName[256];
    ULONG psmValue = 0;
    if(pHarvester->bINSTClientDefReportingPeriodChanged)
    {
        psmValue = pHarvester->uINSTClientDefReportingPeriod;
        SetINSTDefReportingPeriod(pHarvester->uINSTClientDefReportingPeriod);
	SetNVRamULONGConfiguration(InstWifiClientDefReportingPeriod, psmValue);
        pHarvester->bINSTClientDefReportingPeriodChanged = false;
        CosaDmlWiFiClient_InstantMeasurementsDefReportingPeriod(pHarvester->uINSTClientDefReportingPeriod);
    }
 
    if(pHarvester->bINSTClientDefOverrideTTLChanged)
    {
        SetINSTOverrideTTL(pHarvester->uINSTClientDefOverrideTTL);
        pHarvester->bINSTClientDefOverrideTTLChanged = false;
        CosaDmlWiFiClient_InstantMeasurementsOverrideTTL(pHarvester->uINSTClientDefOverrideTTL);
    }

    return 0;
}

ULONG
WifiClient_Default_Rollback
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if(pHarvester->bINSTClientDefReportingPeriodChanged)
    {
        pHarvester->uINSTClientDefReportingPeriod = GetINSTDefReportingPeriod();
        pHarvester->bINSTClientDefReportingPeriodChanged = false;
    }

    if(pHarvester->bINSTClientDefOverrideTTLChanged)
    {
        pHarvester->uINSTClientDefOverrideTTL = GetINSTOverrideTTL();
        pHarvester->bINSTClientDefOverrideTTLChanged = false;
    }

    return 0;
}

#endif
