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
static char *WiFiActiveMsmtEnabled              = "eRT.com.cisco.spvtg.ccsp.Device.WiFi.WiFiActiveMsmtEnabled";
static char *WiFiActiveMsmtPktSize              = "eRT.com.cisco.spvtg.ccsp.Device.WiFi.WiFiActiveMsmtPktSize";
static char *WiFiActiveMsmtNumberOfSamples      = "eRT.com.cisco.spvtg.ccsp.Device.WiFi.WiFiActiveMsmtNumberOfSample";
static char *WiFiActiveMsmtSampleDuration       = "eRT.com.cisco.spvtg.ccsp.Device.WiFi.WiFiActiveMsmtSampleDuration";

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
#if defined(_XB6_PRODUCT_REQ_)
    /* PSM GET for ActiveMsmtEnabled */
    if (CCSP_SUCCESS != GetNVRamULONGConfiguration(WiFiActiveMsmtEnabled, &psmValue))
    {
        AnscTraceWarning(("%s : fetching the PSM db failed for ActiveMsmtEnabled\n", __func__));
    }
    else
    {
        pHarvester->bActiveMsmtEnabled = psmValue;
        pHarvester->bActiveMsmtEnabledChanged = FALSE;
        if(ANSC_STATUS_SUCCESS != CosaDmlWiFi_ActiveMsmtEnable(pHarvester))
        {
            AnscTraceWarning(("%s : Active measurement enable failed\n", __func__));
        }
    }

    /* PSM GET for ActiveMsmtSampleDuration */
    if (CCSP_SUCCESS != GetNVRamULONGConfiguration(WiFiActiveMsmtSampleDuration, &psmValue))
    {
        AnscTraceWarning(("%s : fetching the PSM db failed for ActiveMsmtSampleDuration\n", __func__));
    }
    else
    {
        pHarvester->uActiveMsmtSampleDuration = psmValue;
        pHarvester->bActiveMsmtSampleDurationChanged = FALSE;
        if(ANSC_STATUS_SUCCESS != CosaDmlWiFi_ActiveMsmtSampleDuration(pHarvester))
        {
            AnscTraceWarning(("%s : setting Active measurement Sample duration failed\n", __func__));
        }
    }

    /* PSM GET for ActiveMsmtPktSize */
    if (CCSP_SUCCESS != GetNVRamULONGConfiguration(WiFiActiveMsmtPktSize, &psmValue))
    {
        AnscTraceWarning(("%s : fetching the PSM db failed for ActiveMsmtPktSize\n", __func__));
    }
    else
    {
        pHarvester->uActiveMsmtPktSize = psmValue;
        pHarvester->bActiveMsmtPktSizeChanged = FALSE;
        if(ANSC_STATUS_SUCCESS != CosaDmlWiFi_ActiveMsmtPktSize(pHarvester))
        {
            AnscTraceWarning(("%s : setting Active measurement Packet Size failed\n", __func__));
        }
    }

    /* PSM GET for ActiveMsmtNumberOfSamples */
    if (CCSP_SUCCESS != GetNVRamULONGConfiguration(WiFiActiveMsmtNumberOfSamples, &psmValue))
    {
        AnscTraceWarning(("%s : fetching the PSM db failed for ActiveMsmtNumberOfSamples\n", __func__));
    }
    else
    {
        pHarvester->uActiveMsmtNumberOfSamples = psmValue;
        pHarvester->bActiveMsmtNumberOfSamplesChanged = FALSE;
        if(ANSC_STATUS_SUCCESS != CosaDmlWiFi_ActiveMsmtNumberOfSamples(pHarvester))
        {
            AnscTraceWarning(("%s : setting Active measurement Number of samples failed\n", __func__));
        }
    }
#endif
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

/***********************************************************************

 APIs for Object:

    WiFi.X_RDKCENTRAL-COM_Report.WifiClient.ActiveMeasurements.
    *  WifiClient_ActiveMeasurements_GetParamBoolValue
    *  WifiClient_ActiveMeasurements_SetParamBoolValue
    *  WifiClient_ActiveMeasurements_GetParamUlongValue
    *  WifiClient_ActiveMeasurements_SetParamUlongValue
    *  WifiClient_ActiveMeasurements_Validate
    *  WifiClient_ActiveMeasurements_Commit
    *  WifiClient_ActiveMeasurements_Rollback

***********************************************************************/

BOOL
WifiClient_ActiveMeasurements_GetParamBoolValue
(
    ANSC_HANDLE                 hInsContext,
    char*                       ParamName,
    BOOL*                       pBool
)
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if ( AnscEqualString(ParamName, "Enable", TRUE))
    {
        *pBool    =  CosaDmlWiFi_IsActiveMeasurementEnable();
        return TRUE;
    }
    return FALSE;
}

BOOL
WifiClient_ActiveMeasurements_GetParamUlongValue
(
    ANSC_HANDLE                 hInsContext,
    char*                       ParamName,
    ULONG*                      puLong
)
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if ( AnscEqualString(ParamName, "PacketSize", TRUE))
    {
        *puLong = pHarvester->uActiveMsmtPktSize;
        return TRUE;
    }

    if ( AnscEqualString(ParamName, "SampleDuration", TRUE))
    {
        *puLong = pHarvester->uActiveMsmtSampleDuration;
        return TRUE;
    }

    if ( AnscEqualString(ParamName, "NumberOfSamples", TRUE))
    {
        *puLong =  pHarvester->uActiveMsmtNumberOfSamples;
        return TRUE;
    }
    return FALSE;
}

BOOL
WifiClient_ActiveMeasurements_SetParamBoolValue
(
    ANSC_HANDLE                 hInsContext,
    char*                       ParamName,
    BOOL                        bValue
)
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    BOOL rfc;
    char recName[256] = {0x0};
    char* strValue = NULL;

    memset(recName, 0, sizeof(recName));
    sprintf(recName, "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.WifiClient.ActiveMeasurements.Enable");

    if (PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue) != CCSP_SUCCESS) {
        AnscTraceWarning(("%s : fetching the PSM db failed for ActiveMsmt RFC\n", __func__));
    }
    else
    {
        rfc = atoi(strValue);
        if(!rfc)
        {
            AnscTraceWarning(("%s : ActiveMsmt RFC is disabled \n", __func__));
            return FALSE;
        }
    }

    /* check the parameter name and set the corresponding value */

    if ( AnscEqualString(ParamName, "Enable", TRUE))
    {
        pHarvester->bActiveMsmtEnabledChanged = true;
        pHarvester->bActiveMsmtEnabled = bValue;
        return TRUE;
    }

    return FALSE;
}

BOOL
WifiClient_ActiveMeasurements_SetParamUlongValue
(
    ANSC_HANDLE                 hInsContext,
    char*                       ParamName,
    ULONG                       uValue
)
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if ( AnscEqualString(ParamName, "PacketSize", TRUE))
    {
        pHarvester->uActiveMsmtOldPktSize = pHarvester->uActiveMsmtPktSize;
        pHarvester->uActiveMsmtPktSize = uValue;
        pHarvester->bActiveMsmtPktSizeChanged = TRUE;
        return TRUE;
    }

    if ( AnscEqualString(ParamName, "SampleDuration", TRUE))
    {
        pHarvester->uActiveMsmtOldSampleDuration = pHarvester->uActiveMsmtSampleDuration;
        pHarvester->uActiveMsmtSampleDuration = uValue;
        pHarvester->bActiveMsmtSampleDurationChanged = TRUE;
        return TRUE;
    }

    if ( AnscEqualString(ParamName, "NumberOfSamples", TRUE))
    {
        pHarvester->uActiveMsmtOldNumberOfSamples = pHarvester->uActiveMsmtNumberOfSamples;
        pHarvester->uActiveMsmtNumberOfSamples = uValue;
        pHarvester->bActiveMsmtNumberOfSamplesChanged = TRUE;
        return TRUE;
    }
    return FALSE;
}

BOOL
WifiClient_ActiveMeasurements_Validate
(
    ANSC_HANDLE                 hInsContext,
    char*                       pReturnParamName,
    ULONG*                      puLength
)
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if (pHarvester->bActiveMsmtPktSizeChanged)
    {
        if((pHarvester->uActiveMsmtPktSize < MIN_ACTIVE_MSMT_PKT_SIZE) ||
           (pHarvester->uActiveMsmtPktSize > MAX_ACTIVE_MSMT_PKT_SIZE))
        {
            AnscCopyString(pReturnParamName, "PacketSize");
            *puLength = AnscSizeOfString("PacketSize");
            AnscTraceWarning(("Unsupported parameter value '%s'\n", pReturnParamName));
            return FALSE;
        }
    }

    if (pHarvester->bActiveMsmtNumberOfSamplesChanged)
    {
        if((pHarvester->uActiveMsmtNumberOfSamples < MIN_ACTIVE_MSMT_SAMPLE_COUNT) ||
           (pHarvester->uActiveMsmtNumberOfSamples > MAX_ACTIVE_MSMT_SAMPLE_COUNT))
        {
            AnscCopyString(pReturnParamName, "NumberOfSamples");
            *puLength = AnscSizeOfString("NumberOfSamples");
            AnscTraceWarning(("Unsupported parameter value '%s'\n", pReturnParamName));
            return FALSE;
        }
    }

    if (pHarvester->bActiveMsmtSampleDurationChanged)
    {
        if((pHarvester->uActiveMsmtSampleDuration < MIN_ACTIVE_MSMT_SAMPLE_DURATION) ||
           (pHarvester->uActiveMsmtSampleDuration > MAX_ACTIVE_MSMT_SAMPLE_DURATION))
        {
            AnscCopyString(pReturnParamName, "SampleDuration");
            *puLength = AnscSizeOfString("SampleDuration");
            AnscTraceWarning(("Unsupported parameter value '%s'\n", pReturnParamName));
            return FALSE;
        }
    }

    return TRUE;
}

ULONG
WifiClient_ActiveMeasurements_Commit
(
    ANSC_HANDLE                 hInsContext
)
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;
    ULONG psmValue = 0;

    if (pHarvester->bActiveMsmtEnabledChanged)
    {
        psmValue = pHarvester->bActiveMsmtEnabled;
        if(CCSP_SUCCESS != SetNVRamULONGConfiguration(WiFiActiveMsmtEnabled, psmValue))
        {
            AnscTraceWarning(("%s : updating the PSM db failed for ActiveMsmtEnabled\n", __func__));
        }
        pHarvester->bActiveMsmtEnabledChanged = false;
        if(ANSC_STATUS_SUCCESS != CosaDmlWiFi_ActiveMsmtEnable(pHarvester))
        {
            AnscTraceWarning(("%s : Active measurement enable failed\n", __func__));
        }
    }

    if (pHarvester->bActiveMsmtSampleDurationChanged)
    {
        psmValue = pHarvester->uActiveMsmtSampleDuration;
        if(CCSP_SUCCESS != SetNVRamULONGConfiguration(WiFiActiveMsmtSampleDuration, psmValue))
        {
            AnscTraceWarning(("%s : updating the PSM db failed for ActiveMsmtSampleDuration\n", __func__));
        }
        pHarvester->bActiveMsmtSampleDurationChanged = false;
        if(ANSC_STATUS_SUCCESS != CosaDmlWiFi_ActiveMsmtSampleDuration(pHarvester))
        {
            AnscTraceWarning(("%s : setting Active measurement Sample duration failed\n", __func__));
        }
    }

    if (pHarvester->bActiveMsmtPktSizeChanged)
    {
        psmValue = pHarvester->uActiveMsmtPktSize;
        if(CCSP_SUCCESS != SetNVRamULONGConfiguration(WiFiActiveMsmtPktSize, psmValue))
        {
            AnscTraceWarning(("%s : updating the PSM db failed for ActiveMsmtPktSize\n", __func__));
        }
        pHarvester->bActiveMsmtPktSizeChanged = false;
        if(ANSC_STATUS_SUCCESS != CosaDmlWiFi_ActiveMsmtPktSize(pHarvester))
        {
            AnscTraceWarning(("%s : setting Active measurement Packet Size failed\n", __func__));
        }
    }

    if (pHarvester->bActiveMsmtNumberOfSamplesChanged)
    {
        psmValue = pHarvester->uActiveMsmtNumberOfSamples;
        if(CCSP_SUCCESS != SetNVRamULONGConfiguration(WiFiActiveMsmtNumberOfSamples, psmValue))
        {
            AnscTraceWarning(("%s : updating the PSM db failed for ActiveMsmtNumberOfSamples\n", __func__));
        }
        pHarvester->bActiveMsmtNumberOfSamplesChanged = false;
        if(ANSC_STATUS_SUCCESS != CosaDmlWiFi_ActiveMsmtNumberOfSamples(pHarvester))
        {
            AnscTraceWarning(("%s : setting Active measurement Number of samples failed\n", __func__));
        }
    }

    return 0;
}

ULONG
WifiClient_ActiveMeasurements_Rollback
(
    ANSC_HANDLE                 hInsContext
)
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if (pHarvester->bActiveMsmtSampleDurationChanged)
    {
        pHarvester->uActiveMsmtSampleDuration = pHarvester->uActiveMsmtOldSampleDuration;
        pHarvester->bActiveMsmtSampleDurationChanged = false;
    }

    if (pHarvester->bActiveMsmtPktSizeChanged)
    {
        pHarvester->uActiveMsmtPktSize = pHarvester->uActiveMsmtOldPktSize;
        pHarvester->bActiveMsmtPktSizeChanged = false;
    }

    if (pHarvester->bActiveMsmtNumberOfSamplesChanged)
    {
        pHarvester->uActiveMsmtNumberOfSamples = pHarvester->uActiveMsmtOldNumberOfSamples;
        pHarvester->bActiveMsmtNumberOfSamplesChanged = false;
    }

    return 0;
}

/***********************************************************************

 APIs for Object:

    WiFi.X_RDKCENTRAL-COM_Report.WifiClient.ActiveMeasurements.Plan
    *  ActiveMeasurements_Plan_GetParamStringValue
    *  ActiveMeasurements_Plan_SetParamStringValue
    *  ActiveMeasurements_Plan_Validate
    *  ActiveMeasurements_Plan_Commit
    *  ActiveMeasurements_Plan_Rollback

***********************************************************************/
ULONG
ActiveMeasurements_Plan_GetParamStringValue
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

    if ( AnscEqualString(ParamName, "PlanID", TRUE))
    {
        AnscCopyString(pValue, pHarvester->ActiveMsmtPlanID);
        return 0;
    }
    return -1;
}

BOOL
ActiveMeasurements_Plan_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue
    )
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if ( AnscEqualString(ParamName, "PlanID", TRUE))
    {
        AnscCopyString(pHarvester->ActiveMsmtPlanID, pValue);
        pHarvester->bActiveMsmtPlanIDChanged = true;

        return TRUE;
    }
    return FALSE;
}

BOOL
ActiveMeasurements_Plan_Validate
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pReturnParamName,
        ULONG*                      puLength
    )
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if (pHarvester->bActiveMsmtPlanIDChanged)
    {
        if (AnscSizeOfString(pHarvester->ActiveMsmtPlanID) != (PLAN_ID_LEN -1 ))
        {
            AnscCopyString(pReturnParamName, "PlanID");
            *puLength = AnscSizeOfString("PlanID");
            AnscTraceWarning(("Unsupported parameter value '%s'\n", pReturnParamName));
            return FALSE;
        }
    }
    return TRUE;
}

ULONG
ActiveMeasurements_Plan_Commit
(
    ANSC_HANDLE                 hInsContext
)
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if (pHarvester->bActiveMsmtPlanIDChanged)
    {
        pHarvester->bActiveMsmtPlanIDChanged = false;
        if (ANSC_STATUS_SUCCESS != CosaDmlWiFiClient_SetActiveMsmtPlanId(pHarvester))
        {
            AnscTraceWarning(("%s : setting Active measurement Plan ID failed\n", __func__));
        }
    }
    return 0;
}

ULONG
ActiveMeasurements_Plan_Rollback
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if (pHarvester->bActiveMsmtPlanIDChanged)
    {
        pHarvester->bActiveMsmtPlanIDChanged = false;
    }
    return 0;
}

/***********************************************************************

 APIs for Object:

    WiFi.X_RDKCENTRAL-COM_Report.WifiClient.ActiveMeasurements.Plan.Step{i}.
    *  ActiveMeasurement_Step_GetEntryCount
    *  ActiveMeasurement_Step_GetEntry
    *  ActiveMeasurement_Step_GetParamIntValue
    *  ActiveMeasurement_Step_GetParamStringValue
    *  ActiveMeasurement_Step_SetParamIntValue
    *  ActiveMeasurement_Step_SetParamStringValue
    *  ActiveMeasurement_Step_Validate
    *  ActiveMeasurement_Step_Commit
    *  ActiveMeasurement_Step_Rollback

***********************************************************************/
ULONG
ActiveMeasurement_Step_GetEntryCount
    (
        ANSC_HANDLE                 hInsContext
    )
{
    return ACTIVE_MSMT_STEP_COUNT;
}

ANSC_HANDLE
ActiveMeasurement_Step_GetEntry
    (
        ANSC_HANDLE                 hInsContext,
        ULONG                       nIndex,
        ULONG*                      pInsNumber
    )
{
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;
    PCOSA_DML_WIFI_ACTIVE_MSMT_STEP_FULL pStepFull = (PCOSA_DML_WIFI_ACTIVE_MSMT_STEP_FULL) &pHarvester->Step;

    if (nIndex < 0 || nIndex >= ACTIVE_MSMT_STEP_COUNT)
        return (ANSC_HANDLE)NULL;

    *pInsNumber = nIndex + 1;
    return (ANSC_HANDLE)&pStepFull->StepCfg[nIndex];
}

BOOL
ActiveMeasurement_Step_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG*                      puLong
    )
{
    PCOSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG pStepCfg = (PCOSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG) hInsContext;
    /* check the parameter name and return the corresponding value */
    if ( AnscEqualString(ParamName, "StepID", TRUE))
    {
        /* collect value */
        *puLong = pStepCfg->StepId;
        return TRUE;
    }
    return FALSE;
}

ULONG
ActiveMeasurement_Step_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
    PCOSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG pStepCfg = (PCOSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG) hInsContext;

    if ( AnscEqualString(ParamName, "SourceMac", TRUE))
    {
        AnscCopyString(pValue, pStepCfg->SourceMac);
        return 0;
    }
    if ( AnscEqualString(ParamName, "DestMac", TRUE))
    {
        AnscCopyString(pValue, pStepCfg->DestMac);
        return 0;
    }
    return -1;
}

BOOL
ActiveMeasurement_Step_SetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        ULONG                       uValue
    )
{
    PCOSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG pStepCfg = (PCOSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG) hInsContext;
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    ULONG    StepIns = 0;
    if (ANSC_STATUS_SUCCESS != ValidateActiveMsmtPlanID(pHarvester->ActiveMsmtPlanID))
    {
        CcspTraceWarning(("%s-%d : NULL value for PlanId\n" , __FUNCTION__, __LINE__ ));
        return FALSE;
    }

    /* Get the instance number */
    if (ANSC_STATUS_SUCCESS != GetActiveMsmtStepInsNum(pStepCfg, &StepIns))
    {
        AnscTraceWarning(("%s : GetActiveMsmtStepInsNum failed\n", __func__));
        return FALSE;
    }

    /* check the parameter name and return the corresponding value */
    if ( AnscEqualString(ParamName, "StepID", TRUE))
    {
        pStepCfg->StepId = (unsigned int) uValue;

        if (ANSC_STATUS_SUCCESS != CosaDmlWiFiClient_SetActiveMsmtStepId(pStepCfg->StepId, StepIns))
        {
            AnscTraceWarning(("%s : setting Active measurement Step ID failed\n", __func__));
        }
        return TRUE;
    }
    return FALSE;
}

BOOL
ActiveMeasurement_Step_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        char*                       pValue
    )
{
    PCOSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG pStepCfg = (PCOSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG) hInsContext;
    PCOSA_DATAMODEL_WIFI    pMyObject   = (PCOSA_DATAMODEL_WIFI  )g_pCosaBEManager->hWifi;
    PCOSA_DML_WIFI_HARVESTER pHarvester = (PCOSA_DML_WIFI_HARVESTER)pMyObject->pHarvester;

    if (ANSC_STATUS_SUCCESS != ValidateActiveMsmtPlanID(pHarvester->ActiveMsmtPlanID))
    {
        CcspTraceWarning(("%s-%d : NULL value for PlanId\n" , __FUNCTION__, __LINE__ ));
        return FALSE;
    }

    if (AnscEqualString(ParamName, "SourceMac", TRUE))
    {
        AnscCopyString(pStepCfg->SourceMac, pValue);
        pStepCfg->bSrcMacChanged = true;
        return TRUE;
    }

    if (AnscEqualString(ParamName, "DestMac", TRUE))
    {
        AnscCopyString(pStepCfg->DestMac, pValue);
        pStepCfg->bDstMacChanged = true;
        return TRUE;
    }
    return FALSE;
}
BOOL
ActiveMeasurement_Step_Validate
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pReturnParamName,
        ULONG*                      puLength
    )
{
    PCOSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG pStepCfg = (PCOSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG) hInsContext;

    if (pStepCfg->bSrcMacChanged)
    {
        if (!Validate_InstClientMac(pStepCfg->SourceMac))
        {
            AnscCopyString(pReturnParamName, "SourceMac");
            *puLength = AnscSizeOfString("SourceMac");
            AnscTraceWarning(("Unsupported parameter value '%s'\n", pReturnParamName));
            return FALSE;
        }
    }

    if (pStepCfg->bDstMacChanged)
    {
        if (!Validate_InstClientMac(pStepCfg->DestMac))
        {
            AnscCopyString(pReturnParamName, "DestMac");
            *puLength = AnscSizeOfString("DestMac");
            AnscTraceWarning(("Unsupported parameter value '%s'\n", pReturnParamName));
            return FALSE;
        }
    }

    return TRUE;
}

ULONG
ActiveMeasurement_Step_Commit
(
    ANSC_HANDLE                 hInsContext
)
{
    PCOSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG pStepCfg = (PCOSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG) hInsContext;
    ULONG    StepIns = 0;

    /* Get the instance number */
    if (ANSC_STATUS_SUCCESS != GetActiveMsmtStepInsNum(pStepCfg, &StepIns))
    {
        AnscTraceWarning(("%s : GetActiveMsmtStepInsNum failed\n", __func__));
        return FALSE;
    }

    if (pStepCfg->bSrcMacChanged)
    {
        pStepCfg->bSrcMacChanged = false;

        if (ANSC_STATUS_SUCCESS != CosaDmlActiveMsmt_Step_SetSrcMac(pStepCfg->SourceMac, StepIns))
        {
            AnscTraceWarning(("%s : setting Active measurement Step Source Mac failed\n", __func__));
        }
    }

    if (pStepCfg->bDstMacChanged)
    {
        pStepCfg->bDstMacChanged = false;

        if (ANSC_STATUS_SUCCESS != CosaDmlActiveMsmt_Step_SetDestMac(pStepCfg->DestMac, StepIns))
        {
            AnscTraceWarning(("%s : setting Active measurement Step Dest Mac failed\n", __func__));
        }
    }
    return ANSC_STATUS_SUCCESS;
}

ULONG
ActiveMeasurement_Step_Rollback
    (
        ANSC_HANDLE                 hInsContext
    )
{
    PCOSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG pStepCfg = (PCOSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG) hInsContext;

    if (pStepCfg->bSrcMacChanged)
    {
        pStepCfg->bSrcMacChanged = false;
    }

    if (pStepCfg->bDstMacChanged)
    {
        pStepCfg->bDstMacChanged = false;
    }
    return ANSC_STATUS_SUCCESS;
}

#endif
