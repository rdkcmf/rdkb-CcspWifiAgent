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
#include "cosa_dbus_api.h"
#include "cosa_wifi_apis.h"
#include "cosa_wifi_internal.h"
#include "plugin_main_apis.h"
#include "wifi_hal.h"
#include "ccsp_psm_helper.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "ansc_platform.h"
#include "pack_file.h"

/**************************************************************************
*
*	Function Declarations
*
**************************************************************************/
ANSC_STATUS CosaDmlWiFiApMfPushCfg(PCOSA_DML_WIFI_AP_MF_CFG pCfg, ULONG wlanIndex);
ANSC_STATUS CosaDmlWiFiApWpsApplyCfg(PCOSA_DML_WIFI_APWPS_CFG pCfg, ULONG index);
ANSC_STATUS CosaDmlWiFiApSecPushCfg(PCOSA_DML_WIFI_APSEC_CFG pCfg, ULONG instanceNumber);
ANSC_STATUS CosaDmlWiFiApSecApplyCfg(PCOSA_DML_WIFI_APSEC_CFG pCfg, ULONG instanceNumber);
ANSC_STATUS CosaDmlWiFiApSecApplyWepCfg(PCOSA_DML_WIFI_APSEC_CFG pCfg, ULONG instanceNumber);
ANSC_STATUS CosaDmlWiFiApPushCfg (PCOSA_DML_WIFI_AP_CFG pCfg);
ANSC_STATUS CosaDmlWiFiApPushMacFilter(QUEUE_HEADER *pMfQueue, ULONG wlanIndex);
ANSC_STATUS CosaDmlWiFiSsidApplyCfg(PCOSA_DML_WIFI_SSID_CFG pCfg);
ANSC_STATUS CosaDmlWiFiApApplyCfg(PCOSA_DML_WIFI_AP_CFG pCfg);

/**************************************************************************
*
*	Function Definitions
*
**************************************************************************/
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
static int gRadioCount = 1;
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

#if 1
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
        sprintf(pWifiRadio->Cfg.Alias, "Radio%d", ulIndex);
        
        pWifiRadio->StaticInfo.bUpstream               = TRUE;
        pWifiRadio->StaticInfo.MaxBitRate              = 128+ulIndex;
        pWifiRadio->StaticInfo.SupportedFrequencyBands = COSA_DML_WIFI_FREQ_BAND_2_4G;                   /* Bitmask of COSA_DML_WIFI_FREQ_BAND */
        pWifiRadio->StaticInfo.SupportedStandards      = COSA_DML_WIFI_STD_b | COSA_DML_WIFI_STD_g;      /* Bitmask of COSA_DML_WIFI_STD */
        AnscCopyString(pWifiRadio->StaticInfo.PossibleChannels, "1,2,3,4,5,6,7,8,9,10,11");
        pWifiRadio->StaticInfo.AutoChannelSupported    = TRUE;
        AnscCopyString(pWifiRadio->StaticInfo.TransmitPowerSupported, "10,20,50,100");
        pWifiRadio->StaticInfo.IEEE80211hSupported     = TRUE;

        CosaDmlWiFiRadioGetCfg(hContext, pWifiRadioCfg);
        CosaDmlWiFiRadioGetDinfo(hContext, pWifiRadioCfg->InstanceNumber, pWifiRadioDinfo);    

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

    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
    
        pCfg->bEnabled                       = TRUE;
        pCfg->LastChange                   = 123456;
        pCfg->OperatingFrequencyBand         = COSA_DML_WIFI_FREQ_BAND_2_4G;
        pCfg->OperatingStandards             = COSA_DML_WIFI_STD_b | COSA_DML_WIFI_STD_g;         /* Bitmask of COSA_DML_WIFI_STD */
        pCfg->Channel                        = 1;
        pCfg->AutoChannelEnable              = TRUE;
        pCfg->AutoChannelRefreshPeriod       = 3600;
        pCfg->OperatingChannelBandwidth      = COSA_DML_WIFI_CHAN_BW_20M;
        pCfg->ExtensionChannel               = COSA_DML_WIFI_EXT_CHAN_Above;
        pCfg->GuardInterval                  = COSA_DML_WIFI_GUARD_INTVL_400ns;
        pCfg->MCS                            = 1;
        pCfg->TransmitPower                  = 100;
        pCfg->IEEE80211hEnabled              = TRUE;
        AnscCopyString(pCfg->RegulatoryDomain, "COM");
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
        pCfg->ApplySetting                  = TRUE;
        pCfg->ApplySettingSSID = 0;
    
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
        AnscCopyString(pInfo->ChannelsInUse, "1");
        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS
CosaDmlWiFiRadioGetChannelsInUse
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
        AnscCopyString(pInfo->ChannelsInUse,"1");

        return ANSC_STATUS_SUCCESS;
    }
}
ANSC_STATUS
CosaDmlWiFiRadioGetApChannelScan
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
        AnscCopyString(pInfo->ApChannelScan ,"HOME-10C4-2.4|WPA/WPA2-PSK AES-CCMP TKIP|802.11b/g/n|-63|6|70:54:D2:00:AA:50");
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
static int gSsidCount = 1;
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
    
        pEntry->Cfg.InstanceNumber    = ulIndex;
        _ansc_sprintf(pEntry->Cfg.WiFiRadioName, "eth0");
    
        /*indicated by InstanceNumber*/
        CosaDmlWiFiSsidGetCfg((ANSC_HANDLE)hContext,&pEntry->Cfg);
    
        CosaDmlWiFiSsidGetDinfo((ANSC_HANDLE)hContext,pEntry->Cfg.InstanceNumber,&pEntry->DynamicInfo);

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
        pCfg->bEnabled                 = FALSE;
        pCfg->LastChange = 1923;
        _ansc_sprintf(pCfg->SSID, "test%d", pCfg->InstanceNumber);
        return ANSC_STATUS_SUCCESS;
    }
}

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
        pStats->ErrorsSent                  = 234;
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
        pStats->BroadcastPacketsReceived    = 112;
    
        return ANSC_STATUS_SUCCESS;
    }
}

/* WiFi AP is always associated with a SSID in the system */
static int gApCount = 1;
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
        AnscCopyString(pCfg->SSID, "Device.WiFi.SSID.1.");

        pCfg->WmmNoAck      = 123;
        pCfg->MulticastRate = 123;
        pCfg->BssMaxNumSta  = 128;
        pCfg->BssCountStaAsCpe  = TRUE;
        pCfg->BssHotSpot    = TRUE;
    
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

        pEntry->Info.ModesSupported = COSA_DML_WIFI_SECURITY_WEP_64 | COSA_DML_WIFI_SECURITY_WEP_128;
        
        CosaDmlWiFiApSecGetCfg((ANSC_HANDLE)hContext, NULL, &pEntry->Cfg);

        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS
CosaDmlWiFiApSecGetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APSEC_CFG    pCfg
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
        AnscCopyString(pCfg->WEPKeyp, "1234");
    
        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS
CosaDmlWiFiApSecSetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APSEC_CFG    pCfg
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
    
    if (FALSE)
    {
        *pulCount = ulCount;
        
        return AssocDeviceArray;
    }
    else
    {
        /*For example we have 5 AssocDevices*/
        *pulCount = 5;
        AssocDeviceArray = (PCOSA_DML_WIFI_AP_ASSOC_DEVICE)AnscAllocateMemory(sizeof(COSA_DML_WIFI_AP_ASSOC_DEVICE)*(*pulCount));
    
        if ( !AssocDeviceArray )
        {
            *pulCount = 0;
            return NULL;
        }
    
        for(index = 0; index < *pulCount; index++)
        {
            AssocDeviceArray[index].AuthenticationState  = TRUE;
            AssocDeviceArray[index].LastDataDownlinkRate = 200+index;
            AssocDeviceArray[index].LastDataUplinkRate   = 100+index;
            AssocDeviceArray[index].SignalStrength       = 50+index;
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

#define MAX_MAC_FILT                16

static int                          g_macFiltCnt = 1;
static COSA_DML_WIFI_AP_MAC_FILTER  g_macFiltTab[MAX_MAC_FILT] = {
    { 1, "MacFilterTable1", "00:1a:2b:aa:bb:cc" },
};

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

#elif (_COSA_DRG_CNS_)

#include <utctx/utctx.h>
#include <utctx/utctx_api.h>
#include <utapi/utapi.h>
#include <utapi/utapi_util.h>

ANSC_STATUS
CosaDmlWiFiInit
    (
        ANSC_HANDLE                 hDml,
        PANSC_HANDLE                phContext
    )
{
    return ANSC_STATUS_SUCCESS;
}

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
    return Utopia_GetWifiRadioInstances();
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

    int rc = -1;
    UtopiaContext ctx;

    CosaDmlWiFiRadioSetDefaultCfgValues(NULL,ulIndex,pWifiRadioCfg); /* Fill the default values for Cfg */
    if(pEntry)
    {
        /* Initialize a Utopia Context */
        if(!Utopia_Init(&ctx))
           return ANSC_STATUS_FAILURE;

        rc = Utopia_GetWifiRadioEntry(&ctx, ulIndex, pEntry);

        /* Free Utopia Context */
        Utopia_Free(&ctx,0);
    }
    if (rc != 0)
       return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
}


ANSC_STATUS
CosaDmlWiFiRadioSetDefaultCfgValues
    (
        ANSC_HANDLE                 hContext,
        unsigned long               ulIndex,
	PCOSA_DML_WIFI_RADIO_CFG    pCfg
    )
{
    if(0 == ulIndex)
    {
	strcpy(pCfg->Alias,"wl0");
	pCfg->OperatingFrequencyBand = COSA_DML_WIFI_FREQ_BAND_2_4G;
	pCfg->OperatingStandards = COSA_DML_WIFI_STD_b | COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n;
        pCfg->OperatingChannelBandwidth = COSA_DML_WIFI_CHAN_BW_20M;
     }else
     {
        strcpy(pCfg->Alias,"wl1");
        pCfg->OperatingFrequencyBand = COSA_DML_WIFI_FREQ_BAND_5G;
        pCfg->OperatingStandards = COSA_DML_WIFI_STD_a | COSA_DML_WIFI_STD_n;
        pCfg->OperatingChannelBandwidth = COSA_DML_WIFI_CHAN_BW_40M;
     }
    pCfg->bEnabled = TRUE;
    pCfg->Channel = 0;
    pCfg->AutoChannelEnable = TRUE;
    pCfg->AutoChannelRefreshPeriod = (15*60);
    pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Above;
    pCfg->GuardInterval = COSA_DML_WIFI_GUARD_INTVL_Auto;
    pCfg->MCS = -1;
    pCfg->TransmitPower = 100; /* High */
    pCfg->IEEE80211hEnabled = FALSE;
    strcpy(pCfg->RegulatoryDomain,"US");
    pCfg->APIsolation = FALSE;
    pCfg->FrameBurst = TRUE;
    pCfg->TxRate = COSA_DML_WIFI_TXRATE_Auto;
    pCfg->CTSProtectionMode = TRUE;
    pCfg->BeaconInterval = 100;
    pCfg->DTIMInterval = 1;
    pCfg->FragmentationThreshold = 2346;
    pCfg->RTSThreshold = 2347;  
   
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
    UtopiaContext ctx;
    int rc = -1;

    /* Initialize a Utopia Context */
    if(!Utopia_Init(&ctx))
        return ANSC_STATUS_FAILURE;
    rc = Utopia_WifiRadioSetValues(&ctx,ulIndex,ulInstanceNumber,pAlias);

    /* Free Utopia Context */
    Utopia_Free(&ctx,!rc);

    if (rc != 0)
       return ANSC_STATUS_FAILURE;
    else
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
    /*PPOAM_COSAWIFIDM_OBJECT         pPoamWiFiDm    = (PPOAM_COSAWIFIDM_OBJECT )hContext;*/
    ULONG ulAutoChannelCycle = 0;

    UtopiaContext ctx;
    int rc = -1;

    if(pCfg)
    {
        /* Initialize a Utopia Context */
        if(!Utopia_Init(&ctx))
           return ANSC_STATUS_FAILURE;

        rc = Utopia_SetWifiRadioCfg(&ctx,pCfg);

        /* Set WLAN Restart event */
        Utopia_SetEvent(&ctx,Utopia_Event_WLAN_Restart);

        /* Free Utopia Context */
        Utopia_Free(&ctx,!rc);
    }

    if (rc != 0)
        return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiRadioGetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_RADIO_CFG    pCfg        /* Identified by InstanceNumber */
    )
{
    UtopiaContext ctx;
    int rc = -1;

    if(pCfg)
    {
        /* Initialize a Utopia Context */
        if(!Utopia_Init(&ctx))
           return ANSC_STATUS_FAILURE;

        rc = Utopia_GetWifiRadioCfg(&ctx,0,pCfg);

        /* Free Utopia Context */
        Utopia_Free(&ctx,0);
    }

    if (rc != 0)
        return ANSC_STATUS_FAILURE;
    else
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
    int rc = -1;
    if(pInfo)
    {
        rc = Utopia_GetWifiRadioDinfo(ulInstanceNumber,pInfo);
    }

    if (rc != 0)
       return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiRadioGetChannelsInUse
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
        AnscCopyString(pInfo->ChannelsInUse,"1");

        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS
CosaDmlWiFiRadioGetApChannelScan
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
        AnscCopyString(pInfo->ApChannelScan ,"HOME-10C4-2.4|WPA/WPA2-PSK AES-CCMP TKIP|802.11b/g/n|-63|6|70:54:D2:00:AA:50");
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
   
    int rc = -1;
    if(pStats)
    {
        rc = Utopia_GetWifiRadioStats(ulInstanceNumber,pStats);
    }

    if (rc != 0)
       return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
}

/* WiFi SSID */
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
    int count = 0;

    UtopiaContext ctx;

    if(!Utopia_Init(&ctx))
        return ANSC_STATUS_FAILURE;
    count =  Utopia_GetWifiSSIDInstances(&ctx);

    /* Free Utopia Context */
    Utopia_Free(&ctx,0);
    if(count < 2)
        count = 2;
    return count;
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
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_FAILURE;

    UtopiaContext ctx;

    if(pEntry)
    {
        /* Initialize a Utopia Context */
        if(!Utopia_Init(&ctx))
           return ANSC_STATUS_FAILURE;

        returnStatus = Utopia_GetWifiSSIDEntry(&ctx, ulIndex, pEntry);

        /* Free Utopia Context */
        Utopia_Free(&ctx,0);
    }

    if (returnStatus != 0)
       return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
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

    UtopiaContext ctx;
    int rc = -1;

    /* Initialize a Utopia Context */
    if(!Utopia_Init(&ctx))
        return ANSC_STATUS_FAILURE;
    returnStatus = Utopia_WifiSSIDSetValues(&ctx,ulIndex,ulInstanceNumber,pAlias); 
   
    /* Free Utopia Context */
    Utopia_Free(&ctx,!returnStatus);
 
    if (returnStatus != 0)
       return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
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

    UtopiaContext ctx;

    if(!Utopia_Init(&ctx))
        return ANSC_STATUS_FAILURE;
    returnStatus =  Utopia_AddWifiSSID(&ctx,pEntry);

    /* Free Utopia Context */
    Utopia_Free(&ctx,!returnStatus);
    
    if (returnStatus != 0)
       return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiSsidDelEntry
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    
    UtopiaContext ctx;

    if(!Utopia_Init(&ctx))
        return ANSC_STATUS_FAILURE;
    returnStatus =  Utopia_DelWifiSSID(&ctx,ulInstanceNumber);
        
    /* Free Utopia Context */
    Utopia_Free(&ctx,!returnStatus);
    
    if (returnStatus != 0)
       return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiSsidSetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_SSID_CFG     pCfg
    )
{

    UtopiaContext ctx;
    int rc = -1;

    if(pCfg)
    {
        /* Initialize a Utopia Context */
        if(!Utopia_Init(&ctx))
           return ANSC_STATUS_FAILURE;

        rc = Utopia_SetWifiSSIDCfg(&ctx,pCfg);

        /* Set WLAN Restart event */
        Utopia_SetEvent(&ctx,Utopia_Event_WLAN_Restart);

        /* Free Utopia Context */
        Utopia_Free(&ctx,!rc);
    }

    if (rc != 0)
        return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiSsidGetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_SSID_CFG     pCfg
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    UtopiaContext ctx;

    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }

    /* Initialize a Utopia Context */
    if(!Utopia_Init(&ctx))
        return ANSC_STATUS_FAILURE;

    returnStatus = Utopia_GetWifiSSIDCfg(&ctx,0,pCfg);

    /* Free Utopia Context */
    Utopia_Free(&ctx,0);

    if (returnStatus != 0)
       return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
}

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
    returnStatus = Utopia_GetWifiSSIDDInfo(ulInstanceNumber,pInfo);

    if (returnStatus != 0)
       return ANSC_STATUS_FAILURE;
    else
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
    returnStatus = Utopia_GetWifiSSIDStats(ulInstanceNumber,pStats);
   
    if (returnStatus != 0)
       return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
}

/* WiFi AP is always associated with a SSID in the system */
ULONG
CosaDmlWiFiAPGetNumberOfEntries
    (
        ANSC_HANDLE                 hContext
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    int count = 0;

    UtopiaContext ctx;

    if(!Utopia_Init(&ctx))
        return ANSC_STATUS_FAILURE;
    count =  Utopia_GetWifiAPInstances(&ctx);

    /* Free Utopia Context */
    Utopia_Free(&ctx,0);
    if(count < 2)
        count = 2;
    return count;
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
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_FAILURE;

    UtopiaContext ctx;
    int rc = -1;

    if((pEntry) && (pSsid))
    {
        /* Initialize a Utopia Context */
        if(!Utopia_Init(&ctx))
           return ANSC_STATUS_FAILURE;

        returnStatus = Utopia_GetWifiAPEntry(&ctx,pSsid,pEntry);

        /* Free Utopia Context */
        Utopia_Free(&ctx,0);
    }

    if (returnStatus != 0)
       return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
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

    UtopiaContext ctx;

    /* Initialize a Utopia Context */
    if(!Utopia_Init(&ctx))
        return ANSC_STATUS_FAILURE;
    returnStatus = Utopia_WifiAPSetValues(&ctx,ulIndex,ulInstanceNumber,pAlias);

    /* Free Utopia Context */
    Utopia_Free(&ctx,!returnStatus);

    if (returnStatus != 0)
       return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiApSetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_AP_CFG       pCfg
    )
{
    UtopiaContext ctx;
    int rc = -1;

    if( (pCfg) && (pSsid) )
    {
        /* Initialize a Utopia Context */
        if(!Utopia_Init(&ctx))
           return ANSC_STATUS_FAILURE;

        rc = Utopia_SetWifiAPCfg(&ctx,pCfg);

        /* Set WLAN Restart event */
        Utopia_SetEvent(&ctx,Utopia_Event_WLAN_Restart);

        /* Free Utopia Context */
        Utopia_Free(&ctx,!rc);
    }

    if (rc != 0)
        return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;

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
    UtopiaContext ctx;

    if ( (!pCfg) || (!pSsid) )
    {
        return ANSC_STATUS_FAILURE;
    }
    /* Initialize a Utopia Context */
    if(!Utopia_Init(&ctx))
        return ANSC_STATUS_FAILURE;

    returnStatus = Utopia_GetWifiAPCfg(&ctx,0,pCfg);

    /* Free Utopia Context */
    Utopia_Free(&ctx,0);

    if (returnStatus != 0)
       return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
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
    UtopiaContext ctx;

    if ( (!pInfo) || (!pSsid) )
    {
        return ANSC_STATUS_FAILURE;
    }
    /* Initialize a Utopia Context */
    if(!Utopia_Init(&ctx))
        return ANSC_STATUS_FAILURE;

    returnStatus = Utopia_GetWifiAPInfo(&ctx,pSsid, pInfo);

    /* Free Utopia Context */
    Utopia_Free(&ctx,0);

    if (returnStatus != 0)
       return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
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
    UtopiaContext ctx;

    if ( (!pEntry) || (!pSsid) )
    {
        return ANSC_STATUS_FAILURE;
    }

    /* Initialize a Utopia Context */
    if(!Utopia_Init(&ctx))
        return ANSC_STATUS_FAILURE;

    returnStatus = Utopia_GetWifiAPSecEntry(&ctx,pSsid,pEntry);

    /* Free Utopia Context */
    Utopia_Free(&ctx,0);

    if (returnStatus != 0)
       return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiApSecGetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APSEC_CFG    pCfg
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    UtopiaContext ctx;

    if ( (!pCfg) || (!pSsid) )
    {
        return ANSC_STATUS_FAILURE;
    }

    /* Initialize a Utopia Context */
    if(!Utopia_Init(&ctx))
        return ANSC_STATUS_FAILURE;

    returnStatus = Utopia_GetWifiAPSecCfg(&ctx,pSsid,pCfg);

    /* Free Utopia Context */
    Utopia_Free(&ctx,0);
        
    if (returnStatus != 0)          
       return ANSC_STATUS_FAILURE;  
    else
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
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    UtopiaContext ctx;

    if ( (!pCfg) || (!pSsid) )
    {
        return ANSC_STATUS_FAILURE;
    }
      
    /* Initialize a Utopia Context */
    if(!Utopia_Init(&ctx))
        return ANSC_STATUS_FAILURE;

    returnStatus = Utopia_SetWifiAPSecCfg(&ctx,pSsid,pCfg);

    /* Set WLAN Restart event */
    Utopia_SetEvent(&ctx,Utopia_Event_WLAN_Restart);

    /* Free Utopia Context */
    Utopia_Free(&ctx,!returnStatus);            
        
    if (returnStatus != 0)          
       return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiApWpsGetEntry
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APWPS_FULL   pEntry
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

    UtopiaContext ctx;

    if ( (!pEntry) || (!pSsid) )
    {
        return ANSC_STATUS_FAILURE;
    }

    /* Initialize a Utopia Context */
    if(!Utopia_Init(&ctx))
        return ANSC_STATUS_FAILURE;

    returnStatus = Utopia_GetWifiAPWPSEntry(&ctx,pSsid,pEntry);

    /* Free Utopia Context */
    Utopia_Free(&ctx,0);

    if (returnStatus != 0)
       return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
    
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
    UtopiaContext ctx;

    if ( (!pCfg) || (!pSsid) )
    {
        return ANSC_STATUS_FAILURE;
    }

    /* Initialize a Utopia Context */
    if(!Utopia_Init(&ctx))
        return ANSC_STATUS_FAILURE;

    returnStatus = Utopia_SetWifiAPWPSCfg(&ctx,pSsid,pCfg);

    /* Free Utopia Context */
    Utopia_Free(&ctx,!returnStatus);

    if (returnStatus != 0)
       return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
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
    UtopiaContext ctx;

    if ( (!pCfg) || (!pSsid) )
    {
        return ANSC_STATUS_FAILURE;
    }

    /* Initialize a Utopia Context */
    if(!Utopia_Init(&ctx))
        return ANSC_STATUS_FAILURE;

    returnStatus = Utopia_GetWifiAPWPSCfg(&ctx,pSsid,pCfg);

    /* Free Utopia Context */
    Utopia_Free(&ctx,0);

    if (returnStatus != 0)
       return ANSC_STATUS_FAILURE;
    else
       return ANSC_STATUS_SUCCESS;
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
    ANSC_STATUS                     returnStatus      = ANSC_STATUS_SUCCESS;
    ULONG                           index             = 0;
    ULONG                           ulCount           = 0;
    UtopiaContext ctx;

    if (!pSsid)
    {
        return ANSC_STATUS_FAILURE;
    }

    /* Initialize a Utopia Context */
    if(!Utopia_Init(&ctx))
        return ANSC_STATUS_FAILURE;

    *pulCount = Utopia_GetAssociatedDevicesCount(&ctx,pSsid);
    if(0 == *pulCount)
    {
       Utopia_Free(&ctx,0);
       return NULL;
    }

    AssocDeviceArray = (PCOSA_DML_WIFI_AP_ASSOC_DEVICE)AnscAllocateMemory(sizeof(COSA_DML_WIFI_AP_ASSOC_DEVICE)*(*pulCount));

    if ( !AssocDeviceArray )
    {
        *pulCount = 0;
        Utopia_Free(&ctx,0);
        return NULL;
    }

    for(index = 0; index < *pulCount; index++)
    {
        returnStatus = Utopia_GetAssocDevice(&ctx,pSsid,index,&AssocDeviceArray[index]);
    }

    /* Free Utopia Context */
    Utopia_Free(&ctx,0);

    return AssocDeviceArray;
}

ANSC_STATUS
CosaDmlWiFiApMfGetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_AP_MF_CFG    pCfg
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    UtopiaContext ctx;

    if ( (!pCfg) || (!pSsid) )
    {
        return ANSC_STATUS_FAILURE;
    }
    /* Initialize a Utopia Context */
    if(!Utopia_Init(&ctx))
        return ANSC_STATUS_FAILURE;

    returnStatus = Utopia_GetWifiAPMFCfg(&ctx, pSsid, pCfg);

    /* Free Utopia Context */
    Utopia_Free(&ctx,0);

    if (returnStatus != 0)
       return ANSC_STATUS_FAILURE;
    else
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
    UtopiaContext ctx;
    int rc = -1;

    if( (pCfg) && (pSsid) )
    {
        /* Initialize a Utopia Context */
        if(!Utopia_Init(&ctx))
           return ANSC_STATUS_FAILURE;

        rc = Utopia_SetWifiAPMFCfg(&ctx, pSsid, pCfg);

        /* Free Utopia Context */
        Utopia_Free(&ctx,!rc);
    }

    if (rc != 0)
        return ANSC_STATUS_FAILURE;
    else
        return ANSC_STATUS_SUCCESS;
}

/**
 * stub funtions for USGv2 extension
 */

ANSC_STATUS
CosaDmlWiFi_GetWEPKey64ByIndex(ULONG apIns, ULONG keyIdx, PCOSA_DML_WEPKEY_64BIT pWepKey)
{
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_SetWEPKey64ByIndex(ULONG apIns, ULONG keyIdx, PCOSA_DML_WEPKEY_64BIT pWepKey)
{
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_GetWEPKey128ByIndex(ULONG apIns, ULONG keyIdx, PCOSA_DML_WEPKEY_128BIT pWepKey)
{
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_SetWEPKey128ByIndex(ULONG apIns, ULONG keyIdx, PCOSA_DML_WEPKEY_128BIT pWepKey)
{
    return ANSC_STATUS_SUCCESS;
}

ULONG
CosaDmlMacFilt_GetNumberOfEntries(ULONG apIns)
{
    return 0;
}

ANSC_STATUS
CosaDmlMacFilt_GetEntryByIndex(ULONG apIns, ULONG index, PCOSA_DML_WIFI_AP_MAC_FILTER pMacFilt)
{
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlMacFilt_SetValues(ULONG apIns, ULONG index, ULONG ins, char *Alias)
{
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlMacFilt_AddEntry(ULONG apIns, PCOSA_DML_WIFI_AP_MAC_FILTER pMacFilt)
{
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlMacFilt_DelEntry(ULONG apIns, ULONG macFiltIns)
{
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlMacFilt_GetConf(ULONG apIns, ULONG macFiltIns, PCOSA_DML_WIFI_AP_MAC_FILTER pMacFilt)
{
    return ANSC_STATUS_SUCCESS;
}
 
ANSC_STATUS
CosaDmlMacFilt_SetConf(ULONG apIns, ULONG macFiltIns, PCOSA_DML_WIFI_AP_MAC_FILTER pMacFilt)
{
    return ANSC_STATUS_SUCCESS;
}

#elif (_COSA_DRG_TPG_)

#include <utctx.h>
#include <utctx_api.h>
#include <utapi.h>
#include <utapi_util.h>

ANSC_STATUS
CosaDmlWiFiInit
    (
        ANSC_HANDLE                 hDml,
        PANSC_HANDLE                phContext
    )
{
    return ANSC_STATUS_FAILURE;
}

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

    /*wifi is in remote CPU*/
    CcspTraceWarning(("CosaDmlWiFiRadioGetNumberOfEntries - This is a local call ...\n"));
    return 0;
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

        return ANSC_STATUS_FAILURE;

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

        return ANSC_STATUS_FAILURE;
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
    
        return ANSC_STATUS_FAILURE;
}

ANSC_STATUS
CosaDmlWiFiRadioGetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_RADIO_CFG    pCfg        /* Identified by InstanceNumber */
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
    
        return ANSC_STATUS_FAILURE;
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
    
        return ANSC_STATUS_FAILURE;
}

ANSC_STATUS
CosaDmlWiFiRadioGetChannelsInUse
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
        AnscCopyString(pInfo->ChannelsInUse,"1");

        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS
CosaDmlWiFiRadioGetApChannelScan
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
        AnscCopyString(pInfo->ApChannelScan ,"HOME-10C4-2.4|WPA/WPA2-PSK AES-CCMP TKIP|802.11b/g/n|-63|6|70:54:D2:00:AA:50");
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
    
        return ANSC_STATUS_FAILURE;
}

/* WiFi SSID */
static int gSsidCount = 1;
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

    return 0;
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

        return ANSC_STATUS_FAILURE;
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
    
        return ANSC_STATUS_FAILURE;
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

        return ANSC_STATUS_FAILURE;
}

ANSC_STATUS
CosaDmlWiFiSsidDelEntry
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

        return ANSC_STATUS_FAILURE;
}

ANSC_STATUS
CosaDmlWiFiSsidSetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_SSID_CFG     pCfg
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

        return ANSC_STATUS_FAILURE;
}

ANSC_STATUS
CosaDmlWiFiSsidGetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_SSID_CFG     pCfg
    )
{
    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
    
        return ANSC_STATUS_FAILURE;
}

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
    
        return ANSC_STATUS_FAILURE;
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
    
        return ANSC_STATUS_FAILURE;
}

/* WiFi AP is always associated with a SSID in the system */
static int gApCount = 1;
ULONG
CosaDmlWiFiAPGetNumberOfEntries
    (
        ANSC_HANDLE                 hContext
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

        return 0;
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

        return ANSC_STATUS_FAILURE;
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

    return returnStatus;
    
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

        return ANSC_STATUS_FAILURE;
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
        
        return ANSC_STATUS_FAILURE;
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
    
        return ANSC_STATUS_FAILURE;
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
    
        return ANSC_STATUS_FAILURE;
}

ANSC_STATUS
CosaDmlWiFiApSecGetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APSEC_CFG    pCfg
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
    
        return ANSC_STATUS_FAILURE;
}

ANSC_STATUS
CosaDmlWiFiApSecSetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APSEC_CFG    pCfg
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;

        return ANSC_STATUS_FAILURE;
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

        return ANSC_STATUS_FAILURE;

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

        return ANSC_STATUS_FAILURE;

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

        return ANSC_STATUS_FAILURE;

    
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
    
        
        *pulCount = ulCount;
        
        return AssocDeviceArray;
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

#elif (_COSA_INTEL_USG_ATOM_)

#include <pthread.h>
pthread_mutex_t sWiFiThreadMutex = PTHREAD_MUTEX_INITIALIZER;

// #define wifiDbgPrintf 
#define wifiDbgPrintf printf

static int gRadioCount = 2;
/* WiFi SSID */
static int gSsidCount = 16;
static int gApCount = 16;

static  COSA_DML_WIFI_RADIO_CFG sWiFiDmlRadioStoredCfg[2];
static  COSA_DML_WIFI_RADIO_CFG sWiFiDmlRadioRunningCfg[2];
static COSA_DML_WIFI_SSID_CFG sWiFiDmlSsidStoredCfg[16];
static COSA_DML_WIFI_SSID_CFG sWiFiDmlSsidRunningCfg[16];
static COSA_DML_WIFI_AP_FULL sWiFiDmlApStoredCfg[16];
static COSA_DML_WIFI_AP_FULL sWiFiDmlApRunningCfg[16];
static COSA_DML_WIFI_APSEC_FULL  sWiFiDmlApSecurityStored[16];
static COSA_DML_WIFI_APSEC_FULL  sWiFiDmlApSecurityRunning[16];
static COSA_DML_WIFI_APWPS_FULL sWiFiDmlApWpsStored[16];
static COSA_DML_WIFI_APWPS_FULL sWiFiDmlApWpsRunning[16];
static PCOSA_DML_WIFI_AP_MF_CFG  sWiFiDmlApMfCfg[16];
static BOOLEAN sWiFiDmlRestartHostapd = FALSE;
static QUEUE_HEADER *sWiFiDmlApMfQueue[16];
static BOOLEAN sWiFiDmlWepChg[16] = { FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE };
static BOOLEAN sWiFiDmlAffectedVap[16] = { FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE };
static BOOLEAN sWiFiDmlPushWepKeys[16] = { FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE };
static BOOLEAN sWiFiDmlUpdateVlanCfg[16] = { FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE };
static BOOLEAN sWiFiDmlUpdatedAdvertisement[16] = { FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE };
static ULONG sWiFiDmlRadioLastStatPoll[16] = { 0, 0 };
static ULONG sWiFiDmlSsidLastStatPoll[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; 

extern ANSC_HANDLE bus_handle;
extern char        g_Subsystem[32];
extern PCOSA_BACKEND_MANAGER_OBJECT g_pCosaBEManager;
static PCOSA_DATAMODEL_WIFI            pMyObject = NULL;
static COSA_DML_WIFI_SSID_SINFO   gCachedSsidInfo[16];

static PCOSA_DML_WIFI_SSID_BRIDGE  pBridgeVlanCfg = NULL;

static char *FactoryReset    	= "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.FactoryReset";
static char *FactoryResetSSID    	= "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.Radio.%d.FactoryResetSSID";
static char *ValidateSSIDName        = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.ValidateSSIDName";
static char *FixedWmmParams        = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.FixedWmmParamsValues";
static char *SsidUpgradeRequired = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.SsidUpgradeRequired";

// Not being set for 1st GA
// static char *RegulatoryDomain 	= "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.Radio.%d.RegulatoryDomain";

static char *CTSProtection    	= "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.Radio.%d.CTSProtection";
static char *BeaconInterval   	= "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.Radio.%d.BeaconInterval";
static char *DTIMInterval   	= "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.Radio.%d.DTIMInterval";
static char *FragThreshold   	= "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.Radio.%d.FragThreshold";
static char *RTSThreshold   	= "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.Radio.%d.RTSThreshold";
static char *ObssCoex   	= "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.Radio.%d.ObssCoex";
static char *STBCEnable         = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.Radio.%d.STBCEnable";
static char *GuardInterval      = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.Radio.%d.GuardInterval";
static char *GreenField         = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.Radio.%d.GreenField";
static char *TransmitPower      = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.Radio.%d.TransmitPower";
static char *UserControl       = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.Radio.%d.UserControl";
static char *AdminControl    = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.Radio.%d.AdminControl";

static char *WmmEnable   	= "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.WmmEnable";
static char *UAPSDEnable   	= "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.UAPSDEnable";
static char *WmmNoAck   	= "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.WmmNoAck";
static char *BssMaxNumSta   	= "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.BssMaxNumSta";
static char *AssocDevNextInstance = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.AssocDevNextInstance";
static char *MacFilterNextInstance = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.MacFilterNextInstance";
// comma separated list of valid mac filter instances, format numInst:i1,i2,...,in where there are n instances. "4:1,5,6,10"
static char *MacFilterMode      = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.MacFilterMode";
static char *MacFilterList      = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.MacFilterList";
static char *MacFilter          = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.MacFilter.%d";
static char *MacFilterDevice    = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.MacFilterDevice.%d";
static char *WepKeyLength    = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.WepKeyLength";
static char *ApIsolationEnable    = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.ApIsolationEnable";
static char *BssHotSpot        = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.HotSpot";
static char *WpsPushButton = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.WpsPushButton";

// Currently these are statically set during initialization
// static char *WmmCapabilities   	= "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.WmmCapabilities";
// static char *UAPSDCapabilities  = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.UAPSDCapabilities";

// Platform specific data that is stored in the ARM Intel DB and converted to PSM entries
// They will be read only on Factory Reset command and override the current Wifi configuration
static char *RadioIndex ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.SSID.%d.RadioIndex";
static char *WlanEnable ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.SSID.%d.WLANEnable";
static char *BssSsid ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.SSID.%d.SSID";
static char *HideSsid ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.SSID.%d.HideSSID";
static char *SecurityMode ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.SSID.%d.Security";
static char *EncryptionMethod ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.SSID.%d.Encryption";
static char *Passphrase ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.SSID.%d.Passphrase";
static char *WmmRadioEnable ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.SSID.%d.WMMEnable";
static char *WpsEnable ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.SSID.%d.WPSEnable";
static char *WpsPin ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.WPSPin";
static char *Vlan ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.SSID.%d.Vlan";
static char *ReloadConfig = "com.cisco.spvtg.ccsp.psm.ReloadConfig";

static char *WifiVlanCfgVersion ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.VlanCfgVerion";
static char *l2netBridgeInstances = "dmsb.l2net.";
static char *l2netBridge = "dmsb.l2net.%d.Members.WiFi";
static char *l2netVlan   = "dmsb.l2net.%d.Vid";
static char *l2netl3InstanceNum = "dmsb.atom.l2net.%d.l3net";
static char *l3netIpAddr = "dmsb.atom.l3net.%d.V4Addr";
static char *l3netIpSubNet = "dmsb.atom.l3net.%d.V4SubnetMask";

#define WIFIEXT_DM_OBJ           "Device.MoCA."
#define WIFIEXT_DM_RADIO_UPDATE  "Device.MoCA.X_CISCO_COM_WiFi_Extender.X_CISCO_COM_Radio_Updated"
#define WIFIEXT_DM_WPS_UPDATE    "Device.MoCA.X_CISCO_COM_WiFi_Extender.X_CISCO_COM_WPS_Updated"
#define WIFIEXT_DM_SSID_UPDATE   "Device.MoCA.X_CISCO_COM_WiFi_Extender.X_CISCO_COM_SSID_Updated"

typedef enum {
   COSA_WIFIEXT_DM_UPDATE_RADIO = 0x1,
   COSA_WIFIEXT_DM_UPDATE_WPS   = 0x2,
   COSA_WIFIEXT_DM_UPDATE_SSID  = 0x4
} COSA_WIFIEXT_DM_UPDATE;

static ANSC_STATUS CosaDmlWiFiRadioSetTransmitPowerPercent ( int wlanIndex, int transmitPowerPercent);

static char *dstComp, *dstPath; /* cache */
static BOOL FindWifiDestComp(void);
static const int gWifiVlanCfgVersion = 2;

static BOOL FindWifiDestComp(void)
{
    if (dstComp && dstPath)
        return TRUE;

    if (dstComp)
        AnscFreeMemory(dstComp);
    if (dstPath)
        AnscFreeMemory(dstPath);
    dstComp = dstPath = NULL;

    if (!Cosa_FindDestComp(WIFIEXT_DM_OBJ, &dstComp, &dstPath)
            || !dstComp || !dstPath)
    {
        wifiDbgPrintf("%s: fail to find dest comp\n", __FUNCTION__);
        return FALSE;
    }

    return TRUE;
}


static int CosaDml_SetWiFiExt(char *paramName) {
    parameterValStruct_t valStr;
    char str[2][100];
    valStr.parameterName=str[0];
    valStr.parameterValue=str[1];
    
    BOOL retVal = FindWifiDestComp();
    wifiDbgPrintf("%s: FindWifiDestComp returned %s\n", __func__, (retVal == TRUE) ? "True" : "False");
    if (retVal != TRUE) {
       return -1;
    }
    
    sprintf(valStr.parameterName, paramName);
    sprintf(valStr.parameterValue, "%s", "true");
    valStr.type = ccsp_boolean;

    if (!Cosa_SetParamValuesNoCommit(dstComp, dstPath, &valStr, 1))
    {
        wifiDbgPrintf("%s: fail to set: %s\n", __FUNCTION__, valStr.parameterName);
        return -1;
    }

    return 0;
}

static void CosaDml_NotifyWiFiExtThread(int input) 
{
    int flag = (int) input;
    int retval = 1;

    wifiDbgPrintf("%s: Enter flag = %d \n", __func__, flag ); 

    pthread_detach(pthread_self());

    // Get Lock only to ensure that calling thread is done then release it
    printf("%s Calling pthread_mutex_lock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 
    pthread_mutex_lock(&sWiFiThreadMutex);
    printf("%s Called pthread_mutex_lock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 
    printf("%s Calling pthread_mutex_unlock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 
    pthread_mutex_unlock(&sWiFiThreadMutex);
    printf("%s Called pthread_mutex_unlock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ );  

    if ( flag & COSA_WIFIEXT_DM_UPDATE_RADIO )
    {
        retval = CosaDml_SetWiFiExt(WIFIEXT_DM_RADIO_UPDATE);
        wifiDbgPrintf("%s: CosaDml_SetWiFiExt WIFIEXT_DM_RADIO_UPDATE returned %d \n", __func__, retval); 
    } 

    if ( flag & COSA_WIFIEXT_DM_UPDATE_WPS ) 
    {
        retval = CosaDml_SetWiFiExt(WIFIEXT_DM_WPS_UPDATE);
        wifiDbgPrintf("%s: CosaDml_SetWiFiExt WIFIEXT_DM_WPS_UPDATE returned %d \n", __func__, retval); 
    } 

    if ( flag & COSA_WIFIEXT_DM_UPDATE_SSID )
    {
        retval = CosaDml_SetWiFiExt(WIFIEXT_DM_SSID_UPDATE);
        wifiDbgPrintf("%s: CosaDml_SetWiFiExt WIFIEXT_DM_SSID_UPDATE returned %d \n", __func__, retval); 
    }

    if (retval == 0)
    {
        Cosa_SetCommit(dstComp,dstPath,TRUE);
    }

}

static void CosaDml_NotifyWiFiExt(int flag) 
{
    printf("%s: Enter flag = %d \n", __func__, flag ); 
    // Notify WiFiExtender that WiFi parameter have changed
    {
        pthread_t tid; 
        if(pthread_create(&tid,NULL,CosaDml_NotifyWiFiExtThread,flag))  {
            return;
        }
    }
}

static COSA_DML_WIFI_RADIO_POWER gRadioPowerState[2] = { COSA_DML_WIFI_POWER_UP, COSA_DML_WIFI_POWER_UP};
// Are Global for whole WiFi
static COSA_DML_WIFI_RADIO_POWER gRadioPowerSetting = COSA_DML_WIFI_POWER_UP;
static COSA_DML_WIFI_RADIO_POWER gRadioNextPowerSetting = COSA_DML_WIFI_POWER_UP;

ANSC_STATUS
CosaDmlWiFiFactoryResetSsidData(int start, int end)
{
    char recName[256];
    char recValue[256];
    char *strValue = NULL;
    int retPsmGet = CCSP_SUCCESS;
    int resetSSID[2] = {0,0};
    int i = 0;
printf("%s g_Subsytem = %s\n",__FUNCTION__,g_Subsystem);

    // Delete PSM entries for the specified range of Wifi SSIDs
    for (i = start; i <= end; i++)
	{
printf("%s: deleting records for index %d \n", __FUNCTION__, i);
	    sprintf(recName, WmmEnable, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, UAPSDEnable, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, WmmNoAck, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, BssMaxNumSta, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, BssHotSpot, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    // Platform specific data that is stored in the ARM Intel DB and converted to PSM entries
	    // They will be read only on Factory Reset command and override the current Wifi configuration
	    sprintf(recName, RadioIndex, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, WlanEnable, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, BssSsid, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, HideSsid, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, SecurityMode, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, EncryptionMethod, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, Passphrase, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, WmmRadioEnable, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, WpsEnable, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, Vlan, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);
    }

    PSM_Set_Record_Value2(bus_handle,g_Subsystem, ReloadConfig, ccsp_string, "true");

    return ANSC_STATUS_SUCCESS;
}
ANSC_STATUS
CosaDmlWiFiGetFactoryResetPsmData
    (
        BOOLEAN *factoryResetFlag
    )
{
    char *strValue = NULL;
    int retPsmGet = CCSP_SUCCESS;

printf("%s g_Subsytem = %s\n",__FUNCTION__, g_Subsystem);

    // Get Non-vol parameters from ARM through PSM
    // PSM may not be available yet on arm so sleep if there is not connection
    int retry = 0;
    while (retry++ < 20)
    {
	retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, FactoryReset, NULL, &strValue);
	if (retPsmGet == CCSP_SUCCESS) {
printf("%s %s = %s \n",__FUNCTION__, FactoryReset, strValue); 
	    *factoryResetFlag = _ansc_atoi(strValue);
	    ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);

            // Set to FALSE after FactoryReset has been applied
	    // PSM_Set_Record_Value2(bus_handle,g_Subsystem, FactoryReset, ccsp_string, "0");

	} else if (retPsmGet == CCSP_CR_ERR_INVALID_PARAM) { 
            *factoryResetFlag = 0;
	    printf("%s PSM_Get_Record_Value2 (%s) returned error %d \n",__FUNCTION__, FactoryReset, retPsmGet); 
            // Set to FALSE
	    PSM_Set_Record_Value2(bus_handle,g_Subsystem, FactoryReset, ccsp_string, "0");
	} else { 
	    printf("%s PSM_Get_Record_Value2 returned error %d retry in 10 seconds \n",__FUNCTION__, retPsmGet); 
	    AnscSleep(10000); 
	    continue;
	} 
	break;
    }

    if (retPsmGet != CCSP_SUCCESS && retPsmGet != CCSP_CR_ERR_INVALID_PARAM) {
            printf("%s Could not connect to the server error %d\n",__FUNCTION__, retPsmGet);
            *factoryResetFlag = 0;
            return ANSC_STATUS_FAILURE;
    }


    // Check to see if there is a required upgrad reset for the SSID
    // This is required for Comcast builds that were upgraded from 1.3.  This code should only be trigger 
    // in Comcast non-BWG builds.
    if (*factoryResetFlag == 0) {

        retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, SsidUpgradeRequired, NULL, &strValue);
        if (retPsmGet == CCSP_SUCCESS) {
            printf("%s %s = %s \n",__FUNCTION__, SsidUpgradeRequired, strValue); 

            int upgradeFlag = _ansc_atoi(strValue);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);

            if (upgradeFlag == 1) {
                *factoryResetFlag = 1;
            }
        }

        {
            char secSsid1[32], secSsid2[32], hotSpot1[32], hotSpot2[32];
            wifi_getSSID(2,secSsid1);
            wifi_getSSID(3,secSsid2);
            wifi_getSSID(4,hotSpot1);
            wifi_getSSID(5,hotSpot2);
            wifiDbgPrintf("%s: secSsid1 = %s secSsid2 = %s hotSpot1 = %s hotSpot2 = %s \n", __func__, secSsid1,secSsid2,hotSpot1,hotSpot2);

            if ( (strcmp(secSsid1,"Security-2.4") == 0)  || (strcmp(secSsid2,"Security-5") == 0)  ||
                 (strcmp(hotSpot1,"Hotspot-2.4") == 0)  || (strcmp(hotSpot2,"Hotspot-5") == 0) ) {
                wifiDbgPrintf("%s: Factory Reset required for all but the primary SSIDs \n", __func__);
                *factoryResetFlag = 1;
            }
        }

        if (*factoryResetFlag == 1) {
            char recName[256];

            // Set FactoryReset flags 
            sprintf(recName, FactoryResetSSID, 1);
            PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "254");
            sprintf(recName, FactoryResetSSID, 2);
            PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "254");
            PSM_Set_Record_Value2(bus_handle,g_Subsystem, FactoryReset, ccsp_string, "1");
            PSM_Set_Record_Value2(bus_handle,g_Subsystem, SsidUpgradeRequired, ccsp_string, "0"); 
            //  Force new PSM values from default config and wifidb in Intel db for non-Primary SSIDs
            CosaDmlWiFiFactoryResetSsidData(3,16);
        }
    } else {
        // if the FactoryReset was set, we don't need to do the Upgrade reset as well.
        PSM_Set_Record_Value2(bus_handle,g_Subsystem, SsidUpgradeRequired, ccsp_string, "0");
    }

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiGetResetRequired
(
BOOLEAN *resetFlag
)
{
    char *strValue = NULL;
    int retPsmGet = CCSP_SUCCESS;

    wifiDbgPrintf("%s g_Subsytem = %s\n",__FUNCTION__, g_Subsystem);  
    *resetFlag = FALSE;

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, WifiVlanCfgVersion, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        wifiDbgPrintf("%s %s = %s \n",__FUNCTION__, WifiVlanCfgVersion, strValue); 

        int version = _ansc_atoi(strValue);
        if (version != gWifiVlanCfgVersion) {
            wifiDbgPrintf("%s: Radio restart required:  %s value of %s was not the required cfg value %d \n",__FUNCTION__, WifiVlanCfgVersion, strValue, gWifiVlanCfgVersion); 
            *resetFlag = TRUE;
        } else {
            *resetFlag = FALSE;
        }
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    } else {
        wifiDbgPrintf("%s %s not found in PSM set to %d \n",__FUNCTION__, WifiVlanCfgVersion , gWifiVlanCfgVersion); 
        *resetFlag = TRUE;
    }

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiGetResetHotSpotRequired
(
BOOLEAN *resetFlag
)
{
    char *strValue = NULL;
	char recName[256];
    int retPsmGet = CCSP_SUCCESS;
    int i;

    wifiDbgPrintf("%s g_Subsytem = %s\n",__FUNCTION__, g_Subsystem);  
    *resetFlag = FALSE;

    // if either Primary SSID is set to TRUE, then HotSpot needs to be reset 
    // since these should not be HotSpot SSIDs
    for (i = 1; i <= 2; i++) {
        sprintf(recName, BssHotSpot, i);
        retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
        if (retPsmGet == CCSP_SUCCESS) {
            wifiDbgPrintf("%s: found BssHotSpot value = %s \n", __func__, strValue);
            BOOL enable = _ansc_atoi(strValue);
            if (enable == TRUE) {
                *resetFlag = TRUE;
            }
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
        }
    }

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiGetRadioSetSecurityDataPsmData
    (
        ULONG                       wlanIndex,
        ULONG                       ulInstance,
        ULONG                       modeEnabled
    )
{
    char securityType[32];
    char authMode[32];
    char method[32];

    memset(securityType,0,sizeof(securityType));
    if (modeEnabled == COSA_DML_WIFI_SECURITY_None)
    {
        strcpy(securityType,"None");
        strcpy(authMode,"None");
    } else if (modeEnabled == COSA_DML_WIFI_SECURITY_WEP_64 ||
               modeEnabled == COSA_DML_WIFI_SECURITY_WEP_128)
    { 
	ULONG wepLen;
	char *strValue = NULL;
	char recName[256];
	int retPsmSet = CCSP_SUCCESS;

        strcpy(securityType,"Basic");
        strcpy(authMode,"None");

        wifi_setBasicEncryptionMode(wlanIndex, "WEPEncryption");

	wifi_setApWepKeyIndex(wlanIndex, 1);

	memset(recName, 0, sizeof(recName));
	sprintf(recName, WepKeyLength, ulInstance);
	if (modeEnabled == COSA_DML_WIFI_SECURITY_WEP_64)
	{ 
	    PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "64" );
	} else if (modeEnabled == COSA_DML_WIFI_SECURITY_WEP_128)
	{
	    PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "128" );
	}

    } else if (modeEnabled == COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal)
    {
        strcpy(securityType,"WPAand11i");
        strcpy(authMode,"PSKAuthentication");
    } else if (modeEnabled == COSA_DML_WIFI_SECURITY_WPA_Personal)
    {
        strcpy(securityType,"WPA");
        strcpy(authMode,"PSKAuthentication");
    } else if (modeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Personal)
    {
        strcpy(securityType,"11i");
        strcpy(authMode,"PSKAuthentication");
    } else
    { 
        strcpy(securityType,"None");
        strcpy(authMode,"None");
    }
    wifi_setBeaconType(wlanIndex, securityType);
    wifi_setBasicAuthenticationMode(wlanIndex, authMode);
    
    return ANSC_STATUS_SUCCESS;
}
ANSC_STATUS
CosaDmlWiFiGetRadioFactoryResetPsmData
    (
        ULONG                       wlanIndex,
        ULONG                       ulInstance
    )
{
    char *strValue = NULL;
    char recName[256];
    int intValue;
    int retPsmGet = CCSP_SUCCESS;

printf("%s g_Subsytem = %s\n",__FUNCTION__, g_Subsystem);

    unsigned int password = 0;
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, WpsPin, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        password = _ansc_atoi(strValue);
        wifi_setWpsDevicePIN(wlanIndex, password);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiGetSSIDFactoryResetPsmData
    (
        ULONG                       wlanIndex,
        ULONG                       ulInstance
    )
{
    char *strValue = NULL;
    char recName[256];
    int intValue;
    int retPsmGet = CCSP_SUCCESS;

printf("%s g_Subsytem = %s wlanIndex = %d \n",__FUNCTION__, g_Subsystem, wlanIndex);
    memset(recName, 0, sizeof(recName));
    sprintf(recName, RadioIndex, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        intValue = _ansc_atoi(strValue);
	wifi_setRadioIndex(wlanIndex, intValue);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, WlanEnable, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        intValue = _ansc_atoi(strValue);
	wifi_setEnable(wlanIndex, intValue);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, BssSsid, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
	wifi_setSSID(wlanIndex, strValue);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, HideSsid, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        intValue = (_ansc_atoi(strValue) == 0) ? 1 : 0;
        wifi_setSsidAdvertisementEnable(wlanIndex, intValue);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, SecurityMode, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        intValue = _ansc_atoi(strValue);
        CosaDmlWiFiGetRadioSetSecurityDataPsmData(wlanIndex, ulInstance, intValue);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, EncryptionMethod, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        intValue = _ansc_atoi(strValue);
	if (intValue == COSA_DML_WIFI_AP_SEC_TKIP)
	{ 
	    wifi_setWpaEncryptionMode(wlanIndex, "TKIPEncryption");
	} else if ( intValue == COSA_DML_WIFI_AP_SEC_AES)
	{
	    wifi_setWpaEncryptionMode(wlanIndex, "AESEncryption");
	} else if ( intValue == COSA_DML_WIFI_AP_SEC_AES_TKIP)
	{
	    wifi_setWpaEncryptionMode(wlanIndex, "TKIPandAESEncryption");
	}
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, Passphrase, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        wifi_setKeyPassphrase(wlanIndex, strValue);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, WmmRadioEnable, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS)
    {
        intValue = _ansc_atoi(strValue);
        wifi_setWmmEnable(wlanIndex, intValue);
        // This value is also written to PSM because it is not stored by Atheros
        // Want to override with Platform specific data on factory reset
        sprintf(recName, WmmEnable, ulInstance);
        PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue); 
        sprintf(recName, UAPSDEnable, ulInstance);
        PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue); 
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    unsigned int password = 0;
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, WpsPin, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        password = _ansc_atoi(strValue);
        wifi_setWpsDevicePIN(wlanIndex, password);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, WpsEnable, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        int intValue = _ansc_atoi(strValue);
        if ( (intValue == TRUE) && (password != 0) ) {
           intValue = 2; // Configured 
        } 
        wifi_setWpsEnable(wlanIndex, intValue);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }
    
   memset(recName, 0, sizeof(recName));
    sprintf(recName, BssHotSpot, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        // if this is a HotSpot SSID, then set EnableOnline=TRUE and
        //  it should only be brought up once the RouterEnabled=TRUE
        wifiDbgPrintf("%s: found BssHotSpot value = %s \n", __func__, strValue);
        BOOL enable = _ansc_atoi(strValue);
        wifi_setEnableOnLine(wlanIndex,enable);
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    } else {
        wifiDbgPrintf("%s: didn't find BssHotSpot setting EnableOnline to FALSE \n", __func__);
        wifi_setEnableOnLine(wlanIndex,0); 
    }

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiGetRadioPsmData
    (
        PCOSA_DML_WIFI_RADIO_CFG    pCfg        /* Identified by InstanceNumber */
    )
{
    char *strValue = NULL;
    char recName[256];
    int intValue;
    int retPsmGet = CCSP_SUCCESS;
    ULONG                       wlanIndex;
    ULONG                       ulInstance;

    if (pCfg != NULL) {
        ulInstance = pCfg->InstanceNumber;
        wlanIndex = pCfg->InstanceNumber - 1;
    } else {
        return ANSC_STATUS_FAILURE;
    }

printf("%s g_Subsytem = %s\n",__FUNCTION__, g_Subsystem);

    // All these values need to be set once the VAP is up

    memset(recName, 0, sizeof(recName));
    sprintf(recName, CTSProtection, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
	BOOL enable = _ansc_atoi(strValue);
        pCfg->CTSProtectionMode = (enable == TRUE) ? TRUE : FALSE;

        wifi_setCtsProtectionEnable(wlanIndex, enable);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    } 

    memset(recName, 0, sizeof(recName));
    sprintf(recName, BeaconInterval, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        intValue = _ansc_atoi(strValue);
        pCfg->BeaconInterval = intValue;
	wifi_setBeaconInterval(wlanIndex, intValue);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, DTIMInterval, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        intValue = _ansc_atoi(strValue);
        pCfg->DTIMInterval = intValue;
        wifi_setDTIMInterval(wlanIndex, intValue);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, FragThreshold, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        char opStandards[32];
	BOOL gOnly;
	BOOL nOnly;
        BOOL acOnly;
       
        intValue = _ansc_atoi(strValue);
        pCfg->FragmentationThreshold = intValue;

	wifi_getStandard(wlanIndex, opStandards, &gOnly, &nOnly, &acOnly);
        if (strncmp("n",opStandards,1)!=0 && strncmp("ac",opStandards,1)!=0) {
	    wifi_setFragmentationThreshold(wlanIndex, intValue);
        }
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, RTSThreshold, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        intValue = _ansc_atoi(strValue);
        pCfg->RTSThreshold = intValue;
	wifi_setRtsThreshold(wlanIndex, intValue);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, ObssCoex, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
	BOOL enable = _ansc_atoi(strValue);
        pCfg->ObssCoex = (enable == TRUE) ? TRUE : FALSE;

        wifi_setObssCoexistenceEnable(wlanIndex, enable);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    } 

    memset(recName, 0, sizeof(recName));
    sprintf(recName, STBCEnable, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        BOOL enable = _ansc_atoi(strValue);
        pCfg->X_CISCO_COM_STBCEnable = (enable == TRUE) ? TRUE : FALSE;
	wifi_setSTBCEnable(wlanIndex, enable);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, GuardInterval, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        // if COSA_DML_WIFI_GUARD_INTVL_800ns set to FALSE otherwise was (400ns or Auto, set to TRUE)
        int guardInterval = _ansc_atoi(strValue);
        pCfg->GuardInterval = guardInterval;
        BOOL enable = (guardInterval == 2) ? FALSE : TRUE;
	wifi_setShortGuardInterval(wlanIndex, enable);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, TransmitPower, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        int transmitPower = _ansc_atoi(strValue);
        pCfg->TransmitPower = transmitPower;
        wifiDbgPrintf("%s: Found TransmitPower in PSM %d\n", __func__ , transmitPower);
        if ( (  gRadioPowerState[wlanIndex] == COSA_DML_WIFI_POWER_UP ) &&
             ( gRadioNextPowerSetting != COSA_DML_WIFI_POWER_DOWN ) )
        {
            CosaDmlWiFiRadioSetTransmitPowerPercent(wlanIndex,transmitPower);
        }
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, UserControl, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        pCfg->MbssUserControl = atoi(strValue);
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    } else {
        pCfg->MbssUserControl =  0x0003; // ath0 and ath1 on by default 
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, AdminControl, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        pCfg->AdminControl = atoi(strValue);
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    } else {
        pCfg->AdminControl =  0xFFFF; // all on by default
    }

	memset(recName, 0, sizeof(recName));
	sprintf(recName, GreenField, ulInstance);
	retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
	if (retPsmGet == CCSP_SUCCESS) {
        pCfg->X_CISCO_COM_11nGreenfieldEnabled =  _ansc_atoi(strValue);
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    } 

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiSetRadioPsmData
    (
        PCOSA_DML_WIFI_RADIO_CFG    pCfg,
        ULONG                       wlanIndex,
        ULONG                       ulInstance
    )
{
    char strValue[256];
    char recName[256];
    int intValue;
    int retPsmSet = CCSP_SUCCESS;
    PCOSA_DML_WIFI_RADIO_CFG        pStoredCfg  = &sWiFiDmlRadioStoredCfg[pCfg->InstanceNumber-1];

wifiDbgPrintf("%s g_Subsytem = %s\n",__FUNCTION__, g_Subsystem);
 
    if (pCfg->CTSProtectionMode != pStoredCfg->CTSProtectionMode) {
        sprintf(recName, CTSProtection, ulInstance);
    sprintf(strValue,"%d",pCfg->CTSProtectionMode);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
    if (retPsmSet != CCSP_SUCCESS) {
	wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting CTSProtectionMode\n",__FUNCTION__, retPsmSet); 
    }
    }

    if (pCfg->BeaconInterval != pStoredCfg->BeaconInterval) {
        sprintf(recName, BeaconInterval, ulInstance);
    sprintf(strValue,"%d",pCfg->BeaconInterval);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
    if (retPsmSet != CCSP_SUCCESS) {
	wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting BeaconInterval\n",__FUNCTION__, retPsmSet); 
    }
    }

    if (pCfg->DTIMInterval != pStoredCfg->DTIMInterval) {
        sprintf(recName, DTIMInterval, ulInstance);
    sprintf(strValue,"%d",pCfg->DTIMInterval);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
    if (retPsmSet != CCSP_SUCCESS) {
	wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting DTIMInterval\n",__FUNCTION__, retPsmSet); 
    }
    }

    if (pCfg->FragmentationThreshold != pStoredCfg->FragmentationThreshold) {
        sprintf(recName, FragThreshold, ulInstance);
    sprintf(strValue,"%d",pCfg->FragmentationThreshold);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
    if (retPsmSet != CCSP_SUCCESS) {
	wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting FragmentationThreshold\n",__FUNCTION__, retPsmSet); 
    }
    }

    if (pCfg->RTSThreshold != pStoredCfg->RTSThreshold) {
        memset(recName, 0, sizeof(recName));
    sprintf(recName, RTSThreshold, ulInstance);
    sprintf(strValue,"%d",pCfg->RTSThreshold);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
    if (retPsmSet != CCSP_SUCCESS) {
	wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting RTS Threshold\n",__FUNCTION__, retPsmSet); 
    }
    }

    if (pCfg->ObssCoex != pStoredCfg->ObssCoex) {
        sprintf(recName, ObssCoex, ulInstance);
    sprintf(strValue,"%d",pCfg->ObssCoex);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
    if (retPsmSet != CCSP_SUCCESS) {
	wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting ObssCoex\n",__FUNCTION__, retPsmSet); 
    }
    }

    if (pCfg->X_CISCO_COM_STBCEnable != pStoredCfg->X_CISCO_COM_STBCEnable) {
        memset(recName, 0, sizeof(recName));
    sprintf(recName, STBCEnable, ulInstance);
    sprintf(strValue,"%d",pCfg->X_CISCO_COM_STBCEnable);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
    if (retPsmSet != CCSP_SUCCESS) {
	wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting STBC \n",__FUNCTION__, retPsmSet); 
    }
    }

    if (pCfg->GuardInterval != pStoredCfg->GuardInterval) {
        memset(recName, 0, sizeof(recName));
    sprintf(recName, GuardInterval, ulInstance);
    sprintf(strValue,"%d",pCfg->GuardInterval);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
    if (retPsmSet != CCSP_SUCCESS) {
	wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting Guard Interval \n",__FUNCTION__, retPsmSet); 
    }
    }

    if (pCfg->TransmitPower != pStoredCfg->TransmitPower ) {
        memset(recName, 0, sizeof(recName));
    sprintf(recName, TransmitPower, ulInstance);
    sprintf(strValue,"%d",pCfg->TransmitPower);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
    if (retPsmSet != CCSP_SUCCESS) {
	wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting Transmit Power \n",__FUNCTION__, retPsmSet); 
    }
    }

    if (pCfg->MbssUserControl != pStoredCfg->MbssUserControl) {
        sprintf(recName, UserControl, ulInstance);
    sprintf(strValue,"%d",pCfg->MbssUserControl);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
    if (retPsmSet != CCSP_SUCCESS) {
	    wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting UserControl \n",__FUNCTION__, retPsmSet); 
        }
    }

    if (pCfg->AdminControl != pStoredCfg->AdminControl) {
        sprintf(recName, AdminControl, ulInstance);
    sprintf(strValue,"%d",pCfg->AdminControl);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
    if (retPsmSet != CCSP_SUCCESS) {
	    wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting AdminControl  \n",__FUNCTION__, retPsmSet); 
        }
    }

    if (pCfg->X_CISCO_COM_11nGreenfieldEnabled != pStoredCfg->X_CISCO_COM_11nGreenfieldEnabled) {
        sprintf(recName, GreenField, ulInstance);
        sprintf(strValue,"%d",pCfg->X_CISCO_COM_11nGreenfieldEnabled);
        retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting GreenfieldEnabled  \n",__FUNCTION__, retPsmSet); 
    }
    }

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiGetAccessPointPsmData
    (
    PCOSA_DML_WIFI_AP_CFG       pCfg
)
{
    char *strValue = NULL;
    int intValue;
    char recName[256];
    int retPsmGet = CCSP_SUCCESS;
    int retPsmSet = CCSP_SUCCESS;
    BOOL enabled; 
    ULONG                       wlanIndex;
    ULONG                       ulInstance;

    if (pCfg != NULL) {
        ulInstance = pCfg->InstanceNumber;
        wlanIndex = pCfg->InstanceNumber - 1;
    } else {
        return ANSC_STATUS_FAILURE;
    }

    wifi_getEnable(wlanIndex, &enabled);

printf("%s g_Subsytem = %s wlanIndex %d ulInstance %d enabled = %s\n",__FUNCTION__, g_Subsystem, wlanIndex, ulInstance, 
       (enabled == TRUE) ? "TRUE" : "FALSE");

    // SSID does not need to be enabled to push this param to the configuration
    memset(recName, 0, sizeof(recName));
    sprintf(recName, BssHotSpot, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        // if this is a HotSpot SSID, then set is EnableOnline=TRUE and
        //  it should only be brought up once the RouterEnabled=TRUE
        wifiDbgPrintf("%s: found BssHotSpot value = %s \n", __func__, strValue);
        BOOL enable = _ansc_atoi(strValue);
        pCfg->BssHotSpot  = (enable == TRUE) ? TRUE : FALSE;
        wifi_setEnableOnLine(wlanIndex,enable);
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    } else {
        wifiDbgPrintf("%s: didn't find BssHotSpot setting EnableOnline to FALSE \n", __func__);
        wifi_setEnableOnLine(wlanIndex,0); 
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, WmmEnable, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        BOOL enable = _ansc_atoi(strValue);
        pCfg->WMMEnable = enable;
        if (enabled == TRUE) {
            wifi_setWmmEnable(wlanIndex, enable);
        }
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, UAPSDEnable, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        BOOL enable = _ansc_atoi(strValue);
        pCfg->UAPSDEnable = enable;
        if (enabled == TRUE) {
            wifi_setWmmUapsdEnable(wlanIndex, enable);
        }
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    // For Backwards compatibility with 1.3 versions, the PSM value for NoAck must be 1
    // When set/get from the PSM to DML the value must be interperted to the opposite
    // 1->0 and 0->1
    memset(recName, 0, sizeof(recName));
    sprintf(recName, WmmNoAck, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        intValue = _ansc_atoi(strValue);
        pCfg->WmmNoAck = !intValue;

        if (enabled == TRUE) {
            wifi_setWmmOgAckPolicy(wlanIndex, 0, intValue);
			wifi_setWmmOgAckPolicy(wlanIndex, 1, intValue);
			wifi_setWmmOgAckPolicy(wlanIndex, 2, intValue);
			wifi_setWmmOgAckPolicy(wlanIndex, 3, intValue);
        }
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, BssMaxNumSta, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        intValue = _ansc_atoi(strValue);
        pCfg->BssMaxNumSta = intValue;
        if (enabled == TRUE) {
            wifi_setMaxStations(wlanIndex, intValue);
        }
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, ApIsolationEnable, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        BOOL enable = atoi(strValue);
        BOOL disable = !enable;
        pCfg->IsolationEnable = enable;
        if (enabled == TRUE) {
            wifi_setApBridging(wlanIndex, disable);
        }
        printf("%s: wifi_setApBridging %d, %d \n", __FUNCTION__, wlanIndex, disable);
	    ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    return ANSC_STATUS_SUCCESS;
    }

ANSC_STATUS
CosaDmlWiFiSetAccessPointPsmData
(
PCOSA_DML_WIFI_AP_CFG       pCfg
) {
    char strValue[256];
    char recName[256];
    int intValue;
    int retPsmSet;
    ULONG                       wlanIndex;
    ULONG                       ulInstance;
    PCOSA_DML_WIFI_AP_CFG        pStoredCfg  = &sWiFiDmlApStoredCfg[pCfg->InstanceNumber-1].Cfg;

    if (pCfg != NULL) {
        ulInstance = pCfg->InstanceNumber;
        wlanIndex = pCfg->InstanceNumber -1;
    } else {
        return ANSC_STATUS_FAILURE;
    }

    wifiDbgPrintf("%s g_Subsytem = %s\n",__FUNCTION__, g_Subsystem);

    if (pCfg->WMMEnable != pStoredCfg->WMMEnable) {
        sprintf(recName, WmmEnable, ulInstance);
        sprintf(strValue,"%d",pCfg->WMMEnable);
        retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting WmmEnable\n",__FUNCTION__, retPsmSet); 
        }
    }

    if (pCfg->UAPSDEnable != pStoredCfg->UAPSDEnable) {
        sprintf(recName, UAPSDEnable, ulInstance);
        sprintf(strValue,"%d",pCfg->UAPSDEnable);
        retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting UAPSDEnable\n",__FUNCTION__, retPsmSet); 
        }
    }

    // For Backwards compatibility with 1.3 versions, the PSM value for NoAck must be negated
    // When set/get from the PSM to DML the value must be interperted to the opposite
    // 1->0 and 0->1
    if (pCfg->WmmNoAck != pStoredCfg->WmmNoAck) {
        sprintf(recName, WmmNoAck, ulInstance);
        sprintf(strValue,"%d",!pCfg->WmmNoAck);
        retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting WmmNoAck\n",__FUNCTION__, retPsmSet); 
        }
    }

    if (pCfg->BssMaxNumSta != pStoredCfg->BssMaxNumSta) {
        sprintf(recName, BssMaxNumSta, ulInstance);
        sprintf(strValue,"%d",pCfg->BssMaxNumSta);
        retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting BssMaxNumSta\n",__FUNCTION__, retPsmSet); 
        }
    }

    if (pCfg->IsolationEnable != pStoredCfg->IsolationEnable) {
        sprintf(recName, ApIsolationEnable, ulInstance);
        sprintf(strValue,"%d",(pCfg->IsolationEnable == TRUE) ? 1 : 0 );
        retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting ApIsolationEnable \n",__FUNCTION__, retPsmSet); 
        }
    }

    if (pCfg->BssHotSpot != pStoredCfg->BssHotSpot) {
        sprintf(recName, BssHotSpot, ulInstance);
        sprintf(strValue,"%d",(pCfg->BssHotSpot == TRUE) ? 1 : 0 );
        retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting BssHotSpot \n",__FUNCTION__, retPsmSet); 
        }
    }

    return ANSC_STATUS_SUCCESS;
}

static ANSC_STATUS
CosaDmlWiFiGetBridgePsmData
    (
        void
    )
{
    PCOSA_DML_WIFI_SSID_BRIDGE  pBridge = NULL;
    char *strValue = NULL;
    char *ssidStrValue = NULL;
    int vlanId;
    char ipAddr[16];
    char ipSubNet[16];
    int numSSIDs = 0;
    char recName[256];
    int retPsmGet = CCSP_SUCCESS;
    int numInstances = 0;
    unsigned int *pInstanceArray = NULL;

    pBridgeVlanCfg = NULL;

    wifiDbgPrintf("%s g_Subsytem = %s  \n",__FUNCTION__, g_Subsystem );

    retPsmGet =  PsmGetNextLevelInstances ( bus_handle,g_Subsystem, l2netBridgeInstances, &numInstances, &pInstanceArray);
    wifiDbgPrintf("%s: Got %d  Bridge instances \n", __func__, numInstances );

    int i;
    for (i = 0; i < numInstances; i++) {
        int bridgeIndex = (int)pInstanceArray[i];
        wifiDbgPrintf("%s: Index %d is Bridge instances  %d \n", __func__, i, pInstanceArray[i] );

        pBridge = NULL;

        memset(recName, 0, sizeof(recName));
        sprintf(recName, l2netBridge, bridgeIndex);
        retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &ssidStrValue);
        if (retPsmGet == CCSP_SUCCESS) {
            char *ssidName = ssidStrValue;
            BOOL firstSSID = TRUE;
            int wlanIndex;
            int retVal;

            if (strlen(ssidName) > 0) {
                wifiDbgPrintf("%s: %s returned %s\n", __func__, recName, ssidName);

                ssidName = strtok(ssidName," ");
                while (ssidName != NULL) {

                    if (strstr(ssidName,"ath") != NULL) {

                        if (firstSSID == TRUE) {
                            firstSSID = FALSE;
                            pBridge = (PCOSA_DML_WIFI_SSID_BRIDGE)AnscAllocateMemory(sizeof(COSA_DML_WIFI_SSID_BRIDGE)*(1));
                            pBridge->InstanceNumber = bridgeIndex;
                            pBridge->SSIDCount = 0;

                            // Get the VlanId, IpAddress and Subnet once
                            memset(recName, 0, sizeof(recName));
                            sprintf(recName, l2netVlan, bridgeIndex);
                            retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
                            if (retPsmGet == CCSP_SUCCESS) {
                                wifiDbgPrintf("%s: %s returned %s\n", __func__, recName, strValue);
                                pBridge->VlanId =  _ansc_atoi(strValue);
                                ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
                            } else {
                                // No bridge with this id
                                return ANSC_STATUS_FAILURE;
                            }

                            memset(recName, 0, sizeof(recName));
                            sprintf(recName, l2netl3InstanceNum, bridgeIndex);
                            retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
                            if (retPsmGet == CCSP_SUCCESS) {
                                wifiDbgPrintf("%s: %s returned %s\n", __func__, recName, strValue);
                                int l3InstanceNum = _ansc_atoi(strValue);

                                memset(recName, 0, sizeof(recName));
                                sprintf(recName, l3netIpAddr, l3InstanceNum);
                                retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
                                if (retPsmGet == CCSP_SUCCESS) {
                                    wifiDbgPrintf("%s: %s returned %s\n", __func__, recName, strValue);
                                    sprintf(pBridge->IpAddress, "%s", strValue);
                                    ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
                                } else {
                                    sprintf(pBridge->IpAddress, "0.0.0.0");
                                }

                                memset(recName, 0, sizeof(recName));
                                sprintf(recName, l3netIpSubNet, l3InstanceNum);
                                retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
                                if (retPsmGet == CCSP_SUCCESS) {
                                    wifiDbgPrintf("%s: %s returned %s\n", __func__, recName, strValue);
                                    sprintf(pBridge->IpSubNet, "%s", strValue);
                                    ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
                                } else {
                                    // No bridge with this id
                                    sprintf(pBridge->IpSubNet, "255.255.255.0", strValue);
                                }
                            } else {
                                // If no link between l2net and l3net is found set to default values
                                wifiDbgPrintf("%s: %s returned %s\n", __func__, recName, strValue);
                                sprintf(pBridge->IpAddress, "0.0.0.0");
                                sprintf(pBridge->IpSubNet, "255.255.255.0", strValue);
                            }

                        }

                        sprintf(pBridge->SSIDName[pBridge->SSIDCount],"%s", ssidName);
                        wifiDbgPrintf("%s: ssidName  = %s \n", __FUNCTION__, ssidName);

                        retVal = wifi_getIndexFromName(pBridge->SSIDName[pBridge->SSIDCount], &wlanIndex);
                        if (retVal == 0) {
                            char bridgeName[32];
                            sprintf(bridgeName, "br%d", pBridge->InstanceNumber-1);
                            // sprintf(bridgeName, "br%d", pBridge->VlanId);
                            
                            // Determine if bridge, ipaddress or subnet changed.  If so flag it and save it 
                            {
                                char bridge[16];
                                char ip[32];
                                char ipSubNet[32];
                                wifi_getBridgeInfo(wlanIndex, bridge, ip, ipSubNet);
                                if (strcmp(ip,pBridge->IpAddress) != 0 ||
                                    strcmp(ipSubNet,pBridge->IpSubNet) != 0 ||
                                    strcmp(bridge, bridgeName) != 0) {
                                    wifi_setBridgeInfo(wlanIndex,bridgeName, pBridge->IpAddress, pBridge->IpSubNet);
                                    wifi_setVlanID(wlanIndex,pBridge->VlanId);
                                    sWiFiDmlUpdateVlanCfg[wlanIndex] = TRUE;
                                }
                            }
                        }

                        pBridge->SSIDCount++;

                    }
                    ssidName = strtok('\0',",");
                }
            }
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(ssidStrValue);
        }
        // Added to gload list
        if (pBridge) {
            pBridge->next = pBridgeVlanCfg;
            pBridgeVlanCfg = pBridge;
        }
    }

    if (pInstanceArray) {
        AnscFreeMemory(pInstanceArray);
    }

    return ANSC_STATUS_SUCCESS;
}
ANSC_STATUS
CosaDmlWiFiFactoryReset
    (
    )
{
    int i;
    char recName[256];
    char recValue[256];
    char *strValue = NULL;
    int retPsmGet = CCSP_SUCCESS;
    int resetSSID[2] = {0,0};

    for (i = 1; i <= gRadioCount; i++)
    {
        memset(recName, 0, sizeof(recName));
        sprintf(recName, FactoryResetSSID, i);
        retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
        if (retPsmGet == CCSP_SUCCESS)
        {
            resetSSID[i-1] = atoi(strValue);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
        }
        // Reset to 0
        PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "0");

        wifiDbgPrintf("%s: Radio %d has resetSSID = %d \n", __FUNCTION__, i, resetSSID[i-1]);
    }

    // reset all SSIDs
    if (resetSSID[0] == 0 && resetSSID[1] == 0)
    {
        // delete current configuration
        wifi_factoryReset();

        // create current configuration
        // It is neccessary to first recreate the config
        // from the defaults and next we will override them with the Platform specific data
        wifi_createInitialConfigFiles();

        // These are the values PSM values that were generated from the ARM intel db
        // modidify current configuration
        for (i = 0; i < gRadioCount; i++)
        {
            CosaDmlWiFiGetRadioFactoryResetPsmData(i, i+1);
        }

        for (i = 0; i < gSsidCount; i++)
        {
            CosaDmlWiFiGetSSIDFactoryResetPsmData(i, i+1);
        }

        CosaDmlWiFiGetBridgePsmData();

        BOOLEAN newVlanCfg = FALSE;
        CosaDmlWiFiGetResetRequired(&newVlanCfg);
        // if new Vlan configuration is required, write the new cfg version to PSM
        // wifi_init() is always called in this function
        if (newVlanCfg == TRUE)
        {
            char verString[32];
            sprintf(verString, "%d",gWifiVlanCfgVersion);
            PSM_Set_Record_Value2(bus_handle,g_Subsystem, WifiVlanCfgVersion, ccsp_string, verString);
        }
    } else
    {
        // Only Apply to FactoryResetSSID list
        int ssidIndex = 0;
        for (ssidIndex = 0; ssidIndex < gSsidCount; ssidIndex++)
        {
            int radioIndex = (ssidIndex %2);
            printf("%s: ssidIndex = %d radioIndex = %d (1<<(ssidIndex/2)) & resetSSID[radioIndex] = %d \n", __FUNCTION__,  ssidIndex, radioIndex, ((1<<(ssidIndex/2)) & resetSSID[radioIndex] ));
            if ( ((1<<(ssidIndex/2)) & resetSSID[radioIndex] ) != 0)
            {
                CosaDmlWiFiGetSSIDFactoryResetPsmData(ssidIndex, ssidIndex+1);
            }
        }

        // Reset radio parameters
        wifi_factoryResetRadios();
        for (i = 0; i < gRadioCount; i++)
        {
            CosaDmlWiFiGetRadioFactoryResetPsmData(i, i+1);
        }

    }

    // Bring Radios Up again if we aren't doing PowerSaveMode
    if ( gRadioPowerSetting != COSA_DML_WIFI_POWER_DOWN &&
         gRadioNextPowerSetting != COSA_DML_WIFI_POWER_DOWN ) {
        wifi_init();
    }

    // Set FixedWmmParams to TRUE on Factory Reset so that we won't override the data.
    // There were two required changes.  Set to 3 so that we know neither needs to be applied
    PSM_Set_Record_Value2(bus_handle,g_Subsystem, FixedWmmParams, ccsp_string, "3");

    return ANSC_STATUS_SUCCESS;
}

static void *CosaDmlWiFiResetRadiosThread(void *arg) 
{
    pthread_detach(pthread_self());
    printf("%s Calling pthread_mutex_lock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 
    pthread_mutex_lock(&sWiFiThreadMutex);
    printf("%s Called pthread_mutex_lock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 

    // Restart Radios again if we aren't doing PowerSaveMode
    if ( gRadioPowerSetting != COSA_DML_WIFI_POWER_DOWN &&
         gRadioNextPowerSetting != COSA_DML_WIFI_POWER_DOWN ) {
        printf("%s: Calling wifi_reset  \n", __func__);
        wifi_reset();

        wifiDbgPrintf("%s Calling Initialize() \n",__FUNCTION__);

        pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
        CosaWifiReInitialize((ANSC_HANDLE)pMyObject, 0);
        CosaWifiReInitialize((ANSC_HANDLE)pMyObject, 1);

        wifiDbgPrintf("%s Called Initialize() \n",__FUNCTION__);
    }

    printf("%s Calling pthread_mutex_unlock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 
    pthread_mutex_unlock(&sWiFiThreadMutex);
    printf("%s Called pthread_mutex_unlock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ );  

    return(NULL);
}

ANSC_STATUS
CosaDmlWiFi_ResetRadios
    (
    )
{
    printf("%s: \n", __func__);

    {
        pthread_t tid; 

        printf("%s Reset WiFi in background.  Process will take upto 90 seconds to complete  \n",__FUNCTION__ ); 

        if (pthread_create(&tid,NULL,CosaDmlWiFiResetRadiosThread,NULL))
        {
            return ANSC_STATUS_FAILURE;
        }
    }

    return ANSC_STATUS_SUCCESS;
}

static void CosaDmlWiFiCheckWmmParams
    (
    )
{
    char recName[256];
    char recValue[256];
    char *strValue = NULL;
    int retPsmGet = CCSP_SUCCESS;
    BOOL resetNoAck = FALSE;

printf("%s \n",__FUNCTION__);

    // if the value is FALSE or not present WmmNoAck values should be reset
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, FixedWmmParams, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        int value = atoi(strValue);
        if (value != 3) {
            resetNoAck = TRUE;
        }
    } else {
        resetNoAck = TRUE;
    }

    // Force NoAck to 1 for now.  There are upgrade/downgrade issues that sometimes cause an issue with the values being inconsistent 
    resetNoAck = TRUE;

    if (resetNoAck == TRUE) {
        int i;

        printf("%s: Resetting Wmm parameters \n",__FUNCTION__);

        for (i =0; i < 16; i++) {
            memset(recName, 0, sizeof(recName));
            sprintf(recName, WmmEnable, i+1);
            PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "1");
            memset(recName, 0, sizeof(recName));
            sprintf(recName, UAPSDEnable, i+1);
            PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "1");
            // For Backwards compatibility with 1.3 versions, the PSM value for NoAck must be 1
            // When set/get from the PSM to DML the value must be interperted to the opposite
            // 1->0 and 0->1
            memset(recName, 0, sizeof(recName));
            sprintf(recName, WmmNoAck, i+1);
            PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "1");
        }
    }

    // Set FixedWmmParams to TRUE so that we won't override the data again.
    PSM_Set_Record_Value2(bus_handle,g_Subsystem, FixedWmmParams, ccsp_string, "3");
}

static void CosaDmlWiFiCheckSecurityParams
(
)
{
    char recName[256];
    char *strValue = NULL;
    int retPsmGet = CCSP_SUCCESS;
    BOOL resetNoAck = FALSE;
    int wlanIndex;
    char ssid[64];
    unsigned int wpsPin;
    char pskKey[64];

    printf("%s \n",__FUNCTION__);

    for (wlanIndex = 0; wlanIndex < 2; wlanIndex++)
    {
        wpsPin = 0;
        wifi_getWpsDevicePIN(wlanIndex,&wpsPin);
        printf("%s  called wifi_getWpsDevicePIN on ath%d\n",__FUNCTION__, wlanIndex);
        if (wpsPin == 0)
        {
            unsigned int password = 0;
            retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, WpsPin, NULL, &strValue);
            if (retPsmGet == CCSP_SUCCESS)
            {
                password = _ansc_atoi(strValue);
                wifi_setWpsDevicePIN(wlanIndex, password);
                ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
            }
        }

        pskKey[0] = '\0';
        wifi_getKeyPassphrase(wlanIndex,pskKey);
        if (strlen(pskKey) == 0)
        {
            memset(recName, 0, sizeof(recName));
            sprintf(recName, Passphrase, wlanIndex+1);
            retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
            if (retPsmGet == CCSP_SUCCESS)
            {
                wifi_setKeyPassphrase(wlanIndex, strValue);
                ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
            }
        }
    }
}


ANSC_STATUS
CosaDmlWiFiInit
    (
        ANSC_HANDLE                 hDml,
        PANSC_HANDLE                phContext
    )
{
printf("%s \n",__FUNCTION__);
    BOOLEAN factoryResetFlag = FALSE;
    int i;
    static BOOL firstTime = TRUE;

    CosaDmlWiFiGetFactoryResetPsmData(&factoryResetFlag);
    if (factoryResetFlag == TRUE) {
printf("%s: Calling CosaDmlWiFiFactoryReset \n",__FUNCTION__);
	CosaDmlWiFiFactoryReset();
printf("%s: Called CosaDmlWiFiFactoryReset \n",__FUNCTION__);
        // Set to FALSE after FactoryReset has been applied
	PSM_Set_Record_Value2(bus_handle,g_Subsystem, FactoryReset, ccsp_string, "0");
printf("%s: Reset FactoryReset to 0 \n",__FUNCTION__);
    } 

    // Only do once and store BSSID and MacAddress in memory
    if (firstTime == TRUE) {

        firstTime = FALSE;

        CosaDmlWiFiCheckSecurityParams();
        CosaDmlWiFiCheckWmmParams();

        // Fill Cache
        CosaDmlWiFiSsidFillSinfoCache(NULL, 1);
        CosaDmlWiFiSsidFillSinfoCache(NULL, 2);

        // Temporary fix for HotSpot builds prior to 11/22/2013 had a bug that cuased the 
        // HotSpot param of the Primary SSIDs to be set to 1.  
        BOOLEAN resetHotSpot = FALSE;
        CosaDmlWiFiGetResetHotSpotRequired(&resetHotSpot);
        // if new Vlan configuration is required, write the new cfg version to PSM 
        if (resetHotSpot == TRUE) {      
            char recName[256];

            int i;
            for (i = 1; i <= gRadioCount; i++) {
                sprintf(recName, BssHotSpot, i);               
                PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "0");
                wifi_setEnableOnLine(i-1,0);
            }            
        }

        // If no VAPs were up or we have new Vlan Cfg re-init both Radios
        if ( resetHotSpot == TRUE && 
             gRadioPowerSetting != COSA_DML_WIFI_POWER_DOWN &&
             gRadioNextPowerSetting != COSA_DML_WIFI_POWER_DOWN ) {
            printf("%s: calling wifi_init  \n", __func__);
            wifi_init();
        }

        BOOLEAN noEnableVaps = TRUE;
                BOOL radioActive = TRUE;
        wifi_getRadioActive(0, &radioActive);
        printf("%s: radioActive wifi0 = %s \n", __func__, (radioActive == TRUE) ? "TRUE" : "FALSE");
        if (radioActive == TRUE) {
            noEnableVaps = FALSE;
        }
        wifi_getRadioActive(1,&radioActive);
        printf("%s: radioActive wifi1 = %s \n", __func__, (radioActive == TRUE) ? "TRUE" : "FALSE");
        if (radioActive == TRUE) {
            noEnableVaps = FALSE;
        }
        printf("%s: noEnableVaps = %s \n", __func__, (noEnableVaps == TRUE) ? "TRUE" : "FALSE");

        CosaDmlWiFiGetBridgePsmData();
        BOOLEAN newVlanCfg = FALSE;
        CosaDmlWiFiGetResetRequired(&newVlanCfg);
        // if new Vlan configuration is required, write the new cfg version to PSM 
        if (newVlanCfg == TRUE) {
            char verString[32];
            sprintf(verString, "%d",gWifiVlanCfgVersion);
            PSM_Set_Record_Value2(bus_handle,g_Subsystem, WifiVlanCfgVersion, ccsp_string, verString);
        }

        // If no VAPs were up or we have new Vlan Cfg re-init both Radios
        if ( (noEnableVaps == TRUE || newVlanCfg == TRUE)  &&
             gRadioPowerSetting != COSA_DML_WIFI_POWER_DOWN &&
             gRadioNextPowerSetting != COSA_DML_WIFI_POWER_DOWN ) {
            wifi_init();
        }
    }


    BOOL retInit = Cosa_Init(bus_handle);
    printf("%s: Cosa_Init returned %s \n", __func__, (retInit == 1) ? "True" : "False");

    return ANSC_STATUS_SUCCESS;
}

static ANSC_STATUS
CosaDmlWiFiPsmDelMacFilterTable( ULONG ulInstance )
{
    char recName[256];
    char *strValue = NULL;
    int retPsmGet;
   
wifiDbgPrintf("%s ulInstance = %d\n",__FUNCTION__, ulInstance);

    sprintf(recName, MacFilterList, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        int numFilters = 0;
        int macInstance = 0;
        char *mac;

        if (strlen(strValue) > 0) {
            char *start = strValue;
            char *end = NULL;
        
            end = strValue + strlen(strValue);

            if ((end = strstr(strValue, ":" ))) {
                *end = 0;
             
		numFilters = _ansc_atoi(start);
		wifiDbgPrintf("%s numFilters = %d \n",__FUNCTION__, numFilters);
                start = end+1;

                if (numFilters > 0 && strlen(start) > 0) {
		    wifiDbgPrintf("%s filterList = %s \n",__FUNCTION__, start);
                }

                while ((end = strstr(start,","))) {
                    *end = 0;
		    macInstance = _ansc_atoi(start);
		    wifiDbgPrintf("%s macInstance  = %d \n", __FUNCTION__, macInstance);
		    start = end+1;

		    sprintf(recName, MacFilter, ulInstance, macInstance);
		    PSM_Del_Record(bus_handle,g_Subsystem, recName);
		    sprintf(recName, MacFilterDevice, ulInstance, macInstance);
		    PSM_Del_Record(bus_handle,g_Subsystem, recName);
                }

		// get last one
                if (strlen(start) > 0) {
		    macInstance = _ansc_atoi(start);
		    wifiDbgPrintf("%s macInstance  = %d \n", __FUNCTION__, macInstance);

		    sprintf(recName, MacFilter, ulInstance, macInstance);
		    PSM_Del_Record(bus_handle,g_Subsystem, recName);
		    sprintf(recName, MacFilterDevice, ulInstance, macInstance);
		    PSM_Del_Record(bus_handle,g_Subsystem, recName);
                }
	    } 
        }
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);

    } 
    sprintf(recName, MacFilterList, ulInstance);
    PSM_Del_Record(bus_handle,g_Subsystem,recName);

    sprintf(recName, MacFilterMode, ulInstance);
    PSM_Del_Record(bus_handle,g_Subsystem,recName);

    return ANSC_STATUS_SUCCESS;
}

static void *CosaDmlWiFiFactoryResetThread(void *arg) 
{
    pthread_detach(pthread_self());
    printf("%s Calling pthread_mutex_lock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 
    pthread_mutex_lock(&sWiFiThreadMutex);
    printf("%s Called pthread_mutex_lock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 

    PSM_Set_Record_Value2(bus_handle,g_Subsystem, ReloadConfig, ccsp_string, "TRUE");

    wifiDbgPrintf("%s Calling Initialize() \n",__FUNCTION__);

    pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    CosaWifiReInitialize((ANSC_HANDLE)pMyObject, 0);
    CosaWifiReInitialize((ANSC_HANDLE)pMyObject, 1);

    wifiDbgPrintf("%s Called Initialize() \n",__FUNCTION__);

	// Notify WiFiExtender that WiFi parameter have changed
	{
	    CosaDml_NotifyWiFiExt(COSA_WIFIEXT_DM_UPDATE_RADIO|COSA_WIFIEXT_DM_UPDATE_WPS|COSA_WIFIEXT_DM_UPDATE_SSID);
	}

    printf("%s Calling pthread_mutex_unlock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 
    pthread_mutex_unlock(&sWiFiThreadMutex);
    printf("%s Called pthread_mutex_unlock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ );  

    return(NULL);
}

ANSC_STATUS
CosaDmlWiFi_FactoryReset()
{
    char recName[256];
    char recValue[256];
    char *strValue = NULL;
    int retPsmGet = CCSP_SUCCESS;
    int resetSSID[2] = {0,0};
    int i = 0;
printf("%s g_Subsytem = %s\n",__FUNCTION__,g_Subsystem);

    // This function is only called when the WiFi DML FactoryReset is set
    // From this interface only reset the UserControlled SSIDs
    for (i = 1; i <= gRadioCount; i++)
    {
        memset(recName, 0, sizeof(recName));
        sprintf(recName, UserControl, i);
        retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
        if (retPsmGet == CCSP_SUCCESS) {
            resetSSID[i-1] = atoi(strValue);
printf("%s: resetSSID[%d] = %d \n", __FUNCTION__, i-1,  resetSSID[i-1]);
            memset(recName, 0, sizeof(recName));
            sprintf(recName, FactoryResetSSID, i);
            PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
        } 
    }

    // Delete PSM entries for Wifi Primary SSIDs related values
#if 0
    for (i = 1; i <= gRadioCount; i++) {
	sprintf(recName, CTSProtection, i);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, BeaconInterval, i);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, DTIMInterval, i);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, FragThreshold, i);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, RTSThreshold, i);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, ObssCoex, i);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);
    }
#endif
    for (i = 1; i <= gSsidCount; i++)
    {
        int ssidIndex = i -1;
	int radioIndex = (ssidIndex %2);
	printf("%s: ssidIndex = %d radioIndex = %d (1<<(ssidIndex/2)) & resetSSID[radioIndex] = %d \n", __FUNCTION__,  ssidIndex, radioIndex, ((1<<(ssidIndex/2)) & resetSSID[radioIndex] ));
	if ( ((1<<(ssidIndex/2)) & resetSSID[radioIndex] ) != 0)
	{
printf("%s: deleting records for index %d \n", __FUNCTION__, i);
	    sprintf(recName, WmmEnable, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, UAPSDEnable, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, WmmNoAck, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, BssMaxNumSta, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, BssHotSpot, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    CosaDmlWiFiPsmDelMacFilterTable(i);

	    // Platform specific data that is stored in the ARM Intel DB and converted to PSM entries
	    // They will be read only on Factory Reset command and override the current Wifi configuration
	    sprintf(recName, RadioIndex, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, WlanEnable, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, BssSsid, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, HideSsid, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, SecurityMode, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, EncryptionMethod, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, Passphrase, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, WmmRadioEnable, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, WpsEnable, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);

	    sprintf(recName, Vlan, i);
	    PSM_Del_Record(bus_handle,g_Subsystem,recName);
        }
    }

    PSM_Del_Record(bus_handle,g_Subsystem,WpsPin);

    PSM_Del_Record(bus_handle,g_Subsystem,FactoryReset);

    PSM_Reset_UserChangeFlag(bus_handle,g_Subsystem,"Device.WiFi.");

    {
        pthread_t tid; 

        printf("%s Factory Reset WiFi  \n",__FUNCTION__ ); 

        if (pthread_create(&tid,NULL,CosaDmlWiFiFactoryResetThread,NULL))
        {
            return ANSC_STATUS_FAILURE;
        }
    }

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_EnableTelnet(BOOL bEnabled)
{

    if (bEnabled) {
	// Attempt to start the telnet daemon on ATOM
        if ( system("/usr/sbin/telnetd -b 192.168.101.3") != 0 ) {
	    return ANSC_STATUS_FAILURE;
	}
    }

    else {
        // Attempt to kill the telnet daemon on ATOM
        if ( system("pkill telnetd") != 0 ) {
	    return ANSC_STATUS_FAILURE;
        }
    }

    return ANSC_STATUS_SUCCESS;

}

#if 0
#include "intel_ce_pm.h"

BOOL sCosaDmlWiFi_AtomInSuspension = FALSE;

static int CosaDmlWiFi_PsmEventHandler(icepm_event_t event, void * cookie)
{
    switch (event) {
    case ICEPM_EVT_SUSPEND:
        // printf("%s: Got event that Atom is being suspended\n",__FUNCTION__);
        sCosaDmlWiFi_AtomInSuspension = TRUE; 

        break;

    case ICEPM_EVT_RESUME:
        printf("%s: Got event that Atom is being resumed from suspended \n",__FUNCTION__);

        sCosaDmlWiFi_AtomInSuspension = FALSE; 
        // Execute resume logic here

        break;
    case ICEPM_EVT_INTERRUPTED:
        // Should only happen if application is killed.
        printf("%s: Process was killed \n",__FUNCTION__);
        break;
    default:
        // Should not happen
        break;
    }
    return 0;
}

static void CosaDmlWiFi_RegisterPsmEventHandler(void)
{
    printf("%s: Entered \n",__FUNCTION__);
    icepm_ret_t rc = icepm_register_callback(CosaDmlWiFi_PsmEventHandler, "CcspPandM", NULL);
    if (rc != ICEPM_OK) {
        printf("%s: Process was killed \n",__FUNCTION__);
    }
}
#endif

static ANSC_STATUS
CosaDmlWiFi_ShutdownAtom(void)
{ 
    int fd;
    int ret = 0;

    switch(fork()) {
    case 0: /*Child*/
        {
	    wifiDbgPrintf("%s: write mem to /sys/power/state to suspend Atom cpu\n",__FUNCTION__);
            char *myArgv[] = { "suspendAtom.sh", NULL};
            execvp("/etc/ath/suspendAtom.sh", myArgv);
            exit(-1);
        }
    default:
        break;
    }

    return ANSC_STATUS_SUCCESS;
}

static void * CosaDmlWifi_RadioPowerThread(void *arg)
{
    pthread_detach(pthread_self());
    COSA_DML_WIFI_RADIO_POWER power = gRadioNextPowerSetting;

    printf("%s Calling pthread_mutex_lock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 
    pthread_mutex_lock(&sWiFiThreadMutex);
    printf("%s Called pthread_mutex_lock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 

    if (power != gRadioPowerSetting) {

        switch (power) {
        case COSA_DML_WIFI_POWER_DOWN:
            wifi_down();
            gRadioPowerState[0] = power;
            gRadioPowerState[1] = power;
            gRadioPowerSetting = power;

            CosaDmlWiFi_ShutdownAtom();
            break;
        case COSA_DML_WIFI_POWER_UP:
            // Only call wifi_init if the radios are currently down
            // When the DML is reint, the radios will be brought to full power
            if (gRadioPowerSetting == COSA_DML_WIFI_POWER_DOWN) {
                wifi_init();
            }
            gRadioPowerState[0] = power;
            gRadioPowerState[1] = power;
            gRadioPowerSetting = power;
            wifiDbgPrintf("%s Calling Initialize() \n",__FUNCTION__);
            pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
            CosaWifiReInitialize((ANSC_HANDLE)pMyObject, 0);
	    CosaWifiReInitialize((ANSC_HANDLE)pMyObject, 1);
            wifiDbgPrintf("%s Called Initialize() \n",__FUNCTION__);
            break;
        case COSA_DML_WIFI_POWER_LOW:
            wifi_setTransmitPower(0, 5);
            wifi_setTransmitPower(1, 5);
            gRadioPowerState[0] = power;
            gRadioPowerState[1] = power;
            gRadioPowerSetting = power;
            wifiDbgPrintf("%s Calling Initialize() \n",__FUNCTION__);
            pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
            CosaWifiReInitialize((ANSC_HANDLE)pMyObject, 0);
	    CosaWifiReInitialize((ANSC_HANDLE)pMyObject, 1);
            wifiDbgPrintf("%s Called Initialize() \n",__FUNCTION__);
            break;
        default:
            printf("%s Got invalid Radio Power %d, not updating the power \n",__FUNCTION__, power);
        }

    } else {
        unsigned int ath0Power = 0, ath1Power = 0;

        wifi_getTransmitPower(0,&ath0Power);
        wifi_getTransmitPower(1, &ath1Power);

        if ( ( power == COSA_DML_WIFI_POWER_UP ) &&
             ( (ath0Power == 5 ) || (ath1Power == 5) ) ) {
            // Power may have been lowered with interrupt, but PSM didn't send power down
            // may happen if AC is reconnected before it notifies WiFi or
            // because Docsis was not operational
            // Calling CosaWifiReInitialize() will bring it back to full power
            wifiDbgPrintf("%s Calling Initialize() \n",__FUNCTION__);
            pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
            CosaWifiReInitialize((ANSC_HANDLE)pMyObject, 0);
		    CosaWifiReInitialize((ANSC_HANDLE)pMyObject, 1);
            wifiDbgPrintf("%s Called Initialize() \n",__FUNCTION__);
        } else {
            printf("%s Noting to do.  Power was already in desired state %d(%d) \n",__FUNCTION__, gRadioPowerSetting, power);
        }
    }

    printf("%s Calling pthread_mutex_unlock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 
    pthread_mutex_unlock(&sWiFiThreadMutex);
    printf("%s Called pthread_mutex_unlock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 

    return(NULL);
}

ANSC_STATUS
CosaDmlWiFi_GetRadioPower(COSA_DML_WIFI_RADIO_POWER *power)
{ 
    *power = gRadioPowerSetting;
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_SetRadioPower(COSA_DML_WIFI_RADIO_POWER power)
{ 
    pthread_t tid; 

    printf("%s Changing Radio Power to = %d\n",__FUNCTION__, power); 

    gRadioNextPowerSetting = power;

    if(pthread_create(&tid,NULL,CosaDmlWifi_RadioPowerThread,NULL))  {
        return ANSC_STATUS_FAILURE;
    }

    return ANSC_STATUS_SUCCESS;
}

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
    return gRadioCount;
}    
    
ANSC_STATUS
CosaDmlWiFiRadioGetSinfo
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber,
        PCOSA_DML_WIFI_RADIO_SINFO  pInfo
    )
{
wifiDbgPrintf("%s\n",__FUNCTION__);
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    // Atheros Radio index start at 0, not 1 as in the DM
    int wlanIndex = ulInstanceNumber-1;

    if (!pInfo)
    {
        return ANSC_STATUS_FAILURE;
    }
    
    sprintf(pInfo->Name, "wifi%d", wlanIndex);
    pInfo->bUpstream = TRUE;

    //  Currently this is not working
    { 
	char maxBitRate[32];
	wifi_getMaxBitRate(wlanIndex, maxBitRate);
	wifiDbgPrintf("%s: wifi_getMaxBitRate returned %s\n", __FUNCTION__, maxBitRate);
	if (strncmp(maxBitRate,"Auto",strlen("Auto")) == 0)
	{ 
	    pInfo->MaxBitRate = COSA_DML_WIFI_TXRATE_Auto;

	} else if (strncmp(maxBitRate,"6M",strlen("6M")) == 0)
	{
	    pInfo->MaxBitRate = COSA_DML_WIFI_TXRATE_6M;
	    
	} else if (strncmp(maxBitRate,"9M",strlen("9M")) == 0)
	{
	    pInfo->MaxBitRate = COSA_DML_WIFI_TXRATE_9M;
	    
	} else if (strncmp(maxBitRate,"12M",strlen("12M")) == 0)
	{
	    pInfo->MaxBitRate = COSA_DML_WIFI_TXRATE_12M;
	    
	} else if (strncmp(maxBitRate,"18M",strlen("18M")) == 0)
	{
	    pInfo->MaxBitRate = COSA_DML_WIFI_TXRATE_18M;
	    
	} else if (strncmp(maxBitRate,"24M",strlen("24M")) == 0)
	{
	    pInfo->MaxBitRate = COSA_DML_WIFI_TXRATE_24M;
	    
	} else if (strncmp(maxBitRate,"36M",strlen("36M")) == 0)
	{
	    pInfo->MaxBitRate = COSA_DML_WIFI_TXRATE_36M;
	    
	} else if (strncmp(maxBitRate,"48M",strlen("48M")) == 0)
	{
	    pInfo->MaxBitRate = COSA_DML_WIFI_TXRATE_48M;
	    
	} else if (strncmp(maxBitRate,"54M",strlen("54M")) == 0)
	{
	    pInfo->MaxBitRate = COSA_DML_WIFI_TXRATE_54M;
	    
	}
    }

    char frequencyBand[10];
    wifi_getSupportedFrequencyBands(wlanIndex, frequencyBand);
    if (strstr(frequencyBand,"2.4G") != NULL) {
        pInfo->SupportedStandards = COSA_DML_WIFI_STD_b | COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n;
        pInfo->SupportedFrequencyBands = COSA_DML_WIFI_FREQ_BAND_2_4G; /* Bitmask of COSA_DML_WIFI_FREQ_BAND */
        pInfo->IEEE80211hSupported     = FALSE;
    } else if (strstr(frequencyBand,"5G_11N") != NULL) {
        pInfo->SupportedStandards = COSA_DML_WIFI_STD_a | COSA_DML_WIFI_STD_n;
        pInfo->SupportedFrequencyBands = COSA_DML_WIFI_FREQ_BAND_5G; /* Bitmask of COSA_DML_WIFI_FREQ_BAND */
        pInfo->IEEE80211hSupported     = TRUE;	    
	} else if (strstr(frequencyBand,"5G_11AC") != NULL) {
        pInfo->SupportedStandards = COSA_DML_WIFI_STD_a | COSA_DML_WIFI_STD_n | COSA_DML_WIFI_STD_ac;
        pInfo->SupportedFrequencyBands = COSA_DML_WIFI_FREQ_BAND_5G; /* Bitmask of COSA_DML_WIFI_FREQ_BAND */
        pInfo->IEEE80211hSupported     = TRUE;
    } else {
        // if we can't determine frequency band assue wifi0 is 2.4 and wifi1 is 5 11n
        if (wlanIndex == 0)
    {
        pInfo->SupportedStandards = COSA_DML_WIFI_STD_b | COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n;
        pInfo->SupportedFrequencyBands = COSA_DML_WIFI_FREQ_BAND_2_4G; /* Bitmask of COSA_DML_WIFI_FREQ_BAND */
        pInfo->IEEE80211hSupported     = FALSE;
    }
    else 
    {
        pInfo->SupportedStandards = COSA_DML_WIFI_STD_a | COSA_DML_WIFI_STD_n;
        pInfo->SupportedFrequencyBands = COSA_DML_WIFI_FREQ_BAND_5G; /* Bitmask of COSA_DML_WIFI_FREQ_BAND */
        pInfo->IEEE80211hSupported     = TRUE;
        }
    }

    wifi_getPossibleChannels(wlanIndex, pInfo->PossibleChannels);
    
    pInfo->AutoChannelSupported = TRUE;

    sprintf(pInfo->TransmitPowerSupported, "12,25,50,75,100");

    return ANSC_STATUS_SUCCESS;
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
wifiDbgPrintf("%s\n",__FUNCTION__);
    PCOSA_DML_WIFI_RADIO_FULL       pWifiRadio      = pEntry;
    PCOSA_DML_WIFI_RADIO_CFG        pWifiRadioCfg   = &pWifiRadio->Cfg;
    PCOSA_DML_WIFI_RADIO_SINFO      pWifiRadioSinfo = &pWifiRadio->StaticInfo;
    PCOSA_DML_WIFI_RADIO_DINFO      pWifiRadioDinfo = &pWifiRadio->DynamicInfo;
    /*PPOAM_COSAWIFIDM_OBJECT*/ANSC_HANDLE         pPoamWiFiDm     = (/*PPOAM_COSAWIFIDM_OBJECT*/ANSC_HANDLE)hContext;

    pWifiRadio->Cfg.InstanceNumber = ulIndex+1;
    CosaDmlWiFiRadioSetDefaultCfgValues(hContext,ulIndex,pWifiRadioCfg);
    CosaDmlWiFiRadioGetCfg(NULL, pWifiRadioCfg);
    CosaDmlWiFiRadioGetSinfo(NULL, pWifiRadioCfg->InstanceNumber, pWifiRadioSinfo);    
    CosaDmlWiFiRadioGetDinfo(NULL, pWifiRadioCfg->InstanceNumber, pWifiRadioDinfo);    

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiRadioSetDefaultCfgValues
    (
        ANSC_HANDLE                 hContext,
        unsigned long               ulIndex,
        PCOSA_DML_WIFI_RADIO_CFG    pCfg
    )
{
    if (0 == ulIndex)
    {
	strcpy(pCfg->Alias,"Radio0");
    } else
    {
        strcpy(pCfg->Alias,"Radio1");
    }
       
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

static ANSC_STATUS
CosaDmlWiFiRadioGetTransmitPowerPercent
    (
        int wlanIndex,
        int *transmitPowerPercent
    )
{
    int percent = 0;
    int curTransmitPower;
    int maxTransmitPower;
    int retries = 0;

    wifi_getTransmitPower(wlanIndex, &curTransmitPower);

    // If you set to > than the max it sets to max - Atheros logic
    wifi_setTransmitPower(wlanIndex, 30);
    wifi_getTransmitPower(wlanIndex, &maxTransmitPower);
    while ( (retries < 5) && ( (maxTransmitPower <= 5) || (maxTransmitPower >= 30) ) ) {
          wifiDbgPrintf("%s: maxTransmitPower wifi%d = %d sleep and retry (%d) \n", __func__, wlanIndex, maxTransmitPower, retries);
          sleep(1);
          wifi_getTransmitPower(wlanIndex, &maxTransmitPower);
          retries++;
    } 
    wifi_setTransmitPower(wlanIndex, curTransmitPower);

    if (maxTransmitPower == curTransmitPower) percent = 100;
    else if ((maxTransmitPower-2) == curTransmitPower) percent = 75;
    else if ((maxTransmitPower-3) == curTransmitPower) percent = 50;
    else if ((maxTransmitPower-6) == curTransmitPower) percent = 25;
    else if ((maxTransmitPower-9) == curTransmitPower) percent = 12;

    // if a match was not found set to 100%
    if ( percent == 0 ) {
        // set to max Transmit Power
        percent = 100;
        wifi_setTransmitPower(wlanIndex, maxTransmitPower);
    }

    *transmitPowerPercent = percent;

    return ANSC_STATUS_SUCCESS;
}

static ANSC_STATUS
CosaDmlWiFiRadioSetTransmitPowerPercent
    (
        int wlanIndex,
        int transmitPowerPercent
    )
{
    int curTransmitPower;
    int maxTransmitPower;
    int transmitPower = 0;
    int retries = 0;

    wifiDbgPrintf("%s: enter wlanIndex %d transmitPowerPercent %d \n", __func__, wlanIndex, transmitPowerPercent);

    int ret = wifi_getTransmitPower(wlanIndex, &curTransmitPower);
    if (ret == 0) {

        // If you set to > than the max it sets to max - Atheros logic
        wifi_setTransmitPower(wlanIndex, 30);
        wifi_getTransmitPower(wlanIndex, &maxTransmitPower);
        while ( (retries < 5) && ((maxTransmitPower <= 5) || (maxTransmitPower >= 30) )  ) {
              wifiDbgPrintf("%s: maxTransmitPower wifi%d = %d sleep and retry (%d) \n", __func__, wlanIndex, maxTransmitPower, retries);
              sleep(1);
              wifi_getTransmitPower(wlanIndex, &maxTransmitPower);
              retries++;
        } 
        wifi_setTransmitPower(wlanIndex, curTransmitPower);
        wifiDbgPrintf("%s: maxTransmitPower wifi%d = %d \n", __func__, wlanIndex, maxTransmitPower);
        if (maxTransmitPower > 0) {
            if (transmitPowerPercent == 100) transmitPower = maxTransmitPower;
            if (transmitPowerPercent == 75) transmitPower = maxTransmitPower-2;
            if (transmitPowerPercent == 50) transmitPower = maxTransmitPower-3;
            if (transmitPowerPercent == 25) transmitPower = maxTransmitPower-6;
            if (transmitPowerPercent == 12) transmitPower = maxTransmitPower-9;

            wifiDbgPrintf("%s: transmitPower wifi%d = %d percent = %d \n", __func__, wlanIndex, transmitPower, transmitPowerPercent);

            if (transmitPower != 0) {
                wifi_setTransmitPower(wlanIndex, transmitPower);
            } 
        }
    }



    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiRadioPushCfg
    (
        PCOSA_DML_WIFI_RADIO_CFG    pCfg,        /* Identified by InstanceNumber */
        ULONG  wlanAthIndex,
        BOOLEAN firstVap
    )
{
    PCOSA_DML_WIFI_RADIO_CFG        pRunningCfg  = &sWiFiDmlRadioRunningCfg[pCfg->InstanceNumber-1];
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    wifiDbgPrintf("%s Config changes  \n",__FUNCTION__);
    int  wlanIndex;

    if (!pCfg )
    {
        return ANSC_STATUS_FAILURE;
    }

    wlanIndex = (ULONG) pCfg->InstanceNumber-1;  
    wifiDbgPrintf("%s[%d] Config changes  wlanIndex %d wlanAthIndex %d firstVap %s\n",__FUNCTION__, __LINE__, wlanIndex, wlanAthIndex, (firstVap==TRUE) ? "TRUE" : "FALSE");

    // Push parameters that are set on the wifi0/wifi1 interfaces if this is the first VAP enabled on the radio
    // iwpriv / iwconfig wifi# cmds
    if (firstVap == TRUE) {
        wifi_pushTxChainMask(wlanIndex);	//, pCfg->X_CISCO_COM_HTTxStream);
        wifi_pushRxChainMask(wlanIndex);	//, pCfg->X_CISCO_COM_HTRxStream);
        wifi_pushDefaultValues(wlanIndex);
        CosaDmlWiFiRadioSetTransmitPowerPercent(wlanIndex, pCfg->TransmitPower);
        wifi_setAMSDUEnable(wlanIndex, pCfg->X_CISCO_COM_AggregationMSDU);
        wifi_setSTBCEnable(wlanIndex,pCfg->X_CISCO_COM_STBCEnable);
    }

    // Push the parameters that are set based on the ath interface
    // iwpriv / iwconfig ath# cmds
    wifi_pushChannelMode(wlanAthIndex);
    if (pCfg->AutoChannelEnable == TRUE) {
        wifi_pushChannel(wlanAthIndex, 0);
    } else {
        wifi_pushChannel(wlanAthIndex, pCfg->Channel);
    }

    BOOL enable = (pCfg->GuardInterval == 2) ? FALSE : TRUE;
    wifi_setShortGuardInterval(wlanAthIndex, enable);

    wifi_setCtsProtectionEnable(wlanAthIndex, pCfg->CTSProtectionMode);
    wifi_setBeaconInterval(wlanAthIndex, pCfg->BeaconInterval);
    wifi_setDTIMInterval(wlanAthIndex, pCfg->DTIMInterval);

    //  Only set Fragmentation if mode is not n and therefore not HT
    if ( (pCfg->OperatingStandards|COSA_DML_WIFI_STD_n) == 0) {
        wifi_setFragmentationThreshold(wlanAthIndex, pCfg->FragmentationThreshold);
    }
    wifi_setRtsThreshold(wlanAthIndex, pCfg->RTSThreshold);
    wifi_setObssCoexistenceEnable(wlanAthIndex, pCfg->ObssCoex); 

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiRadioApplyCfg
(
PCOSA_DML_WIFI_RADIO_CFG    pCfg        /* Identified by InstanceNumber */
) 
{
    PCOSA_DML_WIFI_RADIO_CFG        pRunningCfg  = &sWiFiDmlRadioRunningCfg[pCfg->InstanceNumber-1];
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    wifiDbgPrintf("%s Config changes  \n",__FUNCTION__);
    int  wlanIndex;
    BOOL pushCfg = FALSE;
    BOOL createdNewVap = FALSE;

    if (!pCfg )
    {
        return ANSC_STATUS_FAILURE;
    }

    // Apply Settings to Radio
    printf("%s Calling pthread_mutex_lock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 
    pthread_mutex_lock(&sWiFiThreadMutex);
    printf("%s Called pthread_mutex_lock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 
    wifiDbgPrintf("%s Config changes  %d\n",__FUNCTION__, __LINE__);

    wlanIndex = (ULONG) pCfg->InstanceNumber-1;  
    wifiDbgPrintf("%s[%d] wlanIndex %d\n",__FUNCTION__, __LINE__, wlanIndex);

    // Need to get vlan configuration
    CosaDmlWiFiGetBridgePsmData();

    if ( gRadioPowerSetting != COSA_DML_WIFI_POWER_DOWN  &&
         gRadioNextPowerSetting != COSA_DML_WIFI_POWER_DOWN )
    {

        // If the Radio is disabled see if there are any SSIDs that need to be brought down
        if (pCfg->bEnabled == FALSE )
        {
            BOOL activeVaps = FALSE;

            wifi_getRadioActive(wlanIndex, &activeVaps);
            // bring down all VAPs
            if (activeVaps == TRUE)
            {
                int i;
                for (i = wlanIndex; i < gSsidCount; i+=2)
                {
                    BOOL vapEnabled = FALSE;
                    wifi_getApEnable(i, &vapEnabled);
                    if (vapEnabled == TRUE)
                    {
                        PCOSA_DML_WIFI_APSEC_CFG pRunningApSecCfg = &sWiFiDmlApSecurityRunning[i].Cfg;
                        wifi_deleteAp(i); 

                        if (pRunningApSecCfg->ModeEnabled >= COSA_DML_WIFI_SECURITY_WPA_Personal)
                        {
                            wifi_removeSecVaribles(i);
                            sWiFiDmlRestartHostapd = TRUE;
                            wifiDbgPrintf("%s %d sWiFiDmlRestartHostapd set to TRUE\n",__FUNCTION__, __LINE__);
                        }
                    }
                }
                if (sWiFiDmlRestartHostapd == TRUE)
                {
                    // Bounce hostapd to pick up security changes
                    wifi_stopHostApd();
                    wifi_startHostApd(); 
                    sWiFiDmlRestartHostapd = FALSE;
                    wifiDbgPrintf("%s %d sWiFiDmlRestartHostapd set to FALSE\n",__FUNCTION__, __LINE__);
                }
            }
        } else
        {

            int i;
            BOOL activeVaps = FALSE;

            wifi_getRadioActive(wlanIndex, &activeVaps);
            wifiDbgPrintf("%s Config changes   %dApplySettingSSID = %d\n",__FUNCTION__, __LINE__, pCfg->ApplySettingSSID);

            for (i=wlanIndex; i < 16; i += 2)
            {
                // if ApplySettingSSID has been set, only apply changes to the specified SSIDs
                if ( (pCfg->ApplySettingSSID != 0) && !((1<<i) & pCfg->ApplySettingSSID ))
                {
                    printf("%s: Skipping SSID ath%d, it was not in ApplySettingSSID(%d)\n", __func__, i, pCfg->ApplySettingSSID);
                    sWiFiDmlAffectedVap[i] = FALSE;
                    continue;
                }

                PCOSA_DML_WIFI_SSID_CFG pStoredSsidCfg = &sWiFiDmlSsidStoredCfg[i];
                PCOSA_DML_WIFI_AP_CFG pStoredApCfg = &sWiFiDmlApStoredCfg[i].Cfg;
                PCOSA_DML_WIFI_APSEC_FULL pStoredApSecEntry = &sWiFiDmlApSecurityStored[i];
                PCOSA_DML_WIFI_APSEC_CFG pStoredApSecCfg = &sWiFiDmlApSecurityStored[i].Cfg;
                PCOSA_DML_WIFI_APWPS_CFG pStoredApWpsCfg = &sWiFiDmlApWpsStored[i].Cfg;

                PCOSA_DML_WIFI_SSID_CFG pRunningSsidCfg = &sWiFiDmlSsidRunningCfg[i];
                PCOSA_DML_WIFI_AP_CFG pRunningApCfg = &sWiFiDmlApRunningCfg[i].Cfg;
                PCOSA_DML_WIFI_APSEC_FULL pRunningApSecEntry = &sWiFiDmlApSecurityRunning[i];
                PCOSA_DML_WIFI_APSEC_CFG pRunningApSecCfg = &sWiFiDmlApSecurityRunning[i].Cfg;
                PCOSA_DML_WIFI_APWPS_CFG pRunningApWpsCfg = &sWiFiDmlApWpsRunning[i].Cfg;

                BOOL up;

                wifi_getApEnable(i,&up);
                if ( (up == FALSE) && 
                     (pStoredSsidCfg->bEnabled == TRUE) &&  
                     ( (pStoredSsidCfg->EnableOnline == FALSE) ||
                       (pStoredSsidCfg->RouterEnabled == TRUE)))
                {
                    sWiFiDmlAffectedVap[i] = TRUE;

                    wifi_createAp(i,wlanIndex,pStoredSsidCfg->SSID, (pStoredApCfg->SSIDAdvertisementEnabled == TRUE) ? FALSE : TRUE);
                    createdNewVap = TRUE;
                    // push Radio config to new VAP
                    CosaDmlWiFiRadioPushCfg(pCfg, i,((activeVaps == FALSE) ? TRUE : FALSE));
                    activeVaps = TRUE;
                    CosaDmlWiFiApPushCfg(pStoredApCfg); 
                    CosaDmlWiFiApMfPushCfg(sWiFiDmlApMfCfg[i], i);
                    CosaDmlWiFiApPushMacFilter(sWiFiDmlApMfQueue[i], i);
                    // push security and restart hostapd
                    CosaDmlWiFiApSecPushCfg(pStoredApSecCfg, pStoredApCfg->InstanceNumber);
                    // push mac filters
                    wifi_pushBridgeInfo(i);

                    // else if up=TRUE, but should be down
                } else if (up==TRUE && 
                           (  (pStoredSsidCfg->bEnabled == FALSE) ||  
                              (  (pStoredSsidCfg->EnableOnline == TRUE) && 
                                 (pStoredSsidCfg->RouterEnabled == FALSE )) ) )
                {

                    wifi_deleteAp(i); 
                    if (pRunningApSecCfg->ModeEnabled >= COSA_DML_WIFI_SECURITY_WPA_Personal)
                    {
                        wifi_removeSecVaribles(i);
                        sWiFiDmlRestartHostapd = TRUE;
                        wifiDbgPrintf("%s %d sWiFiDmlRestartHostapd set to TRUE\n",__FUNCTION__, __LINE__);
                    }

                    // else if up=TRUE and changes, apply them
                } else if (up==TRUE)
                {
                    BOOLEAN wpsChange = (strcmp(pRunningSsidCfg->SSID, pStoredSsidCfg->SSID) != 0) ? TRUE : FALSE;
                    sWiFiDmlAffectedVap[i] = TRUE;

                    // wifi_ifConfigDown(i);

                    CosaDmlWiFiSsidApplyCfg(pStoredSsidCfg);
                    CosaDmlWiFiApApplyCfg(pStoredApCfg);

                    if ( memcmp(pStoredApSecEntry,pRunningApSecEntry,sizeof(COSA_DML_WIFI_APSEC_FULL)) != 0 ||
                         memcmp(pStoredApWpsCfg, pRunningApWpsCfg, sizeof(COSA_DML_WIFI_APWPS_CFG)) != 0  ||
                         sWiFiDmlWepChg[i] == TRUE ||
                         sWiFiDmlUpdatedAdvertisement[i] == TRUE ||
                         wpsChange == TRUE )
                    {
                        CosaDmlWiFiApSecApplyCfg(pStoredApSecCfg, pStoredApCfg->InstanceNumber); 
                        sWiFiDmlWepChg[i] = FALSE;
                        sWiFiDmlUpdatedAdvertisement[i] = FALSE;
                    }
                    CosaDmlWiFiApWpsApplyCfg(pStoredApWpsCfg,i);
                }
            } // for each SSID

            /* if any of the user-controlled SSID is up, turn on the WiFi LED */
            int  MbssEnable = 0;
            for (i=wlanIndex; i < 16; i += 2)
            {
                if (sWiFiDmlSsidStoredCfg[i].bEnabled == TRUE)
                    MbssEnable += 1<<((i-wlanIndex)/2);
            }
            wifi_setLED(wlanIndex, (MbssEnable & pCfg->MbssUserControl)?1:0);

            if (sWiFiDmlRestartHostapd == TRUE)
            {
                // Bounce hostapd to pick up security changes
                wifi_stopHostApd();
                wifi_startHostApd(); 
                sWiFiDmlRestartHostapd = FALSE;
                wifiDbgPrintf("%s %d sWiFiDmlRestartHostapd set to FALSE\n",__FUNCTION__, __LINE__);
            }

            // If a new SSID was not created, then the radio parameters still need to be pushed.
            if (createdNewVap == FALSE && activeVaps == TRUE)
            {
                int athIndex;


                wifiDbgPrintf("%s Pushing Radio Config changes  %d\n",__FUNCTION__, __LINE__);
                if ( pCfg->OperatingStandards != pRunningCfg->OperatingStandards ||
                     pCfg->OperatingChannelBandwidth != pRunningCfg->OperatingChannelBandwidth ||
                     pCfg->ExtensionChannel != pRunningCfg->ExtensionChannel )
                {

                    // Currently do not allow the channel mode to change while the SSIDs are running
                    // so this code should never be called.  
                    wifi_pushChannelMode(wlanIndex);

                } // Mode changed

                if (pCfg->AutoChannelEnable != pRunningCfg->AutoChannelEnable || 
                    (pRunningCfg->AutoChannelEnable == FALSE && pCfg->Channel != pRunningCfg->Channel) )
                {
                    // Currently do not allow the channel to change while the SSIDs are running
                    // so this code should never be called.  
                    wifi_pushChannel(wlanIndex, pCfg->Channel); 
                }

                if ( pCfg->X_CISCO_COM_HTTxStream != pRunningCfg->X_CISCO_COM_HTTxStream)
                {
                    wifi_pushTxChainMask(wlanIndex);
                }
                if (pCfg->X_CISCO_COM_HTRxStream != pRunningCfg->X_CISCO_COM_HTRxStream)
                {
                    wifi_pushRxChainMask(wlanIndex);
                }

                if (pCfg->TransmitPower != pRunningCfg->TransmitPower)
                {
                    if ( ( gRadioPowerState[wlanIndex] == COSA_DML_WIFI_POWER_UP ) &&
                         ( gRadioNextPowerSetting != COSA_DML_WIFI_POWER_DOWN ) )
                    {
                        CosaDmlWiFiRadioSetTransmitPowerPercent(wlanIndex, pCfg->TransmitPower);
                    } else if ( gRadioPowerState[wlanIndex] == COSA_DML_WIFI_POWER_LOW )
                    {
                        wifiDbgPrintf("%s calling wifi_setTransmitPower to 5 dbM, RadioPower was set to COSA_DML_WIFI_POWER_LOW \n",__FUNCTION__);
                        wifi_setTransmitPower(wlanIndex, 5);
                    }
                }
                
                if (pCfg->X_CISCO_COM_AggregationMSDU != pRunningCfg->X_CISCO_COM_AggregationMSDU)
                {
                    wifi_setAMSDUEnable(wlanIndex, pCfg->X_CISCO_COM_AggregationMSDU);
                }
                if (pCfg->X_CISCO_COM_STBCEnable != pRunningCfg->X_CISCO_COM_STBCEnable )
                {
                    wifi_setSTBCEnable(wlanIndex, pCfg->X_CISCO_COM_STBCEnable);
                }

                wifiDbgPrintf("%s Pushing Radio Config changes  %d\n",__FUNCTION__, __LINE__);
                // Find the first ath that is up on the given radio
                for (athIndex = wlanIndex; athIndex < 16; athIndex+=2) {
                    BOOL enabled;
                    wifi_getApEnable(athIndex, &enabled);
                    wifiDbgPrintf("%s Pushing Radio Config changes ath%d %d\n",__FUNCTION__, athIndex, __LINE__);

                    if (enabled == TRUE) {
                        // These Radio parameters are set on SSID basis (iwpriv/iwconfig ath%d commands) 
                        if (pCfg->GuardInterval != pRunningCfg->GuardInterval)
                        {
                            // pCfg->GuardInterval   
                            // COSA_DML_WIFI_GUARD_INTVL_400ns and COSA_DML_WIFI_GUARD_INTVL_Auto are the
                            // same in the WiFi driver
                            BOOL enable = (pCfg->GuardInterval == 2) ? FALSE : TRUE;
                            wifi_setShortGuardInterval(athIndex, enable);
                        }
                        
                        if (pCfg->CTSProtectionMode != pRunningCfg->CTSProtectionMode)
                        {
                            wifi_setCtsProtectionEnable(athIndex, pCfg->CTSProtectionMode);
                        }
                        if (pCfg->BeaconInterval != pRunningCfg->BeaconInterval)
                        {
                            wifi_setBeaconInterval(athIndex, pCfg->BeaconInterval);
                        }
                        if (pCfg->DTIMInterval != pRunningCfg->DTIMInterval)
                        {
                            wifi_setDTIMInterval(athIndex, pCfg->DTIMInterval);
                        }
                        //  Only set Fragmentation if mode is not n and therefore not HT
                        if (pCfg->FragmentationThreshold != pRunningCfg->FragmentationThreshold &&
                            (pCfg->OperatingStandards|COSA_DML_WIFI_STD_n) == 0)
                        {
                            wifi_setFragmentationThreshold(athIndex, pCfg->FragmentationThreshold);
                        }
                        if (pCfg->RTSThreshold != pRunningCfg->RTSThreshold)
                        {
                            wifi_setRtsThreshold(athIndex, pCfg->RTSThreshold);
                        }
                        if (pCfg->ObssCoex != pRunningCfg->ObssCoex)
                        {
                            wifi_setObssCoexistenceEnable(athIndex, pCfg->ObssCoex); 
                        }
                    }
                }
            }

            for (i=wlanIndex; i < 16; i += 2) { 
                if (sWiFiDmlAffectedVap[i] == TRUE)
                {
                    if (sWiFiDmlPushWepKeys[i] == TRUE)
                    {
                        PCOSA_DML_WIFI_APSEC_CFG pCfg = &sWiFiDmlApSecurityStored[i].Cfg;
                        CosaDmlWiFiApSecApplyWepCfg(pCfg,i+1);
                    }
                    wifi_ifConfigUp(i);
                }
                if (sWiFiDmlUpdateVlanCfg[i] == TRUE) {
                    // update vlan configuration
                    wifi_resetVlanCfg(i); 
                    sWiFiDmlUpdateVlanCfg[i] = FALSE;
                }
                sWiFiDmlAffectedVap[i] = FALSE;
                sWiFiDmlPushWepKeys[i] = FALSE;
            }
        }

        pCfg->ApplySetting = FALSE;
        pCfg->ApplySettingSSID = 0;
    }

    // Notify WiFiExtender that WiFi parameter have changed
    {
        CosaDml_NotifyWiFiExt(COSA_WIFIEXT_DM_UPDATE_RADIO|COSA_WIFIEXT_DM_UPDATE_WPS|COSA_WIFIEXT_DM_UPDATE_SSID);
    }
    printf("%s Calling pthread_mutex_unlock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 
    pthread_mutex_unlock(&sWiFiThreadMutex);
    printf("%s Called pthread_mutex_unlock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 

    memcpy(&sWiFiDmlRadioRunningCfg[pCfg->InstanceNumber-1], pCfg, sizeof(COSA_DML_WIFI_RADIO_CFG));
    memcpy(&sWiFiDmlRadioStoredCfg[pCfg->InstanceNumber-1], pCfg, sizeof(COSA_DML_WIFI_RADIO_CFG));

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiRadioSetCfg
(
ANSC_HANDLE                 hContext,
PCOSA_DML_WIFI_RADIO_CFG    pCfg        /* Identified by InstanceNumber */
)
{
    PCOSA_DML_WIFI_RADIO_CFG        pStoredCfg  = &sWiFiDmlRadioStoredCfg[pCfg->InstanceNumber-1];
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    wifiDbgPrintf("%s Config changes  \n",__FUNCTION__);
    int  wlanIndex;
    char frequency[32];
    char channelMode[32];
    char opStandards[32];
    BOOL wlanRestart = FALSE;

    if (!pCfg )
    {
        return ANSC_STATUS_FAILURE;
    }

    pCfg->LastChange             = AnscGetTickInSeconds();
    printf("%s: LastChange %d \n", __func__,pCfg->LastChange);

    wlanIndex = (ULONG) pCfg->InstanceNumber-1;

    // Push changed parameters to persistent store, but don't push to the radio
    // Until after ApplySettings is set to TRUE
    CosaDmlWiFiSetRadioPsmData(pCfg, wlanIndex, pCfg->InstanceNumber);

    if (pStoredCfg->bEnabled != pCfg->bEnabled )
    {
        // this function will set a global Radio flag that will be used by the apup script
        // if the value is FALSE, the SSIDs on that radio will not be brought up even if they are enabled
        // if ((SSID.Enable==TRUE)&&(Radio.Enable==true)) then bring up SSID 
        wifi_setRadioEnable(wlanIndex,pCfg->bEnabled);
    }

    if (pCfg->AutoChannelEnable != pStoredCfg->AutoChannelEnable)
    {
        // If ACS is turned off or on the radio must be restarted to pick up the new channel
        wlanRestart = TRUE;  // Radio Restart Needed
        wifiDbgPrintf("%s: Radio Reset Needed!!!!\n",__FUNCTION__);

        if (pCfg->AutoChannelEnable == TRUE)
        {
            printf("%s: Setting Auto Channel Selection to TRUE \n",__FUNCTION__);
            wifi_setAutoChannelEnable(wlanIndex, pCfg->AutoChannelEnable);
        } else {
            printf("%s: Setting Channel= %d\n",__FUNCTION__,pCfg->Channel);
            wifi_setChannel(wlanIndex, (UINT)pCfg->Channel);
        }

    } else if (  (pCfg->AutoChannelEnable == FALSE) && (pCfg->Channel != pStoredCfg->Channel) )
    {
        printf("%s: Setting Channel= %d\n",__FUNCTION__,pCfg->Channel);
        wifi_setChannel(wlanIndex, (UINT)pCfg->Channel);
        wlanRestart=TRUE; // FIX ME !!!
    }

    // In certain releases GUI sends down the ExtensionChannel, but the GUI only supports Auto
    // and the driver does not support Auto.  Ignore the for now because it is causing the Radio to be restarted.
    if (pCfg->ExtensionChannel != pStoredCfg->ExtensionChannel &&
        pCfg->ExtensionChannel == COSA_DML_WIFI_EXT_CHAN_Auto ) {
        pCfg->ExtensionChannel = pStoredCfg->ExtensionChannel;
    }
    if ( pCfg->OperatingStandards != pStoredCfg->OperatingStandards ||
         pCfg->OperatingChannelBandwidth != pStoredCfg->OperatingChannelBandwidth ||
         pCfg->ExtensionChannel != pStoredCfg->ExtensionChannel )
    {

        char chnMode[32];
        BOOL gOnlyFlag = FALSE;
        BOOL nOnlyFlag = FALSE;
        BOOL acOnlyFlag = FALSE;
        wlanRestart = TRUE;      // Radio Restart Needed

        // Note based on current channel, the Extension Channel may need to change, if channel is not auto. Deal with that first!
        // Only care about fixing the ExtensionChannel and Channel number pairing if Radio is not  set to AutoChannel
        if ( (pCfg->OperatingChannelBandwidth == COSA_DML_WIFI_CHAN_BW_40M) &&
             (pCfg->AutoChannelEnable == FALSE) )
        {
            if (pCfg->OperatingFrequencyBand == COSA_DML_WIFI_FREQ_BAND_2_4G)
            {
                if (pCfg->ExtensionChannel == COSA_DML_WIFI_EXT_CHAN_Above)
                {
                    if(pCfg->Channel > 7 ) 
                    {
                        pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Below;
                    }
                } else { // trying to set secondary below ...
                    if(pCfg->Channel <= 5 ) {
                        pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Above;
                    }
                }
            } else { // else 5GHz
                if (pCfg->ExtensionChannel == COSA_DML_WIFI_EXT_CHAN_Above )
                {
                    switch( pCfg->Channel ) {
                        case 40: case 48: case 56: case 64: case 104: case 112: case 120: case 128: case 136: case 153: case 161:
                        pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Below;
                        break;
                    default:
                        break;
                    }
                } else { // Trying to set it below
                    switch( pCfg->Channel ) {
                        case 36: case 44: case 52: case 60: case 100: case 108: case 116: case 124: case 132: case 149: case 157:
                        pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Above;
                        break;
                    default:
                        break;
                    }
                }
            } // endif(FrequencBand)   
        } // endif (40MHz)

        if (pCfg->OperatingStandards == COSA_DML_WIFI_STD_a)
        {
            sprintf(chnMode,"11A");
        } else if (pCfg->OperatingStandards == COSA_DML_WIFI_STD_b)
        {
            sprintf(chnMode,"11B");
        } else if (pCfg->OperatingStandards == COSA_DML_WIFI_STD_g)
        {
            sprintf(chnMode,"11G");
        } else if (pCfg->OperatingStandards == ( COSA_DML_WIFI_STD_b | COSA_DML_WIFI_STD_g ) )
        {
            sprintf(chnMode,"11G");
            // all below are n with a possible combination of a, b and g
        } else if ( !(pCfg->OperatingStandards&COSA_DML_WIFI_STD_ac) )
        {
            // n but not ac modes
            if ( pCfg->OperatingChannelBandwidth == COSA_DML_WIFI_CHAN_BW_20M)
            {

                if (pCfg->OperatingFrequencyBand == COSA_DML_WIFI_FREQ_BAND_2_4G)
                {
                    sprintf(chnMode,"11NGHT20");
                } else // COSA_DML_WIFI_FREQ_BAND_5G
                {
                    sprintf(chnMode,"11NAHT20");
                }
            } else if (pCfg->OperatingChannelBandwidth & COSA_DML_WIFI_CHAN_BW_40M)
            {
                // treat 40 and Auto as 40MHz, the driver does not have an 'Auto setting' that can be toggled

                if (pCfg->OperatingFrequencyBand == COSA_DML_WIFI_FREQ_BAND_2_4G)
                {

                    if (pCfg->ExtensionChannel == COSA_DML_WIFI_EXT_CHAN_Above )
                    {
                        sprintf(chnMode,"11NGHT40PLUS");
                    } else
                    {
                        sprintf(chnMode,"11NGHT40MINUS");
                    }
                } else // else 5GHz
                {
                    if (pCfg->ExtensionChannel == COSA_DML_WIFI_EXT_CHAN_Above )
                    {
                        sprintf(chnMode,"11NAHT40PLUS");
                    } else
                    {
                        sprintf(chnMode,"11NAHT40MINUS");
                    }
                }
            }
        } else if (pCfg->OperatingStandards&COSA_DML_WIFI_STD_ac)
        {

            if ( pCfg->OperatingChannelBandwidth == COSA_DML_WIFI_CHAN_BW_20M)
            {
                sprintf(chnMode,"11ACVHT20");
            } else if (pCfg->OperatingChannelBandwidth == COSA_DML_WIFI_CHAN_BW_40M)
            {
                if (pCfg->ExtensionChannel == COSA_DML_WIFI_EXT_CHAN_Above )
                {
                    sprintf(chnMode,"11ACVHT40PLUS");
                } else
                {
                    sprintf(chnMode,"11ACVHT40MINUS");
                }
            } else if (pCfg->OperatingChannelBandwidth == COSA_DML_WIFI_CHAN_BW_80M)
            {
                sprintf(chnMode,"11ACVHT80");
            }
        }

        // if OperatingStandards is set to only g or only n, set only flag to TRUE.
        // wifi_setChannelMode will set PUREG=1/PUREN=1 in the config
        if ( (pCfg->OperatingStandards == COSA_DML_WIFI_STD_g) ||
             (pCfg->OperatingStandards == (COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n) ) )
        {
            gOnlyFlag = TRUE;
        }

        if ( ( pCfg->OperatingStandards == COSA_DML_WIFI_STD_n ) ||
             ( pCfg->OperatingStandards == (COSA_DML_WIFI_STD_n | COSA_DML_WIFI_STD_ac) ) )
        {
            nOnlyFlag = TRUE;
        }

        if (pCfg->OperatingStandards == COSA_DML_WIFI_STD_ac )
        {
            acOnlyFlag = TRUE;
        }
        printf("%s: wifi_setChannelMode= Wlan%d, Mode: %s, gOnlyFlag: %d, nOnlyFlag: %d\n acOnlyFlag: %d\n",__FUNCTION__,wlanIndex,chnMode,gOnlyFlag,nOnlyFlag, acOnlyFlag);
        wifi_setChannelMode(wlanIndex, chnMode, gOnlyFlag, nOnlyFlag, acOnlyFlag);
    } // Done with Mode settings

    if (pCfg->X_CISCO_COM_HTTxStream != pStoredCfg->X_CISCO_COM_HTTxStream)
    {
        wifi_setTxChainMask(wlanIndex, pCfg->X_CISCO_COM_HTTxStream);
    }
    if (pCfg->X_CISCO_COM_HTRxStream != pStoredCfg->X_CISCO_COM_HTRxStream)
    {
        wifi_setRxChainMask(wlanIndex, pCfg->X_CISCO_COM_HTRxStream);
    }

    // pCfg->AutoChannelRefreshPeriod       = 3600;
    // Modulation Coding Scheme 0-23, value of -1 means Auto
    // pCfg->MCS                            = 1;

    // ######## Needs to be update to get actual value
    // pCfg->BasicRate = COSA_DML_WIFI_BASICRATE_Default;

    // Need to translate
#if 0
    {
        char maxBitRate[128];
        wifi_getMaxBitRate(wlanIndex, maxBitRate);
        wifiDbgPrintf("%s: wifi_getMaxBitRate returned %s\n", __FUNCTION__, maxBitRate);
        if (strcmp(maxBitRate,"Auto") == 0)
        {
            pCfg->TxRate = COSA_DML_WIFI_TXRATE_Auto;
            wifiDbgPrintf("%s: set to auto pCfg->TxRate = %d\n", __FUNCTION__, pCfg->TxRate);
        } else
        {
            char *pos;
            pos = strstr(maxBitRate, " ");
            if (pos != NULL) pos = 0;
            wifiDbgPrintf("%s: wifi_getMaxBitRate returned %s\n", __FUNCTION__, maxBitRate);
            pCfg->TxRate = atoi(maxBitRate);
            pCfg->TxRate = COSA_DML_WIFI_TXRATE_48M;
            wifiDbgPrintf("%s: pCfg->TxRate = %d\n", __FUNCTION__, pCfg->TxRate);
        }
    }
    pCfg->TxRate                         = COSA_DML_WIFI_TXRATE_Auto;
#endif

    /* Below is Cisco Extensions */
    // pCfg->APIsolation                    = TRUE;
    // pCfg->FrameBurst                     = TRUE;
    // Not for first GA
    // pCfg->OnOffPushButtonTime           = 23;
    // pCfg->MulticastRate                 = 45;
    // BOOL                            X_CISCO_COM_ReverseDirectionGrant;
    // BOOL                            X_CISCO_COM_AutoBlockAck;
    // BOOL                            X_CISCO_COM_DeclineBARequest;

    if (pCfg->X_CISCO_COM_WirelessOnOffButton != pStoredCfg->X_CISCO_COM_WirelessOnOffButton)
    {
        wifi_getWifiEnableStatus(wlanIndex,pCfg->X_CISCO_COM_WirelessOnOffButton);
        printf("%s: called wifi_getWifiEnableStatus\n",__FUNCTION__);
    }
    
  
    if ( pCfg->ApplySetting == TRUE )
    {
        wifiDbgPrintf("%s: ApplySettings!!!!!! \n",__FUNCTION__);

	if(wlanRestart == TRUE)
	{
            memcpy(&sWiFiDmlRadioStoredCfg[pCfg->InstanceNumber-1], pCfg, sizeof(COSA_DML_WIFI_RADIO_CFG));
            wifiDbgPrintf("\n%s: ***** RESTARTING RADIO !!! *****\n",__FUNCTION__);
            wifi_initRadio(wlanIndex);

            pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
            CosaWifiReInitialize((ANSC_HANDLE)pMyObject, wlanIndex);

            // Notify WiFiExtender that WiFi parameter have changed
            {
                CosaDml_NotifyWiFiExt(COSA_WIFIEXT_DM_UPDATE_RADIO|COSA_WIFIEXT_DM_UPDATE_WPS|COSA_WIFIEXT_DM_UPDATE_SSID);
            }

        } else {
            CosaDmlWiFiRadioApplyCfg(pCfg);
	    memcpy(&sWiFiDmlRadioStoredCfg[pCfg->InstanceNumber-1], pCfg, sizeof(COSA_DML_WIFI_RADIO_CFG));
        }
    }
   
    return ANSC_STATUS_SUCCESS;
}

// Called from middle layer to get Cfg information that can be changed by the Radio
// for example, when in Auto channel mode, the radio can change the Channel so it should 
// always be quired.
ANSC_STATUS
CosaDmlWiFiRadioGetDCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_RADIO_CFG    pCfg        /* Identified by InstanceNumber */
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS; 
    int                             wlanIndex;
    char channelMode[32];

    if (!pCfg )
    {
        return ANSC_STATUS_FAILURE;
    }
    
    wlanIndex = (ULONG) pCfg->InstanceNumber-1;  

	wifi_getChannel(wlanIndex, (UINT *)&pCfg->Channel);

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
    int                             wlanIndex;
    BOOL radioEnabled = FALSE;
    BOOL enabled = FALSE;
    char frequency[32];
    char channelMode[32];
    char opStandards[32];
    static BOOL firstTime[2] = { TRUE, TRUE};

    if (!pCfg )
    {
        return ANSC_STATUS_FAILURE;
    }

    CosaDmlWiFiGetRadioPsmData(pCfg);

    wlanIndex = (ULONG) pCfg->InstanceNumber-1;  
    
    if (firstTime[wlanIndex] == TRUE) {
        pCfg->LastChange             = AnscGetTickInSeconds(); 
        printf("%s: LastChange %d \n", __func__, pCfg->LastChange);
        firstTime[wlanIndex] = FALSE;
    }

    wifi_getRadioEnable(wlanIndex, &radioEnabled);
    pCfg->bEnabled = (radioEnabled == TRUE) ? 1 : 0;

    char frequencyBand[10];
    wifi_getSupportedFrequencyBands(wlanIndex, frequencyBand);
    if (strstr(frequencyBand,"2.4G") != NULL)
    {
        pCfg->OperatingFrequencyBand = COSA_DML_WIFI_FREQ_BAND_2_4G; 
    } else if (strstr(frequencyBand,"5G_11N") != NULL)
    {
        pCfg->OperatingFrequencyBand = COSA_DML_WIFI_FREQ_BAND_5G;
    } else if (strstr(frequencyBand,"5G_11AC") != NULL)
    {
        pCfg->OperatingFrequencyBand = COSA_DML_WIFI_FREQ_BAND_5G;
    } else
    {
        // if we can't determine frequency band assume wifi0 is 2.4 and wifi1 is 5 11n
        if (wlanIndex == 0)
        {
            pCfg->OperatingFrequencyBand = COSA_DML_WIFI_FREQ_BAND_2_4G;
        } else
        {
            pCfg->OperatingFrequencyBand = COSA_DML_WIFI_FREQ_BAND_5G; 
        }
    }
 
    // Non-Vol cfg data
    BOOL gOnly;
    BOOL nOnly;
    BOOL acOnly;
    wifi_getStandard(wlanIndex, opStandards, &gOnly, &nOnly, &acOnly);
    if (strcmp("a",opStandards)==0)
    { 
	pCfg->OperatingStandards = COSA_DML_WIFI_STD_a;      /* Bitmask of COSA_DML_WIFI_STD */
    } else if (strcmp("b",opStandards)==0)
    { 
	pCfg->OperatingStandards = COSA_DML_WIFI_STD_b;      /* Bitmask of COSA_DML_WIFI_STD */
    } else if (strcmp("g",opStandards)==0)
    { 
        if (gOnly == TRUE) {  
	    pCfg->OperatingStandards = COSA_DML_WIFI_STD_g;      /* Bitmask of COSA_DML_WIFI_STD */
        } else {
	    pCfg->OperatingStandards = COSA_DML_WIFI_STD_b | COSA_DML_WIFI_STD_g;      /* Bitmask of COSA_DML_WIFI_STD */
        }
    } else if (strncmp("n",opStandards,1)==0)
    { 
	if ( pCfg->OperatingFrequencyBand == COSA_DML_WIFI_FREQ_BAND_2_4G )
        {
            if (gOnly == TRUE) {
	        pCfg->OperatingStandards = COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n;      /* Bitmask of COSA_DML_WIFI_STD */
            } else if (nOnly == TRUE) {
	        pCfg->OperatingStandards = COSA_DML_WIFI_STD_n;      /* Bitmask of COSA_DML_WIFI_STD */
            } else {
	        pCfg->OperatingStandards = COSA_DML_WIFI_STD_b | COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n;      /* Bitmask of COSA_DML_WIFI_STD */
            }
        } else {
            if (nOnly == TRUE) {
	        pCfg->OperatingStandards = COSA_DML_WIFI_STD_n;      /* Bitmask of COSA_DML_WIFI_STD */
            } else {
	        pCfg->OperatingStandards = COSA_DML_WIFI_STD_a | COSA_DML_WIFI_STD_n;      /* Bitmask of COSA_DML_WIFI_STD */
            }
        }
    } else if (strcmp("ac",opStandards) == 0) {
        if (acOnly == TRUE) {
            pCfg->OperatingStandards = COSA_DML_WIFI_STD_ac;
        } else  if (nOnly == TRUE) {
            pCfg->OperatingStandards = COSA_DML_WIFI_STD_n | COSA_DML_WIFI_STD_ac;
        } else {
            pCfg->OperatingStandards = COSA_DML_WIFI_STD_a | COSA_DML_WIFI_STD_n | COSA_DML_WIFI_STD_ac;
        }
    }

	wifi_getChannel(wlanIndex, (UINT*)&pCfg->Channel);
     
    wifi_getAutoChannelEnable(wlanIndex, &enabled); 
    pCfg->AutoChannelEnable = (enabled == TRUE) ? TRUE : FALSE;

    pCfg->AutoChannelRefreshPeriod       = 3600;

    wifi_getChannelMode(wlanIndex, channelMode);
    if (strstr(channelMode, "40") != NULL)
    {
        pCfg->OperatingChannelBandwidth = COSA_DML_WIFI_CHAN_BW_40M;
	if (strstr(channelMode, "PLUS") != NULL) 
	{
	    pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Above;
	} else if (strstr(channelMode, "MINUS") != NULL)
	{
	    pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Below;
	} else {
	    pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Auto;
	}
    } else if (strstr(channelMode, "80") != NULL) {
        pCfg->OperatingChannelBandwidth = COSA_DML_WIFI_CHAN_BW_80M;
        pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Auto;
    } else {
        pCfg->OperatingChannelBandwidth = COSA_DML_WIFI_CHAN_BW_20M;
        pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Auto;
	}

    // Modulation Coding Scheme 0-15, value of -1 means Auto
    pCfg->MCS                            = 1;

    // got from CosaDmlWiFiGetRadioPsmData
    {
            if ( ( gRadioPowerState[wlanIndex] == COSA_DML_WIFI_POWER_UP) &&
                 ( gRadioNextPowerSetting != COSA_DML_WIFI_POWER_DOWN ) )
            {
                CosaDmlWiFiRadioSetTransmitPowerPercent(wlanIndex, pCfg->TransmitPower);
            } else {
                printf("%s: Radio was not in Power Up mode, didn't set the tranmitPower level \n", __func__);
	}
    }

    if (wlanIndex == 0) {
        pCfg->IEEE80211hEnabled              = FALSE;
    } else {
        pCfg->IEEE80211hEnabled              = TRUE;
    }

    wifi_getCountryCode(wlanIndex, pCfg->RegulatoryDomain);

    // ######## Needs to be update to get actual value
    pCfg->BasicRate = COSA_DML_WIFI_BASICRATE_Default;

    { 
        char maxBitRate[128]; 
        wifi_getMaxBitRate(wlanIndex, maxBitRate); 

        if (strcmp(maxBitRate,"Auto") == 0)
        {
            pCfg->TxRate = COSA_DML_WIFI_TXRATE_Auto;
        } else {
            char *pos;
            pos = strstr(maxBitRate, " ");
            if (pos != NULL) pos = 0;
            pCfg->TxRate = atoi(maxBitRate);
            pCfg->TxRate = COSA_DML_WIFI_TXRATE_48M;
        }
    }
    pCfg->TxRate                         = COSA_DML_WIFI_TXRATE_Auto;

    /* Below is Cisco Extensions */
    pCfg->APIsolation                    = TRUE;
    pCfg->FrameBurst                     = TRUE;

    // Not for first GA
    // pCfg->OnOffPushButtonTime           = 23;

    // pCfg->MulticastRate                 = 45;

    // BOOL                            X_CISCO_COM_ReverseDirectionGrant;

    wifi_getAMSDUEnable(wlanIndex, &enabled);
    pCfg->X_CISCO_COM_AggregationMSDU = (enabled == TRUE) ? TRUE : FALSE;

    // BOOL                            X_CISCO_COM_AutoBlockAck;
    // BOOL                            X_CISCO_COM_DeclineBARequest;
    
    wifi_getTxChainMask(wlanIndex, (int *) &pCfg->X_CISCO_COM_HTTxStream);
    wifi_getRxChainMask(wlanIndex, (int *) &pCfg->X_CISCO_COM_HTRxStream);

    wifi_getWifiEnableStatus(wlanIndex, &enabled);
    pCfg->X_CISCO_COM_WirelessOnOffButton = (enabled == TRUE) ? TRUE : FALSE;

    /* if any of the user-controlled SSID is up, turn on the WiFi LED */
    int i, MbssEnable = 0;
    BOOL vapEnabled = FALSE;
    for (i=wlanIndex; i < 16; i += 2)
    {
        wifi_getApEnable(i, &vapEnabled);
        if (vapEnabled == TRUE)
            MbssEnable += 1<<((i-wlanIndex)/2);
    }
    wifi_setLED(wlanIndex, (MbssEnable & pCfg->MbssUserControl)?1:0);

    // Should this be Write-Only parameter?
    pCfg->ApplySetting  = FALSE;
    pCfg->ApplySettingSSID = 0;

    memcpy(&sWiFiDmlRadioStoredCfg[pCfg->InstanceNumber-1], pCfg, sizeof(COSA_DML_WIFI_RADIO_CFG));
    memcpy(&sWiFiDmlRadioRunningCfg[pCfg->InstanceNumber-1], pCfg, sizeof(COSA_DML_WIFI_RADIO_CFG));

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
        char status[32];

        wifi_getStatus(ulInstanceNumber-1,status);
        if (strcmp(status,"Up") == 0)
        {
	    pInfo->Status                 = COSA_DML_IF_STATUS_Up;
        } else if (strcmp(status,"Disable") == 0) {
	    pInfo->Status                 = COSA_DML_IF_STATUS_Down;
        } else {
	    pInfo->Status                 = COSA_DML_IF_STATUS_Error;
        }
        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS
CosaDmlWiFiRadioGetChannelsInUse
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber,
        PCOSA_DML_WIFI_RADIO_DINFO  pInfo
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    wifiDbgPrintf("%s\n",__FUNCTION__);

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
        pInfo->ChannelsInUse[0] = 0;
        wifi_getChannelsInUse(ulInstanceNumber-1, pInfo->ChannelsInUse);
        wifiDbgPrintf("%s ChannelsInUse = %s \n",__FUNCTION__, pInfo->ChannelsInUse);
        return ANSC_STATUS_SUCCESS;
    }
}

ANSC_STATUS
CosaDmlWiFiRadioGetApChannelScan
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber,
        PCOSA_DML_WIFI_RADIO_DINFO  pInfo
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    wifiDbgPrintf("%s\n",__FUNCTION__);

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
        if ( (strlen(pInfo->ApChannelScan) == 0) || (AnscGetTickInSeconds() -  pInfo->LastScan) > 60) 
        {
            pInfo->ApChannelScan[0] = 0;
            wifi_scanApChannels(ulInstanceNumber-1, pInfo->ApChannelScan); 
            pInfo->LastScan = AnscGetTickInSeconds();
        }
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
	ULONG currentTime = AnscGetTickInSeconds(); 

    // if the last poll was within 10 seconds skip the poll
    // When the whole Stat table is pull the top level DML calls this funtion once
    // for each parameter in the table
    if ( (currentTime - sWiFiDmlRadioLastStatPoll[ulInstanceNumber-1]) < 10)  {
        return ANSC_STATUS_SUCCESS;
    } 
wifiDbgPrintf("%s Getting Stats last poll was %d seconds ago \n",__FUNCTION__, currentTime-sWiFiDmlRadioLastStatPoll[ulInstanceNumber-1] );
    sWiFiDmlRadioLastStatPoll[ulInstanceNumber-1] = currentTime;

    if (!pStats)
    {
        return ANSC_STATUS_FAILURE;
    }
    
    memset(pStats,0,sizeof(COSA_DML_WIFI_RADIO_STATS));

    int i;
    for (i = 0; i < gSsidCount; i++) {
        int wlanRadioIndex;
        int result = 0;
        BOOL enabled = FALSE;

        wifi_getRadioIndex(i, &wlanRadioIndex);
        wifi_getApEnable(i, &enabled);

        if ( ((ulInstanceNumber-1) == wlanRadioIndex) &&
             (enabled == TRUE) ) {
            wifi_basicTrafficStats_t basicStats; 
            wifi_trafficStats_t errorStats;

            // This is per VAP not per radio will have to add up all VAPs on radio
            result = wifi_getBasicTrafficStats(i, &basicStats);
            if (result == 0) {
                pStats->BytesSent                          += basicStats.wifi_BytesSent;
                pStats->BytesReceived                  += basicStats.wifi_BytesReceived;
                pStats->PacketsSent                      += basicStats.wifi_PacketsSent;
                pStats->PacketsReceived              += basicStats.wifi_PacketsReceived;
            }

            // Next params from wifi_trafficStats_t - wifi_getWifiTrafficStats() currently not implemented by Atheros
            result = wifi_getWifiTrafficStats(i, &errorStats);
            if (result == 0) {
                pStats->ErrorsSent                          += errorStats.wifi_ErrorsSent;
                pStats->ErrorsReceived                  += errorStats.wifi_ErrorsReceived;
                pStats->DiscardPacketsSent          += errorStats.wifi_DiscardedPacketsSent;
                pStats->DiscardPacketsReceived  += errorStats.wifi_DiscardedPacketsReceived;

            }
        }
    }

    return ANSC_STATUS_SUCCESS;
}

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

    return gSsidCount;
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
wifiDbgPrintf("%s ulIndex = %d \n",__FUNCTION__, ulIndex);
    ANSC_STATUS returnStatus = ANSC_STATUS_SUCCESS;
    char entry[32];
    int wlanIndex;
    int wlanRadioIndex;

    // sscanf(entry,"ath%d", &wlanIndex);
    wlanIndex = ulIndex;

    wifi_getRadioIndex(wlanIndex, &wlanRadioIndex);

    /*Set default Name & Alias*/
    wifi_getName(wlanIndex, pEntry->StaticInfo.Name);

    pEntry->Cfg.InstanceNumber    = wlanIndex+1;

    CosaDmlWiFiSsidGetCfg((ANSC_HANDLE)hContext,&pEntry->Cfg);
    CosaDmlWiFiSsidGetDinfo((ANSC_HANDLE)hContext,pEntry->Cfg.InstanceNumber,&pEntry->DynamicInfo);
    CosaDmlWiFiSsidGetSinfo((ANSC_HANDLE)hContext,pEntry->Cfg.InstanceNumber,&pEntry->StaticInfo);

    return ANSC_STATUS_SUCCESS;
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
wifiDbgPrintf("%s\n",__FUNCTION__);
    
    return returnStatus;
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
wifiDbgPrintf("%s\n",__FUNCTION__);

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
wifiDbgPrintf("%s\n",__FUNCTION__);

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
    PCOSA_DML_WIFI_SSID_CFG pStoredCfg = NULL;
    int wlanIndex = 0;
    BOOL cfgChange = FALSE;

wifiDbgPrintf("%s\n",__FUNCTION__);

    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
    wlanIndex = pCfg->InstanceNumber-1;
    pStoredCfg = &sWiFiDmlSsidStoredCfg[pCfg->InstanceNumber-1];

    if (pCfg->bEnabled != pStoredCfg->bEnabled) {
        wifi_setEnable(wlanIndex, pCfg->bEnabled);
        cfgChange = TRUE;
    }

    if (strcmp(pCfg->SSID, pStoredCfg->SSID) != 0) {
        wifi_setSSID(wlanIndex, pCfg->SSID);
        cfgChange = TRUE;
    }
       
    if (pCfg->EnableOnline != pStoredCfg->EnableOnline) {
        wifi_setEnableOnLine(wlanIndex, pCfg->EnableOnline);  
        cfgChange = TRUE;
    }

    if (pCfg->RouterEnabled != pStoredCfg->RouterEnabled) {
        wifi_setRouterEnable(wlanIndex, pCfg->RouterEnabled);
        cfgChange = TRUE;
    }

    if (cfgChange == TRUE)
    {
	pCfg->LastChange = AnscGetTickInSeconds(); 
	printf("%s: LastChange %d \n", __func__, pCfg->LastChange);
    }

    memcpy(&sWiFiDmlSsidStoredCfg[pCfg->InstanceNumber-1], pCfg, sizeof(COSA_DML_WIFI_SSID_CFG));
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiSsidApplyCfg
    (
        PCOSA_DML_WIFI_SSID_CFG     pCfg
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    PCOSA_DML_WIFI_SSID_CFG pRunningCfg = NULL;
    int wlanIndex = 0;

wifiDbgPrintf("%s\n",__FUNCTION__);

    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
    wlanIndex = pCfg->InstanceNumber-1;
    wifiDbgPrintf("%s[%d] wlanIndex %d\n",__FUNCTION__, __LINE__, wlanIndex);
    pRunningCfg = &sWiFiDmlSsidRunningCfg[wlanIndex];

    if (strcmp(pCfg->SSID, pRunningCfg->SSID) != 0) {
        wifi_pushSSID(wlanIndex, pCfg->SSID);
    }
       
    memcpy(&sWiFiDmlSsidRunningCfg[pCfg->InstanceNumber-1], pCfg, sizeof(COSA_DML_WIFI_SSID_CFG));

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiSsidGetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_SSID_CFG     pCfg
    )
{ 
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    int wlanIndex = pCfg->InstanceNumber-1;
    int wlanRadioIndex;
    BOOL enabled = FALSE;
    static BOOL firstTime[16] = { true, true, true, true, true, true, true, true, 
                                                   true, true, true, true, true, true, true, true };
wifiDbgPrintf("%s wlanIndex = %d\n",__FUNCTION__, wlanIndex);

    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
    
    if (firstTime[wlanIndex] == TRUE) {
        pCfg->LastChange = AnscGetTickInSeconds(); 
        firstTime[wlanIndex] = FALSE;
    }

    wifi_getRadioIndex(wlanIndex, &wlanRadioIndex);

    _ansc_sprintf(pCfg->Alias, "ath%d",wlanIndex);

    wifi_getEnable(wlanIndex, &enabled);
    pCfg->bEnabled = (enabled == TRUE) ? TRUE : FALSE;

    _ansc_sprintf(pCfg->WiFiRadioName, "wifi%d",wlanRadioIndex);

    wifi_getSSID(wlanIndex, pCfg->SSID);
       
    wifi_getEnableOnLine(wlanIndex, &enabled);
    pCfg->EnableOnline = (enabled == TRUE) ? TRUE : FALSE;

    wifi_getRouterEnable(wlanIndex, &enabled);
    pCfg->RouterEnabled = (enabled == TRUE) ? TRUE : FALSE;

    memcpy(&sWiFiDmlSsidStoredCfg[pCfg->InstanceNumber-1], pCfg, sizeof(COSA_DML_WIFI_SSID_CFG));
    memcpy(&sWiFiDmlSsidRunningCfg[pCfg->InstanceNumber-1], pCfg, sizeof(COSA_DML_WIFI_SSID_CFG));

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiSsidGetDinfo
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber,
        PCOSA_DML_WIFI_SSID_DINFO   pInfo
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    ULONG wlanIndex = ulInstanceNumber-1;
wifiDbgPrintf("%s\n",__FUNCTION__);

    if (!pInfo)
    {
        return ANSC_STATUS_FAILURE;
    }
    
    char vapStatus[32];
	BOOL enabled; 


	wifi_getApEnable(wlanIndex, &enabled);
	// Nothing to do if VAP is not enabled
	if (enabled == FALSE) {
            pInfo->Status = COSA_DML_IF_STATUS_Down;
	} else {

        wifi_getStatus(wlanIndex, vapStatus);

        if (strncmp(vapStatus, "Up", strlen("Up")) == 0)
        {
            pInfo->Status = COSA_DML_IF_STATUS_Up;
        } else if (strncmp(vapStatus,"Disable", strlen("Disable")) == 0)
        {
            pInfo->Status = COSA_DML_IF_STATUS_Down;
        } else 
        {
            pInfo->Status = COSA_DML_IF_STATUS_Error;
        }

    }

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiSsidFillSinfoCache
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
wifiDbgPrintf("%s: ulInstanceNumber = %d\n",__FUNCTION__, ulInstanceNumber);

    ULONG wlanIndex = ulInstanceNumber-1;
    char bssid[32];
    PCOSA_DML_WIFI_SSID_SINFO   pInfo = &gCachedSsidInfo[wlanIndex];

    sprintf(pInfo->Name,"ath%d", wlanIndex);

    memset(bssid,0,sizeof(bssid));
    memset(pInfo->BSSID,0,sizeof(pInfo->BSSID));
    memset(pInfo->MacAddress,0,sizeof(pInfo->MacAddress));

    wifi_getBaseBSSID(wlanIndex, bssid);
        wifiDbgPrintf("%s: wifi_getBaseBSSID returned bssid = %s\n",__FUNCTION__, bssid);

        {
            int i;
            char tmp[20];
            char *pos = bssid;
            for (i=0;i<6;i++) {
                memcpy(tmp, pos, 2);
                tmp[2] = 0;
                pInfo->BSSID[i] = strtol(tmp, (char**)NULL, 0x10 /*in Hex*/);
                pInfo->MacAddress[i] = strtol(tmp, (char**)NULL, 0x10 /*in Hex*/);
                pos+=3;
            }
        }
    wifiDbgPrintf("%s: BSSID %02x%02x%02x%02x%02x%02x\n", __func__,
       pInfo->BSSID[0], pInfo->BSSID[1], pInfo->BSSID[2],
       pInfo->BSSID[3], pInfo->BSSID[4], pInfo->BSSID[5]);
    wifiDbgPrintf("%s: MacAddress %02x%02x%02x%02x%02x%02x\n", __func__,
       pInfo->MacAddress[0], pInfo->MacAddress[1], pInfo->MacAddress[2],
       pInfo->MacAddress[3], pInfo->MacAddress[4], pInfo->MacAddress[5]);

    int i = 0;
    for (i = wlanIndex+2; i < gSsidCount; i += 2) {
        // Fill in the rest based on ath0 and ath1, even numbers index are on wifi0 and odd are on wifi1
        if ((wlanIndex%2) == 0) {
            memcpy(&gCachedSsidInfo[i],&gCachedSsidInfo[0],sizeof(COSA_DML_WIFI_SSID_SINFO));
        } else {
            memcpy(&gCachedSsidInfo[i],&gCachedSsidInfo[1],sizeof(COSA_DML_WIFI_SSID_SINFO));
        }
        sprintf(gCachedSsidInfo[i].Name,"ath%d", i);
        gCachedSsidInfo[i].BSSID[5] += i/2;
        gCachedSsidInfo[i].MacAddress[5] += i/2;
    }

    return ANSC_STATUS_SUCCESS;
}
ANSC_STATUS
CosaDmlWiFiSsidGetSinfo
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber,
        PCOSA_DML_WIFI_SSID_SINFO   pInfo
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
wifiDbgPrintf("%s: ulInstanceNumber = %d\n",__FUNCTION__, ulInstanceNumber);

    if (!pInfo)
    {
        return ANSC_STATUS_FAILURE;
    }
    
    ULONG wlanIndex = ulInstanceNumber-1;
    char bssid[64];

    sprintf(pInfo->Name,"ath%d", wlanIndex);
	memcpy(pInfo,&gCachedSsidInfo[wlanIndex],sizeof(COSA_DML_WIFI_SSID_SINFO));

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
wifiDbgPrintf("%s\n",__FUNCTION__);
	ULONG currentTime = AnscGetTickInSeconds(); 

    // if the last poll was within 10 seconds skip the poll
    // When the whole Stat table is pull the top level DML calls this funtion once
    // for each parameter in the table
    if ( (currentTime - sWiFiDmlSsidLastStatPoll[ulInstanceNumber-1]) < 10)  {
        return ANSC_STATUS_SUCCESS;
    } 
wifiDbgPrintf("%s Getting Stats last poll was %d seconds ago \n",__FUNCTION__, currentTime-sWiFiDmlSsidLastStatPoll[ulInstanceNumber-1] );
    sWiFiDmlSsidLastStatPoll[ulInstanceNumber-1] = currentTime;

    if (!pStats)
    {
        return ANSC_STATUS_FAILURE;
    }
    
    ULONG wlanIndex = ulInstanceNumber-1;
    wifi_basicTrafficStats_t basicStats;
    wifi_trafficStats_t errorStats;
    char status[16];
    BOOL enabled; 

    memset(pStats,0,sizeof(COSA_DML_WIFI_SSID_STATS));

    wifi_getEnable(wlanIndex, &enabled);
    // Nothing to do if VAP is not enabled
    if (enabled == FALSE) {
        return ANSC_STATUS_SUCCESS; 
    }

    wifi_getStatus(wlanIndex, status);
    if (strstr(status,"Up") == NULL) {
        return ANSC_STATUS_SUCCESS; 
    }

    int result = 0;
    result = wifi_getBasicTrafficStats(wlanIndex, &basicStats);
    if (result == 0) {
        pStats->BytesSent = basicStats.wifi_BytesSent;
        pStats->BytesReceived = basicStats.wifi_BytesReceived;
        pStats->PacketsSent = basicStats.wifi_PacketsSent; 
        pStats->PacketsReceived = basicStats.wifi_PacketsReceived;
    }

    result = wifi_getWifiTrafficStats(wlanIndex, &errorStats);
    if (result == 0) {
        pStats->ErrorsSent                         = errorStats.wifi_ErrorsSent;
        pStats->ErrorsReceived                     = errorStats.wifi_ErrorsReceived;
        pStats->UnicastPacketsSent                 = errorStats.wifi_UnicastPacketsSent;
        pStats->UnicastPacketsReceived             = errorStats.wifi_UnicastPacketsReceived;
        pStats->DiscardPacketsSent                 = errorStats.wifi_DiscardedPacketsSent;
        pStats->DiscardPacketsReceived             = errorStats.wifi_DiscardedPacketsReceived;
        pStats->MulticastPacketsSent               = errorStats.wifi_MulticastPacketsSent;
        pStats->MulticastPacketsReceived           = errorStats.wifi_MulticastPacketsReceived;
        pStats->BroadcastPacketsSent               = errorStats.wifi_BroadcastPacketsSent;
        pStats->BroadcastPacketsReceived           = errorStats.wifi_BroadcastPacketsRecevied;
        pStats->UnknownProtoPacketsReceived        = errorStats.wifi_UnknownPacketsReceived;
    }

    return ANSC_STATUS_SUCCESS;
}

BOOL CosaDmlWiFiSsidValidateSSID (void)
{
    BOOL validateFlag = FALSE;
    char *strValue = NULL;
    int retPsmGet = CCSP_SUCCESS;

	retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, ValidateSSIDName, NULL, &strValue);
	if (retPsmGet == CCSP_SUCCESS) {
        validateFlag = _ansc_atoi(strValue);
	    ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
	}

    return validateFlag;
}

ANSC_STATUS
CosaDmlWiFiSsidGetSSID
    (
        ULONG                       ulInstanceNumber,
        char                            *ssid
    )
{
wifiDbgPrintf("%s\n",__FUNCTION__);

    ULONG wlanIndex = ulInstanceNumber-1;

    wifi_getSSID(wlanIndex, ssid);

    return ANSC_STATUS_SUCCESS;

}

/* WiFi AP is always associated with a SSID in the system */
ULONG
CosaDmlWiFiAPGetNumberOfEntries
    (
        ANSC_HANDLE                 hContext
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
wifiDbgPrintf("%s\n",__FUNCTION__);

    return gApCount;
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
wifiDbgPrintf("%s pSsid = %s\n",__FUNCTION__, pSsid);

    CosaDmlWiFiApGetCfg(NULL, pSsid, &pEntry->Cfg);
    CosaDmlWiFiApGetInfo(NULL, pSsid, &pEntry->Info);

    return ANSC_STATUS_SUCCESS;
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
wifiDbgPrintf("%s\n",__FUNCTION__);
    
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiApPushCfg
    (
        PCOSA_DML_WIFI_AP_CFG       pCfg
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
wifiDbgPrintf("%s\n",__FUNCTION__);

    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
        
    int wlanIndex = pCfg->InstanceNumber-1;

    wifi_setWmmEnable(wlanIndex,pCfg->WMMEnable);
    wifi_setWmmUapsdEnable(wlanIndex, pCfg->UAPSDEnable);        

    {
	// Ic and Og policies set the same for first GA release
    // setting Ack policy so negate the NoAck value
	wifi_setWmmOgAckPolicy(wlanIndex, 0, !pCfg->WmmNoAck);
	wifi_setWmmOgAckPolicy(wlanIndex, 1, !pCfg->WmmNoAck);
	wifi_setWmmOgAckPolicy(wlanIndex, 2, !pCfg->WmmNoAck);
	wifi_setWmmOgAckPolicy(wlanIndex, 3, !pCfg->WmmNoAck);
    }
    wifi_setMaxStations(wlanIndex, pCfg->BssMaxNumSta);
    // If Isolation is required (TRUE) then disable bridging
    BOOL enabled = (pCfg->IsolationEnable == TRUE) ? FALSE : TRUE;
    wifi_setApBridging(wlanIndex, enabled);

    wifi_pushSsidAdvertisementEnable(wlanIndex, pCfg->SSIDAdvertisementEnabled);

    memcpy(&sWiFiDmlApRunningCfg[pCfg->InstanceNumber-1], pCfg, sizeof(COSA_DML_WIFI_AP_CFG));

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiApPushMacFilter
    (
        QUEUE_HEADER       *pMfQueue,
        ULONG                      wlanIndex
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    PCOSA_CONTEXT_LINK_OBJECT       pMacFiltLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)NULL; 
    PCOSA_DML_WIFI_AP_MAC_FILTER    pMacFilt            = (PCOSA_DML_WIFI_AP_MAC_FILTER        )NULL;
wifiDbgPrintf("%s\n",__FUNCTION__);

    if (!pMfQueue)
    {
        return ANSC_STATUS_FAILURE;
    }
wifiDbgPrintf("%s : %d filters \n",__FUNCTION__, pMfQueue->Depth);

    int index;
    for (index = 0; index < pMfQueue->Depth; index++) {
        pMacFiltLinkObj = (PCOSA_CONTEXT_LINK_OBJECT) AnscSListGetEntryByIndex((PSLIST_HEADER)pMfQueue, index);
        if (pMacFiltLinkObj) {
            pMacFilt =  (PCOSA_DML_WIFI_AP_MAC_FILTER) pMacFiltLinkObj->hContext;
            wifi_addAclDevice(wlanIndex, pMacFilt->MACAddress);
        }
    }
    
        
    return ANSC_STATUS_SUCCESS;
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
    PCOSA_DML_WIFI_AP_CFG pStoredCfg = NULL;
wifiDbgPrintf("%s\n",__FUNCTION__);

    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
    pStoredCfg = &sWiFiDmlApStoredCfg[pCfg->InstanceNumber-1].Cfg;
        
    int wlanIndex;
    int wlanRadioIndex;

    wifi_getIndexFromName(pSsid, &wlanIndex);
    if (wlanIndex == -1) 
    {
	// Error could not find index
	return ANSC_STATUS_FAILURE;
    }
    wifi_getRadioIndex(wlanIndex, &wlanRadioIndex);
    pCfg->InstanceNumber = wlanIndex+1;

    CosaDmlWiFiSetAccessPointPsmData(pCfg);

    // only set on SSID
    // wifi_setEnable(wlanIndex, pCfg->bEnabled);

    /* USGv2 Extensions */
    // static value for first GA release not settable
    pCfg->LongRetryLimit = 16;
    pCfg->RetryLimit     = 16;

    // These should be pushed when the SSID is up
    //  They are currently set from ApGetCfg when it call GetAccessPointPsmData
    #if 0 
    wifi_setWmmEnable(wlanIndex,pCfg->WMMEnable);
    wifi_setWmmUapsdEnable(wlanIndex, pCfg->UAPSDEnable);        

    {
	// Ic and Og policies set the same for first GA release
	wifi_setWmmOgAckPolicy(wlanIndex, 0, pCfg->WmmNoAck);
	wifi_setWmmOgAckPolicy(wlanIndex, 1, pCfg->WmmNoAck);
	wifi_setWmmOgAckPolicy(wlanIndex, 2, pCfg->WmmNoAck);
	wifi_setWmmOgAckPolicy(wlanIndex, 3, pCfg->WmmNoAck);
    }
    wifi_setMaxStations(wlanIndex, pCfg->BssMaxNumSta);
    // If Isolation is required (TRUE) then disable bridging
    BOOL enabled = (pCfg->IsolationEnable == TRUE) ? FALSE : TRUE;
    wifi_setApBridging(wlanIndex, enabled);
    #endif

    if (pCfg->SSIDAdvertisementEnabled != pStoredCfg->SSIDAdvertisementEnabled) {
        wifi_setSsidAdvertisementEnable(wlanIndex, pCfg->SSIDAdvertisementEnabled);
    }

    // pCfg->MulticastRate = 123;
    // pCfg->BssCountStaAsCpe  = TRUE;

    if (pCfg->KickAssocDevices == TRUE) {
        CosaDmlWiFiApKickAssocDevices(pSsid);
        pCfg->KickAssocDevices = FALSE;
    }

    memcpy(&sWiFiDmlApStoredCfg[pCfg->InstanceNumber-1].Cfg, pCfg, sizeof(COSA_DML_WIFI_AP_CFG));

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiApApplyCfg
    (
        PCOSA_DML_WIFI_AP_CFG       pCfg
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    PCOSA_DML_WIFI_AP_CFG pRunningCfg = NULL;
    int wlanIndex;
wifiDbgPrintf("%s\n",__FUNCTION__);

    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
    wlanIndex = pCfg->InstanceNumber-1;
    wifiDbgPrintf("%s[%d] wlanIndex %d\n",__FUNCTION__, __LINE__, wlanIndex);

    pRunningCfg = &sWiFiDmlApRunningCfg[pCfg->InstanceNumber-1].Cfg ;

    /* USGv2 Extensions */
    // static value for first GA release not settable
    // pCfg->LongRetryLimit = 16;
    // pCfg->RetryLimit     = 16;

    // These should be pushed when the SSID is up
    //  They are currently set from ApGetCfg when it call GetAccessPointPsmData
    if (pCfg->WMMEnable != pRunningCfg->WMMEnable) {
        wifi_setWmmEnable(wlanIndex,pCfg->WMMEnable);
    }
    if (pCfg->UAPSDEnable != pRunningCfg->UAPSDEnable) {
        wifi_setWmmUapsdEnable(wlanIndex, pCfg->UAPSDEnable);        
    }

    if (pCfg->WmmNoAck != pRunningCfg->WmmNoAck) {
        // Ic and Og policies set the same for first GA release
		wifi_setWmmOgAckPolicy(wlanIndex, 0, !pCfg->WmmNoAck);
		wifi_setWmmOgAckPolicy(wlanIndex, 1, !pCfg->WmmNoAck);
		wifi_setWmmOgAckPolicy(wlanIndex, 2, !pCfg->WmmNoAck);
		wifi_setWmmOgAckPolicy(wlanIndex, 3, !pCfg->WmmNoAck);
    }

    if (pCfg->BssMaxNumSta != pRunningCfg->BssMaxNumSta) {
        // Check to see the current # of associated devices exceeds the new limit
        // if so kick all the devices off
        unsigned int devNum;
        wifi_getNumDevicesAssociated(wlanIndex, &devNum);
        if (devNum > pCfg->BssMaxNumSta) {
            char ssidName[COSA_DML_WIFI_MAX_SSID_NAME_LEN];
            wifi_getName(wlanIndex, ssidName);
            CosaDmlWiFiApKickAssocDevices(ssidName);
        }
        wifi_setMaxStations(wlanIndex, pCfg->BssMaxNumSta);
    }

    if (pCfg->IsolationEnable != pRunningCfg->IsolationEnable) {
        // If Isolation is required (TRUE) then disable bridging
    BOOL enabled = (pCfg->IsolationEnable == TRUE) ? FALSE : TRUE;
    wifi_setApBridging(wlanIndex, enabled);
    }

    if (pCfg->SSIDAdvertisementEnabled != pRunningCfg->SSIDAdvertisementEnabled) {
        wifi_pushSsidAdvertisementEnable(wlanIndex, pCfg->SSIDAdvertisementEnabled);
        sWiFiDmlUpdatedAdvertisement[wlanIndex] = TRUE;
    }

    // pCfg->MulticastRate = 123;
    // pCfg->BssCountStaAsCpe  = TRUE;

    memcpy(&sWiFiDmlApRunningCfg[pCfg->InstanceNumber-1], pCfg, sizeof(COSA_DML_WIFI_AP_CFG));

    return ANSC_STATUS_SUCCESS;
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
wifiDbgPrintf("%s pSsid = %s\n",__FUNCTION__, pSsid);

    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
        
    int wlanIndex;
    int wlanRadioIndex;
    BOOL enabled = FALSE;

    wifi_getIndexFromName(pSsid, &wlanIndex);
    if (wlanIndex == -1) 
    {
wifiDbgPrintf("%s pSsid = %s Couldn't find wlanIndex \n",__FUNCTION__, pSsid);
	// Error could not find index
	return ANSC_STATUS_FAILURE;
    }
    wifi_getRadioIndex(wlanIndex, &wlanRadioIndex);
    pCfg->InstanceNumber = wlanIndex+1;
    sprintf(pCfg->Alias,"AccessPoint%d", pCfg->InstanceNumber);

    wifi_getEnable(wlanIndex, &enabled);
    pCfg->bEnabled = (enabled == TRUE) ? TRUE : FALSE;

    CosaDmlWiFiGetAccessPointPsmData(pCfg);

    /* USGv2 Extensions */
    // static value for first GA release not settable
    pCfg->LongRetryLimit = 16;
    pCfg->RetryLimit     = 16;

    sprintf(pCfg->SSID, "Device.WiFi.SSID.%d.", wlanIndex+1);

    wifi_getSsidAdvertisementEnable(wlanIndex,  &enabled);
    pCfg->SSIDAdvertisementEnabled = (enabled == TRUE) ? TRUE : FALSE;

    pCfg->MulticastRate = 123;
    pCfg->BssCountStaAsCpe  = TRUE;


    // pCfg->BssUserStatus

    memcpy(&sWiFiDmlApStoredCfg[pCfg->InstanceNumber-1].Cfg, pCfg, sizeof(COSA_DML_WIFI_AP_CFG));
    memcpy(&sWiFiDmlApRunningCfg[pCfg->InstanceNumber-1].Cfg, pCfg, sizeof(COSA_DML_WIFI_AP_CFG));

    return ANSC_STATUS_SUCCESS;
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
    int wlanIndex;
    BOOL enabled = FALSE;

wifiDbgPrintf("%s pSsid = %s\n",__FUNCTION__, pSsid);

    if (!pInfo)
    {
        return ANSC_STATUS_FAILURE;
    }
    
    wifi_getIndexFromName(pSsid, &wlanIndex);
    if (wlanIndex == -1) 
    {
	// Error could not find index
	return ANSC_STATUS_FAILURE;
    }

    wifi_getApEnable(wlanIndex,&enabled);
    pInfo->Status = (enabled == TRUE) ? COSA_DML_WIFI_AP_STATUS_Enabled : COSA_DML_WIFI_AP_STATUS_Disabled;
    pInfo->BssUserStatus = (enabled == TRUE) ? 1 : 2;
	pInfo->WMMCapability = TRUE;        
	pInfo->UAPSDCapability = TRUE;

    return ANSC_STATUS_SUCCESS;
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
    int wlanIndex;

wifiDbgPrintf("%s pSsid = %s\n",__FUNCTION__, pSsid);

    if (!pEntry)
    {
        return ANSC_STATUS_FAILURE;
    }

    pEntry->Info.ModesSupported = COSA_DML_WIFI_SECURITY_None | COSA_DML_WIFI_SECURITY_WEP_64 | COSA_DML_WIFI_SECURITY_WEP_128 | 
				  COSA_DML_WIFI_SECURITY_WPA_Personal | COSA_DML_WIFI_SECURITY_WPA2_Personal | 
				  COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal | COSA_DML_WIFI_SECURITY_WPA_Enterprise |
				  COSA_DML_WIFI_SECURITY_WPA2_Enterprise | COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise;
    
    CosaDmlWiFiApSecGetCfg((ANSC_HANDLE)hContext, pSsid, &pEntry->Cfg);

    wifi_getIndexFromName(pSsid, &wlanIndex);
    if (wlanIndex == -1) 
    {
	// Error could not find index
	return ANSC_STATUS_FAILURE;
    }

    if (pEntry->Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_64)
    {
	CosaDmlWiFi_GetWEPKey64ByIndex(wlanIndex+1, 0, &pEntry->WEPKey64Bit[0]);
	CosaDmlWiFi_GetWEPKey64ByIndex(wlanIndex+1, 1, &pEntry->WEPKey64Bit[1]);
	CosaDmlWiFi_GetWEPKey64ByIndex(wlanIndex+1, 2, &pEntry->WEPKey64Bit[2]);
	CosaDmlWiFi_GetWEPKey64ByIndex(wlanIndex+1, 3, &pEntry->WEPKey64Bit[3]);
    } else if (pEntry->Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_128)
    {
	CosaDmlWiFi_GetWEPKey128ByIndex(wlanIndex+1, 0, &pEntry->WEPKey128Bit[0]);
	CosaDmlWiFi_GetWEPKey128ByIndex(wlanIndex+1, 1, &pEntry->WEPKey128Bit[1]);
	CosaDmlWiFi_GetWEPKey128ByIndex(wlanIndex+1, 2, &pEntry->WEPKey128Bit[2]);
	CosaDmlWiFi_GetWEPKey128ByIndex(wlanIndex+1, 3, &pEntry->WEPKey128Bit[3]);
    }

    memcpy(&sWiFiDmlApSecurityStored[wlanIndex], pEntry, sizeof(COSA_DML_WIFI_APSEC_FULL));
    memcpy(&sWiFiDmlApSecurityRunning[wlanIndex], pEntry, sizeof(COSA_DML_WIFI_APSEC_FULL));

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiApSecGetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APSEC_CFG    pCfg
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    int wlanIndex;
    char securityType[32];

wifiDbgPrintf("%s pSsid = %s\n",__FUNCTION__, pSsid);

    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
    
    wifi_getIndexFromName(pSsid, &wlanIndex);
    if (wlanIndex == -1) 
    {
	// Error could not find index
	return ANSC_STATUS_FAILURE;
    }

    wifi_getBeaconType(wlanIndex, securityType);
    if (strncmp(securityType,"None", strlen("None")) == 0)
    {
	pCfg->ModeEnabled =  COSA_DML_WIFI_SECURITY_None; 
    } else if (strncmp(securityType,"Basic", strlen("Basic")) == 0)
    { 
	ULONG wepLen;
	char *strValue = NULL;
	char recName[256];
	int retPsmGet = CCSP_SUCCESS;

	memset(recName, 0, sizeof(recName));
	sprintf(recName, WepKeyLength, wlanIndex+1);
	retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
	if (retPsmGet == CCSP_SUCCESS) {
	    wepLen = _ansc_atoi(strValue);
	} else {
            // Default to 128
            wepLen = 128;
	    PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "128" );
        }

	if (wepLen == 64)
	{ 
	    pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_WEP_64; 
	} else if (wepLen == 128)
	{
	    pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_WEP_128;
	}

    } else if (strncmp(securityType,"WPAand11i", strlen("WPAand11i")) == 0)
    {
	pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal;
    } else if (strncmp(securityType,"WPA", strlen("WPA")) == 0)
    {
	pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_WPA_Personal;
    } else if (strncmp(securityType,"11i", strlen("11i")) == 0)
    {
	pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_WPA2_Personal;
    } else
    { 
	pCfg->ModeEnabled =  COSA_DML_WIFI_SECURITY_None; 
    }
    
    wifi_getApWepKeyIndex(wlanIndex, (unsigned int *) &pCfg->DefaultKey);
    wifi_getPreSharedKey(wlanIndex, pCfg->PreSharedKey);
    wifi_getKeyPassphrase(wlanIndex, pCfg->KeyPassphrase);

    { 
	char method[32];

	wifi_getWpaEncryptionMode(wlanIndex, method);

	if (strncmp(method, "TKIPEncryption",strlen("TKIPEncryption")) == 0)
	{ 
	    pCfg->EncryptionMethod = COSA_DML_WIFI_AP_SEC_TKIP;
	} else if (strncmp(method, "AESEncryption",strlen("AESEncryption")) == 0)
	{
	    pCfg->EncryptionMethod = COSA_DML_WIFI_AP_SEC_AES;
	} else if (strncmp(method, "TKIPandAESEncryption",strlen("TKIPandAESEncryption")) == 0)
	{
	    pCfg->EncryptionMethod = COSA_DML_WIFI_AP_SEC_AES_TKIP;
	} 
    } 
    wifi_getWpaRekeyInterval(wlanIndex,  (unsigned int *) &pCfg->RekeyingInterval);

    // Don't set for first GA release
    #if 0
    pCfg->RadiusReAuthInterval;
    pCfg->RadiusSecret;
    pCfg->RadiusServerIPAddr;
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
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    PCOSA_DML_WIFI_APSEC_CFG pStoredCfg = NULL;
wifiDbgPrintf("%s\n",__FUNCTION__);

    int wlanIndex;
    char securityType[32];
    char authMode[32];

    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
    
    wifi_getIndexFromName(pSsid, &wlanIndex);
    if (wlanIndex == -1) 
    {
	// Error could not find index
	return ANSC_STATUS_FAILURE;
    }
    pStoredCfg = &sWiFiDmlApSecurityStored[wlanIndex].Cfg;

    if (pCfg->ModeEnabled != pStoredCfg->ModeEnabled) {

        if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_None) 
       {
            strcpy(securityType,"None");
            strcpy(authMode,"None");
        } else if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_64 ||
                   pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_128 ) 
        {
            ULONG wepLen;
            char *strValue = NULL;
            char recName[256];
            int retPsmSet = CCSP_SUCCESS;

            strcpy(securityType,"Basic");
            strcpy(authMode,"None");

            wifi_setBasicEncryptionMode(wlanIndex, "WEPEncryption");

            memset(recName, 0, sizeof(recName));
            sprintf(recName, WepKeyLength, wlanIndex+1);
            if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_64) 
           {
                PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "64" );
            } else if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_128)
            {
                PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "128" );
            }

        } else if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal) 
        {
            strcpy(securityType,"WPAand11i");
            strcpy(authMode,"PSKAuthentication");
        } else if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_Personal) 
        {
            strcpy(securityType,"WPA");
            strcpy(authMode,"PSKAuthentication");
        } else if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Personal) 
        {
            strcpy(securityType,"11i");
            strcpy(authMode,"PSKAuthentication");
        } else
        {
            strcpy(securityType,"None");
            strcpy(authMode,"None");
        }
        wifi_setBeaconType(wlanIndex, securityType);
        wifi_setBasicAuthenticationMode(wlanIndex, authMode);
    }
    
    if (pCfg->DefaultKey != pStoredCfg->DefaultKey) {
        wifi_setApWepKeyIndex(wlanIndex, pCfg->DefaultKey);
    }

    if (strcmp(pCfg->PreSharedKey,pStoredCfg->PreSharedKey) != 0) {
        if (strlen(pCfg->PreSharedKey) > 0) { 
        wifi_setPreSharedKey(wlanIndex, pCfg->PreSharedKey);
        }
    }

    if (strcmp(pCfg->KeyPassphrase, pStoredCfg->KeyPassphrase) != 0) {
        if (strlen(pCfg->KeyPassphrase) > 0) { 
        wifi_setKeyPassphrase(wlanIndex, pCfg->KeyPassphrase);
    }
    }

    // WPA
    if ( pCfg->EncryptionMethod != pStoredCfg->EncryptionMethod &&
         pCfg->ModeEnabled >= COSA_DML_WIFI_SECURITY_WPA_Personal &&
         pCfg->ModeEnabled <= COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise )
    { 
	char method[32];

        memset(method,0,sizeof(method));
	if ( pCfg->EncryptionMethod == COSA_DML_WIFI_AP_SEC_TKIP)
	{ 
            strcpy(method,"TKIPEncryption");
	} else if ( pCfg->EncryptionMethod == COSA_DML_WIFI_AP_SEC_AES)
	{
            strcpy(method,"AESEncryption");
	} else if ( pCfg->EncryptionMethod == COSA_DML_WIFI_AP_SEC_AES_TKIP)
	{
            strcpy(method,"TKIPandAESEncryption");
	} 
		wifi_setWpaEncryptionMode(wlanIndex, method);
    } 

    if ( pCfg->RekeyingInterval != pStoredCfg->RekeyingInterval) {
        wifi_setWpaRekeyInterval(wlanIndex,  pCfg->RekeyingInterval);
    }

    // Don't set for first GA release
    #if 0
    pCfg->RadiusReAuthInterval;
    pCfg->RadiusSecret;
    pCfg->RadiusServerIPAddr;
    pCfg->RadiusServerPort;
    #endif

    memcpy(&sWiFiDmlApSecurityStored[wlanIndex].Cfg, pCfg, sizeof(COSA_DML_WIFI_APSEC_CFG));

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiApSecApplyWepCfg
(
PCOSA_DML_WIFI_APSEC_CFG    pCfg,
ULONG                                          instanceNumber
)
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    wifiDbgPrintf("%s\n",__FUNCTION__);

    int wlanIndex = instanceNumber-1;
    wifiDbgPrintf("%s[%d] wlanIndex %d\n",__FUNCTION__, __LINE__, wlanIndex);

    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }

    if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_64 || 
        pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_128 )
    {

        // Push Defualt Key to SSID as key 1.  This is to compensate for a Qualcomm bug
        wifi_pushWepKeyIndex( wlanIndex, 1);
        wifiDbgPrintf("%s[%d] wlanIndex %d DefualtKey %d \n",__FUNCTION__, __LINE__, wlanIndex, pCfg->DefaultKey);
        wifi_pushWepKey( wlanIndex, pCfg->DefaultKey);
        #if 0
        int i;
        for (i = 1; i <= 4; i++)
        {
        }
        wifi_setAuthMode(wlanIndex, 4);
        #endif

    }

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiApSecApplyCfg
(
PCOSA_DML_WIFI_APSEC_CFG    pCfg,
ULONG                                          instanceNumber
)
{
    PCOSA_DML_WIFI_APSEC_CFG pRunningCfg = &sWiFiDmlApSecurityRunning[instanceNumber-1].Cfg;
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    wifiDbgPrintf("%s\n",__FUNCTION__);

    int wlanIndex = instanceNumber-1;
    wifiDbgPrintf("%s[%d] wlanIndex %d\n",__FUNCTION__, __LINE__, wlanIndex);

    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }

    // Make sure there is no entry in the /tmp/conf_finename
    wifi_removeSecVaribles(wlanIndex);

    // Reset security to off 
    wifiDbgPrintf("%s %d Set encryptionOFF to reset security \n",__FUNCTION__, __LINE__);
    wifi_disableEncryption(wlanIndex);

    // If the Running config has security = WPA or None hostapd must be restarted
    if ( (pRunningCfg->ModeEnabled >= COSA_DML_WIFI_SECURITY_WPA_Personal && 
          pRunningCfg->ModeEnabled <= COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise) ||
         (pRunningCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_None) )
    {
        wifiDbgPrintf("%s %d sWiFiDmlRestartHostapd set to TRUE\n",__FUNCTION__, __LINE__);
        sWiFiDmlRestartHostapd = TRUE;
    } else {
        // If the new config has security = WPA or None hostapd must be restarted
        if ( (pCfg->ModeEnabled >= COSA_DML_WIFI_SECURITY_WPA_Personal && 
              pCfg->ModeEnabled <= COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise) ||
             (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_None) )
        {
            wifiDbgPrintf("%s %d sWiFiDmlRestartHostapd set to TRUE\n",__FUNCTION__, __LINE__);
            sWiFiDmlRestartHostapd = TRUE;
        }
    }

    if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_None) 
    {
        int wpsCfg = 0;
        BOOL enableWps = FALSE;

        wifi_getWpsEnable(wlanIndex, &wpsCfg);
        enableWps = (wpsCfg == 0) ? FALSE : TRUE;

        if (enableWps == TRUE)
        {
            // create WSC_ath*.conf file
            wifi_createHostApdConfig(wlanIndex, TRUE);
        }

    } else if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_64 ||  pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_128 )
    {
        // Check the other primary SSID, if it is WPA and the current is going from WPA to WEP, turn off WPS
        if (wlanIndex < 2)
        {
            int checkIndex = (wlanIndex == 0) ? 1 : 0;
        
            // if the other Primary SSID (ath0/ath1) has security set to WPA or None with WPS enabled, WPS must be turned off
            if ( ( (sWiFiDmlApSecurityStored[checkIndex].Cfg.ModeEnabled >= COSA_DML_WIFI_SECURITY_WPA_Personal) ||
                   (sWiFiDmlApSecurityStored[checkIndex].Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_None) ) &&
                 (pRunningCfg->ModeEnabled >= COSA_DML_WIFI_SECURITY_WPA2_Personal) )
            {
                int wpsCfg = 0;
                BOOL enableWps = FALSE;

                wifi_getWpsEnable(checkIndex, &wpsCfg);
                enableWps = (wpsCfg == 0) ? FALSE : TRUE;

                if (enableWps == TRUE)
                {
                    wifi_removeSecVaribles(checkIndex);
                    wifi_createHostApdConfig(checkIndex, FALSE);

                    wifiDbgPrintf("%s %d sWiFiDmlRestartHostapd set to TRUE\n",__FUNCTION__, __LINE__);
                    sWiFiDmlRestartHostapd = TRUE;
                }
            }
        }
        
        sWiFiDmlPushWepKeys[wlanIndex] = TRUE;
        
        } else if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_Personal ||
               pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Personal ||
               pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal ||
               pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_Enterprise || 
               pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Enterprise ||
               pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise )
    {
        // WPA
        int wpsCfg = 0;
        BOOL enableWps = FALSE;

        wifi_getWpsEnable(wlanIndex, &wpsCfg);
        enableWps = (wpsCfg == 0) ? FALSE : TRUE;

        if (sWiFiDmlApStoredCfg[0].Cfg.SSIDAdvertisementEnabled == FALSE || 
            sWiFiDmlApStoredCfg[1].Cfg.SSIDAdvertisementEnabled == FALSE ||
            sWiFiDmlApSecurityStored[0].Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_64 ||
            sWiFiDmlApSecurityStored[0].Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_128 ||
            sWiFiDmlApSecurityStored[1].Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_64 ||
            sWiFiDmlApSecurityStored[1].Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_128)
        {
            enableWps = FALSE;
        }

        wifi_createHostApdConfig(wlanIndex, enableWps);

       // Check the other primary SSID, if it is WPA recreate the config file
        if (wlanIndex < 2)
        {
            int checkIndex = (wlanIndex == 0) ? 1 : 0;

            // If the other SSID is running WPA recreate the config file
            if ( sWiFiDmlApSecurityStored[checkIndex].Cfg.ModeEnabled >= COSA_DML_WIFI_SECURITY_WPA_Personal ) 
            {
                wifi_getWpsEnable(wlanIndex, &wpsCfg);
                enableWps = (wpsCfg == 0) ? FALSE : TRUE;

                if (sWiFiDmlApStoredCfg[0].Cfg.SSIDAdvertisementEnabled == FALSE || 
                    sWiFiDmlApStoredCfg[1].Cfg.SSIDAdvertisementEnabled == FALSE) 
                { 
                    enableWps = FALSE;
                }
printf("%s: recreating sec file enableWps = %s \n" , __FUNCTION__, ( enableWps == FALSE ) ? "FALSE" : "TRUE");
                wifi_removeSecVaribles(checkIndex);
                wifi_createHostApdConfig(checkIndex, enableWps);
            }
        }

    }

    memcpy(&sWiFiDmlApSecurityRunning[wlanIndex].Cfg, pCfg, sizeof(COSA_DML_WIFI_APSEC_CFG));
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiApSecPushCfg
    (
        PCOSA_DML_WIFI_APSEC_CFG    pCfg,
        ULONG                                          instanceNumber
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS; 
wifiDbgPrintf("%s\n",__FUNCTION__);

    int wlanIndex = instanceNumber-1;

    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
    
    // Make sure there is no entry in the /tmp/conf_finename
    wifi_removeSecVaribles(wlanIndex);

    if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_None)
   {
        int wpsCfg = 0;
        BOOL enableWps = FALSE;

        wifi_getWpsEnable(wlanIndex, &wpsCfg);
        enableWps = (wpsCfg == 0) ? FALSE : TRUE;

        if (enableWps == TRUE)
        {
            sWiFiDmlRestartHostapd = TRUE;
            wifiDbgPrintf("%s %d sWiFiDmlRestartHostapd set to TRUE\n",__FUNCTION__, __LINE__);
            // create WSC_ath*.conf file
            wifi_createHostApdConfig(wlanIndex, TRUE);
        }

    } else if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_64 || 
                   pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_128 ) { 

        // Check the other primary SSID, if it is WPA and the current is going from WPA to WEP, turn off WPS
        if (wlanIndex < 2)
        {
            int checkIndex = (wlanIndex == 0) ? 1 : 0;
        
            // if the other Primary SSID (ath0/ath1) has security set to WPA or None with WPS enabled, WPS must be turned off
            if ( (sWiFiDmlApSecurityStored[checkIndex].Cfg.ModeEnabled >= COSA_DML_WIFI_SECURITY_WPA_Personal) ||
                   (sWiFiDmlApSecurityStored[checkIndex].Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_None) ) 
            {
                int wpsCfg = 0;
                BOOL enableWps = FALSE;

                wifi_getWpsEnable(checkIndex, &wpsCfg);
                enableWps = (wpsCfg == 0) ? FALSE : TRUE;

                if (enableWps == TRUE)
                {
                    wifi_removeSecVaribles(checkIndex);
                    wifi_disableEncryption(checkIndex);

                    // Only create sec file for WPA, secath.  Can't run None/WPS with WEP on another SSID
                    if (sWiFiDmlApSecurityStored[checkIndex].Cfg.ModeEnabled >= COSA_DML_WIFI_SECURITY_WPA_Personal)
                    {
                        wifi_createHostApdConfig(checkIndex, FALSE);
                    }

                    wifiDbgPrintf("%s %d sWiFiDmlRestartHostapd set to TRUE\n",__FUNCTION__, __LINE__);
                    sWiFiDmlRestartHostapd = TRUE;
                }
            }
        }

        sWiFiDmlPushWepKeys[wlanIndex] = TRUE;

    } else  if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_Personal ||
                pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Personal ||
                pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal ||
                pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_Enterprise || 
                pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Enterprise ||
                pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise )
    { 
        // WPA
        int wpsCfg = 0;
        BOOL enableWps = FALSE;

        wifi_getWpsEnable(wlanIndex, &wpsCfg);
        enableWps = (wpsCfg == 0) ? FALSE : TRUE;

        if (sWiFiDmlApStoredCfg[0].Cfg.SSIDAdvertisementEnabled == FALSE || 
            sWiFiDmlApStoredCfg[1].Cfg.SSIDAdvertisementEnabled == FALSE ||
            sWiFiDmlApSecurityStored[0].Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_64 ||
            sWiFiDmlApSecurityStored[0].Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_128 ||
            sWiFiDmlApSecurityStored[1].Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_64 ||
            sWiFiDmlApSecurityStored[1].Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_128) {
            enableWps = FALSE;
        }

        wifi_createHostApdConfig(wlanIndex, enableWps);
        sWiFiDmlRestartHostapd = TRUE;
wifiDbgPrintf("%s %d sWiFiDmlRestartHostapd set to TRUE\n",__FUNCTION__, __LINE__);
    }

    memcpy(&sWiFiDmlApSecurityRunning[wlanIndex].Cfg, pCfg, sizeof(COSA_DML_WIFI_APSEC_CFG));

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiApWpsGetEntry
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APWPS_FULL   pEntry
    )
{
    ANSC_STATUS               returnStatus = ANSC_STATUS_SUCCESS;
    int wlanIndex;
    PCOSA_DML_WIFI_APWPS_CFG  pCfg = &pEntry->Cfg;
    PCOSA_DML_WIFI_APWPS_INFO pInfo = &pEntry->Info;
  
    wifi_getIndexFromName(pSsid, &wlanIndex);
    if (wlanIndex == -1) 
    {
        // Error could not find index
        return ANSC_STATUS_FAILURE;
    }

    returnStatus = CosaDmlWiFiApWpsGetCfg ( hContext, pSsid, pCfg );

    if (returnStatus == ANSC_STATUS_SUCCESS) {
        returnStatus = CosaDmlWiFiApWpsGetInfo ( hContext, pSsid, pInfo );
    }
    
    memcpy(&sWiFiDmlApWpsRunning[wlanIndex], pEntry, sizeof(COSA_DML_WIFI_APWPS_FULL));
    memcpy(&sWiFiDmlApWpsStored[wlanIndex], pEntry, sizeof(COSA_DML_WIFI_APWPS_FULL));
    return returnStatus;
}

ANSC_STATUS
CosaDmlWiFiApWpsApplyCfg
    (
        PCOSA_DML_WIFI_APWPS_CFG    pCfg,
        ULONG                       index
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    PCOSA_DML_WIFI_APWPS_CFG    pRunningCfg = &sWiFiDmlApWpsRunning[index].Cfg;
wifiDbgPrintf("%s\n",__FUNCTION__);

    wifiDbgPrintf("%s[%d] wlanIndex %d\n",__FUNCTION__, __LINE__, index);
    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }

    if (memcmp(pCfg, pRunningCfg, sizeof(COSA_DML_WIFI_APWPS_CFG)) != 0) {
        sWiFiDmlRestartHostapd = TRUE;
wifiDbgPrintf("%s %d sWiFiDmlRestartHostapd set to TRUE\n",__FUNCTION__, __LINE__);
    }
wifiDbgPrintf("%s %d\n",__FUNCTION__, __LINE__);

    memcpy(pRunningCfg, pCfg, sizeof(COSA_DML_WIFI_APWPS_CFG));
wifiDbgPrintf("%s %d\n",__FUNCTION__, __LINE__);
    return ANSC_STATUS_SUCCESS;
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
    PCOSA_DML_WIFI_APWPS_CFG pStoredCfg = NULL;
    int wlanIndex;
    char methodsEnabled[64];
    char recName[256];
    char strValue[32];
    int retPsmSet = CCSP_SUCCESS;

wifiDbgPrintf("%s\n",__FUNCTION__);

    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }


    wifi_getIndexFromName(pSsid, &wlanIndex);
    if (wlanIndex == -1) 
    {
        // Error could not find index
        return ANSC_STATUS_FAILURE;
    }
    pStoredCfg = &sWiFiDmlApWpsStored[wlanIndex].Cfg;

    if (pCfg->bEnabled != pStoredCfg->bEnabled)
    {
        unsigned int pin;
        wifi_getWpsDevicePIN(wlanIndex, &pin);

        if (pCfg->bEnabled == TRUE && pin != 0)
        {
            wifi_setWpsEnable(wlanIndex, 2);
        } else
        {
            wifi_setWpsEnable(wlanIndex, pCfg->bEnabled);
        }
    }

    if (pCfg->ConfigMethodsEnabled != pStoredCfg->ConfigMethodsEnabled)
    {
	// Label and Display should always be set.  The currently DML enum does not include them, but they must be 
	// set on the Radio
	if ( pCfg->ConfigMethodsEnabled == COSA_DML_WIFI_WPS_METHOD_PushButton ) {
	    wifi_setWpsConfigMethodsEnabled(wlanIndex,"PushButton");
	} else  if (pCfg->ConfigMethodsEnabled == COSA_DML_WIFI_WPS_METHOD_Pin) {
	    wifi_setWpsConfigMethodsEnabled(wlanIndex,"Keypad,Label,Display");
	} else if ( pCfg->ConfigMethodsEnabled == (COSA_DML_WIFI_WPS_METHOD_PushButton|COSA_DML_WIFI_WPS_METHOD_Pin) ) {
	    wifi_setWpsConfigMethodsEnabled(wlanIndex,"PushButton,Keypad,Label,Display");
	} 
    } 
    
    if (pCfg->WpsPushButton != pStoredCfg->WpsPushButton) {

        sprintf(recName, WpsPushButton, wlanIndex+1);
        sprintf(strValue,"%d", pCfg->WpsPushButton);
        printf("%s: Setting %s to %d(%s)\n", __FUNCTION__, recName, pCfg->WpsPushButton, strValue);
        retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
        if (retPsmSet != CCSP_SUCCESS) {
            wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting %s \n",__FUNCTION__, retPsmSet, recName); 
        }
    }

    // looks like hostapd_cli allows for settings a timeout when the pin is set
    // would need to expand or create a new API that could handle both.
    // Either ClientPin or ActivatePushButton should be set
    if (strlen(pCfg->X_CISCO_COM_ClientPin) > 0) {
		wifi_setWpsEnrolleePin(wlanIndex,pCfg->X_CISCO_COM_ClientPin);
    } else if (pCfg->X_CISCO_COM_ActivatePushButton == TRUE) {
        wifi_setWpsButtonPush(wlanIndex);
    } else if (pCfg->X_CISCO_COM_CancelSession == TRUE) {
        wifi_cancelWPS(wlanIndex);
    }

    // reset Pin and Activation
    memset(pCfg->X_CISCO_COM_ClientPin, 0, sizeof(pCfg->X_CISCO_COM_ClientPin));
    pCfg->X_CISCO_COM_ActivatePushButton = FALSE;
    pCfg->X_CISCO_COM_CancelSession = FALSE;
    
    // Notify WiFiExtender that Wps has changed
    {
        CosaDml_NotifyWiFiExt(COSA_WIFIEXT_DM_UPDATE_WPS);
    }

    memcpy(&sWiFiDmlApWpsStored[wlanIndex].Cfg, pCfg, sizeof(COSA_DML_WIFI_APWPS_CFG));

    return ANSC_STATUS_SUCCESS;

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
    int wlanIndex;
    char methodsEnabled[64];
    char recName[256];
    char *strValue = NULL;
    int retPsmGet = CCSP_SUCCESS;

wifiDbgPrintf("%s\n",__FUNCTION__);
    
    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }

    wifi_getIndexFromName(pSsid, &wlanIndex);
    if (wlanIndex == -1) 
    {
        // Error could not find index
        return ANSC_STATUS_FAILURE;
    }

    int wpsEnabled = 0;
    wifi_getWpsEnable(wlanIndex, &wpsEnabled);
    pCfg->bEnabled = (wpsEnabled == 0) ? FALSE : TRUE;

    sprintf(recName, WpsPushButton, wlanIndex+1);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS)
    {
        pCfg->WpsPushButton = atoi(strValue);
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    } else {
        pCfg->WpsPushButton = 1;  // Use as default value
    }

    pCfg->ConfigMethodsEnabled = 0;
    wifi_getWpsConfigMethodsEnabled(wlanIndex,methodsEnabled);
    if (strstr(methodsEnabled,"PushButton") != NULL) {
        pCfg->ConfigMethodsEnabled |= COSA_DML_WIFI_WPS_METHOD_PushButton;
    } 
    if (strstr(methodsEnabled,"Keypad") != NULL) {
        pCfg->ConfigMethodsEnabled |= COSA_DML_WIFI_WPS_METHOD_Pin;
    } 
    
    /* USGv2 Extensions */
    // These may be write only parameters
    memset(pCfg->X_CISCO_COM_ClientPin, 0, sizeof(pCfg->X_CISCO_COM_ClientPin));
    pCfg->X_CISCO_COM_ActivatePushButton = FALSE;
    pCfg->X_CISCO_COM_CancelSession = FALSE;
    
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiApWpsSetInfo
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APWPS_INFO    pInfo
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    int wlanIndex;
wifiDbgPrintf("%s\n",__FUNCTION__);

    if (!pInfo)
    {
        return ANSC_STATUS_FAILURE;
    }

    wifi_getIndexFromName(pSsid, &wlanIndex);
    if (wlanIndex == -1) 
    {
        // Error could not find index
        return ANSC_STATUS_FAILURE;
    }

    // Read Only parameter
    // pInfo->ConfigMethodsSupported

    unsigned int pin = _ansc_atoi(pInfo->X_CISCO_COM_Pin);
    wifi_setWpsDevicePIN(wlanIndex, pin);

    // Already set WPS enabled in WpsSetCfg, but 
    //   if config==TRUE set again to configured(2).
    if ( pInfo->X_Comcast_com_Configured == TRUE ) {
        wifi_setWpsEnable(wlanIndex, 2);
    }

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiApWpsGetInfo
(
ANSC_HANDLE                 hContext,
char*                       pSsid,
PCOSA_DML_WIFI_APWPS_INFO   pInfo
)
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    int wlanIndex;
    char  configState[32];
    unsigned int pin;

    wifiDbgPrintf("%s\n",__FUNCTION__);

    if (!pInfo)
    {
        return ANSC_STATUS_FAILURE;
    }

    wifi_getIndexFromName(pSsid, &wlanIndex);
    if (wlanIndex == -1) 
    {
        // Error could not find index
        return ANSC_STATUS_FAILURE;
    }

    pInfo->ConfigMethodsSupported = COSA_DML_WIFI_WPS_METHOD_PushButton | COSA_DML_WIFI_WPS_METHOD_Pin;

    wifi_getWpsDevicePIN(wlanIndex, &pin);
    sprintf(pInfo->X_CISCO_COM_Pin, "%d", pin);

    wifi_getWpsConfigurationState(wlanIndex, configState);
    if (strstr(configState,"Not configured") != NULL) {
        pInfo->X_Comcast_com_Configured = FALSE;
    } else {
        pInfo->X_Comcast_com_Configured = TRUE;
    }
    
    return ANSC_STATUS_SUCCESS;
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
// wifiDbgPrintf("%s SSID %s\n",__FUNCTION__, pSsid);
    PCOSA_DML_WIFI_AP_ASSOC_DEVICE  AssocDeviceArray  = (PCOSA_DML_WIFI_AP_ASSOC_DEVICE)NULL;
    ULONG                           index             = 0;
    ULONG                           ulCount           = 0;
    int wlanIndex;
    BOOL enabled; 
    wifi_device_t **wlanDevice;

    *pulCount = 0;

    wifi_getIndexFromName(pSsid, &wlanIndex);
    if (wlanIndex == -1) 
    {
	// Error could not find index
	return NULL;
    }

    wifi_getApEnable(wlanIndex, &enabled);
    // Nothing to do if VAP is not enabled
    if (enabled == FALSE) {
        return NULL; 
    }

    wifi_getAllAssociatedDeviceDetail(wlanIndex, (UINT *)pulCount, &wlanDevice);
    if (*pulCount > 0) {
        AssocDeviceArray = (PCOSA_DML_WIFI_AP_ASSOC_DEVICE)AnscAllocateMemory(sizeof(COSA_DML_WIFI_AP_ASSOC_DEVICE)*(*pulCount));

        if ( !AssocDeviceArray ) {
            *pulCount = 0;
            return NULL;
        }

        for (index = 0; index < *pulCount; index++) { 
            AssocDeviceArray[index].MacAddress[0] = wlanDevice[index]->wifi_devMacAddress[0];
            AssocDeviceArray[index].MacAddress[1] = wlanDevice[index]->wifi_devMacAddress[1];
            AssocDeviceArray[index].MacAddress[2] = wlanDevice[index]->wifi_devMacAddress[2];
            AssocDeviceArray[index].MacAddress[3] = wlanDevice[index]->wifi_devMacAddress[3];
            AssocDeviceArray[index].MacAddress[4] = wlanDevice[index]->wifi_devMacAddress[4];
            AssocDeviceArray[index].MacAddress[5] = wlanDevice[index]->wifi_devMacAddress[5];
            AssocDeviceArray[index].AuthenticationState  = (wlanDevice[index]->wifi_devAssociatedDeviceAuthentiationState == 1) ? TRUE : FALSE;

            AssocDeviceArray[index].LastDataDownlinkRate = wlanDevice[index]->wifi_devTxRate;
            AssocDeviceArray[index].LastDataUplinkRate   = wlanDevice[index]->wifi_devRxRate;
            AssocDeviceArray[index].SignalStrength       = wlanDevice[index]->wifi_devSignalStrength;
            AssocDeviceArray[index].Retransmissions      = 0; // not implemented
            AssocDeviceArray[index].Active               = TRUE;
        }
    }

    return AssocDeviceArray;
}

/* Description:
 *	This routine is to retrieve the complete list of currently associated WiFi devices
 *	and kick them to force them to disassociate.
 * Arguments:
 *	pName   		Indicate which SSID to operate on.
 * Return:
 * Status
 */
ANSC_STATUS
CosaDmlWiFiApKickAssocDevices
    (
        char*                       pSsid
    )
{
wifiDbgPrintf("%s SSID %s\n",__FUNCTION__, pSsid);

    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    ULONG                           index             = 0;
    ULONG                           ulCount           = 0;
    
    /*For example we have 5 AssocDevices*/
    int wlanIndex;

    wifi_getIndexFromName(pSsid, &wlanIndex);
    if (wlanIndex == -1) 
    {
	// Error could not find index
        return ANSC_STATUS_FAILURE;
    }

    ulCount = 0;
    wifi_getNumDevicesAssociated(wlanIndex, (UINT *)&ulCount);
    if (ulCount > 0)
    {
	for (index = 0; index < ulCount; index++)
	{ 
	    wifi_device_t wlanDevice;

	    wifi_getAssociatedDeviceDetail(wlanIndex, index+1, &wlanDevice);
	    wifi_kickAssociatedDevice(wlanIndex, &wlanDevice);
	}
    }

    return returnStatus;
}

ANSC_STATUS
CosaDmlWiFiApMfGetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_AP_MF_CFG    pCfg
    )
{
wifiDbgPrintf("%s\n",__FUNCTION__);
    // R3 requirement 
    int mode = 0;
    int wlanIndex;
    char recName[256];
    int retPsmGet = CCSP_SUCCESS;
    char *strValue = NULL;

    wifi_getIndexFromName(pSsid, &wlanIndex);
    if (wlanIndex == -1) 
    {
        // Error could not find index
        return ANSC_STATUS_FAILURE;
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilterMode, wlanIndex+1);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        mode = _ansc_atoi(strValue);
        wifi_setMacAddressControlMode(wlanIndex,mode);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    if (mode == 0) 
    {
        pCfg->bEnabled = FALSE;
        pCfg->FilterAsBlackList = FALSE;
    } else if (mode == 1) {
        pCfg->bEnabled = TRUE;
        pCfg->FilterAsBlackList = FALSE;
    } else if (mode == 2) {
        pCfg->bEnabled = TRUE;
        pCfg->FilterAsBlackList = TRUE;
    }

    sWiFiDmlApMfCfg[wlanIndex] = pCfg;
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
wifiDbgPrintf("%s\n",__FUNCTION__);
    int mode;
    int wlanIndex;
    char recName[256];
    int retPsmSet = CCSP_SUCCESS;

    wifi_getIndexFromName(pSsid, &wlanIndex);
    if (wlanIndex == -1) 
    {
        // Error could not find index
        return ANSC_STATUS_FAILURE;
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilterMode, wlanIndex+1);

    if ( pCfg->bEnabled == FALSE )
    {
	wifi_setMacAddressControlMode(wlanIndex, 0);
        PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "0");
    } else if ( pCfg->FilterAsBlackList == FALSE ) {
	wifi_setMacAddressControlMode(wlanIndex, 1);
        PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "1");
    } else if ( pCfg->FilterAsBlackList == TRUE ) {
	wifi_setMacAddressControlMode(wlanIndex, 2);
        PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "2");
    }

    if ( pCfg->bEnabled == TRUE ) {
        wifi_kickAclAssociatedDevices(wlanIndex, pCfg->FilterAsBlackList);
    }

    // Notify WiFiExtender that MacFilter has changed
    {
        CosaDml_NotifyWiFiExt(COSA_WIFIEXT_DM_UPDATE_SSID);
    }

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiApMfPushCfg
(
PCOSA_DML_WIFI_AP_MF_CFG    pCfg,
ULONG                                           wlanIndex
)
{
    wifiDbgPrintf("%s\n",__FUNCTION__);

    if ( pCfg->bEnabled == FALSE ) {
        wifi_setMacAddressControlMode(wlanIndex, 0);
    } else if ( pCfg->FilterAsBlackList == FALSE ) {
        wifi_setMacAddressControlMode(wlanIndex, 1);
    } else if ( pCfg->FilterAsBlackList == TRUE ) {
        wifi_setMacAddressControlMode(wlanIndex, 2);
    }

    return ANSC_STATUS_SUCCESS;
    }

ANSC_STATUS
CosaDmlWiFiApSetMFQueue
    (
        QUEUE_HEADER *mfQueue,
        ULONG                  apIns
    )
{
wifiDbgPrintf("%s apIns = %d \n",__FUNCTION__, apIns );
    sWiFiDmlApMfQueue[apIns-1] = mfQueue;
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_GetWEPKey64ByIndex(ULONG apIns, ULONG keyIdx, PCOSA_DML_WEPKEY_64BIT pWepKey)
{
wifiDbgPrintf("%s apIns = %d, keyIdx = %d\n",__FUNCTION__, apIns, keyIdx);
    wifi_getWepKey(apIns-1, keyIdx+1, pWepKey->WEPKey);
    
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_SetWEPKey64ByIndex(ULONG apIns, ULONG keyIdx, PCOSA_DML_WEPKEY_64BIT pWepKey)
{
wifiDbgPrintf("%s apIns = %d, keyIdx = %d\n",__FUNCTION__, apIns, keyIdx);

    // for downgrade compatibility set both index 0 & 1 for keyIdx 1 
    // Comcast 1.3 release uses keyIdx 0 maps to driver index 0, this is used by driver and the on GUI
    // all four keys are set the same.  If a box is downgraded from 1.6 to 1.3 and the required WEP key will be set
   if (keyIdx == 0)
   {
       wifi_setWepKey(apIns-1, keyIdx, pWepKey->WEPKey);
   }
    
    wifi_setWepKey(apIns-1, keyIdx+1, pWepKey->WEPKey);
    sWiFiDmlWepChg[apIns-1] = TRUE;

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_GetWEPKey128ByIndex(ULONG apIns, ULONG keyIdx, PCOSA_DML_WEPKEY_128BIT pWepKey)
{
wifiDbgPrintf("%s apIns = %d, keyIdx = %d\n",__FUNCTION__, apIns, keyIdx);
    wifi_getWepKey(apIns-1, keyIdx+1, pWepKey->WEPKey);

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_SetWEPKey128ByIndex(ULONG apIns, ULONG keyIdx, PCOSA_DML_WEPKEY_128BIT pWepKey)
{
wifiDbgPrintf("%s apIns = %d, keyIdx = %d\n",__FUNCTION__, apIns, keyIdx);

    // for downgrade compatibility set both index 0 & 1 for keyIdx 1 
    // Comcast 1.3 release uses keyIdx 0 maps to driver index 0, this is used by driver and the on GUI
    // all four keys are set the same.  If a box is downgraded from 1.6 to 1.3 and the required WEP key will be set
   if (keyIdx == 0)
   {
       wifi_setWepKey(apIns-1, keyIdx, pWepKey->WEPKey);
   }
    
    wifi_setWepKey(apIns-1, keyIdx+1, pWepKey->WEPKey);
    sWiFiDmlWepChg[apIns-1] = TRUE;

    return ANSC_STATUS_SUCCESS;
}

#define MAX_MAC_FILT                16

static int                          g_macFiltCnt[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
static COSA_DML_WIFI_AP_MAC_FILTER  g_macFiltTab[MAX_MAC_FILT]; // = { { 1, "MacFilterTable1", "00:1a:2b:aa:bb:cc" }, };

ULONG
CosaDmlMacFilt_GetNumberOfEntries(ULONG apIns)
{
    char *strValue = NULL;
    char recName[256];
    int retPsmGet = CCSP_SUCCESS;
    int retPsmSet = CCSP_SUCCESS;
    BOOL enabled; 
wifiDbgPrintf("%s apIns = %d\n",__FUNCTION__, apIns);

    g_macFiltCnt[apIns-1] = 0;

    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilterList, apIns);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        int numFilters = 0;
        int macInstance = 0;
        char *mac;

        if (strlen(strValue) > 0) {
            char *start = strValue;
            char *end = NULL;

            end = strValue + strlen(strValue);

            if ((end = strstr(strValue, ":" ))) {
                *end = 0;

                g_macFiltCnt[apIns-1] = _ansc_atoi(start);
            }
        }
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    } else  if (retPsmGet == CCSP_CR_ERR_INVALID_PARAM){
        char *macFilter = "0:"; 

        g_macFiltCnt[apIns-1] = 0;

        // Init to empty list on factory fresh
        retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, macFilter);
        if (retPsmSet != CCSP_SUCCESS) {
            wifiDbgPrintf("%s PSM_Set_Record_Value2 returned error %d while setting recName %s\n",__FUNCTION__, retPsmSet, recName);
    }
    }

wifiDbgPrintf("%s apIns = %d g_macFiltCnt = %d\n",__FUNCTION__, apIns, g_macFiltCnt[apIns-1]);
    return g_macFiltCnt[apIns-1];
}

ANSC_STATUS
CosaDmlMacFilt_GetMacInstanceNumber(ULONG apIns, ULONG index, PCOSA_DML_WIFI_AP_MAC_FILTER pMacFilt)
{
wifiDbgPrintf("%s\n",__FUNCTION__);
    char recName[256];
    char *strValue;
    char *macFilterList;
    int retPsmGet = CCSP_SUCCESS;
    int i = 0;

    pMacFilt->InstanceNumber = 0;

    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilterList, apIns);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
	if ((macFilterList = strstr(strValue, ":" ))) {
	    macFilterList += 1;
	    while (i < index) {
                i++;
                if ((macFilterList = strstr(macFilterList,","))) {
                    macFilterList += 1;
                }
            } 
            if (i == index && macFilterList != NULL) {
                pMacFilt->InstanceNumber = _ansc_atoi(macFilterList);
            } 
        }
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    } else {
        return ANSC_STATUS_FAILURE;
    }
    return ANSC_STATUS_SUCCESS;
}


ANSC_STATUS
CosaDmlMacFilt_GetEntryByIndex(ULONG apIns, ULONG index, PCOSA_DML_WIFI_AP_MAC_FILTER pMacFilt)
{
wifiDbgPrintf("%s\n",__FUNCTION__);
    char recName[256];
    char *devName;
    char *devMac;
    int retPsmGet = CCSP_SUCCESS;

    if (index >= g_macFiltCnt[apIns-1])
        return ANSC_STATUS_FAILURE;

    if ( CosaDmlMacFilt_GetMacInstanceNumber(apIns, index, pMacFilt) == ANSC_STATUS_FAILURE)
    {
        return ANSC_STATUS_FAILURE;
    }

    sprintf(pMacFilt->Alias,"MacFilter%d", pMacFilt->InstanceNumber);

    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilter, apIns, pMacFilt->InstanceNumber);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &devMac);
    if (retPsmGet == CCSP_SUCCESS) {
	sprintf(pMacFilt->MACAddress,"%s",devMac);
        wifi_addAclDevice(apIns-1,devMac);
        wifiDbgPrintf("%s called wifi_addAclDevice index = %d mac %s \n",__FUNCTION__, apIns-1,devMac);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(devMac);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilterDevice, apIns, pMacFilt->InstanceNumber);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &devName);
    if (retPsmGet == CCSP_SUCCESS) {
	sprintf(pMacFilt->DeviceName,"%s",devName);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(devName);
    }

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlMacFilt_SetValues(ULONG apIns, ULONG index, ULONG ins, char *Alias)
{
    int i;
wifiDbgPrintf("%s\n",__FUNCTION__);

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlMacFilt_AddEntry(ULONG apIns, PCOSA_DML_WIFI_AP_MAC_FILTER pMacFilt)
{
wifiDbgPrintf("%s\n",__FUNCTION__);
    char recName[256];
    char macFilterList[256];
    int retPsmGet = CCSP_SUCCESS;
    int retPsmSet = CCSP_SUCCESS;
    char *strValue = NULL;

    if (g_macFiltCnt[apIns-1] >= MAX_MAC_FILT)
        return ANSC_STATUS_FAILURE;


    int rc = wifi_addAclDevice(apIns-1,pMacFilt->MACAddress);
    if (rc != 0)
    {
	wifiDbgPrintf("%s apIns = %d wifi_addAclDevice failed for %s\n",__FUNCTION__, apIns, pMacFilt->MACAddress);
        return ANSC_STATUS_FAILURE;
    }

    wifi_getAclDeviceNum(apIns-1, &g_macFiltCnt[apIns-1]);
    sprintf(pMacFilt->Alias,"MacFilter%d", pMacFilt->InstanceNumber);

    // Add Mac to Non-Vol PSM
    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilter, apIns, pMacFilt->InstanceNumber);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, pMacFilt->MACAddress);
    if (retPsmSet != CCSP_SUCCESS) {
	wifiDbgPrintf("%s Error %d adding mac = %s \n", __FUNCTION__, retPsmSet, pMacFilt->MACAddress);
        return ANSC_STATUS_FAILURE;
    }

    // Add Mac to Non-Vol PSM
    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilterDevice, apIns, pMacFilt->InstanceNumber);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, pMacFilt->DeviceName);
    if (retPsmSet != CCSP_SUCCESS) {
	wifiDbgPrintf("%s Error %d adding mac device name = %s \n", __FUNCTION__, retPsmSet, pMacFilt->DeviceName);
        return ANSC_STATUS_FAILURE;
    }

    memset(macFilterList, 0, sizeof(macFilterList));
    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilterList, apIns);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        int numFilters = 0;
        int macInstance = 0;
        char *mac;
        char *start = strValue;
        char *macs = NULL;

        if (strlen(start) > 0) {
	    numFilters = _ansc_atoi(start);

            if (numFilters == 0) {
                sprintf(macFilterList,"%d:%d", numFilters+1,pMacFilt->InstanceNumber);
            } else {
               if ((macs = strstr(strValue, ":" ))) {
                   sprintf(macFilterList,"%d%s,%d", numFilters+1,macs,pMacFilt->InstanceNumber);
               }  else {
		   // else illformed string.  If there are instances should have a :
                   sprintf(macFilterList,"%d:%d", numFilters+1,pMacFilt->InstanceNumber);
               }
            }
        }
        
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    } else {
	sprintf(macFilterList,"1:%d",pMacFilt->InstanceNumber);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilterList, apIns);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, macFilterList);
    if (retPsmSet != CCSP_SUCCESS) {
	wifiDbgPrintf("%s PSM error adding MacFilterList  mac %d \n", __FUNCTION__, retPsmSet);
	return ANSC_STATUS_FAILURE;
    }

    // Notify WiFiExtender that MacFilter has changed
    {
        CosaDml_NotifyWiFiExt(COSA_WIFIEXT_DM_UPDATE_SSID);
    }

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlMacFilt_DelEntry(ULONG apIns, ULONG macFiltIns)
{
wifiDbgPrintf("%s apIns = %d macFiltIns = %d g_macFiltCnt = %d\n",__FUNCTION__, apIns, macFiltIns, g_macFiltCnt[apIns-1]);
    char recName[256];
    int retPsmGet = CCSP_SUCCESS;
    int retPsmSet = CCSP_SUCCESS;
    char *macAddress = NULL;
    char *macFilterList = NULL;
    COSA_DML_WIFI_AP_MAC_FILTER macFilt;

    // Add Mac to Non-Vol PSM
    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilter, apIns, macFiltIns);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &macAddress);
    if (retPsmGet == CCSP_SUCCESS) {
	wifi_delAclDevice(apIns-1,macAddress);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(macAddress);

	g_macFiltCnt[apIns-1]--;

        memset(recName, 0, sizeof(recName));
	sprintf(recName, MacFilter, apIns, macFiltIns);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

        memset(recName, 0, sizeof(recName));
	sprintf(recName, MacFilterDevice, apIns, macFiltIns);
	PSM_Del_Record(bus_handle,g_Subsystem, recName);

        // Remove from MacFilterList
	memset(recName, 0, sizeof(recName));
	sprintf(recName, MacFilterList, apIns);
	retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &macFilterList);
	if (retPsmGet == CCSP_SUCCESS) {
            char inst[32];
            char *mac = NULL;
            char *macs = NULL;
            char *nextMac = NULL;
            char *prev = NULL;

	    macs = strstr(macFilterList,":");
            if (macs) {
		sprintf(inst,"%d",macFiltIns);
		mac = strstr(macs,inst);
            }
            if (mac) {
                // Not the only or last mac in list
                if ((nextMac=strstr(mac,","))) {
                    nextMac += 1;
                    sprintf(mac,"%s",nextMac);
                } else {
                    prev = mac - 1;
                    if (strstr(prev,":")) {
			*mac = 0;
                    } else {
			*prev = 0;
                    }
                }

                mac=strstr(macFilterList,":");
                if (mac) {
                    char newMacList[256];
		    sprintf(newMacList,"%d%s",g_macFiltCnt[apIns-1],mac);
	            retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, newMacList);
		    if (retPsmSet != CCSP_SUCCESS) {
			wifiDbgPrintf("%s PSM error %d while setting MacFilterList %s \n", __FUNCTION__, retPsmSet, newMacList);
			return ANSC_STATUS_FAILURE;
		    }
                }
            }
        }
        
    } else {
        return ANSC_STATUS_FAILURE;
    }

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlMacFilt_GetConf(ULONG apIns, ULONG macFiltIns, PCOSA_DML_WIFI_AP_MAC_FILTER pMacFilt)
{
wifiDbgPrintf("%s\n",__FUNCTION__);

    CosaDmlMacFilt_GetEntryByIndex(apIns, macFiltIns, pMacFilt);

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlMacFilt_SetConf(ULONG apIns, ULONG macFiltIns, PCOSA_DML_WIFI_AP_MAC_FILTER pMacFilt)
{
wifiDbgPrintf("%s\n",__FUNCTION__);
    char recName[256];
    int retPsmSet = CCSP_SUCCESS;

    // Add Mac to Non-Vol PSM
    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilter, apIns, pMacFilt->InstanceNumber);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, pMacFilt->MACAddress);
    if (retPsmSet != CCSP_SUCCESS) {
	wifiDbgPrintf("%s Error %d adding mac = %s \n", __FUNCTION__, retPsmSet, pMacFilt->MACAddress);
        return ANSC_STATUS_FAILURE;
    }

    // Add Mac Device Name to Non-Vol PSM
    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilterDevice, apIns, pMacFilt->InstanceNumber);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, pMacFilt->DeviceName);
    if (retPsmSet != CCSP_SUCCESS) {
	wifiDbgPrintf("%s Error %d adding mac device name = %s \n", __FUNCTION__, retPsmSet, pMacFilt->DeviceName);
        return ANSC_STATUS_FAILURE;
    }

    return ANSC_STATUS_SUCCESS;
}

#ifndef NELEMS
#define NELEMS(arr)         (sizeof(arr) / sizeof((arr)[0]))
#endif 

/*
 * @buf [OUT], buffer to save config file
 * @size [IN-OUT], buffer size as input and config file size as output
 */
ANSC_STATUS 
CosaDmlWiFi_GetConfigFile(void *buf, int *size)
{
    const char *wifi_cfgs[] = {
        "/nvram/etc/ath/.configData",
    };
    struct pack_hdr *hdr;

    if (!buf || !size) {
        wifiDbgPrintf("%s: bad parameter\n", __FUNCTION__);
        return ANSC_STATUS_FAILURE;
    }

    if ((hdr = pack_files(wifi_cfgs, NELEMS(wifi_cfgs))) == NULL) {
        wifiDbgPrintf("%s: pack_files error\n", __FUNCTION__);
        return ANSC_STATUS_FAILURE;
    }

    dump_pack_hdr(hdr);

    if (*size < hdr->totsize) {
        wifiDbgPrintf("%s: buffer too small: %d, need %d\n", __FUNCTION__, *size, hdr->totsize);
        return ANSC_STATUS_FAILURE;
    }

    *size = hdr->totsize;
    memcpy(buf, hdr, hdr->totsize);
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS 
CosaDmlWiFi_SetConfigFile(const void *buf, int size)
{
    const struct pack_hdr *hdr = buf;

    if (!buf || size != hdr->totsize) {
        wifiDbgPrintf("%s: bad parameter\n", __FUNCTION__);
        return ANSC_STATUS_FAILURE;
    }

    wifiDbgPrintf("%s: Remove Generated files\n", __FUNCTION__);
    system("rm -rf /nvram/etc/wpa2/WSC_ath*");
    system("rm -rf /tmp/secath*");

    wifiDbgPrintf("%s: unpack files\n", __FUNCTION__);
    if (unpack_files(hdr) != 0) {
        wifiDbgPrintf("%s: unpack_files error\n", __FUNCTION__);
        return ANSC_STATUS_FAILURE;
    }

    return ANSC_STATUS_SUCCESS;
}

#endif
