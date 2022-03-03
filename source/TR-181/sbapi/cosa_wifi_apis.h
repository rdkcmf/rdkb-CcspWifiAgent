/*
 * If not stated otherwise in this file or this component's LICENSE file the
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

    module: cosa_nat_apis.h

        For COSA Data Model Library Development

    -------------------------------------------------------------------

    description:

        This file defines the apis for objects to support Data Model Library.

    -------------------------------------------------------------------


    author:

        COSA XML TOOL CODE GENERATOR 1.0

    -------------------------------------------------------------------

    revision:

        01/11/2011    initial revision.

**************************************************************************/


#ifndef  _COSA_WIFI_APIS_H
#define  _COSA_WIFI_APIS_H

#include "cosa_apis.h"
#include "ccsp_base_api.h"
#include "wifi_hal.h"
#include <sysevent/sysevent.h>

#ifdef WIFI_HAL_VERSION_3
#include "wifi_hal.h"

#define CCSP_WIFI_TRACE 1
#define CCSP_WIFI_INFO  2

#endif

//#include "secure_wrapper.h"

#define  COSA_DML_WIFI_MAX_MAC_FILTER_NUM           50

#define COSA_DML_WEP_KEY_NUM                                   4

#define COSA_DML_WIFI_MAX_SSID_NAME_LEN               33

#define COSA_DML_WIFI_MAX_11N_CALC_RESULTS         3

#define  COSA_DML_WIFI_MAX_BAND_STEERING_HISTORY_NUM  ( 1024 ) // 2 * 512 = 1024 bytes

#define COSA_DML_WIFI_MAX_BAND_STEERING_APGROUP_STR_LEN 64

#define COSA_DML_WIFI_ATM_MAX_APGROUP_NUM				8
#define COSA_DML_WIFI_ATM_MAX_APLIST_STR_LEN            256 
#define COSA_DML_WIFI_ATM_MAX_STA_NUM	              	32 
#if defined(DMCLI_SUPPORT_TO_ADD_DELETE_VAP)
#define WIFI_MAX_ENTRIES_PER_RADIO                      8 /* Maximum VAP per Radio */
#define COSA_DML_WIFI_STR_LENGHT_8 8
#define COSA_DML_WIFI_STR_LENGHT_128 128
#endif

#ifdef WIFI_HAL_VERSION_3
#define WIFI_INDEX_MAX MAX_VAP
#define MAX_NUM_PRIVATE_VAP MAX_NUM_RADIOS
#else
#define WIFI_INDEX_MAX 16
#endif

#if defined (MULTILAN_FEATURE)
#define WIFI_INDEX_MIN 6    /* ccsp webui requires 6 default entries of SSID/AccessPoint */
#endif

#define  MAC_ADDRESS_LENGTH  13

 /* Active Measurement Step count */
#define ACTIVE_MSMT_STEP_COUNT        32
/* Active Measurement Plan ID length */
#define PLAN_ID_LEN    33

#ifdef WIFI_HAL_VERSION_3
#define SAE_PASSPHRASE_MIN_LENGTH 8
#define SAE_PASSPHRASE_MAX_LENGTH 64
#endif

#ifndef ULLONG
#define ULLONG unsigned long long
#endif

// RDKB-40257 - change sysevent call to library
extern int gWrite_sysevent_fd;
extern token_t gWrite_sysEtoken;
INT initGSyseventVar();

/* Active Measurement Step Info */
struct
_COSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG
{
    UINT            StepId;
    BOOLEAN         bSrcMacChanged;
    BOOLEAN         bDstMacChanged;
    CHAR            SourceMac[MAC_ADDRESS_LENGTH];
    CHAR            DestMac[MAC_ADDRESS_LENGTH];
}_struct_pack_;

typedef struct _COSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG COSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG, *PCOSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG;

struct
_COSA_DML_WIFI_ACTIVE_MSMT_STEP_FULL
{
    COSA_DML_WIFI_ACTIVE_MSMT_STEP_CFG   StepCfg[ACTIVE_MSMT_STEP_COUNT];
}_struct_pack_;

typedef struct _COSA_DML_WIFI_ACTIVE_MSMT_STEP_FULL COSA_DML_WIFI_ACTIVE_MSMT_STEP_FULL, *PCOSA_DML_WIFI_ACTIVE_MSMT_STEP_FULL;

#if defined(_COSA_BCM_MIPS_) || defined(_XB6_PRODUCT_REQ_) || defined(_COSA_BCM_ARM_) || defined(_PLATFORM_TURRIS_)
typedef  struct
_COSA_BOOTSTRAP_STR
{
    CHAR                    ActiveValue[4];
    CHAR		    UpdateSource[16];
}
COSA_BOOTSTRAP_STR;

typedef  struct
_COSA_DATAMODEL_RDKB_WIFIREGION_CLASS_CONTENT
{
        COSA_BOOTSTRAP_STR                    Code;
}
COSA_DATAMODEL_RDKB_WIFIREGION, *PCOSA_DATAMODEL_RDKB_WIFIREGION;
#else
typedef  struct
_COSA_DATAMODEL_RDKB_WIFIREGION_CLASS_CONTENT
{
        CHAR                    Code[4];
}
COSA_DATAMODEL_RDKB_WIFIREGION, *PCOSA_DATAMODEL_RDKB_WIFIREGION;
#endif

typedef  enum
_COSA_DML_WIFI_FREQ_BAND
{
    COSA_DML_WIFI_FREQ_BAND_2_4G        = 0x1,
    COSA_DML_WIFI_FREQ_BAND_5G          = 0x2,
    COSA_DML_WIFI_FREQ_BAND_5G_L        = 0x4,
    COSA_DML_WIFI_FREQ_BAND_5G_H        = 0x8,
    COSA_DML_WIFI_FREQ_BAND_6G          = 0x10,
    COSA_DML_WIFI_FREQ_BAND_60          = 0x20
}
COSA_DML_WIFI_FREQ_BAND, *PCOSA_DML_WIFI_FREQ_BAND;


typedef  enum
_COSA_DML_WIFI_STD
{
    COSA_DML_WIFI_STD_a             = 1,
    COSA_DML_WIFI_STD_b             = 2,
    COSA_DML_WIFI_STD_g             = 4,
    COSA_DML_WIFI_STD_n             = 8,
    COSA_DML_WIFI_STD_ac            = 16
#ifdef _WIFI_AX_SUPPORT_
,
    COSA_DML_WIFI_STD_ax            = 32
#endif
#ifdef WIFI_HAL_VERSION_3
,
    COSA_DML_WIFI_STD_h             = 64,
    COSA_DML_WIFI_STD_ad            = 128
#endif

}
COSA_DML_WIFI_STD, *PCOSA_DML_WIFI_STD;


typedef  enum
_COSA_DML_WIFI_CHAN_BW
{
    
    COSA_DML_WIFI_CHAN_BW_AUTO		= 0,
    COSA_DML_WIFI_CHAN_BW_20M,
    COSA_DML_WIFI_CHAN_BW_40M,
    COSA_DML_WIFI_CHAN_BW_80M,
    COSA_DML_WIFI_CHAN_BW_160M,
    COSA_DML_WIFI_CHAN_BW_80_80M
}
COSA_DML_WIFI_CHAN_BW, *PCOSA_DML_WIFI_CHAN_BW;


typedef  enum
_COSA_DML_WIFI_EXT_CHAN
{
    COSA_DML_WIFI_EXT_CHAN_Above        = 1,
    COSA_DML_WIFI_EXT_CHAN_Below,
    COSA_DML_WIFI_EXT_CHAN_Auto
}
COSA_DML_WIFI_EXT_CHAN, *PCOSA_DML_WIFI_EXT_CHAN;


typedef  enum
_COSA_DML_WIFI_GUARD_INTVL
{
    COSA_DML_WIFI_GUARD_INTVL_400ns     = 1,
    COSA_DML_WIFI_GUARD_INTVL_800ns,
    COSA_DML_WIFI_GUARD_INTVL_Auto,
    COSA_DML_WIFI_GUARD_INTVL_1600ns,
    COSA_DML_WIFI_GUARD_INTVL_3200ns
}
COSA_DML_WIFI_GUARD_INTVL, *PCOSA_DML_WIFI_GUARD_INTVL;

typedef  enum
_COSA_DML_WIFI_BASICRATE
{
    COSA_DML_WIFI_BASICRATE_Default      = 1,
    COSA_DML_WIFI_BASICRATE_1_2Mbps,
    COSA_DML_WIFI_BASICRATE_All
}
COSA_DML_WIFI_BASICRATE, *PCOSA_DML_WIFI_BASICRATE;

typedef  enum
_COSA_DML_WIFI_TXRATE
{
    COSA_DML_WIFI_TXRATE_Auto               = 1,
    COSA_DML_WIFI_TXRATE_6M,
    COSA_DML_WIFI_TXRATE_9M,
    COSA_DML_WIFI_TXRATE_12M,
    COSA_DML_WIFI_TXRATE_18M,
    COSA_DML_WIFI_TXRATE_24M,
    COSA_DML_WIFI_TXRATE_36M,
    COSA_DML_WIFI_TXRATE_48M,
    COSA_DML_WIFI_TXRATE_54M
}
COSA_DML_WIFI_TXRATE, *PCOSA_DML_WIFI_TXRATE;


typedef  enum
_COSA_DML_WIFI_AP_STATUS
{
    COSA_DML_WIFI_AP_STATUS_Disabled    = 1,
    COSA_DML_WIFI_AP_STATUS_Enabled,
    COSA_DML_WIFI_AP_STATUS_Error_Misconfigured,
    COSA_DML_WIFI_AP_STATUS_Error
}
COSA_DML_WIFI_AP_STATUS, *PCOSA_DML_WIFI_AP_STATUS;


typedef  enum
_COSA_DML_WIFI_SECURITY
{
    COSA_DML_WIFI_SECURITY_None                            = 0x00000001,
    COSA_DML_WIFI_SECURITY_WEP_64                          = 0x00000002,
    COSA_DML_WIFI_SECURITY_WEP_128                         = 0x00000004,
    COSA_DML_WIFI_SECURITY_WPA_Personal                    = 0x00000008,
    COSA_DML_WIFI_SECURITY_WPA2_Personal                   = 0x00000010,
    COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal               = 0x00000020,
    COSA_DML_WIFI_SECURITY_WPA_Enterprise                  = 0x00000040,
    COSA_DML_WIFI_SECURITY_WPA2_Enterprise                 = 0x00000080,
    COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise             = 0x00000100,
    COSA_DML_WIFI_SECURITY_WPA3_Personal                   = 0x00000200,
    COSA_DML_WIFI_SECURITY_WPA3_Personal_Transition        = 0x00000400,
    COSA_DML_WIFI_SECURITY_WPA3_Enterprise                 = 0x00000800
}
COSA_DML_WIFI_SECURITY, *PCOSA_DML_WIFI_SECURITY;

typedef  enum
_COSA_DML_WIFI_AP_SEC_ENCRYPTION
{
    COSA_DML_WIFI_AP_SEC_TKIP    = 1,
    COSA_DML_WIFI_AP_SEC_AES,
    COSA_DML_WIFI_AP_SEC_AES_TKIP,
    COSA_DML_WIFI_AP_SEC_NONE
}
COSA_DML_WIFI_AP_SEC_ENCRYPTION, *PCOSA_DML_WIFI_AP_SEC_ENCRYPTION;

typedef  enum
_COSA_DML_WIFI_WPS_METHOD
{
    COSA_DML_WIFI_WPS_METHOD_UsbFlashDrive      = 0x00000001,
    COSA_DML_WIFI_WPS_METHOD_Ethernet           = 0x00000002,
    COSA_DML_WIFI_WPS_METHOD_ExternalNFCToken   = 0x00000004,
    COSA_DML_WIFI_WPS_METHOD_IntgratedNFCToken  = 0x00000008,
    COSA_DML_WIFI_WPS_METHOD_NFCInterface       = 0x00000010,
    COSA_DML_WIFI_WPS_METHOD_PushButton         = 0x00000020,
    COSA_DML_WIFI_WPS_METHOD_Pin                = 0x00000040,
    COSA_DML_WIFI_WPS_METHOD_Label              = 0x00000080,
    COSA_DML_WIFI_WPS_METHOD_Display            = 0x00000100,
    COSA_DML_WIFI_WPS_METHOD_PhysicalPushButton = 0x00000200,
    COSA_DML_WIFI_WPS_METHOD_PhysicalDisplay    = 0x00000400,
    COSA_DML_WIFI_WPS_METHOD_VirtualPushButton  = 0x00000800,
    COSA_DML_WIFI_WPS_METHOD_VirtualDisplay     = 0x00001000,
    COSA_DML_WIFI_WPS_METHOD_EasyConnect        = 0x00002000
}
COSA_DML_WIFI_WPS_METHOD, *PCOSA_DML_WIFI_WPS_METHOD;

typedef enum
_COSA_DML_WIFI_AP_SEC_WEPMODE
{
    COSA_DML_WIFI_WEPMODE_WEP64 = 1,
    COSA_DML_WIFI_WEPMODE_WEP128,
}
COSA_DML_WIFI_AP_SEC_WEPMODE, *PCOSA_DML_WIFI_AP_SEC_WEPMODE;

typedef enum
_COSA_DML_WIFI_RADIO_POWER
{
    COSA_DML_WIFI_POWER_LOW = 0,
    COSA_DML_WIFI_POWER_DOWN,
    COSA_DML_WIFI_POWER_UP,
}
COSA_DML_WIFI_RADIO_POWER, *PCOSA_DML_WIFI_RADIO_POWER;

typedef enum
_COSA_DML_WIFI_CALC_STATUS
{
    COSA_DML_WIFI_PASS = 1,
    COSA_DML_WIFI_FAIL,
    COSA_DML_WIFI_INVALID
}
COSA_DML_WIFI_CALC_STATUS, *PCOSA_DML_WIFI_CALC_STATUS;

typedef enum
_COSA_DML_WIFI_CALC_OPT
{
    COSA_DML_WIFI_CALC_DISABLE = 0,
    COSA_DML_WIFI_CALC_ENABLE,
    COSA_DML_WIFI_CALC_ENABLEALLCHANNELS
}
COSA_DML_WIFI_CALC_OPT, *PCOSA_DML_WIFI_CALC_OPT;

/*
 *  Structure definitions for WiFi Radio
 */
struct 
_COSA_DML_WIFI_RADIO_CFG
{
    ULONG                           InstanceNumber;
    char                            Alias[COSA_DML_ALIAS_NAME_LENGTH];
    BOOLEAN                         bEnabled;
    ULONG                           LastChange;
    COSA_DML_WIFI_FREQ_BAND         OperatingFrequencyBand;
    ULONG                           OperatingStandards;         /* Bitmask of COSA_DML_WIFI_STD */
    ULONG                           Channel;
    BOOLEAN                         AutoChannelEnable;
    ULONG                           AutoChannelRefreshPeriod;
    COSA_DML_WIFI_CHAN_BW           OperatingChannelBandwidth;
    COSA_DML_WIFI_EXT_CHAN          ExtensionChannel;
    COSA_DML_WIFI_GUARD_INTVL       GuardInterval;
    int                             MCS;
    int                             TransmitPower;
    BOOLEAN                         IEEE80211hEnabled;
    BOOLEAN			    X_COMCAST_COM_DFSEnable; 
    BOOLEAN			    X_COMCAST_COM_IGMPSnoopingEnable; 
    BOOLEAN             X_COMCAST_COM_DFSSupport;
	BOOLEAN             X_COMCAST_COM_DCSSupported;
    BOOLEAN			    X_COMCAST_COM_DCSEnable; 
    BOOLEAN			    X_COMCAST_COM_AutoChannelRefreshPeriodSupported;	
    BOOLEAN			    X_COMCAST_COM_IEEE80211hSupported;		
    BOOLEAN			    X_COMCAST_COM_ReverseDirectionGrantSupported;		
    BOOLEAN			    X_COMCAST_COM_RtsThresholdSupported;
    char                            RegulatoryDomain[4];
    char                            BasicDataTransmitRates[256];
    char                            SupportedDataTransmitRates[2048];
    char                            OperationalDataTransmitRates[2048];
    /* Below is Cisco Extensions */
    COSA_DML_WIFI_BASICRATE         BasicRate;
    COSA_DML_WIFI_TXRATE            TxRate;
    BOOLEAN                         APIsolation;
    BOOLEAN                         FrameBurst;
    BOOLEAN                         CTSProtectionMode;
    ULONG                           BeaconInterval;
    ULONG                           DTIMInterval;
    ULONG                           FragmentationThreshold;
    ULONG                           RTSThreshold;
    /* USGv2 Extensions */
    int                             MbssUserControl;
    int                             AdminControl;
    int                             OnOffPushButtonTime;
    int                             ObssCoex;
    int                             MulticastRate;
    BOOL                            ApplySetting;
    int                                 ApplySettingSSID;
    BOOL                            X_CISCO_COM_ReverseDirectionGrant;
    BOOL                            X_CISCO_COM_AggregationMSDU;
    BOOL                            X_CISCO_COM_AutoBlockAck;
    BOOL                            X_CISCO_COM_DeclineBARequest;
/*    ULONG                           X_CISCO_COM_HTTxStream;
    ULONG                           X_CISCO_COM_HTRxStream;*/
    BOOL                            X_CISCO_COM_STBCEnable;
    BOOL                            X_CISCO_COM_11nGreenfieldEnabled;
    BOOL                            X_CISCO_COM_WirelessOnOffButton;
    BOOL                            AutoChannelRefreshPeriodSupported;
    BOOL                            RtsThresholdSupported;		
    BOOL                            ReverseDirectionGrantSupported;
    /* Technicolor Extensions */
    int                             TC;
    int                             CT;
    int                             CNI;
    int                             CCm;

    /* For X_RDKCENTRAL-COM_DCSEnable */
    BOOL                            X_RDKCENTRAL_COM_DCSEnable;
	int 							iX_RDKCENTRAL_COM_clientInactivityTimeout;
#ifdef WIFI_HAL_VERSION_3
    BOOL                            isRadioConfigChanged;
#endif //WIFI_HAL_VERSION_3
	ULONG X_RDK_OffChannelTscan;
	ULONG X_RDK_OffChannelNscan;
	ULONG X_RDK_OffChannelNchannel;
	ULONG X_RDK_OffChannelTidle;
}_struct_pack_;

typedef struct _COSA_DML_WIFI_RADIO_CFG COSA_DML_WIFI_RADIO_CFG,  *PCOSA_DML_WIFI_RADIO_CFG;

struct 
_COSA_DML_WIFI_RADIO_CALC_RESULT
{
    /* Technicolor Extensions */
    ULONG                        InstanceNumber;
    ULONG                        WI;   /* radio index*/
    ULONG                        LastChange;
    int                          Result;
    char                         CT[64];
    char                         PfAC[512];
    char                         PnAC[512];
    char                         PfMC[512];
    char                         PnMC[512];
    char                         ResultsInfo[512];
}_struct_pack_;

typedef struct _COSA_DML_WIFI_RADIO_CALC_RESULT COSA_DML_WIFI_RADIO_CALC_RESULT,  *PCOSA_DML_WIFI_RADIO_CALC_RESULT;
/*
 *  Static portion of WiFi radio info
 */
struct
_COSA_DML_WIFI_RADIO_SINFO
{
    char                            Name[COSA_DML_ALIAS_NAME_LENGTH];
    BOOLEAN                         bUpstream;
    ULONG                           MaxBitRate;
    ULONG                           SupportedFrequencyBands;    /* Bitmask of COSA_DML_WIFI_FREQ_BAND */
    ULONG                           SupportedStandards;         /* Bitmask of COSA_DML_WIFI_STD */
    char                            PossibleChannels[512];
    BOOLEAN                         AutoChannelSupported;
    char                            TransmitPowerSupported[64];	
    BOOLEAN                         IEEE80211hSupported;
    UINT                            CipherSupported;
    ULONG                           SupportedChannelWidth;    /* Bitmask of COSA_DML_WIFI_CHAN_BW */
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_RADIO_SINFO COSA_DML_WIFI_RADIO_SINFO,  *PCOSA_DML_WIFI_RADIO_SINFO;


/*
 *  Dynamic portion of WiFi radio info
 */
#define MaxApScanLen 16384
struct
_COSA_DML_WIFI_RADIO_DINFO
{
    COSA_DML_IF_STATUS              Status;
    char                                            ChannelsInUse[512];
    char                                            ApChannelScan[MaxApScanLen];
    ULONG                                       LastScan;
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_RADIO_DINFO COSA_DML_WIFI_RADIO_DINFO,  *PCOSA_DML_WIFI_RADIO_DINFO;



typedef struct _COSA_DML_NEIGHTBOURING_WIFI_RESULT {
     //    CHAR  Radio[64];	
	 CHAR  SSID[64];	
	 CHAR  BSSID[64];	
	 CHAR  Mode[64];	
	 UINT  Channel;	
	 INT   SignalStrength;	
	 CHAR  SecurityModeEnabled[64];	
	 CHAR  EncryptionMode[64];	
	 CHAR  OperatingFrequencyBand[16];	
	 CHAR  SupportedStandards[64];	
	 CHAR  OperatingStandards[16];	 
	 CHAR  OperatingChannelBandwidth[16];	
	 UINT  BeaconPeriod;	
	 INT   Noise;	
	 CHAR  BasicDataTransferRates[256];	
	 CHAR  SupportedDataTransferRates[256];	
	 UINT  DTIMPeriod;	
	 UINT  ChannelUtilization;

}COSA_DML_NEIGHTBOURING_WIFI_RESULT,*PCOSA_DML_NEIGHTBOURING_WIFI_RESULT;

typedef struct _COSA_DML_NEIGHTBOURING_WIFI_DIAG_CFG {
   BOOL bEnable;
   CHAR DiagnosticsState[64];
   ULONG ResultCount;
#ifdef WIFI_HAL_VERSION_3
   ULONG resultCountPerRadio[MAX_NUM_RADIOS];
   PCOSA_DML_NEIGHTBOURING_WIFI_RESULT pResult[MAX_NUM_RADIOS];
#else
   ULONG ResultCount_2;
   ULONG ResultCount_5;
   PCOSA_DML_NEIGHTBOURING_WIFI_RESULT pResult_2;
   PCOSA_DML_NEIGHTBOURING_WIFI_RESULT pResult_5;
#endif
}COSA_DML_NEIGHTBOURING_WIFI_DIAG_CFG,*PCOSA_DML_NEIGHTBOURING_WIFI_DIAG_CFG;

struct
_COSA_DML_WIFI_RADIO_FULL
{
    COSA_DML_WIFI_RADIO_CFG         Cfg;
    COSA_DML_WIFI_RADIO_CALC_RESULT Calc[COSA_DML_WIFI_MAX_11N_CALC_RESULTS];
    COSA_DML_WIFI_RADIO_SINFO       StaticInfo;
    COSA_DML_WIFI_RADIO_DINFO       DynamicInfo;
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_RADIO_FULL COSA_DML_WIFI_RADIO_FULL, *PCOSA_DML_WIFI_RADIO_FULL;

#define RSL_MAX	2048
struct
_COSA_DML_WIFI_RADIO_STATS_RSL
{
    INT                             ReceivedSignalLevel[RSL_MAX];
	ULONG							Count;
	ULONG							StartIndex;
    //ULONG                           LastChange;
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_RADIO_STATS_RSL COSA_DML_WIFI_RADIO_STATS_RSL,  *PCOSA_DML_WIFI_RADIO_STATS_RSL;

struct
_COSA_DML_WIFI_RADIO_CHANNEL_STATS
{
        INT                          ch_number;
        ULLONG                        ch_utilization_busy_tx;
        ULLONG	                     ch_utilization_busy_self;
        ULONG                        LastUpdatedTime;
        ULLONG                       last_tx_count;
        ULLONG                       last_rx_count;
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_RADIO_CHANNEL_STATS  COSA_DML_WIFI_RADIO_CHANNEL_STATS,  *PCOSA_DML_WIFI_RADIO_CHANNEL_STATS;

struct
_COSA_DML_WIFI_RADIO_STATS
{
    ULONG                           BytesSent;
    ULONG                           BytesReceived;
    ULONG                           PacketsSent;
    ULONG                           PacketsReceived;
    ULONG                           ErrorsSent;
    ULONG                           ErrorsReceived;
    ULONG                           DiscardPacketsSent;
    ULONG                           DiscardPacketsReceived;
	ULONG                           PLCPErrorCount;
    ULONG                           FCSErrorCount;
    ULONG                           InvalidMACCount;
    ULONG                           PacketsOtherReceived;
	INT                             NoiseFloor;	
	
	ULONG							ChannelUtilization;
	INT 							ActivityFactor;
	INT								CarrierSenseThreshold_Exceeded;
	INT								RetransmissionMetric;
	INT								MaximumNoiseFloorOnChannel;
	INT								MinimumNoiseFloorOnChannel;
	INT								MedianNoiseFloorOnChannel;
	ULONG                           RadioStatisticsMeasuringRate;		//30 sec
	ULONG                           RadioStatisticsMeasuringInterval;	//1800 sec 
	BOOL                            RadioStatisticsEnable;
    ULONG                           StatisticsStartTime;
	ULONG							LastSampling;
	COSA_DML_WIFI_RADIO_STATS_RSL	RslInfo;
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_RADIO_STATS COSA_DML_WIFI_RADIO_STATS, *PCOSA_DML_WIFI_RADIO_STATS;


#if defined (FEATURE_CSI)
#define COSA_DML_CSI_CLIENTMACLIST  650
#define MAX_NUM_CSI_CLIENTS         3
/*
 *  Structure definitions for WiFi CSI
 */
struct
_COSA_DML_WIFI_CSI
{
    ULONG                    Index;
    BOOLEAN                  Enable;
    CHAR                     ClientMaclist[COSA_DML_CSI_CLIENTMACLIST];
    //used to revert in case the total number of mac is invalid
    BOOLEAN                  validEnable;
    CHAR                     validClientMaclist[COSA_DML_CSI_CLIENTMACLIST];
}_struct_pack_;

typedef struct _COSA_DML_WIFI_CSI COSA_DML_WIFI_CSI,  *PCOSA_DML_WIFI_CSI;
#endif
/*
 *  Structure definitions for WiFi SSID
 */
struct
_COSA_DML_WIFI_SSID_CFG
{
    ULONG                    InstanceNumber;
    char                     Alias[COSA_DML_ALIAS_NAME_LENGTH];
    BOOLEAN                  bEnabled;
    ULONG                    LastChange;
    char                     WiFiRadioName[COSA_DML_ALIAS_NAME_LENGTH]; /* Points to the underlying WiFi Radio */
    char                     DefaultSSID[COSA_DML_WIFI_MAX_SSID_NAME_LEN];
    char                     SSID[COSA_DML_WIFI_MAX_SSID_NAME_LEN];
    BOOLEAN                  EnableOnline;
    BOOLEAN                  RouterEnabled;
#ifdef WIFI_HAL_VERSION_3
    BOOLEAN                 isSsidChanged;
#endif
}_struct_pack_;

typedef struct _COSA_DML_WIFI_SSID_CFG COSA_DML_WIFI_SSID_CFG,  *PCOSA_DML_WIFI_SSID_CFG;


/*
 *  Static portion of WiFi SSID info
 */
struct
_COSA_DML_WIFI_SSID_SINFO
{
    char                            Name[COSA_DML_ALIAS_NAME_LENGTH];
    UCHAR                       BSSID[6];
    UCHAR                       MacAddress[6];
}_struct_pack_;

typedef struct _COSA_DML_WIFI_SSID_SINFO COSA_DML_WIFI_SSID_SINFO,  *PCOSA_DML_WIFI_SSID_SINFO;


/*
 *  Dynamic portion of WiFi SSID info
 */
struct
_COSA_DML_WIFI_SSID_DINFO
{
    COSA_DML_IF_STATUS              Status;
}_struct_pack_;

typedef struct _COSA_DML_WIFI_SSID_DINFO COSA_DML_WIFI_SSID_DINFO,  *PCOSA_DML_WIFI_SSID_DINFO;

struct
_COSA_DML_WIFI_SSID_FULL
{
    COSA_DML_WIFI_SSID_CFG          Cfg;
    COSA_DML_WIFI_SSID_SINFO        StaticInfo;
    COSA_DML_WIFI_SSID_DINFO        DynamicInfo;
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_SSID_FULL COSA_DML_WIFI_SSID_FULL, *PCOSA_DML_WIFI_SSID_FULL;

struct
_COSA_DML_WIFI_SSID_STATS
{
    ULONG                           BytesSent;
    ULONG                           BytesReceived;
    ULONG                           PacketsSent;
    ULONG                           PacketsReceived;
    ULONG                           ErrorsSent;
    ULONG                           ErrorsReceived;
    ULONG                           UnicastPacketsSent;
    ULONG                           UnicastPacketsReceived;
    ULONG                           DiscardPacketsSent;
    ULONG                           DiscardPacketsReceived;
    ULONG                           MulticastPacketsSent;
    ULONG                           MulticastPacketsReceived;
    ULONG                           BroadcastPacketsSent;
    ULONG                           BroadcastPacketsReceived;
    ULONG                           UnknownProtoPacketsReceived;
    ULONG                           RetransCount;
    ULONG                           FailedRetransCount;
    ULONG                           RetryCount;
    ULONG                           MultipleRetryCount;
    ULONG                           ACKFailureCount;
    ULONG                           AggregatedPacketCount;
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_SSID_STATS COSA_DML_WIFI_SSID_STATS, *PCOSA_DML_WIFI_SSID_STATS;

struct _COSA_DML_WIFI_SSID_BRIDGE 
{
    ULONG                           InstanceNumber;
    ULONG                           VlanId;
   // Support a Max of 16 SSIDs
    ULONG                           SSIDCount;
    char                                SSIDName[16][COSA_DML_ALIAS_NAME_LENGTH]; 
    char                                IpAddress[16];
    char                                IpSubNet[16];
    struct _COSA_DML_WIFI_SSID_BRIDGE  *next;
}; 

typedef  struct _COSA_DML_WIFI_SSID_BRIDGE COSA_DML_WIFI_SSID_BRIDGE, *PCOSA_DML_WIFI_SSID_BRIDGE;

typedef struct _COSA_DML_WIFI_GASCFG { // Values correspond to the dot11GASAdvertisementEntry field definitions; see 802.11-2016 Annex C.3.
    UINT AdvertisementID;
    BOOL PauseForServerResponse;
    UINT ResponseTimeout;
    UINT ComeBackDelay;
    UINT ResponseBufferingTime;
    UINT QueryResponseLengthLimit;
} COSA_DML_WIFI_GASCFG, *PCOSA_DML_WIFI_GASCFG;

typedef struct _COSA_DML_WIFI_GASTATS {    // Values correspond to the dot11GASAdvertisementEntry field definitions; see 802.11-2016 Annex C.3.
    UINT AdvertisementID;
    UINT Queries;
    UINT QueryRate;
    UINT Responses;
    UINT ResponseRate;
    UINT NoRequestOutstanding;
    UINT ResponsesDiscarded;
    UINT FailedResponses;
} COSA_DML_WIFI_GASSTATS,*PCOSA_DML_WIFI_GASSTATS;

/*
 *  Structure definitions for 802.11u configurations
 */
struct
_COSA_DML_WIFI_ROAMING_CNSTR_CFG
{
    UCHAR       iWIFIRoamingConsortiumCount;
    UCHAR       iWIFIRoamingConsortiumOui[3][15+1];//only 3 OIS is allowed in beacon and probe responses OIS length is variable between 3-15
    UCHAR       iWIFIRoamingConsortiumLen[3];
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_ROAMING_CNSTR_CFG COSA_DML_WIFI_ROAMING_CNSTR_CFG,  *PCOSA_DML_WIFI_ROAMING_CNSTR_CFG;

struct
_COSA_DML_WIFI_ADVERTISEMENT_CFG
{
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_ADVERTISEMENT_CFG COSA_DML_WIFI_ADVERTISEMENT_CFG,  *PCOSA_DML_WIFI_ADVERTISEMENT_CFG;

typedef  enum
_COSA_DML_WIFI_ACCESS_NETWORK_TYPE
{
       COSA_DML_WIFI_ACCESS_NETWORK_TYPE_PRIVATE,
       COSA_DML_WIFI_ACCESS_NETWORK_TYPE_PRIVATE_WITH_GUEST_ACCESS,
       COSA_DML_WIFI_ACCESS_NETWORK_TYPE_PUBLIC_CHARGABLE,
       COSA_DML_WIFI_ACCESS_NETWORK_TYPE_PUBLIC_FREE,
       COSA_DML_WIFI_ACCESS_NETWORK_TYPE_PERSONAL,
       COSA_DML_WIFI_ACCESS_NETWORK_TYPE_EMERGENCY_SERVICES,
       COSA_DML_WIFI_ACCESS_NETWORK_TYPE_EXPERIMENTAL = 14,
   COSA_DML_WIFI_ACCESS_NETWORK_TYPE_WILDCARD  
}
COSA_DML_WIFI_ACCESS_NETWORK_TYPE, *PCOSA_DML_WIFI_ACCESS_NETWORK_TYPE;

struct
_COSA_DML_WIFI_INTERWORKING_CFG
{
   //Interworking Element structure; see 802.11-2016 section 9.4.2.92 for field definition.
    COSA_DML_WIFI_ACCESS_NETWORK_TYPE      iAccessNetworkType;
    INT       	iInternetAvailable;
    INT       	iASRA;
    INT       	iESR;
    INT       	iUESA;
    INT       	iVenueOptionPresent;    // True when venue information has not been provided, e.g. the hostspot is in a residence.
    INT      	iVenueGroup;    
    INT      	iVenueType;    
	BOOL	   	iHESSOptionPresent;
    char       	iHESSID[18];    // Optional; use empty string to indicate no value provided.
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_INTERWORKING_CFG COSA_DML_WIFI_INTERWORKING_CFG,  *PCOSA_DML_WIFI_INTERWORKING_CFG;

//Passpoint and ANQP configuration parameters
struct
_COSA_DML_WIFI_PASSPOINT_CFG
{
    BOOL            Capability;
    BOOL            Status;
    BOOL            gafDisable;
    BOOL            p2pDisable;
    BOOL            l2tif;
    char            *ANQPConfigParameters;
    char            *HS2Parameters;
    char            WANMetrics[256];
    char            Stats[1024];
}_struct_pack_;

typedef struct _COSA_DML_WIFI_PASSPOINT_CFG COSA_DML_WIFI_PASSPOINT_CFG, *PCOSA_DML_WIFI_PASSPOINT_CFG;

struct
_COSA_DML_WIFI_80211U_CFG
{
   COSA_DML_WIFI_INTERWORKING_CFG  IntwrkCfg;
   COSA_DML_WIFI_ADVERTISEMENT_CFG AdvCfg;
   COSA_DML_WIFI_ROAMING_CNSTR_CFG RoamCfg;
   COSA_DML_WIFI_PASSPOINT_CFG     PasspointCfg;
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_80211U_CFG COSA_DML_WIFI_80211U_CFG,  *PCOSA_DML_WIFI_80211U_CFG;

/*
 *  Structure definitions for WiFi AP
 *  WiFi AP is always associated with a SSID in the system, thus,
 *  it is identified by the SSID.
 *
 *  Middle layer handles orphan AP (not associated with a SSID) internally.
 *  Those APs are not passed down to backend system.
 */
struct
_COSA_DML_WIFI_AP_CFG
{
    ULONG                       InstanceNumber;
    char                            Alias[COSA_DML_ALIAS_NAME_LENGTH];
    char                            SSID[COSA_DML_WIFI_MAX_SSID_NAME_LEN];           /* Reference to SSID name */

    BOOLEAN                   bEnabled;
    BOOLEAN                   SSIDAdvertisementEnabled;
    ULONG                       RetryLimit;
    BOOLEAN                   WMMEnable;
    BOOLEAN                   UAPSDEnable;
    BOOLEAN                   IsolationEnable;

    /* USGv2 Extensions */ 
    int                             WmmNoAck;
    int                             MulticastRate;
    int                             BssMaxNumSta;
    BOOL                        BssCountStaAsCpe;
    BOOL                        BssHotSpot;
    ULONG                      LongRetryLimit;
    ULONG                      MaxAssociatedDevices;
    ULONG                      HighWatermarkThreshold;
    ULONG                      HighWatermarkThresholdReached;
    ULONG                      HighWatermark;
    BOOLEAN                  KickAssocDevices;
    BOOLEAN                  InterworkingCapability;
    BOOLEAN                  InterworkingEnable;
    BOOLEAN	             WirelessManagementImplemented;
    BOOLEAN		     BSSTransitionImplemented;
    BOOLEAN 		     BSSTransitionActivated;
    COSA_DML_WIFI_80211U_CFG	IEEE80211uCfg;
    char 		     MacFilterMode[12];
	char			 BeaconRate[32];
	    int			      ManagementFramePowerControl;
	    BOOLEAN			      X_RDKCENTRAL_COM_rapidReconnectCountEnable;				
	    int			      X_RDKCENTRAL_COM_rapidReconnectMaxTime;
    BOOLEAN                  X_RDKCENTRAL_COM_StatsEnable;
    BOOLEAN                  X_RDKCENTRAL_COM_NeighborReportActivated;
    ULONG                    TXOverflow;
}_struct_pack_;

typedef struct _COSA_DML_WIFI_AP_CFG COSA_DML_WIFI_AP_CFG,  *PCOSA_DML_WIFI_AP_CFG;

struct
_COSA_DML_WIFI_AP_INFO
{
    COSA_DML_WIFI_AP_STATUS         Status;
    BOOLEAN                         WMMCapability;
    BOOLEAN                         UAPSDCapability;
    /* USGv2 Extensions */ 
    int                             BssUserStatus;
}_struct_pack_;

typedef struct _COSA_DML_WIFI_AP_INFO COSA_DML_WIFI_AP_INFO,  *PCOSA_DML_WIFI_AP_INFO;

/*
 *  Structure definitions for WiFi Radius Settings
 */
struct
_COSA_DML_WIFI_RadiusSetting
{
    BOOL                            bPMKCaching;
    int                             iRadiusServerRetries;
    int                             iRadiusServerRequestTimeout;
    int                             iPMKLifetime;
    int                             iPMKCacheInterval;
    int                             iMaxAuthenticationAttempts;
    int                             iBlacklistTableTimeout;
    int                             iIdentityRequestRetryInterval;
    int                             iQuietPeriodAfterFailedAuthentication;
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_RadiusSetting COSA_DML_WIFI_RadiusSetting,  *PCOSA_DML_WIFI_RadiusSetting;



struct
_COSA_DML_WIFI_AP_FULL
{
    COSA_DML_WIFI_AP_CFG            Cfg;
    COSA_DML_WIFI_AP_INFO           Info;
    COSA_DML_WIFI_RadiusSetting     RadiusSetting; //zqiu: move from COSA_DML_WIFI_AP
    /* USGv2 Extensions */ 
    QUEUE_HEADER                    MacFilterList;
    ULONG                           ulMacFilterNextInsNum;
    ANSC_HANDLE                     hIrepFolderMacFilt;
    ANSC_HANDLE                     hPoamMacFlitDm;
#ifdef WIFI_HAL_VERSION_3
    BOOLEAN                         isApChanged;
#endif
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_AP_FULL COSA_DML_WIFI_AP_FULL, *PCOSA_DML_WIFI_AP_FULL;


/*
 *  Structure definitions for WiFi AP Security
 */
struct
_COSA_DML_WIFI_APSEC_CFG
{
    COSA_DML_WIFI_SECURITY          ModeEnabled;
    UCHAR                           WEPKeyp[13];
    UCHAR                           PreSharedKey[64+1];
    UCHAR                           DefaultKeyPassphrase[64+1];
    UCHAR                           KeyPassphrase[64+1];
    ULONG                           RekeyingInterval;
    COSA_DML_WIFI_AP_SEC_ENCRYPTION EncryptionMethod;
    UCHAR              		    	RadiusServerIPAddr[45];
    ULONG                           RadiusServerPort;
    char                            RadiusSecret[64];
    UCHAR              		    	SecondaryRadiusServerIPAddr[45];
    ULONG                           SecondaryRadiusServerPort;
    char                            SecondaryRadiusSecret[64];
    UCHAR                           RadiusDASIPAddr[45];
    ULONG                           RadiusDASPort;
    char 			    RadiusDASSecret[64];
    char                            MFPConfig[32];
#ifdef WIFI_HAL_VERSION_3
    char                            SAEPassphrase[SAE_PASSPHRASE_MAX_LENGTH];
    BOOL                            WPA3TransitionDisable;
#endif
    /* USGv2 Extensions */
    int                             RadiusReAuthInterval;
    int                             DefaultKey;
    BOOL 			    bReset;
    unsigned int                    uiEAPOLKeyTimeout;
    unsigned int                    uiEAPOLKeyRetries;
    unsigned int                    uiEAPIdentityRequestTimeout;
    unsigned int                    uiEAPIdentityRequestRetries;
    unsigned int                    uiEAPRequestTimeout;
    unsigned int                    uiEAPRequestRetries;
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_APSEC_CFG COSA_DML_WIFI_APSEC_CFG,  *PCOSA_DML_WIFI_APSEC_CFG;

struct
_COSA_DML_WIFI_APSEC_INFO
{
    ULONG                           ModesSupported;     /* Bitmask of COSA_DML_WIFI_SECURITY */
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_APSEC_INFO COSA_DML_WIFI_APSEC_INFO,  *PCOSA_DML_WIFI_APSEC_INFO;

typedef struct
_COSA_DML_WEPKEY_64BIT
{
    char                            WEPKey[5 * 2 + 1];
}
COSA_DML_WEPKEY_64BIT, *PCOSA_DML_WEPKEY_64BIT;

typedef struct
_COSA_DML_WEPKEY_128BIT
{
    char                            WEPKey[13 * 2 + 1];
}
COSA_DML_WEPKEY_128BIT, *PCOSA_DML_WEPKEY_128BIT;

struct
_COSA_DML_WIFI_APSEC_FULL
{
    COSA_DML_WIFI_APSEC_CFG         Cfg;
    COSA_DML_WIFI_APSEC_INFO        Info;

    /* USGv2 Extensions */
    COSA_DML_WEPKEY_64BIT           WEPKey64Bit[COSA_DML_WEP_KEY_NUM];
    COSA_DML_WEPKEY_128BIT          WEPKey128Bit[COSA_DML_WEP_KEY_NUM];
#ifdef WIFI_HAL_VERSION_3
    BOOLEAN                         isSecChanged;
#endif
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_APSEC_FULL COSA_DML_WIFI_APSEC_FULL, *PCOSA_DML_WIFI_APSEC_FULL;


/*
 *  Structure definitions for WiFi AP WPS
 */
struct
_COSA_DML_WIFI_APWPS_CFG
{
    BOOLEAN                         bEnabled;
    ULONG                           ConfigMethodsEnabled;   /* Bitmask of COSA_DML_WIFI_WPS_METHOD */
    /* USGv2 Extensions */
    int                             WpsPushButton;
    BOOLEAN                         X_CISCO_COM_ActivatePushButton;
    char                            X_CISCO_COM_ClientPin[64];
    BOOLEAN                         X_CISCO_COM_CancelSession;
}_struct_pack_;

typedef struct _COSA_DML_WIFI_APWPS_CFG COSA_DML_WIFI_APWPS_CFG,  *PCOSA_DML_WIFI_APWPS_CFG;


struct
_COSA_DML_WIFI_APWPS_INFO
{
    ULONG                           ConfigMethodsSupported; /* Bitmask of COSA_DML_WIFI_WPS_METHOD */
    char                            X_CISCO_COM_Pin[64];
    BOOL                            X_Comcast_com_Configured;
}_struct_pack_;

typedef struct _COSA_DML_WIFI_APWPS_INFO COSA_DML_WIFI_APWPS_INFO,  *PCOSA_DML_WIFI_APWPS_INFO;

struct
_COSA_DML_WIFI_APWPS_FULL
{
    COSA_DML_WIFI_APWPS_CFG         Cfg;
    COSA_DML_WIFI_APWPS_INFO        Info;
}_struct_pack_;

typedef struct _COSA_DML_WIFI_APWPS_FULL COSA_DML_WIFI_APWPS_FULL, *PCOSA_DML_WIFI_APWPS_FULL;


/*
 *  Structure definitions for WiFi AP Associated Device
 */
struct
_COSA_DML_WIFI_AP_ASSOC_DEVICE
{
    UCHAR                           MacAddress[6];
    BOOLEAN                         AuthenticationState;
    ULONG                           LastDataDownlinkRate;
    ULONG                           LastDataUplinkRate;
    int                             SignalStrength;
    ULONG                           Retransmissions;
    BOOLEAN                         Active;
    char			    OperatingStandard[64];
    char			    OperatingChannelBandwidth[64];
    int 			    SNR;
    char 			    InterferenceSources[64];
    ULONG 			    DataFramesSentAck;
    ULONG			    DataFramesSentNoAck;
    ULONG 			    BytesSent;
    ULONG			    BytesReceived;
    ULONG                           PacketsSent;
    ULONG                           PacketsReceived;
    ULONG                           ErrorsSent;
    ULONG                           RetransCount;
    ULONG                           FailedRetransCount;
    ULONG                           RetryCount;
    ULONG                           MultipleRetryCount;

    int 			    RSSI;
    int 			    MinRSSI;
    int 			    MaxRSSI;
    unsigned int		    Disassociations;
    unsigned int		    AuthenticationFailures;
    unsigned int		    maxUplinkRate;
    unsigned int		    maxDownlinkRate;	
}_struct_pack_;

typedef struct _COSA_DML_WIFI_AP_ASSOC_DEVICE COSA_DML_WIFI_AP_ASSOC_DEVICE,  *PCOSA_DML_WIFI_AP_ASSOC_DEVICE;

/*
 * Structure definitions for WiFi Device Provisioning Protocol
 */
#ifdef WIFI_HAL_VERSION_3
#define COSA_DML_WIFI_DPP_STA_MAX                                   MAX_VAP
#else
#define COSA_DML_WIFI_DPP_STA_MAX                                   16
#endif
#if !defined(_HUB4_PRODUCT_REQ_) && !defined(_XB7_PRODUCT_REQ_)
typedef  enum
_COSA_DML_WIFI_DPP_ENROLEE_RESP_STATUS
{
    STATUS_OK                      = 0, 
    STATUS_NOT_COMPATIBLE, 
    STATUS_AUTH_FAILURE, 
    STATUS_DECRYPT_FAILURE, 
    STATUS_CONFIGURE_FAILURE, 
    STATUS_RESPONSE_PENDING, 
    STATUS_INVALID_CONNECTOR        
}
COSA_DML_WIFI_DPP_ENROLEE_RESP_STATUS, *PCOSA_DML_WIFI_DPP_ENROLEE_RESP_STATUS;

#endif // !defined(_HUB4_PRODUCT_REQ_) && !defined(_XB7_PRODUCT_REQ_)

typedef struct 
_COSA_DML_WIFI_DPP_STA_CRED
{
    CHAR            KeyManagement[32];
    UCHAR           psk_hex[64];
    CHAR            password[64];
}
COSA_DML_WIFI_DPP_STA_CRED, *PCOSA_DML_WIFI_DPP_STA_CRED;
struct
_COSA_DML_WIFI_DPP_STA_CFG
{
    unsigned int    MaxRetryCount;
    BOOL            Activate;
    COSA_DML_WIFI_DPP_STA_CRED        Cred;
    CHAR            ClientMac[18];
    UCHAR           ActivationStatus[32];
    UCHAR           EnrolleeResponderStatus[64];
	UINT			NumChannels;
    UINT            Channels[32];
    CHAR            InitiatorBootstrapSubjectPublicKeyInfo[256];
    CHAR            ResponderBootstrapSubjectPublicKeyInfo[256];
}_struct_pack_;

typedef struct _COSA_DML_WIFI_DPP_STA_CFG COSA_DML_WIFI_DPP_STA_CFG, *PCOSA_DML_WIFI_DPP_STA_CFG;

struct
_COSA_DML_WIFI_DPP_RECFG
{
    CHAR            PrivateSigningKey[512];
    CHAR            PrivateReconfigAccessKey[512];
}_struct_pack_;

typedef struct _COSA_DML_WIFI_DPP_RECFG COSA_DML_WIFI_DPP_RECFG, *PCOSA_DML_WIFI_DPP_RECFG;
      
struct
_COSA_DML_WIFI_DPP_CFG
{
    COSA_DML_WIFI_DPP_STA_CFG   Cfg[COSA_DML_WIFI_DPP_STA_MAX];
    UCHAR                       Version;
    COSA_DML_WIFI_DPP_RECFG     Recfg;
}_struct_pack_;

typedef struct _COSA_DML_WIFI_DPP_CFG COSA_DML_WIFI_DPP_CFG, *PCOSA_DML_WIFI_DPP_CFG;

/*
 * Structure definitions for WiFi AP MAC filter
 */

struct
_COSA_DML_WIFI_AP_MF_CFG
{
    BOOLEAN                         bEnabled;
    BOOLEAN                         FilterAsBlackList;
    ULONG                           NumberMacAddrList;
    UCHAR                           MacAddrList[6*COSA_DML_WIFI_MAX_MAC_FILTER_NUM];
}_struct_pack_;

typedef struct _COSA_DML_WIFI_AP_MF_CFG COSA_DML_WIFI_AP_MF_CFG, *PCOSA_DML_WIFI_AP_MF_CFG;

typedef struct 
_COSA_DML_WIFI_AP_MAC_FILTER
{
    ULONG                           InstanceNumber;
    char                            Alias[COSA_DML_ALIAS_NAME_LENGTH];

    char                            MACAddress[18];
    char                            DeviceName[64];
}
COSA_DML_WIFI_AP_MAC_FILTER, *PCOSA_DML_WIFI_AP_MAC_FILTER;




//>> ATM
struct
_COSA_DML_WIFI_ATM_APGRP_STA
{
    char                            MACAddress[18];
    ULONG                           AirTimePercent;
	char							*pAPList;
}_struct_pack_;
typedef struct _COSA_DML_WIFI_ATM_APGRP_STA COSA_DML_WIFI_ATM_APGROUP_STA, *PCOSA_DML_WIFI_ATM_APGROUP_STA;

struct
_COSA_DML_WIFI_ATM_APGROUP
{
    char                            APList[COSA_DML_WIFI_ATM_MAX_APLIST_STR_LEN];
    ULONG                           AirTimePercent;
	ULONG							NumberSta;
	COSA_DML_WIFI_ATM_APGROUP_STA     StaList[COSA_DML_WIFI_ATM_MAX_STA_NUM];	
}_struct_pack_;
typedef struct _COSA_DML_WIFI_ATM_APGROUP COSA_DML_WIFI_ATM_APGROUP, *PCOSA_DML_WIFI_ATM_APGROUP;

#define ATM_GROUP_COUNT 6
typedef  struct
_COSA_DML_WIFI_ATM {
	BOOLEAN							Capable;
	BOOLEAN							Enable;		
	UINT							grpCount;
	COSA_DML_WIFI_ATM_APGROUP		APGroup[ATM_GROUP_COUNT];	
}
COSA_DML_WIFI_ATM, *PCOSA_DML_WIFI_ATM;

#define MAC_LENGTH      13 

typedef struct
_COSA_DML_WIFI_HARVESTER
{
    BOOLEAN                         bINSTClientEnabled;
    BOOLEAN                         bINSTClientEnabledChanged;
    ULONG                           uINSTClientReportingPeriod;
    BOOLEAN                         bINSTClientReportingPeriodChanged;
    ULONG                           uINSTClientDefReportingPeriod;
    BOOLEAN                         bINSTClientDefReportingPeriodChanged;
    ULONG                           uINSTClientDefOverrideTTL;
    BOOLEAN                         bINSTClientDefOverrideTTLChanged;
    CHAR                            MacAddress[MAC_LENGTH];
    BOOLEAN                         bINSTClientMacAddressChanged;
    BOOLEAN                         bActiveMsmtEnabled;
    BOOLEAN                         bActiveMsmtEnabledChanged;
    ULONG                           uActiveMsmtSampleDuration;
    ULONG                           uActiveMsmtOldSampleDuration;
    BOOLEAN                         bActiveMsmtSampleDurationChanged;
    ULONG                           uActiveMsmtPktSize;
    ULONG                           uActiveMsmtOldPktSize;
    BOOLEAN                         bActiveMsmtPktSizeChanged;
    ULONG                           uActiveMsmtNumberOfSamples;
    ULONG                           uActiveMsmtOldNumberOfSamples;
    BOOLEAN                         bActiveMsmtNumberOfSamplesChanged;
    UCHAR                           ActiveMsmtPlanID[PLAN_ID_LEN];
    BOOLEAN                         bActiveMsmtPlanIDChanged;
    COSA_DML_WIFI_ACTIVE_MSMT_STEP_FULL  Step;
#if 0
    CHAR                            SchemaID[256];
    CHAR                            Schema[256];
    ULONG                           uINSTClientMaxReports;
    BOOLEAN                         bINSTClientMaxReportsChanged;
    BOOLEAN                         bIDWEnabled;
    BOOLEAN                         bIDWEnabledChanged;
    ULONG                           uIDWPollingPeriod;
    BOOLEAN                         bIDWPollingPeriodChanged;
    ULONG                           uIDWReportingPeriod;
    BOOLEAN                         bIDWReportingPeriodChanged;
    ULONG                           uIDWPollingPeriodDefault;
    ULONG                           uIDWReportingPeriodDefault;
    ULONG                           uIDWOverrideTTL;
    BOOLEAN                         bIDWDefaultPollingPeriodChanged;
    BOOLEAN                         bIDWDefaultReportingPeriodChanged;
    ULONG                           uIDWDefaultPollingPeriod;
    ULONG                           uIDWDefaultReportingPeriod;
    BOOLEAN                         bNAPEnabled;
    BOOLEAN                         bNAPEnabledChanged;
    ULONG                           uNAPPollingPeriod;
    BOOLEAN                         bNAPPollingPeriodChanged;
    ULONG                           uNAPReportingPeriod;
    BOOLEAN                         bNAPReportingPeriodChanged;
    ULONG                           uNAPPollingPeriodDefault;
    ULONG                           uNAPReportingPeriodDefault;
    ULONG                           uNAPOverrideTTL;
    BOOLEAN                         bNAPDefaultPollingPeriodChanged;
    BOOLEAN                         bNAPDefaultReportingPeriodChanged;
    ULONG                           uNAPDefaultPollingPeriod;
    ULONG                           uNAPDefaultReportingPeriod;
    BOOLEAN                         bNAPOnDemandEnabled;
    BOOLEAN                         bNAPOnDemandEnabledChanged;
    BOOLEAN                         bRISEnabled;
    BOOLEAN                         bRISEnabled;
    BOOLEAN                         bRISEnabledChanged;
    ULONG                           uRISPollingPeriod;
    BOOLEAN                         bRISPollingPeriodChanged;
    ULONG                           uRISReportingPeriod;
    BOOLEAN                         bRISReportingPeriodChanged;
    ULONG                           uRISPollingPeriodDefault;
    ULONG                           uRISReportingPeriodDefault;
    ULONG                           uRISOverrideTTL;
    BOOLEAN                         bRISDefaultPollingPeriodChanged;
    BOOLEAN                         bRISDefaultReportingPeriodChanged;
    ULONG                           uRISDefaultPollingPeriod;
    ULONG                           uRISDefaultReportingPeriod;
#endif

}
COSA_DML_WIFI_HARVESTER, *PCOSA_DML_WIFI_HARVESTER;

ANSC_STATUS CosaDmlWiFi_GetATMOptions(PCOSA_DML_WIFI_ATM  pATM);
ANSC_STATUS CosaWifiRegGetATMInfo( ANSC_HANDLE   hThisObject);

ANSC_STATUS CosaDmlWiFi_SetATMEnable(PCOSA_DML_WIFI_ATM pATM, BOOL bValue);
ANSC_STATUS CosaDmlWiFi_SetATMAirTimePercent(char *APList, UINT AirTimePercent);
ANSC_STATUS CosaDmlWiFi_SetATMSta(char *APList, char *MACAddress, UINT AirTimePercent);
//<<

/*
 *  Structure definitions for WiFi BandSteering Settings
 */
struct
_COSA_DML_WIFI_BANDSTEERING_OPTION
{
    BOOLEAN                          bEnable;
    BOOLEAN                          bCapability;
    CHAR                             APGroup[ COSA_DML_WIFI_MAX_BAND_STEERING_APGROUP_STR_LEN ];
    CHAR							 BandHistory[ COSA_DML_WIFI_MAX_BAND_STEERING_HISTORY_NUM ];	
    BOOL                             bLastBSDisableForRadio;
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_BANDSTEERING_OPTION COSA_DML_WIFI_BANDSTEERING_OPTION, *PCOSA_DML_WIFI_BANDSTEERING_OPTION;

struct
_COSA_DML_WIFI_BANDSTEERING_SETTINGS
{
    int                          	InstanceNumber;
    int                          	UtilizationThreshold;
    int     	                 	RSSIThreshold;
	int 						 	PhyRateThreshold;
    int                                 OverloadInactiveTime;
    int                                 IdleInactiveTime;
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_BANDSTEERING_SETTINGS COSA_DML_WIFI_BANDSTEERING_SETTINGS, *PCOSA_DML_WIFI_BANDSTEERING_SETTINGS;

#if defined(FEATURE_HOSTAP_AUTHENTICATOR)
typedef enum
_COSA_WIFI_LIB_HOSTAPD_TR181_PARAM
{
    COSA_WIFI_HAPD_RADIUS_SERVER_IP = 0,
    COSA_WIFI_HAPD_RADIUS_SERVER_SECRET,
    COSA_WIFI_HAPD_RADIUS_SERVER_PORT,
    COSA_WIFI_HAPD_RADIUS_SERVER_PMK_CACHING,
    COSA_WIFI_HAPD_WPS_STATE,
    COSA_WIFI_HAPD_WPS_CONFIG_METHODS,
    COSA_WIFI_HAPD_WPS_PUSH_BUTTON
}COSA_WIFI_LIB_HOSTAPD_TR181_PARAM;
#endif //(FEATURE_HOSTAP_AUTHENTICATOR)

#if defined (_HUB4_PRODUCT_REQ_)
struct
_COSA_WIFI_LMHOST_CFG
{
    BOOLEAN         bActive;
    CHAR            acMACAddress[64];
    CHAR            acLowerLayerInterface1[256];
    INT             iVAPIndex;
}_struct_pack_;

typedef  struct _COSA_WIFI_LMHOST_CFG COSA_WIFI_LMHOST_CFG, *PCOSA_WIFI_LMHOST_CFG;

struct
_COSA_WIFI_CLIENT_CFG
{
    CHAR            acMACAddress[64];
    INT             iVAPIndex;
}_struct_pack_;

typedef  struct _COSA_WIFI_CLIENT_CFG COSA_WIFI_CLIENT_CFG, *PCOSA_WIFI_CLIENT_CFG;
#endif /* * _HUB4_PRODUCT_REQ_ */
/**********************************************************************
                FUNCTION PROTOTYPES
**********************************************************************/


ANSC_STATUS
CosaDmlWiFiInit
    (
        ANSC_HANDLE                 hDml,
        PANSC_HANDLE                phContext
    );
    
ANSC_STATUS
CosaDmlWiFi_FactoryReset
    (
       void
    );

ANSC_STATUS
CosaDmlWiFi_GetInterworkingInternetAvailable
    (
       BOOL *value
    );

ANSC_STATUS
CosaDmlWiFi_GetPreferPrivateData
    (
       BOOL *value
    );

ANSC_STATUS
CosaDmlWiFi_GetPreferPrivatePsmData
    (
       BOOL *value
    );

ANSC_STATUS
CosaDmlWiFi_SetPreferPrivatePsmData
    (
        BOOL value
    );

void
CosaDmlWiFiGetEnableRadiusGreylist
    (
	BOOL *value
    );
ANSC_STATUS
CosaDmlWiFiSetEnableRadiusGreylist
   (
	BOOL value
   );

void CosaDmlWiFi_UpdateMfCfg(void);


ANSC_STATUS
CosaDmlWiFiRegionInit
  (
	PCOSA_DATAMODEL_RDKB_WIFIREGION PWiFiRegion
  );

ANSC_STATUS SetWiFiRegionCode(char *code);

ANSC_STATUS
CosaDmlWiFiGetvAPStatsFeatureEnable
    (
        BOOLEAN     *pbValue
    );

ANSC_STATUS
CosaDmlWiFiSetvAPStatsFeatureEnable
    (
        BOOLEAN     bValue
    );

BOOLEAN
IsCosaDmlWiFivAPStatsFeatureEnabled
    (
        void
    );

ANSC_STATUS
CosaDmlWiFiGetTxOverflowSelfheal
    (
        BOOLEAN     *pbValue
    );

ANSC_STATUS
CosaDmlWiFiSetTxOverflowSelfheal
    (
        BOOLEAN     bValue
    );

ANSC_STATUS CosaDmlWiFiDeAllocBridgeVlan(void);
ANSC_STATUS CosaDmlWiFiSetForceDisableWiFiRadio(BOOLEAN bValue);
ANSC_STATUS CosaDmlWiFiGetForceDisableWiFiRadio(BOOLEAN *pbValue);
ANSC_STATUS CosaDmlWiFiGetCurrForceDisableWiFiRadio(BOOLEAN *pbValue);
ANSC_STATUS CosaDmlWiFiSetDFSAtBootUp(BOOLEAN bValue);
ANSC_STATUS CosaDmlWiFiGetDFSAtBootUp(BOOLEAN *pbValue);
ANSC_STATUS CosaDmlWiFiSetDFS(BOOLEAN bValue);
ANSC_STATUS CosaDmlWiFiGetDFS(BOOLEAN *pbValue);



#ifdef WIFI_HAL_VERSION_3
ANSC_STATUS CosaDmlWiFi_FactoryResetRadioAndAp(CHAR * radioApIndexes);

ANSC_STATUS
CosaDmlWiFi_ParseRadioAPIndexes(CHAR *radioApIndexes, UINT* radioIndexList, UINT* apIndexList, UINT maxListSize, UINT* listSize);
void CosaWiFiDmlGetWPA3TransitionRFC (BOOL *WPA3_RFC);
#else
ANSC_STATUS CosaDmlWiFi_FactoryResetRadioAndAp(ULONG radioIndex, ULONG radioIndex_2, ULONG apIndex, ULONG apIndex_2);
#endif

ANSC_STATUS CosaDmlWiFiFactoryResetRadioAndAp (ULONG radioIndex, ULONG apIndex, BOOL needRestart);
ANSC_STATUS CosaDmlWiFiGetBridge0PsmData(char *ip, char *sub);

ANSC_STATUS
CosaDmlWiFi_ResetRadios
    (
       void
    );
    
ANSC_STATUS
CosaDmlWiFi_EnableTelnet
    (
	BOOL			    bEnabled
    );

/*
 *  Description:
 *     The API retrieves the number of WiFi radios in the system.
 */
ULONG
CosaDmlWiFiRadioGetNumberOfEntries
    (
        ANSC_HANDLE                 hContext
    );
    
ANSC_STATUS
CosaDmlWiFi_GetRadioPower
    (
        COSA_DML_WIFI_RADIO_POWER *power
    );
    
ANSC_STATUS
CosaDmlWiFi_SetRadioPower
    (
        COSA_DML_WIFI_RADIO_POWER power
    );
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
    );

ANSC_STATUS
CosaDmlWiFiRadioSetValues
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIndex,
        ULONG                       ulInstanceNumber,
        char*                       pAlias
    );

ANSC_STATUS
CosaDmlWiFiRadioSetDefaultCfgValues
    (
        ANSC_HANDLE                 hContext,
        unsigned long               ulIndex,
        PCOSA_DML_WIFI_RADIO_CFG    pCfg
    );
    
ANSC_STATUS
CosaDmlWiFiRadioSetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_RADIO_CFG    pCfg          /* Identified by InstanceNumber */
    );

ANSC_STATUS
CosaDmlWiFiRadioGetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_RADIO_CFG    pCfg          /* Identified by InstanceNumber */
    );

ANSC_STATUS
CosaDmlWiFiRadioStatsSet
	(
		int     InstanceNumber,
		PCOSA_DML_WIFI_RADIO_STATS    pWifiRadioStats        
	);

ANSC_STATUS
CosaDmlWiFiRadioGetDCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_RADIO_CFG    pCfg        /* Identified by InstanceNumber */
    );

ANSC_STATUS
CosaDmlWiFiRadioGetDinfo
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber,
        PCOSA_DML_WIFI_RADIO_DINFO  pInfo
    );

ANSC_STATUS
CosaDmlWiFiRadioChannelGetStats
    (
        char*                  ParamName,
        ULONG                  ulInstanceNumber,
        PCOSA_DML_WIFI_RADIO_CHANNEL_STATS  pStats,
        UINT                   *percentage
    );

ANSC_STATUS
CosaDmlWiFiRadioGetStats
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber,
        PCOSA_DML_WIFI_RADIO_STATS  pStats
    );

ANSC_STATUS
CosaDmlWiFiRadioGetCalc
    (
        ANSC_HANDLE                 hContext, 
        PCOSA_DML_WIFI_RADIO_CALC_RESULT    pCalc        /* Identified by InstanceNumber */ 
    );

#if defined(DMCLI_SUPPORT_TO_ADD_DELETE_VAP)
ANSC_STATUS
CosaDmlWiFiGetNumberOfAPsOnRadio
    (
        UINT                      radioIndex,
        UINT                      *output_count
    );
#endif


#if defined (FEATURE_CSI)
ANSC_STATUS
CosaDmlWiFiCSISetClientMaclist
    (
        ULONG                       ulIndex,
        CHAR*                       ClientMaclist
    );

ANSC_STATUS
CosaDmlWiFiCSISetEnable
    (
        ULONG                       ulIndex,
        BOOL                        Enable
    );

ANSC_STATUS
CosaDmlWiFiCSIAddEntry
    (
        PCOSA_DML_WIFI_CSI          pEntry
    );

ANSC_STATUS
CosaDmlWiFiCSIDelEntry
    (
        ULONG                       ulIndex
    );
#endif

/* WiFi SSID */
/* Description:
 *	The API retrieves the number of WiFi SSIDs in the system.
 */
ULONG
CosaDmlWiFiSsidGetNumberOfEntries
    (
        ANSC_HANDLE                 hContext
    );

    
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
    );

ANSC_STATUS
CosaDmlWiFiSsidSetValues
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIndex,
        ULONG                       ulInstanceNumber,
        char*                       pAlias
    );    
    
/* Description:
 *	The API adds a new WiFi SSID into the system. 
 * Arguments:
 *	ulIndex		Indicates the index number of the entry.
 *	pEntry		Caller pass in the configuration through pEntry->Cfg field and gets back the generated pEntry->StaticInfo.Name, MACAddress, etc.
 */
ANSC_STATUS
CosaDmlWiFiSsidAddEntry
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_SSID_FULL    pEntry
    );


ANSC_STATUS
CosaDmlWiFiSsidDelEntry
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber
    );

ANSC_STATUS
CosaDmlWiFiSsidSetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_SSID_CFG     pCfg         /* Identified by InstanceNumber */
    );

ANSC_STATUS
CosaDmlWiFiApGetRetryLimit
    (
        char* pSsid,
        ULONG* pRetryLimit
    );

ANSC_STATUS
CosaDmlWiFiSsidApplyCfg
    (
        PCOSA_DML_WIFI_SSID_CFG     pCfg         /* Identified by InstanceNumber */
    );

ANSC_STATUS
CosaDmlWiFiSsidGetCfg
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_WIFI_SSID_CFG     pCfg         /* Identified by InstanceNumber */
    );

ANSC_STATUS
CosaDmlWiFiSsidGetDinfo
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber,
        PCOSA_DML_WIFI_SSID_DINFO   pInfo
    );

ANSC_STATUS
CosaDmlWiFiSsidFillSinfoCache
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber
    );

ANSC_STATUS
CosaDmlWiFiSsidGetSinfo
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber,
        PCOSA_DML_WIFI_SSID_SINFO   pInfo
    );

ANSC_STATUS
CosaDmlWiFiSsidGetStats
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber,
        PCOSA_DML_WIFI_SSID_STATS   pStats
    );

BOOL 
CosaDmlWiFiSsidValidateSSID 
     (
         void
     );

ANSC_STATUS
CosaDmlWiFiSsidGetSSID
    (
        ULONG                       ulInstanceNumber,
        char                            *ssid
    );

/* WiFi AP is always associated with a SSID in the system */
/* Description:
 *	The API retrieves the number of WiFi APs in the system.
 */
ULONG
CosaDmlWiFiAPGetNumberOfEntries
    (
        ANSC_HANDLE                 hContext
    );
    
ANSC_STATUS
CosaDmlWiFiApGetEntry
    (
        ANSC_HANDLE                 hContext,
        char                        *pSsid,
        PCOSA_DML_WIFI_AP_FULL      pEntry
    );

ANSC_STATUS
CosaDmlWiFiApSetValues
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulIndex,
        ULONG                       ulInstanceNumber,
        char*                       pAlias
    );
    
ANSC_STATUS
CosaDmlWiFiApPushMacFilter
    (
        QUEUE_HEADER       *pMfQueue,
        ULONG                      wlanIndex
    );

BOOL
CosaDmlWiFiApIsSecmodeOpenForPrivateAP
    (
         void
    );

ANSC_STATUS
#if defined (MULTILAN_FEATURE)
CosaDmlWiFiApAddEntry
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_AP_FULL      pEntry
    );
#if defined(DMCLI_SUPPORT_TO_ADD_DELETE_VAP)
ANSC_STATUS
CosaDmlWiFiApDelEntry
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber
    );
#endif
ANSC_STATUS
#endif
CosaDmlWiFiApSetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_AP_CFG       pCfg
    );

ANSC_STATUS
CosaDmlWiFiApGetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_AP_CFG       pCfg
    );

ANSC_STATUS
CosaDmlWiFiApApplyCfg
    (
        PCOSA_DML_WIFI_AP_CFG       pCfg
    );

ANSC_STATUS
CosaDmlWiFiApGetStatsEnable
    (
        UINT        InstanceNumber,
        BOOLEAN     *pbValue
    );

ANSC_STATUS
CosaDmlWiFiApSetStatsEnable
    (
        UINT        InstanceNumber,
        BOOLEAN     bValue
    );

BOOLEAN
IsCosaDmlWiFiApStatsEnable
    (
        UINT    uvAPIndex
    );

ANSC_STATUS
CosaDmlWiFiApPushCfg
    (
        PCOSA_DML_WIFI_AP_CFG       pCfg
    );

ANSC_STATUS
CosaDmlWiFiApGetInfo
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_AP_INFO      pInfo
    );

ANSC_STATUS
CosaDmlWiFiApAssociatedDevicesHighWatermarkGetVal
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_AP_CFG       pCfg
    );
//zqiu
//ANSC_STATUS
//CosaDmlGetHighWatermarkDate
//    (
//       ANSC_HANDLE                 hContext,
//       char*                       pSsid,
//       char                       *pDate
//    );

ANSC_STATUS
CosaDmlGetApRadiusSettings
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_RadiusSetting       pRadiusSetting
    );

ANSC_STATUS
CosaDmlSetApRadiusSettings
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_RadiusSetting       pRadiusSetting
    );

ANSC_STATUS
CosaDmlWiFiApEapAuthCfg
    (
        ANSC_HANDLE                     hContext,
        char*                           pSsid,
        PCOSA_DML_WIFI_APSEC_CFG        pCfg
    );

ANSC_STATUS
CosaDmlWiFiApSecGetEntry
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APSEC_FULL   pEntry
    );

ANSC_STATUS
CosaDmlWiFiApSecApplyEntry
    (
        PCOSA_DML_WIFI_APSEC_FULL   pEntry,
        ULONG                                          instanceNumber
    );

ANSC_STATUS
CosaDmlWiFiApSecSetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APSEC_CFG    pCfg
    );

ANSC_STATUS
CosaDmlWiFiApSecLoadKeyPassphrase
    (
	ULONG                    instanceNumber,
	PCOSA_DML_WIFI_APSEC_CFG pCfg
    );

ANSC_STATUS
CosaDmlWiFiApSecGetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APSEC_CFG    pCfg
    );

ANSC_STATUS
CosaDmlWiFiApSecPushCfg
    (
        PCOSA_DML_WIFI_APSEC_CFG    pCfg,
        ULONG                                          instanceNumber
    );

ANSC_STATUS
CosaDmlWiFiApSecApplyCfg
    (
        PCOSA_DML_WIFI_APSEC_CFG    pCfg,
        ULONG                                          instanceNumber
    );

ANSC_STATUS CosaDmlWiFiApSecsetMFPConfig( int vAPIndex, CHAR *pMfpConfig );

ANSC_STATUS
CosaDmlWiFiApSecApplyWepCfg
    (
PCOSA_DML_WIFI_APSEC_CFG    pCfg,
ULONG                                          instanceNumber
    );

ANSC_STATUS
CosaDmlWiFiApWpsGetEntry
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APWPS_FULL   pEntry
    );

ANSC_STATUS
CosaDmlWiFiApWpsSetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APWPS_CFG    pCfg
    );

ANSC_STATUS
CosaDmlWiFiApWpsGetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APWPS_CFG    pCfg
    );

ANSC_STATUS
CosaDmlWiFiApWpsApplyCfg
    (
        PCOSA_DML_WIFI_APWPS_CFG    pCfg,
        ULONG                       index
    );

ANSC_STATUS
CosaDmlWiFiApWpsGetInfo
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APWPS_INFO   pInfo
    );

ANSC_STATUS
CosaDmlWiFiApWpsSetInfo
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APWPS_INFO    pInfo
    );

/* Description:
 *	This routine is to retrieve the complete list of currently associated WiFi devices, 
 *	which is a dynamic table.
 * Arguments:
 *	pSsid   		Indicate which SSID to operate on.
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
    );

/* Description:
 *	This routine is to retrieve the complete list of currently associated WiFi devices
 *	and kick them to force them to disassociate.
 * Arguments:
 *	pSsid   		Indicate which SSID to operate on.
 * Return:
 * Status
 */
ANSC_STATUS
CosaDmlWiFiApKickAssocDevices
    (
        char*                       pSsid
    );

/*
 * WiFi AP MAC Filter
 */

ANSC_STATUS 
CosaDmlWifi_setBSSTransitionActivated
    (

       PCOSA_DML_WIFI_AP_CFG pCfg, 
       ULONG apIns
);

ANSC_STATUS
CosaDmlWiFiApMfSetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_AP_MF_CFG    pCfg
    );

ANSC_STATUS
CosaDmlWiFiApMfGetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_AP_MF_CFG    pCfg
    );

ANSC_STATUS
CosaDmlWiFiApMfPushCfg
    (
        PCOSA_DML_WIFI_AP_MF_CFG    pCfg,
        ULONG                                           wlanIndex
    );

ANSC_STATUS
CosaDmlWiFiApSetMFQueue
    (
        QUEUE_HEADER *mfQueue,
        ULONG                  apIns
    );

ANSC_STATUS
CosaDmlWiFi_GetWEPKey64ByIndex
    (
        ULONG apIns, 
        ULONG keyIdx, 
        PCOSA_DML_WEPKEY_64BIT pWepKey
    );

ANSC_STATUS
CosaDmlWiFi_SetWEPKey64ByIndex
    (
        ULONG apIns, 
        ULONG keyIdx, 
        PCOSA_DML_WEPKEY_64BIT pWepKey
    );

ANSC_STATUS
CosaDmlWiFi_GetWEPKey128ByIndex
    (
        ULONG apIns, 
        ULONG keyIdx, 
        PCOSA_DML_WEPKEY_128BIT pWepKey
    );

ANSC_STATUS
CosaDmlWiFi_SetWEPKey128ByIndex
    (
        ULONG apIns, 
        ULONG keyIdx, 
        PCOSA_DML_WEPKEY_128BIT pWepKey
    );

ULONG
CosaDmlMacFilt_GetNumberOfEntries
    (
        ULONG apIns
    );

ANSC_STATUS
CosaDmlMacFilt_GetEntryByIndex
    (
        ULONG apIns, 
        ULONG index, 
        PCOSA_DML_WIFI_AP_MAC_FILTER pMacFilt
    );

ANSC_STATUS
CosaDmlMacFilt_SetValues
    (
        ULONG apIns, 
        ULONG index, 
        ULONG ins, 
        char *Alias
    );

ANSC_STATUS
CosaDmlMacFilt_AddEntry
    (
        ULONG apIns, 
        PCOSA_DML_WIFI_AP_MAC_FILTER pMacFilt
    );

ANSC_STATUS
CosaDmlMacFilt_DelEntry
    (
        ULONG apIns, 
        ULONG macFiltIns
    );

ANSC_STATUS
CosaDmlMacFilt_GetConf
    (
        ULONG apIns, 
        ULONG macFiltIns, 
        PCOSA_DML_WIFI_AP_MAC_FILTER pMacFilt
    );

ANSC_STATUS
CosaDmlMacFilt_SetConf
    (
        ULONG apIns, 
        ULONG macFiltIns, 
        PCOSA_DML_WIFI_AP_MAC_FILTER pMacFilt
    );

ANSC_STATUS 
CosaDmlWiFi_GetConfigFile(void *buf, int *size);

ANSC_STATUS
CosaDmlWiFi_SetConfigFile(const void *buf, int size);

#if defined (FEATURE_SUPPORT_WEBCONFIG)
ANSC_STATUS
CosaDmlWiFi_setWebConfig(char *webconfstr, int size,uint8_t ssid);
#endif

ANSC_STATUS 
CosaDmlWiFi_RadioUpTime(ULONG *TimeInSecs, int radioIndex);
ANSC_STATUS 
CosaDmlWiFi_getRadioCarrierSenseThresholdRange(INT radioIndex, INT *output);
ANSC_STATUS 
CosaDmlWiFi_getRadioCarrierSenseThresholdInUse(INT radioIndex, INT *output);
ANSC_STATUS 
CosaDmlWiFi_setRadioCarrierSenseThresholdInUse(INT radioIndex, INT threshold);
ANSC_STATUS 
CosaDmlWiFi_getRadioBeaconPeriod(INT radioIndex, UINT *output);
ANSC_STATUS 
CosaDmlWiFi_setRadioBeaconPeriod(INT radioIndex, UINT BeaconPeriod);
ANSC_STATUS
CosaDmlWiFi_setRadio_X_RDK_OffChannelTscan(INT radioIndex, UINT X_RDK_OffChannelTscan);
ANSC_STATUS
CosaDmlWiFi_setRadio_X_RDK_OffChannelNscan(INT radioIndex, UINT X_RDK_OffChannelNscan);
ANSC_STATUS
CosaDmlWiFi_setRadio_X_RDK_OffChannelTidle(INT radioIndex, UINT X_RDK_OffChannelTidle);

//ANSC_STATUS 
//CosaDmlWiFi_getRadioBasicDataTransmitRates(INT radioIndex, ULONG *output);
//ANSC_STATUS 
//CosaDmlWiFi_setRadioBasicDataTransmitRates(INT radioIndex,ULONG val);

ANSC_STATUS 
CosaDmlWiFi_getRadioStatsRadioStatisticsMeasuringRate(INT radioInstanceNumber, INT *output);
ANSC_STATUS 
CosaDmlWiFi_setRadioStatsRadioStatisticsMeasuringRate(INT radioInstanceNumber, INT rate);
ANSC_STATUS 
CosaDmlWiFi_setRadioStatsRadioStatisticsMeasuringInterval(INT radioInstanceNumber, INT rate);
ANSC_STATUS 
CosaDmlWiFi_getRadioStatsRadioStatisticsMeasuringInterval(INT radioInstanceNumber, INT *output);
ANSC_STATUS 
CosaDmlWiFi_resetRadioStats(PCOSA_DML_WIFI_RADIO_STATS pWifiStats);
ANSC_STATUS 
CosaDmlWiFi_getRadioStatsReceivedSignalLevel(INT radioInstanceNumber, INT *iRsl);

ANSC_STATUS 
//CosaDmlWiFi_doNeighbouringScan ( PCOSA_DML_NEIGHTBOURING_WIFI_RESULT *ppNeighScanResult, unsigned int *pResCount );
CosaDmlWiFi_doNeighbouringScan ( PCOSA_DML_NEIGHTBOURING_WIFI_DIAG_CFG pNeighScan);

ANSC_STATUS
CosaDmlWiFiNeighbouringGetEntry
    (
        ANSC_HANDLE                 hContext,
        PCOSA_DML_NEIGHTBOURING_WIFI_DIAG_CFG   pEntry
    );

	
void CosaDmlGetNeighbouringDiagnosticEnable(BOOLEAN *DiagEnable);
void CosaDmlSetNeighbouringDiagnosticEnable(BOOLEAN DiagEnableVal);

	
void *RegisterWiFiConfigureCallBack(void *par);
void getDefaultSSID(int wlanIndex, char *DefaultSSID);
void getDefaultPassphase(int wlanIndex, char *DefaultPassphrase);

ANSC_STATUS 
CosaDmlWiFi_GetBandSteeringOptions(PCOSA_DML_WIFI_BANDSTEERING_OPTION pBandSteeringOption);

ANSC_STATUS 
CosaDmlWiFi_GetBandSteeringCapability(BOOL *support);

ANSC_STATUS 
CosaDmlWiFi_GetBandSteeringEnable(BOOL *enable);

ANSC_STATUS 
CosaDmlWiFi_GetBandSteeringLog(CHAR *BandHistory, INT TotalNoOfChars);

ANSC_STATUS 
CosaDmlWiFi_GetBandSteeringLog_2(void);

ANSC_STATUS 
CosaDmlWiFi_GetBandSteeringLog_3(void);

ANSC_STATUS 
CosaDmlWiFi_SetBandSteeringOptions(PCOSA_DML_WIFI_BANDSTEERING_OPTION  pBandSteeringOption);

ANSC_STATUS 
CosaDmlWiFi_GetBandSteeringSettings(int radioIndex, PCOSA_DML_WIFI_BANDSTEERING_SETTINGS pBandSteeringSettings);

ANSC_STATUS 
CosaDmlWiFi_SetBandSteeringSettings(int radioIndex, PCOSA_DML_WIFI_BANDSTEERING_SETTINGS pBandSteeringSettings);

void WriteWiFiLog(char *);

void AssociatedDevice_callback_register();
void* CosaDmlWiFi_DisableBandSteeringBasedonACLThread( void *input );  

ANSC_STATUS
CosaDmlWiFi_GetGoodRssiThresholdValue( int  *piRssiThresholdValue );

ANSC_STATUS
CosaDmlWiFi_SetGoodRssiThresholdValue( int  iRssiThresholdValue );

ANSC_STATUS
CosaDmlWiFi_GetAssocCountThresholdValue( int  *piAssocCountThresholdValue );

ANSC_STATUS
CosaDmlWiFi_SetAssocCountThresholdValue( int  iAssocCountThresholdValue );

ANSC_STATUS
CosaDmlWiFi_GetAssocMonitorDurationValue( int  *piAssocMonitorDurationValue );

ANSC_STATUS
CosaDmlWiFi_SetAssocMonitorDurationValue( int  iAssocMonitorDurationValue );

ANSC_STATUS
CosaDmlWiFi_GetAssocGateTimeValue( int  *piAssocGateTimeValue );

ANSC_STATUS
CosaDmlWiFi_SetAssocGateTimeValue( int  iAssocGateTimeValue );

ANSC_STATUS
CosaDmlWiFi_SetRapidReconnectThresholdValue(ULONG vAPIndex, int	rapidReconnThresholdValue );

ANSC_STATUS
CosaDmlWiFi_GetRapidReconnectThresholdValue(ULONG vAPIndex, int	*rapidReconnThresholdValue );

ANSC_STATUS
CosaDmlWiFi_GetRapidReconnectCountEnable(ULONG vAPIndex, BOOLEAN *pbReconnectCountEnable, BOOLEAN usePersistent );

ANSC_STATUS
CosaDmlWiFi_SetRapidReconnectCountEnable(ULONG vAPIndex, BOOLEAN bReconnectCountEnable );

ANSC_STATUS
CosaDmlWiFi_SetFeatureMFPConfigValue( BOOLEAN bFeatureMFPConfig );

ANSC_STATUS
CosaDmlWiFi_SetApMFPConfigValue( ULONG vAPIndex, char *pMFPConfig );

ANSC_STATUS
CosaDmlWiFi_GetApMFPConfigValue( ULONG vAPIndex, char *pMFPConfig );

ANSC_STATUS
CosaDmlWiFi_GetFeatureMFPConfigValue( BOOLEAN *pbFeatureMFPConfig );

ANSC_STATUS
CosaDmlWiFiRadiogetSupportedStandards ( int wlanIndex, ULONG *pulsupportedStandards );

ANSC_STATUS
CosaDmlWiFi_InstantMeasurementsEnable(PCOSA_DML_WIFI_AP_ASSOC_DEVICE pWifiApDev, BOOL enable);

BOOL
CosaDmlWiFi_IsInstantMeasurementsEnable();

ANSC_STATUS
CosaDmlWiFiClient_InstantMeasurementsEnable(PCOSA_DML_WIFI_HARVESTER pHarvester);

ANSC_STATUS
CosaDmlWiFiClient_InstantMeasurementsReportingPeriod(ULONG reportingPeriod);

ANSC_STATUS
CosaDmlWiFiClient_InstantMeasurementsMacAddress(char *macAddress);

ANSC_STATUS
CosaDmlWiFiClient_InstantMeasurementsOverrideTTL(ULONG overrideTTL);

ANSC_STATUS
CosaDmlWiFiClient_InstantMeasurementsDefReportingPeriod(ULONG defPeriod);

BOOL
Validate_InstClientMac(char * physAddress);

BOOL
validateDefReportingPeriod(ULONG period);

void* RemoveInvalidMacFilterListFromPsm();

#if defined(FEATURE_HOSTAP_AUTHENTICATOR)
// HOSTAPD RELATED API(S)
void CosaDmlWiFiGetHostapdAuthenticatorEnable(BOOLEAN *pbEnableHostapdAuthenticator);
BOOL CosaDmlWiFiSetHostapdAuthenticatorEnable(PANSC_HANDLE phContext, BOOLEAN bValue, BOOLEAN bInit);
#endif //FEATURE_HOSTAP_AUTHENTICATOR

#if !defined(_HUB4_PRODUCT_REQ_) && !defined(_XB7_PRODUCT_REQ_)
void CosaDmlWifi_getDppConfigFromPSM(PANSC_HANDLE phContext);
#endif // !defined(_HUB4_PRODUCT_REQ_)

ANSC_STATUS
CosaDmlWiFi_setStatus(ULONG status, PANSC_HANDLE pMyObject);

#if defined(_COSA_BCM_MIPS_) || defined(_XB6_PRODUCT_REQ_) || defined(_COSA_BCM_ARM_) || defined(_PLATFORM_TURRIS_)
ANSC_STATUS UpdateJsonParam
        (
                char*                       pKey,
                char*                   PartnerId,
                char*                   pValue,
                char*                   pSource,
                char*                   pCurrentTime
    );
#endif

#ifdef WIFI_HAL_VERSION_3
UINT getRadioIndexFromAp(UINT apIndex);
UINT getPrivateApFromRadioIndex(UINT radioIndex);

CHAR* getVAPName(UINT apIndex);
BOOL isVapPrivate(UINT apIndex);
BOOL isVapXhs(UINT apIndex);
BOOL isVapHotspot(UINT apIndex);
BOOL isVapLnf(UINT apIndex);
BOOL isVapLnfPsk(UINT apIndex);
BOOL isVapMesh(UINT apIndex);
BOOL isVapHotspotSecure(UINT apIndex);
ANSC_STATUS getVAPIndexFromName(CHAR *vapName, UINT *apIndex);
UINT convertRadioIndexToFrequencyNum(UINT radioIndex);
BOOL isVapLnfSecure(UINT apIndex);

#endif

#ifdef FEATURE_RADIO_WEBCONFIG
INT getRadioIndexFromRadioName(char *radio_name, wifi_radio_index_t *radio_instance);
#endif

#if defined (_HUB4_PRODUCT_REQ_)
ANSC_STATUS CosaDmlWiFi_GetParamValues( char *pComponent, char *pBus, char *pParamName, char *pReturnVal );
ANSC_STATUS CosaDmlWiFi_StartWiFiClientsMonitorAndSyncThread( void );
void* CosaDmlWiFi_WiFiClientsMonitorAndSyncThread( void *arg );

#endif /* * _HUB4_PRODUCT_REQ_ */

ANSC_STATUS CosaDmlWiFi_GetRapidReconnectIndicationEnable(BOOL *bEnable, BOOL usePersistent);
ANSC_STATUS COSAGetParamValueByPathName(void* bus_handle, parameterValStruct_t *val, ULONG *parameterValueLength);
void WiFiPramValueChangedCB(parameterSigStruct_t* val, int size, void* user_data);
int SetParamAttr(char *pParameterName, int NotificationType);
INT wifi_getIndexFromName(CHAR *inputSsidString, INT *ouput_int);
ANSC_STATUS CosaDmlWiFi_ApplyRoamingConsortiumElement(PCOSA_DML_WIFI_AP_CFG pCfg);
INT wifi_restartHostApd();
INT wifi_ifConfigUp(INT apIndex);
INT wifi_getApBasicAuthenticationMode(INT apIndex, CHAR *authMode);
INT wifi_getRadioEnable(INT radioIndex, BOOL *output_bool);
ANSC_STATUS CosaDmlWiFi_SetRapidReconnectIndicationEnable(BOOL bEnable);
ANSC_STATUS CosaDmlWiFi_RadioGetResetCount(INT radioIndex, ULONG *output);
ANSC_STATUS CosaDmlWiFiApGetNeighborReportActivated(ULONG vAPIndex, BOOLEAN *pbNeighborReportActivated, BOOLEAN usePersistent );
ANSC_STATUS CosaDmlWiFi_getChanUtilSelfHealEnable(INT radioInstance, ULONG *enable);
ANSC_STATUS CosaDmlWiFi_getChanUtilThreshold(INT radioInstance, PUINT ChanUtilThreshold); 
INT wifi_getBandSteeringEnable(BOOL *enable);
void CosaDmlWiFiWebConfigFrameworkInit();
ANSC_STATUS CosaDmlWiFiRadioGetChannelsInUse(ANSC_HANDLE hContext, ULONG ulInstanceNumber, PCOSA_DML_WIFI_RADIO_DINFO  pInfo);
ANSC_STATUS CosaDmlWiFiRadioGetApChannelScan(ANSC_HANDLE hContext, ULONG ulInstanceNumber, PCOSA_DML_WIFI_RADIO_DINFO  pInfo);
BOOL is_mesh_enabled();
ANSC_STATUS CosaDmlWiFi_setChanUtilSelfHealEnable(INT radioInstance, UINT enable);
ANSC_STATUS CosaDmlWiFi_setChanUtilThreshold(INT radioInstance, UINT ChanUtilThreshold);
INT CosaWifiAdjustBeaconRate(int radioindex, char *beaconRate);
INT CosaDmlWiFiGetApBeaconRate(int apIndex, char *BeaconRate);
ANSC_STATUS CosaDmlWiFiApSetNeighborReportActivated(ULONG vAPIndex, BOOLEAN bNeighborReportActivated);
ANSC_STATUS CosaDmlWiFi_setInterworkingElement(PCOSA_DML_WIFI_AP_CFG pCfg);
ANSC_STATUS CosaDmlWiFi_setDppVersion(ULONG apIns, ULONG version);
ANSC_STATUS CosaDmlWiFi_setDppReconfig(ULONG apIns,char* ParamName,char *value );
int CosaDmlWiFi_IsValidMacAddr(const char* mac);
ANSC_STATUS CosaDmlWiFi_setDppValue(ULONG apIns, ULONG staIndex,char* ParamName,char *value);
ANSC_STATUS CosaDmlWiFi_ParseEasyConnectEnrolleeChannels(UINT apIndex, PCOSA_DML_WIFI_DPP_STA_CFG pWifiDppSta, const char *pString);
char *CosaDmlWiFi_ChannelsListToString(PCOSA_DML_WIFI_DPP_STA_CFG pWifiDppSta, char *string);
INT wifi_getApWpsEnable(INT apIndex, BOOL *output_bool);

#if !defined (_COSA_BCM_MIPS_)&& !defined(_COSA_BCM_ARM_) && !defined(_PLATFORM_TURRIS_)
    INT wifi_setApEnableOnLine(INT index, BOOL status);
    INT wifi_setRouterEnable(INT radioIndex, BOOL output_bool);
    INT wifi_getApEnableOnLine(INT index, BOOL *status);
    INT wifi_getRouterEnable(INT radioIndex, BOOL *output_bool);
#endif

#if !defined(_XF3_PRODUCT_REQ_) && !defined(_CBR_PRODUCT_REQ_) && !defined(_HUB4_PRODUCT_REQ_) && !defined(_XB7_PRODUCT_REQ_) && !defined(_PLATFORM_TURRIS_)
    void wifi_anqpStartReceivingTestFrame();
#endif

#ifdef CISCO_XB3_PLATFORM_CHANGES
    INT wifi_setApWepKeyIndex(INT apIndex, ULONG output_ulong);
    INT wifi_getApWepKeyIndex(INT apIndex, ULONG *output_ulong);
    INT wifi_getWepKey(INT apIndex, INT keyIndex, CHAR *output_string);
    INT wifi_setApBasicEncryptionMode(INT apIndex, CHAR *encMode);
#endif

#ifdef CISCO_XB3_PLATFORM_CHANGES
    void wifi_PCIReset();
    int wifi_setNFCalcThreshold(int radioIndex, int threshold);
    int wifi_setNFCalcChainmask(int radioIndex, int chainmask);
    int wifi_getCurrentTime(char *currentTime);
    int wifi_setNFCalcTrigger(int radioIndex, char *createTime, int testOption);
    int wifi_getNFCalcThreshold(int radioIndex, int *threshold);
    int wifi_getNFCalcNumIter(int radioIndex, int *num_iter);
    int wifi_getNFCalcChainmask(int radioIndex, int *chainmask);
    int wifi_getNFCalcResults(int radioIndex, int resultsIndex, int *results);
    int wifi_getNFCalcResultsCreateTime(int radioIndex, int resultsIndex, char *createTime);
    int wifi_getNFCalcResultsPAOffAvgCh(int radioIndex, int resultsIndex, char *paOffAvgCh);
    int wifi_getNFCalcResultsPAOnAvgCh(int radioIndex, int resultsIndex, char *paOnAvgCh);
    int wifi_getNFCalcResultsPAOffMaxCh(int radioIndex, int resultsIndex, char *paOffMaxCh);
    int wifi_getNFCalcResultsPAOnMaxCh(int radioIndex, int resultsIndex, char *paOnMaxCh);
    int wifi_getNFCalcResultsInfo(int radioIndex, int resultsIndex, char *results_info);
    int wifi_setNFCalcNumIter(int radioIndex, int num_iter);
#endif

#if !defined(_XB6_PRODUCT_REQ_)
    INT CosaDmlWiFiGetRadioOperationalDataTransmitRates(int radioIndex, char *TransmitRates);
#endif

#if defined(_COSA_BCM_MIPS_)
    INT CosaDmlWiFiGetRadioBasicDataTransmitRates(int radioIndex, char *TransmitRates);
    INT CosaDmlWiFiGetRadioSupportedDataTransmitRates(int radioIndex, char *TransmitRates);
#endif

#if defined(_INTEL_BUG_FIXES_)
    ANSC_STATUS CosaDmlWiFiRadioGetDBWCfg(ANSC_HANDLE hContext, PCOSA_DML_WIFI_RADIO_CFG pCfg);
#endif

struct wifiDataTxRateHalMap
{
    wifi_bitrate_t  DataTxRateEnum;
    char DataTxRateStr[8];
};

#if defined (FEATURE_HOSTAP_AUTHENTICATOR) && defined(_XB7_PRODUCT_REQ_)
void *wifi_libhostap_apply_settings(void *arg);
#endif

#ifdef WIFI_HAL_VERSION_3
#define CCSP_WIFI_TRACE 1
#define CCSP_WIFI_INFO  2

struct wifiFreqBandHalMap
{
    wifi_freq_bands_t halWifiFreqBand;
    COSA_DML_WIFI_FREQ_BAND cosaWifiFreqBand;
    char wifiFreqBandStr[16];
};

struct wifiStdCosaHalMap
{
    wifi_ieee80211Variant_t halWifiStd;
    COSA_DML_WIFI_STD  cosaWifiStd;
    char wifiStdName[4];
};

struct wifiChanWidthCosaHalMap
{
    wifi_channelBandwidth_t halWifiChanWidth;
    COSA_DML_WIFI_CHAN_BW  cosaWifiChanWidth;
    char wifiChanWidthName[16];
};

struct wifiCountryEnumStrMap
{
    wifi_countrycode_type_t countryCode;
    char countryStr[4];
};

struct wifiWPSCosaHalMap
{
    wifi_onboarding_methods_t halWPSCfgMethod;
    COSA_DML_WIFI_WPS_METHOD  cosaWPSCfgMethod;
    char wpsConfigMethod[32];
};

struct wifiSecCosaHalMap
{
    wifi_security_modes_t halSecCfgMethod;
    COSA_DML_WIFI_SECURITY cosaSecCfgMethod;
    char wifiSecType[32];
};

struct wifiSecEncrCosaHalMap
{
    wifi_encryption_method_t halSecEncrMethod;
    COSA_DML_WIFI_AP_SEC_ENCRYPTION cosaSecEncrMethod;
    char wifiSecEncrType[16];
};

struct wifiSecMfpCosaHalMap
{
    wifi_mfp_cfg_t halSecMFP;
    char wifiSecMFP[32];
};

struct wifiGuardIntervalMap
{
    wifi_guard_interval_t halGuardInterval;
    COSA_DML_WIFI_GUARD_INTVL cosaGuardInterval;
    char wifiGuardIntervalType[8];
};

typedef struct {
      wifi_vap_name_t         vap_name[64];
      UINT                    vap_index;
} rdk_wifi_vap_info_t;

typedef struct {
      //Hal variables
      UINT                num_vaps;
      wifi_vap_info_t     vap_array[MAX_NUM_VAP_PER_RADIO];
      //CCspWifiAgent variables
      wifi_radio_index_t  radio_index;
      rdk_wifi_vap_info_t rdk_vap_array[MAX_NUM_VAP_PER_RADIO];
} __attribute__((packed)) rdk_wifi_vap_map_t;

typedef struct {
      wifi_radio_operationParam_t oper;
      rdk_wifi_vap_map_t          vaps;
} rdk_wifi_radio_t;

wifi_vap_info_t *getVapInfo(UINT apIndex);
wifi_radio_capabilities_t *getRadioCapability(UINT radioIndex);
wifi_radio_operationParam_t *getRadioOperationParam(UINT radioIndex);
rdk_wifi_vap_info_t *getRdkVapInfo(UINT apIndex);
ANSC_STATUS rdkWifiConfigInit();
void ccspWifiDbgPrint(int level, char *format, ...);
ANSC_STATUS rdkGetIndexFromName(char *pSsid, UINT *pWlanIndex);
ANSC_STATUS txRateStrToUint(char *inputStr, UINT *pTxRate);
ANSC_STATUS freqBandStrToEnum(char *pFreqBandStr, wifi_freq_bands_t *pFreqBandEnum);
ANSC_STATUS wifiStdStrToEnum(char *pWifiStdStr, wifi_ieee80211Variant_t *p80211VarEnum);
ANSC_STATUS regDomainStrToEnum(char *pRegDomain, wifi_countrycode_type_t *pCountryCode);
ANSC_STATUS guardIntervalDmlEnumtoHalEnum(UINT ccspGiEnum, wifi_guard_interval_t *halGiEnum);
ANSC_STATUS operChanBandwidthDmlEnumtoHalEnum(UINT ccspBw, wifi_channelBandwidth_t *halBw);
ANSC_STATUS radioGetCfgUpdateFromHalToDml(UINT wlanIndex, PCOSA_DML_WIFI_RADIO_CFG pCfg, wifi_radio_operationParam_t *pWifiRadioOperParam);
ANSC_STATUS getMFPTypeFromString (const char *MFPName, wifi_mfp_cfg_t *MFPType);
ANSC_STATUS CosaDmlWiFiSetApMacFilterPsmData(int wlanIndex, PCOSA_DML_WIFI_AP_MF_CFG  pCfg);
ANSC_STATUS wifiRadioOperParamValidation(UINT radioIndex, wifi_radio_operationParam_t *pWifiRadioOperParam);
ANSC_STATUS wifiRadioChannelIsValid(UINT radioIndex, UINT inputChannel);
ANSC_STATUS wifiRadioSecondaryChannelUpdate(UINT radioIndex, wifi_radio_operationParam_t *pWifiRadioOperParam,UINT extensionChannel);
ANSC_STATUS wifiRadioVapInfoValidation(UINT vapIndex, wifi_vap_info_t *pWifiVapInfo);
ANSC_STATUS wifiApIsSecmodeOpenForPrivateAP(UINT vapIndex);
ANSC_STATUS radioGetCfgUpdateFromDmlToHal(UINT  radioIndex, PCOSA_DML_WIFI_RADIO_CFG    pCfg, wifi_radio_operationParam_t *pWifiRadioOperParam);
ANSC_STATUS wifiSecModeDmlToStr(UINT dmlSecModeEnabled, char *str, UINT strSize);
ANSC_STATUS wifiSecSupportedDmlToStr(UINT dmlSecModeSupported, char *str, UINT strSize);
ANSC_STATUS vapGetCfgUpdateFromDmlToHal(rdk_wifi_vap_map_t *tmpWifiVapInfoMap);

UINT getTotalNumberVAPs();
UINT getNumberRadios();
UINT getMaxNumberVAPsPerRadio(UINT radioIndex);
UINT getNumberofVAPsPerRadio(UINT radioIndex);
rdk_wifi_vap_map_t *getRdkWifiVap(UINT radioIndex);
INT getSecurityTypeFromString(const char *securityName, wifi_security_modes_t *securityType, COSA_DML_WIFI_SECURITY *cosaSecurityType);
INT getOperBandwidthFromString(const char *operBandwidth, wifi_channelBandwidth_t *halWifiChanWidth, COSA_DML_WIFI_CHAN_BW *cosaWifiChanWidth);
INT getIpAddressFromString (const char * ipString, ip_addr_t * ip);
INT  getBeaconRateFromString (const char *beaconName, wifi_bitrate_t *beaconType);

UINT getTotalNumberVAPs();
UINT getNumberRadios();
UINT getMaxNumberVAPsPerRadio(UINT radioIndex);
UINT getNumberofVAPsPerRadio(UINT radioIndex);
ANSC_STATUS CosaDmlWiFiSetDefaultApSecCfg (ULONG wlanIndex);
ANSC_STATUS getMFPTypeFromString (const char *MFPName, wifi_mfp_cfg_t *MFPType);
ANSC_STATUS CosaDmlCheckToKickAssocDevices(char* pSsid, PCOSA_DML_WIFI_AP_CFG pCfg);
#endif //WIFI_HAL_VERSION_3

#endif
