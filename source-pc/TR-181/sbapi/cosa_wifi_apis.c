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
#include  <pthread.h>
//LNT_EMU
extern ANSC_HANDLE bus_handle;//lnt
extern char g_Subsystem[32];//lnt
static char *BssSsid ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.SSID.%d.SSID";
static char *HideSsid ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.SSID.%d.HideSSID";
static char *Passphrase ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.SSID.%d.Passphrase";
static char *ChannelNumber ="eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.%d.Channel";
static char *DiagnosticEnable = "eRT.com.cisco.spvtg.ccsp.Device.WiFi.NeighbouringDiagnosticEnable" ;
static char *WpsPushButton = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.WpsPushButton";

int sMac_to_cMac(char *sMac, unsigned char *cMac);

int sMac_to_cMac(char *sMac, unsigned char *cMac) {
        unsigned int iMAC[6];
        int i=0;
        if (!sMac || !cMac) return 0;
        sscanf(sMac, "%x:%x:%x:%x:%x:%x", &iMAC[0], &iMAC[1], &iMAC[2], &iMAC[3], &iMAC[4], &iMAC[5]);
        for(i=0;i<6;i++)
                cMac[i] = (unsigned char)iMAC[i];

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
	wifi_init();
        return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiRegionInit
  (
    PCOSA_DATAMODEL_RDKB_WIFIREGION PWiFiRegion
  )
{
        return ANSC_STATUS_SUCCESS;
}

static int gRadioCount = 2;//RDK_EMU
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
     int wlanIndex = ulIndex;
     char buf[1024] = {0};
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
        pWifiRadio->StaticInfo.SupportedFrequencyBands = COSA_DML_WIFI_FREQ_BAND_2_4G;                   /* Bitmask of COSA_DML_WIFI_FREQ_BAND */
        pWifiRadio->StaticInfo.SupportedStandards      = COSA_DML_WIFI_STD_b | COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n;      /* Bitmask of COSA_DML_WIFI_STD */
	}
	else if(ulIndex+1 == 2)
	{
        pWifiRadio->StaticInfo.SupportedFrequencyBands = COSA_DML_WIFI_FREQ_BAND_5G;                   /* Bitmask of COSA_DML_WIFI_FREQ_BAND */
        pWifiRadio->StaticInfo.SupportedStandards      = COSA_DML_WIFI_STD_ac | COSA_DML_WIFI_STD_a | COSA_DML_WIFI_STD_n;      /* Bitmask of COSA_DML_WIFI_STD */
	}
	else if(ulIndex+1 == 5)
	{
		pWifiRadio->StaticInfo.SupportedFrequencyBands = COSA_DML_WIFI_FREQ_BAND_2_4G;                   /* Bitmask of COSA_DML_WIFI_FREQ_BAND */
        pWifiRadio->StaticInfo.SupportedStandards      = COSA_DML_WIFI_STD_b | COSA_DML_WIFI_STD_g | COSA_DML_WIFI_STD_n;      /* Bitmask of COSA_DML_WIFI_STD */

	}
	else if(ulIndex+1 == 6)
	{
		 pWifiRadio->StaticInfo.SupportedFrequencyBands = COSA_DML_WIFI_FREQ_BAND_5G;                   /* Bitmask of COSA_DML_WIFI_FREQ_BAND */
        pWifiRadio->StaticInfo.SupportedStandards      = COSA_DML_WIFI_STD_ac | COSA_DML_WIFI_STD_a | COSA_DML_WIFI_STD_n;      /* Bitmask of COSA_DML_WIFI_STD */
	}

        sprintf(pWifiRadio->Cfg.Alias, "Radio%d", ulIndex);
	wifi_getRadioIfName(wlanIndex,&pWifiRadio->StaticInfo.Name);        
        pWifiRadio->StaticInfo.bUpstream               = TRUE;
        //pWifiRadio->StaticInfo.MaxBitRate              = 128+ulIndex;
        pWifiRadio->StaticInfo.AutoChannelSupported    = TRUE;
        AnscCopyString(pWifiRadio->StaticInfo.TransmitPowerSupported, "10,20,50,100");
        pWifiRadio->StaticInfo.IEEE80211hSupported     = TRUE;

	wifi_getRadioEnable(wlanIndex,&pWifiRadio->Cfg.bEnabled);//RDKB-EMU
	wifi_getRadioMaxBitRate(wlanIndex,buf);//RDKB-EMU
        /*if (strstr(buf, "Mb/s")) {
                //216.7 Mb/s
                pWifiRadio->StaticInfo.MaxBitRate = strtof(buf,0);
        } else if (strstr(buf, "Gb/s")) {
                //1.3 Gb/s
                pWifiRadio->StaticInfo.MaxBitRate = strtof(buf,0) * 1000;
        } else {
                //Auto or Kb/s
                pWifiRadio->StaticInfo.MaxBitRate = 0;
        }*/
	pWifiRadio->StaticInfo.MaxBitRate = atol(buf);
        if(pWifiRadio->Cfg.bEnabled == TRUE)
                wifi_getRadioPossibleChannels(wlanIndex,&pWifiRadio->StaticInfo.PossibleChannels);
        else
	{
		if(pWifiRadio->Cfg.InstanceNumber == 1)
                AnscCopyString(pWifiRadio->StaticInfo.PossibleChannels,"1,2,3,4,5,6,7,8,9,10,11");
		else if(pWifiRadio->Cfg.InstanceNumber == 2)
                AnscCopyString(pWifiRadio->StaticInfo.PossibleChannels,"36,40,44,48,52,56,60,64,100,104,108,112,116,120,124,128,132,136,140");
	}

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
	BOOL GetSSIDEnable;
	char buf[256] = {0};
	int fd = 0;
	int wlanIndex = pCfg->InstanceNumber - 1;
        wifi_getSSIDEnable(wlanIndex,&GetSSIDEnable);
	if(pCfg->InstanceNumber == 1)
	{
		fd = open("/tmp/Get2gssidEnable.txt","r");
		if(fd == -1)
		{
			sprintf(buf,"%s%d%s","echo ",GetSSIDEnable," > /tmp/Get2gssidEnable.txt");
			system(buf);
		}
	}
	else if(pCfg->InstanceNumber == 2)
	{
		fd = open("/tmp/Get5gssidEnable.txt","r");
		if(fd == -1)
		{
			sprintf(buf,"%s%d%s","echo ",GetSSIDEnable," > /tmp/Get5gssidEnable.txt");
			system(buf);
		}
	}
	if(pCfg->ApplySetting == TRUE) 
        {
        //wifi_stopHostApd();
        //wifi_startHostApd();
	if((pCfg->bEnabled == TRUE) && (GetSSIDEnable == TRUE))
		wifi_applyRadioSettings(wlanIndex);//RDKB-EMU-L 
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
    int wlanIndex = pCfg->InstanceNumber - 1;
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
        char bandwidth[64];
        char extchan[64];
        wifi_getRadioOperatingChannelBandwidth(wlanIndex, bandwidth);
        if (strcmp(bandwidth, "40MHz") == 0) {
             wifi_getRadioExtChannel(wlanIndex, extchan);
             pCfg->OperatingChannelBandwidth = COSA_DML_WIFI_CHAN_BW_40M;
             if (strcmp(extchan, "AboveControlChannel") == 0) {
                  pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Above;
             } else if (strcmp(extchan, "BelowControlChannel") == 0) {
                  pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Below;
             } else {
                  pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Auto;
             }
        } else if (strcmp(bandwidth, "80MHz") == 0) {
               pCfg->OperatingChannelBandwidth = COSA_DML_WIFI_CHAN_BW_80M;
               pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Auto;
        } else if (strcmp(bandwidth, "160") == 0) {
               pCfg->OperatingChannelBandwidth = COSA_DML_WIFI_CHAN_BW_160M;
               pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Auto;
        } else if (strcmp(bandwidth, "20MHz") == 0) {
               pCfg->OperatingChannelBandwidth = COSA_DML_WIFI_CHAN_BW_20M;
               pCfg->ExtensionChannel = COSA_DML_WIFI_EXT_CHAN_Auto;
        }
	wifi_getRadioEnable(wlanIndex,&pCfg->bEnabled);//RDKB-EMU
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
        //pInfo->Status                 = COSA_DML_IF_STATUS_Up;
	//RDKB-EMU
	char Radio_status[50] = {0};
	int wlanIndex = ulInstanceNumber - 1;
        wifi_getSSIDStatus(wlanIndex,Radio_status);
        if(strcmp(Radio_status,"Enabled") == 0)
                pInfo->Status = COSA_DML_IF_STATUS_Up;
        else
                pInfo->Status = COSA_DML_IF_STATUS_Down;
        //pInfo->LastChange             = 123456;
        //AnscCopyString(pInfo->ChannelsInUse, "1");
	if(strcmp(Radio_status,"Enabled") == 0)
                wifi_getRadioChannelsInUse(wlanIndex,&pInfo->ChannelsInUse);
        else
                AnscCopyString(pInfo->ChannelsInUse, "0");
	
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
    wifi_radioTrafficStats2_t               radioTrafficStats;//RDKB-EMU
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
   	 wifi_getRadioTrafficStats2(ulInstanceNumber-1, &radioTrafficStats);
            pStats->BytesSent                          = radioTrafficStats.radio_BytesSent;
            pStats->BytesReceived                      = radioTrafficStats.radio_BytesReceived;
            pStats->PacketsSent                        = radioTrafficStats.radio_PacketsSent;
            pStats->PacketsReceived                    = radioTrafficStats.radio_PacketsReceived;
            pStats->ErrorsSent                         = radioTrafficStats.radio_ErrorsSent;
            pStats->ErrorsReceived                     = radioTrafficStats.radio_ErrorsReceived;
            pStats->DiscardPacketsSent                 = radioTrafficStats.radio_DiscardPacketsSent;
            pStats->DiscardPacketsReceived             = radioTrafficStats.radio_DiscardPacketsReceived;

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
        sprintf(pEntry->StaticInfo.Name, "ath%d", ulIndex);
        sprintf(pEntry->Cfg.Alias, "ath%d", ulIndex); //RDKB-EMU
	sprintf(pEntry->Cfg.SSID, "SSID%d", ulIndex); //RDKB-EMU
    
        pEntry->Cfg.InstanceNumber    = ulIndex+1;//LNT_EMU
        _ansc_sprintf(pEntry->Cfg.WiFiRadioName, "wifi%d",ulIndex);
    
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
	int wlanIndex = pCfg->InstanceNumber - 1;
	wifi_getSSIDEnable(wlanIndex,&pCfg->bEnabled);
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
	int wlanIndex = ulInstanceNumber-1;
        //wifi_getBaseBSSID(wlanIndex,pInfo->BSSID);
        //wifi_getSSIDMACAddress(wlanIndex,pInfo->MacAddress);//RDKB-EMULATOR
	//wifi_getSSIDName(ulInstanceNumber,pInfo->Name);
        char bssid[64];
	wifi_getBaseBSSID(wlanIndex, bssid);
        sMac_to_cMac(bssid, &pInfo->BSSID);
        sMac_to_cMac(bssid, &pInfo->MacAddress);
    return ANSC_STATUS_SUCCESS;
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
	int wlanIndex = ulInstanceNumber-1;
	wifi_getSSIDStatus(wlanIndex,wifi_status);
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
    int wlanIndex = ulInstanceNumber-1;	
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
	
	wifi_getWifiTrafficStats(wlanIndex,pStats);
   	wifi_getBasicTrafficStats(wlanIndex,pStats);
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
	BOOL enabled = FALSE;
	pCfg->InstanceNumber = (( pSsid[strlen(pSsid)-1] ) - '0') +1;
	sprintf(pCfg->Alias,"AccessPoint%d", pCfg->InstanceNumber);
	int wlanIndex = pCfg->InstanceNumber - 1;
	wifi_getApEnable(wlanIndex, &enabled);
    	pCfg->bEnabled = (enabled == TRUE) ? TRUE : FALSE;
        ApinsCount = pCfg->InstanceNumber;//LNT_EMU 
	wifi_getApSsidAdvertisementEnable(wlanIndex,&pCfg->SSIDAdvertisementEnabled);//LNT_EMU
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
	BOOL enabled = FALSE;
	int wlanIndex = (((( pSsid[strlen(pSsid)-1] ) - '0') +1) - 1);
	wifi_getApEnable(wlanIndex,&enabled);
        pInfo->Status = (enabled == TRUE) ? COSA_DML_WIFI_AP_STATUS_Enabled : COSA_DML_WIFI_AP_STATUS_Disabled;
        pInfo->WMMCapability = TRUE;
    	pInfo->UAPSDCapability = TRUE;
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
    .EncryptionMethod       = COSA_DML_WIFI_AP_SEC_AES_TKIP,
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
	char 			    password[50],SecurityMode[50] = {0},method[50] = {0};
	int wlanIndex = ApinsCount - 1;
	if (!pCfg)
		return ANSC_STATUS_FAILURE;

	memcpy(pCfg, &g_WiFiApSecCfg, sizeof(COSA_DML_WIFI_APSEC_CFG));
	wifi_getApSecurityPreSharedKey(wlanIndex,password); //RDKB-EMU-L
        if((ApinsCount == 1) || (ApinsCount == 2))
        {
        if(strcmp(password,"")==0)
        {
                pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_None;
        }
        else
        {
                //pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_WPA_Personal;
		wifi_getApSecurityModeEnabled(wlanIndex,SecurityMode);
                if(strcmp(SecurityMode,"WPA-Personal") == 0)
                                pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_WPA_Personal;
                else if(strcmp(SecurityMode,"WPA2-Personal") == 0)
                                pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_WPA2_Personal;
                else if(strcmp(SecurityMode,"WPA-WPA2-Personal") == 0)
                                pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal;

                AnscCopyString(pCfg->KeyPassphrase,password);

        }
        }
        if((ApinsCount == 5) || (ApinsCount == 6))//LNT_EMU
        {
                pCfg->ModeEnabled = COSA_DML_WIFI_SECURITY_None;
                AnscCopyString(pCfg->KeyPassphrase,"");
        }
	
	wifi_getApWpaEncryptionMode(wlanIndex,method);
        if (strncmp(method, "TKIPEncryption",strlen("TKIPEncryption")) == 0)
        {
            pCfg->EncryptionMethod = COSA_DML_WIFI_AP_SEC_TKIP;
        } else if (strncmp(method, "AESEncryption",strlen("AESEncryption")) == 0)
        {
            pCfg->EncryptionMethod = COSA_DML_WIFI_AP_SEC_AES;
        }
        else if (strncmp(method, "TKIPandAESEncryption",strlen("TKIPandAESEncryption")) == 0)
        {
            pCfg->EncryptionMethod = COSA_DML_WIFI_AP_SEC_AES_TKIP;
        }

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

	int wlanIndex = (( pSsid[strlen(pSsid)-1] ) - '0');
	memcpy(&g_WiFiApSecCfg, pCfg, sizeof(COSA_DML_WIFI_APSEC_CFG));
	// WPA
	if ( pCfg->ModeEnabled >= COSA_DML_WIFI_SECURITY_WPA_Personal &&
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
		else if ( pCfg->EncryptionMethod == COSA_DML_WIFI_AP_SEC_AES_TKIP)
		{
			strcpy(method,"TKIPandAESEncryption");
		}
		wifi_setApWpaEncryptionMode(wlanIndex, method);
	}
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
	int wlanIndex = (( pSsid[strlen(pSsid)-1] ) - '0');
	if (!pEntry || !pSsid)
	{
		return ANSC_STATUS_FAILURE;
	}
	PCOSA_DML_WIFI_APWPS_CFG  pCfg = &pEntry->Cfg;
        PCOSA_DML_WIFI_APWPS_INFO pInfo = &pEntry->Info;

	if (FALSE)
	{
		return returnStatus;
	}
	else
	{
		returnStatus = CosaDmlWiFiApWpsGetCfg ( hContext, pSsid, pCfg );

		if (returnStatus == ANSC_STATUS_SUCCESS) {
			returnStatus = CosaDmlWiFiApWpsGetInfo ( hContext, pSsid, pInfo );
		}
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
	PCOSA_DML_WIFI_APWPS_CFG pStoredCfg = NULL;
	int wlanIndex = 0;
        char methodsEnabled[64] = {0};
	char recName[256] = {0};
	char strValue[32] = {0};
	int retPsmSet = CCSP_SUCCESS;
	wlanIndex=(( pSsid[strlen(pSsid)-1] ) - '0');

	if (FALSE)
	{
		return returnStatus;
	}
	else
	{
	/*	wifi_setApWpsEnable(wlanIndex, pCfg->bEnabled);
		//ConfigMethods
		if ( pCfg->ConfigMethodsEnabled == COSA_DML_WIFI_WPS_METHOD_PushButton ) {
			wifi_setApWpsConfigMethodsEnabled(wlanIndex,"PushButton");
		} else  if (pCfg->ConfigMethodsEnabled == COSA_DML_WIFI_WPS_METHOD_Pin) {
			wifi_setApWpsConfigMethodsEnabled(wlanIndex,"Keypad,Label,Display");
		if ( pCfg->ConfigMethodsEnabled == (COSA_DML_WIFI_WPS_METHOD_PushButton|COSA_DML_WIFI_WPS_METHOD_Pin) ) {
			wifi_setApWpsConfigMethodsEnabled(wlanIndex,"PushButton,Keypad,Label,Display");
		}*/
		//PSM
		snprintf(recName, sizeof(recName) - 1, WpsPushButton, wlanIndex+1);
		snprintf(strValue, sizeof(strValue) - 1,"%d", pCfg->WpsPushButton);
		printf("%s: Setting %s to %d(%s)\n", __FUNCTION__, recName, pCfg->WpsPushButton, strValue);
		retPsmSet = PSM_Set_Record_Value2(bus_handle,g_Subsystem, recName, ccsp_string, strValue);
		if (retPsmSet != CCSP_SUCCESS) {
			printf("%s PSM_Set_Record_Value2 returned error %d while setting %s \n",__FUNCTION__, retPsmSet, recName);
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
	int wlanIndex = 0;
        char methodsEnabled[64] = {0};
	wlanIndex=(( pSsid[strlen(pSsid)-1] ) - '0');
	char recName[256];
	char *strValue = NULL;
	int retPsmGet = CCSP_SUCCESS;

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
    int wlanIndex = (( pSsid[strlen(pSsid)-1] ) - '0');
    char methodsEnabled[64] = {0};
    char  configState[32] = {0};
    unsigned int pin = 0;
    if (!pInfo || !pSsid)
    {
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
	PCOSA_DML_WIFI_AP_ASSOC_DEVICE  AssocDeviceArray  = (PCOSA_DML_WIFI_AP_ASSOC_DEVICE)NULL;
	ULONG                           index             = 0;
	ULONG                           ulCount           = 0;
	wifi_associated_dev_t *wlanDevice = NULL;
	int InstanceNumber = 0,array_size = 0,wlanIndex;
	InstanceNumber = (( pSsid[strlen(pSsid)-1] ) - '0') +1;
	wlanIndex = InstanceNumber - 1;
	
	if (FALSE)
	{
		*pulCount = ulCount;

		return AssocDeviceArray;
	}
	else
	{
		//wifi_getAllAssociatedDeviceDetail(InstanceNumber,pulCount,&wlanDevice);//LNT_EMU
		wifi_getApAssociatedDeviceDiagnosticResult(wlanIndex, &wlanDevice, &array_size);//RDKB-EMU

		/*For example we have 5 AssocDevices*/
		// *pulCount = 5;//LNT_EMU
		*pulCount = array_size;
		AssocDeviceArray = (PCOSA_DML_WIFI_AP_ASSOC_DEVICE)AnscAllocateMemory(sizeof(COSA_DML_WIFI_AP_ASSOC_DEVICE)*(*pulCount));
		if ( !AssocDeviceArray )
		{
			*pulCount = 0;
			return NULL;
		}

		for(index = 0; index < *pulCount; index++)
		{
			AssocDeviceArray[index].MacAddress[0]        = wlanDevice[index].cli_MACAddress[0];
			AssocDeviceArray[index].MacAddress[1]        = wlanDevice[index].cli_MACAddress[1];
			AssocDeviceArray[index].MacAddress[2]        = wlanDevice[index].cli_MACAddress[2];
			AssocDeviceArray[index].MacAddress[3]        = wlanDevice[index].cli_MACAddress[3];
			AssocDeviceArray[index].MacAddress[4]        = wlanDevice[index].cli_MACAddress[4];
			AssocDeviceArray[index].MacAddress[5]        = wlanDevice[index].cli_MACAddress[5];
			AssocDeviceArray[index].SignalStrength       = wlanDevice[index].cli_SignalStrength;
			AssocDeviceArray[index].AuthenticationState  = TRUE;
			AssocDeviceArray[index].LastDataDownlinkRate = wlanDevice[index].cli_LastDataDownlinkRate;
			AssocDeviceArray[index].LastDataUplinkRate   = wlanDevice[index].cli_LastDataUplinkRate;
		      //AssocDeviceArray[index].SignalStrength       = 50+index;//LNT_EMU
			AssocDeviceArray[index].Retransmissions      = 0;
			//AssocDeviceArray[index].Retransmissions      = wlanDevice[index].cli_Retransmissions;
#if 1
			AssocDeviceArray[index].Active               = TRUE;
			//AssocDeviceArray[index].DataFramesSentAck               = wlanDevice[index].cli_DataFramesSentAck;
			//AssocDeviceArray[index].DataFramesSentNoAck               = wlanDevice[index].cli_DataFramesSentNoAck;
			AssocDeviceArray[index].DataFramesSentAck               = 0;
			AssocDeviceArray[index].DataFramesSentNoAck               = 0;
			AssocDeviceArray[index].BytesSent               = wlanDevice[index].cli_BytesSent;
			AssocDeviceArray[index].BytesReceived               = wlanDevice[index].cli_BytesReceived;
			AssocDeviceArray[index].RSSI               = wlanDevice[index].cli_RSSI;
			//AssocDeviceArray[index].MinRSSI               = wlanDevice[index].cli_MinRSSI;
			//AssocDeviceArray[index].MaxRSSI              = wlanDevice[index].cli_MaxRSSI;
			AssocDeviceArray[index].MinRSSI               = 0;
			AssocDeviceArray[index].MaxRSSI              = 0;
			//AssocDeviceArray[index].Disassociations               = wlanDevice[index].cli_Disassociations;
			//AssocDeviceArray[index].AuthenticationFailures               = wlanDevice[index].cli_AuthenticationFailures;
			AssocDeviceArray[index].Disassociations               = 0;
			AssocDeviceArray[index].AuthenticationFailures               = 0;
			AssocDeviceArray[index].SNR               = wlanDevice[index].cli_SNR;
                        memcpy(AssocDeviceArray[index].OperatingChannelBandwidth,"20MHz", sizeof(char)*64);
			if((InstanceNumber == 1) || (InstanceNumber == 5))
                                memcpy(AssocDeviceArray[index].OperatingStandard,"2.4GHz", sizeof(char)*64);
                        else if((InstanceNumber == 2) || (InstanceNumber == 6))
                                memcpy(AssocDeviceArray[index].OperatingStandard,"5GHz", sizeof(char)*64);
			memcpy(AssocDeviceArray[index].InterferenceSources,"0", sizeof(char)*64);
#endif
		

		}
		free(wlanDevice);//RDKB-EMU
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
        char SSIDName[COSA_DML_WIFI_MAX_SSID_NAME_LEN];
        char PassKey[256];
        char ChannelCount[256];
        char *strValue = NULL;
        char *recValue = NULL;
        char *paramValue = NULL;
        int retPsmGet_SSID = CCSP_SUCCESS;
        int retPsmGet_PassKey = CCSP_SUCCESS;
        int retPsmGet_Channel = CCSP_SUCCESS;
	char buf[256] = {0};
        int fd = 0;
	BOOL GetSSIDEnable;
	system("sh /lib/rdk/restore-wifi.sh");
	defaultwifi_restarting_process();
#if 0
        for (instanceNumber = 1; instanceNumber <= gSsidCount; instanceNumber++)
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

                                wifi_setSSIDName(instanceNumber-1,strValue);
                                wifi_setApSecurityPreSharedKey(instanceNumber-1,recValue);
                                wifi_setRadioChannel(instanceNumber-1, atoi(paramValue));

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
        			wifi_getSSIDEnable(instanceNumber-1,&GetSSIDEnable);
                		fd = open("/tmp/Get2gssidEnable.txt","r");
                		if(fd == -1)
                		{
                        		sprintf(buf,"%s%d%s","echo ",GetSSIDEnable," > /tmp/Get2gssidEnable.txt");
                        		system(buf);
                		}
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

                                wifi_setSSIDName(instanceNumber-1,strValue);
                                wifi_setApSecurityPreSharedKey(instanceNumber-1,recValue);
                                wifi_setRadioChannel(instanceNumber-1, atoi(paramValue));

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
       				wifi_getSSIDEnable(instanceNumber-1,&GetSSIDEnable);
                                fd = open("/tmp/Get5gssidEnable.txt","r");
                                if(fd == -1)
                                {
                                        sprintf(buf,"%s%d%s","echo ",GetSSIDEnable," > /tmp/Get5gssidEnable.txt");
                                        system(buf);
                                }
                                KillHostapd_5g();
                        }

                }
                else {
                        return 0;
                }
        }
#endif
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

BOOL is_mesh_enabled()
{
    int ret = ANSC_STATUS_FAILURE;
    parameterValStruct_t    **valStructs = NULL;
    char dstComponent[64]="eRT.com.cisco.spvtg.ccsp.meshagent";
    char dstPath[64]="/com/cisco/spvtg/ccsp/meshagent";
    char *paramNames[]={"Device.DeviceInfo.X_RDKCENTRAL-COM_xOpsDeviceMgmt.Mesh.Enable"};
    int  valNum = 0;

    ret = CcspBaseIf_getParameterValues(
            bus_handle,
            dstComponent,
            dstPath,
            paramNames,
            1,
            &valNum,
            &valStructs);

    if(CCSP_Message_Bus_OK != ret)
    {
         CcspTraceError(("%s CcspBaseIf_getParameterValues %s error %d\n", __FUNCTION__,paramNames[0],ret));
         free_parameterValStruct_t(bus_handle, valNum, valStructs);
         return FALSE;
    }

    CcspTraceWarning(("valStructs[0]->parameterValue = %s\n",valStructs[0]->parameterValue));

    if(strncmp("true", valStructs[0]->parameterValue,4)==0)
    {
         free_parameterValStruct_t(bus_handle, valNum, valStructs);
         return TRUE;
    }
    else
    {
         free_parameterValStruct_t(bus_handle, valNum, valStructs);
         return FALSE;
    }
}

// Function gets the initial values of NeighbouringDiagnostic
ANSC_STATUS
CosaDmlWiFiNeighbouringGetEntry
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_NEIGHTBOURING_WIFI_DIAG_CFG   pEntry
    )
{

    if (!pEntry) return ANSC_STATUS_FAILURE;
        printf("%s\n",__FUNCTION__);
        CosaDmlGetNeighbouringDiagnosticEnable(&pEntry->bEnable);
        return ANSC_STATUS_SUCCESS;
}

// Function reads NeighbouringDiagnosticEnable value from PSM
void CosaDmlGetNeighbouringDiagnosticEnable(BOOLEAN *DiagEnable)
{
        printf("%s\n",__FUNCTION__);
        char* strValue = NULL;
        PSM_Get_Record_Value2(bus_handle,g_Subsystem, DiagnosticEnable, NULL, &strValue);

        if(strValue)
        {
                *DiagEnable = (atoi(strValue) == 1) ? TRUE : FALSE;
                ((CCSP_MESSAGE_BUS_INFO *)bus_handle)->freefunc(strValue);
        }
        else
        {
                *DiagEnable =FALSE;
        }
}

// Function sets NeighbouringDiagnosticEnable value to PSM
void CosaDmlSetNeighbouringDiagnosticEnable(BOOLEAN DiagEnableVal)
{
        char strValue[10];
        printf("%s\n",__FUNCTION__);
        memset(strValue,0,sizeof(strValue));
        sprintf(strValue,"%d",DiagEnableVal);
        PSM_Set_Record_Value2(bus_handle,g_Subsystem, DiagnosticEnable, ccsp_string, strValue);

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
	char ifname_2g[50] = {0},ifname_5g[50] = {0};
	char wifistatus_2g[50] = {0},wifistatus_5g[50] = {0};
	char cmd[80];

	wifi_getNeighboringWiFiDiagnosticResult2(0, &wifiNeighbour_2,&count_2);
	snprintf(cmd, sizeof(cmd), "syscfg set NeighboringWifiScan 1");
	system(cmd);
        wifi_getNeighboringWiFiDiagnosticResult2(1, &wifiNeighbour_5,&count_5);
	snprintf(cmd, sizeof(cmd), "syscfg set NeighboringWifiScan 0");
	system(cmd);

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

	return NULL;
}
ANSC_STATUS
CosaDmlWiFi_doNeighbouringScan ( PCOSA_DML_NEIGHTBOURING_WIFI_DIAG_CFG pNeighScan)
{
    if (!pNeighScan) return ANSC_STATUS_FAILURE;
        fprintf(stderr, "-- %s1Y %d\n", __func__, __LINE__);
        pthread_t tid;
        AnscCopyString(pNeighScan->DiagnosticsState, "Requested");
    if(pthread_create(&tid,NULL,CosaDmlWiFi_doNeighbouringScanThread, (void*)pNeighScan))  {
                AnscCopyString(pNeighScan->DiagnosticsState, "Error");
                return ANSC_STATUS_FAILURE;
        }
        return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFi_SetATMEnable(PCOSA_DML_WIFI_ATM pATM, BOOL bValue) {
        if( NULL == pATM )
                return ANSC_STATUS_FAILURE;

        pATM->Enable=bValue;
#ifdef _ATM_SUPPORT
    //    wifi_setATMEnable(bValue);
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
  //                      wifi_setApATMAirTimePercent(apIndex, AirTimePercent);
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
//                        wifi_setApATMSta(apIndex, MACAddress, AirTimePercent);
#endif
                }
        token = strtok_r(restStr, ",", &restStr);
    }
        return ANSC_STATUS_SUCCESS;

}

ANSC_STATUS
CosaDmlWiFi_GetATMOptions(PCOSA_DML_WIFI_ATM  pATM) {
        if( NULL == pATM )
                return ANSC_STATUS_FAILURE;

#ifdef _ATM_SUPPORT
//        wifi_getATMCapable(&pATM->Capable);
//        wifi_getATMEnable(&pATM->Enable);
#endif
	pATM->Enable = FALSE;
	pATM->Capable = FALSE;
        return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaWifiRegGetATMInfo( ANSC_HANDLE   hThisObject){
    PCOSA_DML_WIFI_ATM                  pATM    = (PCOSA_DML_WIFI_ATM     )hThisObject;
    int g=0;
        int s=0;
        UINT percent=0;
        UCHAR buf[256]={0};
        char *token=NULL, *dev=NULL;

        pATM->grpCount=ATM_GROUP_COUNT;
        for(g=0; g<pATM->grpCount; g++) {
                snprintf(pATM->APGroup[g].APList, COSA_DML_WIFI_ATM_MAX_APLIST_STR_LEN, "%d,%d", g*2+1, g*2+2);

#ifdef _ATM_SUPPORT
//                wifi_getApATMAirTimePercent(g*2, &percent);
  //              wifi_getApATMSta(g*2, buf, sizeof(buf));
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

#endif

