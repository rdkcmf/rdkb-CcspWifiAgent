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

        *  CosaWifiCreate
        *  CosaWifiInitialize
        *  CosaWifiRemove
        *  CosaDmlWifiGetPortMappingNumber
    -------------------------------------------------------------------

    environment:

        platform independent

    -------------------------------------------------------------------

    author:

        COSA XML TOOL CODE GENERATOR 1.0

    -------------------------------------------------------------------

    revision:

        01/11/2011    initial revision.

**************************************************************************/
#include "cosa_apis.h"
#include "cosa_wifi_apis.h"
#include "cosa_wifi_internal.h"
#include "plugin_main_apis.h"
#include "wifi_hal.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lm_api.h"

//LNT_EMU
extern ANSC_HANDLE bus_handle;//lnt
extern char g_Subsystem[32];//lnt
static char *BssSsid ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.SSID.%d.SSID";
static char *HideSsid ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.SSID.%d.HideSSID";
static char *Passphrase ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.SSID.%d.Passphrase";
static char *ChannelNumber ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.%d.Channel";
#ifdef _COSA_SIM_

ANSC_STATUS
CosaDmlWiFiInit
    (
        ANSC_HANDLE                 hDml,
        PANSC_HANDLE                phContext
    )
{

        return ANSC_STATUS_SUCCESS;
}

static int gRadioCount = 6;//RDK_EMU
static int ServiceTab = 0;

/*
 *  Description:
 *     The API retrieves the number of WiFi radios in the system.
 */
ULONG
CosaDmlWiFiRadioGetNumberOfEntries
    (
        ANSC_HANDLE                 hContext
    )
{
    ANSC_STATUS                     returnStatus = ANSC_STATUS_SUCCESS;
    ULONG                           ulCount      = 0;

#if 0
    if (!g_pCosaBEManager->has_wifi_slap)
    {
        /*wifi is in remote CPU*/
        return 0;
    }
    else
    {
        CcspTraceWarning(("CosaDmlWiFiRadioGetNumberOfEntries - This is a local call ...\n"));
        return gRadioCount;
    }
#endif
    return gRadioCount;
}    
    
    
/* Description:
 *	The API retrieves the complete info of the WiFi radio designated by index. 
 *	The usual process is the caller gets the total number of entries, 
 *	then iterate through those by calling this API.
 * Arguments:
 * 	ulIndex		Indicates the index number of the entry.
 * 	pEntry		To receive the complete info of the entry.
 */
ANSC_STATUS
CosaDmlWiFiRadioGetEntry
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIndex,
        PCOSA_DML_WIFI_RADIO_FULL   pEntry
    )
{
    PCOSA_DML_WIFI_RADIO_FULL       pWifiRadio      = pEntry;
    PCOSA_DML_WIFI_RADIO_CFG        pWifiRadioCfg   = &pWifiRadio->Cfg;
    PCOSA_DML_WIFI_RADIO_SINFO      pWifiRadioSinfo = &pWifiRadio->StaticInfo;
    PCOSA_DML_WIFI_RADIO_DINFO      pWifiRadioDinfo = &pWifiRadio->DynamicInfo;
    /*PPOAM_COSAWIFIDM_OBJECT*/ANSC_HANDLE         pPoamWiFiDm     = (/*PPOAM_COSAWIFIDM_OBJECT*/ANSC_HANDLE)hContext;

    if ( pPoamWiFiDm )
    {
        return 0;
    }
    else
    {
        /*dummy data*/
        pWifiRadio->Cfg.InstanceNumber = ulIndex + 1;

        sprintf(pWifiRadio->StaticInfo.Name, "eth%d", ulIndex);
//RDKB-EMULATOR
	if(ulIndex+1 == 1)
	{
		AnscCopyString(pWifiRadio->StaticInfo.Name,"wlan0");
        pWifiRadio->StaticInfo.SupportedFrequencyBands = COSA_DML_WIFI_FREQ_BAND_2_4G;                   /* Bitmask of COSA_DML_WIFI_FREQ_BAND */
        pWifiRadio->StaticInfo.SupportedStandards      = COSA_DML_WIFI_STD_b | COSA_DML_WIFI_STD_g;      /* Bitmask of COSA_DML_WIFI_STD */
        AnscCopyString(pWifiRadio->StaticInfo.PossibleChannels, "1,2,3,4,5,6,7,8,9,10,11");
	}
	else if(ulIndex+1 == 2)
	{
		AnscCopyString(pWifiRadio->StaticInfo.Name,"wlan1");
        pWifiRadio->StaticInfo.SupportedFrequencyBands = COSA_DML_WIFI_FREQ_BAND_5G;                   /* Bitmask of COSA_DML_WIFI_FREQ_BAND */
        pWifiRadio->StaticInfo.SupportedStandards      = COSA_DML_WIFI_STD_ac;      /* Bitmask of COSA_DML_WIFI_STD */
        AnscCopyString(pWifiRadio->StaticInfo.PossibleChannels, "36,40,44,48,149,153,157,161,165");
	}
	else if(ulIndex+1 == 5)
	{
		AnscCopyString(pWifiRadio->StaticInfo.Name,"wlan0_0");
		pWifiRadio->StaticInfo.SupportedFrequencyBands = COSA_DML_WIFI_FREQ_BAND_2_4G;                   /* Bitmask of COSA_DML_WIFI_FREQ_BAND */
        pWifiRadio->StaticInfo.SupportedStandards      = COSA_DML_WIFI_STD_b | COSA_DML_WIFI_STD_g;      /* Bitmask of COSA_DML_WIFI_STD */
        AnscCopyString(pWifiRadio->StaticInfo.PossibleChannels, "1,2,3,4,5,6,7,8,9,10,11");

	}
	else if(ulIndex+1 == 6)
	{
          		AnscCopyString(pWifiRadio->StaticInfo.Name,"wlan2");
		 pWifiRadio->StaticInfo.SupportedFrequencyBands = COSA_DML_WIFI_FREQ_BAND_5G;                   /* Bitmask of COSA_DML_WIFI_FREQ_BAND */
        pWifiRadio->StaticInfo.SupportedStandards      = COSA_DML_WIFI_STD_ac;      /* Bitmask of COSA_DML_WIFI_STD */
        AnscCopyString(pWifiRadio->StaticInfo.PossibleChannels, "36,40,44,48,149,153,157,161,165");

	}

        sprintf(pWifiRadio->Cfg.Alias, "Radio%d", ulIndex);
        
        pWifiRadio->StaticInfo.bUpstream               = TRUE;
        pWifiRadio->StaticInfo.MaxBitRate              = 128+ulIndex;
        pWifiRadio->StaticInfo.AutoChannelSupported    = TRUE;
        AnscCopyString(pWifiRadio->StaticInfo.TransmitPowerSupported, "10,20,50,100");
        pWifiRadio->StaticInfo.IEEE80211hSupported     = TRUE;

        CosaDmlWiFiRadioGetCfg(NULL, pWifiRadioCfg);
        CosaDmlWiFiRadioGetDinfo(NULL, pWifiRadioCfg->InstanceNumber, pWifiRadioDinfo);    

        return ANSC_STATUS_SUCCESS;
    }

}

ANSC_STATUS
CosaDmlWiFiRadioSetDefaultCfgValues
    (
        ANSC_HANDLE                 hContext,
        unsigned long               ulIndex,
        PCOSA_DML_WIFI_RADIO_CFG    pCfg
    )
{
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiRadioSetValues
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIndex,
        ULONG                       ulInstanceNumber,
        char*                       pAlias
    )
{
        return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiRadioSetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_RADIO_CFG    pCfg        /* Identified by InstanceNumber */
    )
{
    PCOSA_DML_WIFI_RADIO_CFG        pWifiRadioCfg  = pCfg;
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
//LNT_EMU	
        pCfg->LastChange             = AnscGetTickInSeconds();
	if(pCfg->ApplySetting == TRUE)
        {
        //wifi_stopHostApd();
        //wifi_startHostApd();
	if(pCfg->bEnabled == TRUE)
		wifi_applyRadioSettings( pCfg->InstanceNumber);//RDKB-EMU-L 
	pCfg->ApplySetting = FALSE;
	pWifiRadioCfg->ApplySetting = FALSE;
        }
        return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiRadioGetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_RADIO_CFG    pCfg        /* Identified by InstanceNumber */
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    char *param_value;//RDKB_EMULATOR
    char param_name[256] = {0};
    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
    
 //     pCfg->bEnabled                       = TRUE; //RDKB-EMU
        pCfg->LastChange                       = 123456; //RDKB-EMU
        pCfg->OperatingFrequencyBand         = COSA_DML_WIFI_FREQ_BAND_2_4G;
        pCfg->OperatingStandards             = COSA_DML_WIFI_STD_b | COSA_DML_WIFI_STD_g;         /* Bitmask of COSA_DML_WIFI_STD */
       // pCfg->Channel                        = 1;//LNT_EMU
        pCfg->AutoChannelEnable              = FALSE; //RDKB-EMU
        pCfg->AutoChannelRefreshPeriod       = 3600;
        pCfg->OperatingChannelBandwidth      = COSA_DML_WIFI_CHAN_BW_20M;
        pCfg->ExtensionChannel               = COSA_DML_WIFI_EXT_CHAN_Above;
        pCfg->GuardInterval                  = COSA_DML_WIFI_GUARD_INTVL_400ns;
        pCfg->MCS                            = 1;
        pCfg->TransmitPower                  = 100;
        pCfg->IEEE80211hEnabled              = TRUE;
        AnscCopyString(pCfg->RegulatoryDomain, "COM");

//	wifi_getRadioChannel(pCfg->InstanceNumber,&pCfg->Channel);//LNT_EMU
        /* Below is Cisco Extensions */
        pCfg->APIsolation                    = TRUE;
        pCfg->FrameBurst                     = TRUE;
        pCfg->TxRate                         = COSA_DML_WIFI_TXRATE_54M;
        pCfg->CTSProtectionMode              = TRUE;
        pCfg->BeaconInterval                 = 3600;
        pCfg->DTIMInterval                   = 100;
        pCfg->FragmentationThreshold         = 1024;
        pCfg->RTSThreshold                   = 1024;
        /* USGv2 Extensions */

        pCfg->LongRetryLimit                = 5;
        pCfg->MbssUserControl               = 1;
        pCfg->AdminControl                  = 12;
        pCfg->OnOffPushButtonTime           = 23;
        pCfg->ObssCoex                      = 34;
        pCfg->MulticastRate                 = 45;
        pCfg->ApplySetting                  = FALSE;
        pCfg->X_CISCO_COM_ReverseDirectionGrant = FALSE;
        pCfg->X_CISCO_COM_AggregationMSDU = FALSE;
        pCfg->X_CISCO_COM_AutoBlockAck = FALSE;
        pCfg->X_CISCO_COM_DeclineBARequest = FALSE;
        pCfg->X_CISCO_COM_HTTxStream = 1;
        pCfg->X_CISCO_COM_HTRxStream = 2;
        pCfg->X_CISCO_COM_STBCEnable = TRUE;

	memset(param_name, 0, sizeof(param_name));//PSM ACCESS RDKB_EMULATOR
        sprintf(param_name, ChannelNumber,pCfg->InstanceNumber);
        PSM_Get_Record_Value2(bus_handle,g_Subsystem, param_name, NULL, &param_value);
        if(param_value!=NULL){
                pCfg->Channel = atoi(param_value);
        }
        else{
                return 0;
        }
	wifi_getRadioEnable(pCfg->InstanceNumber,&pCfg->bEnabled);//RDKB-EMU
	if((pCfg->InstanceNumber == 1) || (pCfg->InstanceNumber == 5))
	{
	pCfg->OperatingFrequencyBand         = COSA_DML_WIFI_FREQ_BAND_2_4G;
        pCfg->OperatingStandards             = COSA_DML_WIFI_STD_g;
	}
	else if((pCfg->InstanceNumber == 2) || (pCfg->InstanceNumber == 6))
	{
	pCfg->OperatingFrequencyBand         = COSA_DML_WIFI_FREQ_BAND_5G;
        pCfg->OperatingStandards             = COSA_DML_WIFI_STD_ac;
	}
        return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiRadioGetDinfo
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber,
        PCOSA_DML_WIFI_RADIO_DINFO  pInfo
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

    if (!pInfo)
    {
        return ANSC_STATUS_FAILURE;
    }
    
    if (FALSE)
    {
        return returnStatus;
    }
    else
    {
        pInfo->Status                 = COSA_DML_IF_STATUS_Up;
        //pInfo->LastChange             = 123456;
        AnscCopyString(pInfo->ChannelsInUse, "1");
        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS
CosaDmlWiFiRadioGetStats
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber,
        PCOSA_DML_WIFI_RADIO_STATS  pStats
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

    if (!pStats)
    {
        return ANSC_STATUS_FAILURE;
    }
    
    if (FALSE)
    {
	return returnStatus;
    }
    else
    {
        pStats->BytesSent                          = 123456;
        pStats->BytesReceived                      = 234561;
        pStats->PacketsSent                        = 235;
        pStats->PacketsReceived                    = 321;
        pStats->ErrorsSent                         = 0;
        pStats->ErrorsReceived                     = 0;
        pStats->DiscardPacketsSent                 = 1;
        pStats->DiscardPacketsReceived             = 3;
    
        return ANSC_STATUS_SUCCESS;
    }
}

/* WiFi SSID */
static int gSsidCount = 6;//RDKB-EMU
/* Description:
 *	The API retrieves the number of WiFi SSIDs in the system.
 */
ULONG
CosaDmlWiFiSsidGetNumberOfEntries
    (
        ANSC_HANDLE                 hContext
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

    if (FALSE)
    {
        return 0;
    }
    else
    {
        return gSsidCount;
    }
}

/* Description:
 *	The API retrieves the complete info of the WiFi SSID designated by index. The usual process is the caller gets the total number of entries, then iterate through those by calling this API.
 * Arguments:
 * 	ulIndex		Indicates the index number of the entry.
 * 	pEntry		To receive the complete info of the entry.
 */
ANSC_STATUS
CosaDmlWiFiSsidGetEntry
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIndex,
        PCOSA_DML_WIFI_SSID_FULL    pEntry
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

    if (FALSE)
    {
        return returnStatus;
    }
    else
    {
        /*Set default Name & Alias*/
        sprintf(pEntry->StaticInfo.Name, "SSID%d", ulIndex);
        sprintf(pEntry->Cfg.Alias, "SSID%d", ulIndex); //RDKB-EMU
	sprintf(pEntry->Cfg.SSID, "SSID%d", ulIndex); //RDKB-EMU
    
        pEntry->Cfg.InstanceNumber    = ulIndex+1;//LNT_EMU
        _ansc_sprintf(pEntry->Cfg.WiFiRadioName, "eth0");
    
        /*indicated by InstanceNumber*/
        CosaDmlWiFiSsidGetCfg((ANSC_HANDLE)hContext,&pEntry->Cfg);
    
        CosaDmlWiFiSsidGetDinfo((ANSC_HANDLE)hContext,pEntry->Cfg.InstanceNumber,&pEntry->DynamicInfo);

	CosaDmlWiFiSsidGetSinfo((ANSC_HANDLE)hContext,pEntry->Cfg.InstanceNumber,&pEntry->StaticInfo);//LNT_EMU

        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS
CosaDmlWiFiSsidSetValues
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIndex,
        ULONG                       ulInstanceNumber,
        char*                       pAlias
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    
    if (/*pPoamWiFiDm*/FALSE)
    {
        return returnStatus;
    }
    else
    {
        return ANSC_STATUS_SUCCESS;
    }
}    

/* Description:
 *	The API adds a new WiFi SSID into the system. 
 * Arguments:
 *	hContext	reserved.
 *	pEntry		Caller pass in the configuration through pEntry->Cfg field and gets back the generated pEntry->StaticInfo.Name, MACAddress, etc.
 */
ANSC_STATUS
CosaDmlWiFiSsidAddEntry
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_SSID_FULL    pEntry
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

    if (/*pPoamWiFiDm*/FALSE)
    {
        return returnStatus;
    }
    else
    {
        gSsidCount++;
        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS
CosaDmlWiFiSsidDelEntry
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

    if (FALSE/*pPoamWiFiDm*/)
    {
        return returnStatus;
    }
    else
    {
        gSsidCount--;
        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS
CosaDmlWiFiSsidSetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_SSID_CFG     pCfg
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

    if (/*pPoamWiFiDm*/FALSE)
    {
        return returnStatus;
    }
    else
    {
        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS
CosaDmlWiFiSsidGetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_SSID_CFG     pCfg
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    char *param_value;//LNT_EMU
    char param_name[256] = {0};

    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
    
    if (/*pPoamWiFiDm*/FALSE)
    {
        return returnStatus;
    }
    else
    {
       // pCfg->bEnabled                 = FALSE;//LNT_EMU
      //  _ansc_sprintf(pCfg->SSID, "test%d", pCfg->InstanceNumber);//LNT_EMU
 	/* memset(param_name, 0, sizeof(param_name));
        sprintf(param_name, HideSsid, pCfg->InstanceNumber);
        PSM_Get_Record_Value2(bus_handle,g_Subsystem, param_name, NULL, &param_value);
        pCfg->bEnabled = (atoi(param_value) == 1) ? TRUE : FALSE;*/
	 //pCfg->LastChange = 1923;
	wifi_getSSIDEnable(pCfg->InstanceNumber,&pCfg->bEnabled);
        if(pCfg->bEnabled == true)
        {
        memset(param_name, 0, sizeof(param_name));
        sprintf(param_name, BssSsid, pCfg->InstanceNumber);
        PSM_Get_Record_Value2(bus_handle,g_Subsystem, param_name, NULL, &param_value);
        AnscCopyString(pCfg->SSID,param_value);

        }
        else
        {
                AnscCopyString(pCfg->SSID,"OutOfService");

        }

        return ANSC_STATUS_SUCCESS;
    }
}

#if 1//LNT_EMU
ANSC_STATUS
CosaDmlWiFiSsidGetSinfo
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber,
        PCOSA_DML_WIFI_SSID_SINFO   pInfo
    )
{
        wifi_getBaseBSSID(ulInstanceNumber,pInfo->BSSID);
        wifi_getSSIDMACAddress(ulInstanceNumber,pInfo->MacAddress);//RDKB-EMULATOR
	//wifi_getSSIDName(ulInstanceNumber,pInfo->Name);
}
#endif

ANSC_STATUS
CosaDmlWiFiSsidGetDinfo
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber,
        PCOSA_DML_WIFI_SSID_DINFO   pInfo
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

    if (!pInfo)
    {
        return ANSC_STATUS_FAILURE;
    }
   
#if 0 //RDKB-EMULATOR
    if (/*pPoamWiFiDm*/FALSE)
    {
        return returnStatus;
    }
    else
    {
        pInfo->LastChange = 1923;
        return ANSC_STATUS_SUCCESS;
    }
#endif
	char wifi_status[50] = {0};
	wifi_getSSIDStatus(ulInstanceNumber,wifi_status);
	if(strcmp(wifi_status,"Enabled") == 0)
		pInfo->Status = COSA_DML_IF_STATUS_Up;
	else
		pInfo->Status = COSA_DML_IF_STATUS_Down;
        return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiSsidGetStats
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber,
        PCOSA_DML_WIFI_SSID_STATS   pStats
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    if (!pStats)
    {
        return ANSC_STATUS_FAILURE;
    }
    
    if (FALSE)
    {
        return returnStatus;
    }
    else
    {
//RDKB-EMULATOR
      /*  pStats->ErrorsSent                  = 234;
        pStats->UnknownProtoPacketsReceived = 56;
        pStats->BytesSent                   = 100;
        pStats->BytesReceived               = 101;
        pStats->PacketsSent                 = 102;
        pStats->PacketsReceived             = 103;
        pStats->ErrorsReceived              = 104;
        pStats->UnicastPacketsSent          = 105;
        pStats->UnicastPacketsReceived      = 106;
        pStats->DiscardPacketsSent          = 107;
        pStats->DiscardPacketsReceived      = 108;
    
        pStats->MulticastPacketsSent        = 109;
        pStats->MulticastPacketsReceived    = 110;
        pStats->BroadcastPacketsSent        = 111;
        pStats->BroadcastPacketsReceived    = 112;*/
	
	wifi_getWifiTrafficStats(ulInstanceNumber,pStats);
   	wifi_getBasicTrafficStats(ulInstanceNumber,pStats);
        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS
CosaDmlWiFiSSIDEncryptionGetCfg
    (
        ANSC_HANDLE                 hContext,
        ULONG                       SSIDInstanceNumber,
        PCOSA_DML_WIFI_SSID_EncryptionInfo pEncryption
    )
{
    return  ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiSSIDEncryptionSetCfg
    (
        ANSC_HANDLE                 hContext,
        ULONG                       SSIDInstanceNumber,
        PCOSA_DML_WIFI_SSID_EncryptionInfo pEncryption
    )
{
    return  ANSC_STATUS_SUCCESS;
}


ANSC_STATUS
CosaDmlWiFiSSIDQoSGetCfg
    (
        ANSC_HANDLE                 hContext,
        ULONG                       SSIDInstanceNumber,
        PCOSA_DML_WIFI_SSID_QosInfo pQos
    )
{
    return  ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiSSIDQoSSetCfg
    (
        ANSC_HANDLE                 hContext,
        ULONG                       SSIDInstanceNumber,
        PCOSA_DML_WIFI_SSID_QosInfo pQos
    )
{
    return  ANSC_STATUS_SUCCESS;
}

ULONG
CosaDmlWiFiSSIDQosSettingGetCount
    (
        ANSC_HANDLE                 hContext,
        ULONG                       SSIDInstanceNumber
    )
{
    return  0;
}

ANSC_STATUS
CosaDmlWiFiSSIDQosSettingGetEntry
    (
        ANSC_HANDLE                 hContext,
        ULONG                       SSIDInstanceNumber,
        ULONG                       nIndex,                  /* Identified by Index */
        PCOSA_DML_WIFI_SSID_QosSetting pQosSetting
    )
{
    return  ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiSSIDQosSettingGetCfg
    (
        ANSC_HANDLE                 hContext,
        ULONG                       SSIDInstanceNumber,
        PCOSA_DML_WIFI_SSID_QosSetting pQosSetting          /* Identified by Instance Number */
    )
{
    return  ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiSSIDQosSettingSetCfg
    (
        ANSC_HANDLE                 hContext,
        ULONG                       SSIDInstanceNumber,
        PCOSA_DML_WIFI_SSID_QosSetting pQosSetting          /* Identified by Instance Number */
    )
{
    return  ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiWPSGetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_WPS          pWPS
    )
{
    return  ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiWPSSetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_WPS          pWPS
    )
{
    return  ANSC_STATUS_SUCCESS;
}


ANSC_STATUS 
CosaDmlWiFi_SetDisconnectClients(char *p)
{
    return  ANSC_STATUS_SUCCESS;
}

ANSC_STATUS 
CosaDmlWiFi_GetDisconnectClients(char *p)
{
    return  ANSC_STATUS_SUCCESS;
}

ANSC_STATUS 
CosaDmlWiFi_GetExtStatus(int *ext_count, ANSC_HANDLE ext_status)
{
    return  ANSC_STATUS_SUCCESS;
}

/* WiFi AP is always associated with a SSID in the system */
static int gApCount = 6;//RDKB-EMU
static ULONG ApinsCount = 0;//LNT_EMU

ULONG
CosaDmlWiFiAPGetNumberOfEntries
    (
        ANSC_HANDLE                 hContext
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

    if (FALSE)
    {
        return 0; 
    }
    else
    {
        return gApCount;
    }
}

ANSC_STATUS
CosaDmlWiFiApGetEntry
    (
        ANSC_HANDLE                 hContext,
        char                        *pSsid,
        PCOSA_DML_WIFI_AP_FULL      pEntry
    )
{
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj      = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntry   = (PSINGLE_LINK_ENTRY       )NULL;
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

    if (FALSE)
    {
        return returnStatus;
    }
    else
    {
        CosaDmlWiFiApGetCfg(NULL, pSsid, &pEntry->Cfg);
        CosaDmlWiFiApGetInfo(NULL, pSsid, &pEntry->Info);
    
        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS
CosaDmlWiFiApSetValues
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIndex,
        ULONG                       ulInstanceNumber,
        char*                       pAlias
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    
    if (FALSE)
    {
        return returnStatus;        
    }
    else
    {
        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS
CosaDmlWiFiApSetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_AP_CFG       pCfg
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

    if (FALSE)
    {
        return returnStatus;
    }
    else
    {
        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS
CosaDmlWiFiApGetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_AP_CFG       pCfg
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
        
    if (FALSE)
    {
        return returnStatus;
    }
    else
    {
        pCfg->bEnabled      = TRUE;
        pCfg->RetryLimit    = 1;

        pCfg->WmmNoAck      = 123;
        pCfg->MulticastRate = 123;
        pCfg->BssMaxNumSta  = 123;
        pCfg->BssCountStaAsCpe  = TRUE;
        pCfg->BssHotSpot    = TRUE;
//	pCfg->InstanceNumber = 1; //LNT_EMU
	pCfg->InstanceNumber = (( pSsid[strlen(pSsid)-1] ) - '0') +1;
        ApinsCount = pCfg->InstanceNumber;//LNT_EMU 
	wifi_getApSsidAdvertisementEnable(pCfg->InstanceNumber,&pCfg->SSIDAdvertisementEnabled);//LNT_EMU
        printf(" Instance Number = %d\n",pCfg->InstanceNumber);
	if(pCfg->InstanceNumber == 1)
        AnscCopyString(pCfg->SSID, "Device.WiFi.SSID.1.");
	else if(pCfg->InstanceNumber == 2)
        AnscCopyString(pCfg->SSID, "Device.WiFi.SSID.2.");
	else if(pCfg->InstanceNumber == 3)
        AnscCopyString(pCfg->SSID, "Device.WiFi.SSID.3.");
	else if(pCfg->InstanceNumber == 4)
        AnscCopyString(pCfg->SSID, "Device.WiFi.SSID.4.");
	else if(pCfg->InstanceNumber == 5)
        AnscCopyString(pCfg->SSID, "Device.WiFi.SSID.5.");
	else if(pCfg->InstanceNumber == 6)
        AnscCopyString(pCfg->SSID, "Device.WiFi.SSID.6.");
 
        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS
CosaDmlWiFiApGetInfo
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_AP_INFO      pInfo
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

    if (!pInfo)
    {
        return ANSC_STATUS_FAILURE;
    }
    
    if (FALSE)
    {
        return returnStatus;
    }
    else
    {
        pInfo->WMMCapability = TRUE;
    
        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS
CosaDmlWiFiApSecGetEntry
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APSEC_FULL   pEntry
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

    if (!pEntry)
    {
        return ANSC_STATUS_FAILURE;
    }
    
    if (FALSE)
    {
        return returnStatus;
    }
    else
    {
        //pEntry->Info.ModesSupported = COSA_DML_WIFI_SECURITY_WEP_64 | COSA_DML_WIFI_SECURITY_WEP_128;
	pEntry->Info.ModesSupported = (COSA_DML_WIFI_SECURITY_None | COSA_DML_WIFI_SECURITY_WEP_64 | COSA_DML_WIFI_SECURITY_WEP_128 | COSA_DML_WIFI_SECURITY_WPA_Personal | COSA_DML_WIFI_SECURITY_WPA2_Personal | COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal| COSA_DML_WIFI_SECURITY_WPA_Enterprise | COSA_DML_WIFI_SECURITY_WPA2_Enterprise | COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise);
        
        CosaDmlWiFiApSecGetCfg((ANSC_HANDLE)hContext, NULL, &pEntry->Cfg);
    
        return ANSC_STATUS_SUCCESS;
    }
}

static COSA_DML_WIFI_APSEC_CFG g_WiFiApSecCfg = {
    .ModeEnabled            = COSA_DML_WIFI_SECURITY_WEP_64,
    .WEPKeyp                = "1122334455667788",
    .PreSharedKey           = "012345678",
    .KeyPassphrase          = "PassPhrase",
    .RekeyingInterval       = 3600,
    .EncryptionMethod       = COSA_DML_WIFI_AP_SEC_TKIP,
    .RadiusServerIPAddr.Dot = {10, 74, 120, 1},
    .RadiusServerPort       = 9877,
    .RadiusSecret           = "Secret",
    .RadiusReAuthInterval   = 1000,
    .DefaultKey             = 1234,
};

ANSC_STATUS
CosaDmlWiFiApSecGetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APSEC_CFG    pCfg
    )
{
	char 			    password[50];
	if (!pCfg)
		return ANSC_STATUS_FAILURE;

	memcpy(pCfg, &g_WiFiApSecCfg, sizeof(COSA_DML_WIFI_APSEC_CFG));
#if 1 //LNT_EMU
	wifi_getApSecurityPreSharedKey(ApinsCount,password); //RDKB-EMU-L
        if((ApinsCount == 1) || (ApinsCount == 2))
        {
        if(strcmp(password,"")==0)
        {
                pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_None;
        }
        else
        {
                pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_WPA_Personal;
                pCfg->EncryptionMethod = COSA_DML_WIFI_AP_SEC_TKIP;
                AnscCopyString(pCfg->KeyPassphrase,password);

        }
        }
        if((ApinsCount == 5) || (ApinsCount == 6))//LNT_EMU
        {
                pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_None;
                AnscCopyString(pCfg->KeyPassphrase,"");
        }

#endif

	return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiApSecSetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APSEC_CFG    pCfg
    )
{
    if (!pCfg)
        return ANSC_STATUS_FAILURE;

    memcpy(&g_WiFiApSecCfg, pCfg, sizeof(COSA_DML_WIFI_APSEC_CFG));
    return ANSC_STATUS_SUCCESS;
}

/*not called*/
ANSC_STATUS
CosaDmlWiFiApWpsGetEntry
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APWPS_FULL   pEntry
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

    if (FALSE)
    {
        return returnStatus;
    }
    else
    {
    
        return ANSC_STATUS_SUCCESS;
    }

}

ANSC_STATUS
CosaDmlWiFiApWpsSetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APWPS_CFG    pCfg
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

    if (FALSE)
    {
        return returnStatus;
    }
    else
    {
    
        return ANSC_STATUS_SUCCESS;
    }

}

ANSC_STATUS
CosaDmlWiFiApWpsGetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APWPS_CFG    pCfg
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    
    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }

    if (FALSE)
    {
        return returnStatus;
    }
    else
    {
        pCfg->ConfigMethodsEnabled = COSA_DML_WIFI_WPS_METHOD_Ethernet;
    
        return ANSC_STATUS_SUCCESS;
    }

    
}

/* Description:
 *	This routine is to retrieve the complete list of currently associated WiFi devices, 
 *	which is a dynamic table.
 * Arguments:
 *	pName   		Indicate which SSID to operate on.
 *	pulCount		To receive the actual number of entries.
 * Return:
 * The pointer to the array of WiFi associated devices, allocated by callee. 
 * If no entry is found, NULL is returned.
 */
PCOSA_DML_WIFI_AP_ASSOC_DEVICE
CosaDmlWiFiApGetAssocDevices
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PULONG                      pulCount
    )
{
	PCOSA_DML_WIFI_AP_ASSOC_DEVICE  AssocDeviceArray  = (PCOSA_DML_WIFI_AP_ASSOC_DEVICE)NULL;
	ULONG                           index             = 0;
	ULONG                           ulCount           = 0;
	wifi_device_t *wlanDevice = NULL;
	int InstanceNumber = 0;
	InstanceNumber = (( pSsid[strlen(pSsid)-1] ) - '0') +1;
	
	if (FALSE)
	{
		*pulCount = ulCount;

		return AssocDeviceArray;
	}
	else
	{
		wifi_getAllAssociatedDeviceDetail(InstanceNumber,pulCount,&wlanDevice);//LNT_EMU

		/*For example we have 5 AssocDevices*/
		// *pulCount = 5;//LNT_EMU
		AssocDeviceArray = (PCOSA_DML_WIFI_AP_ASSOC_DEVICE)AnscAllocateMemory(sizeof(COSA_DML_WIFI_AP_ASSOC_DEVICE)*(*pulCount));

		if ( !AssocDeviceArray )
		{
			*pulCount = 0;
			return NULL;
		}

		for(index = 0; index < *pulCount; index++)
		{
			AssocDeviceArray[index].MacAddress[0]        = wlanDevice[index].wifi_devMacAddress[0];
			AssocDeviceArray[index].MacAddress[1]        = wlanDevice[index].wifi_devMacAddress[1];
			AssocDeviceArray[index].MacAddress[2]        = wlanDevice[index].wifi_devMacAddress[2];
			AssocDeviceArray[index].MacAddress[3]        = wlanDevice[index].wifi_devMacAddress[3];
			AssocDeviceArray[index].MacAddress[4]        = wlanDevice[index].wifi_devMacAddress[4];
			AssocDeviceArray[index].MacAddress[5]        = wlanDevice[index].wifi_devMacAddress[5];
			AssocDeviceArray[index].SignalStrength       = wlanDevice[index].wifi_devSignalStrength;
			AssocDeviceArray[index].AuthenticationState  = TRUE;
			AssocDeviceArray[index].LastDataDownlinkRate = 200+index;
			AssocDeviceArray[index].LastDataUplinkRate   = 100+index;
		      //AssocDeviceArray[index].SignalStrength       = 50+index;//LNT_EMU
			AssocDeviceArray[index].Retransmissions      = 20+index;
			AssocDeviceArray[index].Active               = TRUE;
		}

		return AssocDeviceArray;
	}
}

ANSC_STATUS
CosaDmlWiFiApMfGetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_AP_MF_CFG    pCfg
    )
{
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiApMfSetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_AP_MF_CFG    pCfg
    )
{
    return ANSC_STATUS_SUCCESS;
}

int updateMacFilter(char *Operation)
{
        int i,Count=0;
        struct hostDetails hostlist[MAX_NUM_HOST];
        LM_hosts_t hosts;
	int ret;
        if(!ServiceTab)
        {
		do_MacFilter_Startrule();
                ServiceTab=1;
        }
	do_MacFilter_Flushrule();
	if(!strcmp(Operation,"ACCEPT"))
        {
		 ret =  lm_get_all_hosts(&hosts);
		 if( ret == LM_RET_SUCCESS )
                 {
                        for(i = 0; i < hosts.count; i++)
                        {
                              /* filter unwelcome device */
                              sprintf(hostlist[i].hostName,"%02x:%02x:%02x:%02x:%02x:%02x", hosts.hosts[i].phyAddr[0], hosts.hosts[i].phyAddr[1], hosts.hosts[i].phyAddr[2], hosts.hosts[i].phyAddr[3], hosts.hosts[i].phyAddr[4], hosts.hosts[i].phyAddr[5]);
                              if (strlen(hosts.hosts[i].l1IfName) > (sizeof(hostlist[i].InterfaceType)-1))
                              {
                                  AnscTraceError(("%s: l1IfName is bigger than destination.", __FUNCTION__));
                              }
                              strncpy(hostlist[i].InterfaceType,hosts.hosts[i].l1IfName,sizeof(hostlist[i].InterfaceType)-1);
                         }
			 Count = hosts.count;
                }

        }
        do_MacFilter_Update(Operation, g_macFiltCnt,&g_macFiltTab,Count,&hostlist);
	return ANSC_STATUS_SUCCESS;
}


ANSC_STATUS
CosaDmlWiFi_GetWEPKey64ByIndex(ULONG apIns, ULONG keyIdx, PCOSA_DML_WEPKEY_64BIT pWepKey)
{
    /* dummy data for simu */
    static char wepKey64[COSA_DML_WEP_KEY_NUM][64 / 8 * 2 + 1];

    if (apIns != 1) /* only support 1 ap in simu */
        return ANSC_STATUS_FAILURE;

    snprintf((char*)pWepKey->WEPKey, sizeof(pWepKey->WEPKey), "%s", wepKey64[keyIdx]);
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_SetWEPKey64ByIndex(ULONG apIns, ULONG keyIdx, PCOSA_DML_WEPKEY_64BIT pWepKey)
{
    /* dummy data for simu */
    static char wepKey64[COSA_DML_WEP_KEY_NUM][64 / 8 * 2 + 1];

    if (apIns != 1) /* only support 1 ap in simu */
        return ANSC_STATUS_FAILURE;

    snprintf(wepKey64[keyIdx], sizeof(wepKey64[keyIdx]), "%s", pWepKey->WEPKey);
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_GetWEPKey128ByIndex(ULONG apIns, ULONG keyIdx, PCOSA_DML_WEPKEY_128BIT pWepKey)
{
    /* dummy data for simu */
    static char wepKey128[COSA_DML_WEP_KEY_NUM][128 / 8 * 2 + 1];

    if (apIns != 1) /* only support 1 ap in simu */
        return ANSC_STATUS_FAILURE;

    snprintf((char *)pWepKey->WEPKey, sizeof(pWepKey->WEPKey), "%s", wepKey128[keyIdx]);
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_SetWEPKey128ByIndex(ULONG apIns, ULONG keyIdx, PCOSA_DML_WEPKEY_128BIT pWepKey)
{
    /* dummy data for simu */
    static char wepKey128[COSA_DML_WEP_KEY_NUM][128 / 8 * 2 + 1];

    if (apIns != 1) /* only support 1 ap in simu */
        return ANSC_STATUS_FAILURE;

    snprintf(wepKey128[keyIdx], sizeof(wepKey128[keyIdx]), "%s", pWepKey->WEPKey);
    return ANSC_STATUS_SUCCESS;
}


ULONG
CosaDmlMacFilt_GetNumberOfEntries(ULONG apIns)
{
    if (apIns != 1) /* only support 1 ap in simu */
        return ANSC_STATUS_FAILURE;

    return g_macFiltCnt;
}

ANSC_STATUS
CosaDmlMacFilt_GetEntryByIndex(ULONG apIns, ULONG index, PCOSA_DML_WIFI_AP_MAC_FILTER pMacFilt)
{
    if (apIns != 1) /* only support 1 ap in simu */
        return ANSC_STATUS_FAILURE;

    if (index >= g_macFiltCnt)
        return ANSC_STATUS_FAILURE;

    memcpy(pMacFilt, &g_macFiltTab[index], sizeof(COSA_DML_WIFI_AP_MAC_FILTER));

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlMacFilt_SetValues(ULONG apIns, ULONG index, ULONG ins, char *Alias)
{
    int i;

    if (apIns != 1) /* only support 1 ap in simu */
        return ANSC_STATUS_FAILURE;

    if (index >= g_macFiltCnt)
        return ANSC_STATUS_FAILURE;


    g_macFiltTab[index].InstanceNumber = ins;
    snprintf(g_macFiltTab[index].Alias, sizeof(g_macFiltTab[index].Alias),
            "%s", Alias);

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlMacFilt_AddEntry(ULONG apIns, PCOSA_DML_WIFI_AP_MAC_FILTER pMacFilt)
{
    if (apIns != 1) /* only support 1 ap in simu */
        return ANSC_STATUS_FAILURE;

    if (g_macFiltCnt >= MAX_MAC_FILT)
        return ANSC_STATUS_FAILURE;

    memcpy(&g_macFiltTab[g_macFiltCnt], pMacFilt, sizeof(COSA_DML_WIFI_AP_MAC_FILTER));
    g_macFiltCnt++;
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlMacFilt_DelEntry(ULONG apIns, ULONG macFiltIns)
{
    int i;

    if (apIns != 1) /* only support 1 ap in simu */
        return ANSC_STATUS_FAILURE;

    for (i = 0; i < g_macFiltCnt; i++)
    {
        if (g_macFiltTab[i].InstanceNumber == macFiltIns)
            break;
    }
    if (i == g_macFiltCnt)
        return ANSC_STATUS_FAILURE;

    memmove(&g_macFiltTab[i], &g_macFiltTab[i+1], 
            (g_macFiltCnt - i - 1) * sizeof(COSA_DML_WIFI_AP_MAC_FILTER));
    g_macFiltCnt--;

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlMacFilt_GetConf(ULONG apIns, ULONG macFiltIns, PCOSA_DML_WIFI_AP_MAC_FILTER pMacFilt)
{
    int i;

    if (apIns != 1) /* only support 1 ap in simu */
        return ANSC_STATUS_FAILURE;

    for (i = 0; i < g_macFiltCnt; i++)
    {
        if (g_macFiltTab[i].InstanceNumber == macFiltIns)
            break;
    }
    if (i == g_macFiltCnt)
        return ANSC_STATUS_FAILURE;

    memcpy(pMacFilt, &g_macFiltTab[i], sizeof(COSA_DML_WIFI_AP_MAC_FILTER));
    pMacFilt->InstanceNumber = macFiltIns; /* just in case */

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlMacFilt_SetConf(ULONG apIns, ULONG macFiltIns, PCOSA_DML_WIFI_AP_MAC_FILTER pMacFilt)
{
    int i;

    if (apIns != 1) /* only support 1 ap in simu */
        return ANSC_STATUS_FAILURE;

    for (i = 0; i < g_macFiltCnt; i++)
    {
        if (g_macFiltTab[i].InstanceNumber == macFiltIns)
            break;
    }
    if (i == g_macFiltCnt)
        return ANSC_STATUS_FAILURE;

    memcpy(&g_macFiltTab[i], pMacFilt, sizeof(COSA_DML_WIFI_AP_MAC_FILTER));
    g_macFiltTab[i].InstanceNumber = macFiltIns; /* just in case */

    return ANSC_STATUS_SUCCESS;
}

#define WIFI_CONF_FILE      "./wifi.conf"

/*
 * @buf [OUT], buffer to save config file
 * @size [IN-OUT], buffer size as input and config file size as output
 */
ANSC_STATUS 
CosaDmlWiFi_GetConfigFile(void *buf, int *size)
{
    FILE *fp;
    struct stat st;

    if (!buf || !size)
        return ANSC_STATUS_FAILURE;

    if (stat(WIFI_CONF_FILE, &st) != 0)
    {
        AnscTraceError(("%s: fail to stat file %s\n", __FUNCTION__, WIFI_CONF_FILE));
        return ANSC_STATUS_FAILURE;
    }
    if (st.st_size > *size)
    {
        AnscTraceError(("%s: config file too big\n", __FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
    *size = st.st_size;

    fp = fopen(WIFI_CONF_FILE, "rb");
    if (fp == NULL)
    {
        AnscTraceError(("%s: fail to open file\n", __FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }

    if (fread(buf, st.st_size, 1, fp) != 1)
    {
        AnscTraceError(("%s: fail to read file\n", __FUNCTION__));
        fclose(fp);
        return ANSC_STATUS_FAILURE;
    }

    fclose(fp);
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS 
CosaDmlWiFi_SetConfigFile(const void *buf, int size)
{
    FILE *fp;

    if (!buf)
        return ANSC_STATUS_FAILURE;

    fp = fopen(WIFI_CONF_FILE, "wb");
    if (fp == NULL)
    {
        AnscTraceError(("%s: fail to open file: %s\n", __FUNCTION__, WIFI_CONF_FILE));
        return ANSC_STATUS_FAILURE;
    }

    if (fwrite(buf, size, 1, fp) != 1)
    {
        AnscTraceError(("%s: fail to write file\n", __FUNCTION__));
        fclose(fp);
        return ANSC_STATUS_FAILURE;
    }

    fclose(fp);
    return ANSC_STATUS_SUCCESS;
}
typedef struct DefaultWiFiSettings//RDKB_EMULATOR
{
        char SSID[20];
        UCHAR PassKey[20];
	ULONG Channel;
}g_WiFiApDefCfg;

g_WiFiApDefCfg DEFAULT_2_4G = { {"RDKB-EMU-2.4G"}, {"password"},{6} };
g_WiFiApDefCfg DEFAULT_5G = { {"RDKB-EMU-5G"}, {"5g-password"},{36} };

ANSC_STATUS
CosaDmlWiFi_FactoryReset(void)
{//RDKB_EMULATOR
        fprintf(stderr, "WiFi resetting ...\n");
        int instanceNumber = 0;
        char SSIDName[256];
        char PassKey[256];
        char ChannelCount[256];
        char *strValue = NULL;
        char *recValue = NULL;
        char *paramValue = NULL;
        int retPsmGet_SSID = CCSP_SUCCESS;
        int retPsmGet_PassKey = CCSP_SUCCESS;
        int retPsmGet_Channel = CCSP_SUCCESS;
        for (instanceNumber = 1; instanceNumber <= gRadioCount; instanceNumber++)
        {
                if(instanceNumber == 1){ //Restore default values for pivate wifi 2.4G
                        memset(SSIDName, 0, sizeof(SSIDName));
                        sprintf(SSIDName, BssSsid , instanceNumber);
                        retPsmGet_SSID = PSM_Get_Record_Value2(bus_handle,g_Subsystem, SSIDName, NULL, &strValue);

                        memset(PassKey, 0, sizeof(PassKey));
                        sprintf(PassKey, Passphrase , instanceNumber);
                        retPsmGet_PassKey = PSM_Get_Record_Value2(bus_handle,g_Subsystem, PassKey, NULL, &recValue);

                        memset(ChannelCount, 0, sizeof(ChannelCount));
                        sprintf(ChannelCount, ChannelNumber, instanceNumber);
                        retPsmGet_Channel =  PSM_Get_Record_Value2(bus_handle,g_Subsystem, ChannelCount,NULL, &paramValue);


                        if (retPsmGet_SSID == CCSP_SUCCESS && retPsmGet_PassKey == CCSP_SUCCESS && retPsmGet_Channel == CCSP_SUCCESS ) {
                                /* Restore WiFi Factory settings */

                                strcpy(strValue, DEFAULT_2_4G.SSID);
                                strcpy(recValue, DEFAULT_2_4G.PassKey);
                                sprintf(paramValue,"%d",DEFAULT_2_4G.Channel);

                                wifi_setSSIDName(instanceNumber,strValue);
                                wifi_setApSecurityPreSharedKey(instanceNumber,recValue);
                                wifi_setRadioChannel(instanceNumber, atoi(paramValue));

				/*PSM ACCESS*/

                                memset(SSIDName, 0, sizeof(SSIDName));
                                sprintf(SSIDName, BssSsid, instanceNumber);
                                PSM_Set_Record_Value2(bus_handle,g_Subsystem, SSIDName, ccsp_string, strValue);

                                memset(PassKey, 0, sizeof(PassKey));
                                sprintf(PassKey, Passphrase , instanceNumber);
                                PSM_Set_Record_Value2(bus_handle,g_Subsystem,  PassKey, ccsp_string, recValue);

                                memset(ChannelCount, 0, sizeof(ChannelCount));
                                sprintf(ChannelCount, ChannelNumber, instanceNumber);
                                PSM_Set_Record_Value2(bus_handle,g_Subsystem,  ChannelCount, ccsp_string, paramValue);
                                /*restart WiFi with Default Configurations*/

                                wifi_stopHostApd();
                                wifi_startHostApd();
                        }
                }
                else if(instanceNumber == 2){ //Restore default values to private wifi 5G
                        memset(SSIDName, 0, sizeof(SSIDName));
                        sprintf(SSIDName, BssSsid , instanceNumber);
                        retPsmGet_SSID = PSM_Get_Record_Value2(bus_handle,g_Subsystem, SSIDName, NULL, &strValue);

                        memset(PassKey, 0, sizeof(PassKey));
                        sprintf(PassKey, Passphrase , instanceNumber);
                        retPsmGet_PassKey = PSM_Get_Record_Value2(bus_handle,g_Subsystem, PassKey, NULL, &recValue);

                        memset(ChannelCount, 0, sizeof(ChannelCount));
                        sprintf(ChannelCount, ChannelNumber, instanceNumber);
                        retPsmGet_Channel =  PSM_Get_Record_Value2(bus_handle,g_Subsystem, ChannelCount,NULL, &paramValue);


                        if (retPsmGet_SSID == CCSP_SUCCESS && retPsmGet_PassKey == CCSP_SUCCESS && retPsmGet_Channel == CCSP_SUCCESS ) {
                                /* Restore WiFi Factory settings */

                                strcpy(strValue, DEFAULT_5G.SSID);
                                strcpy(recValue, DEFAULT_5G.PassKey);
                                sprintf(paramValue,"%d",DEFAULT_5G.Channel);

                                wifi_setSSIDName(instanceNumber,strValue);
                                wifi_setApSecurityPreSharedKey(instanceNumber,recValue);
                                wifi_setRadioChannel(instanceNumber, atoi(paramValue));

                                /*PSM ACCESS*/

                                memset(SSIDName, 0, sizeof(SSIDName));
                                sprintf(SSIDName, BssSsid, instanceNumber);
                                PSM_Set_Record_Value2(bus_handle,g_Subsystem, SSIDName, ccsp_string, strValue);

                                memset(PassKey, 0, sizeof(PassKey));
				sprintf(PassKey, Passphrase , instanceNumber);
                                PSM_Set_Record_Value2(bus_handle,g_Subsystem,  PassKey, ccsp_string, recValue);

                                memset(ChannelCount, 0, sizeof(ChannelCount));
                                sprintf(ChannelCount, ChannelNumber, instanceNumber);
                                PSM_Set_Record_Value2(bus_handle,g_Subsystem,  ChannelCount, ccsp_string, paramValue);
                                /*restart WiFi with Default Configurations*/
                                KillHostapd_5g();
                        }

                }
                else {
                        return 0;
                }
        }
        return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_RadioUpdated()
{
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_SSIDUpdated()
{
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_WPSUpdated()
{
    return ANSC_STATUS_SUCCESS;
}

#endif

