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
#include "cosa_dbus_api.h"
#include "cosa_wifi_apis.h"
#include "cosa_wifi_sbapi_custom.h"
#include "cosa_wifi_internal.h"
#include "plugin_main_apis.h"
#include "wifi_hal.h"
#include "ccsp_psm_helper.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "ansc_platform.h"
#include "pack_file.h"
#include "ccsp_WifiLog_wrapper.h"
#include <sysevent/sysevent.h>

#define WLAN_MAX_LINE_SIZE 1024
#define RADIO_BROADCAST_FILE "/tmp/broadcast_ssids"
#if defined(_COSA_BCM_MIPS)
#define WLAN_WAIT_LIMIT 3
#endif
#ifdef USE_NOTIFY_COMPONENT
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/un.h>
#endif
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
BOOL gRadioRestartRequest[2]={FALSE,FALSE};



#ifndef __user
#define __user
#endif

INT CosaDmlWiFi_AssociatedDevice_callback(INT apIndex, wifi_associated_dev_t *associated_dev);
int sMac_to_cMac(char *sMac, unsigned char *cMac);
INT m_wifi_init();
ANSC_STATUS CosaDmlWiFi_startDCSScanThread(void);
/**************************************************************************
*
*	Function Definitions
*
**************************************************************************/
int syscfg_executecmd(const char *caller, char *cmd, char **retBuf) 
{
  FILE *f;
  char *ptr = NULL;
  size_t buff_size = 0;  // current memory in-use size
  *retBuf = NULL;


  if((f = popen(cmd, "r")) == NULL) {
    printf("%s: popen %s error\n",caller, cmd);
    return -1;
  }

  while(!feof(f))
  {
    // allocate memory to allow for one more line:
    if((ptr = realloc(*retBuf, buff_size + WLAN_MAX_LINE_SIZE)) == NULL)
    {
      printf("%s: realloc %s error\n",caller, cmd);
      // Note: caller still needs to free retBuf
      pclose(f);
      return -1;
    }

    *retBuf=ptr;        // update retBuf
    ptr+=buff_size;     // ptr points to current end of string

    *ptr = 0;
    fgets(ptr,WLAN_MAX_LINE_SIZE,f);

    if(strlen(ptr) == 0)
    {
      break;
    }
    buff_size += strlen(ptr);  // update current memory in-use
  }
  pclose(f);

  return 0;
}

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
    if (!pEntry)
    {
        return 0;
    }
    
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

		gChannelSwitchingCount = 0;
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

    if (!pEntry)
    {
        return ANSC_STATUS_FAILURE
    }
    
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

    if (apIns != 1 || !pWepKey) /* only support 1 ap in simu */
        return ANSC_STATUS_FAILURE;

    snprintf((char*)pWepKey->WEPKey, sizeof(pWepKey->WEPKey), "%s", wepKey64[keyIdx]);
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_SetWEPKey64ByIndex(ULONG apIns, ULONG keyIdx, PCOSA_DML_WEPKEY_64BIT pWepKey)
{
    /* dummy data for simu */
    static char wepKey64[COSA_DML_WEP_KEY_NUM][64 / 8 * 2 + 1];

    if (apIns != 1 || !pWepKey) /* only support 1 ap in simu */
        return ANSC_STATUS_FAILURE;

    snprintf(wepKey64[keyIdx], sizeof(wepKey64[keyIdx]), "%s", pWepKey->WEPKey);
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_GetWEPKey128ByIndex(ULONG apIns, ULONG keyIdx, PCOSA_DML_WEPKEY_128BIT pWepKey)
{
    /* dummy data for simu */
    static char wepKey128[COSA_DML_WEP_KEY_NUM][128 / 8 * 2 + 1];

    if (apIns != 1 || !pWepKey) /* only support 1 ap in simu */
        return ANSC_STATUS_FAILURE;

    snprintf((char *)pWepKey->WEPKey, sizeof(pWepKey->WEPKey), "%s", wepKey128[keyIdx]);
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_SetWEPKey128ByIndex(ULONG apIns, ULONG keyIdx, PCOSA_DML_WEPKEY_128BIT pWepKey)
{
    /* dummy data for simu */
    static char wepKey128[COSA_DML_WEP_KEY_NUM][128 / 8 * 2 + 1];

    if (apIns != 1 || !pWepKey) /* only support 1 ap in simu */
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
    if (apIns != 1 || !pMacFilt) /* only support 1 ap in simu */
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

    if (apIns != 1 || !Alias) /* only support 1 ap in simu */
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
    if (apIns != 1 || !pMacFilt) /* only support 1 ap in simu */
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

    if (apIns != 1 || !pMacFilt) /* only support 1 ap in simu */
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

    if (apIns != 1 || !pMacFilt) /* only support 1 ap in simu */
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
    if (!pEntry)
    {
        return ANSC_STATUS_FAILURE;
    }
    
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
    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
    
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

    if (!pAlias)
    {
        return ANSC_STATUS_FAILURE;
    }

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

    if (!pEntry)
    {
        return ANSC_STATUS_FAILURE;
    }

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

    if (!pAlias)
    {
        return ANSC_STATUS_FAILURE;
    }

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
    if (!pEntry)
    {
        return ANSC_STATUS_FAILURE;
    }
    
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
    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
    
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

#elif defined(_COSA_INTEL_USG_ATOM_) || defined(_COSA_BCM_MIPS_) || defined(_COSA_BCM_ARM_)

#include <pthread.h>
pthread_mutex_t sWiFiThreadMutex = PTHREAD_MUTEX_INITIALIZER;

// #define wifiDbgPrintf 
#define wifiDbgPrintf printf

#define WIFI_INDEX_MAX 16
#define RADIO_INDEX_MAX 2

static int gRadioCount = 2;
/* WiFi SSID */
static int gSsidCount = 16;
static int gApCount = 16;

static  COSA_DML_WIFI_RADIO_CFG sWiFiDmlRadioStoredCfg[2];
static  COSA_DML_WIFI_RADIO_CFG sWiFiDmlRadioRunningCfg[2];
static COSA_DML_WIFI_SSID_CFG sWiFiDmlSsidStoredCfg[WIFI_INDEX_MAX];
static COSA_DML_WIFI_SSID_CFG sWiFiDmlSsidRunningCfg[WIFI_INDEX_MAX];
static COSA_DML_WIFI_AP_FULL sWiFiDmlApStoredCfg[WIFI_INDEX_MAX];
static COSA_DML_WIFI_AP_FULL sWiFiDmlApRunningCfg[WIFI_INDEX_MAX];
static COSA_DML_WIFI_APSEC_FULL  sWiFiDmlApSecurityStored[WIFI_INDEX_MAX];
static COSA_DML_WIFI_APSEC_FULL  sWiFiDmlApSecurityRunning[WIFI_INDEX_MAX];
static COSA_DML_WIFI_APWPS_FULL sWiFiDmlApWpsStored[WIFI_INDEX_MAX];
static COSA_DML_WIFI_APWPS_FULL sWiFiDmlApWpsRunning[WIFI_INDEX_MAX];
static PCOSA_DML_WIFI_AP_MF_CFG  sWiFiDmlApMfCfg[WIFI_INDEX_MAX];
static BOOLEAN sWiFiDmlRestartHostapd = FALSE;
static QUEUE_HEADER *sWiFiDmlApMfQueue[WIFI_INDEX_MAX];
static BOOLEAN sWiFiDmlWepChg[WIFI_INDEX_MAX] = { FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE };
static BOOLEAN sWiFiDmlAffectedVap[WIFI_INDEX_MAX] = { FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE };
static BOOLEAN sWiFiDmlPushWepKeys[WIFI_INDEX_MAX] = { FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE };
static BOOLEAN sWiFiDmlUpdateVlanCfg[WIFI_INDEX_MAX] = { FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE };
static BOOLEAN sWiFiDmlUpdatedAdvertisement[WIFI_INDEX_MAX] = { FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE,FALSE };
static ULONG sWiFiDmlRadioLastStatPoll[WIFI_INDEX_MAX] = { 0, 0 };
static ULONG sWiFiDmlSsidLastStatPoll[WIFI_INDEX_MAX] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; 
static COSA_DML_WIFI_BANDSTEERING_SETTINGS sWiFiDmlBandSteeringStoredSettinngs[2];

extern ANSC_HANDLE bus_handle;
extern char        g_Subsystem[32];
extern PCOSA_BACKEND_MANAGER_OBJECT g_pCosaBEManager;
static PCOSA_DATAMODEL_WIFI            pMyObject = NULL;
//static COSA_DML_WIFI_SSID_SINFO   gCachedSsidInfo[16];

static PCOSA_DML_WIFI_SSID_BRIDGE  pBridgeVlanCfg = NULL;

static char *FactoryReset    	= "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.FactoryReset";
static char *FactoryResetSSID    	= "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.Radio.%d.FactoryResetSSID";
static char *ValidateSSIDName        = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.ValidateSSIDName";
static char *FixedWmmParams        = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.FixedWmmParamsValues";
static char *SsidUpgradeRequired = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.SsidUpgradeRequired";

static char *MeasuringRateRd        = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.Radio.%d.Stats.X_COMCAST-COM_RadioStatisticsMeasuringRate";
static char *MeasuringIntervalRd = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.Radio.%d.Stats.X_COMCAST-COM_RadioStatisticsMeasuringInterval";

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

static char *BeaconRateCtl   = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.Radio.AccessPoint.%d.BeaconRateCtl";

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

static char *PreferPrivate    	= "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.PreferPrivate";
static char *PreferPrivate_configured    	= "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.PreferPrivateConfigure";

#define WIFIEXT_DM_OBJ           "Device.MoCA."
#define WIFIEXT_DM_RADIO_UPDATE  "Device.MoCA.X_CISCO_COM_WiFi_Extender.X_CISCO_COM_Radio_Updated"
#define WIFIEXT_DM_WPS_UPDATE    "Device.MoCA.X_CISCO_COM_WiFi_Extender.X_CISCO_COM_WPS_Updated"
#define WIFIEXT_DM_SSID_UPDATE   "Device.MoCA.X_CISCO_COM_WiFi_Extender.X_CISCO_COM_SSID_Updated"
#define INTERVAL 50000

#define WIFI_COMP				"eRT.com.cisco.spvtg.ccsp.wifi"
#define WIFI_BUS					"/com/cisco/spvtg/ccsp/wifi"
#define HOTSPOT_DEVICE_NAME	"AP_steering"
#define HOTSPOT_NO_OF_INDEX			4
static const int Hotspot_Index[HOTSPOT_NO_OF_INDEX]={5,6,9,10};

static BOOLEAN SSID1_Changed = FALSE ;
static BOOLEAN SSID2_Changed = FALSE ;
static BOOLEAN PASSPHRASE1_Changed = FALSE ;
static BOOLEAN PASSPHRASE2_Changed = FALSE ;
static BOOLEAN AdvEnable24 = TRUE ;
static BOOLEAN AdvEnable5 = TRUE ;

static char *SSID1 = "Device.WiFi.SSID.1.SSID" ;
static char *SSID2 = "Device.WiFi.SSID.2.SSID" ;

static char *PASSPHRASE1 = "Device.WiFi.AccessPoint.1.Security.X_COMCAST-COM_KeyPassphrase" ;
static char *PASSPHRASE2 = "Device.WiFi.AccessPoint.2.Security.X_COMCAST-COM_KeyPassphrase" ;
static char *NotifyWiFiChanges = "eRT.com.cisco.spvtg.ccsp.Device.WiFi.NotifyWiFiChanges" ;
static char *DiagnosticEnable = "eRT.com.cisco.spvtg.ccsp.Device.WiFi.NeighbouringDiagnosticEnable" ;


typedef enum {
   COSA_WIFIEXT_DM_UPDATE_RADIO = 0x1,
   COSA_WIFIEXT_DM_UPDATE_WPS   = 0x2,
   COSA_WIFIEXT_DM_UPDATE_SSID  = 0x4
} COSA_WIFIEXT_DM_UPDATE;

static ANSC_STATUS CosaDmlWiFiRadioSetTransmitPowerPercent ( int wlanIndex, int transmitPowerPercent);

static char *dstComp, *dstPath; /* cache */
static BOOL FindWifiDestComp(void);
static const int gWifiVlanCfgVersion = 2;

void configWifi(BOOLEAN redirect)
{
	char   dst_pathname_cr[64]  =  {0};
//	sleep(2);
        CCSP_MESSAGE_BUS_INFO *bus_info = (CCSP_MESSAGE_BUS_INFO *)bus_handle;
        componentStruct_t **        ppComponents = NULL;
	char* faultParam = NULL;
        int size =0;
	int ret;
	snprintf(dst_pathname_cr, sizeof(dst_pathname_cr), "%s%s", g_Subsystem, CCSP_DBUS_INTERFACE_CR);
	ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
               dst_pathname_cr,
               "Device.DeviceInfo.X_RDKCENTRAL-COM_ConfigureWiFi",
                g_Subsystem,        /* prefix */
                &ppComponents,
                &size);
	if ( ret != CCSP_SUCCESS )	
		return ;

	parameterValStruct_t    value = { "Device.DeviceInfo.X_RDKCENTRAL-COM_ConfigureWiFi", "true", ccsp_boolean};

	if ( !redirect )
	{
		value.parameterValue = "false";
	}
        ret = CcspBaseIf_setParameterValues
               (                        bus_handle,
                                        ppComponents[0]->componentName,
                                        ppComponents[0]->dbusPath,
                                        0, 0x0,   /* session id and write id */
                                        &value,
                                        1,
                                        TRUE,   /* no commit */
                                        &faultParam
                );

        if (ret != CCSP_SUCCESS && faultParam)
        {
	     CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s Failed to SetValue for param '%s' and ret val is %d\n",__FUNCTION__,faultParam,ret));
             printf("Error:Failed to SetValue for param '%s'\n", faultParam);
             bus_info->freefunc(faultParam);
        }
	 free_componentStruct_t(bus_handle, 1, ppComponents);

}

char SSID1_DEF[COSA_DML_WIFI_MAX_SSID_NAME_LEN],SSID2_DEF[COSA_DML_WIFI_MAX_SSID_NAME_LEN];
char PASSPHRASE1_DEF[65],PASSPHRASE2_DEF[65];

// Function gets the initial values of NeighbouringDiagnostic
ANSC_STATUS
CosaDmlWiFiNeighbouringGetEntry
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_NEIGHTBOURING_WIFI_DIAG_CFG   pEntry
    )
{

    if (!pEntry) return ANSC_STATUS_FAILURE;
	wifiDbgPrintf("%s\n",__FUNCTION__);
	CosaDmlGetNeighbouringDiagnosticEnable(&pEntry->bEnable);
	return ANSC_STATUS_SUCCESS;
}

// Function reads NeighbouringDiagnosticEnable value from PSM
void CosaDmlGetNeighbouringDiagnosticEnable(BOOLEAN *DiagEnable)
{
	wifiDbgPrintf("%s\n",__FUNCTION__);
	char* strValue = NULL;
	PSM_Get_Record_Value2(bus_handle,g_Subsystem, DiagnosticEnable, NULL, &strValue);
  	*DiagEnable = (atoi(strValue) == 1) ? TRUE : FALSE;
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
}

// Function sets NeighbouringDiagnosticEnable value to PSM
void CosaDmlSetNeighbouringDiagnosticEnable(BOOLEAN DiagEnableVal)
{
	char strValue[10];
	wifiDbgPrintf("%s\n",__FUNCTION__);
	memset(strValue,0,sizeof(strValue));
   	sprintf(strValue,"%d",DiagEnableVal);
        PSM_Set_Record_Value2(bus_handle,g_Subsystem, DiagnosticEnable, ccsp_string, strValue); 

}

void getDefaultSSID(int wlanIndex, char *DefaultSSID)
{
	char recName[256];
	char* strValue = NULL;
#if defined(_COSA_BCM_MIPS)
	int wlanWaitLimit = WLAN_WAIT_LIMIT;
#endif
    if (!DefaultSSID) return;
        memset(recName, 0, sizeof(recName));
        sprintf(recName, BssSsid, wlanIndex+1);
        printf("getDefaultSSID fetching %s\n", recName);
#if defined(_COSA_BCM_MIPS)
	// There seemed to be problem getting the SSID and passphrase.  Give it a multiple tries.
	while ( wlanWaitLimit-- && !strValue )
	{
		PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
		if (strValue != NULL)
		{
			strcpy(DefaultSSID,strValue);
		    ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
		}
		else
		{
			sleep(1);
		}
	}
#else
	PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (strValue != NULL)
    {
	    strcpy(DefaultSSID,strValue);
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }
#endif
}

void getDefaultPassphase(int wlanIndex, char *DefaultPassphrase)
{
	char recName[256];
	char* strValue = NULL;
#if defined(_COSA_BCM_MIPS)
	int wlanWaitLimit = WLAN_WAIT_LIMIT;
#endif
    if (!DefaultPassphrase) return;
        memset(recName, 0, sizeof(recName));
        sprintf(recName, Passphrase, wlanIndex+1);
        printf("getDefaultPassphrase fetching %s\n", recName);
#if defined(_COSA_BCM_MIPS)
	// There seemed to be problem getting the SSID and passphrase.  Give it a multiple tries.
	while ( wlanWaitLimit-- && !strValue )
	{
		PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
		if (strValue != NULL)
		{
		    strcpy(DefaultPassphrase,strValue);
		    ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
	  	}
	   	else
	   	{
			sleep(1);
	   	}
	}
#else
	PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (strValue != NULL)
    {
	    strcpy(DefaultPassphrase,strValue);
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }
#endif
}

#if defined(RDKLOGGER_SUPPORT_WIFI)

	void WriteWiFiLog(char *msg)
	{
	       char LogMsg_arr[512] = {0};     
		char *LogMsg = LogMsg_arr;      
		char LogLevel[512] = {0};       
		strcpy (LogLevel, msg);   
		strtok_r (LogLevel, ",",&LogMsg);        
		if( AnscEqualString(LogLevel, "RDK_LOG_ERROR", TRUE))   
		{        
		        CcspTraceError((LogMsg));        
		}        
		else if( AnscEqualString(LogLevel, "RDK_LOG_WARN", TRUE))        
		{        
		        CcspTraceWarning((LogMsg));      
		}        
		else if( AnscEqualString(LogLevel, "RDK_LOG_NOTICE", TRUE))      
		{        
		        CcspTraceNotice((LogMsg));       
		}        
		   else if( AnscEqualString(LogLevel, "RDK_LOG_INFO", TRUE))     
		{        
		         CcspTraceInfo((LogMsg));        
		}        
		else if( AnscEqualString(LogLevel, "RDK_LOG_DEBUG", TRUE))       
		{        
		        CcspTraceDebug((LogMsg));        
		}        
		else if( AnscEqualString(LogLevel, "RDK_LOG_FATAL", TRUE))       
		{        
		        CcspTraceCritical((LogMsg));     
		}        
		else     
		{        
		        CcspTraceInfo((LogMsg));         
		}
	}
#endif

void Captive_Portal_Check(void)
{
	if ( (SSID1_Changed) && (SSID2_Changed) && (PASSPHRASE1_Changed) && (PASSPHRASE2_Changed))
	{
		BOOLEAN redirect;
		redirect = FALSE;
  	    CcspWifiTrace(("RDK_LOG_WARN,CaptivePortal:%s - All four notification's received, Now start reverting redirection changes...\n",__FUNCTION__));
		printf("%s - All four notification's received, Now start reverting redirection changes...\n",__FUNCTION__);
		int retPsmSet;
	       	retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, NotifyWiFiChanges, ccsp_string,"false");
                if (retPsmSet == CCSP_SUCCESS) {
			CcspWifiTrace(("RDK_LOG_INFO,CaptivePortal:%s - PSM set of NotifyWiFiChanges success ...\n",__FUNCTION__));
		}
		else
		{
			CcspWifiTrace(("RDK_LOG_ERROR,CaptivePortal:%s - PSM set of NotifyWiFiChanges failed and ret value is %d...\n",__FUNCTION__,retPsmSet));
		}
		configWifi(redirect);	
		SSID1_Changed = FALSE;	
		SSID2_Changed = FALSE;
		PASSPHRASE1_Changed = FALSE;
		PASSPHRASE2_Changed = FALSE;
	}
}

void *RegisterWiFiConfigureCallBack(void *par)
{
    char *stringValue = NULL;
    int retPsmGet = CCSP_SUCCESS;
    int notify;
    notify = 1;

	
	pthread_detach(pthread_self());
   retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, NotifyWiFiChanges, NULL, &stringValue);

   CcspWifiTrace(("RDK_LOG_WARN,%s CaptivePortal: PSM get of NotifyChanges value is %s PSM get returned %d...\n",__FUNCTION__,stringValue,retPsmGet));

    if (AnscEqualString(stringValue, "true", TRUE))
    {
	sleep (15);

	char SSID1_CUR[COSA_DML_WIFI_MAX_SSID_NAME_LEN],SSID2_CUR[COSA_DML_WIFI_MAX_SSID_NAME_LEN];
	
	char PASSPHRASE1_CUR[65],PASSPHRASE2_CUR[65];

	memset(SSID1_DEF,0,sizeof(SSID1_DEF));
	memset(SSID1_CUR,0,sizeof(SSID1_CUR));

	memset(SSID2_DEF,0,sizeof(SSID2_DEF));
	memset(SSID2_CUR,0,sizeof(SSID2_CUR));

	memset(PASSPHRASE1_DEF,0,sizeof(PASSPHRASE1_DEF));
	memset(PASSPHRASE1_CUR,0,sizeof(PASSPHRASE1_CUR));

	memset(PASSPHRASE2_DEF,0,sizeof(PASSPHRASE2_DEF));
	memset(PASSPHRASE2_CUR,0,sizeof(PASSPHRASE2_CUR));

	int wlanIndex=0;
#if defined(_COSA_BCM_MIPS)
	int wlanWaitLimit = WLAN_WAIT_LIMIT;

	// There seemed to be problem getting the SSID and passphrase.  Give it a multiple tries.
	getDefaultSSID(wlanIndex,&SSID1_DEF);

	while ( wlanWaitLimit-- && !SSID1_CUR[0] )
	{
		wifi_getSSIDName(wlanIndex,&SSID1_CUR);
		if ( !SSID1_CUR[0] )
		{
			sleep(2);
		}
	}

	getDefaultPassphase(wlanIndex,&PASSPHRASE1_DEF);

	wlanWaitLimit = WLAN_WAIT_LIMIT;
	while ( wlanWaitLimit-- && !PASSPHRASE1_CUR[0] )
	{
		wifi_getApSecurityKeyPassphrase(wlanIndex,&PASSPHRASE1_CUR);
		if ( !PASSPHRASE1_CUR[0] )
		{
			sleep(2);
		}
	}

	wlanIndex=1;
	getDefaultSSID(wlanIndex,&SSID2_DEF);

	wlanWaitLimit = WLAN_WAIT_LIMIT;
	while ( wlanWaitLimit-- && !SSID2_CUR[0] )
	{
		wifi_getSSIDName(wlanIndex,&SSID2_CUR);
		if ( !SSID2_CUR[0] )
		{
			sleep(2);
		}
	}

	getDefaultPassphase(wlanIndex,&PASSPHRASE2_DEF);

	wlanWaitLimit = WLAN_WAIT_LIMIT;
	while ( wlanWaitLimit-- && !PASSPHRASE2_CUR[0] )
	{
		wifi_getApSecurityKeyPassphrase(wlanIndex,&PASSPHRASE2_CUR);
		if ( !PASSPHRASE2_CUR[0] )
		{
			sleep(2);
		}
	}
#else
	getDefaultSSID(wlanIndex,&SSID1_DEF);
	wifi_getSSIDName(wlanIndex,&SSID1_CUR);
	getDefaultPassphase(wlanIndex,&PASSPHRASE1_DEF);
	wifi_getApSecurityKeyPassphrase(wlanIndex,&PASSPHRASE1_CUR);

	wlanIndex=1;
	getDefaultSSID(wlanIndex,&SSID2_DEF);
	wifi_getSSIDName(wlanIndex,&SSID2_CUR);
	getDefaultPassphase(wlanIndex,&PASSPHRASE2_DEF);
	wifi_getApSecurityKeyPassphrase(wlanIndex,&PASSPHRASE2_CUR);
#endif
	if (AnscEqualString(SSID1_DEF, SSID1_CUR , TRUE))
	{
		CcspWifiTrace(("RDK_LOG_WARN,CaptivePortal:%s - Registering for 2.4GHz SSID value change notification ...\n",__FUNCTION__));
  		SetParamAttr("Device.WiFi.SSID.1.SSID",notify);

	}	
	else
	{
		CcspWifiTrace(("RDK_LOG_WARN, Inside SSID1 is changed already\n"));
		SSID1_Changed = TRUE;
	}

	if (AnscEqualString(PASSPHRASE1_DEF, PASSPHRASE1_CUR , TRUE))
	{
		CcspWifiTrace(("RDK_LOG_WARN,CaptivePortal:%s - Registering for 2.4GHz Passphrase value change notification ...\n",__FUNCTION__));
        	SetParamAttr("Device.WiFi.AccessPoint.1.Security.X_COMCAST-COM_KeyPassphrase",notify);
	}
	else
	{
		CcspWifiTrace(("RDK_LOG_WARN, Inside KeyPassphrase1 is changed already\n"));
		PASSPHRASE1_Changed = TRUE;
	}
	if (AnscEqualString(SSID2_DEF, SSID2_CUR , TRUE))
	{
		CcspWifiTrace(("RDK_LOG_WARN,CaptivePortal:%s - Registering for 5GHz SSID value change notification ...\n",__FUNCTION__));
		SetParamAttr("Device.WiFi.SSID.2.SSID",notify);
	}
	else
	{
		CcspWifiTrace(("RDK_LOG_WARN, Inside SSID2 is changed already\n"));
		SSID2_Changed = TRUE;
	}

	if (AnscEqualString(PASSPHRASE2_DEF, PASSPHRASE2_CUR , TRUE))
	{
		CcspWifiTrace(("RDK_LOG_WARN,CaptivePortal:%s - Registering for 5GHz Passphrase value change notification ...\n",__FUNCTION__));
        SetParamAttr("Device.WiFi.AccessPoint.2.Security.X_COMCAST-COM_KeyPassphrase",notify);
	}
	else
	{
		CcspWifiTrace(("RDK_LOG_WARN, Inside KeyPassphrase2 is changed already\n"));
		PASSPHRASE2_Changed = TRUE;
	}
	Captive_Portal_Check();
   }
   else
   {
	 CcspWifiTrace(("RDK_LOG_WARN, CaptivePortal: Inside else check for NotifyChanges\n"));
   }
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(stringValue);

}

void
WiFiPramValueChangedCB
    (
        parameterSigStruct_t*       val,
        int                         size,
        void*                       user_data
    )
{
    if (!val) return;
	printf(" value change received for prameter = %s\n",val->parameterName);

	CcspWifiTrace(("RDK_LOG_WARN,CaptivePortal:%s - value change received for prameter %s...\n",__FUNCTION__,val->parameterName));
	
    char *stringValue = NULL;
    int retPsmGet = CCSP_SUCCESS;

   retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, NotifyWiFiChanges, NULL, &stringValue);

	CcspWifiTrace(("RDK_LOG_WARN,%s CaptivePortal: PSM get of NotifyChanges value is %s \n PSM get returned %d...\n",__FUNCTION__,stringValue,retPsmGet));

    if (AnscEqualString(stringValue, "true", TRUE))
    {
	if (AnscEqualString(val->parameterName, SSID1, TRUE) && strcmp(val->newValue,SSID1_DEF))
	{
  	    CcspWifiTrace(("RDK_LOG_WARN,CaptivePortal:%s - Received notification for changing 2.4GHz SSID of private WiFi... DEFAULT_SSID_NAME_CHANGE:1 %s\n",__FUNCTION__,val->newValue));
		SSID1_Changed = TRUE;	
	}
	else if (AnscEqualString(val->parameterName, SSID2, TRUE) && strcmp(val->newValue,SSID2_DEF)) 
	{
		CcspWifiTrace(("RDK_LOG_WARN,CaptivePortal:%s - Received notification for changing 5GHz SSID of private WiFi...DEFAULT_SSID_NAME_CHANGE:2 %s\n",__FUNCTION__,val->newValue));
		SSID2_Changed = TRUE;	
	}
	else if (AnscEqualString(val->parameterName, PASSPHRASE1, TRUE) && strcmp(val->newValue,PASSPHRASE1_DEF)) 
	{
		CcspWifiTrace(("RDK_LOG_WARN,CaptivePortal:%s - Received notification for changing 2.4GHz passphrase of private WiFi...\n",__FUNCTION__));
		PASSPHRASE1_Changed = TRUE;	
	}
	else if (AnscEqualString(val->parameterName, PASSPHRASE2, TRUE) && strcmp(val->newValue,PASSPHRASE2_DEF) ) 
	{
		CcspWifiTrace(("RDK_LOG_WARN,CaptivePortal:%s - Received notification for changing 5 GHz passphrase of private WiFi...\n",__FUNCTION__));
		PASSPHRASE2_Changed = TRUE;	
	} else {
	
	        printf("This is Factory reset case Ignore \n");
	    	CcspWifiTrace(("RDK_LOG_WARN,CaptivePortal:%s - This is Factory reset case Ignore ...\n",__FUNCTION__));
            return;
	    
	}
	Captive_Portal_Check();
    }
	
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(stringValue);

}

int SetParamAttr(char *pParameterName, int NotificationType) 
{
	char* faultParam = NULL;
	int nResult = CCSP_SUCCESS;
	char dst_pathname_cr[64] = { 0 };
	extern ANSC_HANDLE bus_handle;
	char l_Subsystem[32] = { 0 };
	int ret;
	int size = 0;
	componentStruct_t ** ppComponents = NULL;
	char paramName[100] = { 0 };
    if (!pParameterName) return CCSP_CR_ERR_INVALID_PARAM;
	parameterAttributeStruct_t attriStruct;
    attriStruct.parameterName = NULL;
    attriStruct.notificationChanged = 1;
    attriStruct.accessControlChanged = 0;
	strcpy(l_Subsystem, "eRT.");
	strcpy(paramName, pParameterName);
	sprintf(dst_pathname_cr, "%s%s", l_Subsystem, CCSP_DBUS_INTERFACE_CR);

	ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle,
			dst_pathname_cr, paramName, l_Subsystem, /* prefix */
			&ppComponents, &size);

	if (ret == CCSP_SUCCESS && size == 1)
		{
#ifndef USE_NOTIFY_COMPONENT
			    ret = CcspBaseIf_Register_Event(
                bus_handle, 
                ppComponents[0]->componentName, 
                "parameterValueChangeSignal"
            );      
        
        if ( CCSP_SUCCESS != ret) 
        {
            CcspWifiTrace(("RDK_LOG_ERROR,CaptivePortal:WiFi Parameter failed to Register for notification event ...\n"));

        } 

        CcspBaseIf_SetCallback2
            (
                bus_handle,
                "parameterValueChangeSignal",
                WiFiPramValueChangedCB,
                NULL
            );
#endif
		attriStruct.parameterName = paramName;
		attriStruct.notification = NotificationType; 
		ret = CcspBaseIf_setParameterAttributes(
            bus_handle,
            ppComponents[0]->componentName,
            ppComponents[0]->dbusPath,
            0,
            &attriStruct,
            1
        );
		if ( CCSP_SUCCESS != ret) 
        {
            CcspWifiTrace(("RDK_LOG_ERROR,CaptivePortal:%s WiFi Parameter failed to turn notification on ...\n",__FUNCTION__));
        } 
		free_componentStruct_t(bus_handle, size, ppComponents);
	} else {
		CcspWifiTrace(("RDK_LOG_ERROR,CaptivePortal:%s WiFi Parameter failed to SetValue for SetParamAttr ret val is: %d and param is :%s ... \n",__FUNCTION__,ret,paramName));
	}
	return ret;
}


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
    
    if (!paramName) return -1;
    
    BOOL retVal = FindWifiDestComp();
    wifiDbgPrintf("%s: FindWifiDestComp returned %s\n", __func__, (retVal == TRUE) ? "True" : "False");
    if (retVal != TRUE) {
       return -1;
    }
    
    sprintf(valStr.parameterName, paramName);
    sprintf(valStr.parameterValue, "%s", "TRUE");
    valStr.type = ccsp_boolean;

    if (!Cosa_SetParamValuesNoCommit(dstComp, dstPath, &valStr, 1))
    {
        wifiDbgPrintf("%s: fail to set: %s\n", __FUNCTION__, valStr.parameterName);
        return -1;
    }

    return 0;
}

static void *CosaDml_NotifyWiFiExtThread(void *input) 
{
    if (!input) return;
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
        if(pthread_create(&tid,NULL,CosaDml_NotifyWiFiExtThread, (void*)flag))  {
            return;
        }
    }
}

static COSA_DML_WIFI_RADIO_POWER gRadioPowerState[2] = { COSA_DML_WIFI_POWER_UP, COSA_DML_WIFI_POWER_UP};
// Are Global for whole WiFi
static COSA_DML_WIFI_RADIO_POWER gRadioPowerSetting = COSA_DML_WIFI_POWER_UP;
static COSA_DML_WIFI_RADIO_POWER gRadioNextPowerSetting = COSA_DML_WIFI_POWER_UP;
/* zqiu
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
*/
ANSC_STATUS
CosaDmlWiFiGetFactoryResetPsmData
    (
        BOOLEAN *factoryResetFlag
    )
{
    char *strValue = NULL;
    int retPsmGet = CCSP_SUCCESS;

    if (!factoryResetFlag) return ANSC_STATUS_FAILURE;

	printf("%s g_Subsytem = %s\n",__FUNCTION__, g_Subsystem);
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s \n",__FUNCTION__));
    // Get Non-vol parameters from ARM through PSM
    // PSM may not be available yet on arm so sleep if there is not connection
    int retry = 0;
    while (retry++ < 20)
    {
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s :Calling PSM GET to get FactoryReset flag value\n",__FUNCTION__));
	retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, FactoryReset, NULL, &strValue);
	if (retPsmGet == CCSP_SUCCESS) {
	printf("%s %s = %s \n",__FUNCTION__, FactoryReset, strValue);
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s :PSM GET Success %s = %s \n",__FUNCTION__, FactoryReset, strValue));
	    *factoryResetFlag = _ansc_atoi(strValue);
	    ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);

            // Set to FALSE after FactoryReset has been applied
	    // PSM_Set_Record_Value2(bus_handle,g_Subsystem, FactoryReset, ccsp_string, "0");

	} else if (retPsmGet == CCSP_CR_ERR_INVALID_PARAM) { 
            *factoryResetFlag = 0;
	    printf("%s PSM_Get_Record_Value2 (%s) returned error %d \n",__FUNCTION__, FactoryReset, retPsmGet); 
		CcspWifiTrace(("RDK_LOG_WARN,WIFI %s :PSM_Get_Record_Value2 (%s) returned error %d \n",__FUNCTION__, FactoryReset, retPsmGet));
            // Set to FALSE
	    PSM_Set_Record_Value2(bus_handle,g_Subsystem, FactoryReset, ccsp_string, "0");
	} else { 
	    printf("%s PSM_Get_Record_Value2 returned error %d retry in 10 seconds \n",__FUNCTION__, retPsmGet);
		CcspWifiTrace(("RDK_LOG_WARN,WIFI %s :returned error %d retry in 10 seconds\n",__FUNCTION__, retPsmGet));	
	    AnscSleep(10000); 
	    continue;
	} 
	break;
    }

    if (retPsmGet != CCSP_SUCCESS && retPsmGet != CCSP_CR_ERR_INVALID_PARAM) {
            printf("%s Could not connect to the server error %d\n",__FUNCTION__, retPsmGet);
			CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : Could not connect to the server error %d \n",__FUNCTION__, retPsmGet));
            *factoryResetFlag = 0;
            return ANSC_STATUS_FAILURE;
    }
/*zqiu:

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
            wifi_getSSIDName(2,secSsid1);
            wifi_getSSIDName(3,secSsid2);
            wifi_getSSIDName(4,hotSpot1);
            wifi_getSSIDName(5,hotSpot2);
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
*/
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : Returning Success \n",__FUNCTION__));
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

    if (!resetFlag) return ANSC_STATUS_FAILURE;

    wifiDbgPrintf("%s g_Subsytem = %s\n",__FUNCTION__, g_Subsystem);  
    *resetFlag = FALSE;
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : Calling PSM GET for %s \n",__FUNCTION__,WifiVlanCfgVersion));
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, WifiVlanCfgVersion, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        wifiDbgPrintf("%s %s = %s \n",__FUNCTION__, WifiVlanCfgVersion, strValue); 
		CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : PSM GET Success %s = %s\n",__FUNCTION__, WifiVlanCfgVersion, strValue));
        int version = _ansc_atoi(strValue);
        if (version != gWifiVlanCfgVersion) {
            wifiDbgPrintf("%s: Radio restart required:  %s value of %s was not the required cfg value %d \n",__FUNCTION__, WifiVlanCfgVersion, strValue, gWifiVlanCfgVersion);
			CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : Radio restart required:  %s value of %s was not the required cfg value %d \n",__FUNCTION__, WifiVlanCfgVersion, strValue, gWifiVlanCfgVersion));
            *resetFlag = TRUE;
        } else {
            *resetFlag = FALSE;
        }
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    } else {
        wifiDbgPrintf("%s %s not found in PSM set to %d \n",__FUNCTION__, WifiVlanCfgVersion , gWifiVlanCfgVersion);
		CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : %s not found in PSM set to %d \n",__FUNCTION__,WifiVlanCfgVersion, gWifiVlanCfgVersion));
        *resetFlag = TRUE;
    }
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : Returning Success \n",__FUNCTION__));
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

    if (!resetFlag) return ANSC_STATUS_FAILURE;

    wifiDbgPrintf("%s g_Subsytem = %s\n",__FUNCTION__, g_Subsystem);  
    *resetFlag = FALSE;

    // if either Primary SSID is set to TRUE, then HotSpot needs to be reset 
    // since these should not be HotSpot SSIDs
    for (i = 1; i <= 2; i++) {
        sprintf(recName, BssHotSpot, i);
        retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
        if (retPsmGet == CCSP_SUCCESS) {
            wifiDbgPrintf("%s: found BssHotSpot value = %s \n", __func__, strValue);
			CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : found BssHotSpot value = %s \n",__FUNCTION__, strValue));
            BOOL enable = _ansc_atoi(strValue);
            if (enable == TRUE) {
                *resetFlag = TRUE;
            }
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
        }
    }
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : Returning Success \n",__FUNCTION__));
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
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s \n",__FUNCTION__));
    memset(securityType,0,sizeof(securityType));
    if (modeEnabled == COSA_DML_WIFI_SECURITY_None)
    {
        strcpy(securityType,"None");
        strcpy(authMode,"None");
    } 
	//>> Deprecated
	/*
	else if (modeEnabled == COSA_DML_WIFI_SECURITY_WEP_64 ||
               modeEnabled == COSA_DML_WIFI_SECURITY_WEP_128)
	{ 
		ULONG wepLen;
		char *strValue = NULL;
		char recName[256];
		int retPsmSet = CCSP_SUCCESS;

			strcpy(securityType,"Basic");
			strcpy(authMode,"None");

			wifi_setApBasicEncryptionMode(wlanIndex, "WEPEncryption");

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

    } */
	//<<
#ifndef _XB6_PRODUCT_REQ_
	else if (modeEnabled == COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal)
    {
        strcpy(securityType,"WPAand11i");
        strcpy(authMode,"PSKAuthentication");
    }
	else if (modeEnabled == COSA_DML_WIFI_SECURITY_WPA_Personal)
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
#else
	else if (modeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Personal)
    {
        strcpy(securityType,"11i");
        strcpy(authMode,"PSKAuthentication");
    } else
    { 
        strcpy(securityType,"None");
        strcpy(authMode,"None");
    }
#endif
    wifi_setApBeaconType(wlanIndex, securityType);
    wifi_setApBasicAuthenticationMode(wlanIndex, authMode);
    CcspWifiTrace(("RDK_LOG_WARN,WIFI %s wlanIndex = %d,securityType =%s,authMode = %s\n",__FUNCTION__, wlanIndex,securityType,authMode));
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : Returning Success \n",__FUNCTION__));
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
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : Calling PSM GET for %s \n",__FUNCTION__, WpsPin));
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, WpsPin, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        password = _ansc_atoi(strValue);
		CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : PSM GET Success password %d \n",__FUNCTION__, password));
        wifi_setApWpsDevicePIN(wlanIndex, password);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : Returning Success \n",__FUNCTION__));
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

printf("%s g_Subsytem = %s wlanInex = %d \n",__FUNCTION__, g_Subsystem, wlanIndex);
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s wlanInex = %d \n",__FUNCTION__, wlanIndex));
    memset(recName, 0, sizeof(recName));
    sprintf(recName, RadioIndex, ulInstance);
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s Get Factory Reset PsmData & Apply to WIFI ",__FUNCTION__));
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        intValue = _ansc_atoi(strValue);
fprintf(stderr, "-- %s %d wifi_setApRadioIndex  wlanIndex = %d intValue=%d \n", __func__, __LINE__, wlanIndex, intValue);
	wifi_setApRadioIndex(wlanIndex, intValue);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, WlanEnable, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        intValue = _ansc_atoi(strValue);
    int retStatus = wifi_setApEnable(wlanIndex, intValue);
	if(retStatus == 0) {
		CcspWifiTrace(("RDK_LOG_WARN,WIFI %s wifi_setApEnable success index %d , %d",__FUNCTION__,wlanIndex,intValue));
	}
	else {
		CcspWifiTrace(("RDK_LOG_WARN,WIFI %s wifi_setApEnable failed  index %d , %d",__FUNCTION__,wlanIndex,intValue));
	}
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, BssSsid, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
	wifi_setSSIDName(wlanIndex, strValue);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, HideSsid, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        intValue = (_ansc_atoi(strValue) == 0) ? 1 : 0;
        wifi_setApSsidAdvertisementEnable(wlanIndex, intValue);
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
	    wifi_setApWpaEncryptionMode(wlanIndex, "TKIPEncryption");
	} else if ( intValue == COSA_DML_WIFI_AP_SEC_AES)
	{
	    wifi_setApWpaEncryptionMode(wlanIndex, "AESEncryption");
	} 
#ifndef _XB6_PRODUCT_REQ_	
	else if ( intValue == COSA_DML_WIFI_AP_SEC_AES_TKIP)
	{
	    wifi_setApWpaEncryptionMode(wlanIndex, "TKIPandAESEncryption");
	}
#endif
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, Passphrase, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        wifi_setApSecurityKeyPassphrase(wlanIndex, strValue);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, WmmRadioEnable, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS)
    {
        intValue = _ansc_atoi(strValue);
        wifi_setApWmmEnable(wlanIndex, intValue);
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
        wifi_setApWpsDevicePIN(wlanIndex, password);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, WpsEnable, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        int intValue = _ansc_atoi(strValue);
#if !defined(_COSA_BCM_MIPS_)&& !defined(_COSA_BCM_ARM_)
        if ( (intValue == TRUE) && (password != 0) ) {
           intValue = 2; // Configured 
        } 
#endif
        wifi_setApWpsEnable(wlanIndex, (intValue>0));
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
#if !defined(_COSA_BCM_MIPS_) && !defined(_COSA_BCM_ARM_)
        wifi_setApEnableOnLine(wlanIndex,enable);
#endif
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    } else {
        wifiDbgPrintf("%s: didn't find BssHotSpot setting EnableOnline to FALSE \n", __func__);
#if !defined(_COSA_BCM_MIPS_)&& !defined(_COSA_BCM_ARM_)
        wifi_setApEnableOnLine(wlanIndex,0);
#endif
    }

	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : Returning Success \n",__FUNCTION__));
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
	extern int gChannelSwitchingCount;
    ULONG                       wlanIndex;
    ULONG                       ulInstance;
	static int bBootTime = 0;
    if (pCfg != NULL) {
        ulInstance = pCfg->InstanceNumber;
        wlanIndex = pCfg->InstanceNumber - 1;
    } else {
        return ANSC_STATUS_FAILURE;
    }

printf("%s g_Subsytem = %s\n",__FUNCTION__, g_Subsystem);

    // All these values need to be set once the VAP is up
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s wlanInex = %d \n",__FUNCTION__, wlanIndex));
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s Get Factory Reset Radio PsmData & Apply to WIFI ",__FUNCTION__));
    memset(recName, 0, sizeof(recName));
    sprintf(recName, CTSProtection, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
	BOOL enable = _ansc_atoi(strValue);
        pCfg->CTSProtectionMode = (enable == TRUE) ? TRUE : FALSE;

        wifi_setRadioCtsProtectionEnable(wlanIndex, enable);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    } 

    memset(recName, 0, sizeof(recName));
    sprintf(recName, BeaconInterval, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        intValue = _ansc_atoi(strValue);
        pCfg->BeaconInterval = intValue;
	wifi_setApBeaconInterval(wlanIndex, intValue);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));

    sprintf(recName, DTIMInterval, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        intValue = _ansc_atoi(strValue);
        pCfg->DTIMInterval = intValue;
        wifi_setApDTIMInterval(wlanIndex, intValue);
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

		wifi_getRadioStandard(wlanIndex, opStandards, &gOnly, &nOnly, &acOnly);
		if (strncmp("n",opStandards,1)!=0 && strncmp("ac",opStandards,1)!=0) {
	    wifi_setRadioFragmentationThreshold(wlanIndex, intValue);
        }
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, RTSThreshold, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        intValue = _ansc_atoi(strValue);
        pCfg->RTSThreshold = intValue;
	wifi_setApRtsThreshold(wlanIndex, intValue);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, ObssCoex, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
	BOOL enable = _ansc_atoi(strValue);
        pCfg->ObssCoex = (enable == TRUE) ? TRUE : FALSE;

        wifi_setRadioObssCoexistenceEnable(wlanIndex, enable);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    } 

    memset(recName, 0, sizeof(recName));
    sprintf(recName, STBCEnable, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        BOOL enable = _ansc_atoi(strValue);
        pCfg->X_CISCO_COM_STBCEnable = (enable == TRUE) ? TRUE : FALSE;
	wifi_setRadioSTBCEnable(wlanIndex, enable);
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, GuardInterval, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        // if COSA_DML_WIFI_GUARD_INTVL_800ns set to FALSE otherwise was (400ns or Auto, set to TRUE)
        int guardInterval = _ansc_atoi(strValue);
        pCfg->GuardInterval = guardInterval;
        //BOOL enable = (guardInterval == 2) ? FALSE : TRUE;
		//wifi_setRadioGuardInterval(wlanIndex, enable);
		wifi_setRadioGuardInterval(wlanIndex, (pCfg->GuardInterval == 2)?"800nsec":"Auto");
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
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : Returning Success \n",__FUNCTION__));
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
    
    if (!pCfg)
    {
        return ANSC_STATUS_FAILURE;
    }
    
    PCOSA_DML_WIFI_RADIO_CFG        pStoredCfg  = &sWiFiDmlRadioStoredCfg[pCfg->InstanceNumber-1];

	wifiDbgPrintf("%s g_Subsytem = %s\n",__FUNCTION__, g_Subsystem);
	 CcspWifiTrace(("RDK_LOG_WARN,WIFI %s \n",__FUNCTION__));
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
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : Returning Success \n",__FUNCTION__));
    return ANSC_STATUS_SUCCESS;
}

//>> zqiu
INT CosaDmlWiFiGetRadioStandards(int radioIndex, COSA_DML_WIFI_FREQ_BAND OperatingFrequencyBand, ULONG *pOperatingStandards) {
	// Non-Vol cfg data
	char opStandards[32];
    BOOL gOnly;
    BOOL nOnly;
    BOOL acOnly;
    
    if (!pOperatingStandards) return -1;
    
    wifi_getRadioStandard(radioIndex, opStandards, &gOnly, &nOnly, &acOnly);

    if (strcmp("a",opStandards)==0){ 
		*pOperatingStandards = COSA_DML_WIFI_STD_a;      /* Bitmask of COSA_DML_WIFI_STD */
    } else if (strcmp("b",opStandards)==0) { 
		*pOperatingStandards = COSA_DML_WIFI_STD_b;      /* Bitmask of COSA_DML_WIFI_STD */
    } else if (strcmp("g",opStandards)==0) { 
        if (gOnly == TRUE) {  
			*pOperatingStandards = COSA_DML_WIFI_STD_g;      /* Bitmask of COSA_DML_WIFI_STD */
        } else {
			*pOperatingStandards = COSA_DML_WIFI_STD_b | COSA_DML_WIFI_STD_g;      /* Bitmask of COSA_DML_WIFI_STD */
        }
    } else if (strncmp("n",opStandards,1)==0) { 
		if ( OperatingFrequencyBand == COSA_DML_WIFI_FREQ_BAND_2_4G ) {
            if (gOnly == TRUE) {
				*pOperatingStandards = COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n;      /* Bitmask of COSA_DML_WIFI_STD */
            } else if (nOnly == TRUE) {
				*pOperatingStandards = COSA_DML_WIFI_STD_n;      /* Bitmask of COSA_DML_WIFI_STD */
            } else {
				*pOperatingStandards = COSA_DML_WIFI_STD_b | COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n;      /* Bitmask of COSA_DML_WIFI_STD */
            }
        } else {
            if (nOnly == TRUE) {
				*pOperatingStandards = COSA_DML_WIFI_STD_n;      /* Bitmask of COSA_DML_WIFI_STD */
            } else {
				*pOperatingStandards = COSA_DML_WIFI_STD_a | COSA_DML_WIFI_STD_n;      /* Bitmask of COSA_DML_WIFI_STD */
            }
        }
    } else if (strcmp("ac",opStandards) == 0) {
        if (acOnly == TRUE) {
            *pOperatingStandards = COSA_DML_WIFI_STD_ac;
        } else  if (nOnly == TRUE) {
            *pOperatingStandards = COSA_DML_WIFI_STD_n | COSA_DML_WIFI_STD_ac;
        } else {
            *pOperatingStandards = COSA_DML_WIFI_STD_a | COSA_DML_WIFI_STD_n | COSA_DML_WIFI_STD_ac;
        }
    }
	return 0;
}

INT CosaDmlWiFiGetApStandards(int apIndex, ULONG *pOperatingStandards) {
	return CosaDmlWiFiGetRadioStandards(apIndex%2, ((apIndex%2)==0)?COSA_DML_WIFI_FREQ_BAND_2_4G:COSA_DML_WIFI_FREQ_BAND_5G, pOperatingStandards);
}

INT CosaDmlWiFiSetApBeaconRateControl(int apIndex, ULONG  OperatingStandards) {
	char beaconRate[64]={0};
#ifdef _BEACONRATE_SUPPORT
	if(OperatingStandards == (COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n)) {
		wifi_getApBeaconRate(apIndex, beaconRate);
		if(strcmp(beaconRate, "1Mbps")==0)
			wifi_setApBeaconRate(apIndex, "6Mbps");	
	} else if (OperatingStandards == (COSA_DML_WIFI_STD_b | COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n)) {
		wifi_getApBeaconRate(apIndex, beaconRate);
		if(strcmp(beaconRate, "1Mbps")!=0)
			wifi_setApBeaconRate(apIndex, "1Mbps");	
	}
#endif
	return 0;
}
//<<

INT CosaWifiAdjustBeaconRate(int radioindex, char *beaconRate) {

	CcspWifiTrace(("RDK_LOG_WARN,WIFI Function= %s Start  \n",__FUNCTION__));

    if (!beaconRate) return -1;
    
	if(radioindex==1) {
#ifdef _BEACONRATE_SUPPORT
		//2.4G
		wifi_setApBeaconRate(0, beaconRate);
		wifi_setApBeaconRate(2, beaconRate);
                wifi_setApBeaconRate(4, beaconRate);
                wifi_setApBeaconRate(6, beaconRate);
                wifi_setApBeaconRate(8, beaconRate);
                wifi_setApBeaconRate(10, beaconRate);
		CcspWifiTrace(("RDK_LOG_WARN,WIFI Beacon Rate %s changed for 2.4G, Function= %s  \n",beaconRate,__FUNCTION__));
#endif
	} else {
#ifdef _BEACONRATE_SUPPORT
		//5G   radioindex==2
                wifi_setApBeaconRate(1, beaconRate);
                wifi_setApBeaconRate(3, beaconRate);
                wifi_setApBeaconRate(5, beaconRate);
                wifi_setApBeaconRate(7, beaconRate);
                wifi_setApBeaconRate(9, beaconRate);
                wifi_setApBeaconRate(11, beaconRate);
		CcspWifiTrace(("RDK_LOG_WARN,WIFI Beacon Rate %s changed for 5G, Function= %s  \n",beaconRate,__FUNCTION__));
#endif
	}

	CcspWifiTrace(("RDK_LOG_WARN,WIFI Function= %s End  \n",__FUNCTION__));
	return 0;
}

INT CosaDmlWiFiGetApBeaconRate(int apIndex, ULONG  *BeaconRate) {

        if (!BeaconRate) return -1;

        if ((apIndex >= 0) &&  (apIndex <= 15) )
        {
#ifdef _BEACONRATE_SUPPORT
                wifi_getApBeaconRate(apIndex, BeaconRate);
                CcspWifiTrace(("RDK_LOG_WARN,WIFI APIndex %d , BeaconRate %s \n",apIndex,BeaconRate));
#endif
        }

        return 0;
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

    wifi_getApEnable(wlanIndex, &enabled);

printf("%s g_Subsytem = %s wlanIndex %d ulInstance %d enabled = %s\n",__FUNCTION__, g_Subsystem, wlanIndex, ulInstance, 
       (enabled == TRUE) ? "TRUE" : "FALSE");
		CcspWifiTrace(("RDK_LOG_WARN,WIFI %s wlanInex = %d \n",__FUNCTION__, wlanIndex));
		
		CcspWifiTrace(("RDK_LOG_WARN,WIFI %s Get Factory Reset AccessPoint PsmData & Apply to WIFI ",__FUNCTION__));
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
#if !defined(_COSA_BCM_MIPS_)&& !defined(_COSA_BCM_ARM_)
        wifi_setApEnableOnLine(wlanIndex,enable);
#endif
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    } else {
        wifiDbgPrintf("%s: didn't find BssHotSpot setting EnableOnline to FALSE \n", __func__);
#if !defined(_COSA_BCM_MIPS_)&& !defined(_COSA_BCM_ARM_)
        wifi_setApEnableOnLine(wlanIndex,0);
#endif
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, WmmEnable, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        BOOL enable = _ansc_atoi(strValue);
        pCfg->WMMEnable = enable;
        if (enabled == TRUE) {
            wifi_setApWmmEnable(wlanIndex, enable);
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
            wifi_setApWmmUapsdEnable(wlanIndex, enable);
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
            wifi_setApWmmOgAckPolicy(wlanIndex, 0, intValue);
			wifi_setApWmmOgAckPolicy(wlanIndex, 1, intValue);
			wifi_setApWmmOgAckPolicy(wlanIndex, 2, intValue);
			wifi_setApWmmOgAckPolicy(wlanIndex, 3, intValue);
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
            wifi_setApMaxAssociatedDevices(wlanIndex, intValue);
        }
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, ApIsolationEnable, ulInstance);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        BOOL enable = atoi(strValue);
        pCfg->IsolationEnable = enable;
        if (enabled == TRUE) {
            wifi_setApIsolationEnable(wlanIndex, enable);
        }
        printf("%s: wifi_setApIsolationEnable %d, %d \n", __FUNCTION__, wlanIndex, enable);
	    ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
    }
	
//>> zqiu
/*  //RDKB-7475
	if((wlanIndex%2)==0) { //if it is 2.4G
		memset(recName, 0, sizeof(recName));
		sprintf(recName, BeaconRateCtl, ulInstance);
		retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
		if (retPsmGet == CCSP_SUCCESS) {
			BOOL enable = atoi(strValue);			
			printf("%s: %s %d \n", __FUNCTION__, recName, enable);
			((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
		} else {
			PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "0");		
			ULONG OperatingStandards;
			CosaDmlWiFiGetApStandards(wlanIndex, &OperatingStandards);
			CosaDmlWiFiSetApBeaconRateControl(wlanIndex, OperatingStandards);			
		}
	}
*/
//<<
	
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : Returning Success \n",__FUNCTION__));
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
    PCOSA_DML_WIFI_AP_CFG       pStoredCfg = (PCOSA_DML_WIFI_AP_CFG)NULL;
    CcspWifiTrace(("RDK_LOG_WARN,WIFI %s \n",__FUNCTION__));
    if (pCfg != NULL) {
        ulInstance = pCfg->InstanceNumber;
        wlanIndex = pCfg->InstanceNumber -1;
    } else {
        CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pCfg is NULL \n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }

    pStoredCfg  = &sWiFiDmlApStoredCfg[pCfg->InstanceNumber-1].Cfg;  /*RDKB-6907,  CID-33117, null check before use*/

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
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : Returning Success \n",__FUNCTION__));
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS CosaDmlWiFiGetBridge0PsmData(char *ip, char *sub) {
    char *strValue = NULL;
    char *ssidStrValue = NULL;
    char ipAddr[16]={0};
    char ipSubNet[16]={0};
    char recName[256]={0};
	int retPsmGet = CCSP_SUCCESS;
	
	//zqiu>>
	if(ip) {
		strncpy(ipAddr,ip, sizeof(ipAddr));
	} else  {
		retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, "dmsb.atom.l3net.4.V4Addr", NULL, &strValue);
		if (retPsmGet == CCSP_SUCCESS) {
			strncpy(ipAddr,strValue, sizeof(ipAddr)); 
			((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
		} 
	}
	if(sub) {
		strncpy(ipSubNet,sub, sizeof(ipAddr)); 
	} else {
		retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, "dmsb.atom.l3net.4.V4SubnetMask", NULL, &strValue);
		if (retPsmGet == CCSP_SUCCESS) {
			strncpy(ipSubNet,strValue, sizeof(ipAddr)); 
			((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
		} 
	}
#ifdef DUAL_CORE_XB3	
	if(ipAddr[0]!=0 && ipSubNet[0]!=0) {
		snprintf(recName, sizeof(recName),  "/usr/ccsp/wifi/br0_ip.sh %s %s", ipAddr, ipSubNet);
		system(recName);
	}
#endif	
	fprintf(stderr, "====================== %s [%s]\n", __func__, recName);
	//<<	
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
    char ipAddr[16]={0};
    char ipSubNet[16]={0};
    int numSSIDs = 0;
    char recName[256]={0};
    int retPsmGet = CCSP_SUCCESS;
    int numInstances = 0;
    unsigned int *pInstanceArray = NULL;

    pBridgeVlanCfg = NULL;
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s \n",__FUNCTION__));
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

					//zqiu
                    //if (strstr(ssidName,"ath") != NULL) {
					if (strlen(ssidName) >=2) {
					
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
							//>>zqiu
							if(pBridge->InstanceNumber!=6) {
                            sprintf(bridgeName, "br%d", pBridge->InstanceNumber-1);
							} else { //br106
                            sprintf(bridgeName, "br%d", pBridge->VlanId);
							}
							//<<
                            
                            // Determine if bridge, ipaddress or subnet changed.  If so flag it and save it 
                            {
                                char bridge[16];
                                char ip[32];
                                char ipSubNet[32];
                                wifi_getApBridgeInfo(wlanIndex, bridge, ip, ipSubNet);
                                if (strcmp(ip,pBridge->IpAddress) != 0 ||
                                    strcmp(ipSubNet,pBridge->IpSubNet) != 0 ||
                                    strcmp(bridge, bridgeName) != 0) {
                                    wifi_setApBridgeInfo(wlanIndex,bridgeName, pBridge->IpAddress, pBridge->IpSubNet);
                                    wifi_setApVlanID(wlanIndex,pBridge->VlanId);
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
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : Returning Success \n",__FUNCTION__));
    return ANSC_STATUS_SUCCESS;
}

pthread_mutex_t Hotspot_MacFilt_ThreadMutex = PTHREAD_MUTEX_INITIALIZER;
void Delete_Hotspot_MacFilt_Entries_Thread_Func()
{
	int retPsmGet = CCSP_SUCCESS;
	char recName[256];
	char *device_name = NULL;
	int i,j;
	BOOL ret = FALSE;
	int apIns;

	unsigned int 		InstNumCount    = 0;
	unsigned int*		pInstNumList    = NULL;

    wifiDbgPrintf("%s\n",__FUNCTION__);
    pthread_mutex_lock(&Hotspot_MacFilt_ThreadMutex);
	for(j=0;j<HOTSPOT_NO_OF_INDEX;j++)
	{
		apIns = Hotspot_Index[j];
		memset(recName, 0, sizeof(recName));
		sprintf(recName, "Device.WiFi.AccessPoint.%d.X_CISCO_COM_MacFilterTable.", apIns);

		if(CcspBaseIf_GetNextLevelInstances
		(
			bus_handle,
			WIFI_COMP,
			WIFI_BUS,
			recName,
			&InstNumCount,
			&pInstNumList
		) == CCSP_SUCCESS)
		{

			for(i=InstNumCount-1; i>=0; i--)
			{
				memset(recName, 0, sizeof(recName));
				sprintf(recName, MacFilterDevice, apIns, pInstNumList[i]);

				retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &device_name);
				if (retPsmGet == CCSP_SUCCESS)
				{
					if(strcasecmp(HOTSPOT_DEVICE_NAME, device_name)==0)
					{
						sprintf(recName,"Device.WiFi.AccessPoint.%d.X_CISCO_COM_MacFilterTable.%d.",apIns, pInstNumList[i]);
						ret = Cosa_DelEntry(WIFI_COMP,WIFI_BUS,recName);
						if( !ret)
						{
							printf("MAC_FILTER : Cosa_DelEntry for recName %s failed \n", recName);
						}
					}
					((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(device_name);
				}
			}
			if(pInstNumList)
				free(pInstNumList);

	        }
		else
		{
			printf("MAC_FILTER : CcspBaseIf_GetNextLevelInstances failed \n");
		}
	}
    pthread_mutex_unlock(&Hotspot_MacFilt_ThreadMutex);
}

void Delete_Hotspot_MacFilt_Entries() {

	pthread_t Delete_Hotspot_MacFilt_Entries_Thread;
	int res;
	res = pthread_create(&Delete_Hotspot_MacFilt_Entries_Thread, NULL, Delete_Hotspot_MacFilt_Entries_Thread_Func, NULL);
	if(res != 0) {
	    printf("Create Delete_MacFilt_Entries_Thread %d\n", res);
	}
}
static ANSC_STATUS
CosaDmlWiFiCheckPreferPrivateFeature

    (
        void
    )
{
    BOOL bEnabled;
    int idx[4]={5,6,9,10}, index=0; 
    int apIndex=0;
    char recName[256];
    ULONG ulMacFilterCount=0, macFiltIns;

    CcspWifiTrace(("%s \n",__FUNCTION__));
    printf("%s \n",__FUNCTION__);

    CosaDmlWiFi_GetPreferPrivatePsmData(&bEnabled);

    if (bEnabled == TRUE)
    {
    	for(index = 0; index <4 ; index++) {
       	        apIndex=idx[index];
  		memset(recName, 0, sizeof(recName));
    		sprintf(recName, MacFilterMode, apIndex);
        	wifi_setApMacAddressControlMode(apIndex-1, 2);
        	PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "2");
        }

    }
    else
    {
    	for(index = 0; index <4 ; index++) {
                apIndex=idx[index];

                memset(recName, 0, sizeof(recName));
                sprintf(recName, MacFilterMode, apIndex);
                wifi_setApMacAddressControlMode(apIndex-1, 0);
                PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "0");

    	}
	//Delete_Hotspot_MacFilt_Entries();
    }

    CcspWifiTrace(("%s returning\n",__FUNCTION__));
    printf("%s returning\n",__FUNCTION__);

    return ANSC_STATUS_SUCCESS;
}

void *Wifi_Hosts_Sync_Func(void *pt, int index, wifi_associated_dev_t *associated_dev, BOOL bCallForFullSync);
void CosaDMLWiFi_Send_FullHostDetails_To_LMLite(LM_wifi_hosts_t *phosts);
void CosaDMLWiFi_Send_ReceivedHostDetails_To_LMLite(LM_wifi_host_t   *phost);

SyncLMLite()
{

	parameterValStruct_t    value = { "Device.Hosts.X_RDKCENTRAL-COM_LMHost_Sync", "0", ccsp_unsignedInt};
	char compo[256] = "eRT.com.cisco.spvtg.ccsp.lmlite";
	char bus[256] = "/com/cisco/spvtg/ccsp/lmlite";
	char* faultParam = NULL;
	int ret = 0;	

    CcspWifiTrace(("RDK_LOG_WARN,WIFI %s  \n",__FUNCTION__));

	ret = CcspBaseIf_setParameterValues(
		  bus_handle,
		  compo,
		  bus,
		  0,
		  0,
		  &value,
		  1,
		  TRUE,
		  &faultParam
		  );

	if(ret == CCSP_SUCCESS)
	{
		CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : Sync with LMLite\n",__FUNCTION__));
		Wifi_Hosts_Sync_Func(NULL,0, NULL, 1);
	}
	else
	{
		CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : FAILED to sync with LMLite ret: %d \n",__FUNCTION__,ret));
	}	
}

void *wait_for_brlan1_up()
{
    UCHAR      ucEntryNameValue[128]       = {0};
    UCHAR      ucEntryParamName[128]       = {0};
    int        ulEntryNameLen;
    parameterValStruct_t varStruct;
    BOOL radioEnabled = FALSE;
    printf("****entering %s\n",__FUNCTION__);
    pthread_detach(pthread_self());
    int timeout=240;
    //sleep(100);

    AnscCopyString(ucEntryParamName,"Device.IP.Interface.5.Status");
    varStruct.parameterName = ucEntryParamName;
    varStruct.parameterValue = ucEntryNameValue;
    do 
    {
        if (COSAGetParamValueByPathName(g_MessageBusHandle,&varStruct,&ulEntryNameLen)==0 )
        {
               
           //printf("****%s, %s\n",__FUNCTION__, varStruct.parameterValue);
           timeout-=2;
    	   if(timeout<=0)  //wait at most 4 minutes
              break;
           sleep(2);  
        }
        if (access(RADIO_BROADCAST_FILE, F_OK) == 0) 
        {
            CcspWifiTrace(("%s is created Start Radio Broadcasting\n", RADIO_BROADCAST_FILE));
            break;
        }
        else
        {
            printf("%s is not created not starting Radio Broadcasting\n", RADIO_BROADCAST_FILE);
        }
    } while (strcasecmp(varStruct.parameterValue ,"Up"));

	char SSID1_CUR[COSA_DML_WIFI_MAX_SSID_NAME_LEN]={0},SSID2_CUR[COSA_DML_WIFI_MAX_SSID_NAME_LEN]={0};
	wifi_getSSIDName(0,&SSID1_CUR);
   	wifi_pushSsidAdvertisementEnable(0, AdvEnable24);
   	CcspTraceInfo(("\n"));
   	CcspTraceInfo(("WIFI_SSID_UP:%s\n",SSID1_CUR));
   	wifi_getSSIDName(1,&SSID2_CUR);
    	wifi_pushSsidAdvertisementEnable(1, AdvEnable5);
    	CcspTraceInfo(("WIFI_SSID_UP:%s\n",SSID2_CUR));
    	


    wifi_getRadioEnable(0, &radioEnabled);
    if (radioEnabled == TRUE)
    {
        wifi_setLED(0, true);
    }
    else
    {
        fprintf(stderr,"Radio 0 is not Enabled\n");
    }
    wifi_getRadioEnable(1, &radioEnabled);
    if (radioEnabled == TRUE)
    {
        wifi_setLED(1, true);
    }
    else
    {
       fprintf(stderr, "Radio 1 is not Enabled\n");
    }

	//zqiu: move to CosaDmlWiFiGetBridge0PsmData
	//system("/usr/ccsp/wifi/br0_ip.sh"); 
	CosaDmlWiFiGetBridge0PsmData(NULL, NULL);
	system("/usr/ccsp/wifi/br106_addvlan.sh");
}

//zqiu: set the passphrase for L&F SSID int wifi config
static void wifi_setLFSecurityKeyPassphrase() {
	system("/usr/ccsp/wifi/lfp.sh");
}


ANSC_STATUS
CosaDmlWiFiFactoryReset
    (
    )
{
    int i;
    pthread_t tid4;
    char recName[256];
    char recValue[256];
    char *strValue = NULL;
    int retPsmGet = CCSP_SUCCESS;
    int resetSSID[2] = {0,0};

	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s \n",__FUNCTION__));
    for (i = 1; i <= gRadioCount; i++)
    {
        memset(recName, 0, sizeof(recName));
        sprintf(recName, FactoryResetSSID, i);
		CcspWifiTrace(("RDK_LOG_WARN,WIFI %s PSM GET for FactoryResetSSID \n",__FUNCTION__));
        retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
        if (retPsmGet == CCSP_SUCCESS)
        {
            resetSSID[i-1] = atoi(strValue);
            ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
        }
        // Reset to 0
        PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "0");

        wifiDbgPrintf("%s: Radio %d has resetSSID = %d \n", __FUNCTION__, i, resetSSID[i-1]);
		CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : Radio %d has resetSSID = %d\n",__FUNCTION__, i, resetSSID[i-1]));
    }

    // reset all SSIDs
    if ( (resetSSID[0] == COSA_DML_WIFI_FEATURE_ResetSsid1) && (resetSSID[1] == COSA_DML_WIFI_FEATURE_ResetSsid2) )
    {
        // delete current configuration
        wifi_factoryReset();

        // create current configuration
        // It is neccessary to first recreate the config
        // from the defaults and next we will override them with the Platform specific data
        wifi_createInitialConfigFiles();

        // These are the values PSM values that were generated from the ARM intel db
        // modidify current configuration

    #if COSA_DML_WIFI_FEATURE_LoadPsmDefaults
        for (i = 0; i < gRadioCount; i++)
        {
            CosaDmlWiFiGetRadioFactoryResetPsmData(i, i+1);
        }

        for (i = 0; i < gSsidCount; i++)
        {
            CosaDmlWiFiGetSSIDFactoryResetPsmData(i, i+1);
        }
    #endif

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

    #if COSA_DML_WIFI_FEATURE_LoadPsmDefaults
        // Reset radio parameters
        wifi_factoryResetRadios();
        for (i = 0; i < gRadioCount; i++)
        {
            CosaDmlWiFiGetRadioFactoryResetPsmData(i, i+1);
        }
    #endif
    }

    wifi_getApSsidAdvertisementEnable(0, &AdvEnable24);
    wifi_getApSsidAdvertisementEnable(1, &AdvEnable5);

    // Bring Radios Up again if we aren't doing PowerSaveMode
    if ( gRadioPowerSetting != COSA_DML_WIFI_POWER_DOWN &&
         gRadioNextPowerSetting != COSA_DML_WIFI_POWER_DOWN ) {
        //printf("******%s***Initializing wifi 3\n",__FUNCTION__);

        wifi_setLED(0, false);
        wifi_setLED(1, false);
        fprintf(stderr, "-- wifi_setLED off\n");
		wifi_setLFSecurityKeyPassphrase();
        m_wifi_init();
#if !defined(_COSA_BCM_MIPS_)&& !defined(_COSA_BCM_ARM_)
        wifi_pushSsidAdvertisementEnable(0, false);
        wifi_pushSsidAdvertisementEnable(1, false);
	
//Home Security is currently not supported for Raspberry Pi platform
#if !defined(_PLATFORM_RASPBERRYPI_)
        pthread_create(&tid4, NULL, &wait_for_brlan1_up, NULL);
#endif
#endif
    }

    // Set FixedWmmParams to TRUE on Factory Reset so that we won't override the data.
    // There were two required changes.  Set to 3 so that we know neither needs to be applied
    PSM_Set_Record_Value2(bus_handle,g_Subsystem, FixedWmmParams, ccsp_string, "3");
    /*CcspWifiEventTrace(("RDK_LOG_NOTICE, KeyPassphrase changed in Factory Reset\n"));*/

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiFactoryResetRadioAndAp( ULONG radioIndex, ULONG apIndex, BOOL needRestart) {
    
	if(radioIndex>0) {
fprintf(stderr, "+++++++++++++++++++++ wifi_factoryResetRadio %d\n", radioIndex-1);
		wifi_factoryResetRadio(radioIndex-1);
#if COSA_DML_WIFI_FEATURE_LoadPsmDefaults
                CosaDmlWiFiGetRadioFactoryResetPsmData(radioIndex-1, radioIndex);
#endif
    }
	if(apIndex>0) {
fprintf(stderr, "+++++++++++++++++++++ wifi_factoryResetAP %d\n", apIndex-1);
		wifi_factoryResetAP(apIndex-1);
#if COSA_DML_WIFI_FEATURE_LoadPsmDefaults
                CosaDmlWiFiGetSSIDFactoryResetPsmData(apIndex-1, apIndex); 
#endif
	}
    // Bring Radios Up again if we aren't doing PowerSaveMode
    if (needRestart &&
		gRadioPowerSetting != COSA_DML_WIFI_POWER_DOWN &&
        gRadioNextPowerSetting != COSA_DML_WIFI_POWER_DOWN ) {
fprintf(stderr, "+++++++++++++++++++++ wifi_init\n");		
		wifi_setLFSecurityKeyPassphrase();
        m_wifi_init();
    }

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
        //zqiu: wifi_reset has bug
		wifi_reset();
		//wifi_down();
		//m_wifi_init();
		
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

#if defined(ENABLE_FEATURE_MESHWIFI)
            // Turn off power save for mesh access points (ath12 & ath13)
            if (i == 12 || i == 13) {
                sprintf(recName, UAPSDEnable, i+1);
                PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "0");
            } else {
                sprintf(recName, UAPSDEnable, i+1);
                PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "1");
            }
#else
            sprintf(recName, UAPSDEnable, i+1);
            PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "1");
#endif
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
/*zqiu
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
        wlan_getWpsDevicePassword(wlanIndex,&wpsPin);
        printf("%s  called wlan_getWpsDevicePassword on ath%d\n",__FUNCTION__, wlanIndex);
        if (wpsPin == 0)
        {
            unsigned int password = 0;
            retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, WpsPin, NULL, &strValue);
            if (retPsmGet == CCSP_SUCCESS)
            {
                password = _ansc_atoi(strValue);
                wlan_setWpsDevicePassword(wlanIndex, password);
                ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
            }
        }

        pskKey[0] = '\0';
        wlan_getKeyPassphrase(wlanIndex,pskKey);
        if (strlen(pskKey) == 0)
        {
            memset(recName, 0, sizeof(recName));
            sprintf(recName, Passphrase, wlanIndex+1);
            retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
            if (retPsmGet == CCSP_SUCCESS)
            {
                wlan_setKeyPassphrase(wlanIndex, strValue);
                ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
            }
        }
    }
}
*/

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
    pthread_t tid4;
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

#if defined (_COSA_BCM_MIPS_) || defined (_PLATFORM_RASPBERRYPI_)|| defined (_COSA_BCM_ARM_)
        //Scott: Broadcom hal needs wifi_init to be called when we are started up
		//wifi_setLFSecurityKeyPassphrase();
        m_wifi_init();
#endif

        //zqiu: do not merge
//        CosaDmlWiFiCheckSecurityParams();
        CosaDmlWiFiCheckWmmParams();

        //>>zqiu
		// Fill Cache
        //CosaDmlWiFiSsidFillSinfoCache(NULL, 1);
        //CosaDmlWiFiSsidFillSinfoCache(NULL, 2);
		//<<
		
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
#if !defined (_COSA_BCM_MIPS_) && !defined(_COSA_BCM_ARM_)
                wifi_setApEnableOnLine(i-1,0);
#endif
            }            
        }

            wifi_getApSsidAdvertisementEnable(0, &AdvEnable24);
            wifi_getApSsidAdvertisementEnable(1, &AdvEnable5);

#if !defined(_CBR_PRODUCT_REQ_)
        // If no VAPs were up or we have new Vlan Cfg re-init both Radios
        if ( resetHotSpot == TRUE && 
             gRadioPowerSetting != COSA_DML_WIFI_POWER_DOWN &&
             gRadioNextPowerSetting != COSA_DML_WIFI_POWER_DOWN ) {
            //printf("%s: calling wifi_init 1 \n", __func__);
            wifi_setLED(0, false);
            wifi_setLED(1, false);
            fprintf(stderr, "-- wifi_setLED off\n");
			wifi_setLFSecurityKeyPassphrase();
            m_wifi_init();
#if !defined(_COSA_BCM_MIPS_)&& !defined(_COSA_BCM_ARM_)
            wifi_pushSsidAdvertisementEnable(0, false);
            wifi_pushSsidAdvertisementEnable(1, false);


		   /* crate a thread and wait */
    	   pthread_create(&tid4, NULL, &wait_for_brlan1_up, NULL);
#endif
        }
#endif

        BOOLEAN noEnableVaps = TRUE;
                BOOL radioActive = TRUE;
        wifi_getRadioStatus(0, &radioActive);
        printf("%s: radioActive wifi0 = %s \n", __func__, (radioActive == TRUE) ? "TRUE" : "FALSE");
        if (radioActive == TRUE) {
            noEnableVaps = FALSE;
        }
        wifi_getRadioStatus(1,&radioActive);
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

#if !defined(_CBR_PRODUCT_REQ_)
        // If no VAPs were up or we have new Vlan Cfg re-init both Radios
        if ( (noEnableVaps == TRUE || newVlanCfg == TRUE)  &&
             gRadioPowerSetting != COSA_DML_WIFI_POWER_DOWN &&
             gRadioNextPowerSetting != COSA_DML_WIFI_POWER_DOWN ) {
            //printf("%s: calling wifi_init 2 \n", __func__);

            wifi_setLED(0, false);
            wifi_setLED(1, false);
            fprintf(stderr, "-- wifi_setLED off\n");
			wifi_setLFSecurityKeyPassphrase();
            m_wifi_init();
#if !defined(_COSA_BCM_MIPS_)&& !defined(_COSA_BCM_ARM_)
            wifi_pushSsidAdvertisementEnable(0, false);
            wifi_pushSsidAdvertisementEnable(1, false);
//Home Security is currently not supported for Raspberry Pi platform
#if !defined(_PLATFORM_RASPBERRYPI_)
            pthread_create(&tid4, NULL, &wait_for_brlan1_up, NULL);
#endif
#endif
        }
#endif

//XB6 phase 1 lost and Found
#ifdef _XB6_PRODUCT_REQ_
	//pthread_create(&tid4, NULL, &wait_for_brlan1_up, NULL);
        fprintf(stderr,"CALL VLAN UTIL TO SET UP LNF\n");
        system("sysevent set lnf-setup 6");
        //wifi_setLFSecurityKeyPassphrase();
#endif

        CosaDmlWiFiCheckPreferPrivateFeature();

    }


    BOOL retInit = Cosa_Init(bus_handle);
    printf("%s: Cosa_Init returned %s \n", __func__, (retInit == 1) ? "True" : "False");

	wifi_newApAssociatedDevice_callback_register(CosaDmlWiFi_AssociatedDevice_callback);
#if defined(ENABLE_FEATURE_MESHWIFI)
	wifi_handle_sysevent_async();
#endif
	CosaDmlWiFi_startDCSScanThread();
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
static void *CosaDmlWiFiFactoryResetRadioAndApThread(void *arg) 
{
    if (!arg) return NULL;
	ULONG indexes=(ULONG)arg;
	
    pthread_detach(pthread_self());
    pthread_mutex_lock(&sWiFiThreadMutex);

    PSM_Set_Record_Value2(bus_handle,g_Subsystem, ReloadConfig, ccsp_string, "TRUE");

    pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    CosaWifiReInitializeRadioAndAp((ANSC_HANDLE)pMyObject, indexes);    
 
	// Notify WiFiExtender that WiFi parameter have changed
    CosaDml_NotifyWiFiExt(COSA_WIFIEXT_DM_UPDATE_RADIO|COSA_WIFIEXT_DM_UPDATE_WPS|COSA_WIFIEXT_DM_UPDATE_SSID);

    pthread_mutex_unlock(&sWiFiThreadMutex);


    pthread_t tid3;
    pthread_create(&tid3, NULL, &RegisterWiFiConfigureCallBack, NULL);

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
CosaDmlWiFi_GetPreferPrivatePsmData(BOOL *value)
{
    char *strValue = NULL;
    char str[2];
    int retPsmGet = CCSP_SUCCESS;

    if (!value) return ANSC_STATUS_FAILURE;

    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, PreferPrivate, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        *value = _ansc_atoi(strValue);
        CcspWifiTrace(("RDK_LOG_WARN,%s-%d Enable PreferPrivate is %d\n",__FUNCTION__,__LINE__,*value));
        ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
        retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, PreferPrivate_configured, NULL, &strValue);
        if (retPsmGet == CCSP_SUCCESS)
          {
          ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
          }
        else
          {
             *value = TRUE; //Default value , TRUE
             sprintf(str,"%d",*value);
             CcspWifiTrace(("RDK_LOG_WARN,%s-%d Enable PreferPrivate by default\n",__FUNCTION__,__LINE__));
             retPsmGet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, PreferPrivate, ccsp_string, str);
             if (retPsmGet != CCSP_SUCCESS) {
                CcspWifiTrace(("RDK_LOG_WARN,%s PSM_Set_Record_Value2 returned error %d while setting %s \n",__FUNCTION__, retPsmGet, PreferPrivate));
             return ANSC_STATUS_FAILURE;
             }
             retPsmGet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, PreferPrivate_configured, ccsp_string, str);
             if (retPsmGet != CCSP_SUCCESS) {
                CcspWifiTrace(("RDK_LOG_WARN,%s PSM_Set_Record_Value2 returned error %d while setting %s \n",__FUNCTION__, retPsmGet, PreferPrivate_configured));
                return ANSC_STATUS_FAILURE;
             } 

          }

    }
    else
    {
        *value = TRUE; //Default value , TRUE
        sprintf(str,"%d",*value);
        CcspWifiTrace(("RDK_LOG_WARN,%s Enable PreferPrivate by default\n",__FUNCTION__));
        retPsmGet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, PreferPrivate, ccsp_string, str);
        if (retPsmGet != CCSP_SUCCESS) {
           CcspWifiTrace(("RDK_LOG_WARN,%s PSM_Set_Record_Value2 returned error %d while setting %s \n",__FUNCTION__, retPsmGet, PreferPrivate));
           return ANSC_STATUS_FAILURE;
        }
        retPsmGet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, PreferPrivate_configured, ccsp_string, str);
        if (retPsmGet != CCSP_SUCCESS) {
           CcspWifiTrace(("RDK_LOG_WARN,%s PSM_Set_Record_Value2 returned error %d while setting %s \n",__FUNCTION__, retPsmGet, PreferPrivate_configured));
           return ANSC_STATUS_FAILURE;
        }


    }

    return ANSC_STATUS_SUCCESS;
}



ANSC_STATUS
CosaDmlWiFi_SetPreferPrivatePsmData(BOOL value)
{
    char strValue[2] = {0};
    int retPsmSet = CCSP_SUCCESS;
    int idx[4]={5,6,9,10}, index=0; 
    int apIndex=0;
    ULONG ulMacFilterCount=0, macFiltIns;
    char recName[256];

    sprintf(strValue,"%d",value);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, PreferPrivate, ccsp_string, strValue);
    if (retPsmSet != CCSP_SUCCESS) {
        CcspWifiTrace(("%s PSM_Set_Record_Value2 returned error %d while setting %s \n",__FUNCTION__, retPsmSet, PreferPrivate));
        return ANSC_STATUS_FAILURE;
    }


   if(value == TRUE)
   {

    for(index = 0; index <4 ; index++) {
                apIndex=idx[index];
    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilterMode, apIndex);
    wifi_setApMacAddressControlMode(apIndex-1, 2);
    PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "2");
    }

   }
   
   if(value == FALSE)
   { 
    for(index = 0; index <4 ; index++) {
                apIndex=idx[index];

    		memset(recName, 0, sizeof(recName));
    		sprintf(recName, MacFilterMode, apIndex);
		wifi_setApMacAddressControlMode(apIndex-1, 0);
		PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "0");
    }
	Delete_Hotspot_MacFilt_Entries();
  }

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS CosaDmlWiFi_PSM_Del_Radio(ULONG radioIndex) { 
	char recName[256];
    char recValue[256];
	
	printf("-- %s: deleting PSM radio %d \n", __FUNCTION__, radioIndex);
	sprintf(recName, CTSProtection, radioIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, BeaconInterval, radioIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, DTIMInterval, radioIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, FragThreshold, radioIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, RTSThreshold, radioIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, ObssCoex, radioIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);
	return CCSP_SUCCESS;
}
		
ANSC_STATUS CosaDmlWiFi_PSM_Del_Ap(ULONG apIndex) { 		
	char recName[256];
    char recValue[256];
	
	printf("-- %s: deleting PSM Ap %d \n", __FUNCTION__, apIndex);

	sprintf(recName, WmmEnable, apIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, UAPSDEnable, apIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, WmmNoAck, apIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, BssMaxNumSta, apIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, BssHotSpot, apIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	CosaDmlWiFiPsmDelMacFilterTable(apIndex);

	// Platform specific data that is stored in the ARM Intel DB and converted to PSM entries
	// They will be read only on Factory Reset command and override the current Wifi configuration
	sprintf(recName, RadioIndex, apIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, WlanEnable, apIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, BssSsid, apIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, HideSsid, apIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, SecurityMode, apIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, EncryptionMethod, apIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, Passphrase, apIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, WmmRadioEnable, apIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, WpsEnable, apIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);

	sprintf(recName, Vlan, apIndex);
	PSM_Del_Record(bus_handle,g_Subsystem,recName);
	
	return CCSP_SUCCESS;
}
	
ANSC_STATUS
CosaDmlWiFi_FactoryResetRadioAndAp(ULONG radioIndex, ULONG radioIndex_2, ULONG apIndex, ULONG apIndex_2) {   
    int retPsmGet = CCSP_SUCCESS;
fprintf(stderr, "-- %s %d %d %d %d\n", __func__,  radioIndex,   radioIndex_2,  apIndex, apIndex_2);
    // Delete PSM entries for Wifi Primary SSIDs related values
	if(radioIndex>0 && radioIndex<=2) 
		CosaDmlWiFi_PSM_Del_Radio(radioIndex);
	else
		radioIndex=0;
		
    if(radioIndex_2>0 && radioIndex_2<=2) 
		CosaDmlWiFi_PSM_Del_Radio(radioIndex_2);
	else
		radioIndex_2=0;

	if(apIndex>0 && apIndex<=16) 
        CosaDmlWiFi_PSM_Del_Ap(apIndex);
	else
		apIndex=0;
		
    if(apIndex_2>0 && apIndex_2<=16) 
        CosaDmlWiFi_PSM_Del_Ap(apIndex_2);
	else
		apIndex_2=0;
	
    PSM_Del_Record(bus_handle,g_Subsystem,WpsPin);
    PSM_Del_Record(bus_handle,g_Subsystem,FactoryReset);
    PSM_Reset_UserChangeFlag(bus_handle,g_Subsystem,"Device.WiFi.");

    {
        pthread_t tid; 
		ULONG indexes=0;
        
		indexes=(radioIndex<<24) + (radioIndex_2<<16) + (apIndex<<8) + apIndex_2;
		printf("%s Factory Reset Radio %lu %lu and AP %lu %lu  (indexes=%lu)\n",__FUNCTION__, radioIndex, radioIndex_2, apIndex, apIndex_2, indexes ); 
        if (pthread_create(&tid,NULL,CosaDmlWiFiFactoryResetRadioAndApThread, (void *)indexes))
        {
            return ANSC_STATUS_FAILURE;
        }

       	PSM_Set_Record_Value2(bus_handle,g_Subsystem, NotifyWiFiChanges, ccsp_string,"true");


/*		FILE *fp;
		char command[30];

		memset(command,0,sizeof(command));
		sprintf(command, "ls /tmp/*walledgarden*");
		char buffer[50];
		memset(buffer,0,sizeof(buffer));
        if(!(fp = popen(command, "r")))
		{
              exit(1);
        }
		while(fgets(buffer, sizeof(buffer), fp)!=NULL)
		{
			buffer[strlen(buffer) - 1] = '\0';
		}

		if ( strlen(buffer) == 0 )
		{
			//pthread_t captive;
			//pthread_create(&captive, NULL, &configWifi, NULL);
			BOOLEAN redirect;
			redirect = TRUE;
			CcspWifiTrace(("RDK_LOG_WARN,CaptivePortal:%s - WiFi restore case, setting system in Captive Portal redirection mode...\n",__FUNCTION__));
			configWifi(redirect);

		}
		pclose(fp); */

		BOOLEAN redirect;
		redirect = TRUE;
		CcspWifiTrace(("RDK_LOG_WARN,CaptivePortal:%s - WiFi restore case, setting system in Captive Portal redirection mode...\n",__FUNCTION__));
		configWifi(redirect);

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
                m_wifi_init();
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
            wifi_setRadioTransmitPower(0, 5);
            wifi_setRadioTransmitPower(1, 5);
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

        wifi_getRadioTransmitPower(0,&ath0Power);
        wifi_getRadioTransmitPower(1, &ath1Power);

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
    if (!power) return ANSC_STATUS_FAILURE;
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

    if (!pInfo || (wlanIndex<0) || (wlanIndex>=RADIO_INDEX_MAX))
    {
        return ANSC_STATUS_FAILURE;
    }
    //zqiu
    //sprintf(pInfo->Name, "wifi%d", wlanIndex);
    wifi_getRadioIfName(wlanIndex, pInfo->Name);

    pInfo->bUpstream = FALSE;

    //  Currently this is not working
    { 
	char maxBitRate[32];
	wifi_getRadioMaxBitRate(wlanIndex, maxBitRate);
	wifiDbgPrintf("%s: wifi_getRadioMaxBitRate returned %s\n", __FUNCTION__, maxBitRate);
//>> zqiu: fix Wifi MaxBitRate Parsing
	if (strstr(maxBitRate, "Mb/s")) {
		//216.7 Mb/s
		pInfo->MaxBitRate = strtof(maxBitRate,0);
	} else if (strstr(maxBitRate, "Gb/s")) {
        //1.3 Gb/s
        pInfo->MaxBitRate = strtof(maxBitRate,0) * 1000;
    } else {
		//Auto or Kb/s
		pInfo->MaxBitRate = 0;
	}


//	if (strncmp(maxBitRate,"Auto",strlen("Auto")) == 0)
//	{ 
//	    pInfo->MaxBitRate = COSA_DML_WIFI_TXRATE_Auto;
//
//	} else if (strncmp(maxBitRate,"6M",strlen("6M")) == 0)
//	{
//	    pInfo->MaxBitRate = COSA_DML_WIFI_TXRATE_6M;
//	    
//	} else if (strncmp(maxBitRate,"9M",strlen("9M")) == 0)
//	{
//	    pInfo->MaxBitRate = COSA_DML_WIFI_TXRATE_9M;
//	    
//	} else if (strncmp(maxBitRate,"12M",strlen("12M")) == 0)
//	{
//	    pInfo->MaxBitRate = COSA_DML_WIFI_TXRATE_12M;
//	    
//	} else if (strncmp(maxBitRate,"18M",strlen("18M")) == 0)
//	{
//	    pInfo->MaxBitRate = COSA_DML_WIFI_TXRATE_18M;
//	    
//	} else if (strncmp(maxBitRate,"24M",strlen("24M")) == 0)
//	{
//	    pInfo->MaxBitRate = COSA_DML_WIFI_TXRATE_24M;
//	    
//	} else if (strncmp(maxBitRate,"36M",strlen("36M")) == 0)
//	{
//	    pInfo->MaxBitRate = COSA_DML_WIFI_TXRATE_36M;
//	    
//	} else if (strncmp(maxBitRate,"48M",strlen("48M")) == 0)
//	{
//	    pInfo->MaxBitRate = COSA_DML_WIFI_TXRATE_48M;
//	    
//	} else if (strncmp(maxBitRate,"54M",strlen("54M")) == 0)
//	{
//	    pInfo->MaxBitRate = COSA_DML_WIFI_TXRATE_54M;
//	    
//	}
//<<
    }

    char frequencyBand[10];
    wifi_getRadioSupportedFrequencyBands(wlanIndex, frequencyBand);
    //zqiu: Make it more generic
    if (strstr(frequencyBand,"2.4") != NULL) {
#if defined (_XB6_PRODUCT_REQ_)
	//b mode is not supported in xb6
        pInfo->SupportedStandards = COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n;
#else
        pInfo->SupportedStandards = COSA_DML_WIFI_STD_b | COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n;
#endif
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
#if defined (_XB6_PRODUCT_REQ_)
	//b mode is not supported in xb6
        pInfo->SupportedStandards = COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n;
#else
        pInfo->SupportedStandards = COSA_DML_WIFI_STD_b | COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n;
#endif
        pInfo->SupportedFrequencyBands = COSA_DML_WIFI_FREQ_BAND_2_4G; /* Bitmask of COSA_DML_WIFI_FREQ_BAND */
        pInfo->IEEE80211hSupported     = FALSE;
    }
    else 
    {	
	//zqiu: set 11ac as 5G default
        pInfo->SupportedStandards = COSA_DML_WIFI_STD_a | COSA_DML_WIFI_STD_n | COSA_DML_WIFI_STD_ac;
        pInfo->SupportedFrequencyBands = COSA_DML_WIFI_FREQ_BAND_5G; /* Bitmask of COSA_DML_WIFI_FREQ_BAND */
        pInfo->IEEE80211hSupported     = TRUE;
        }
    }

    wifi_getRadioPossibleChannels(wlanIndex, pInfo->PossibleChannels);
    
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
    if (!pEntry) return ANSC_STATUS_FAILURE;
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
    if (!pCfg) return ANSC_STATUS_FAILURE;
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

    if (!transmitPowerPercent) return ANSC_STATUS_FAILURE;

    wifi_getRadioTransmitPower(wlanIndex, &curTransmitPower);

#if defined(_COSA_BCM_MIPS_)|| defined(_COSA_BCM_ARM_)
    percent = curTransmitPower;
#else
    // If you set to > than the max it sets to max - Atheros logic
    wifi_setRadioTransmitPower(wlanIndex, 30);
    wifi_getRadioTransmitPower(wlanIndex, &maxTransmitPower);
/* zqiu: do not merge
    while ( (retries < 5) && ( (maxTransmitPower <= 5) || (maxTransmitPower >= 30) ) ) {
          wifiDbgPrintf("%s: maxTransmitPower wifi%d = %d sleep and retry (%d) \n", __func__, wlanIndex, maxTransmitPower, retries);
          sleep(1);
          wifi_getRadioTransmitPower(wlanIndex, &maxTransmitPower);
          retries++;
    } 
*/
    wifi_setRadioTransmitPower(wlanIndex, curTransmitPower);

    if (maxTransmitPower == curTransmitPower) percent = 100;
    else if ((maxTransmitPower-2) == curTransmitPower) percent = 75;
    else if ((maxTransmitPower-3) == curTransmitPower) percent = 50;
    else if ((maxTransmitPower-6) == curTransmitPower) percent = 25;
    else if ((maxTransmitPower-9) == curTransmitPower) percent = 12;

    // if a match was not found set to 100%
    if ( percent == 0 ) {
        // set to max Transmit Power
        percent = 100;
        wifi_setRadioTransmitPower(wlanIndex, maxTransmitPower);
    }
#endif

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

#if defined(_COSA_BCM_MIPS_)|| defined(_COSA_BCM_ARM_)
    wifi_setRadioTransmitPower(wlanIndex, transmitPowerPercent);
#else
    int ret = wifi_getRadioTransmitPower(wlanIndex, &curTransmitPower);
    if (ret == 0) {

        // If you set to > than the max it sets to max - Atheros logic
        wifi_setRadioTransmitPower(wlanIndex, 30);
        wifi_getRadioTransmitPower(wlanIndex, &maxTransmitPower);
        while ( (retries < 5) && ((maxTransmitPower <= 5) || (maxTransmitPower >= 30) )  ) {
              wifiDbgPrintf("%s: maxTransmitPower wifi%d = %d sleep and retry (%d) \n", __func__, wlanIndex, maxTransmitPower, retries);
              sleep(1);
              wifi_getRadioTransmitPower(wlanIndex, &maxTransmitPower);
              retries++;
        } 
        wifi_setRadioTransmitPower(wlanIndex, curTransmitPower);
        wifiDbgPrintf("%s: maxTransmitPower wifi%d = %d \n", __func__, wlanIndex, maxTransmitPower);
        if (maxTransmitPower > 0) {
            if (transmitPowerPercent == 100) transmitPower = maxTransmitPower;
            if (transmitPowerPercent == 75) transmitPower = maxTransmitPower-2;
            if (transmitPowerPercent == 50) transmitPower = maxTransmitPower-3;
            if (transmitPowerPercent == 25) transmitPower = maxTransmitPower-6;
            if (transmitPowerPercent == 12) transmitPower = maxTransmitPower-9;

            wifiDbgPrintf("%s: transmitPower wifi%d = %d percent = %d \n", __func__, wlanIndex, transmitPower, transmitPowerPercent);

            if (transmitPower != 0) {
                wifi_setRadioTransmitPower(wlanIndex, transmitPower);
            } 
        }
    }
#endif

    return ANSC_STATUS_SUCCESS;
}

/* zqiu: keep old function
ANSC_STATUS
CosaDmlWiFiRadioPushCfg
    (
        PCOSA_DML_WIFI_RADIO_CFG    pCfg,        // Identified by InstanceNumber 
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
        wifi_setRadioAMSDUEnable(wlanIndex, pCfg->X_CISCO_COM_AggregationMSDU);
        wifi_setRadioSTBCEnable(wlanIndex,pCfg->X_CISCO_COM_STBCEnable);
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
    wifi_setRadioGuardInterval(wlanAthIndex, enable);

    wifi_setRadioCtsProtectionEnable(wlanAthIndex, pCfg->CTSProtectionMode);
    wifi_setApBeaconInterval(wlanAthIndex, pCfg->BeaconInterval);
    wifi_setDTIMInterval(wlanAthIndex, pCfg->DTIMInterval);

    //  Only set Fragmentation if mode is not n and therefore not HT
    if ( (pCfg->OperatingStandards|COSA_DML_WIFI_STD_n) == 0) {
        wifi_setRadioFragmentationThreshold(wlanAthIndex, pCfg->FragmentationThreshold);
    }
    wifi_setApRtsThreshold(wlanAthIndex, pCfg->RTSThreshold);
    wifi_setRadioObssCoexistenceEnable(wlanAthIndex, pCfg->ObssCoex); 

    return ANSC_STATUS_SUCCESS;
}
*/

ANSC_STATUS
CosaDmlWiFiRadioPushCfg
    (
        PCOSA_DML_WIFI_RADIO_CFG    pCfg        /* Identified by InstanceNumber */
    )
{
    PCOSA_DML_WIFI_RADIO_CFG        pRunningCfg  =  (PCOSA_DML_WIFI_RADIO_CFG)NULL;
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    wifiDbgPrintf("%s Config changes  \n",__FUNCTION__);
    int  wlanIndex;

    if (!pCfg )
    {
        return ANSC_STATUS_FAILURE;
    }

    pRunningCfg  = &sWiFiDmlRadioRunningCfg[pCfg->InstanceNumber-1]; /*RDKB-6907, CID-33012, null check before use*/

    wlanIndex = (ULONG) pCfg->InstanceNumber-1;  
    wifiDbgPrintf("%s[%d] Config changes  wlanIndex %d \n",__FUNCTION__, __LINE__, wlanIndex);

	//zqiu: replace with INT wifi_applyRadioSettings(INT radioIndex);
	//>>	
    //wifi_pushChannelMode(wlanIndex);
    //wifi_pushChannel(wlanIndex, pCfg->Channel);
    //wifi_pushTxChainMask(wlanIndex);
    //wifi_pushRxChainMask(wlanIndex);
    //wifi_pushDefaultValues(wlanIndex);
	wifi_setRadioChannel(wlanIndex, pCfg->Channel);
	wifi_applyRadioSettings(wlanIndex);
	//<<
	
    //BOOL enable = (pCfg->GuardInterval == 2) ? FALSE : TRUE;
    wifi_setRadioGuardInterval(wlanIndex, (pCfg->GuardInterval == 2)?"800nsec":"Auto");

    CosaDmlWiFiRadioSetTransmitPowerPercent(wlanIndex, pCfg->TransmitPower);

    wifi_setRadioCtsProtectionEnable(wlanIndex, pCfg->CTSProtectionMode);
    wifi_setApBeaconInterval(wlanIndex, pCfg->BeaconInterval);
	wifi_setApDTIMInterval(wlanIndex, pCfg->DTIMInterval);

    //  Only set Fragmentation if mode is not n and therefore not HT
    if ( (pCfg->OperatingStandards|COSA_DML_WIFI_STD_n) == 0) {
        wifi_setRadioFragmentationThreshold(wlanIndex, pCfg->FragmentationThreshold);
    }
    wifi_setApRtsThreshold(wlanIndex, pCfg->RTSThreshold);
    wifi_setRadioObssCoexistenceEnable(wlanIndex, pCfg->ObssCoex); 
    wifi_setRadioAMSDUEnable(wlanIndex, pCfg->X_CISCO_COM_AggregationMSDU);
    wifi_setRadioSTBCEnable(wlanIndex,pCfg->X_CISCO_COM_STBCEnable);

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiRadioApplyCfg
(
PCOSA_DML_WIFI_RADIO_CFG    pCfg        /* Identified by InstanceNumber */
) 
{
    PCOSA_DML_WIFI_RADIO_CFG        pRunningCfg    = (PCOSA_DML_WIFI_RADIO_CFG )NULL;
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    int  wlanIndex;
    BOOL pushCfg = FALSE;
    BOOL createdNewVap = FALSE;

    wifiDbgPrintf("%s Config changes  \n", __FUNCTION__);

    if (!pCfg )
    {
        return ANSC_STATUS_FAILURE;
    }

    wlanIndex = (ULONG) pCfg->InstanceNumber-1;  
    if (( wlanIndex < 0 ) || ( wlanIndex >= WIFI_INDEX_MAX ) )
    {
        return ANSC_STATUS_FAILURE;
    }

    /*RDKB-6907, CID-33379, null check before use*/
    pRunningCfg  = &sWiFiDmlRadioRunningCfg[pCfg->InstanceNumber-1];

    // Apply Settings to Radio
    printf("%s Calling pthread_mutex_lock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 
    pthread_mutex_lock(&sWiFiThreadMutex);
    printf("%s Called pthread_mutex_lock for sWiFiThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 
    wifiDbgPrintf("%s Config changes  %d\n",__FUNCTION__, __LINE__);

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

            wifi_getRadioStatus(wlanIndex, &activeVaps);
            // bring down all VAPs
            if (activeVaps == TRUE)
            {
                int i;
                for (i = wlanIndex; i < gSsidCount; i+=2)
                {
                    BOOL vapEnabled = FALSE;
					//zqiu:>>
					char status[64]={0};
                    //wifi_getApEnable(i, &vapEnabled); 
					wifi_getSSIDStatus(i, status);
					vapEnabled=(strcmp(status,"Enabled")==0);					
fprintf(stderr, "----# %s %d 	ath%d %s\n", __func__, __LINE__, i, status);
					//<<
                    if (vapEnabled == TRUE)
                    {
                        PCOSA_DML_WIFI_APSEC_CFG pRunningApSecCfg = &sWiFiDmlApSecurityRunning[i].Cfg;

					char cmd[128];
					char buf[1024];
					wifi_getApName(i, buf);	
					snprintf(cmd, sizeof(cmd), "ifconfig %s down 2>/dev/null", buf);
					buf[0]='\0';
					//_syscmd(cmd, buf, sizeof(buf));
					system(cmd);
						//zqiu:>>

#if 0
fprintf(stderr, "----# %s %d 	wifi_setApEnable %d false\n", __func__, __LINE__, i);
						int retStatus = wifi_setApEnable(i, FALSE);
						if(retStatus == 0) {
							CcspWifiTrace(("RDK_LOG_WARN,WIFI %s wifi_setApEnable success index %d , false ",__FUNCTION__,i));
						}
						else {
								CcspWifiTrace(("RDK_LOG_WARN,WIFI %s wifi_setApEnable failed  index %d , false",__FUNCTION__,i));
						}
                        wifi_deleteAp(i); 
						//<<
						
                        if (pRunningApSecCfg->ModeEnabled >= COSA_DML_WIFI_SECURITY_WPA_Personal)
                        {
                            wifi_removeApSecVaribles(i);
                            sWiFiDmlRestartHostapd = TRUE;
                            wifiDbgPrintf("%s %d sWiFiDmlRestartHostapd set to TRUE\n",__FUNCTION__, __LINE__);
                        }
#endif
                    }
                }

#if 0
                if (sWiFiDmlRestartHostapd == TRUE)
                {
                    // Bounce hostapd to pick up security changes
                    wifi_stopHostApd();
                    wifi_startHostApd(); 
                    sWiFiDmlRestartHostapd = FALSE;
                    wifiDbgPrintf("%s %d sWiFiDmlRestartHostapd set to FALSE\n",__FUNCTION__, __LINE__);
                }
#endif
            }
        } else
        {

            int i;
            BOOL activeVaps = FALSE;

            wifi_getRadioStatus(wlanIndex, &activeVaps);
            wifiDbgPrintf("%s Config changes   %dApplySettingSSID = %d\n",__FUNCTION__, __LINE__, pCfg->ApplySettingSSID);

            for (i=wlanIndex; i < 16; i += 2)
            {
                // if ApplySettingSSID has been set, only apply changes to the specified SSIDs
                if ( (pCfg->ApplySettingSSID != 0) && !((1<<i) & pCfg->ApplySettingSSID ))
                {
                    printf("%s: Skipping SSID %d, it was not in ApplySettingSSID(%d)\n", __func__, i, pCfg->ApplySettingSSID);
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
				//zqiu:>>
				char status[64]={0};
                //wifi_getApEnable(i,&up); 
				wifi_getSSIDStatus(i, status);
				up=(strcmp(status,"Enabled")==0);				
fprintf(stderr, "----# %s %d 	ath%d %s\n", __func__, __LINE__, i, status);
fprintf(stderr, "----# %s %d 	pStoredSsidCfg->bEnabled=%d  pRunningSsidCfg->bEnabled=%d\n", __func__, __LINE__, pStoredSsidCfg->bEnabled, pRunningSsidCfg->bEnabled);				
fprintf(stderr, "----# %s %d 	pStoredSsidCfg->EnableOnline=%d  pStoredSsidCfg->RouterEnabled=%d\n", __func__, __LINE__, pStoredSsidCfg->EnableOnline, pStoredSsidCfg->RouterEnabled);				
				//<<
				
                if ( (up == FALSE) && 
                     (pStoredSsidCfg->bEnabled == TRUE) &&  
                     ( (pStoredSsidCfg->EnableOnline == FALSE) ||
                       (pStoredSsidCfg->RouterEnabled == TRUE)))
                {
                    sWiFiDmlAffectedVap[i] = TRUE;

                    wifi_createAp(i,wlanIndex,pStoredSsidCfg->SSID, (pStoredApCfg->SSIDAdvertisementEnabled == TRUE) ? FALSE : TRUE);
                    createdNewVap = TRUE;
                    // push Radio config to new VAP
					//zqiu:
					// first VAP created for radio, push Radio config
                    if (activeVaps == FALSE)
                    {

						//CosaDmlWiFiRadioPushCfg(pCfg, i,((activeVaps == FALSE) ? TRUE : FALSE));
						CosaDmlWiFiRadioPushCfg(pCfg);
						activeVaps = TRUE;
					}
                    CosaDmlWiFiApPushCfg(pStoredApCfg); 
					// push mac filters
                    CosaDmlWiFiApMfPushCfg(sWiFiDmlApMfCfg[i], i);					
                    CosaDmlWiFiApPushMacFilter(sWiFiDmlApMfQueue[i], i);
                    // push security and restart hostapd
                    CosaDmlWiFiApSecPushCfg(pStoredApSecCfg, pStoredApCfg->InstanceNumber);
                    
                    wifi_pushBridgeInfo(i);

                    // else if up=TRUE, but should be down
                } else if (up==TRUE && 
                           (  (pStoredSsidCfg->bEnabled == FALSE) ||  
                              (  (pStoredSsidCfg->EnableOnline == TRUE) && 
                                 (pStoredSsidCfg->RouterEnabled == FALSE )) ) )
                {
					//zqiu:>>
					if(pStoredSsidCfg->bEnabled==FALSE) {
fprintf(stderr, "----# %s %d 	wifi_setApEnable %d false\n", __func__, __LINE__, i);				
						int retStatus = wifi_setApEnable(i, FALSE);
						if(retStatus == 0) {
							CcspWifiTrace(("RDK_LOG_WARN,WIFI %s wifi_setApEnable success index %d , false ",__FUNCTION__,i));
						}
						else {
								CcspWifiTrace(("RDK_LOG_WARN,WIFI %s wifi_setApEnable failed index %d , false ",__FUNCTION__,i));
						}
					}
					//<<
                    wifi_deleteAp(i); 
                    if (pRunningApSecCfg->ModeEnabled >= COSA_DML_WIFI_SECURITY_WPA_Personal)
                    {
                        wifi_removeApSecVaribles(i);
                        sWiFiDmlRestartHostapd = TRUE;
                        wifiDbgPrintf("%s %d sWiFiDmlRestartHostapd set to TRUE\n",__FUNCTION__, __LINE__);
                    }

                    // else if up=TRUE and changes, apply them
                } else if (up==TRUE)
                {
                    BOOLEAN wpsChange = (strcmp(pRunningSsidCfg->SSID, pStoredSsidCfg->SSID) != 0) ? TRUE : FALSE;
                    sWiFiDmlAffectedVap[i] = TRUE;

                    // wifi_ifConfigDown(i);
					//zqiu:>>
					if(pStoredSsidCfg->bEnabled==TRUE) {
fprintf(stderr, "----# %s %d 	wifi_setApEnable %d true\n", __func__, __LINE__, i);				
						int retStatus = wifi_setApEnable(i, TRUE);
						if(retStatus == 0) {
							CcspWifiTrace(("RDK_LOG_WARN,WIFI %s wifi_setApEnable success index %d , true ",__FUNCTION__,i));
						}
						else {
								CcspWifiTrace(("RDK_LOG_WARN,WIFI %s wifi_setApEnable failed  index %d , true ",__FUNCTION__,i));
						}
					}
					//<<
					
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

#if defined(ENABLE_FEATURE_MESHWIFI)
                    // Notify Mesh components of an AP config change
                    {
                        char cmd[256] = {0};
                        char secMode[256] = {0};
                        char encryptMode[256] = {0};

                        CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of AP config changes\n",__FUNCTION__));

                        // Grab security Mode
                        switch (pStoredApSecCfg->ModeEnabled)
                        {
                        case COSA_DML_WIFI_SECURITY_WEP_64:
                            strcpy(secMode, "WEP-64");
                            break;
                        case COSA_DML_WIFI_SECURITY_WEP_128:
                            strcpy(secMode, "WEP-128");
                            break;
                        case COSA_DML_WIFI_SECURITY_WPA_Personal:
                            strcpy(secMode, "WPA-Personal");
                            break;
                        case COSA_DML_WIFI_SECURITY_WPA2_Personal:
                            strcpy(secMode, "WPA2-Personal");
                            break;
                        case COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal:
                            strcpy(secMode, "WPA-WPA2-Personal");
                            break;
                        case COSA_DML_WIFI_SECURITY_WPA_Enterprise:
                            strcpy(secMode, "WPA-Enterprise");
                            break;
                        case COSA_DML_WIFI_SECURITY_WPA2_Enterprise:
                            strcpy(secMode, "WPA2-Enterprise");
                            break;
                        case COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise:
                            strcpy(secMode, "WPA-WPA2-Enterprise");
                            break;
                        case COSA_DML_WIFI_SECURITY_None:
                        default:
                            strcpy(secMode, "None");
                            break;
                        }

                        // Grab encryption method
                        switch (pStoredApSecCfg->EncryptionMethod)
                        {
                        case COSA_DML_WIFI_AP_SEC_TKIP:
                            strcpy(encryptMode, "TKIPEncryption");
                            break;
                        case COSA_DML_WIFI_AP_SEC_AES:
                            strcpy(encryptMode, "AESEncryption");
                            break;
                        case COSA_DML_WIFI_AP_SEC_AES_TKIP:
                            strcpy(encryptMode, "TKIPandAESEncryption");
                            break;
                        default:
                            strcpy(encryptMode, "None");
                            break;
                        }

                        // notify mesh components that wifi ap settings changed
                        // index|ssid|passphrase|secMode|encryptMode
                        sprintf(cmd, "/usr/bin/sysevent set wifi_ApSecurity \"RDK|%d|%s|%s|%s\"",
                                i,
                                pStoredApSecCfg->KeyPassphrase,
                                secMode,
                                encryptMode);
                        system(cmd);
                    }
#endif
                }
            } // for each SSID

            /* if any of the user-controlled SSID is up, turn on the WiFi LED */
            int  MbssEnable = 0;
            for (i=wlanIndex; i < 16; i += 2)
            {
                if (sWiFiDmlSsidStoredCfg[i].bEnabled == TRUE)
                    MbssEnable += 1<<((i-wlanIndex)/2);
            }
            //zqiu: let driver contorl LED
			//wifi_setLED(wlanIndex, (MbssEnable & pCfg->MbssUserControl)?1:0);

            if (sWiFiDmlRestartHostapd == TRUE)
            {
                // Bounce hostapd to pick up security changes
#if defined(_COSA_INTEL_USG_ATOM_)
                wifi_restartHostApd();
#else
                wifi_stopHostApd();
                wifi_startHostApd();
#endif
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
					//zqiu: Deprecated
                    //wifi_pushChannelMode(wlanIndex);

                } // Mode changed

                if (pCfg->AutoChannelEnable != pRunningCfg->AutoChannelEnable || 
                    (pRunningCfg->AutoChannelEnable == FALSE && pCfg->Channel != pRunningCfg->Channel) )
                {
                    // Currently do not allow the channel to change while the SSIDs are running
                    // so this code should never be called.  
                    //zqiu: Deprecated
					//wifi_pushChannel(wlanIndex, pCfg->Channel); 
                }

/*                if ( pCfg->X_CISCO_COM_HTTxStream != pRunningCfg->X_CISCO_COM_HTTxStream)
                {
                    wifi_pushRadioTxChainMask(wlanIndex);
                }
                if (pCfg->X_CISCO_COM_HTRxStream != pRunningCfg->X_CISCO_COM_HTRxStream)
                {
                    wifi_pushRadioRxChainMask(wlanIndex);
                }
*/                
		if (pCfg->MCS != pRunningCfg->MCS)
                {
                    wifi_setRadioMCS(wlanIndex, pCfg->MCS);
                }

                if (pCfg->TransmitPower != pRunningCfg->TransmitPower)
                {
                    if ( ( gRadioPowerState[wlanIndex] == COSA_DML_WIFI_POWER_UP ) &&
                         ( gRadioNextPowerSetting != COSA_DML_WIFI_POWER_DOWN ) )
                    {
                        CosaDmlWiFiRadioSetTransmitPowerPercent(wlanIndex, pCfg->TransmitPower);
                    } else if ( gRadioPowerState[wlanIndex] == COSA_DML_WIFI_POWER_LOW )
                    {
                        wifiDbgPrintf("%s calling wifi_setRadioTransmitPower to 5 dbM, RadioPower was set to COSA_DML_WIFI_POWER_LOW \n",__FUNCTION__);
                        wifi_setRadioTransmitPower(wlanIndex, 5);
                    }
                }
                
                if (pCfg->X_CISCO_COM_AggregationMSDU != pRunningCfg->X_CISCO_COM_AggregationMSDU)
                {
                    wifi_setRadioAMSDUEnable(wlanIndex, pCfg->X_CISCO_COM_AggregationMSDU);
                }
                if (pCfg->X_CISCO_COM_STBCEnable != pRunningCfg->X_CISCO_COM_STBCEnable )
                {
                    wifi_setRadioSTBCEnable(wlanIndex, pCfg->X_CISCO_COM_STBCEnable);
                }
#if defined(_COSA_BCM_MIPS_)
                if ( pCfg->GuardInterval != pRunningCfg->GuardInterval )
                {
	                wifi_setRadioGuardInterval(wlanIndex, (pCfg->GuardInterval == 2)?"800nsec":"Auto");
                }
#endif
                wifiDbgPrintf("%s Pushing Radio Config changes  %d\n",__FUNCTION__, __LINE__);
                // Find the first ath that is up on the given radio
                for (athIndex = wlanIndex; athIndex < 16; athIndex+=2) {
                    BOOL enabled;
                    wifi_getApEnable(athIndex, &enabled);
                    wifiDbgPrintf("%s Pushing Radio Config changes %d %d\n",__FUNCTION__, athIndex, __LINE__);

                    if (enabled == TRUE) {
#if !defined (_COSA_BCM_MIPS_)
                        // These Radio parameters are set on SSID basis (iwpriv/iwconfig ath%d commands) 
                        if (pCfg->GuardInterval != pRunningCfg->GuardInterval)
                        {
                            // pCfg->GuardInterval   
                            // COSA_DML_WIFI_GUARD_INTVL_400ns and COSA_DML_WIFI_GUARD_INTVL_Auto are the
                            // same in the WiFi driver
                            //BOOL enable = (pCfg->GuardInterval == 2) ? FALSE : TRUE;
                            wifi_setRadioGuardInterval(athIndex, (pCfg->GuardInterval == 2)?"800nsec":"Auto");
                        }
#endif
                        if (pCfg->CTSProtectionMode != pRunningCfg->CTSProtectionMode)
                        {
                            wifi_setRadioCtsProtectionEnable(athIndex, pCfg->CTSProtectionMode);
                        }
                        if (pCfg->BeaconInterval != pRunningCfg->BeaconInterval)
                        {
                            wifi_setApBeaconInterval(athIndex, pCfg->BeaconInterval);
                        }
                        if (pCfg->DTIMInterval != pRunningCfg->DTIMInterval)
                        {
                            wifi_setApDTIMInterval(athIndex, pCfg->DTIMInterval);
                        }
                        //  Only set Fragmentation if mode is not n and therefore not HT
                        if (pCfg->FragmentationThreshold != pRunningCfg->FragmentationThreshold &&
                            (pCfg->OperatingStandards|COSA_DML_WIFI_STD_n) == 0)
                        {
                            wifi_setRadioFragmentationThreshold(athIndex, pCfg->FragmentationThreshold);
                        }
                        if (pCfg->RTSThreshold != pRunningCfg->RTSThreshold)
                        {
                            wifi_setApRtsThreshold(athIndex, pCfg->RTSThreshold);
                        }
                        if (pCfg->ObssCoex != pRunningCfg->ObssCoex)
                        {
                            wifi_setRadioObssCoexistenceEnable(athIndex, pCfg->ObssCoex); 
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
                    wifi_resetApVlanCfg(i); 
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
    PCOSA_DML_WIFI_RADIO_CFG        pStoredCfg  = (PCOSA_DML_WIFI_RADIO_CFG)NULL;
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    int  wlanIndex;
    char frequency[32];
    char channelMode[32];
    char opStandards[32];
    static char temp[256];
    BOOL wlanRestart = FALSE;

    wifiDbgPrintf("%s Config changes  \n",__FUNCTION__);
    CcspWifiTrace(("RDK_LOG_WARN,%s\n",__FUNCTION__));

    if (!pCfg )
    {
        return ANSC_STATUS_FAILURE;
    }

    wlanIndex = (ULONG) pCfg->InstanceNumber-1;
    if ( (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
        return ANSC_STATUS_FAILURE;
    }
    /*RDKB-6907, CID-32973, null check before use*/
    pStoredCfg = &sWiFiDmlRadioStoredCfg[pCfg->InstanceNumber-1];


    pCfg->LastChange             = AnscGetTickInSeconds();
    printf("%s: LastChange %d \n", __func__,pCfg->LastChange);

    // Push changed parameters to persistent store, but don't push to the radio
    // Until after ApplySettings is set to TRUE
    CosaDmlWiFiSetRadioPsmData(pCfg, wlanIndex, pCfg->InstanceNumber);

    if (pStoredCfg->bEnabled != pCfg->bEnabled )
    {
        // this function will set a global Radio flag that will be used by the apup script
        // if the value is FALSE, the SSIDs on that radio will not be brought up even if they are enabled
        // if ((SSID.Enable==TRUE)&&(Radio.Enable==true)) then bring up SSID 
        wifi_setRadioEnable(wlanIndex,pCfg->bEnabled);
        if(pCfg->bEnabled)
        {
            wifi_setLED(wlanIndex,true);
            CcspWifiEventTrace(("RDK_LOG_NOTICE, WiFi radio %s is set to UP\n ",pCfg->Alias));
            CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : WiFi radio %s is set to UP \n ",pCfg->Alias));
			//>>zqiu
			sWiFiDmlRestartHostapd=TRUE;
			//<<
        }
        else
        {
            CcspWifiEventTrace(("RDK_LOG_NOTICE, WiFi radio %s is set to DOWN\n ",pCfg->Alias));
            CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : WiFi radio %s is set to DOWN \n ",pCfg->Alias));
        }
    }

    if (pStoredCfg->X_COMCAST_COM_DFSEnable != pCfg->X_COMCAST_COM_DFSEnable )
    {
        wifi_setRadioDfsEnable(wlanIndex,pCfg->X_COMCAST_COM_DFSEnable);
    }
	
	if (pStoredCfg->X_COMCAST_COM_DCSEnable != pCfg->X_COMCAST_COM_DCSEnable )
    {
        wifi_setRadioDCSEnable(wlanIndex,pCfg->X_COMCAST_COM_DCSEnable);
    }

    if (pStoredCfg->X_COMCAST_COM_IGMPSnoopingEnable != pCfg->X_COMCAST_COM_IGMPSnoopingEnable )
    {
        wifi_setRadioIGMPSnoopingEnable(wlanIndex,pCfg->X_COMCAST_COM_IGMPSnoopingEnable);
    }

    if (pCfg->AutoChannelEnable != pStoredCfg->AutoChannelEnable)
    {
        // If ACS is turned off or on the radio must be restarted to pick up the new channel
        wlanRestart = TRUE;  // Radio Restart Needed
        wifiDbgPrintf("%s: Radio Reset Needed!!!!\n",__FUNCTION__);
		CcspWifiTrace(("RDK_LOG_WARN, RDKB_WIFI_CONFIG_CHANGED : %s Radio Reset Needed!!!!\n",__FUNCTION__)); 
		//CcspWifiTrace((" Test Reset Needed!!!!\n")); 
        if (pCfg->AutoChannelEnable == TRUE)
        {
            printf("%s: Setting Auto Channel Selection to TRUE \n",__FUNCTION__);
	   		CcspWifiTrace(("RDK_LOG_WARN, RDKB_WIFI_CONFIG_CHANGED : %s Setting Auto Channel Selection to TRUE\n",__FUNCTION__));
            wifi_setRadioAutoChannelEnable(wlanIndex, pCfg->AutoChannelEnable);
        } else {
            printf("%s: Setting Channel= %d\n",__FUNCTION__,pCfg->Channel);
            CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : %s Setting Channel= %d \n",__FUNCTION__,pCfg->Channel));
            wifi_setRadioChannel(wlanIndex, pCfg->Channel);
        }

    } else if (  (pCfg->AutoChannelEnable == FALSE) && (pCfg->Channel != pStoredCfg->Channel) )
    {
        printf("%s: Setting Channel= %d\n",__FUNCTION__,pCfg->Channel);
		CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : %s Setting Channel= %d \n",__FUNCTION__,pCfg->Channel));
        wifi_setRadioChannel(wlanIndex, pCfg->Channel);
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
                    if(pCfg->Channel < 5 ) {   //zqiu
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
            }else if (pCfg->OperatingChannelBandwidth == COSA_DML_WIFI_CHAN_BW_160M)
            {
                sprintf(chnMode,"11ACVHT160");
            }
        }

        // if OperatingStandards is set to only g or only n, set only flag to TRUE.
        // wifi_setRadioChannelMode will set PUREG=1/PUREN=1 in the config
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
        printf("%s: wifi_setRadioChannelMode= Wlan%d, Mode: %s, gOnlyFlag: %d, nOnlyFlag: %d\n acOnlyFlag: %d\n",__FUNCTION__,wlanIndex,chnMode,gOnlyFlag,nOnlyFlag, acOnlyFlag);
	
       CcspWifiTrace(("RDK_LOG_WARN, RDKB_WIFI_CONFIG_CHANGED : %s wifi_setChannelMode= Wlan: %d, Mode: %s, gOnlyFlag: %d, nOnlyFlag: %d acOnlyFlag: %d \n",__FUNCTION__,wlanIndex,chnMode,gOnlyFlag,nOnlyFlag, acOnlyFlag));

        wifi_setRadioChannelMode(wlanIndex, chnMode, gOnlyFlag, nOnlyFlag, acOnlyFlag);

#if defined(ENABLE_FEATURE_MESHWIFI)
        {
            char cmd[256] = {0};
            // notify mesh components that wifi radio settings changed
            CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of Radio Channel Mode changes\n",__FUNCTION__));
            snprintf(cmd, sizeof(cmd)-1, "/usr/bin/sysevent set wifi_RadioChannelMode \"RDK|%d|%s|%s|%s|%s\"",
                    wlanIndex, chnMode, (gOnlyFlag?"true":"false"), (nOnlyFlag?"true":"false"), (acOnlyFlag?"true":"false"));
            system(cmd);
        }
#endif
    } // Done with Mode settings
/*
    if (pCfg->X_CISCO_COM_HTTxStream != pStoredCfg->X_CISCO_COM_HTTxStream)
    {
		CcspWifiTrace(("RDK_LOG_WARN,%s : wlanIndex: %d X_CISCO_COM_HTTxStream : %lu \n",__FUNCTION__,wlanIndex,pCfg->X_CISCO_COM_HTTxStream));
        wifi_setRadioTxChainMask(wlanIndex, pCfg->X_CISCO_COM_HTTxStream);
    }
    if (pCfg->X_CISCO_COM_HTRxStream != pStoredCfg->X_CISCO_COM_HTRxStream)
    {
		CcspWifiTrace(("RDK_LOG_WARN,%s : wlanIndex: %d X_CISCO_COM_HTRxStream : %lu \n",__FUNCTION__,wlanIndex,pCfg->X_CISCO_COM_HTRxStream));
        wifi_setRadioRxChainMask(wlanIndex, pCfg->X_CISCO_COM_HTRxStream);
    }
*/
	if (strcmp(pStoredCfg->BasicDataTransmitRates, pCfg->BasicDataTransmitRates)!=0 )
    {	//zqiu
		CcspWifiTrace(("RDK_LOG_WARN,%s : wlanIndex: %d BasicDataTransmitRates : %s \n",__FUNCTION__,wlanIndex,pCfg->BasicDataTransmitRates));
        memset(temp,0,sizeof(temp));
        strncpy(temp,pCfg->BasicDataTransmitRates,sizeof(pCfg->BasicDataTransmitRates));
        wifi_setRadioBasicDataTransmitRates(wlanIndex,temp);
    }
    	if (strcmp(pStoredCfg->OperationalDataTransmitRates, pCfg->OperationalDataTransmitRates)!=0 )
    {	
		CcspWifiTrace(("RDK_LOG_WARN,%s : wlanIndex: %d OperationalDataTransmitRates : %s \n",__FUNCTION__,wlanIndex,pCfg->OperationalDataTransmitRates));
	memset(temp,0,sizeof(temp));
        strncpy(temp,pCfg->OperationalDataTransmitRates,sizeof(pCfg->OperationalDataTransmitRates));
        wifi_setRadioOperationalDataTransmitRates(wlanIndex,temp);
    }

#if defined(ENABLE_FEATURE_MESHWIFI)
        {
            if (strcmp(pStoredCfg->BasicDataTransmitRates, pCfg->BasicDataTransmitRates)!=0 ||
                strcmp(pStoredCfg->OperationalDataTransmitRates, pCfg->OperationalDataTransmitRates)!=0)
            {
                char cmd[256] = {0};
                // notify mesh components that wifi radio transmission rate changed
                CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of Radio Transmission Rate changes\n",__FUNCTION__));
                snprintf(cmd, sizeof(cmd)-1, "/usr/bin/sysevent set wifi_TxRate \"RDK|%d|BasicRates:%s|OperationalRates:%s\"",
                        wlanIndex+1, pCfg->BasicDataTransmitRates, pCfg->OperationalDataTransmitRates);
                system(cmd);
            }
        }
#endif

    // pCfg->AutoChannelRefreshPeriod       = 3600;
    // Modulation Coding Scheme 0-23, value of -1 means Auto
    // pCfg->MCS                            = 1;

    // ######## Needs to be update to get actual value
    // pCfg->BasicRate = COSA_DML_WIFI_BASICRATE_Default;

    // Need to translate
#if 0
    {
        char maxBitRate[128];
        wifi_getRadioMaxBitRate(wlanIndex, maxBitRate);
        wifiDbgPrintf("%s: wifi_getRadioMaxBitRate returned %s\n", __FUNCTION__, maxBitRate);
        if (strcmp(maxBitRate,"Auto") == 0)
        {
            pCfg->TxRate = COSA_DML_WIFI_TXRATE_Auto;
            wifiDbgPrintf("%s: set to auto pCfg->TxRate = %d\n", __FUNCTION__, pCfg->TxRate);
        } else
        {
            char *pos;
            pos = strstr(maxBitRate, " ");
            if (pos != NULL) pos = 0;
            wifiDbgPrintf("%s: wifi_getRadioMaxBitRate returned %s\n", __FUNCTION__, maxBitRate);
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
    if (pCfg->X_CISCO_COM_AutoBlockAck != pStoredCfg->X_CISCO_COM_AutoBlockAck)
    {
       wifi_setRadioAutoBlockAckEnable(wlanIndex, pCfg->X_CISCO_COM_AutoBlockAck);
   }

	//>>Deprecated
	//if (pCfg->X_CISCO_COM_WirelessOnOffButton != pStoredCfg->X_CISCO_COM_WirelessOnOffButton)
    //{
    //    wifi_getWifiEnableStatus(wlanIndex, &pCfg->X_CISCO_COM_WirelessOnOffButton);
    //    printf("%s: called wifi_getWifiEnableStatus\n",__FUNCTION__);
    //}
    //<<
  
    if ( pCfg->ApplySetting == TRUE )
    {
        wifiDbgPrintf("%s: ApplySettings---!!!!!! \n",__FUNCTION__);
		
		//zqiu: >>
	
		if(gRadioRestartRequest[0] || gRadioRestartRequest[1]) {
fprintf(stderr, "----# %s %d gRadioRestartRequest=%d %d \n", __func__, __LINE__, gRadioRestartRequest[0], gRadioRestartRequest[1] );		
			wlanRestart=TRUE;
			gRadioRestartRequest[0]=FALSE;
			gRadioRestartRequest[1]=FALSE;
		}
		//<<
		if(wlanRestart == TRUE)
		{
            memcpy(&sWiFiDmlRadioStoredCfg[pCfg->InstanceNumber-1], pCfg, sizeof(COSA_DML_WIFI_RADIO_CFG));
            wifiDbgPrintf("\n%s: ***** RESTARTING RADIO !!! *****\n",__FUNCTION__);
			CcspWifiTrace(("RDK_LOG_WARN, RDKB_WIFI_CONFIG_CHANGED : %s RESTARTING RADIO !!! \n",__FUNCTION__)); 
            wifi_initRadio(wlanIndex);
			CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : %s RADIO Restarted !!! \n",__FUNCTION__)); 
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

#if defined(ENABLE_FEATURE_MESHWIFI)
		{
		    char cmd[256] = {0};
		    // notify mesh components that wifi radio settings changed
		    CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of Radio Config changes\n",__FUNCTION__));
		    sprintf(cmd, "/usr/bin/sysevent set wifi_RadioChannel \"RDK|%d|%d\"", pCfg->InstanceNumber-1, pCfg->Channel);
		    system(cmd);
		}
#endif
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
    if ( (wlanIndex < 0) || (wlanIndex >= RADIO_INDEX_MAX) )
    {
        return ANSC_STATUS_FAILURE;
    }

	wifi_getRadioChannel(wlanIndex, &pCfg->Channel);

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
    //BOOL DFSEnabled = FALSE;
    BOOL IGMPEnable = FALSE;
    char frequency[32];
    char channelMode[32];
    char opStandards[32];
    static BOOL firstTime[2] = { TRUE, true};
    static char temp1[256];

    if (!pCfg )
    {
        return ANSC_STATUS_FAILURE;
    }

    if ( CosaDmlWiFiGetRadioPsmData(pCfg) == ANSC_STATUS_FAILURE )
    {
        return ANSC_STATUS_FAILURE;
    }

    wlanIndex = (ULONG) pCfg->InstanceNumber-1;  
    if ( (wlanIndex < 0) || (wlanIndex >= RADIO_INDEX_MAX) )
    {
        return ANSC_STATUS_FAILURE;
    }
    
    if (firstTime[wlanIndex] == TRUE) {
        pCfg->LastChange             = AnscGetTickInSeconds(); 
        printf("%s: LastChange %d \n", __func__, pCfg->LastChange);
		CcspWifiTrace(("RDK_LOG_WARN,%s : LastChange %d!!!!!! \n",__FUNCTION__,pCfg->LastChange));
        firstTime[wlanIndex] = FALSE;
    }

    wifi_getRadioEnable(wlanIndex, &radioEnabled);
    pCfg->bEnabled = (radioEnabled == TRUE) ? 1 : 0;

    char frequencyBand[10];
    wifi_getRadioSupportedFrequencyBands(wlanIndex, frequencyBand);
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

//>> zqiu	
	CosaDmlWiFiGetRadioStandards(wlanIndex, pCfg->OperatingFrequencyBand, &pCfg->OperatingStandards);
//<<

	wifi_getRadioChannel(wlanIndex, &pCfg->Channel);
    

    wifi_getRadioDfsSupport(wlanIndex,&pCfg->X_COMCAST_COM_DFSSupport);
	wifi_getRadioDfsEnable(wlanIndex, &pCfg->X_COMCAST_COM_DFSEnable);

	wifi_getRadioDCSSupported(wlanIndex,&pCfg->X_COMCAST_COM_DCSSupported);
    wifi_getRadioDCSEnable(wlanIndex, &pCfg->X_COMCAST_COM_DCSEnable);
	
    wifi_getRadioIGMPSnoopingEnable(wlanIndex, &IGMPEnable);
    pCfg->X_COMCAST_COM_IGMPSnoopingEnable = (IGMPEnable == TRUE) ? 1 : 0;

	wifi_getRadioAutoChannelEnable(wlanIndex, &enabled); 
    pCfg->AutoChannelEnable = (enabled == TRUE) ? TRUE : FALSE;

	//zqiu: TODO: use hal to get AutoChannelRefreshPeriod
    pCfg->AutoChannelRefreshPeriod       = 3600;
    
    	wifi_getRadioAutoChannelRefreshPeriodSupported(wlanIndex,&pCfg->X_COMCAST_COM_AutoChannelRefreshPeriodSupported);
	
	wifi_getRadioIEEE80211hSupported(wlanIndex,&pCfg->X_COMCAST_COM_IEEE80211hSupported);
	
	wifi_getRadioReverseDirectionGrantSupported(wlanIndex,&pCfg->X_COMCAST_COM_ReverseDirectionGrantSupported);
	
	wifi_getApRtsThresholdSupported(wlanIndex,&pCfg->X_COMCAST_COM_RtsThresholdSupported);
	
	
	//zqiu: >>
    //wifi_getRadioStandard(wlanIndex, channelMode, &gOnly, &nOnly, &acOnly);
	char bandwidth[64];
	char extchan[64];
	wifi_getRadioOperatingChannelBandwidth(wlanIndex, bandwidth);
	if (strstr(bandwidth, "40MHz") != NULL) {
		wifi_getRadioExtChannel(wlanIndex, extchan);
        pCfg->OperatingChannelBandwidth = COSA_DML_WIFI_CHAN_BW_40M;
		if (strstr(extchan, "AboveControlChannel") != NULL) {
			pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Above;
		} else if (strstr(extchan, "BelowControlChannel") != NULL) {
			pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Below;
		} else {
			pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Auto;
		}
    } else if (strstr(bandwidth, "80MHz") != NULL) {
        pCfg->OperatingChannelBandwidth = COSA_DML_WIFI_CHAN_BW_80M;
        pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Auto;
    } else if (strstr(bandwidth, "160") != NULL) {
        pCfg->OperatingChannelBandwidth = COSA_DML_WIFI_CHAN_BW_160M;		
        pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Auto;
    } else if (strstr(bandwidth, "80+80") != NULL) {
        //pCfg->OperatingChannelBandwidth = COSA_DML_WIFI_CHAN_BW_8080M;	//Todo: add definition
        pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Auto;
    } else if (strstr(bandwidth, "20MHz") != NULL) {
        pCfg->OperatingChannelBandwidth = COSA_DML_WIFI_CHAN_BW_20M;
        pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Auto;
	}
	//zqiu: <<
	
    // Modulation Coding Scheme 0-15, value of -1 means Auto
    //pCfg->MCS                            = -1;
    wifi_getRadioMCS(wlanIndex, &pCfg->MCS);

    // got from CosaDmlWiFiGetRadioPsmData
    {
            if ( ( gRadioPowerState[wlanIndex] == COSA_DML_WIFI_POWER_UP) &&
                 ( gRadioNextPowerSetting != COSA_DML_WIFI_POWER_DOWN ) )
            {
				CcspWifiTrace(("RDK_LOG_WARN,%s : setTransmitPowerPercent  wlanIndex:%d TransmitPower:%d \n",__FUNCTION__,wlanIndex,pCfg->TransmitPower));
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

    //wifi_getCountryCode(wlanIndex, pCfg->RegulatoryDomain);
	snprintf(pCfg->RegulatoryDomain, 4, "US");
    //zqiu: RDKB-3346
        memset(temp1,0,sizeof(temp1));
        wifi_getRadioBasicDataTransmitRates(wlanIndex,temp1);
        if(strcmp(temp1,pCfg->BasicDataTransmitRates))
        {
                memset(pCfg->BasicDataTransmitRates,0,sizeof(pCfg->BasicDataTransmitRates));
                strncpy(pCfg->BasicDataTransmitRates,temp1,sizeof(temp1));
        }

	//RDKB-10526
	wifi_getRadioSupportedDataTransmitRates(wlanIndex,pCfg->SupportedDataTransmitRates);
	memset(temp1,0,sizeof(temp1));
        wifi_getRadioOperationalDataTransmitRates(wlanIndex,temp1);
	if(strcmp(temp1,pCfg->OperationalDataTransmitRates))
	{
		memset(pCfg->OperationalDataTransmitRates,0,sizeof(pCfg->OperationalDataTransmitRates));
        	strncpy(pCfg->OperationalDataTransmitRates,temp1,sizeof(temp1));
	}
	
    // ######## Needs to be update to get actual value
    pCfg->BasicRate = COSA_DML_WIFI_BASICRATE_Default;

    { 
        char maxBitRate[128]; 
        wifi_getRadioMaxBitRate(wlanIndex, maxBitRate); 

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

    wifi_getRadioAMSDUEnable(wlanIndex, &enabled);
    pCfg->X_CISCO_COM_AggregationMSDU = (enabled == TRUE) ? TRUE : FALSE;

    // BOOL                            X_CISCO_COM_AutoBlockAck;

    wifi_getRadioAutoBlockAckEnable(wlanIndex, &enabled);
    pCfg->X_CISCO_COM_AutoBlockAck = (enabled == TRUE) ? TRUE : FALSE;

    // BOOL                            X_CISCO_COM_DeclineBARequest;
    
/*    wifi_getRadioTxChainMask(wlanIndex, (int *) &pCfg->X_CISCO_COM_HTTxStream);
    wifi_getRadioRxChainMask(wlanIndex, (int *) &pCfg->X_CISCO_COM_HTRxStream);*/
    wifi_getRadioAutoChannelRefreshPeriodSupported(wlanIndex, &pCfg->AutoChannelRefreshPeriodSupported);
    wifi_getApRtsThresholdSupported(wlanIndex, &pCfg->RtsThresholdSupported);
    wifi_getRadioReverseDirectionGrantSupported(wlanIndex, &pCfg->ReverseDirectionGrantSupported);

	//>>Deprecated
    //wifi_getWifiEnableStatus(wlanIndex, &enabled);
    //pCfg->X_CISCO_COM_WirelessOnOffButton = (enabled == TRUE) ? TRUE : FALSE;
	//<<
	
    /* if any of the user-controlled SSID is up, turn on the WiFi LED */
    int i, MbssEnable = 0;
    BOOL vapEnabled = FALSE;
    for (i=wlanIndex; i < 16; i += 2)
    {
        wifi_getApEnable(i, &vapEnabled);
        if (vapEnabled == TRUE)
            MbssEnable += 1<<((i-wlanIndex)/2);
    }
    //zqiu: let driver control LED
	//wifi_setLED(wlanIndex, (MbssEnable & pCfg->MbssUserControl)?1:0);

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

    if (!pInfo || (ulInstanceNumber<1) || (ulInstanceNumber>RADIO_INDEX_MAX))
    {
        return ANSC_STATUS_FAILURE;
    }
    
    if (FALSE)
    {
        return returnStatus;
    }
    else
    {
        BOOL radioActive = TRUE;

        wifi_getRadioStatus(ulInstanceNumber-1,&radioActive);

		if( TRUE == radioActive )
		{
			pInfo->Status = COSA_DML_IF_STATUS_Up;
		}
		else
		{
			pInfo->Status = COSA_DML_IF_STATUS_Down;
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

    if (!pInfo || (ulInstanceNumber<1) || (ulInstanceNumber>RADIO_INDEX_MAX))
    {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pCfg is NULL \n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
    
    if (FALSE)
    {
        return returnStatus;
    }
    else
    {
        pInfo->ChannelsInUse[0] = 0;
        wifi_getRadioChannelsInUse(ulInstanceNumber-1, pInfo->ChannelsInUse);
        wifiDbgPrintf("%s ChannelsInUse = %s \n",__FUNCTION__, pInfo->ChannelsInUse);
		CcspWifiTrace(("RDK_LOG_WARN,%s : ChannelsInUse = %s \n",__FUNCTION__,pInfo->ChannelsInUse));
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
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pCfg is NULL \n",__FUNCTION__));
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
            //zqiu: TODO: repleaced with wifi_getNeighboringWiFiDiagnosticResult2()
			//wifi_scanApChannels(ulInstanceNumber-1, pInfo->ApChannelScan); 
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
	wifi_radioTrafficStats2_t 		radioTrafficStats;		
	//static ULONG LastChange = 0;	
	if ( ( AnscGetTickInSeconds() - pStats->StatisticsStartTime ) < 5 )	//Do not re pull within 5 sec
		return ANSC_STATUS_SUCCESS;
	//else 
    //	LastChange =  AnscGetTickInSeconds();
      	
    if ((ulInstanceNumber<1) || (ulInstanceNumber>RADIO_INDEX_MAX))
    {
        return ANSC_STATUS_FAILURE;
    }
	wifi_getRadioTrafficStats2(ulInstanceNumber-1, &radioTrafficStats);
	//zqiu: use the wifi_radioTrafficStats_t in phase3 wifi hal
	pStats->BytesSent				= radioTrafficStats.radio_BytesSent;
    pStats->BytesReceived			= radioTrafficStats.radio_BytesReceived;
    pStats->PacketsSent				= radioTrafficStats.radio_PacketsSent;
    pStats->PacketsReceived			= radioTrafficStats.radio_PacketsReceived;
    pStats->ErrorsSent				= radioTrafficStats.radio_ErrorsSent;
    pStats->ErrorsReceived			= radioTrafficStats.radio_ErrorsReceived;
    pStats->DiscardPacketsSent		= radioTrafficStats.radio_DiscardPacketsSent;
    pStats->DiscardPacketsReceived	= radioTrafficStats.radio_DiscardPacketsReceived;
	pStats->PLCPErrorCount			= radioTrafficStats.radio_PLCPErrorCount;
    pStats->FCSErrorCount			= radioTrafficStats.radio_FCSErrorCount;
    pStats->InvalidMACCount			= radioTrafficStats.radio_InvalidMACCount;
    pStats->PacketsOtherReceived	= radioTrafficStats.radio_PacketsOtherReceived;
	pStats->NoiseFloor				= radioTrafficStats.radio_NoiseFloor;

	pStats->ChannelUtilization				= radioTrafficStats.radio_ChannelUtilization;
	pStats->ActivityFactor					= radioTrafficStats.radio_ActivityFactor;
	pStats->CarrierSenseThreshold_Exceeded	= radioTrafficStats.radio_CarrierSenseThreshold_Exceeded;
	pStats->RetransmissionMetric			= radioTrafficStats.radio_RetransmissionMetirc;
	pStats->MaximumNoiseFloorOnChannel		= radioTrafficStats.radio_MaximumNoiseFloorOnChannel;
	pStats->MinimumNoiseFloorOnChannel		= radioTrafficStats.radio_MinimumNoiseFloorOnChannel;
	pStats->MedianNoiseFloorOnChannel		= radioTrafficStats.radio_MedianNoiseFloorOnChannel;
	pStats->StatisticsStartTime				= radioTrafficStats.radio_StatisticsStartTime;
	
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

    if (gSsidCount < 0)
    {
        gSsidCount = 1;
    }

    if (gSsidCount > WIFI_INDEX_MAX)
    {
        gSsidCount = WIFI_INDEX_MAX;
    }
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

    if (!pEntry || (ulIndex<0) || (ulIndex>=WIFI_INDEX_MAX)) return ANSC_STATUS_FAILURE;

    // sscanf(entry,"ath%d", &wlanIndex);
    wlanIndex = ulIndex;

    wifi_getApRadioIndex(wlanIndex, &wlanRadioIndex);

    /*Set default Name & Alias*/
    wifi_getApName(wlanIndex, pEntry->StaticInfo.Name);

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
        if (gSsidCount < WIFI_INDEX_MAX)
        {
            gSsidCount++;
        }
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
        if (gSsidCount > 0)
        {
            gSsidCount--;
        }
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
	char status[64];
    BOOL bEnabled;

wifiDbgPrintf("%s\n",__FUNCTION__);

    if (!pCfg)
    {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pCfg is NULL \n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
    wlanIndex = pCfg->InstanceNumber-1;
    if ( (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
        return ANSC_STATUS_FAILURE;
    }
    pStoredCfg = &sWiFiDmlSsidStoredCfg[pCfg->InstanceNumber-1];

    if (pCfg->bEnabled != pStoredCfg->bEnabled) {
		CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : %s Calling wifi_setEnable to enable/disable SSID on interface:  %d enable: %d \n",__FUNCTION__,wlanIndex,pCfg->bEnabled));
         int retStatus = wifi_setApEnable(wlanIndex, pCfg->bEnabled);
	     if(retStatus == 0) {
       		 CcspWifiTrace(("RDK_LOG_WARN,WIFI %s wifi_setApEnable success  index %d , %d",__FUNCTION__,wlanIndex,pCfg->bEnabled));
                 if (pCfg->InstanceNumber == 5 || pCfg->InstanceNumber == 6 || pCfg->InstanceNumber == 9 || pCfg->InstanceNumber == 10) {
                     CcspWifiTrace(("RDK_LOG_INFO,%d: XfinityWiFi SSID %d [%s]\n",AnscGetTickInSeconds(),pCfg->InstanceNumber,(pCfg->bEnabled ? "Enabled" : "Disabled")));
                 }
	     }
	   else {
        	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s wifi_setApEnable failed  index %d ,%d ",__FUNCTION__,wlanIndex,pCfg->bEnabled));
	   }
		//zqiu:flag radio >>
		if(pCfg->bEnabled) {
			wifi_getSSIDStatus(wlanIndex, status);
fprintf(stderr, "----# %s %d ath%d Status=%s \n", __func__, __LINE__, wlanIndex, status );		
			if(strcmp(status, "Enabled")!=0) {
fprintf(stderr, "----# %s %d gRadioRestartRequest[%d]=true \n", __func__, __LINE__, wlanIndex%2 );			
				gRadioRestartRequest[wlanIndex%2]=TRUE;
			}
		}
		//<<
        cfgChange = TRUE;
    }
    if (strcmp(pCfg->SSID, pStoredCfg->SSID) != 0) {
		CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : %s Calling wifi_setSSID to change SSID name on interface: %d SSID: %s \n",__FUNCTION__,wlanIndex,pCfg->SSID));
        wifi_setSSIDName(wlanIndex, pCfg->SSID);
        cfgChange = TRUE;
	CosaDmlWiFi_GetPreferPrivatePsmData(&bEnabled);
	if (bEnabled == TRUE)
	{
		if(wlanIndex==0 || wlanIndex==1)
		{
			Delete_Hotspot_MacFilt_Entries();
		}
	}
    }
       
    if (pCfg->EnableOnline != pStoredCfg->EnableOnline) {
#if !defined (_COSA_BCM_MIPS_)&& !defined(_COSA_BCM_ARM_)
        wifi_setApEnableOnLine(wlanIndex, pCfg->EnableOnline);  
#endif
        cfgChange = TRUE;
    }
	
	//zqiu: 
    if (pCfg->RouterEnabled != pStoredCfg->RouterEnabled) {
		CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : Calling wifi_setRouterEnable interface: %d SSID :%d \n",__FUNCTION__,wlanIndex,pCfg->RouterEnabled));
#if !defined (_COSA_BCM_MIPS_)&& !defined(_COSA_BCM_ARM_)
		wifi_setRouterEnable(wlanIndex, pCfg->RouterEnabled);
#endif
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
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pCfg is NULL \n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
    wlanIndex = pCfg->InstanceNumber-1;
    if ( (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
        return ANSC_STATUS_FAILURE;
    }
    wifiDbgPrintf("%s[%d] wlanIndex %d\n",__FUNCTION__, __LINE__, wlanIndex);
    CcspWifiTrace(("RDK_LOG_WARN,%s : wlanIndex %d \n",__FUNCTION__,wlanIndex));
    pRunningCfg = &sWiFiDmlSsidRunningCfg[wlanIndex];

    CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : SSID names [current,new]:[%s,%s] \n",__FUNCTION__,pRunningCfg->SSID,pCfg->SSID));
    if (strcmp(pCfg->SSID, pRunningCfg->SSID) != 0) {
        wifi_pushSSID(wlanIndex, pCfg->SSID);
    }

#if defined(ENABLE_FEATURE_MESHWIFI)
    // Notify Mesh components of SSID change
    {
        char cmd[256] = {0};

        CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of SSID change\n",__FUNCTION__));

        // notify mesh components that wifi ssid setting changed
        // index|ssid
        snprintf(cmd, sizeof(cmd)-1, "/usr/bin/sysevent set wifi_SSIDName \"RDK|%d|%s\"",
                wlanIndex,
                pCfg->SSID);
        system(cmd);
    }
#endif

    memcpy(&sWiFiDmlSsidRunningCfg[pCfg->InstanceNumber-1], pCfg, sizeof(COSA_DML_WIFI_SSID_CFG));
	CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Returning Success \n",__FUNCTION__));
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
    int wlanIndex = 0;
    int wlanRadioIndex;
    BOOL enabled = FALSE;
    static BOOL firstTime[16] = { TRUE, true, true, true, true, true, true, true, 
                                        TRUE, true, true, true, true, true, true, true };
    
    if (!pCfg)
    {
        CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pCfg is NULL \n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
    /*RDKB-6907, CID-32945, null check before use, avoid negative numbers*/
    if(pCfg->InstanceNumber > 0)
    {
        wlanIndex = pCfg->InstanceNumber-1;
    }
    else
    {
        wlanIndex = 0;
    }
    if (wlanIndex >= WIFI_INDEX_MAX)
    {
        return ANSC_STATUS_FAILURE;
    }
    wifiDbgPrintf("[%s] THE wlanIndex = %d\n",__FUNCTION__, wlanIndex);

    if (firstTime[wlanIndex] == TRUE) {
        pCfg->LastChange = AnscGetTickInSeconds(); 
        firstTime[wlanIndex] = FALSE;
    }

    wifi_getApRadioIndex(wlanIndex, &wlanRadioIndex);

//#if defined(_COSA_BCM_MIPS_) || defined(INTEL_PUMA7)
//	_ansc_sprintf(pCfg->Alias, "ath%d",wlanIndex);    //zqiu: need BCM to fix bug in wifi_getApName, then we could remove this code
//#else
	wifi_getApName(wlanIndex, pCfg->Alias);
//#endif    

    wifi_getApEnable(wlanIndex, &enabled);
    pCfg->bEnabled = (enabled == TRUE) ? TRUE : FALSE;

    //zqiu
    //_ansc_sprintf(pCfg->WiFiRadioName, "wifi%d",wlanRadioIndex);
    wifi_getRadioIfName(wlanRadioIndex, pCfg->WiFiRadioName);

    wifi_getSSIDName(wlanIndex, pCfg->SSID);

    getDefaultSSID(wlanIndex,pCfg->DefaultSSID);

#if !defined(_COSA_BCM_MIPS_)&& !defined(_COSA_BCM_ARM_)
    wifi_getApEnableOnLine(wlanIndex, &enabled);
#else
    wifi_getApEnable(wlanIndex, &enabled);
#endif
    pCfg->EnableOnline = (enabled == TRUE) ? TRUE : FALSE;

#if !defined(_COSA_BCM_MIPS_)&& !defined(_COSA_BCM_ARM_)
	//zqiu:
    wifi_getRouterEnable(wlanIndex, &enabled);
    pCfg->RouterEnabled = (enabled == TRUE) ? TRUE : FALSE;
	//pCfg->RouterEnabled = TRUE;
#else
    pCfg->RouterEnabled = TRUE;
#endif

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
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pCfg is NULL \n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
    
    if ( (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
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

        wifi_getApStatus(wlanIndex, vapStatus);

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
/*
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
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : ulInstanceNumber = %d \n",__FUNCTION__,ulInstanceNumber));
//>> zqiu  
	char mac[32];
	char status[32];
	
#if !defined(_COSA_BCM_MIPS_) && !defined(INTEL_PUMA7)
	wifi_getSSIDStatus(wlanIndex, status);
	if(strcmp(status,"Enabled")==0) {
		wifi_getApName(wlanIndex, pInfo->Name);
		wifi_getBaseBSSID(wlanIndex, bssid);
		wifi_getSSIDMACAddress(wlanIndex, mac);

		sMac_to_cMac(mac, &pInfo->MacAddress);
		sMac_to_cMac(bssid, &pInfo->BSSID);
	}  

#else
    sprintf(pInfo->Name,"ath%d", wlanIndex);

    memset(bssid,0,sizeof(bssid));
    memset(pInfo->BSSID,0,sizeof(pInfo->BSSID));
    memset(pInfo->MacAddress,0,sizeof(pInfo->MacAddress));

    wifi_getBaseBSSID(wlanIndex, bssid);
    wifi_getSSIDMACAddress(wlanIndex, mac);
    wifiDbgPrintf("%s: wifi_getBaseBSSID returned bssid = %s\n",__FUNCTION__, bssid);
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : wifi_getBaseBSSID returned bssid = %s \n",__FUNCTION__,bssid));

    wifiDbgPrintf("%s: wifi_getSSIDMACAddress returned mac = %s\n",__FUNCTION__, mac);
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : wifi_getSSIDMACAddress returned mac = %s \n",__FUNCTION__,mac));

    // 1st set the main radio mac/bssid
    sMac_to_cMac(mac, &pInfo->MacAddress);
	// The Bssid will be zeros if the radio is not up. This is a problem since we never refresh the cache
	// For now we will just set the bssid to the mac
   	// sMac_to_cMac(bssid, &pInfo->BSSID);
    sMac_to_cMac(mac, &pInfo->BSSID);

    wifiDbgPrintf("%s: BSSID %02x%02x%02x%02x%02x%02x\n", __func__,
       pInfo->BSSID[0], pInfo->BSSID[1], pInfo->BSSID[2],
       pInfo->BSSID[3], pInfo->BSSID[4], pInfo->BSSID[5]);
    wifiDbgPrintf("%s: MacAddress %02x%02x%02x%02x%02x%02x\n", __func__,
       pInfo->MacAddress[0], pInfo->MacAddress[1], pInfo->MacAddress[2],
       pInfo->MacAddress[3], pInfo->MacAddress[4], pInfo->MacAddress[5]);

	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : ath%d BSSID %02x%02x%02x%02x%02x%02x\n",__FUNCTION__,wlanIndex,pInfo->BSSID[0], pInfo->BSSID[1], pInfo->BSSID[2],pInfo->BSSID[3], pInfo->BSSID[4], pInfo->BSSID[5]));
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s :  ath%d MacAddress %02x%02x%02x%02x%02x%02x\n",__FUNCTION__,wlanIndex,pInfo->MacAddress[0], pInfo->MacAddress[1], pInfo->MacAddress[2],pInfo->MacAddress[3], pInfo->MacAddress[4], pInfo->MacAddress[5]));

	int i=0;
	// Now set the sub radio mac addresses and bssids
    for (i = wlanIndex+2; i < gSsidCount; i += 2) {
    	int ret;
        sprintf(gCachedSsidInfo[i].Name,"ath%d", i);

        wifi_getSSIDMACAddress(i, mac);
        sMac_to_cMac(mac, &gCachedSsidInfo[i].MacAddress);

        wifi_getBaseBSSID(i, bssid);
       	// sMac_to_cMac(bssid, &gCachedSsidInfo[i].BSSID);
		// The Bssid will be zeros if the radio is not up. This is a problem since we never refresh the cache
		// For now we will just set the bssid to the mac
       	sMac_to_cMac(mac, &gCachedSsidInfo[i].BSSID);

    	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : ath%d BSSID %02x%02x%02x%02x%02x%02x\n",__FUNCTION__,i,gCachedSsidInfo[i].BSSID[0], gCachedSsidInfo[i].BSSID[1], gCachedSsidInfo[i].BSSID[2],gCachedSsidInfo[i].BSSID[3], gCachedSsidInfo[i].BSSID[4], gCachedSsidInfo[i].BSSID[5]));
    	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : ath%d MacAddress %02x%02x%02x%02x%02x%02x\n",__FUNCTION__,i,gCachedSsidInfo[i].MacAddress[0], gCachedSsidInfo[i].MacAddress[1], gCachedSsidInfo[i].MacAddress[2],gCachedSsidInfo[i].MacAddress[3], gCachedSsidInfo[i].MacAddress[4], gCachedSsidInfo[i].MacAddress[5]));
    }
#endif
//<<
	CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Returning Success \n",__FUNCTION__));
    return ANSC_STATUS_SUCCESS;
}
*/
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
    if ( (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
        return ANSC_STATUS_FAILURE;
    }
    char bssid[64];

	//>>zqiu
//#if defined(_COSA_BCM_MIPS_) || defined(INTEL_PUMA7)
//	sprintf(pInfo->Name,"ath%d", wlanIndex);     //zqiu: need BCM to fix bug in wifi_getApName, then we could remove this code
//#else
	wifi_getApName(wlanIndex, pInfo->Name);
//#endif
	//memcpy(pInfo,&gCachedSsidInfo[wlanIndex],sizeof(COSA_DML_WIFI_SSID_SINFO));
	wifi_getBaseBSSID(wlanIndex, bssid);
	sMac_to_cMac(bssid, &pInfo->BSSID);
	sMac_to_cMac(bssid, &pInfo->MacAddress);  
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : %s BSSID %s\n",__FUNCTION__,pInfo->Name,bssid));
	//<<
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
    if ( (ulInstanceNumber < 1) || (ulInstanceNumber > WIFI_INDEX_MAX) )
    {
        return ANSC_STATUS_FAILURE;
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
    wifi_ssidTrafficStats2_t transStats;

    char status[16];
    BOOL enabled; 

    memset(pStats,0,sizeof(COSA_DML_WIFI_SSID_STATS));

    wifi_getApEnable(wlanIndex, &enabled);
    // Nothing to do if VAP is not enabled
    if (enabled == FALSE) {
        return ANSC_STATUS_SUCCESS; 
    }

    wifi_getApStatus(wlanIndex, status);
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

    result = wifi_getSSIDTrafficStats2(wlanIndex, &transStats);
    if (result == 0) {
        pStats->RetransCount                       = transStats.ssid_RetransCount;
        pStats->FailedRetransCount                 = transStats.ssid_FailedRetransCount;
        pStats->RetryCount	                       = transStats.ssid_RetryCount;
        pStats->MultipleRetryCount                 = transStats.ssid_MultipleRetryCount	;
        pStats->ACKFailureCount                    = transStats.ssid_ACKFailureCount;
        pStats->AggregatedPacketCount              = transStats.ssid_AggregatedPacketCount;
    }
    return ANSC_STATUS_SUCCESS;
}

BOOL CosaDmlWiFiSsidValidateSSID (void)
{
    BOOL validateFlag = FALSE;
    char *strValue = NULL;
    int retPsmGet = CCSP_SUCCESS;
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s Calling PSM GET\n",__FUNCTION__));
	retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, ValidateSSIDName, NULL, &strValue);
	if (retPsmGet == CCSP_SUCCESS) {
		CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : PSG GET Success \n",__FUNCTION__));
        validateFlag = _ansc_atoi(strValue);
	    ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
	}
	else
	{
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : PSM Get Failed \n",__FUNCTION__));
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

    if (!ssid) return ANSC_STATUS_FAILURE;
    ULONG wlanIndex = ulInstanceNumber-1;
    if ( (ulInstanceNumber < 1) || (ulInstanceNumber > WIFI_INDEX_MAX) )
    {
        return ANSC_STATUS_FAILURE;
    }
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s Calling wifi_getSSIDName ulInstanceNumber = %d\n",__FUNCTION__,ulInstanceNumber));
    wifi_getSSIDName(wlanIndex, ssid);
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s wlanIndex = %d ssid = %s\n",__FUNCTION__,ulInstanceNumber,ssid));
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
    if (!pSsid || !pEntry) return ANSC_STATUS_FAILURE;
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
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pCfg is NULL \n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
        
    int wlanIndex = pCfg->InstanceNumber-1;

    if ( (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
        return ANSC_STATUS_FAILURE;
    }
    wifi_setApWmmEnable(wlanIndex,pCfg->WMMEnable);
    wifi_setApWmmUapsdEnable(wlanIndex, pCfg->UAPSDEnable);        

    {
	// Ic and Og policies set the same for first GA release
    // setting Ack policy so negate the NoAck value
	wifi_setApWmmOgAckPolicy(wlanIndex, 0, !pCfg->WmmNoAck);
	wifi_setApWmmOgAckPolicy(wlanIndex, 1, !pCfg->WmmNoAck);
	wifi_setApWmmOgAckPolicy(wlanIndex, 2, !pCfg->WmmNoAck);
	wifi_setApWmmOgAckPolicy(wlanIndex, 3, !pCfg->WmmNoAck);
    }
    wifi_setApMaxAssociatedDevices(wlanIndex, pCfg->BssMaxNumSta);
    
    wifi_setApIsolationEnable(wlanIndex, pCfg->IsolationEnable);

    wifi_pushSsidAdvertisementEnable(wlanIndex, pCfg->SSIDAdvertisementEnabled);

#if defined(ENABLE_FEATURE_MESHWIFI)
    {
        char cmd[256] = {0};
        // notify mesh components that wifi SSID Advertise changed
        CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of SSID Advertise changes\n",__FUNCTION__));
        sprintf(cmd, "/usr/bin/sysevent set wifi_SSIDAdvertisementEnable \"RDK|%d|%s\"",
                wlanIndex, (pCfg->SSIDAdvertisementEnabled?"true":"false"));
        system(cmd);
    }
#endif

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
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pMfQueue is NULL \n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
    if ( (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
        return ANSC_STATUS_FAILURE;
    }
wifiDbgPrintf("%s : %d filters \n",__FUNCTION__, pMfQueue->Depth);
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s : %d filters \n",__FUNCTION__, pMfQueue->Depth));
    int index;
    for (index = 0; index < pMfQueue->Depth; index++) {
        pMacFiltLinkObj = (PCOSA_CONTEXT_LINK_OBJECT) AnscSListGetEntryByIndex((PSLIST_HEADER)pMfQueue, index);
        if (pMacFiltLinkObj) {
            pMacFilt =  (PCOSA_DML_WIFI_AP_MAC_FILTER) pMacFiltLinkObj->hContext;
            wifi_addApAclDevice(wlanIndex, pMacFilt->MACAddress);
#if defined(ENABLE_FEATURE_MESHWIFI)
            {
                char cmd[256] = {0};
                // notify mesh components that wifi SSID Advertise changed
                CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh to add device\n",__FUNCTION__));
                snprintf(cmd, sizeof(cmd)-1, "/usr/bin/sysevent set wifi_addApAclDevice \"RDK|%d|%s\"",
                        wlanIndex, pMacFilt->MACAddress);
                system(cmd);
            }
#endif
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
	CcspWifiTrace(("RDK_LOG_WARN,WIFI %s\n",__FUNCTION__));
    if (!pCfg || !pSsid)
    {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pCfg is NULL \n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
    pStoredCfg = &sWiFiDmlApStoredCfg[pCfg->InstanceNumber-1].Cfg;
        
    int wlanIndex = -1;
    int wlanRadioIndex = 0;

    int wRet = wifi_getIndexFromName(pSsid, &wlanIndex);
    if ( (wRet != RETURN_OK) || (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
	// Error could not find index
	CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : could not find index wlanIndex(-1)  \n",__FUNCTION__));
	return ANSC_STATUS_FAILURE;
    }
    wifi_getApRadioIndex(wlanIndex, &wlanRadioIndex);
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
    wifi_setApWmmEnable(wlanIndex,pCfg->WMMEnable);
    wifi_setApWmmUapsdEnable(wlanIndex, pCfg->UAPSDEnable);        

    {
	// Ic and Og policies set the same for first GA release
	wifi_setApWmmOgAckPolicy(wlanIndex, 0, pCfg->WmmNoAck);
	wifi_setApWmmOgAckPolicy(wlanIndex, 1, pCfg->WmmNoAck);
	wifi_setApWmmOgAckPolicy(wlanIndex, 2, pCfg->WmmNoAck);
	wifi_setApWmmOgAckPolicy(wlanIndex, 3, pCfg->WmmNoAck);
    }
    wifi_setApMaxAssociatedDevices(wlanIndex, pCfg->BssMaxNumSta);
    wifi_setApIsolationEnable(wlanIndex, pCfg->IsolationEnable);
    #endif

    if (pCfg->SSIDAdvertisementEnabled != pStoredCfg->SSIDAdvertisementEnabled) {
        wifi_setApSsidAdvertisementEnable(wlanIndex, pCfg->SSIDAdvertisementEnabled);
#if defined(ENABLE_FEATURE_MESHWIFI)
        {
            char cmd[256] = {0};
            // notify mesh components that wifi SSID Advertise changed
            CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of SSID Advertise changes\n",__FUNCTION__));
            sprintf(cmd, "/usr/bin/sysevent set wifi_SSIDAdvertisementEnable \"RDK|%d|%s\"",
                    wlanIndex, (pCfg->SSIDAdvertisementEnabled?"true":"false"));
            system(cmd);
        }
#endif
    }

    if (pCfg->MaxAssociatedDevices != pStoredCfg->MaxAssociatedDevices) {
        wifi_setApMaxAssociatedDevices(wlanIndex, pCfg->MaxAssociatedDevices);
    }
        if (pCfg->ManagementFramePowerControl != pStoredCfg->ManagementFramePowerControl) {
        wifi_setApManagementFramePowerControl(wlanIndex, pCfg->ManagementFramePowerControl);
        }
//>> zqiu	
    if (strcmp(pCfg->BeaconRate,pStoredCfg->BeaconRate)!=0) {
#ifdef _BEACONRATE_SUPPORT	
        wifi_setApBeaconRate(wlanIndex, pCfg->BeaconRate);
#endif		
    }
//<<
    if (pCfg->HighWatermarkThreshold != pStoredCfg->HighWatermarkThreshold) {
        wifi_setApAssociatedDevicesHighWatermarkThreshold(wlanIndex, pCfg->HighWatermarkThreshold);
    }
    // pCfg->MulticastRate = 123;
    // pCfg->BssCountStaAsCpe  = TRUE;

    if (pCfg->KickAssocDevices == TRUE) {
        CosaDmlWiFiApKickAssocDevices(pSsid);
        pCfg->KickAssocDevices = FALSE;
    }

 /*   if (pCfg->InterworkingEnable != pStoredCfg->InterworkingEnable) {
        wifi_setInterworkingServiceEnable(wlanIndex, pCfg->InterworkingEnable);
    }*/

    memcpy(&sWiFiDmlApStoredCfg[pCfg->InstanceNumber-1].Cfg, pCfg, sizeof(COSA_DML_WIFI_AP_CFG));
	CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Returning Success \n",__FUNCTION__));
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
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pCfg is NULL \n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
    wlanIndex = pCfg->InstanceNumber-1;
    if ( (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
        return ANSC_STATUS_FAILURE;
    }
    wifiDbgPrintf("%s[%d] wlanIndex %d\n",__FUNCTION__, __LINE__, wlanIndex);

    pRunningCfg = &sWiFiDmlApRunningCfg[pCfg->InstanceNumber-1].Cfg ;

    /* USGv2 Extensions */
    // static value for first GA release not settable
    // pCfg->LongRetryLimit = 16;
    // pCfg->RetryLimit     = 16;

    // These should be pushed when the SSID is up
    //  They are currently set from ApGetCfg when it call GetAccessPointPsmData
    if (pCfg->WMMEnable != pRunningCfg->WMMEnable) {
        wifi_setApWmmEnable(wlanIndex,pCfg->WMMEnable);
    }
    if (pCfg->UAPSDEnable != pRunningCfg->UAPSDEnable) {
        wifi_setApWmmUapsdEnable(wlanIndex, pCfg->UAPSDEnable);        
    }

    if (pCfg->WmmNoAck != pRunningCfg->WmmNoAck) {
        // Ic and Og policies set the same for first GA release
		wifi_setApWmmOgAckPolicy(wlanIndex, 0, !pCfg->WmmNoAck);
		wifi_setApWmmOgAckPolicy(wlanIndex, 1, !pCfg->WmmNoAck);
		wifi_setApWmmOgAckPolicy(wlanIndex, 2, !pCfg->WmmNoAck);
		wifi_setApWmmOgAckPolicy(wlanIndex, 3, !pCfg->WmmNoAck);
    }

    if (pCfg->BssMaxNumSta != pRunningCfg->BssMaxNumSta) {
        // Check to see the current # of associated devices exceeds the new limit
        // if so kick all the devices off
        unsigned int devNum;
        wifi_getApNumDevicesAssociated(wlanIndex, &devNum);
        if (devNum > pCfg->BssMaxNumSta) {
            char ssidName[COSA_DML_WIFI_MAX_SSID_NAME_LEN];
            wifi_getApName(wlanIndex, ssidName);
            CosaDmlWiFiApKickAssocDevices(ssidName);
        }
        wifi_setApMaxAssociatedDevices(wlanIndex, pCfg->BssMaxNumSta);
    }

    if (pCfg->IsolationEnable != pRunningCfg->IsolationEnable) {    
		wifi_setApIsolationEnable(wlanIndex, pCfg->IsolationEnable);
    }

    if (pCfg->SSIDAdvertisementEnabled != pRunningCfg->SSIDAdvertisementEnabled) {
        wifi_pushSsidAdvertisementEnable(wlanIndex, pCfg->SSIDAdvertisementEnabled);
        sWiFiDmlUpdatedAdvertisement[wlanIndex] = TRUE;
#if defined(ENABLE_FEATURE_MESHWIFI)
        {
            char cmd[256] = {0};
            // notify mesh components that wifi SSID Advertise changed
            CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of SSID Advertise changes\n",__FUNCTION__));
            sprintf(cmd, "/usr/bin/sysevent set wifi_SSIDAdvertisementEnable \"RDK|%d|%s\"",
                    wlanIndex, (pCfg->SSIDAdvertisementEnabled?"true":"false"));
            system(cmd);
        }
#endif
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
    
    if (!pSsid)
    {
        return ANSC_STATUS_FAILURE;
    }
    
wifiDbgPrintf("%s pSsid = %s\n",__FUNCTION__, pSsid);

    if (!pCfg)
    {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pCfg is NULL \n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
        
    int wlanIndex = -1;
    int wlanRadioIndex = 0;
    BOOL enabled = FALSE;

    int wRet = wifi_getIndexFromName(pSsid, &wlanIndex);
    if ( (wRet != RETURN_OK) || (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
	CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pSsid = %s Couldn't find wlanIndex \n",__FUNCTION__, pSsid));
	// Error could not find index
	return ANSC_STATUS_FAILURE;
    }
    wifi_getApRadioIndex(wlanIndex, &wlanRadioIndex);
    pCfg->InstanceNumber = wlanIndex+1;
    sprintf(pCfg->Alias,"AccessPoint%d", pCfg->InstanceNumber);

    wifi_getApEnable(wlanIndex, &enabled);

    pCfg->bEnabled = (enabled == TRUE) ? TRUE : FALSE;

    CosaDmlWiFiGetAccessPointPsmData(pCfg);

    /* USGv2 Extensions */
    // static value for first GA release not settable
    pCfg->LongRetryLimit = 16;
    pCfg->RetryLimit     = 16;

    sprintf(pCfg->SSID, "Device.WiFi.SSID.%d.", wlanIndex+1);

    wifi_getApSsidAdvertisementEnable(wlanIndex,  &enabled);
    pCfg->SSIDAdvertisementEnabled = (enabled == TRUE) ? TRUE : FALSE;
    wifi_getApManagementFramePowerControl(wlanIndex , &pCfg->ManagementFramePowerControl);

//>> zqiu
#ifdef _BEACONRATE_SUPPORT
	wifi_getApBeaconRate(wlanIndex, pCfg->BeaconRate);
#endif
//<<
/*    wifi_getInterworkingServiceCapability(wlanIndex,  &enabled);
    pCfg->InterworkingCapability = (enabled == TRUE) ? TRUE : FALSE;

    wifi_getInterworkingServiceEnable(wlanIndex,  &enabled);
    pCfg->InterworkingEnable = (enabled == TRUE) ? TRUE : FALSE; */

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
    int wlanIndex = -1;
    BOOL enabled = FALSE;

    if (!pSsid)
    {
        return ANSC_STATUS_FAILURE;
    }

wifiDbgPrintf("%s pSsid = %s\n",__FUNCTION__, pSsid);

    if (!pInfo)
    {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pCfg is NULL \n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
    
    int wRet = wifi_getIndexFromName(pSsid, &wlanIndex);
    if ( (wRet != RETURN_OK) || (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pSsid = %s Couldn't find wlanIndex \n",__FUNCTION__, pSsid));
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
CosaDmlWiFiApAssociatedDevicesHighWatermarkGetVal 
   (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_AP_CFG       pCfg
    )
{

    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    int wlanIndex = -1;
    ULONG maxDevices=0,highWatermarkThreshold=0,highWatermarkThresholdReached=0,highWatermark=0;
    
    if (!pSsid)
    {
        return ANSC_STATUS_FAILURE;
    }
    
	wifiDbgPrintf("%s pSsid = %s\n",__FUNCTION__, pSsid);


    if (!pCfg)
    {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pCfg is NULL \n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }

    int wRet = wifi_getIndexFromName(pSsid, &wlanIndex);
    if ( (wRet != RETURN_OK) || (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pSsid = %s Couldn't find wlanIndex \n",__FUNCTION__, pSsid));
        // Error could not find index
        return ANSC_STATUS_FAILURE;
    }


    wifi_getApMaxAssociatedDevices(wlanIndex,&maxDevices);
	pCfg->MaxAssociatedDevices = maxDevices;

    wifi_getApAssociatedDevicesHighWatermarkThreshold(wlanIndex,&highWatermarkThreshold);
	pCfg->HighWatermarkThreshold = highWatermarkThreshold;

    wifi_getApAssociatedDevicesHighWatermarkThresholdReached(wlanIndex,&highWatermarkThresholdReached);
	pCfg->HighWatermarkThresholdReached = highWatermarkThresholdReached;

    wifi_getApAssociatedDevicesHighWatermark(wlanIndex,&highWatermark);
	pCfg->HighWatermark = highWatermark;


    return ANSC_STATUS_SUCCESS;
}
//>>zqiu
/*
ANSC_STATUS
CosaDmlGetHighWatermarkDate
    (
       ANSC_HANDLE                 hContext,
        char*                       pSsid,
       char                       *pDate
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    int wlanIndex = -1,ret;
    struct tm  ts;
    wifiDbgPrintf("%s pSsid = %s\n",__FUNCTION__, pSsid);
    char buf[80];
    ULONG dateInSecs=0;

    int wRet = wifi_getIndexFromName(pSsid, &wlanIndex);
    if ( (wRet != RETURN_OK) || (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pSsid = %s Couldn't find wlanIndex \n",__FUNCTION__, pSsid));
        // Error could not find index
        return ANSC_STATUS_FAILURE;
    }

    ret=wifi_getApAssociatedDevicesHighWatermarkDate(wlanIndex,&dateInSecs);
    if ( ret != 0 )
    {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : fail to getApAssociatedDevicesHighWatermarkDate \n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }

    ts = *localtime(&dateInSecs);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ts);
    pDate=buf;
    return ANSC_STATUS_SUCCESS;
}
*/
ANSC_STATUS
CosaDmlGetApRadiusSettings
   (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_RadiusSetting       pRadiusSetting
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    int wlanIndex = -1;
  int result; 
     wifi_radius_setting_t radSettings; 
    if (!pSsid)
    {
        return ANSC_STATUS_FAILURE;
    }
     
    wifiDbgPrintf("%s pSsid = %s\n",__FUNCTION__, pSsid);

    if (!pRadiusSetting)
    {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pRadiusSetting is NULL \n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
    memset(pRadiusSetting,0,sizeof(PCOSA_DML_WIFI_RadiusSetting));

    int wRet = wifi_getIndexFromName(pSsid, &wlanIndex);
    if ( (wRet != RETURN_OK) || (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pSsid = %s Couldn't find wlanIndex \n",__FUNCTION__, pSsid));
        // Error could not find index
        return ANSC_STATUS_FAILURE;
    }
    result = wifi_getApSecurityRadiusSettings(wlanIndex, &radSettings);
    if (result == 0) {
        pRadiusSetting->iRadiusServerRetries                      	   = radSettings.RadiusServerRetries;
        pRadiusSetting->iRadiusServerRequestTimeout                	   = radSettings.RadiusServerRequestTimeout;
        pRadiusSetting->iPMKLifetime	                    		   = radSettings.PMKLifetime;
        pRadiusSetting->bPMKCaching			                   = radSettings.PMKCaching;
        pRadiusSetting->iPMKCacheInterval		                   = radSettings.PMKCacheInterval;
        pRadiusSetting->iMaxAuthenticationAttempts                         = radSettings.MaxAuthenticationAttempts;
        pRadiusSetting->iBlacklistTableTimeout                             = radSettings.BlacklistTableTimeout;
        pRadiusSetting->iIdentityRequestRetryInterval                      = radSettings.IdentityRequestRetryInterval;
        pRadiusSetting->iQuietPeriodAfterFailedAuthentication              = radSettings.QuietPeriodAfterFailedAuthentication;
    }
	return returnStatus;
}

ANSC_STATUS
CosaDmlSetApRadiusSettings
   (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_RadiusSetting       pRadiusSetting
    )
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    int wlanIndex = -1;
  int result;
     wifi_radius_setting_t radSettings;
    
    if (!pSsid)
    {
        return ANSC_STATUS_FAILURE;
    }
     
    wifiDbgPrintf("%s pSsid = %s\n",__FUNCTION__, pSsid);

    if (!pRadiusSetting)
    {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pRadiusSetting is NULL \n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
    memset(pRadiusSetting,0,sizeof(PCOSA_DML_WIFI_RadiusSetting));

    int wRet = wifi_getIndexFromName(pSsid, &wlanIndex);
    if ( (wRet != RETURN_OK) || (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pSsid = %s Couldn't find wlanIndex \n",__FUNCTION__, pSsid));
        // Error could not find index
        return ANSC_STATUS_FAILURE;
    }

	radSettings.RadiusServerRetries  	=	pRadiusSetting->iRadiusServerRetries;
	radSettings.RadiusServerRequestTimeout  = 	pRadiusSetting->iRadiusServerRequestTimeout;
	radSettings.PMKLifetime			=	pRadiusSetting->iPMKLifetime;
	radSettings.PMKCaching			=	pRadiusSetting->bPMKCaching;
	radSettings.PMKCacheInterval		=	pRadiusSetting->iPMKCacheInterval;
	radSettings.MaxAuthenticationAttempts	=	pRadiusSetting->iMaxAuthenticationAttempts;
	radSettings.BlacklistTableTimeout	=	pRadiusSetting->iBlacklistTableTimeout;
	radSettings.IdentityRequestRetryInterval=	pRadiusSetting->iIdentityRequestRetryInterval;
	radSettings.QuietPeriodAfterFailedAuthentication=pRadiusSetting->iQuietPeriodAfterFailedAuthentication;
	result = wifi_setApSecurityRadiusSettings(wlanIndex, &radSettings);
    if (result != 0) {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : fail to  setApSecurityRadiusSettings \n",__FUNCTION__));
		return ANSC_STATUS_FAILURE;	  
	}
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
    int wlanIndex = -1;

    if (!pSsid)
    {
        return ANSC_STATUS_FAILURE;
    }

wifiDbgPrintf("%s pSsid = %s\n",__FUNCTION__, pSsid);

    if (!pEntry)
    {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pEntry is NULL \n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }

#ifdef _XB6_PRODUCT_REQ_
    pEntry->Info.ModesSupported = COSA_DML_WIFI_SECURITY_None | 
				  COSA_DML_WIFI_SECURITY_WPA2_Personal | 
				  COSA_DML_WIFI_SECURITY_WPA2_Enterprise;
#else
    pEntry->Info.ModesSupported = COSA_DML_WIFI_SECURITY_None | COSA_DML_WIFI_SECURITY_WEP_64 | COSA_DML_WIFI_SECURITY_WEP_128 | 
				  //COSA_DML_WIFI_SECURITY_WPA_Personal | 
				  COSA_DML_WIFI_SECURITY_WPA2_Personal | 
				  COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal | 
				  //COSA_DML_WIFI_SECURITY_WPA_Enterprise |
				  COSA_DML_WIFI_SECURITY_WPA2_Enterprise | COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise;
#endif
    
    CosaDmlWiFiApSecGetCfg((ANSC_HANDLE)hContext, pSsid, &pEntry->Cfg);

    int wRet = wifi_getIndexFromName(pSsid, &wlanIndex);
    if ( (wRet != RETURN_OK) || (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pSsid = %s Couldn't find wlanIndex \n",__FUNCTION__, pSsid));
		// Error could not find index
		return ANSC_STATUS_FAILURE;
    }
	//>> Deprecated
	/*
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
	*/

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
    int wlanIndex = -1;
    char securityType[32];
    int retVal = 0;
    char authMode[32];

    if (!pSsid)
    {
        return ANSC_STATUS_FAILURE;
    }

wifiDbgPrintf("%s pSsid = %s\n",__FUNCTION__, pSsid);

    if (!pCfg)
    {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pEntry is NULL \n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
    
    int wRet = wifi_getIndexFromName(pSsid, &wlanIndex);
    if ( (wRet != RETURN_OK) || (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : pSsid = %s Couldn't find wlanIndex \n",__FUNCTION__, pSsid));
		// Error could not find index
		return ANSC_STATUS_FAILURE;
    }

    wifi_getApBeaconType(wlanIndex, securityType);
    retVal = wifi_getApBasicAuthenticationMode(wlanIndex, authMode);
    wifiDbgPrintf("wifi_getApBasicAuthenticationMode wanIndex = %d return code = %d for auth mode = %s\n",wlanIndex,retVal,authMode);

#ifndef _XB6_PRODUCT_REQ_
    if (strncmp(securityType,"None", strlen("None")) == 0)
    {
		pCfg->ModeEnabled =  COSA_DML_WIFI_SECURITY_None; 
    } else if (strncmp(securityType,"Basic", strlen("Basic")) == 0 )   { 
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
        if(strncmp(authMode,"EAPAuthentication", strlen("EAPAuthentication")) == 0)
        {
            pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise;
        }
        else
        {
	    pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal;
        }
    } else if (strncmp(securityType,"WPA", strlen("WPA")) == 0)
    {
        if(strncmp(authMode,"EAPAuthentication", strlen("EAPAuthentication")) == 0)
        {
            pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_WPA_Enterprise;
        }
        else
        {
	    pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_WPA_Personal;
        }
    } else if (strncmp(securityType,"11i", strlen("11i")) == 0)
    {
        if(strncmp(authMode,"EAPAuthentication", strlen("EAPAuthentication")) == 0)
        {
            pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_WPA2_Enterprise;
        }
        else
        {
	    pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_WPA2_Personal;
        }
    } else
    { 
	pCfg->ModeEnabled =  COSA_DML_WIFI_SECURITY_None; 
    }
#else
	if (strncmp(securityType,"None", strlen("None")) == 0)
    {
		pCfg->ModeEnabled =  COSA_DML_WIFI_SECURITY_None; 
    } else if (strncmp(securityType,"11i", strlen("11i")) == 0)
    {
        if(strncmp(authMode,"EAPAuthentication", strlen("EAPAuthentication")) == 0)
        {
            pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_WPA2_Enterprise;
        }
        else
        {
	    pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_WPA2_Personal;
        }
    } else
    { 
	pCfg->ModeEnabled =  COSA_DML_WIFI_SECURITY_None; 
    }
#endif
    //>>Deprecated
    //wifi_getApWepKeyIndex(wlanIndex, (unsigned int *) &pCfg->DefaultKey);
	//<<
    wifi_getApSecurityPreSharedKey(wlanIndex, pCfg->PreSharedKey);
    wifi_getApSecurityKeyPassphrase(wlanIndex, pCfg->KeyPassphrase);

    { 
	char method[32];

	wifi_getApWpaEncryptionMode(wlanIndex, method);

	if (strncmp(method, "TKIPEncryption",strlen("TKIPEncryption")) == 0)
	{ 
	    pCfg->EncryptionMethod = COSA_DML_WIFI_AP_SEC_TKIP;
	} else if (strncmp(method, "AESEncryption",strlen("AESEncryption")) == 0)
	{
	    pCfg->EncryptionMethod = COSA_DML_WIFI_AP_SEC_AES;
	} 
#ifndef _XB6_PRODUCT_REQ_	
	else if (strncmp(method, "TKIPandAESEncryption",strlen("TKIPandAESEncryption")) == 0)
	{
	    pCfg->EncryptionMethod = COSA_DML_WIFI_AP_SEC_AES_TKIP;
	}
#endif
    }

    getDefaultPassphase(wlanIndex,pCfg->DefaultKeyPassphrase);
    //wifi_getApSecurityRadiusServerIPAddr(wlanIndex,&pCfg->RadiusServerIPAddr); //bug
    //wifi_getApSecurityRadiusServerPort(wlanIndex, &pCfg->RadiusServerPort);
#if !defined (_COSA_BCM_MIPS_)&& !defined(_COSA_BCM_ARM_)
    wifi_getApSecurityWpaRekeyInterval(wlanIndex,  (unsigned int *) &pCfg->RekeyingInterval);
#endif 
    wifi_getApSecurityRadiusServer(wlanIndex, pCfg->RadiusServerIPAddr, &pCfg->RadiusServerPort, pCfg->RadiusSecret);
    wifi_getApSecuritySecondaryRadiusServer(wlanIndex, pCfg->SecondaryRadiusServerIPAddr, &pCfg->SecondaryRadiusServerPort, pCfg->SecondaryRadiusSecret);
	//zqiu: TODO: set pCfg->RadiusReAuthInterval;    
    
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

    int wlanIndex = -1;
    char securityType[32];
    char authMode[32];
    BOOL bEnabled;

    if (!pCfg || !pSsid)
    {
        return ANSC_STATUS_FAILURE;
    }
    
    int wRet = wifi_getIndexFromName(pSsid, &wlanIndex);
    if ( (wRet != RETURN_OK) || (wlanIndex <0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
	// Error could not find index
	return ANSC_STATUS_FAILURE;
    }
    pStoredCfg = &sWiFiDmlApSecurityStored[wlanIndex].Cfg;

    if (pCfg->ModeEnabled != pStoredCfg->ModeEnabled) {
		
#ifndef _XB6_PRODUCT_REQ_
        if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_None) 
       {
            strcpy(securityType,"None");
            strcpy(authMode,"None");
            CcspWifiEventTrace(("RDK_LOG_NOTICE, Wifi security mode None is Enabled\n"));
            CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : Wifi security mode None is Enabled\n"));
        } 
		//>>Deprecated
		/*
		else if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_64 ||
                   pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_128 ) 
        {
            ULONG wepLen;
            char *strValue = NULL;
            char recName[256];
            int retPsmSet = CCSP_SUCCESS;

            strcpy(securityType,"Basic");
            strcpy(authMode,"None");
			CcspWifiTrace(("RDK_LOG_WARN, RDKB_WIFI_CONFIG_CHANGED : %s setBasicEncryptionMode for %s \n",__FUNCTION__,pSsid));
            wifi_setApBasicEncryptionMode(wlanIndex, "WEPEncryption");

            memset(recName, 0, sizeof(recName));
            sprintf(recName, WepKeyLength, wlanIndex+1);
            if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_64) 
           {
                PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "64" );
                CcspWifiEventTrace(("RDK_LOG_NOTICE, Wifi security mode WEP-64 is Enabled\n"));
                CcspWifiTrace(("RDK_LOG_WARN, RDKB_WIFI_CONFIG_CHANGED : Wifi security mode WEP-64 is Enabled\n"));
            } else if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WEP_128)
            {
                PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "128" );
                CcspWifiEventTrace(("RDK_LOG_NOTICE, Wifi security mode WEP-128 is Enabled\n"));
                CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : Wifi security mode WEP-128 is Enabled\n"));
            }

        }
		*/
		else if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal) 
        {
            strcpy(securityType,"WPAand11i");
            strcpy(authMode,"PSKAuthentication");
            CcspWifiEventTrace(("RDK_LOG_NOTICE, Wifi security mode WPA-WPA2-Personal is Enabled\n"));
            CcspWifiTrace(("RDK_LOG_WARN, RDKB_WIFI_CONFIG_CHANGED : Wifi security mode WPA-WPA2-Personal is Enabled\n"));
        } else if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_Personal) 
        {
            strcpy(securityType,"WPA");
            strcpy(authMode,"PSKAuthentication");
            CcspWifiEventTrace(("RDK_LOG_NOTICE, Wifi security mode WPA-Personal is Enabled\n"));
            CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : Wifi security mode WPA-Personal is Enabled\n"));
        } else if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Personal) 
        {
            strcpy(securityType,"11i");
            strcpy(authMode,"PSKAuthentication");
            CcspWifiEventTrace(("RDK_LOG_NOTICE, Wifi security mode WPA2-Personal is Enabled\n"));
            CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : Wifi security mode WPA2-Personal is Enabled\n"));
		} else if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Enterprise) 
        {
			//zqiu: Add radius support
            strcpy(securityType,"11i");
            strcpy(authMode,"EAPAuthentication");
            CcspWifiEventTrace(("RDK_LOG_NOTICE, Wifi security mode WPA2_Enterprise is Enabled\n"));
            CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : Wifi security mode WPA2_Enterprise is Enabled\n"));
		} else if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise) 
        {
			//zqiu: Add Radius support
            strcpy(securityType,"WPAand11i");
            strcpy(authMode,"EAPAuthentication");
            CcspWifiEventTrace(("RDK_LOG_NOTICE, Wifi security mode WPA_WPA2_Enterprise is Enabled\n"));
            CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : Wifi security mode WPA_WPA2_Enterprise is Enabled\n"));
        } else
        {
            strcpy(securityType,"None");
            strcpy(authMode,"None");
            CcspWifiEventTrace(("RDK_LOG_NOTICE, Wifi security mode None is Enabled\n"));
            CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : Wifi security mode None is Enabled\n"));
        }
#else
		if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_None) 
       {
            strcpy(securityType,"None");
            strcpy(authMode,"None");
            CcspWifiEventTrace(("RDK_LOG_NOTICE, Wifi security mode None is Enabled\n"));
            CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : Wifi security mode None is Enabled\n"));
        } 
		else if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Personal) 
        {
            strcpy(securityType,"11i");
            strcpy(authMode,"PSKAuthentication");
            CcspWifiEventTrace(("RDK_LOG_NOTICE, Wifi security mode WPA2-Personal is Enabled\n"));
            CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : Wifi security mode WPA2-Personal is Enabled\n"));
		} else if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Personal) 
        {
            strcpy(securityType,"11i");
            strcpy(authMode,"PSKAuthentication");
            CcspWifiEventTrace(("RDK_LOG_NOTICE, Wifi security mode WPA2-Personal is Enabled\n"));
            CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : Wifi security mode WPA2-Personal is Enabled\n"));
		} else if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Enterprise) 
        {
			//zqiu: Add radius support
            strcpy(securityType,"11i");
            strcpy(authMode,"EAPAuthentication");
            CcspWifiEventTrace(("RDK_LOG_NOTICE, Wifi security mode WPA2_Enterprise is Enabled\n"));
            CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : Wifi security mode WPA2_Enterprise is Enabled\n"));
		} else
        {
            strcpy(securityType,"None");
            strcpy(authMode,"None");
            CcspWifiEventTrace(("RDK_LOG_NOTICE, Wifi security mode None is Enabled\n"));
            CcspWifiTrace(("RDK_LOG_WARN,RDKB_WIFI_CONFIG_CHANGED : Wifi security mode None is Enabled\n"));
        }
#endif
        wifi_setApBeaconType(wlanIndex, securityType);
		CcspWifiTrace(("RDK_LOG_WARN,\n%s calling setBasicAuthenticationMode ssid : %s authmode : %s \n",__FUNCTION__,pSsid,authMode));
        wifi_setApBasicAuthenticationMode(wlanIndex, authMode);
    }
	//>>Deprecated
    /*
    if (pCfg->DefaultKey != pStoredCfg->DefaultKey) {
		CcspWifiTrace(("RDK_LOG_WARN,\n RDKB_WIFI_CONFIG_CHANGED : %s calling setApWepKeyIndex Index : %d DefaultKey  : %s \n",__FUNCTION__,wlanIndex,pCfg->DefaultKey));
        wifi_setApWepKeyIndex(wlanIndex, pCfg->DefaultKey);
    }
	*/
	//<<
    if (strcmp(pCfg->PreSharedKey,pStoredCfg->PreSharedKey) != 0) {
        if (strlen(pCfg->PreSharedKey) > 0) { 
		CcspWifiTrace(("RDK_LOG_WARN,\n RDKB_WIFI_CONFIG_CHANGED : %s preshared key changed for index = %d   \n",__FUNCTION__,wlanIndex));
        wifi_setApSecurityPreSharedKey(wlanIndex, pCfg->PreSharedKey);
        }
    }

    if (strcmp(pCfg->KeyPassphrase, pStoredCfg->KeyPassphrase) != 0) {
        if (strlen(pCfg->KeyPassphrase) > 0) { 
		CcspWifiTrace(("RDK_LOG_WARN,\n RDKB_WIFI_CONFIG_CHANGED : %s KeyPassphrase changed for index = %d   \n",__FUNCTION__,wlanIndex));
        wifi_setApSecurityKeyPassphrase(wlanIndex, pCfg->KeyPassphrase);
        CcspWifiEventTrace(("RDK_LOG_NOTICE, KeyPassphrase changed \n "));
        CcspWifiTrace(("RDK_LOG_WARN, KeyPassphrase changed \n "));

	CosaDmlWiFi_GetPreferPrivatePsmData(&bEnabled);
	if (bEnabled == TRUE)
	{
		if(wlanIndex==0 || wlanIndex==1)
		{
			Delete_Hotspot_MacFilt_Entries();
		}
	}
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
	} 
#ifndef _XB6_PRODUCT_REQ_	
	else if ( pCfg->EncryptionMethod == COSA_DML_WIFI_AP_SEC_AES_TKIP)
	{
            strcpy(method,"TKIPandAESEncryption");
	}
#endif
		CcspWifiTrace(("RDK_LOG_WARN,\n RDKB_WIFI_CONFIG_CHANGED :%s Encryption method changed ,calling setWpaEncryptionMode Index : %d mode : %s \n",__FUNCTION__,wlanIndex,method));
		wifi_setApWpaEncryptionMode(wlanIndex, method);
    } 

#if !defined (_COSA_BCM_MIPS_)&& !defined(_COSA_BCM_ARM_)
    if ( pCfg->RekeyingInterval != pStoredCfg->RekeyingInterval) {
		CcspWifiTrace(("RDK_LOG_WARN,\n%s calling setWpaRekeyInterval  \n",__FUNCTION__));
        wifi_setApSecurityWpaRekeyInterval(wlanIndex,  pCfg->RekeyingInterval);
    }
#endif

    if ( strcmp(pCfg->RadiusServerIPAddr, pStoredCfg->RadiusServerIPAddr) !=0 || 
		pCfg->RadiusServerPort != pStoredCfg->RadiusServerPort || 
		strcmp(pCfg->RadiusSecret, pStoredCfg->RadiusSecret) !=0) {
		CcspWifiTrace(("RDK_LOG_WARN,\n%s calling wifi_setApSecurityRadiusServer  \n",__FUNCTION__));        
		wifi_setApSecurityRadiusServer(wlanIndex, pCfg->RadiusServerIPAddr, pCfg->RadiusServerPort, pCfg->RadiusSecret);
    }

	if ( strcmp(pCfg->SecondaryRadiusServerIPAddr, pStoredCfg->SecondaryRadiusServerIPAddr) !=0 || 
		pCfg->SecondaryRadiusServerPort != pStoredCfg->SecondaryRadiusServerPort || 
		strcmp(pCfg->SecondaryRadiusSecret, pStoredCfg->SecondaryRadiusSecret) !=0) {
		CcspWifiTrace(("RDK_LOG_WARN,\n%s calling wifi_setApSecurityRadiusServer  \n",__FUNCTION__));
		wifi_setApSecuritySecondaryRadiusServer(wlanIndex, pCfg->SecondaryRadiusServerIPAddr, pCfg->SecondaryRadiusServerPort, pCfg->SecondaryRadiusSecret);
	}
 
	if( pCfg->bReset == TRUE )
	{
		/* Reset the value after do the operation */
		wifi_setApSecurityReset( wlanIndex );
		pCfg->bReset  = FALSE;		
        CcspWifiTrace(("RDK_LOG_WARN,\n%s WiFi security settings are reset to their factory default values \n ",__FUNCTION__));
	}

	//zqiu: TODO: set pCfg->RadiusReAuthInterval;     
    

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

//>> Deprecated
/*	
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
		CcspWifiTrace(("RDK_LOG_WARN,\n%s : pushWepKey wlanIndex %d : DefualtKey %s :  \n",__FUNCTION__,wlanIndex,pCfg->DefaultKey));
        wifi_pushWepKey( wlanIndex, pCfg->DefaultKey);
        #if 0
        int i;
        for (i = 1; i <= 4; i++)
        {
        }
        wifi_setAuthMode(wlanIndex, 4);
        #endif

    }
*/
//<<
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
    wifi_removeApSecVaribles(wlanIndex);

    // Reset security to off 
    wifiDbgPrintf("%s %d Set encryptionOFF to reset security \n",__FUNCTION__, __LINE__);
	CcspWifiTrace(("RDK_LOG_WARN,\n%s : Set encryptionOFF to reset security \n",__FUNCTION__));
    wifi_disableApEncryption(wlanIndex);

    // If the Running config has security = WPA or None hostapd must be restarted
    if ( (pRunningCfg->ModeEnabled >= COSA_DML_WIFI_SECURITY_WPA_Personal && 
          pRunningCfg->ModeEnabled <= COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise) ||
         (pRunningCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_None) )
    {
        wifiDbgPrintf("%s %d sWiFiDmlRestartHostapd set to TRUE\n",__FUNCTION__, __LINE__);
		CcspWifiTrace(("RDK_LOG_WARN,\n%s : sWiFiDmlRestartHostapd set to TRUE \n",__FUNCTION__));
        sWiFiDmlRestartHostapd = TRUE;
    } else {
        // If the new config has security = WPA or None hostapd must be restarted
        if ( (pCfg->ModeEnabled >= COSA_DML_WIFI_SECURITY_WPA_Personal && 
              pCfg->ModeEnabled <= COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise) ||
             (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_None) )
        {
            wifiDbgPrintf("%s %d sWiFiDmlRestartHostapd set to TRUE\n",__FUNCTION__, __LINE__);
			CcspWifiTrace(("RDK_LOG_WARN,\n%s : sWiFiDmlRestartHostapd set to TRUE \n",__FUNCTION__));
            sWiFiDmlRestartHostapd = TRUE;
        }
    }

    if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_None) 
    {
        BOOL enableWps = FALSE;
        wifi_getApWpsEnable(wlanIndex, &enableWps);
        
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
        
            // Only recreate hostapd file if Radio and SSID are enabled
            if ( sWiFiDmlRadioStoredCfg[checkIndex].bEnabled == TRUE && sWiFiDmlSsidStoredCfg[checkIndex].bEnabled )
            {
                // if the other Primary SSID (ath0/ath1) has security set to WPA or None with WPS enabled, WPS must be turned off
                if ( ( (sWiFiDmlApSecurityStored[checkIndex].Cfg.ModeEnabled >= COSA_DML_WIFI_SECURITY_WPA_Personal) ||
                       (sWiFiDmlApSecurityStored[checkIndex].Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_None) ) &&
                     (pRunningCfg->ModeEnabled >= COSA_DML_WIFI_SECURITY_WPA2_Personal) )
                {
                    BOOL enableWps = FALSE;
                    wifi_getApWpsEnable(checkIndex, &enableWps);

                    if (enableWps == TRUE)
                    {
                        wifi_removeApSecVaribles(checkIndex);
                        wifi_createHostApdConfig(checkIndex, FALSE);

                        wifiDbgPrintf("%s %d sWiFiDmlRestartHostapd set to TRUE\n",__FUNCTION__, __LINE__);
		    			CcspWifiTrace(("RDK_LOG_WARN,\n%s : sWiFiDmlRestartHostapd set to TRUE \n",__FUNCTION__));
                        sWiFiDmlRestartHostapd = TRUE;
                    }
                }
            }
        }
        
        sWiFiDmlPushWepKeys[wlanIndex] = TRUE;
        
        }
#ifdef _XB6_PRODUCT_REQ_
	else if ( pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Personal ||
               pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Enterprise )
#else
	else if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_Personal ||
               pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Personal ||
               pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal ||
               pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_Enterprise || 
               pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Enterprise ||
               pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise )
#endif
    {
        // WPA
        BOOL enableWps = FALSE;
        wifi_getApWpsEnable(wlanIndex, &enableWps);
        
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

            // Only recreate hostapd file if Radio and SSID are enabled
            if ( sWiFiDmlRadioStoredCfg[checkIndex].bEnabled == TRUE && sWiFiDmlSsidStoredCfg[checkIndex].bEnabled )
            {
                // If the other SSID is running WPA recreate the config file
                if ( sWiFiDmlApSecurityStored[checkIndex].Cfg.ModeEnabled >= COSA_DML_WIFI_SECURITY_WPA_Personal ) 
                {
                    wifi_getApWpsEnable(wlanIndex, &enableWps);
                
                    if (sWiFiDmlApStoredCfg[0].Cfg.SSIDAdvertisementEnabled == FALSE || 
                        sWiFiDmlApStoredCfg[1].Cfg.SSIDAdvertisementEnabled == FALSE) 
                    { 
                        enableWps = FALSE;
                    }
                    printf("%s: recreating sec file enableWps = %s \n" , __FUNCTION__, ( enableWps == FALSE ) ? "FALSE" : "TRUE");
                    wifi_removeApSecVaribles(checkIndex);
                    wifi_createHostApdConfig(checkIndex, enableWps);
                }
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
    wifi_removeApSecVaribles(wlanIndex);

    if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_None)
   {
        BOOL enableWps = FALSE;
        wifi_getApWpsEnable(wlanIndex, &enableWps);
        
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

            // Only recreate hostapd file if Radio and SSID are enabled
            if ( sWiFiDmlRadioStoredCfg[checkIndex].bEnabled == TRUE && sWiFiDmlSsidStoredCfg[checkIndex].bEnabled )
            {
                // if the other Primary SSID (ath0/ath1) has security set to WPA or None with WPS enabled, WPS must be turned off
                if ( (sWiFiDmlApSecurityStored[checkIndex].Cfg.ModeEnabled >= COSA_DML_WIFI_SECURITY_WPA_Personal) ||
                       (sWiFiDmlApSecurityStored[checkIndex].Cfg.ModeEnabled == COSA_DML_WIFI_SECURITY_None) ) 
                {
                    BOOL enableWps = FALSE;
                    wifi_getApWpsEnable(checkIndex, &enableWps);
                
                    if (enableWps == TRUE)
                    {
                        wifi_removeApSecVaribles(checkIndex);
                        wifi_disableApEncryption(checkIndex);

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
        }

        sWiFiDmlPushWepKeys[wlanIndex] = TRUE;

    } 
#ifdef _XB6_PRODUCT_REQ_
	else  if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Personal || 
                pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Enterprise )
#else
	else  if (pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_Personal ||
                pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Personal ||
                pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal ||
                pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_Enterprise || 
                pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA2_Enterprise ||
                pCfg->ModeEnabled == COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise )
#endif
    { 
        // WPA
        BOOL enableWps = FALSE;
        wifi_getApWpsEnable(wlanIndex, &enableWps);
        
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
    int wlanIndex = -1;
    
    if (!pEntry || !pSsid)
    {
        return ANSC_STATUS_FAILURE;
    }
    PCOSA_DML_WIFI_APWPS_CFG  pCfg = &pEntry->Cfg;
    PCOSA_DML_WIFI_APWPS_INFO pInfo = &pEntry->Info;
  
    int wRet = wifi_getIndexFromName(pSsid, &wlanIndex);
    if ( (wRet != RETURN_OK) || (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
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
    int wlanIndex = -1;
    char methodsEnabled[64];
    char recName[256];
    char strValue[32];
    int retPsmSet = CCSP_SUCCESS;

wifiDbgPrintf("%s\n",__FUNCTION__);

    if (!pCfg || !pSsid)
    {
        return ANSC_STATUS_FAILURE;
    }


    int wRet = wifi_getIndexFromName(pSsid, &wlanIndex);
    if ((wRet != RETURN_OK) || wlanIndex < 0 || wlanIndex >= WIFI_INDEX_MAX) 
    {
        // Error could not find index
        return ANSC_STATUS_FAILURE;
    }
    pStoredCfg = &sWiFiDmlApWpsStored[wlanIndex].Cfg;

    if (pCfg->bEnabled != pStoredCfg->bEnabled)
    {
        unsigned int pin;
        wifi_getApWpsDevicePIN(wlanIndex, &pin);

//#if defined(_COSA_BCM_MIPS_)|| defined(_COSA_BCM_ARM_)
//        wifi_setApWpsEnable(wlanIndex, pCfg->bEnabled);
//#else
//        if (pCfg->bEnabled == TRUE && pin != 0)
//        {
//            wifi_setApWpsEnable(wlanIndex, 2);
//        } else
//        {
            wifi_setApWpsEnable(wlanIndex, pCfg->bEnabled);
//        }
//#endif
    }

    if (pCfg->ConfigMethodsEnabled != pStoredCfg->ConfigMethodsEnabled)
    {
	// Label and Display should always be set.  The currently DML enum does not include them, but they must be 
	// set on the Radio
	if ( pCfg->ConfigMethodsEnabled == COSA_DML_WIFI_WPS_METHOD_PushButton ) {
	    wifi_setApWpsConfigMethodsEnabled(wlanIndex,"PushButton");
	} else  if (pCfg->ConfigMethodsEnabled == COSA_DML_WIFI_WPS_METHOD_Pin) {
	    wifi_setApWpsConfigMethodsEnabled(wlanIndex,"Keypad,Label,Display");
	} else if ( pCfg->ConfigMethodsEnabled == (COSA_DML_WIFI_WPS_METHOD_PushButton|COSA_DML_WIFI_WPS_METHOD_Pin) ) {
	    wifi_setApWpsConfigMethodsEnabled(wlanIndex,"PushButton,Keypad,Label,Display");
	} 
    } 
    
    if (pCfg->WpsPushButton != pStoredCfg->WpsPushButton) {

        snprintf(recName, sizeof(recName) - 1, WpsPushButton, wlanIndex+1);
        snprintf(strValue, sizeof(strValue) - 1,"%d", pCfg->WpsPushButton);
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
		wifi_setApWpsEnrolleePin(wlanIndex,pCfg->X_CISCO_COM_ClientPin);
    } else if (pCfg->X_CISCO_COM_ActivatePushButton == TRUE) {
        wifi_setApWpsButtonPush(wlanIndex);
    } else if (pCfg->X_CISCO_COM_CancelSession == TRUE) {
        wifi_cancelApWPS(wlanIndex);
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
    int wlanIndex = -1;
    char methodsEnabled[64];
    char recName[256];
    char *strValue = NULL;
    int retPsmGet = CCSP_SUCCESS;

wifiDbgPrintf("%s\n",__FUNCTION__);
    
    if (!pCfg || !pSsid)
    {
        return ANSC_STATUS_FAILURE;
    }

    int wRet = wifi_getIndexFromName(pSsid, &wlanIndex);
    if ( (wRet != RETURN_OK) || (wlanIndex <0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
        // Error could not find index
        return ANSC_STATUS_FAILURE;
    }

    wifi_getApWpsEnable(wlanIndex, &pCfg->bEnabled);
    
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
    wifi_getApWpsConfigMethodsEnabled(wlanIndex,methodsEnabled);
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
    int wlanIndex = -1;
wifiDbgPrintf("%s\n",__FUNCTION__);

    if (!pInfo || !pSsid)
    {
        return ANSC_STATUS_FAILURE;
    }

    int wRet = wifi_getIndexFromName(pSsid, &wlanIndex);
    if ( (wRet != RETURN_OK) || (wlanIndex <0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
        // Error could not find index
        return ANSC_STATUS_FAILURE;
    }

    // Read Only parameter
    // pInfo->ConfigMethodsSupported

    unsigned int pin = _ansc_atoi(pInfo->X_CISCO_COM_Pin);
    wifi_setApWpsDevicePIN(wlanIndex, pin);

#if !defined(_COSA_BCM_MIPS_)&& !defined(_COSA_BCM_ARM_)
    // Already set WPS enabled in WpsSetCfg, but 
    //   if config==TRUE set again to configured(2).
    if ( pInfo->X_Comcast_com_Configured == TRUE ) {
        wifi_setApWpsEnable(wlanIndex, TRUE);
    }
#endif

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
    int wlanIndex = -1;
    char  configState[32];
    unsigned int pin;

    wifiDbgPrintf("%s\n",__FUNCTION__);

    if (!pInfo || !pSsid)
    {
        return ANSC_STATUS_FAILURE;
    }

    int wRet = wifi_getIndexFromName(pSsid, &wlanIndex);
    if ( (wRet != RETURN_OK) || (wlanIndex <0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
        // Error could not find index
        return ANSC_STATUS_FAILURE;
    }

    pInfo->ConfigMethodsSupported = COSA_DML_WIFI_WPS_METHOD_PushButton | COSA_DML_WIFI_WPS_METHOD_Pin;

    wifi_getApWpsDevicePIN(wlanIndex, &pin);

    sprintf(pInfo->X_CISCO_COM_Pin, "%08d", pin);

    wifi_getApWpsConfigurationState(wlanIndex, configState);
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
    int wlanIndex = -1;	//???
    BOOL enabled=FALSE; 
    wifi_associated_dev_t *wifi_associated_dev_array=NULL, *ps=NULL;
	COSA_DML_WIFI_AP_ASSOC_DEVICE *pWifiApDev=NULL, *pd=NULL; 
	ULONG i=0, array_size=0;
 
    if (!pSsid)
        return NULL;
    
    int wRet = wifi_getIndexFromName(pSsid, &wlanIndex);
    if ( (wRet != RETURN_OK) || (wlanIndex <0) || (wlanIndex >= WIFI_INDEX_MAX) )
		return NULL;
    
    wifi_getApEnable(wlanIndex, &enabled);
    if (enabled == FALSE) 
		return NULL; 

	//hal would allocate the array
	wifi_getApAssociatedDeviceDiagnosticResult(wlanIndex, &wifi_associated_dev_array, &array_size);
	if(wifi_associated_dev_array && array_size>0) {
		*pulCount=array_size;
		//zqiu: TODO: to search the MAC in exsting pWifiApDev Array to find the match, and count Disassociations/AuthenticationFailures and Active
		pWifiApDev=(PCOSA_DML_WIFI_AP_ASSOC_DEVICE)calloc(sizeof(COSA_DML_WIFI_AP_ASSOC_DEVICE), array_size);
		for(i=0, ps=wifi_associated_dev_array, pd=pWifiApDev; i<array_size; i++, ps++, pd++) {
			memcpy(pd->MacAddress, ps->cli_MACAddress, sizeof(UCHAR)*6);
			pd->AuthenticationState 	= ps->cli_AuthenticationState;
			pd->LastDataDownlinkRate 	= ps->cli_LastDataDownlinkRate;
			pd->LastDataUplinkRate 		= ps->cli_LastDataUplinkRate;
			pd->SignalStrength 			= ps->cli_SignalStrength;
			pd->Retransmissions 		= ps->cli_Retransmissions;
			pd->Active 					= ps->cli_Active;	//???
			memcpy(pd->OperatingStandard, ps->cli_OperatingStandard, sizeof(char)*64);
			memcpy(pd->OperatingChannelBandwidth, ps->cli_OperatingChannelBandwidth, sizeof(char)*64);
			pd->SNR 					= ps->cli_SNR;
			memcpy(pd->InterferenceSources, ps->cli_InterferenceSources, sizeof(char)*64);
			pd->DataFramesSentAck 		= ps->cli_DataFramesSentAck;
			pd->DataFramesSentNoAck 	= ps->cli_DataFramesSentNoAck;
			pd->BytesSent 				= ps->cli_BytesSent;
			pd->BytesReceived 			= ps->cli_BytesReceived;
			pd->RSSI		 			= ps->cli_RSSI;
			pd->MinRSSI 				= ps->cli_MinRSSI;
			pd->MaxRSSI 				= ps->cli_MaxRSSI;
			pd->Disassociations			= 0;	//???
			pd->AuthenticationFailures	= 0;	//???
		}
		free(wifi_associated_dev_array);
		return (PCOSA_DML_WIFI_AP_ASSOC_DEVICE)pWifiApDev; 
	}	
    
    return NULL;
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
    if (!pSsid)
    {
        return ANSC_STATUS_FAILURE;
    }
    
wifiDbgPrintf("%s SSID %s\n",__FUNCTION__, pSsid);

    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
    ULONG                           index             = 0;
    ULONG                           ulCount           = 0;
    
    /*For example we have 5 AssocDevices*/
    int wlanIndex = -1;

    int wRet = wifi_getIndexFromName(pSsid, &wlanIndex);
    if ( (wRet != RETURN_OK) || (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
	// Error could not find index
        return ANSC_STATUS_FAILURE;
    }

    ulCount = 0;

    wifi_getApNumDevicesAssociated(wlanIndex, &ulCount);
    if (ulCount > 0)
    {
		for (index = ulCount; index > 0; index--)
		{ 
			wifi_device_t wlanDevice;

			wifi_getAssociatedDeviceDetail(wlanIndex, index, &wlanDevice);
			wifi_kickAssociatedDevice(wlanIndex, &wlanDevice);
		}
#if defined(ENABLE_FEATURE_MESHWIFI)
        {
            char cmd[256] = {0};
            // notify mesh components that wifi SSID Advertise changed
            CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh to kick off all devices\n",__FUNCTION__));
            sprintf(cmd, "/usr/bin/sysevent set wifi_kickAllApAssociatedDevice \"RDK|%d\"",
                    wlanIndex);
            system(cmd);
        }
#endif
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
    int wlanIndex = -1;
    char recName[256];
    int retPsmGet = CCSP_SUCCESS;
    char *strValue = NULL;

    if (!pCfg || !pSsid)
    {
        return ANSC_STATUS_FAILURE;
    }

    int wRet = wifi_getIndexFromName(pSsid, &wlanIndex);
    if ( (wRet != RETURN_OK) || (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
        // Error could not find index
        return ANSC_STATUS_FAILURE;
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilterMode, wlanIndex+1);
    retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &strValue);
    if (retPsmGet == CCSP_SUCCESS) {
        mode = _ansc_atoi(strValue);
        wifi_setApMacAddressControlMode(wlanIndex,mode);
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
    int wlanIndex = -1;
    char recName[256];
    int retPsmSet = CCSP_SUCCESS;

    if (!pCfg || !pSsid)
    {
        return ANSC_STATUS_FAILURE;
    }

    int wRet = wifi_getIndexFromName(pSsid, &wlanIndex);
    if ((wRet != RETURN_OK) || (wlanIndex < 0) || (wlanIndex >= WIFI_INDEX_MAX) )
    {
        // Error could not find index
        return ANSC_STATUS_FAILURE;
    }

    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilterMode, wlanIndex+1);

    if ( pCfg->bEnabled == FALSE )
    {
	wifi_setApMacAddressControlMode(wlanIndex, 0);
        PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "0");
    } else if ( pCfg->FilterAsBlackList == FALSE ) {
	wifi_setApMacAddressControlMode(wlanIndex, 1);
        PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "1");
    } else if ( pCfg->FilterAsBlackList == TRUE ) {
	wifi_setApMacAddressControlMode(wlanIndex, 2);
        PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, "2");
    }

#if defined(ENABLE_FEATURE_MESHWIFI)
    {
        char cmd[256] = {0};
        // notify mesh components that wifi SMAc Filter Mode changed
        CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh Mac filter mode changed\n",__FUNCTION__));
        sprintf(cmd, "/usr/bin/sysevent set wifi_MacAddressControlMode \"RDK|%d|%s|%s\"",
                wlanIndex, (pCfg->bEnabled?"true":"false"), (pCfg->FilterAsBlackList?"true":"false"));
        system(cmd);
    }
#endif

#if defined(_COSA_BCM_MIPS_) //|| defined(_COSA_BCM_ARM_)
    // special case for Broadcom radios
    wifi_initRadio((wlanIndex%2==0?0:1));
#endif

    if ( pCfg->bEnabled == TRUE ) {
        wifi_kickApAclAssociatedDevices(wlanIndex, pCfg->FilterAsBlackList);
    }

    // Notify WiFiExtender that MacFilter has changed
    {
        CosaDml_NotifyWiFiExt(COSA_WIFIEXT_DM_UPDATE_SSID);
    }

#if defined(_COSA_BCM_MIPS_) //|| defined(_COSA_BCM_ARM_)
    // special case for Broadcom radios
    wifi_initRadio((wlanIndex%2==0?0:1));
#endif

	if ( pCfg->bEnabled == TRUE )
	{
		BOOL enable = FALSE; 
#if defined(_ENABLE_BAND_STEERING_)
		if( ( 0 == wlanIndex ) || \
			( 1 == wlanIndex )
		  )
		{
			//To get Band Steering enable status
			wifi_getBandSteeringEnable( &enable );
			
			/* 
			  * When bandsteering already enabled then need to disable band steering 
			  * when private ACL is going to on case
			  */
			if( enable )
			{
				pthread_t tid; 
				pthread_create( &tid, 
								NULL, 
								CosaDmlWiFi_DisableBandSteeringBasedonACLThread, 
								NULL ); 
			}
		}
#endif /* _ENABLE_BAND_STEERING_ */
	}

    return ANSC_STATUS_SUCCESS;
}

/* CosaDmlWiFi_DisableBandSteeringBasedonACLThread() */
void CosaDmlWiFi_DisableBandSteeringBasedonACLThread( void *input )  
{
	CCSP_MESSAGE_BUS_INFO *bus_info = (CCSP_MESSAGE_BUS_INFO *)bus_handle;
	parameterValStruct_t   param_val[ 1 ]    = { "Device.WiFi.X_RDKCENTRAL-COM_BandSteering.Enable", "false", ccsp_boolean };
	char				   component[ 256 ]  = "eRT.com.cisco.spvtg.ccsp.wifi";
	char				   bus[256]		     = "/com/cisco/spvtg/ccsp/wifi";
	char*				   faultParam 	     = NULL;
	int 				   ret			     = 0;	
			
	pthread_detach( pthread_self( ) );

	CcspWifiTrace(("RDK_LOG_WARN, %s-%d [Disable BS due to ACL is on] \n",__FUNCTION__,__LINE__));
	
	ret = CcspBaseIf_setParameterValues(  bus_handle,
										  component,
										  bus,
										  0,
										  0,
										  &param_val,
										  1,
										  TRUE,
										  &faultParam
										  );
			
	if( ( ret != CCSP_SUCCESS ) && \
		( faultParam )
	  )
	{
		CcspWifiTrace(("RDK_LOG_WARN, %s-%d Failed to disable BS\n",__FUNCTION__,__LINE__));
		bus_info->freefunc( faultParam );
	}
}

ANSC_STATUS
CosaDmlWiFiApMfPushCfg
(
PCOSA_DML_WIFI_AP_MF_CFG    pCfg,
ULONG                                           wlanIndex
)
{
    wifiDbgPrintf("%s\n",__FUNCTION__);
    if (!pCfg) return ANSC_STATUS_FAILURE;

    if ( pCfg->bEnabled == FALSE ) {
        wifi_setApMacAddressControlMode(wlanIndex, 0);
    } else if ( pCfg->FilterAsBlackList == FALSE ) {
        wifi_setApMacAddressControlMode(wlanIndex, 1);
    } else if ( pCfg->FilterAsBlackList == TRUE ) {
        wifi_setApMacAddressControlMode(wlanIndex, 2);
    }

#if defined(ENABLE_FEATURE_MESHWIFI)
    {
        char cmd[256] = {0};
        // notify mesh components that wifi SMAc Filter Mode changed
        CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh Mac filter mode changed\n",__FUNCTION__));
        sprintf(cmd, "/usr/bin/sysevent set wifi_MacAddressControlMode \"RDK|%d|%s|%s\"",
                wlanIndex, (pCfg->bEnabled?"true":"false"), (pCfg->FilterAsBlackList?"true":"false"));
        system(cmd);
    }
#endif
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiApSetMFQueue
    (
        QUEUE_HEADER *mfQueue,
        ULONG                  apIns
    )
{
    if ( (apIns >= 1) && (apIns <= WIFI_INDEX_MAX) )
    {
wifiDbgPrintf("%s apIns = %d \n",__FUNCTION__, apIns );
    sWiFiDmlApMfQueue[apIns-1] = mfQueue;
    }
    return ANSC_STATUS_SUCCESS;
}

//>> Deprecated

ANSC_STATUS
CosaDmlWiFi_GetWEPKey64ByIndex(ULONG apIns, ULONG keyIdx, PCOSA_DML_WEPKEY_64BIT pWepKey)
{
wifiDbgPrintf("%s apIns = %d, keyIdx = %d\n",__FUNCTION__, apIns, keyIdx);
//    wifi_getWepKey(apIns-1, keyIdx+1, pWepKey->WEPKey);
    
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_SetWEPKey64ByIndex(ULONG apIns, ULONG keyIdx, PCOSA_DML_WEPKEY_64BIT pWepKey)
{
wifiDbgPrintf("%s apIns = %d, keyIdx = %d\n",__FUNCTION__, apIns, keyIdx);

    // for downgrade compatibility set both index 0 & 1 for keyIdx 1 
    // Comcast 1.3 release uses keyIdx 0 maps to driver index 0, this is used by driver and the on GUI
    // all four keys are set the same.  If a box is downgraded from 1.6 to 1.3 and the required WEP key will be set
   //if (keyIdx == 0)
   //{
   //    wifi_setWepKey(apIns-1, keyIdx, pWepKey->WEPKey);
   //}
    
    //wifi_setWepKey(apIns-1, keyIdx+1, pWepKey->WEPKey);
    //sWiFiDmlWepChg[apIns-1] = TRUE;

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_GetWEPKey128ByIndex(ULONG apIns, ULONG keyIdx, PCOSA_DML_WEPKEY_128BIT pWepKey)
{
wifiDbgPrintf("%s apIns = %d, keyIdx = %d\n",__FUNCTION__, apIns, keyIdx);
    //wifi_getWepKey(apIns-1, keyIdx+1, pWepKey->WEPKey);

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_SetWEPKey128ByIndex(ULONG apIns, ULONG keyIdx, PCOSA_DML_WEPKEY_128BIT pWepKey)
{
wifiDbgPrintf("%s apIns = %d, keyIdx = %d\n",__FUNCTION__, apIns, keyIdx);
    
    //wifi_setWepKey(apIns-1, keyIdx+1, pWepKey->WEPKey);
    //sWiFiDmlWepChg[apIns-1] = TRUE;

    return ANSC_STATUS_SUCCESS;
}
//<<

#define MAX_MAC_FILT                64

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
    if ( (apIns < 1) || (apIns > WIFI_INDEX_MAX) )
    {
	return 0;
    }
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

    if (!pMacFilt) return ANSC_STATUS_FAILURE;

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
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s : get %s Fail \n",__FUNCTION__, recName));
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

    if (!pMacFilt || (apIns < 1) || (apIns > WIFI_INDEX_MAX)) return ANSC_STATUS_FAILURE;

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
        wifi_addApAclDevice(apIns-1,devMac);
        wifiDbgPrintf("%s called wifi_addApAclDevice index = %d mac %s \n",__FUNCTION__, apIns-1,devMac);
		CcspWifiTrace(("RDK_LOG_WARN,\n%s : called wifi_addApAclDevice index = %d mac %s \n",__FUNCTION__,apIns-1,devMac));
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
	BOOL enabled=FALSE;

    if (!pMacFilt || (apIns < 1) || (apIns > WIFI_INDEX_MAX)) return ANSC_STATUS_FAILURE;

        if (g_macFiltCnt[apIns-1] >= MAX_MAC_FILT)
        {
              CcspTraceWarning(("Mac Filter max limit is reached ,returning failure\n"));
              return ANSC_STATUS_FAILURE;
        }

	wifi_getApEnable(apIns-1, &enabled);
	if (enabled) { 		 			
		CcspTraceWarning(("Mac Filter Entry count:%d\n", g_macFiltCnt[apIns-1]));
		int rc = wifi_addApAclDevice(apIns-1,pMacFilt->MACAddress);
		if (rc != 0) {
			wifiDbgPrintf("%s apIns = %d wifi_addApAclDevice failed for %s\n",__FUNCTION__, apIns, pMacFilt->MACAddress);
			CcspWifiTrace(("RDK_LOG_ERROR,\n%s : apIns = %d wifi_addApAclDevice failed for %s \n",__FUNCTION__, apIns, pMacFilt->MACAddress));
			//zqiu: need to continue to save to PSM
			//return ANSC_STATUS_FAILURE;
		} else {
#if defined(ENABLE_FEATURE_MESHWIFI)
            {
                char cmd[256] = {0};
                // notify mesh components that wifi SSID Advertise changed
                CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh to add device\n",__FUNCTION__));
                snprintf(cmd, sizeof(cmd)-1, "/usr/bin/sysevent set wifi_addApAclDevice \"RDK|%d|%s\"",
                        apIns-1, pMacFilt->MACAddress);
                system(cmd);
            }
#endif
		}

//		wifi_getApAclDeviceNum(apIns-1, &g_macFiltCnt[apIns-1]);
	}
         g_macFiltCnt[apIns-1]++;
    sprintf(pMacFilt->Alias,"MacFilter%d", pMacFilt->InstanceNumber);

    // Add Mac to Non-Vol PSM
    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilter, apIns, pMacFilt->InstanceNumber);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, pMacFilt->MACAddress);
    if (retPsmSet != CCSP_SUCCESS) {
	wifiDbgPrintf("%s Error %d adding mac = %s \n", __FUNCTION__, retPsmSet, pMacFilt->MACAddress);
	CcspWifiTrace(("RDK_LOG_ERROR,\n%s : %d adding mac = %s\n",__FUNCTION__, retPsmSet, pMacFilt->MACAddress));
        return ANSC_STATUS_FAILURE;
    }

    // Add Mac to Non-Vol PSM
    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilterDevice, apIns, pMacFilt->InstanceNumber);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, pMacFilt->DeviceName);
    if (retPsmSet != CCSP_SUCCESS) {
	wifiDbgPrintf("%s Error %d adding mac device name = %s \n", __FUNCTION__, retPsmSet, pMacFilt->DeviceName);
	CcspWifiTrace(("RDK_LOG_ERROR,\n%s : %d adding mac device name = %s \n",__FUNCTION__, retPsmSet, pMacFilt->DeviceName));
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
	CcspWifiTrace(("RDK_LOG_ERROR,\n%s : PSM error adding MacFilterList  mac %d \n",__FUNCTION__, retPsmSet));
	return ANSC_STATUS_FAILURE;
    }

    // Notify WiFiExtender that MacFilter has changed
    {
        CosaDml_NotifyWiFiExt(COSA_WIFIEXT_DM_UPDATE_SSID);
	/* Note:When mac filter mode change gets called before adding mac in the list, kick mac does not work. 
Added api call to kick mac, once entry is added in the list*/
		wifi_kickApAclAssociatedDevices(apIns-1, TRUE);
    }

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlMacFilt_DelEntry(ULONG apIns, ULONG macFiltIns)
{
    if ( (apIns < 1) || (apIns > WIFI_INDEX_MAX) )
    {
	return ANSC_STATUS_FAILURE;
    }

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
	//Note: Since wifi_kickApAclAssociatedDevices was getting called after wifi_delApAclDevice kick was not happening for the removed entry. 
	//Hence calling kick before removing the entry.  
	wifi_kickApAclAssociatedDevices(apIns-1, TRUE);
	wifi_delApAclDevice(apIns-1,macAddress);
#if defined(ENABLE_FEATURE_MESHWIFI)
    {
        char cmd[256] = {0};
        // notify mesh components that wifi SSID Advertise changed
        CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh to delete device\n",__FUNCTION__));
        snprintf(cmd, sizeof(cmd)-1, "/usr/bin/sysevent set wifi_delApAclDevice \"RDK|%d|%s\"",
                apIns-1, macAddress);
        system(cmd);
    }
#endif
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(macAddress);

	if ( g_macFiltCnt[apIns-1] > 0 )
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
		    snprintf(newMacList, sizeof(newMacList)-1, "%d%s",g_macFiltCnt[apIns-1],mac);
	            retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, newMacList);
		    if (retPsmSet != CCSP_SUCCESS) {
			wifiDbgPrintf("%s PSM error %d while setting MacFilterList %s \n", __FUNCTION__, retPsmSet, newMacList);
			CcspWifiTrace(("RDK_LOG_ERROR,\n%s : PSM error %d while setting MacFilterList %s \n",__FUNCTION__, retPsmSet, newMacList));
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

    if (!pMacFilt) return ANSC_STATUS_FAILURE;

    CosaDmlMacFilt_GetEntryByIndex(apIns, macFiltIns, pMacFilt);

    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlMacFilt_SetConf(ULONG apIns, ULONG macFiltIns, PCOSA_DML_WIFI_AP_MAC_FILTER pMacFilt)
{
wifiDbgPrintf("%s\n",__FUNCTION__);
    char recName[256];
    int retPsmSet = CCSP_SUCCESS;

    if (!pMacFilt) return ANSC_STATUS_FAILURE;

    // Add Mac to Non-Vol PSM
    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilter, apIns, pMacFilt->InstanceNumber);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, pMacFilt->MACAddress);
    if (retPsmSet != CCSP_SUCCESS) {
	wifiDbgPrintf("%s Error %d adding mac = %s \n", __FUNCTION__, retPsmSet, pMacFilt->MACAddress);
	CcspWifiTrace(("RDK_LOG_ERROR,\n%s :adding mac = %s\n",__FUNCTION__, retPsmSet, pMacFilt->MACAddress));
        return ANSC_STATUS_FAILURE;
    }
	CcspWifiTrace(("RDK_LOG_INFO,\n%s :adding mac = %s\n",__FUNCTION__, retPsmSet, pMacFilt->MACAddress));

    // Add Mac Device Name to Non-Vol PSM
    memset(recName, 0, sizeof(recName));
    sprintf(recName, MacFilterDevice, apIns, pMacFilt->InstanceNumber);
    retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, pMacFilt->DeviceName);
    if (retPsmSet != CCSP_SUCCESS) {
	wifiDbgPrintf("%s Error %d adding mac device name = %s \n", __FUNCTION__, retPsmSet, pMacFilt->DeviceName);
	CcspWifiTrace(("RDK_LOG_ERROR,\n%s :adding mac device name = %s \n",__FUNCTION__, retPsmSet, pMacFilt->DeviceName));
        return ANSC_STATUS_FAILURE;
    }
	CcspWifiTrace(("RDK_LOG_INFO,\n%s :adding mac device name = %s \n",__FUNCTION__, retPsmSet, pMacFilt->DeviceName));
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
#ifdef _XB6_PRODUCT_REQ_
        "/nvram/config/wireless",
#else
        "/nvram/etc/ath/.configData",
#endif
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
        free(hdr); /*RDKB-6907, CID-33234, free unused resource before exit*/
        return ANSC_STATUS_FAILURE;
    }

    *size = hdr->totsize;
    memcpy(buf, hdr, hdr->totsize);
    free(hdr); /*RDKB-6907, CID-33234, free unused resource before exit*/
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

ANSC_STATUS 
CosaDmlWiFi_RadioUpTime(int *TimeInSecs, int radioIndex)
{
    if (!TimeInSecs) return ANSC_STATUS_FAILURE;
    wifi_getRadioUpTime(radioIndex, TimeInSecs);
    if (*TimeInSecs  == 0) {
        wifiDbgPrintf("%s: error : Radion is not enable \n", __FUNCTION__);
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s :error : Radion is not enable\n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
	return ANSC_STATUS_SUCCESS;
}
ANSC_STATUS 
CosaDmlWiFi_getRadioCarrierSenseThresholdRange(INT radioIndex, INT *output)
{
    int ret = 0;
    ret = wifi_getRadioCarrierSenseThresholdRange(radioIndex,output);
	 
    if (ret != 0) {
		CcspWifiTrace(("RDK_LOG_ERROR,\n%s :wifi_getRadioCarrierSenseThresholdRange returned fail response\n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
	return ANSC_STATUS_SUCCESS;
}
ANSC_STATUS 
CosaDmlWiFi_getRadioCarrierSenseThresholdInUse(INT radioIndex, INT *output)
{
    int ret = 0;
    ret = wifi_getRadioCarrierSenseThresholdInUse(radioIndex,output);
    if (ret != 0) {
		CcspWifiTrace(("RDK_LOG_ERROR,\n%s :wifi_getRadioCarrierSenseThresholdInUse returned fail response\n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
	return ANSC_STATUS_SUCCESS;
}
ANSC_STATUS 
CosaDmlWiFi_setRadioCarrierSenseThresholdInUse(INT radioIndex, INT threshold)
{
    int ret = 0;
    ret = wifi_setRadioCarrierSenseThresholdInUse(radioIndex,threshold);
    if (ret != 0) {
		CcspWifiTrace(("RDK_LOG_ERROR,\n%s :wifi_setRadioCarrierSenseThresholdInUse returned fail response\n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
	return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS 
CosaDmlWiFi_getRadioBeaconPeriod(INT radioIndex, UINT *output)
{
    int ret = 0;
    ret = wifi_getRadioBeaconPeriod(radioIndex,output);
    if (ret != 0) {
		CcspWifiTrace(("RDK_LOG_ERROR,\n%s :wifi_getRadioBeaconPeriod returned fail response\n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
	return ANSC_STATUS_SUCCESS;
}
ANSC_STATUS 
CosaDmlWiFi_setRadioBeaconPeriod(INT radioIndex, UINT BeaconPeriod)
{
    int ret = 0;
    ret = wifi_setRadioBeaconPeriod(radioIndex,BeaconPeriod);
    if (ret != 0) {
		CcspWifiTrace(("RDK_LOG_ERROR,\n%s :wifi_setRadioBeaconPeriod returned fail response\n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
	return ANSC_STATUS_SUCCESS;
}


//zqiu: for RDKB-3346
/*
ANSC_STATUS 
CosaDmlWiFi_getRadioBasicDataTransmitRates(INT radioIndex, ULONG *output)
{
    int ret = 0;
	char sTransmitRates[128]="";
	ret = wifi_getRadioBasicDataTransmitRates(radioIndex,sTransmitRates);
    if (ret != 0) {
		CcspWifiTrace(("RDK_LOG_ERROR,\n%s :wifi_getRadioBasicDataTransmitRates returned fail response\n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
	if(AnscEqualString(sTransmitRates, "Default", TRUE))
	{
		*output = 1;
	}
	else if(AnscEqualString(sTransmitRates, "1-2Mbps", TRUE))
	{
		*output = 2;
	}
	else if(AnscEqualString(sTransmitRates, "All", TRUE))
	{
		*output = 3;
	}
	return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS 
CosaDmlWiFi_setRadioBasicDataTransmitRates(INT radioIndex,ULONG val)
{
    int ret = 0;
	char sTransmitRates[128] = "Default";
	if(val == 2)
		strcpy(sTransmitRates,"1-2Mbps");
	else if(val == 3)
		strcpy(sTransmitRates,"All");
    ret = wifi_setRadioBasicDataTransmitRates(radioIndex,sTransmitRates);
    if (ret != 0) {
		CcspWifiTrace(("RDK_LOG_ERROR,\n%s :wifi_setRadioBasicDataTransmitRates returned fail response\n",__FUNCTION__));
        return ANSC_STATUS_FAILURE;
    }
	return ANSC_STATUS_SUCCESS;
}
*/


ANSC_STATUS 
CosaDmlWiFi_getRadioStatsRadioStatisticsMeasuringRate(INT radioInstanceNumber, INT *output)
{
    int ret = 0;
	char record[256]="";
	char *strValue=NULL;
	if (!output) return ANSC_STATUS_FAILURE;
	sprintf(record, MeasuringRateRd, radioInstanceNumber);
	ret = PSM_Get_Record_Value2(bus_handle,g_Subsystem, record, NULL, &strValue);
    if (ret != CCSP_SUCCESS) {
		CcspWifiTrace(("RDK_LOG_ERROR,\n%s : get %s fail\n",__FUNCTION__, record));
		return ANSC_STATUS_FAILURE;
	}
	*output = _ansc_atoi(strValue);        
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
	return ANSC_STATUS_SUCCESS;
	//return wifi_getRadioStatsRadioStatisticsMeasuringRate(radioInstanceNumber-1,output);
    
}

ANSC_STATUS 
CosaDmlWiFi_setRadioStatsRadioStatisticsMeasuringRate(INT radioInstanceNumber, INT rate)
{
    //int ret = 0;
	char record[256]="";
	char verString[32]="";
	
	//if ( wifi_setRadioStatsRadioStatisticsMeasuringRate(radioInstanceNumber-1,rate) != 0) 
    //    return ANSC_STATUS_FAILURE;
	
	sprintf(record, MeasuringRateRd, radioInstanceNumber);
	sprintf(verString, "%d",rate);
	PSM_Set_Record_Value2(bus_handle,g_Subsystem, record, ccsp_string, verString);
    
	return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS 
CosaDmlWiFi_getRadioStatsRadioStatisticsMeasuringInterval(INT radioInstanceNumber, INT *output)
{
	int ret = 0;
	char record[256]="";
	char *strValue=NULL;
	if (!output) return ANSC_STATUS_FAILURE;
	
	sprintf(record, MeasuringIntervalRd, radioInstanceNumber);
	ret = PSM_Get_Record_Value2(bus_handle,g_Subsystem, record, NULL, &strValue);
    if (ret != CCSP_SUCCESS) {
		CcspWifiTrace(("RDK_LOG_ERROR,\n%s : get %s fail\n",__FUNCTION__, record));
		return ANSC_STATUS_FAILURE;
	}
	*output = _ansc_atoi(strValue);        
	((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
	return ANSC_STATUS_SUCCESS;
    
   //return wifi_getRadioStatsRadioStatisticsMeasuringInterval(radioInstanceNumber-1,output);   
}

ANSC_STATUS 
CosaDmlWiFi_setRadioStatsRadioStatisticsMeasuringInterval(INT radioInstanceNumber, INT rate)
{
    //int ret = 0;
	char record[256]="";
	char verString[32];
    //if (wifi_setRadioStatsRadioStatisticsMeasuringInterval(radioInstanceNumber-1,rate) != 0) 
    //    return ANSC_STATUS_FAILURE;
	
	sprintf(record, MeasuringIntervalRd, radioInstanceNumber);
    sprintf(verString, "%d",rate);
	PSM_Set_Record_Value2(bus_handle,g_Subsystem, record, ccsp_string, verString);
    
	return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS 
CosaDmlWiFi_resetRadioStats(PCOSA_DML_WIFI_RADIO_STATS    pWifiStats)
{
    if (!pWifiStats) return ANSC_STATUS_FAILURE;
    memset(pWifiStats->RslInfo.ReceivedSignalLevel, 0, sizeof(INT)*RSL_MAX);
	pWifiStats->RslInfo.Count=pWifiStats->RadioStatisticsMeasuringInterval/pWifiStats->RadioStatisticsMeasuringRate;
	pWifiStats->RslInfo.StartIndex=0;
	
	pWifiStats->StatisticsStartTime=AnscGetTickInSeconds();
    
	return ANSC_STATUS_SUCCESS;
}



ANSC_STATUS 
CosaDmlWiFi_getRadioStatsReceivedSignalLevel(INT radioInstanceNumber, INT *iRsl)
{
	wifi_getRadioStatsReceivedSignalLevel(radioInstanceNumber-1,0,iRsl);
	return ANSC_STATUS_SUCCESS;
}



ANSC_STATUS
CosaDmlWiFiRadioStatsSet
(
int     InstanceNumber,
PCOSA_DML_WIFI_RADIO_STATS    pWifiRadioStats        
)
{
    ANSC_STATUS                     returnStatus   = ANSC_STATUS_SUCCESS;
	wifi_radioTrafficStatsMeasure_t measure;

    wifiDbgPrintf("%s Config changes  \n",__FUNCTION__);
    int  radioIndex;
    
	CcspWifiTrace(("RDK_LOG_WARN,%s\n",__FUNCTION__));
    if (!pWifiRadioStats )
    {
        return ANSC_STATUS_FAILURE;
    }

    
    radioIndex = InstanceNumber-1;

    //CosaDmlWiFiSetRadioPsmData(pWifiRadioStats, wlanIndex, pCfg->InstanceNumber);
	if(pWifiRadioStats->RadioStatisticsEnable) {
		measure.radio_RadioStatisticsMeasuringRate=pWifiRadioStats->RadioStatisticsMeasuringRate;
		measure.radio_RadioStatisticsMeasuringInterval=pWifiRadioStats->RadioStatisticsMeasuringInterval;
		CosaDmlWiFi_resetRadioStats(pWifiRadioStats);
		wifi_setRadioTrafficStatsMeasure(radioIndex, &measure); 
		
	}
	wifi_setRadioTrafficStatsRadioStatisticsEnable(radioIndex, pWifiRadioStats->RadioStatisticsEnable);

	return ANSC_STATUS_SUCCESS;
}


pthread_mutex_t sNeighborScanThreadMutex = PTHREAD_MUTEX_INITIALIZER;




//CosaDmlWiFi_doNeighbouringScan ( PCOSA_DML_NEIGHTBOURING_WIFI_RESULT *ppNeighScanResult, unsigned int *pResCount ) 
void * CosaDmlWiFi_doNeighbouringScanThread (void *input) 
{
    if (!input) return ANSC_STATUS_FAILURE;
	PCOSA_DML_NEIGHTBOURING_WIFI_DIAG_CFG pNeighScan=input;
	PCOSA_DML_NEIGHTBOURING_WIFI_RESULT tmp_2, tmp_5;
	wifi_neighbor_ap2_t *wifiNeighbour_2=NULL, *wifiNeighbour_5=NULL;
	unsigned int count_2=0, count_5=0;
				
	wifi_getNeighboringWiFiDiagnosticResult2(0, &wifiNeighbour_2,&count_2);	
	wifi_getNeighboringWiFiDiagnosticResult2(1, &wifiNeighbour_5,&count_5);	
		

fprintf(stderr, "-- %s %d count_2=%d count_5=%d\n", __func__, __LINE__,  count_2, count_5);	
	printf("%s Calling pthread_mutex_lock for sNeighborScanThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 
    pthread_mutex_lock(&sNeighborScanThreadMutex);
    printf("%s Called pthread_mutex_lock for sNeighborScanThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 
    pNeighScan->ResultCount=0;
	
	if(count_2 > 0) {
		tmp_2=pNeighScan->pResult_2;
		pNeighScan->pResult_2=(PCOSA_DML_NEIGHTBOURING_WIFI_RESULT)wifiNeighbour_2;
		pNeighScan->ResultCount_2=count_2;
		if(tmp_2) 
			free(tmp_2);	
	}
	if(count_5 > 0) {
		tmp_5=pNeighScan->pResult_5;
		pNeighScan->pResult_5=(PCOSA_DML_NEIGHTBOURING_WIFI_RESULT)wifiNeighbour_5;
		pNeighScan->ResultCount_5=count_5;
		if(tmp_5) 
			free(tmp_5);	
	}
	pNeighScan->ResultCount=pNeighScan->ResultCount_2+pNeighScan->ResultCount_5;
	pNeighScan->ResultCount=(pNeighScan->ResultCount<=250)?pNeighScan->ResultCount:250;
	AnscCopyString(pNeighScan->DiagnosticsState, "Completed");
	
	printf("%s Calling pthread_mutex_unlock for sNeighborScanThreadMutex  %d \n",__FUNCTION__ , __LINE__ ); 
    pthread_mutex_unlock(&sNeighborScanThreadMutex);
    printf("%s Called pthread_mutex_unlock for sNeighborScanThreadMutex  %d \n",__FUNCTION__ , __LINE__ );  
	
}

ANSC_STATUS 
CosaDmlWiFi_doNeighbouringScan ( PCOSA_DML_NEIGHTBOURING_WIFI_DIAG_CFG pNeighScan)
{
    if (!pNeighScan) return ANSC_STATUS_FAILURE;
	fprintf(stderr, "-- %s1Y %d\n", __func__, __LINE__);
	wifiDbgPrintf("%s\n",__FUNCTION__);
	pthread_t tid; 
	AnscCopyString(pNeighScan->DiagnosticsState, "Requested");
    if(pthread_create(&tid,NULL,CosaDmlWiFi_doNeighbouringScanThread, (void*)pNeighScan))  {
		AnscCopyString(pNeighScan->DiagnosticsState, "Error");
		return ANSC_STATUS_FAILURE;
	}
	return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS 
CosaDmlWiFi_RadioGetResetCount(INT radioIndex, ULONG *output)
{
    int ret = 0;
	printf(" **** CosaDmlWiFi_RadioGetResetCoun : Entry **** \n");

	ret = wifi_getRadioResetCount(radioIndex,output);

	return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS 
CosaDmlWiFi_GetBandSteeringOptions(PCOSA_DML_WIFI_BANDSTEERING_OPTION  pBandSteeringOption)
{
	if( NULL != pBandSteeringOption )
	{
		BOOL  support = FALSE,
			  enable  = FALSE;
		CHAR apgroup[COSA_DML_WIFI_MAX_BAND_STEERING_APGROUP_STR_LEN] = "1,2";

#if defined(_ENABLE_BAND_STEERING_)
		//To get Band Steering enable status
		wifi_getBandSteeringEnable( &enable );

		/*
		  * Check whether BS enable. If enable then we have to consider some cases for ACL
		  */
		if( TRUE == enable )
		{
			char *strValue	 = NULL;
			char  recName[ 256 ];
			int   retPsmGet  = CCSP_SUCCESS,
				  mode_24G	 = 0,
				  mode_5G	 = 0;

			//MacFilter mode for private 2.4G
			memset ( recName, 0, sizeof( recName ) );
			sprintf( recName, MacFilterMode, 1 );
			retPsmGet = PSM_Get_Record_Value2( bus_handle, g_Subsystem, recName, NULL, &strValue );
			if ( CCSP_SUCCESS == retPsmGet ) 
			{
				mode_24G = _ansc_atoi( strValue );
				((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc( strValue );
			}
			
			//MacFilter mode for private 5G
			memset ( recName, 0, sizeof( recName ) );
			sprintf( recName, MacFilterMode, 2 );
			retPsmGet = PSM_Get_Record_Value2( bus_handle, g_Subsystem, recName, NULL, &strValue );
			if ( CCSP_SUCCESS == retPsmGet ) 
			{
				mode_5G = _ansc_atoi( strValue );
				((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc( strValue );
			}
			
			/*
			  * Any one of private wifi ACL mode enabled then don't allow to enable BS
			  */
			if( ( 0 != mode_24G ) || \
				( 0 != mode_5G ) 
			  )
			{
				wifi_setBandSteeringEnable( FALSE );
				enable = FALSE;
			}
		}
#endif
		pBandSteeringOption->bEnable 	= enable;			

#if defined(_ENABLE_BAND_STEERING_)
		//To get Band Steering Capability
		wifi_getBandSteeringCapability( &support );
#endif
		pBandSteeringOption->bCapability = support;

#if defined(_ENABLE_BAND_STEERING_)
		//To get Band Steering ApGroup
		wifi_getBandSteeringApGroup( apgroup );
#endif
		AnscCopyString(pBandSteeringOption->APGroup, apgroup);
	}

	return ANSC_STATUS_SUCCESS;
}


ANSC_STATUS 
CosaDmlWiFi_GetBandSteeringLog(CHAR *BandHistory, INT TotalNoOfChars)
{
	INT    record_index=0;
	ULONG  SteeringTime = 0;     
	INT    SourceSSIDIndex = 0, 
		   DestSSIDIndex = 0, 
		   SteeringReason = 0;
	CHAR  ClientMAC[ 48 ] = {0};
	CHAR  band_history_for_one_record[ 96 ] = {0};

   //Records is hardcoded now. This can be changed according to requirement.		  
    int NumOfRecords = 10;		  	  
    int ret = 0;		  
    if (!BandHistory) return ANSC_STATUS_FAILURE;

	// To take BandSteering History for 10 records 
	memset( BandHistory, 0, TotalNoOfChars );

#if defined(_ENABLE_BAND_STEERING_)
	while( !ret)
	{
		SteeringTime    = 0; 
		SourceSSIDIndex = 0; 
		DestSSIDIndex   = 0; 
		SteeringReason  = 0;

		memset( ClientMAC, 0, sizeof( ClientMAC ) );
		memset( band_history_for_one_record, 0, sizeof( band_history_for_one_record ) );		
		
		//Steering history
		ret = wifi_getBandSteeringLog( record_index, 
								 &SteeringTime, 
								 ClientMAC, 
								 &SourceSSIDIndex, 
								 &DestSSIDIndex, 
								 &SteeringReason );
				
				
		//Entry not fund		
		if (ret != 0) {
			//return ANSC_STATUS_SUCCESS;
                        break;
		}						 
		++record_index;						 
				 
	}
        --record_index;
	//strcat( BandHistory, "\n");
	while(record_index >=0 && NumOfRecords >0)
	{
		ret = wifi_getBandSteeringLog( record_index, 
								 &SteeringTime, 
								 ClientMAC, 
								 &SourceSSIDIndex, 
								 &DestSSIDIndex, 
								 &SteeringReason );
				
				
		//Entry not fund		
		if (ret != 0) {
			//return ANSC_STATUS_SUCCESS;
                        break;
		}						 
		snprintf( band_history_for_one_record, sizeof( band_history_for_one_record )-1,
				 "\n%lu|%s|%d|%d|%d",
				 SteeringTime,
				 ClientMAC,
				 SourceSSIDIndex,
				 DestSSIDIndex,
				 SteeringReason );
		if((strlen(BandHistory)+strlen(band_history_for_one_record))<TotalNoOfChars)
		strcat( BandHistory, band_history_for_one_record);
                --NumOfRecords;
                --record_index;
	}
#endif	
	return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS 
CosaDmlWiFi_SetBandSteeringOptions(PCOSA_DML_WIFI_BANDSTEERING_OPTION  pBandSteeringOption)
{
#if defined(_ENABLE_BAND_STEERING_)
    if (!pBandSteeringOption) return ANSC_STATUS_FAILURE;
	
	/*
	  * Check whether call coming for BS enable. If enable then we have to 
	  * consider some cases for ACL
	  */
	if( TRUE == pBandSteeringOption->bEnable )
	{
		char *strValue	 = NULL;
		char  recName[ 256 ];
		int   retPsmGet  = CCSP_SUCCESS,
			  mode_24G	 = 0,
			  mode_5G	 = 0;

		//MacFilter mode for private 2.4G
		memset ( recName, 0, sizeof( recName ) );
		sprintf( recName, MacFilterMode, 1 );
		retPsmGet = PSM_Get_Record_Value2( bus_handle, g_Subsystem, recName, NULL, &strValue );
		if ( CCSP_SUCCESS == retPsmGet ) 
		{
			mode_24G = _ansc_atoi( strValue );
			((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc( strValue );
		}
		
		//MacFilter mode for private 5G
		memset ( recName, 0, sizeof( recName ) );
		sprintf( recName, MacFilterMode, 2 );
		retPsmGet = PSM_Get_Record_Value2( bus_handle, g_Subsystem, recName, NULL, &strValue );
		if ( CCSP_SUCCESS == retPsmGet ) 
		{
			mode_5G = _ansc_atoi( strValue );
			((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc( strValue );
		}
		
		/*
		  * Any one of private wifi ACL mode enabled then don't allow to enable BS
		  */
		if( ( 0 != mode_24G ) || \
			( 0 != mode_5G ) 
		  )
		{
			pBandSteeringOption->bEnable = FALSE;
			return ANSC_STATUS_FAILURE;
		}
	}

	//To turn on/off Band steering
  
  wifi_setBandSteeringApGroup( pBandSteeringOption->APGroup );
  wifi_setBandSteeringEnable( pBandSteeringOption->bEnable );

#endif
	return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS 
CosaDmlWiFi_GetBandSteeringSettings(int radioIndex, PCOSA_DML_WIFI_BANDSTEERING_SETTINGS pBandSteeringSettings)
{
	if( NULL != pBandSteeringSettings )
	{
		INT  PrThreshold   = 0,
			 RssiThreshold = 0,
			 BuThreshold   = 0,
			 OvrLdInactiveTime = 0,
			 IdlInactiveTime = 0;

	
		//to read the band steering physical modulation rate threshold parameters
		#if defined(_ENABLE_BAND_STEERING_)
		wifi_getBandSteeringPhyRateThreshold( radioIndex, &PrThreshold );
		#endif
		pBandSteeringSettings->PhyRateThreshold = PrThreshold;
		
		//to read the band steering band steering RSSIThreshold parameters
		#if defined(_ENABLE_BAND_STEERING_)
		wifi_getBandSteeringRSSIThreshold( radioIndex, &RssiThreshold );
		#endif
		pBandSteeringSettings->RSSIThreshold    = RssiThreshold;

		//to read the band steering BandUtilizationThreshold parameters 
		#if defined(_ENABLE_BAND_STEERING_)
		wifi_getBandSteeringBandUtilizationThreshold( radioIndex, &BuThreshold );
		#endif
		pBandSteeringSettings->UtilizationThreshold    = BuThreshold;

		//to read the band steering OverloadInactiveTime  parameters
		#if defined(_ENABLE_BAND_STEERING_)
		wifi_getBandSteeringOverloadInactiveTime( radioIndex, &OvrLdInactiveTime );
		#endif
		pBandSteeringSettings->OverloadInactiveTime    = OvrLdInactiveTime;

		//to read the band steering IdlInactiveTime parameters
		#if defined(_ENABLE_BAND_STEERING_)
		wifi_getBandSteeringIdleInactiveTime( radioIndex, &IdlInactiveTime );
		#endif
		pBandSteeringSettings->IdleInactiveTime    = IdlInactiveTime;

		// Take copy default band steering settings 
		memcpy( &sWiFiDmlBandSteeringStoredSettinngs[ radioIndex ], 
				pBandSteeringSettings, 
				sizeof( COSA_DML_WIFI_BANDSTEERING_SETTINGS ) );
	}
}

ANSC_STATUS 
CosaDmlWiFi_SetBandSteeringSettings(int radioIndex, PCOSA_DML_WIFI_BANDSTEERING_SETTINGS pBandSteeringSettings)
{
	int ret=0;
	if( NULL != pBandSteeringSettings )
	{
		BOOLEAN bChanged = FALSE;
		
		//to set the band steering physical modulation rate threshold parameters
		if( pBandSteeringSettings->PhyRateThreshold != sWiFiDmlBandSteeringStoredSettinngs[ radioIndex ].PhyRateThreshold ) 
		{
			#if defined(_ENABLE_BAND_STEERING_)
			ret=wifi_setBandSteeringPhyRateThreshold( radioIndex, pBandSteeringSettings->PhyRateThreshold );
                        if(ret) CcspWifiTrace(("RDK_LOG_INFO, WIFI :Phyrate setting failed \n" ));
			#endif
			bChanged = TRUE;
		}
		
		//to set the band steering band steering RSSIThreshold parameters
		if( pBandSteeringSettings->RSSIThreshold != sWiFiDmlBandSteeringStoredSettinngs[ radioIndex ].RSSIThreshold ) 
		{
			
			#if defined(_ENABLE_BAND_STEERING_)
			ret=wifi_setBandSteeringRSSIThreshold( radioIndex, pBandSteeringSettings->RSSIThreshold );
                        if(ret) CcspWifiTrace(("RDK_LOG_INFO, WIFI RSSI setting failed  \n"));
			#endif
			bChanged = TRUE;			
		}

		//to set the band steering BandUtilizationThreshold parameters 
		if( pBandSteeringSettings->UtilizationThreshold != sWiFiDmlBandSteeringStoredSettinngs[ radioIndex ].UtilizationThreshold ) 
		{
			#if defined(_ENABLE_BAND_STEERING_)
			ret=wifi_setBandSteeringBandUtilizationThreshold( radioIndex, pBandSteeringSettings->UtilizationThreshold);
                        if(ret) CcspWifiTrace(("RDK_LOG_INFO, WIFI :Utilization setting failed \n"));
			#endif
			bChanged = TRUE;
		}

		//to set the band steering OverloadInactiveTime parameters
		if( pBandSteeringSettings->OverloadInactiveTime != sWiFiDmlBandSteeringStoredSettinngs[ radioIndex ].OverloadInactiveTime )
		{
			#if defined(_ENABLE_BAND_STEERING_)
			ret=wifi_setBandSteeringOverloadInactiveTime( radioIndex, pBandSteeringSettings->OverloadInactiveTime);
			if(ret) CcspWifiTrace(("RDK_LOG_INFO, WIFI :OverloadInactiveTime setting failed \n"));
			#endif
			bChanged = TRUE;
		}

		//to set the band steering IdleInactiveTime parameters
		if( pBandSteeringSettings->IdleInactiveTime != sWiFiDmlBandSteeringStoredSettinngs[ radioIndex ].IdleInactiveTime )
		{
			#if defined(_ENABLE_BAND_STEERING_)
			ret=wifi_setBandSteeringIdleInactiveTime( radioIndex, pBandSteeringSettings->IdleInactiveTime);
			if(ret) CcspWifiTrace(("RDK_LOG_INFO, WIFI :IdleInactiveTime setting failed \n"));
			#endif
			bChanged = TRUE;
		}

		if( bChanged )
		{
			// Take copy of band steering settings from current values
			memcpy( &sWiFiDmlBandSteeringStoredSettinngs[ radioIndex ], 
					pBandSteeringSettings, 
					sizeof( COSA_DML_WIFI_BANDSTEERING_SETTINGS ) );
		}
	}
}


ANSC_STATUS 
CosaDmlWiFi_GetATMOptions(PCOSA_DML_WIFI_ATM  pATM) {	
	if( NULL == pATM )
		return ANSC_STATUS_FAILURE;
	
#ifdef _ATM_SUPPORT
	wifi_getATMCapable(&pATM->Capable);
	wifi_getATMEnable(&pATM->Enable);
#endif
	return ANSC_STATUS_SUCCESS;
}



ANSC_STATUS
CosaWifiRegGetATMInfo( ANSC_HANDLE   hThisObject){
    PCOSA_DML_WIFI_ATM        		pATM    = (PCOSA_DML_WIFI_ATM     )hThisObject;
    int g=0;
	int s=0;
	UINT percent=0;
	UCHAR buf[256]={0};
	char *token=NULL, *dev=NULL;
	
	pATM->grpCount=ATM_GROUP_COUNT;
	for(g=0; g<pATM->grpCount; g++) {
		snprintf(pATM->APGroup[g].APList, COSA_DML_WIFI_ATM_MAX_APLIST_STR_LEN, "%d,%d", g*2+1, g*2+2);	
			
#ifdef _ATM_SUPPORT
		wifi_getApATMAirTimePercent(g*2, &percent);
		wifi_getApATMSta(g*2, buf, sizeof(buf)); 
#endif
		
		if(percent<=100)
			pATM->APGroup[g].AirTimePercent=percent;
		
		//"$MAC $ATM_percent|$MAC $ATM_percent|$MAC $ATM_percent"
		token = strtok(buf, "|");
		while(token != NULL) {
			dev=strchr(token, ' ');
			if(dev) {
				*dev=0; 
				dev+=1;
				strncpy(pATM->APGroup[g].StaList[s].MACAddress, token, 18);
				pATM->APGroup[g].StaList[s].AirTimePercent=_ansc_atoi(dev); 
				pATM->APGroup[g].StaList[s].pAPList=&pATM->APGroup[g].APList;
				s++;
			}
			token = strtok(NULL, "|");		
		}
                pATM->APGroup[g].NumberSta=s;
                s=0;
                memset(buf,0,256);
                percent=0;
	}
fprintf(stderr, "---- %s ???load from PSM\n", __func__);
//??? load from PSM
	
	return ANSC_STATUS_SUCCESS;
}


ANSC_STATUS
CosaDmlWiFi_SetATMEnable(PCOSA_DML_WIFI_ATM pATM, BOOL bValue) {
	if( NULL == pATM )
		return ANSC_STATUS_FAILURE;
		
	pATM->Enable=bValue;
#ifdef _ATM_SUPPORT
	wifi_setATMEnable(bValue);
#endif
	return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_SetATMAirTimePercent(char *APList, UINT AirTimePercent) {
	char str[128];
	char *token=NULL;
	int apIndex=-1;
	char *restStr=NULL;
	
	strncpy(str, APList, 127); 
	token = strtok_r(str, ",",&restStr);
    while(token != NULL) {
		apIndex = _ansc_atoi(token)-1; 
		if(apIndex>=0) {
fprintf(stderr, "---- %s %s %d %d\n", __func__, "wifi_setApATMAirTimePercent", apIndex, AirTimePercent);
#ifdef _ATM_SUPPORT
			wifi_setApATMAirTimePercent(apIndex, AirTimePercent);
#endif
		}		
        token = strtok_r(restStr, ",", &restStr);		
    }
	return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_SetATMSta(char *APList, char *MACAddress, UINT AirTimePercent) {
	char str[128];
	char *token=NULL;
	int apIndex=-1;
	char *restStr=NULL;
	
	strncpy(str, APList, 127); 
	token = strtok_r(str, ",", &restStr);
    while(token != NULL) {
		apIndex = _ansc_atoi(token)-1; 
		if(apIndex>=0) {
#ifdef _ATM_SUPPORT
			wifi_setApATMSta(apIndex, MACAddress, AirTimePercent);
#endif
		}		
        token = strtok_r(restStr, ",", &restStr);		
    }
	return ANSC_STATUS_SUCCESS;

}





//zqiu >>

int init_client_socket(int *client_fd){
	
	
int sockfd, n;
    if (!client_fd) return ANSC_STATUS_FAILURE;
#ifdef DUAL_CORE_XB3
	struct sockaddr_in serv_addr;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
	{		
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI-CLIENT <%s> <%d> : ERROR opening socket \n",__FUNCTION__, __LINE__));
		return -1;
	}	
	bzero((char *) &serv_addr, sizeof(serv_addr));

		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(5001);

	if(inet_pton(AF_INET,"192.168.254.252", &(serv_addr.sin_addr))<=0)
    {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI-CLIENT <%s> <%d> : inet_pton error occured \n",__FUNCTION__, __LINE__));
        return -1;
    } 
#else
	#define WIFI_SERVER_FILE_NAME  "/tmp/wifi.sock"
	struct sockaddr_un serv_addr; 
	sockfd = socket(PF_UNIX,SOCK_STREAM,0);
	if(sockfd < 0 )
		return -1;
	serv_addr.sun_family=AF_UNIX;  
	strcpy(serv_addr.sun_path,WIFI_SERVER_FILE_NAME); 
#endif	
	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)   
    {  
        close(sockfd);  		
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI-CLIENT <%s> <%d> : Error in connecting socket \n",__FUNCTION__, __LINE__));
        return -1;  
    }
    *client_fd = sockfd;
    return 0;

}

int send_to_socket(void *buff, int buff_size)
{
    int ret;
    int fd;

    ret = init_client_socket(&fd);
    if(ret != 0){		
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI-CLIENT <%s> <%d> : init_client_socket error \n",__FUNCTION__, __LINE__));
        return -1;
    }

    ret = write(fd, buff, buff_size);
	if (ret < 0) 
	{		
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI-CLIENT <%s> <%d> : ERROR writing to socket \n",__FUNCTION__, __LINE__));
	}

    close(fd);
    return 0;
}


#ifdef USE_NOTIFY_COMPONENT
extern void *bus_handle;

void Send_Associated_Device_Notification(int i,ULONG old_val, ULONG new_val)
{

	char  str[512] = {0};
	parameterValStruct_t notif_val[1];
	char param_name[256] = "Device.NotifyComponent.SetNotifi_ParamName";
	char compo[256] = "eRT.com.cisco.spvtg.ccsp.notifycomponent";
	char bus[256] = "/com/cisco/spvtg/ccsp/notifycomponent";
	char* faultParam = NULL;
	int ret = 0;	

	
	sprintf(str,"Device.WiFi.AccessPoint.%d.AssociatedDeviceNumberOfEntries,%lu,%lu,%lu,%d",i,0,new_val,old_val,ccsp_unsignedInt);
	notif_val[0].parameterName =  param_name ;
	notif_val[0].parameterValue = str;
	notif_val[0].type = ccsp_string;

	ret = CcspBaseIf_setParameterValues(
		  bus_handle,
		  compo,
		  bus,
		  0,
		  0,
		  notif_val,
		  1,
		  TRUE,
		  &faultParam
		  );

	if(ret != CCSP_SUCCESS)
		CcspWifiTrace(("RDK_LOG_WARN, RDKB_WIFI_CNNECTED_CLIENT : Sending Notification Fail \n"));

}

//zqiu:
void Send_Notification_for_hotspot(char *mac, BOOL add, int ssidIndex, int rssi) {
	int ret;
	
	char objName[256]="Device.X_COMCAST-COM_GRE.Tunnel.ClientChange";
	char objValue[256]={0};
    if (!mac) return;
	parameterValStruct_t  value[1] = { objName, objValue, ccsp_string};
	
	char dst_pathname_cr[64]  =  {0};
	componentStruct_t **        ppComponents = NULL;
	int size =0;
	
	CCSP_MESSAGE_BUS_INFO *bus_info = (CCSP_MESSAGE_BUS_INFO *)bus_handle;
	char* faultParam = NULL;
    
	snprintf(objValue, sizeof(objValue), "%d|%d|%d|%s", (int)add, ssidIndex, rssi, mac);
	fprintf(stderr, "--  try to set %s=%s\n", objName, objValue);
	
	snprintf(dst_pathname_cr, sizeof(dst_pathname_cr), "%s%s", g_Subsystem, CCSP_DBUS_INTERFACE_CR);
	ret = CcspBaseIf_discComponentSupportingNamespace(
				bus_handle,
				dst_pathname_cr,
				objName,
				g_Subsystem,        /* prefix */
				&ppComponents,
				&size);
	if ( ret != CCSP_SUCCESS ) {
		fprintf(stderr, "Error:'%s' is not exist\n", objName);
		return;
	}	

	ret = CcspBaseIf_setParameterValues(
				bus_handle,
				ppComponents[0]->componentName,
				ppComponents[0]->dbusPath,
				0, 0x0,   /* session id and write id */
				&value,
				1,
				TRUE,   /* no commit */
				&faultParam
			);

	if (ret != CCSP_SUCCESS && faultParam) {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s Failed to SetValue for param '%s' and ret val is %d\n",__FUNCTION__,faultParam,ret));
		fprintf(stderr, "Error:Failed to SetValue for param '%s'\n", faultParam);
		bus_info->freefunc(faultParam);
	}
	free_componentStruct_t(bus_handle, 1, ppComponents);
	return;
}

int sMac_to_cMac(char *sMac, unsigned char *cMac) {
	unsigned int iMAC[6];
	int i=0;
	if (!sMac || !cMac) return 0;
	sscanf(sMac, "%x:%x:%x:%x:%x:%x", &iMAC[0], &iMAC[1], &iMAC[2], &iMAC[3], &iMAC[4], &iMAC[5]);
	for(i=0;i<6;i++) 
		cMac[i] = (unsigned char)iMAC[i];
	
	return 0;
}

int cMac_to_sMac(unsigned char *cMac, char *sMac) {
	if (!sMac || !cMac) return 0;
	snprintf(sMac, 32, "%02X:%02X:%02X:%02X:%02X:%02X", cMac[0],cMac[1],cMac[2],cMac[3],cMac[4],cMac[5]);
	return 0;
}

BOOL wifi_is_mac_in_macfilter(int apIns, char *mac) {
	BOOL found=FALSE;
	int retPsmGet = CCSP_SUCCESS;
	char recName[256];
	char *macAddress = NULL;
	int i;

	unsigned int 		InstNumCount    = 0;
	unsigned int*		pInstNumList    = NULL;

    if (!mac) return false;
	memset(recName, 0, sizeof(recName));
	sprintf(recName, "Device.WiFi.AccessPoint.%d.X_CISCO_COM_MacFilterTable.", apIns);

/*
This call gets instances of table and total count.
*/
	if(CcspBaseIf_GetNextLevelInstances
	(
		bus_handle,
		WIFI_COMP,
		WIFI_BUS,
		recName,
		&InstNumCount,
		&pInstNumList
	) == CCSP_SUCCESS)
	{

		/*
		This loop checks if mac address is matching with the new mac to be added.
		*/
		for(i=InstNumCount-1; i>=0; i--)
		{
			memset(recName, 0, sizeof(recName));
			sprintf(recName, MacFilter, apIns, pInstNumList[i]);

			retPsmGet = PSM_Get_Record_Value2(bus_handle,g_Subsystem, recName, NULL, &macAddress);
			if (retPsmGet == CCSP_SUCCESS)
			{
				if(strcasecmp(mac, macAddress)==0)
					found=TRUE;

				((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(macAddress);

				if(found)
				{
					break;
				}
			}
		}

		if(pInstNumList)
			free(pInstNumList);

        }
	else
	{
		printf("MAC_FILTER : CcspBaseIf_GetNextLevelInstances failed \n");
	}

	return found;
}

void Hotspot_MacFilter_AddEntry(char *mac)
{

    char table_name[512] = {0};
    char param_name[HOTSPOT_NO_OF_INDEX*2] [512] = {0};
    char *param_value=HOTSPOT_DEVICE_NAME;
    int i, j , table_index[HOTSPOT_NO_OF_INDEX]={0};
    parameterValStruct_t param[HOTSPOT_NO_OF_INDEX*2];
    char* faultParam = NULL;
    int ret = CCSP_FAILURE;

    if (!mac) return;
    
	/*
	This Loop checks if mac is already present in the table. If it is present it will continue for next index.
	In case , if mac is not present or add entry fails, table_index[i] will be 0.
	In success case, it will add table entry but values will be blank.
	*/
	for(i=0 ; i<HOTSPOT_NO_OF_INDEX ; i++)
	{
		if(wifi_is_mac_in_macfilter(Hotspot_Index[i], mac))
		{
			continue;
		}
		
		sprintf(table_name,"Device.WiFi.AccessPoint.%d.X_CISCO_COM_MacFilterTable.",Hotspot_Index[i]);
		table_index[i] = Cosa_AddEntry(WIFI_COMP,WIFI_BUS,table_name);

		if( table_index[i] == 0)
		{
		    printf("MAC_FILTER : Access point = %d  Add table failed \n", Hotspot_Index[i]);
		}
	}

/*
This loop check for non zero table_index and fill the parameter values for them.
*/
	for(i=0 , j= 0 ; j<HOTSPOT_NO_OF_INDEX ; j++)
	{
		if( table_index[j] != 0)
		{
		        sprintf( param_name[i], "Device.WiFi.AccessPoint.%d.X_CISCO_COM_MacFilterTable.%d.MACAddress", Hotspot_Index[j],table_index[j] );
		        param[i].parameterName =  param_name[i] ;
		        param[i].parameterValue = mac;
		        param[i].type = ccsp_string;
		        i++;

		        sprintf( param_name[i], "Device.WiFi.AccessPoint.%d.X_CISCO_COM_MacFilterTable.%d.DeviceName", Hotspot_Index[j],table_index[j] );
		        param[i].parameterName =  param_name[i] ;
		        param[i].parameterValue = param_value;
		        param[i].type = ccsp_string;
		        i++;
		}
	}

/*
This is a dbus bulk set call for all parameters of table at one go
*/
	if(i > 0)
	{
	    ret = CcspBaseIf_setParameterValues(
	          bus_handle,
	          WIFI_COMP,
	          WIFI_BUS,
	          0,
	          0,
	          param,
	          i,
	          TRUE,
	          &faultParam
	          );

	        if(ret != CCSP_SUCCESS)
	        {
	            printf("MAC_FILTER : CcspBaseIf_setParameterValues failed\n");
	        }
	}
}

void Hotspot_Macfilter_sync(char *mac) {
    if (!mac) return;
	fprintf(stderr, "---- %s %d\n", __func__, __LINE__);
	Hotspot_MacFilter_AddEntry(mac);
}

void *Wifi_Hosts_Sync_Func(void *pt, int index, wifi_associated_dev_t *associated_dev, BOOL bCallForFullSync)
{
	char *expMacAdd=(char *)pt;	
	int i , j , len = 0;
	char ssid[256]= {0};
	char mac_id[256] = {0},rec_mac_id[256] = {0};
	char assoc_device[256] = {0};
	ULONG count = 0;
	PCOSA_DML_WIFI_AP_ASSOC_DEVICE assoc_devices = NULL;
	LM_wifi_hosts_t hosts;
	static ULONG backup_count[4]={0};
	BOOL enabled=FALSE; 
	CcspWifiTrace(("RDK_LOG_WARN, %s-%d \n",__FUNCTION__,__LINE__));

  
	memset(&hosts,0,sizeof(LM_wifi_hosts_t));
	memset(assoc_device,0,sizeof(assoc_device));
	memset(ssid,0,sizeof(ssid));
	memset(rec_mac_id,0,sizeof(rec_mac_id));
	memset(mac_id,0,sizeof(mac_id));
	
		
	if(bCallForFullSync == 0)  // Single device callback notification
	{
		if (!associated_dev) return NULL;
		
			wifi_getApEnable(index-1, &enabled);
			if (enabled == FALSE) 
				return NULL; 
			
			wifi_getApName(index-1, ssid);
			
	        count = 0;			
			assoc_devices = CosaDmlWiFiApGetAssocDevices(NULL, ssid , &count);

			if(backup_count[index-1] != count) // Notification for AssociatedDeviceNumberOfEntries
			{
				Send_Associated_Device_Notification(index,backup_count[index-1],count);
				backup_count[index-1] = count;
			}
			
		_ansc_sprintf
					(
						rec_mac_id,
						"%02X:%02X:%02X:%02X:%02X:%02X",
						associated_dev->cli_MACAddress[0],
						associated_dev->cli_MACAddress[1],
						associated_dev->cli_MACAddress[2],					
						associated_dev->cli_MACAddress[3],					
						associated_dev->cli_MACAddress[4],					
						associated_dev->cli_MACAddress[5]
					);
				rec_mac_id[17] = '\0';	
				_ansc_sprintf(ssid,"Device.WiFi.SSID.%d",index);
			
			for(j = 0; j < count ; j++)
			{
				//CcspWifiTrace(("RDK_LOG_WARN,WIFI-CLIENT <%s> <%d> : j = %d \n",__FUNCTION__, __LINE__ , j));
				_ansc_sprintf
	            (
	                mac_id,
	                "%02X:%02X:%02X:%02X:%02X:%02X",
	                assoc_devices[j].MacAddress[0],
	                assoc_devices[j].MacAddress[1],
	                assoc_devices[j].MacAddress[2],
	                assoc_devices[j].MacAddress[3],
	                assoc_devices[j].MacAddress[4],
	                assoc_devices[j].MacAddress[5]
	            );
				mac_id[17] = '\0';		
				if( 0 == strcmp( rec_mac_id, mac_id ) )
					{
						_ansc_sprintf(assoc_device,"Device.WiFi.AccessPoint.%d.AssociatedDevice.%d",index,j+1);
						break;
					}
				
			}

				strcpy(hosts.host[0].phyAddr,rec_mac_id);
				strcpy(hosts.host[0].ssid,ssid);
				hosts.host[0].RSSI = associated_dev->cli_SignalStrength;	
				hosts.host[0].phyAddr[17] = '\0';
				
			if(associated_dev->cli_Active) // Online Clients Private, XHS
			{
				hosts.host[0].Status = TRUE;
				if(0 == strlen(assoc_device)) // if clients switch to other ssid and not listing in CosaDmlWiFiApGetAssocDevices
				{
					_ansc_sprintf(assoc_device,"Device.WiFi.AccessPoint.%d.AssociatedDevice.%d",index,count+1);
				
				}
				strcpy(hosts.host[0].AssociatedDevice,assoc_device);
			}
			else // Offline Clients Private, XHS.. AssociatedDevice should be null
			{
				hosts.host[0].Status = FALSE;
			}
			CosaDMLWiFi_Send_ReceivedHostDetails_To_LMLite( &(hosts.host[0]) );
	}
	else  // Group notification - on request from LMLite
	{
		for(i = 1; i <=4 ; i++)
		{
			//zqiu:
			//_ansc_sprintf(ssid,"ath%d",i-1);
			wifi_getApEnable(i-1, &enabled);
			if (enabled == FALSE) 
				continue; 
#if !defined(_COSA_BCM_MIPS_)
			wifi_getApName(i-1, ssid);
#else
			_ansc_sprintf(ssid,"ath%d",i-1);	
#endif	
	        count = 0;			
			assoc_devices = CosaDmlWiFiApGetAssocDevices(NULL, ssid , &count);

			if(backup_count[i-1] != count)
			{
				Send_Associated_Device_Notification(i,backup_count[i-1],count);
				backup_count[i-1] = count;
			}

			for(j = 0; j < count ; j++)
			{
				//CcspWifiTrace(("RDK_LOG_WARN,WIFI-CLIENT <%s> <%d> : j = %d \n",__FUNCTION__, __LINE__ , j));
				_ansc_sprintf
	            (
	                mac_id,
	                "%02X:%02X:%02X:%02X:%02X:%02X",
	                assoc_devices[j].MacAddress[0],
	                assoc_devices[j].MacAddress[1],
	                assoc_devices[j].MacAddress[2],
	                assoc_devices[j].MacAddress[3],
	                assoc_devices[j].MacAddress[4],
	                assoc_devices[j].MacAddress[5]
	            );
				
				_ansc_sprintf(ssid,"Device.WiFi.SSID.%d",i);
				_ansc_sprintf(assoc_device,"Device.WiFi.AccessPoint.%d.AssociatedDevice.%d",i,j+1);

				mac_id[17] = '\0';
				strcpy(hosts.host[hosts.count].AssociatedDevice,assoc_device);
				strcpy(hosts.host[hosts.count].phyAddr,mac_id);
				strcpy(hosts.host[hosts.count].ssid,ssid);
				hosts.host[hosts.count].RSSI = assoc_devices[j].SignalStrength;
				hosts.host[hosts.count].Status = TRUE;
				hosts.host[hosts.count].phyAddr[17] = '\0';
    			(hosts.count)++;
			}				
		}
		CcspWifiTrace(("RDK_LOG_WARN, Total Hosts Count is %d\n",hosts.count));
		if(hosts.count)
		CosaDMLWiFi_Send_FullHostDetails_To_LMLite( &hosts );
	}	
				//zqiu:
			if(assoc_devices)
				AnscFreeMemory(assoc_devices);
	return NULL;
}

/* CosaDMLWiFi_Send_ReceivedHostDetails_To_LMLite */
void CosaDMLWiFi_Send_ReceivedHostDetails_To_LMLite(LM_wifi_host_t   *phost)
{
	BOOL bProcessFurther = TRUE;
	
	/* Validate received param. If it is not valid then dont proceed further */
	if( NULL == phost )
	{
		CcspWifiTrace(("RDK_LOG_WARN, %s-%d Recv Param NULL \n",__FUNCTION__,__LINE__));
		bProcessFurther = FALSE;
	}

	if( bProcessFurther )
	{
		/* 
		  * If physical address not having any valid data then no need to 
		  * send corresponding host details to Lm-Lite
		  */
		if( '\0' != phost->phyAddr[ 0 ] )
		{
			parameterValStruct_t notif_val[1];
			char				 param_name[256] = "Device.Hosts.X_RDKCENTRAL-COM_LMHost_Sync_From_WiFi";
			char				 component[256]  = "eRT.com.cisco.spvtg.ccsp.lmlite";
			char				 bus[256]		 = "/com/cisco/spvtg/ccsp/lmlite";
			char				 str[2048]		 = {0};
			char*				 faultParam 	 = NULL;
			int 				 ret			 = 0;	
			
			/* 
			* Group Received Associated Params as below,
			* MAC_Address,AssociatedDevice_Alias,SSID_Alias,RSSI_Signal_Strength,Status
			*/
			snprintf(str, sizeof(str), "%s,%s,%s,%d,%d", 
										phost->phyAddr, 
										('\0' != phost->AssociatedDevice[ 0 ]) ? phost->AssociatedDevice : "NULL", 
										('\0' != phost->ssid[ 0 ]) ? phost->ssid : "NULL", 
										phost->RSSI,
										phost->Status);
			
			CcspWifiTrace(("RDK_LOG_WARN, %s-%d [%s] \n",__FUNCTION__,__LINE__,str));
			
			notif_val[0].parameterName	= param_name;
			notif_val[0].parameterValue = str;
			notif_val[0].type			= ccsp_string;
			
			ret = CcspBaseIf_setParameterValues(  bus_handle,
												  component,
												  bus,
												  0,
												  0,
												  notif_val,
												  1,
												  TRUE,
												  &faultParam
												  );
			
			if(ret != CCSP_SUCCESS)
			CcspWifiTrace(("RDK_LOG_WARN, RDKB_WIFI_CNNECTED_CLIENT : Sending Notification Fail \n"));
		}
		else
		{
			CcspWifiTrace(("RDK_LOG_WARN, Sending Notification Fail Bcoz NULL MAC Address \n"));
		}
	}
}

/* CosaDMLWiFi_Send_HostDetails_To_LMLite */
void CosaDMLWiFi_Send_FullHostDetails_To_LMLite(LM_wifi_hosts_t *phosts)
{
	BOOL bProcessFurther = TRUE;
	CcspWifiTrace(("RDK_LOG_WARN, %s-%d \n",__FUNCTION__,__LINE__));
	
	/* Validate received param. If it is not valid then dont proceed further */
	if( NULL == phosts )
	{
		CcspWifiTrace(("RDK_LOG_WARN, %s-%d Recv Param NULL \n",__FUNCTION__,__LINE__));
		bProcessFurther = FALSE;
	}

	if( bProcessFurther )
	{
		parameterValStruct_t notif_val[1];
		char				 param_name[256] = "Device.Hosts.X_RDKCENTRAL-COM_LMHost_Sync_From_WiFi";
		char				 component[256]  = "eRT.com.cisco.spvtg.ccsp.lmlite";
		char				 bus[256]		 = "/com/cisco/spvtg/ccsp/lmlite";
		char				 str[2048]		 = {0};
		char*				 faultParam 	 = NULL;
		int 				 ret			 = 0, 
							 i;	
		
		for(i =0; i < phosts->count ; i++)
		{
			/* 
			  * If physical address not having any valid data then no need to 
			  * send corresponding host details to Lm-Lite
			  */
			if( '\0' != phosts->host[i].phyAddr[ 0 ] )
			{
				/* 
				* Group Received Associated Params as below,
				* MAC_Address,AssociatedDevice_Alias,SSID_Alias,RSSI_Signal_Strength,Status
				*/
				snprintf(str, sizeof(str), "%s,%s,%s,%d,%d", 
											phosts->host[i].phyAddr, 
											('\0' != phosts->host[i].AssociatedDevice[ 0 ]) ? phosts->host[i].AssociatedDevice : "NULL", 
											('\0' != phosts->host[i].ssid[ 0 ]) ? phosts->host[i].ssid : "NULL", 
											phosts->host[i].RSSI,
											phosts->host[i].Status);
				
				CcspWifiTrace(("RDK_LOG_WARN, %s-%d [%s] \n",__FUNCTION__,__LINE__,str));
				
				notif_val[0].parameterName	= param_name;
				notif_val[0].parameterValue = str;
				notif_val[0].type			= ccsp_string;
				
				ret = CcspBaseIf_setParameterValues(  bus_handle,
													  component,
													  bus,
													  0,
													  0,
													  notif_val,
													  1,
													  TRUE,
													  &faultParam
													  );
				
				if(ret != CCSP_SUCCESS)
				CcspWifiTrace(("RDK_LOG_WARN, RDKB_WIFI_CNNECTED_CLIENT : Sending Notification Fail \n"));
			}
			else
			{
				CcspWifiTrace(("RDK_LOG_WARN, Sending Notification Fail Bcoz NULL MAC Address \n"));
			}
		}
	}
}

//dispatch the notification here
INT CosaDmlWiFi_AssociatedDevice_callback(INT apIndex, wifi_associated_dev_t *associated_dev) {    
	char mac[32]={0};
	BOOL bEnabled;
	if(!associated_dev)
		return -1;
	
	cMac_to_sMac(associated_dev->cli_MACAddress, mac);	
fprintf(stderr, "-- %s : %d %s %d %d\n", __func__, apIndex, mac, associated_dev->cli_Active, associated_dev->cli_SignalStrength);	
	if(apIndex==0 || apIndex==1) {	//for private network
		if(associated_dev->cli_Active == 1) 
		{
			Wifi_Hosts_Sync_Func(NULL,(apIndex+1), associated_dev, 0);		
			CosaDmlWiFi_GetPreferPrivatePsmData(&bEnabled);
			if (bEnabled == TRUE)
			{
				Hotspot_Macfilter_sync(mac);
			}
		}
		else 				
		{
			Wifi_Hosts_Sync_Func((void *)mac, (apIndex+1), associated_dev, 0);		
		}
	} else if (apIndex==4 || apIndex==5 || apIndex==8 || apIndex==9) { //for hotspot
		Send_Notification_for_hotspot(mac, associated_dev->cli_Active, apIndex+1, associated_dev->cli_SignalStrength);
	} else if (apIndex==2 || apIndex==3 ) { //XHS
                if(associated_dev->cli_Active == 1)
                {
                        Wifi_Hosts_Sync_Func(NULL,(apIndex+1), associated_dev, 0);
                }
                else
                {
                        Wifi_Hosts_Sync_Func((void *)mac,(apIndex+1), associated_dev, 0);
                }	
	} else if (apIndex==6 || apIndex==7 ||  apIndex==10 || apIndex==11 ) { //L&F
	
	} else if (apIndex==14 || apIndex==15 ) { //guest
	
	} else {
		//unused ssid
	}	
	return 0;
}




		
		
#endif //USE_NOTIFY_COMPONENT

INT m_wifi_init() {
	INT ret=wifi_init();

#if defined(ENABLE_FEATURE_MESHWIFI)
	system("/usr/ccsp/wifi/mesh_aclmac.sh allow; /usr/ccsp/wifi/mesh_setip.sh; ");
	// notify mesh components that wifi init was performed.
	CcspWifiTrace(("RDK_LOG_INFO,WIFI %s : Notify Mesh of wifi_init\n",__FUNCTION__));
	system("/usr/bin/sysevent set wifi_init true");
#endif
	return ret;
}
//zqiu <<

#if defined(ENABLE_FEATURE_MESHWIFI)

static BOOL WiFiSysEventHandlerStarted=FALSE;
static int sysevent_fd = 0;
static token_t sysEtoken;
static async_id_t async_id[3];

enum {SYS_EVENT_ERROR=-1, SYS_EVENT_OK, SYS_EVENT_TIMEOUT, SYS_EVENT_HANDLE_EXIT, SYS_EVENT_RECEIVED=0x10};

//extern void *bus_handle;
INT Mesh_Notification(char *event, char *data) {
        char *token=NULL;
        int ret = 0;
 
        int apIndex=-1;
        int radioIndex=-1;
        char ssidName[128]="";
        int channel=0;
        char passphrase[128]="";
        PCOSA_DATAMODEL_WIFI pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
        PSINGLE_LINK_ENTRY pSLinkEntry = NULL;
        PCOSA_DML_WIFI_SSID  pWifiSsid = NULL;
        PCOSA_DML_WIFI_AP      pWifiAp = NULL;

 
        if(!pMyObject) {
            CcspTraceError(("%s Data Model object is NULL!\n",__FUNCTION__));
            return -1;
        }
        if(strncmp(data, "MESH|", 5)!=0) {
            // CcspTraceInfo(("%s Ignore non-mesh notification!\n",__FUNCTION__));
            return -1;  //non mesh notification
        }

        CcspTraceInfo(("%s Received event %s with data = %s\n",__FUNCTION__,event,data));

        if(strcmp(event, "wifi_SSIDName")==0) {
                //MESH|apIndex|ssidName
                if((token = strtok(data+5, "|"))==NULL) {
                    CcspTraceError(("%s Bad event data format\n",__FUNCTION__));
                    return -1;
                }
                sscanf(token, "%d", &apIndex);
                if(apIndex<0 || apIndex>16) {
                        CcspTraceError(("apIndex error:%s\n", apIndex));
                        return -1;
                }
                if((token = strtok(NULL, "|"))==NULL) {
                    CcspTraceError(("%s Bad event data format\n",__FUNCTION__));
                    return -1;
                }
                strncpy(ssidName, token, sizeof(ssidName));
 
                if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->SsidQueue, apIndex))==NULL) {
                    CcspTraceError(("%s Data Model object not found!\n",__FUNCTION__));
                    return -1;
                }
                if((pWifiSsid=ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext)==NULL) {
                    CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
                    return -1;
                }
                strncpy(pWifiSsid->SSID.Cfg.SSID, ssidName, COSA_DML_WIFI_MAX_SSID_NAME_LEN);
                return 0;
 
        } else if (strcmp(event, "wifi_RadioChannel")==0) {
                //MESH|radioIndex|channel
                if((token = strtok(data+5, "|"))==NULL) {
                    CcspTraceError(("%s Bad event data format\n",__FUNCTION__));
                    return -1;
                }
                sscanf(token, "%d", &radioIndex);
                if(radioIndex<0 || radioIndex>2) {
                        CcspTraceError(("radioIndex error:%s\n", radioIndex));
                        return -1;
                }
                if((token = strtok(NULL, "|"))==NULL) {
                    CcspTraceError(("%s Bad event data format\n",__FUNCTION__));
                    return -1;
                }
                sscanf(token, "%d", &channel);
                if(channel<0 || channel>165) {
                        CcspTraceError(("channel error:%s\n", channel));
                        return -1;
                }
 
                return 0;
        } else if (strcmp(event, "wifi_ApSecurity")==0) {
                //MESH|apIndex|passphrase|secMode|encMode
                if((token = strtok(data+5, "|"))==NULL) {
                    CcspTraceError(("%s Bad event data format\n",__FUNCTION__));
                    return -1;
                }
                sscanf(token, "%d", &apIndex);
                if(apIndex<0 || apIndex>16) {
                        CcspTraceError(("apIndex error:%s\n", apIndex));
                        return -1;
                }
                if((token = strtok(NULL, "|"))==NULL) {
                    CcspTraceError(("%s Bad event data format\n",__FUNCTION__));
                    return -1;
                }
                strncpy(passphrase, token, sizeof(passphrase));
 
                if((pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->AccessPointQueue, apIndex))==NULL) {
                   CcspTraceError(("%s Data Model object not found!\n",__FUNCTION__));
                   return -1;
                }
                if((pWifiAp=ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry)->hContext)==NULL) {
                    CcspTraceError(("%s Error linking Data Model object!\n",__FUNCTION__));
                    return -1;
                }
                strncpy(pWifiAp->SEC.Cfg.PreSharedKey, passphrase, 65);
                return 0;
        }
 
        CcspTraceWarning(("unmatch event: %s\n", event));
        return ret;
}

/*
 * Initialize sysevnt
 *   return 0 if success and -1 if failure.
 */
int wifi_sysevent_init(void)
{
    int rc;

    CcspWifiTrace(("wifi_sysevent_init\n"));

    sysevent_fd = sysevent_open("127.0.0.1", SE_SERVER_WELL_KNOWN_PORT, SE_VERSION, "wifi_agent", &sysEtoken);
    if (!sysevent_fd) {
        return(SYS_EVENT_ERROR);
    }

    /*you can register the event as you want*/

	//register wifi_SSIDName event
    sysevent_set_options(sysevent_fd, sysEtoken, "wifi_SSIDName", TUPLE_FLAG_EVENT);
    rc = sysevent_setnotification(sysevent_fd, sysEtoken, "wifi_SSIDName", &async_id[0]);
    if (rc) {
       return(SYS_EVENT_ERROR);
    }

	//register wifi_RadioChannel event
    sysevent_set_options(sysevent_fd, sysEtoken, "wifi_RadioChannel", TUPLE_FLAG_EVENT);
    rc = sysevent_setnotification(sysevent_fd, sysEtoken, "wifi_RadioChannel", &async_id[1]);
    if (rc) {
       return(SYS_EVENT_ERROR);
    }

	//register wifi_ApSecurity event
    sysevent_set_options(sysevent_fd, sysEtoken, "wifi_ApSecurity", TUPLE_FLAG_EVENT);
    rc = sysevent_setnotification(sysevent_fd, sysEtoken, "wifi_ApSecurity", &async_id[2]);
    if (rc) {
       return(SYS_EVENT_ERROR);
    }

    CcspWifiTrace(("wifi_sysevent_init - Exit\n"));
    return(SYS_EVENT_OK);
}

/*
 * Listen sysevent notification message
 */
int wifi_sysvent_listener(void)
{
    int     ret = SYS_EVENT_TIMEOUT;
    fd_set  rfds;
    struct  timeval tv;
    int     retval;

    unsigned char name[128], val[256];
    int namelen = sizeof(name);
    int vallen	= sizeof(val);
    int err;
    async_id_t getnotification_asyncid;

    CcspWifiTrace(("wifi_sysvent_listener created\n"));

    err = sysevent_getnotification(sysevent_fd, sysEtoken, name, &namelen,  val, &vallen, &getnotification_asyncid); 
    if (err)
    {
        CcspTraceError(("sysevent_getnotification failed with error: %d\n", err));
    }
    else
    {
        CcspTraceWarning(("received notification event %s\n", name));

        Mesh_Notification(name,val);
	ret = SYS_EVENT_RECEIVED;
    }

    return ret;
}

/*
 * Close sysevent
 */
int wifi_sysvent_close(void)
{
    /* we are done with this notification, so unregister it using async_id provided earlier */
    sysevent_rmnotification(sysevent_fd, sysEtoken, async_id[0]);
    sysevent_rmnotification(sysevent_fd, sysEtoken, async_id[1]);
    sysevent_rmnotification(sysevent_fd, sysEtoken, async_id[2]);

    /* close this session with syseventd */
    sysevent_close(sysevent_fd, sysEtoken);

    return (SYS_EVENT_OK);
}

/*
 * check the initialized sysevent status (happened or not happened),
 * if the event happened, call the functions registered for the events previously
 */
int wifi_check_sysevent_status(int fd, token_t token)
{
    char evtValue[256] = {0};
    int  returnStatus = ANSC_STATUS_SUCCESS;

	CcspWifiTrace(("wifi_check_sysevent_status\n"));

    /*wifi_SSIDName event*/
    if( 0 == sysevent_get(fd, token, "wifi_SSIDName", evtValue, sizeof(evtValue)) && '\0' != evtValue[0] )
    {
		Mesh_Notification("wifi_SSIDName",evtValue);
    }

    /*wifi_RadioChannel event*/
    if( 0 == sysevent_get(fd, token, "wifi_RadioChannel", evtValue, sizeof(evtValue)) && '\0' != evtValue[0] )
    {
		Mesh_Notification("wifi_RadioChannel",evtValue);
    }

    /*wifi_ApSecurity event*/
    if( 0 == sysevent_get(fd, token, "wifi_ApSecurity", evtValue, sizeof(evtValue)) && '\0' != evtValue[0] )
    {
		Mesh_Notification("wifi_ApSecurity",evtValue);
    }

    return returnStatus;
}


/*
 * The sysevent handler thread.
 */
static void *wifi_sysevent_handler_th(void *arg)
{
    int ret = SYS_EVENT_ERROR;

	CcspWifiTrace(("wifi_sysevent_handler_th created\n"));

    while(SYS_EVENT_ERROR == wifi_sysevent_init())
    {
        CcspWifiTrace(("%s: sysevent init failed!\n", __FUNCTION__));
        sleep(1);
    }

    /*first check the events status*/
    wifi_check_sysevent_status(sysevent_fd, sysEtoken);

    while(1)
    {
        ret = wifi_sysvent_listener();
        switch (ret)
        {
            case SYS_EVENT_RECEIVED:
                break;
            default :
                CcspWifiTrace(("The received event status is not expected!\n"));
                break;
        }

        if (SYS_EVENT_HANDLE_EXIT == ret) //end this event handling loop
            break;

        sleep(2);
    }

    wifi_sysvent_close();

    return NULL;
}


/*
 * Create a thread to handle the sysevent asynchronously
 */
void wifi_handle_sysevent_async(void)
{
    int err;
    pthread_t event_handle_thread;

    if(WiFiSysEventHandlerStarted)
        return;
    else
        WiFiSysEventHandlerStarted = TRUE;

    CcspWifiTrace(("wifi_handle_sysevent_async...\n"));

    err = pthread_create(&event_handle_thread, NULL, wifi_sysevent_handler_th, NULL);
    if(0 != err)
    {
        CcspWifiTrace(("%s: create the event handle thread error!\n", __FUNCTION__));
    }
}

#endif //ENABLE_FEATURE_MESHWIFI

//zqiu >> DCS
/*typedef struct _wifi_apRssi {
	CHAR  ap_BSSID[6];    		//BSSID
	UINT  ap_channelWidth   	//The channel width; 1 for 20Mhz, 2 for 40 MHz, 4 for 80 MHz, 8 for 160 MHz, 10 for 80+80Mhz
	INT   ap_rssi;       		//RSSI of the neighboring AP in dBm.
} wifi_apRssi_t;

typedef struct _wifi_channelMetrics {
	INT  channel_number;		//each channel is only 20MHz bandwidth
	BOOL channel_in_pool; 	    //If channel_in_pool is false, driver do not need to scan this channel
	INT  channel_noise;		    //this is used to return the average noise floor in dbm
	BOOL channel_radar_noise;	//if channel_number is in DFS channel, this is used to return if radar signal is present on DFS channel (5G only)
	INT  channel_non_80211_noise;			//average non 802.11 noise
	INT  channel_utilization;	//this is used to return the 802.11 utilization in percent
	INT  channel_txpower;		//this is used to return the current txpower in dbm on this channel
	wifi_apRssi_t channel_rssi_list[64];	//RSSI list from the neighbor AP on this channel. The list should be sorted descendly based on ap_rssi. If there are more than 64 AP on this channel, return first 64. 
	UINT channel_rssi_count; 	//RSSI counter in channel_rssi_list
} wifi_channelMetrics_t;
*/
#define CHCOUNT2 11
#define CHCOUNT5 25
static UINT channel_array_0[CHCOUNT2]={1,2,3,4,5,6,7,8,9,10,11};
static UINT farwaychannel_0[CHCOUNT2]={11,11,11,11,11,11,1,1,1,1,1};
static BOOL channel_pool_0[CHCOUNT2]={1,1,1,1,1,1,1,1,1,1,1};
static UINT channel_array_1[CHCOUNT5]={36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140,144,149,153,157,161,165};
static UINT dfschan_array_1[CHCOUNT5]={0,0,0,0,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140,144,0,0,0,0,0};
static BOOL channel_pool_1[CHCOUNT5]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
static UINT UNII_1[CHCOUNT5]={1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static UINT UNII_2[CHCOUNT5]={0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0};
static UINT UNII_3[CHCOUNT5]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1};

static INT  max_rssi_0[CHCOUNT2]={0,0,0,0,0,0,0,0,0,0,0};
static UINT high_rssi_count_0[CHCOUNT2]={0,0,0,0,0,0,0,0,0,0,0};
static UINT high_rssi_count_1[CHCOUNT5]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static UINT channel_util_score_0[CHCOUNT2]={0};
static UINT channel_util_score_1[CHCOUNT5]={0};
static UINT channel_score_0[CHCOUNT2]={0};
static UINT channel_score_1[CHCOUNT5]={0};
static wifi_channelMetrics_t channelMetrics_array_0[CHCOUNT2];
static wifi_channelMetrics_t channelMetrics_array_1[CHCOUNT5];
static wifi_channelMetrics_t channelMetrics_ave_array_0[CHCOUNT2];
static wifi_channelMetrics_t channelMetrics_ave_array_1[CHCOUNT5];
static INT  scan_count_0=0;
static INT  scan_count_1=0;
static int scanInterval=1800; //30 min
static BOOL DSCScan_enable_0=FALSE;
static BOOL DSCScan_enable_1=FALSE;
static BOOL DCSScan_Started=FALSE;


static void _reset_channelMetrics_array_0() {
	int i=0;
	memset(channelMetrics_array_0, 0, sizeof(wifi_channelMetrics_t)*CHCOUNT2);
	for(i=0; i<CHCOUNT2; i++) {
		channelMetrics_array_0[i].channel_number=channel_array_0[i];
		channelMetrics_array_0[i].channel_in_pool=channel_pool_0[i];
	}
	return;
}

static void _reset_channelMetrics_array_1() {
	int i=0;
	memset(channelMetrics_array_1, 0, sizeof(wifi_channelMetrics_t)*CHCOUNT5);
	for(i=0; i<CHCOUNT5; i++) {
		channelMetrics_array_1[i].channel_number=channel_array_1[i];
		//DCS-20-90 Default pool of channels to be scored and assigned must be all 20 MHz channels excluding U-NII-2a and U-NII-2c even if equipped and DFS-certified
		channelMetrics_array_1[i].channel_in_pool=(dfschan_array_1[i]==0)?channel_pool_1[i]:FALSE;
	}
	return;
}

static void _reset_channelMetrics_ave_array_0() {
	memset(channelMetrics_ave_array_0, 0, sizeof(wifi_channelMetrics_t)*CHCOUNT2);
	return;
}

static void _reset_channelMetrics_ave_array_1() {
	memset(channelMetrics_ave_array_1, 0, sizeof(wifi_channelMetrics_t)*CHCOUNT5);
	return;
}

static void _reset_count_score_0() {
	memset(max_rssi_0, 0, sizeof(INT)*CHCOUNT2);
	memset(high_rssi_count_0, 0, sizeof(UINT)*CHCOUNT2);
	memset(channel_score_0, 0, sizeof(UINT)*CHCOUNT2);
	memset(channel_util_score_0, 0, sizeof(UINT)*CHCOUNT2);
}

static void _reset_count_score_1() {
	memset(high_rssi_count_1, 0, sizeof(UINT)*CHCOUNT5);
	memset(channel_score_1, 0, sizeof(UINT)*CHCOUNT5);
	memset(channel_util_score_1, 0, sizeof(UINT)*CHCOUNT5);
}

static void _aggregate_ChannelMetrics_0(){
	int i=0;
	wifi_channelMetrics_t *pchan=NULL;
	//DCS-40-80 Any SSID heard with RSSI > -40 dBm on a standard channel (1,6,11) will cause the AP to select that channel for co-channel operation, regardless of other measured parameters

	for(i=0; i<CHCOUNT2; i++) {
		pchan=&channelMetrics_array_0[i];
		if(!pchan->channel_in_pool)
			continue;

		if(pchan->channel_rssi_count>0) {
			if(max_rssi_0[i]==0 || max_rssi_0[i]<pchan->channel_rssi_list[0].ap_rssi)
				max_rssi_0[i]=pchan->channel_rssi_list[0].ap_rssi;
			if(pchan->channel_rssi_list[0].ap_rssi>-82)
				high_rssi_count_0[i]++;
		}

		channelMetrics_ave_array_0[i].channel_number    =channelMetrics_array_0[i].channel_number;
		channelMetrics_ave_array_0[i].channel_in_pool   =TRUE;
		channelMetrics_ave_array_0[i].channel_noise     +=channelMetrics_array_0[i].channel_noise;
		channelMetrics_ave_array_0[i].channel_non_80211_noise+=channelMetrics_array_0[i].channel_non_80211_noise;
		channelMetrics_ave_array_0[i].channel_utilization+=channelMetrics_array_0[i].channel_utilization;
		channelMetrics_ave_array_0[i].channel_txpower   +=channelMetrics_array_0[i].channel_txpower;
		channelMetrics_ave_array_0[i].channel_rssi_count+=channelMetrics_array_0[i].channel_rssi_count;

	}
	return;
}

static void _aggregate_ChannelMetrics_1(){
	int i=0;
	wifi_channelMetrics_t *pchan=NULL;

	for(i=0; i<CHCOUNT5; i++) {
		pchan=&channelMetrics_array_1[i];
		if(!pchan->channel_in_pool)
			continue;

		if(pchan->channel_rssi_count>0) {
			if(pchan->channel_rssi_list[0].ap_rssi>-82)
				high_rssi_count_1[i]++;
		}
		channelMetrics_ave_array_1[i].channel_number    =channelMetrics_array_1[i].channel_number;
		channelMetrics_ave_array_1[i].channel_in_pool   =TRUE;
		channelMetrics_ave_array_1[i].channel_noise     +=channelMetrics_array_1[i].channel_noise;
		channelMetrics_ave_array_1[i].channel_radar_noise=(channelMetrics_ave_array_1[i].channel_radar_noise)?TRUE:channelMetrics_array_1[i].channel_radar_noise;
		channelMetrics_ave_array_1[i].channel_non_80211_noise+=channelMetrics_array_1[i].channel_non_80211_noise;
		channelMetrics_ave_array_1[i].channel_utilization+=channelMetrics_array_1[i].channel_utilization;
		channelMetrics_ave_array_1[i].channel_txpower   +=channelMetrics_array_1[i].channel_txpower;
		channelMetrics_ave_array_1[i].channel_rssi_count+=channelMetrics_array_1[i].channel_rssi_count;
	}
	return;
}

static void _get_channel_score_0() {
	int i=0;
	/*int mxrssi=0;
	int high_rssi_chan=0;
	int high_rssi=-100;
	*/
	wifi_channelMetrics_t *pchan=NULL;
	memset(channel_score_0, 0, sizeof(UINT)*CHCOUNT2);

	/*
	//DCS-40-80 Any SSID heard with RSSI > -40 dBm on a standard channel (1,6,11) will cause the AP to select that channel for co-channel operation, regardless of other measured parameters
	int std_chan[3]={1,6,11};
	int dest_chan=0;
	for(i=0; i<3; i++) {
		mxrssi=max_rssi_0[std_chan[i]];
		if(mxrssi>-40) {
			channel_score_0[std_chan[i]]=9999;
			high_rssi_chan=std_chan[i];
			dest_chan=farwaychannel_0[high_rssi_chan-1];
			printf ("DCS_SCAN_DEST:%d #DCS-40-80  high_rssi_chan:%d, RSSI:%d\n", dest_chan, high_rssi_chan, mxrssi);
		}
	}
	if(dest_chan) {//regardless of other measured parameters
		if(pdest_chan)
			*pdest_chan=dest_chan;
		return;
	}

	//DCS-40-90 Any SSID heard with RSSI > -40 dBm on a non-standard channel (other than 1,6,11) will cause the AP to select a pool channel which has the largest frequency separation possible (i.e., nearby AP @ -40 dBm on Channel 3:  Comcast AP selects Channel 11) regardless of other measured parameters
	int nonstd_chan[8]={2,3,4,5,7,8,9,10};
	mxrssi=0;
	high_rssi_chan=0;
	high_rssi=-100;
	dest_chan=0;
	for(i=0; i<8; i++) {
		mxrssi=max_rssi_0[nonstd_chan[i]];
		if(mxrssi>-40) {
			if(mxrssi>high_rssi) {
				high_rssi=mxrssi;
				high_rssi_chan=nonstd_chan[i];
				dest_chan=farwaychannel_0[high_rssi_chan-1];
			}
		}
	}
	if(dest_chan>0) {
		printf ("DCS_SCAN_DEST:%d #DCS-40-90  high_rssi_chan:%d, RSSI:%d\n", dest_chan, high_rssi_chan, high_rssi);
		if(pdest_chan)
			*pdest_chan=dest_chan;
		return;
	}
	*/
	
	//DCS-40-100 If no AP is heard > -40 dBm, channel score is based on
	for(i=0; i<CHCOUNT2; i++) {
		pchan=&channelMetrics_ave_array_0[i];
		if(!pchan->channel_in_pool)
			continue;

		//get average
		//if(scan_count_0>0) {
			//pchan->channel_noise      /=scan_count_0;
			//pchan->channel_non_80211_noise/=scan_count_0;
			//pchan->channel_utilization/=scan_count_0;
			//pchan->channel_txpower    /=scan_count_0;
			//channelMetrics_ave_array_0[i].channel_rssi_count/=scan_count;
			//high_rssi_count_0[i]/=scan_count;
		//}
		//•	Channel Utilization:  8 when utilization is >80%; 6 when between 60-80%; 4 when between 40-60%; 2 when between 20-40%, zero when <20%
		int cus=0;
		int channel_utilization=0;
		if(scan_count_0>0)
			channel_utilization=pchan->channel_utilization/scan_count_0;
		if(channel_utilization>80)
			cus=8;
		else if(channel_utilization>60)
			cus=6;
		else if(channel_utilization>40)
			cus=4;
		else if(channel_utilization>20)
			cus=2;
		else
			cus=0;

		channel_score_0[i]+=cus;
		channel_util_score_0[i]+=cus;

		//•	RSSI Distribution:  when percentage of recorded SSIDs > -82 dBm is more than 50% of total SSIDs heard, 1; otherwise 0
		if(high_rssi_count_0[i]>0 && (high_rssi_count_0[i]>(channelMetrics_ave_array_0[i].channel_rssi_count/2)))
			channel_score_0[i]+=1;

		//•	Channel Noise Floor:  1 when > -80 dBm; otherwise 0
		if(pchan->channel_noise>(-80*scan_count_0))
			channel_score_0[i]+=1;

		//•	Non 802.11 noise:  1 when non-802.11 noise is detected; otherwise 0
		if(pchan->channel_non_80211_noise!=0)
			channel_score_0[i]+=1;
	}
	return;
}

static void _get_channel_score_1() {
	int i=0;
	wifi_channelMetrics_t *pchan=NULL;
	int high_rssi_count=0;

	//DCS-40-120 For 5 GHz channels, following values are given to parameters for individual 20 MHz channel scoring
	for(i=0; i<CHCOUNT5; i++) {
		pchan=&channelMetrics_array_1[i];
		if(!pchan->channel_in_pool)
			continue;
		//get average
		//if(scan_count_1>0) {
			//pchan->channel_noise      /=scan_count_1;
			//pchan->channel_non_80211_noise/=scan_count_1;
			//pchan->channel_utilization/=scan_count_1;
			//pchan->channel_txpower    /=scan_count_1;
			//channelMetrics_ave_array_0[i].channel_rssi_count/=scan_count;
			//high_rssi_count_0[i]/=scan_count;
		//}
		//•	Channel Utilization:  8 when utilization is >80%; 6 when between 60-80%; 4 when between 40-60%; 2 when between 20-40%, zero when <20%
		int cus=0;
		int channel_utilization=0;
		if(scan_count_1>0)
			channel_utilization=pchan->channel_utilization/scan_count_1;
		if(channel_utilization>80)
			cus=8;
		else if(channel_utilization>60)
			cus=6;
		else if(channel_utilization>40)
			cus=4;
		else if(channel_utilization>20)
			cus=2;
		else
			channel_score_1[i]+=0;

		channel_score_1[i]+=cus;
		channel_util_score_1[i]+=cus;;

		//•	RSSI Distribution:  when percentage of recorded SSIDs > -82 dBm is more than 50% of total SSIDs heard, 1; otherwise 0
		if(high_rssi_count_1[i]>0 && (high_rssi_count_1[i]>(channelMetrics_ave_array_1[i].channel_rssi_count/2)))
			channel_score_1[i]+=1;

		//•	Channel Noise Floor:  1 when > -85 dBm; otherwise 0
		if(pchan->channel_noise>(-85*scan_count_1))
			channel_score_1[i]+=1;

		//•	For 5GHz band, an additional weighting factor is given for the UNII sub bands in which the proposed channel falls
		//I.	UNII-3: 0
		//II.	UNII-2a and 2c: 2
		//III.	UNII-1: 1 (weighted to reflect typical power reductions)
		if(UNII_1[i])
			channel_score_1[i]+=1;
		else if(UNII_2[i])
			channel_score_1[i]+=2;
		//•	Non 802.11 noise:  1 when non-802.11 noise is detected; otherwise 0
		if(pchan->channel_non_80211_noise!=0)
			channel_score_1[i]+=1;

	}

	return;
}

static void _print_channel_score_array_0() {
	int i=0;
	//7. DCS_CHAN_SCORE_1:$channel_number:$score;$channel_number:$score;$channel_number:$score;...
	CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_MAX_RSSI_1:", __FUNCTION__));
	for(i=0; i<CHCOUNT2; i++) {
		CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - %d:%d;", __FUNCTION__, channel_array_0[i], max_rssi_0[i]));
	}
	CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - \n\n", __FUNCTION__));
	CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_HIGH_RSSI_COUNT_1:", __FUNCTION__));
	for(i=0; i<CHCOUNT2; i++) {
		CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - %d:%d;", __FUNCTION__, channel_array_0[i], high_rssi_count_0[i]));
	}
	CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - \n\n", __FUNCTION__));
	CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_CHAN_SCORE_1:", __FUNCTION__));
	for(i=0; i<CHCOUNT2; i++) {
		CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - %d:%d;", __FUNCTION__, channel_array_0[i], channel_score_0[i]));
	}
	CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - \n\n", __FUNCTION__));
	return;
}

static void _print_channel_score_array_1() {
	int i=0;
	//8. DCS_CHAN_SCORE_2:$channel_number:$score;$channel_number:$score;$channel_number:$score;...
	CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_HIGH_RSSI_COUNT_2:", __FUNCTION__));
	for(i=0; i<CHCOUNT5; i++) {
		CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - %d:%d;", __FUNCTION__, channel_array_1[i], high_rssi_count_1[i]));
	}
	CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - \n\n", __FUNCTION__));
	CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_CHAN_SCORE_2:", __FUNCTION__));
	for(i=0; i<CHCOUNT5; i++) {
		CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - %d:%d;", __FUNCTION__, channel_array_1[i], channel_score_1[i]));
	}
	CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - \n\n", __FUNCTION__));
	return;
}


static int _get_channel_on_rssi_0() {
	int i=0;
	int mxrssi=0;
	int high_rssi_chan=0;
	int high_rssi=-100;
	//DCS-40-80 Any SSID heard with RSSI > -40 dBm on a standard channel (1,6,11) will cause the AP to select that channel for co-channel operation, regardless of other measured parameters
	int std_chan[3]={1,6,11};
	int dest_chan=0;
	for(i=0; i<3; i++) {
		mxrssi=max_rssi_0[std_chan[i]];
		if(mxrssi>-40) {
			//channel_score_0[std_chan[i]]=9999;
			high_rssi_chan=std_chan[i];
			dest_chan=farwaychannel_0[high_rssi_chan-1];
			CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_SCAN_DEST:%d #DCS-40-80  high_rssi_chan:%d, RSSI:%d\n", __FUNCTION__, dest_chan, high_rssi_chan, mxrssi));
		}
	}
	if(dest_chan>0)
		return dest_chan;

	//DCS-40-90 Any SSID heard with RSSI > -40 dBm on a non-standard channel (other than 1,6,11) will cause the AP to select a pool channel which has the largest frequency separation possible (i.e., nearby AP @ -40 dBm on Channel 3:  Comcast AP selects Channel 11) regardless of other measured parameters
	int nonstd_chan[8]={2,3,4,5,7,8,9,10};
	mxrssi=0;
	high_rssi_chan=0;
	high_rssi=-100;
	dest_chan=0;
	for(i=0; i<8; i++) {
		mxrssi=max_rssi_0[nonstd_chan[i]];
		if(mxrssi>-40) {
			if(mxrssi>high_rssi) {
				high_rssi=mxrssi;
				high_rssi_chan=nonstd_chan[i];
				dest_chan=farwaychannel_0[high_rssi_chan-1];
			}
		}
	}
	if(dest_chan>0)
		CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_SCAN_DEST:%d #DCS-40-90  high_rssi_chan:%d, RSSI:%d\n", __FUNCTION__, dest_chan, high_rssi_chan, high_rssi));

	return dest_chan;
}

static void _get_lowest_channel_score_0(char *bandwidth, char *extchan, int cur_chan, int *pcur_chan_score, int *pdest_chan, int *pdest_chan_score) {
	int i=0;
	int lowest_score=999;
	int dest_chan=0;
	int idxa=0, idxb=0;
	if(strcmp(bandwidth, "20MHz")==0) {
		for(i=0; i<CHCOUNT2; i++) {
			//Lowest score is best channel.  A perfect score is 2.
			if(channel_score_0[i]<lowest_score) {
				dest_chan=channel_array_0[i];
				lowest_score=channel_score_0[i];
			}
			if(channel_array_0[i]==cur_chan) {
				if(pcur_chan_score)
					*pcur_chan_score=channel_util_score_0[i];
			}
		}
		if(dest_chan) {
			CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_SCAN_DEST:%d,20MHz #DCS-40-100\n", __FUNCTION__, dest_chan));
			if(pdest_chan)
				*pdest_chan=dest_chan;
			if(pdest_chan_score)
				*pdest_chan_score=channel_score_0[dest_chan-1];
		}

	} else { //40
		//DCS-20-101 For wider bandwidth channels, the device must choose a channel of the preferred bandwidth having the lowest average combined score
		if(strcmp(extchan, "BelowControlChannel")!=0) { //plus
			idxa=0;	idxb=7;
		} else {
			idxa=5; idxb=11;
		}
		//2.	Select the primary and secondary channel(s) such that the primary channel has the lowest utilization value of all channels included in the block.
		for(i=idxa; i<idxb; i++) {
			if(channel_util_score_0[i]<lowest_score) {
				dest_chan=channel_array_0[i];
				lowest_score=channel_util_score_0[i];
			}
			if(channel_array_0[i]==cur_chan)
				*pcur_chan_score=channel_util_score_0[i];
		}
		if(dest_chan) {
			CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_SCAN_DEST:%d,40MHz #DCS-20-101\n", __FUNCTION__, dest_chan));
			if(pdest_chan)
				*pdest_chan=dest_chan;
			if(pdest_chan_score)
				*pdest_chan_score=lowest_score;
		}
	}
	return;
}

static void _get_lowest_channel_score_1(char *bandwidth, char *extchan, int cur_chan, int *pcur_chan_score, int *pdest_chan, int *pdest_chan_score) {
	int i=0;
	int sum=0;
	int lowest_score=999;
	int dest_chan=0;
	int idxa=0;
	int dif=0;
	if(strcmp(bandwidth, "20MHz")==0) {
		for(i=0; i<CHCOUNT5; i++) {
			//Lowest score is best channel.  A perfect score is 2.
			if(channel_score_1[i]<lowest_score) {
				dest_chan=channel_array_1[i];
				lowest_score=channel_score_1[i];
			}
			if(channel_array_1[i]==cur_chan) {
				if(pcur_chan_score)
					*pcur_chan_score=channel_util_score_1[i];
			}
		}
		if(dest_chan) {
			CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_SCAN_DEST:%d,20MHz #DCS-40-120\n", __FUNCTION__, dest_chan));
			if(pdest_chan)
				*pdest_chan=dest_chan;
			if(pdest_chan_score)
				*pdest_chan_score=lowest_score;
		}

	} else if(strcmp(bandwidth, "40MHz")==0) {
		//DCS-20-101 For wider bandwidth channels, the device must choose a channel of the preferred bandwidth having the lowest average combined score
		idxa=(strcmp(extchan, "BelowControlChannel"))?0:1; //plus??"AboveControlChannel", "Auto"
		//2.	Select the primary and secondary channel(s) such that the primary channel has the lowest utilization value of all channels included in the block.
		for(i=idxa; i<(CHCOUNT5-1); i+=2) {
			if(channel_util_score_1[i]<lowest_score) {
				dest_chan=channel_array_1[i];
				lowest_score=channel_util_score_1[i];
			}
			if(channel_array_1[i]==cur_chan)
				*pcur_chan_score=channel_util_score_1[i];
		}
		if(dest_chan) {
			CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_SCAN_DEST:%d,40MHz #DCS-20-101\n", __FUNCTION__, dest_chan));
			if(pdest_chan)
				*pdest_chan=dest_chan;
			if(pdest_chan_score)
				*pdest_chan_score=lowest_score;
		}

	} else if(strcmp(bandwidth, "80MHz")==0) {
		//1.	From the individual channel scores, select the channel block having the lowest average combined score
		for(i=0; i<(CHCOUNT5-1); i+=4) {
			sum=channel_score_1[i]+channel_score_1[i+1]+channel_score_1[i+2]+channel_score_1[i+3];
			if(sum<lowest_score) {
				dest_chan=channel_array_1[i];
				lowest_score=sum;
			}
			if(channel_array_1[i]<=cur_chan && cur_chan<=channel_array_1[i+3])
				*pcur_chan_score=sum;
		}
		if(dest_chan) {
			dif=cur_chan-dest_chan;
			if(0<=dif && dif<=3)
				dest_chan=cur_chan;
			CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_SCAN_DEST:%d,80MHz #DCS-20-101\n", __FUNCTION__, dest_chan));
			if(pdest_chan)
				*pdest_chan=dest_chan;
			if(pdest_chan_score)
				*pdest_chan_score=lowest_score;
		}
	} else if(strcmp(bandwidth, "160MHz")==0) {
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s - DCS_ERROR:160\n", __FUNCTION__));
	} else { //80+80
		CcspWifiTrace(("RDK_LOG_ERROR,WIFI %s - DCS_ERROR:80+80\n", __FUNCTION__));
	}
	return;
}

static void _print_channelMetrics_array_0() {
	int i=0, j=0;
	wifi_apRssi_t *paprssi=NULL;
	wifi_channelMetrics_t *pchan=NULL;
	char mac[32]={0};

	for(i=0, pchan=channelMetrics_array_0; i<CHCOUNT2; i++, pchan++) {
		//3.DCS_SCAN_RESULT_1:$channel_number=$util,$noise,$non_80211noise,$radar_noise,$txpower
		CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_SCAN_RESULT_1:%d=%d,%d,%d,%d,%d\n",
                                                __FUNCTION__,
                                                pchan->channel_number,
                                                pchan->channel_utilization,
                                                pchan->channel_noise,
                                                pchan->channel_non_80211_noise,
                                                0,//pchan->channel_radar_noise,
                                                pchan->channel_txpower));

		if(pchan->channel_rssi_count==0)
			continue;
		//4.DCS_SCAN_BSSID_1:$channel_number=$BSSID,$channelwidth,$rssi;$BSSID,$channelwidth,$rssi;...
		CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_SCAN_BSSID_1", __FUNCTION__));
		for(j=0, paprssi=pchan->channel_rssi_list; j<pchan->channel_rssi_count; j++, paprssi++) {
			//$BSSID,$channelwidth,$rssi;
			cMac_to_sMac(paprssi->ap_BSSID, mac);
			//printf("%s,%d,%d;", mac, paprssi->ap_channelWidth, paprssi->ap_rssi);
			CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - %s,%d,%d;", __FUNCTION__, mac, paprssi->ap_channelWidth, paprssi->ap_rssi));
		}
		CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - END\n", __FUNCTION__));
	}
	return;
}

static void _print_channelMetrics_array_1() {
	int i=0, j=0;
	wifi_apRssi_t *paprssi=NULL;
	wifi_channelMetrics_t *pchan=NULL;
	char mac[32]={0};

	for(i=0, pchan=channelMetrics_array_1; i<CHCOUNT5; i++, pchan++) {
		if(!pchan->channel_in_pool)
			continue;
		//5.DCS_SCAN_RESULT_1:$channel_number=$util,$noise,$non_80211noise,$radar_noise,$txpower
		CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_SCAN_RESULT_2:%d=%d,%d,%d,%d,%d\n",
                                                __FUNCTION__,
                                                pchan->channel_number,
                                                pchan->channel_utilization,
                                                pchan->channel_noise,
                                                pchan->channel_non_80211_noise,
                                                pchan->channel_radar_noise,
                                                pchan->channel_txpower));
		if(pchan->channel_rssi_count==0)
			continue;
		//6.DCS_SCAN_BSSID_2:$channel_number=$BSSID,$channelwidth,$rssi;$BSSID,$channelwidth,$rssi;...
		CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_SCAN_BSSID_2:%d=", __FUNCTION__));
		for(j=0, paprssi=pchan->channel_rssi_list; j<pchan->channel_rssi_count; j++, paprssi++) {
			//$BSSID,$channelwidth,$rssi;
			cMac_to_sMac(paprssi->ap_BSSID, mac);
			//printf("%s,%d,%d;", mac, paprssi->ap_channelWidth, paprssi->ap_rssi);
			CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - %s,%d,%d;", __FUNCTION__, mac, paprssi->ap_channelWidth, paprssi->ap_rssi));

		}
		CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - END\n", __FUNCTION__));
	}
	return;
}

BOOL isDCSCheckTime(void) {
	time_t t;
	struct tm *pLcltime;
	static BOOL inCheckTime=FALSE;

	t = time(NULL);
		pLcltime = localtime(&t);
		if(pLcltime->tm_hour==3) {	//check every morning at 3
		if(!inCheckTime) {
			inCheckTime=TRUE;
			return TRUE;
		}
	} else if(pLcltime->tm_hour==4 && inCheckTime) {
		inCheckTime=FALSE;
	}
	return FALSE;
}

void * CosaDmlWiFi_doDCSScanThread (void *input) {
	//PCOSA_DML_NEIGHTBOURING_WIFI_DIAG_CFG pNeighScan=input;
	//PCOSA_DML_NEIGHTBOURING_WIFI_RESULT tmp_2, tmp_5;
	char bandwidth_0[64]="", bandwidth_1[64]="";
	char extchan_0[64]="", extchan_1[64]="";
	int dest_chan_0=0, dest_chan_1=0;
	unsigned long cur_chan_0=0, cur_chan_1=0;
	int cur_chan_score_0=0, cur_chan_score_1=0;
	int dest_chan_score_0=0, dest_chan_score_1=0;
	int chanScoreDiffThreshold=1;


	while(1) {

		sleep(scanInterval-60);
		if(DSCScan_enable_0) {
			printf("%s Calling pthread_mutex_lock for sNeighborScanThreadMutex  %d \n",__FUNCTION__ , __LINE__ );
			pthread_mutex_lock(&sNeighborScanThreadMutex);
			printf("%s Called pthread_mutex_lock for sNeighborScanThreadMutex  %d \n",__FUNCTION__ , __LINE__ );

			//1.$utc_time DCS_SCAN:START
			CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - %ld DCS_SCAN:START  2.4G:%d\n", __FUNCTION__, time(NULL), scan_count_0));
			_reset_channelMetrics_array_0();
			wifi_getRadioDcsChannelMetrics(0, channelMetrics_array_0, CHCOUNT2);
			scan_count_0++;
			//2.$utc_time DCS_SCAN:END
			CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - %ld DCS_SCAN:END  2.4G\n", __FUNCTION__, time(NULL)));
			printf("%s Calling pthread_mutex_unlock for sNeighborScanThreadMutex  %d \n",__FUNCTION__ , __LINE__ );
			pthread_mutex_unlock(&sNeighborScanThreadMutex);
			printf("%s Called pthread_mutex_unlock for sNeighborScanThreadMutex  %d \n",__FUNCTION__ , __LINE__ );

			_print_channelMetrics_array_0();
			_aggregate_ChannelMetrics_0();
			_get_channel_score_0();
		}
		if(DSCScan_enable_1) {
			printf("%s Calling pthread_mutex_lock for sNeighborScanThreadMutex  %d \n",__FUNCTION__ , __LINE__ );
			pthread_mutex_lock(&sNeighborScanThreadMutex);
			printf("%s Called pthread_mutex_lock for sNeighborScanThreadMutex  %d \n",__FUNCTION__ , __LINE__ );

			//1.$utc_time DCS_SCAN:START
			CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - %ld DCS_SCAN:START 5G:%d\n", __FUNCTION__, time(NULL), scan_count_1));
			_reset_channelMetrics_array_1();
			wifi_getRadioDcsChannelMetrics(1, channelMetrics_array_1, CHCOUNT5);
			scan_count_1++;
			//2.$utc_time DCS_SCAN:END
			CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - %ld DCS_SCAN:END  5G\n", __FUNCTION__, time(NULL)));
			printf("%s Calling pthread_mutex_unlock for sNeighborScanThreadMutex  %d \n",__FUNCTION__ , __LINE__ );
			pthread_mutex_unlock(&sNeighborScanThreadMutex);
			printf("%s Called pthread_mutex_unlock for sNeighborScanThreadMutex  %d \n",__FUNCTION__ , __LINE__ );

			_print_channelMetrics_array_1();
			_aggregate_ChannelMetrics_1();
			_get_channel_score_1();
		}

		if(!isDCSCheckTime())
			continue;

		if(DSCScan_enable_0) {
			wifi_getRadioChannel(0, &cur_chan_0);
			wifi_getRadioOperatingChannelBandwidth(0, bandwidth_0);
			if(strcmp(bandwidth_0, "40MHz")==0) {
				wifi_getRadioExtChannel(0, extchan_0);
			}
			dest_chan_0=0;
			cur_chan_score_0=0;
			dest_chan_score_0=0;

			dest_chan_0=_get_channel_on_rssi_0();
			if(dest_chan_0>0) {

			} else {
				//_get_channel_score_0();
				_print_channel_score_array_0();
				_get_lowest_channel_score_0(bandwidth_0, extchan_0, cur_chan_0, &cur_chan_score_0, &dest_chan_0, &dest_chan_score_0);
				//9. DCS_CHAN_SCORE_RESULT_1: $cur_channel:$score,$target_channel:$score;$score_threshold
				CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_CHAN_SCORE_RESULT_1:%d,%d;%d,%d;%d\n", __FUNCTION__, cur_chan_0, cur_chan_score_0, dest_chan_0, dest_chan_score_0, chanScoreDiffThreshold));
			}

			if(dest_chan_0 != cur_chan_0) {
				//11. $utc_time DCS_CHAN_CHNAGE_1:$source;$dest
				CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_CHAN_CHNAGE_1:%d,%d\n", __FUNCTION__, cur_chan_0, dest_chan_0));
				//wifi_pushRadioChannel(0, dest_chan_0);
			}
		}
		if(DSCScan_enable_1) {
			wifi_getRadioChannel(1, &cur_chan_1);
			wifi_getRadioOperatingChannelBandwidth(1, bandwidth_1);
			if(strcmp(bandwidth_1, "40MHz")==0) {
				wifi_getRadioExtChannel(1, extchan_1);
			}
			dest_chan_1=0;
			cur_chan_score_1=0;
			dest_chan_score_1=0;

			//_get_channel_score_1();
			_print_channel_score_array_1();
			_get_lowest_channel_score_1(bandwidth_1, extchan_1, cur_chan_1, &cur_chan_score_1, &dest_chan_1, &dest_chan_score_1);
			//10. DCS_CHAN_SCORE_RESULT_2: $cur_channel:$score,$target_channel:$score;$score_threshold
			CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_CHAN_SCORE_RESULT_2:%d,%d;%d,%d;%d\n", __FUNCTION__, cur_chan_1, cur_chan_score_1, dest_chan_1, dest_chan_score_1, chanScoreDiffThreshold));

			if(dest_chan_1 != cur_chan_1) {
				//12. $utc_time DCS_CHAN_CHNAGE_5:$source;$dest
				CcspWifiTrace(("RDK_LOG_INFO,WIFI %s - DCS_CHAN_CHNAGE_2:%d,%d\n", __FUNCTION__, cur_chan_1, dest_chan_1));
				//wifi_pushRadioChannel(1, dest_chan_1);
			}
		}

		//reset the score regardless
		_reset_channelMetrics_ave_array_0();
		_reset_channelMetrics_ave_array_1();
		_reset_count_score_0();
		_reset_count_score_1();
		scan_count_0=0;
		scan_count_1=0;
	}
	return NULL;
}

ANSC_STATUS
CosaDmlWiFi_setRadioDcsDwelltime(INT radioInstanceNumber, INT ms) {
	//DCS-30-60 During operations for dynamic channel change, default dwell must be configurable in the range of 30-50 ms (default 40ms)
	wifi_setRadioDcsDwelltime(radioInstanceNumber-1, ms);
	return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_getRadioDcsDwelltime(INT radioInstanceNumber, INT *output) {
	wifi_getRadioDcsDwelltime(radioInstanceNumber-1, output);
	return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_startDCSScanThread (void) {
	fprintf(stderr, "-- %s %d\n", __func__, __LINE__);
	pthread_t tid;

	if(DCSScan_Started)
		return;
	else
		DCSScan_Started = TRUE;

	wifiDbgPrintf("%s\n",__FUNCTION__);
	if(pthread_create(&tid,NULL,CosaDmlWiFi_doDCSScanThread, (void*)NULL)) {
		fprintf(stderr, "-- %s %d pthread_create CosaDmlWiFi_doDCSScanThread fail\n", __func__, __LINE__);
		return ANSC_STATUS_FAILURE;
	}
	return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_setDCSScan(INT radioInstance, BOOL enable) {
	fprintf(stderr, "-- %s %d\n", __func__, __LINE__);
	wifiDbgPrintf("%s\n",__FUNCTION__);
	if(radioInstance==1) { //2.4G
		DSCScan_enable_0=enable;
		if(enable) {
			_reset_channelMetrics_ave_array_0();
			_reset_count_score_0();
			scan_count_0=0;
		}
	} else {
		DSCScan_enable_1=enable;
		if(enable) {
			_reset_channelMetrics_ave_array_1();
			_reset_count_score_1();
			scan_count_1=0;
		}
	}
	return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_getDCSChanPool(INT radioInstance, char *pool, int pool_len) {
	int i=0;
	int len=0;
	fprintf(stderr, "-- %s %d\n", __func__, __LINE__);
	wifiDbgPrintf("%s\n",__FUNCTION__);
	if(!pool)
		return ANSC_STATUS_FAILURE;
	pool[0]='\0';
	if(radioInstance==1) { //2.4G
		for(i=0; i<CHCOUNT2; i++) {
			len=strlen(pool);
			if(channel_pool_0[i]) {
				snprintf(pool+len, (pool_len-len), "%d,", channel_array_0[i]);
			}
		}
	} else { //5G
		for(i=0; i<CHCOUNT5; i++) {
			len=strlen(pool);
			if(channel_pool_1[i] && (!dfschan_array_1[i])) { //not include DFS channel
				snprintf(pool+len, (pool_len-len), "%d,", channel_array_1[i]);
			}
		}
	}
	len=strlen(pool);
	if(len>2)
		pool[len-1]='\0';
	return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_setDCSChanPool(INT radioInstance, char *pool) {
	int i=0, j=0;
	char str[128];
	char *token=NULL;
	int ch=0;
	fprintf(stderr, "-- %s %d\n", __func__, __LINE__);
	wifiDbgPrintf("%s\n",__FUNCTION__);
	if(!pool)
		return ANSC_STATUS_FAILURE;

	strncpy(str, pool, 127);
	if(radioInstance==1) { //2.4G
		memset(channel_pool_0, 0, sizeof(BOOL)*CHCOUNT2);
		token = strtok(str, ",");
		while(token != NULL) {
			ch = _ansc_atoi(token);
			if(ch>0 && ch<=11)
				channel_pool_0[ch-1]=TRUE;
			token = strtok(NULL, ",");
		}
	} else { //5G
		memset(channel_pool_1, 0, sizeof(BOOL)*CHCOUNT5);
		token = strtok(str, ",");
		while(token != NULL) {
			ch = _ansc_atoi(token);
			for(j=0; j<CHCOUNT5; j++) {
				if(ch==channel_array_1[j])
					break;
			}
			if(j<CHCOUNT5 && dfschan_array_1[j]==0) {
				channel_pool_1[j]=TRUE;
			}
			token = strtok(NULL, ",");
		}
	}

	return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_getDCSChanScore(INT radioInstance, char *score, int score_len) {
	int i=0;
	int len=0;
	fprintf(stderr, "-- %s %d\n", __func__, __LINE__);
	wifiDbgPrintf("%s\n",__FUNCTION__);
	if(!score)
		return ANSC_STATUS_FAILURE;
	score[0]='\0';
	if(radioInstance==1) { //2.4G
		_print_channel_score_array_0();
		for(i=0; i<CHCOUNT2; i++) {
			len=strlen(score);
			if(channel_pool_0[i]) {
				snprintf(score+len, (score_len-len), "%d:%d,", channel_array_0[i], channel_score_0[i]);
			}
		}
	} else { //5G
		_print_channel_score_array_1();
		for(i=0; i<CHCOUNT5; i++) {
			len=strlen(score);
			if(channel_pool_1[i] && (!dfschan_array_1[i])) { //not include DFS channel
				snprintf(score+len, (score_len-len), "%d:%d,", channel_array_1[i], channel_score_1[i]);
			}
		}
	}
	len=strlen(score);
	if(len>2)
		score[len-1]='\0';
	return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_setDCSScanInterval(int scan_Interval) {
	scanInterval=scan_Interval;
	return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_getDCSScanInterval(int *pscan_Interval) {
	if(pscan_Interval)
		*pscan_Interval=scanInterval;
	return ANSC_STATUS_SUCCESS;
}
//zqiu << DCS

#endif
