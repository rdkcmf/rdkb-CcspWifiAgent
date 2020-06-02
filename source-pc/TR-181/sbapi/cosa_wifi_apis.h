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

#define  COSA_DML_WIFI_MAX_MAC_FILTER_NUM           50

#define COSA_DML_WEP_KEY_NUM                        16

#define COSA_DML_WIFI_MAX_SSID_NAME_LEN               33//LNT_EMU

#define  COSA_DML_WIFI_MAX_BAND_STEERING_HISTORY_NUM  ( 1024 ) // 2 * 512 = 1024 bytes//LNT_EMU

#define MAX_MAC_FILT 				     16//LNT_EMU
#define MAX_NUM_HOST               		     50//LNT_EMU  

#define COSA_DML_WIFI_ATM_MAX_APGROUP_NUM				8
#define COSA_DML_WIFI_ATM_MAX_APLIST_STR_LEN            256 
#define COSA_DML_WIFI_ATM_MAX_STA_NUM	              	32

typedef  struct
_COSA_DATAMODEL_RDKB_WIFIREGION_CLASS_CONTENT
{
        CHAR                    Code[4];
}
COSA_DATAMODEL_RDKB_WIFIREGION, *PCOSA_DATAMODEL_RDKB_WIFIREGION;
 

typedef  enum
_COSA_DML_WIFI_FREQ_BAND
{
    COSA_DML_WIFI_FREQ_BAND_2_4G        = 1,
    COSA_DML_WIFI_FREQ_BAND_5G          = 2
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
}
COSA_DML_WIFI_STD, *PCOSA_DML_WIFI_STD;


typedef  enum
_COSA_DML_WIFI_CHAN_BW
{
    COSA_DML_WIFI_CHAN_BW_20M           = 1,
    COSA_DML_WIFI_CHAN_BW_40M,
    COSA_DML_WIFI_CHAN_BW_80M,
    COSA_DML_WIFI_CHAN_BW_160M
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
    COSA_DML_WIFI_GUARD_INTVL_Auto
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
    COSA_DML_WIFI_SECURITY_None                 = 0x00000001,
    COSA_DML_WIFI_SECURITY_WEP_64               = 0x00000002,
    COSA_DML_WIFI_SECURITY_WEP_128              = 0x00000004,
    COSA_DML_WIFI_SECURITY_WPA_Personal         = 0x00000008,
    COSA_DML_WIFI_SECURITY_WPA2_Personal        = 0x00000010,
    COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal    = 0x00000020,
    COSA_DML_WIFI_SECURITY_WPA_Enterprise       = 0x00000040,
    COSA_DML_WIFI_SECURITY_WPA2_Enterprise      = 0x00000080,
    COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise  = 0x00000100
}
COSA_DML_WIFI_SECURITY, *PCOSA_DML_WIFI_SECURITY;

typedef  enum
_COSA_DML_WIFI_AP_SEC_ENCRYPTION
{
    COSA_DML_WIFI_AP_SEC_TKIP    = 1,
    COSA_DML_WIFI_AP_SEC_AES,
    COSA_DML_WIFI_AP_SEC_AES_TKIP
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
    COSA_DML_WIFI_WPS_METHOD_Pin                = 0x00000040
}
COSA_DML_WIFI_WPS_METHOD, *PCOSA_DML_WIFI_WPS_METHOD;

typedef enum
_COSA_DML_WIFI_AP_SEC_WEPMODE
{
    COSA_DML_WIFI_WEPMODE_WEP64 = 1,
    COSA_DML_WIFI_WEPMODE_WEP128,
}
COSA_DML_WIFI_AP_SEC_WEPMODE, *PCOSA_DML_WIFI_AP_SEC_WEPMODE;

/*
 *  Structure definitions for WiFi Radio
 */
struct 
_COSA_DML_WIFI_RADIO_CFG
{
    ULONG                           InstanceNumber;
    char                            Alias[COSA_DML_ALIAS_NAME_LENGTH];
    BOOLEAN                         bEnabled;
    ULONG		            LastChange; //RDKB-EMU
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
    char                            RegulatoryDomain[4];
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
    ULONG                           LongRetryLimit;
    int                             MbssUserControl;
    int                             AdminControl;
    int                             OnOffPushButtonTime;
    int                             ObssCoex;
    int                             MulticastRate;
    BOOL                            ApplySetting;
    BOOL                            X_CISCO_COM_ReverseDirectionGrant;
    BOOL                            X_CISCO_COM_AggregationMSDU;
    BOOL                            X_CISCO_COM_AutoBlockAck;
    BOOL                            X_CISCO_COM_DeclineBARequest;
    ULONG                           X_CISCO_COM_HTTxStream;
    ULONG                           X_CISCO_COM_HTRxStream;
    BOOL                            X_CISCO_COM_STBCEnable;
}_struct_pack_;

typedef struct _COSA_DML_WIFI_RADIO_CFG COSA_DML_WIFI_RADIO_CFG,  *PCOSA_DML_WIFI_RADIO_CFG;

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
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_RADIO_SINFO COSA_DML_WIFI_RADIO_SINFO,  *PCOSA_DML_WIFI_RADIO_SINFO;


/*
 *  Dynamic portion of WiFi radio info
 */
struct
_COSA_DML_WIFI_RADIO_DINFO
{
    COSA_DML_IF_STATUS              Status;
    //ULONG                           LastChange;
    char                            ChannelsInUse[512];
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_RADIO_DINFO COSA_DML_WIFI_RADIO_DINFO,  *PCOSA_DML_WIFI_RADIO_DINFO;

//LNT_EMU

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

}COSA_DML_NEIGHTBOURING_WIFI_RESULT,*PCOSA_DML_NEIGHTBOURING_WIFI_RESULT;//LNT_EMU

typedef struct _COSA_DML_NEIGHTBOURING_WIFI_DIAG_CFG {
   BOOL bEnable;
   CHAR DiagnosticsState[64];
   ULONG ResultCount;
   ULONG ResultCount_2;
   ULONG ResultCount_5;
   PCOSA_DML_NEIGHTBOURING_WIFI_RESULT pResult_2;
   PCOSA_DML_NEIGHTBOURING_WIFI_RESULT pResult_5;
}COSA_DML_NEIGHTBOURING_WIFI_DIAG_CFG,*PCOSA_DML_NEIGHTBOURING_WIFI_DIAG_CFG;//LNT_EMU


struct
_COSA_DML_WIFI_RADIO_FULL
{
    COSA_DML_WIFI_RADIO_CFG         Cfg;
    COSA_DML_WIFI_RADIO_SINFO       StaticInfo;
    COSA_DML_WIFI_RADIO_DINFO       DynamicInfo;
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_RADIO_FULL COSA_DML_WIFI_RADIO_FULL, *PCOSA_DML_WIFI_RADIO_FULL;

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
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_RADIO_STATS COSA_DML_WIFI_RADIO_STATS, *PCOSA_DML_WIFI_RADIO_STATS;



/*
 *  Structure definitions for WiFi SSID
 */
//LNT_EMU

struct
_COSA_DML_WIFI_SSID_EncryptionInfo
{
    ULONG                           ModeEnabled;
    ULONG                           Encryption;
    char                            WepKey[128];
    char                            PreSharedKey[128];
    char                            Passphrase[128];
    ULONG                           RekeyInterval;
    ANSC_IPV4_ADDRESS               RadiusServerIP;
    ULONG                           RadiusServerPort;
    char                            RadiusSecret[128];
}_struct_pack_;

typedef struct _COSA_DML_WIFI_SSID_EncryptionInfo COSA_DML_WIFI_SSID_EncryptionInfo, *PCOSA_DML_WIFI_SSID_EncryptionInfo;//LNT_EMU

struct
_COSA_DML_WIFI_SSID_QosSetting
{
    ULONG                           InstanceNumber;
    ULONG                           SSIDInstanceNumber;
    ULONG                           AC;
    BOOLEAN                         ACM;
    ULONG                           AIFSN;
    ULONG                           CWMin;
    ULONG                           CWMax;
    ULONG                           TXOPLimit;
    BOOLEAN                         NoACK;
}_struct_pack_;

typedef struct _COSA_DML_WIFI_SSID_QosSetting COSA_DML_WIFI_SSID_QosSetting, *PCOSA_DML_WIFI_SSID_QosSetting;//LNT_EMU

struct
_COSA_DML_WIFI_SSID_QosInfo
{
    BOOLEAN                         WMMEnable;
    BOOLEAN                         UAPSDEnable;
    COSA_DML_WIFI_SSID_QosSetting   QosSetting[4];
}_struct_pack_;

typedef struct _COSA_DML_WIFI_SSID_QosInfo COSA_DML_WIFI_SSID_QosInfo, *PCOSA_DML_WIFI_SSID_QosInfo;//LNT_EMU

struct
_COSA_DML_WIFI_WPS
{
    BOOLEAN                         bEnabled;
    char                            X_CISCO_COM_Pin[64];
    char                            SSIDIndex[17];
}_struct_pack_;

typedef struct _COSA_DML_WIFI_WPS COSA_DML_WIFI_WPS,  *PCOSA_DML_WIFI_WPS;//LNT_EMU


struct
_COSA_DML_WIFI_SSID_CFG
{
    ULONG                           InstanceNumber;
    char                            Alias[COSA_DML_ALIAS_NAME_LENGTH];

    BOOLEAN                         bEnabled;
    char                            WiFiRadioName[COSA_DML_ALIAS_NAME_LENGTH]; /* Points to the underlying WiFi Radio */
    char                            DefaultSSID[COSA_DML_WIFI_MAX_SSID_NAME_LEN];//LNT_EMU
    char                            SSID[32];
}_struct_pack_;

typedef struct _COSA_DML_WIFI_SSID_CFG COSA_DML_WIFI_SSID_CFG,  *PCOSA_DML_WIFI_SSID_CFG;


/*
 *  Static portion of WiFi SSID info
 */
struct
_COSA_DML_WIFI_SSID_SINFO
{
    char                            Name[COSA_DML_ALIAS_NAME_LENGTH];
    UCHAR                           BSSID[6];
    UCHAR                           MacAddress[6];
}_struct_pack_;

typedef struct _COSA_DML_WIFI_SSID_SINFO COSA_DML_WIFI_SSID_SINFO,  *PCOSA_DML_WIFI_SSID_SINFO;


/*
 *  Dynamic portion of WiFi SSID info
 */
struct
_COSA_DML_WIFI_SSID_DINFO
{
    COSA_DML_IF_STATUS              Status;
    ULONG                           LastChange;
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
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_SSID_STATS COSA_DML_WIFI_SSID_STATS, *PCOSA_DML_WIFI_SSID_STATS;

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
    ULONG                           InstanceNumber;
    char                            Alias[COSA_DML_ALIAS_NAME_LENGTH];
    char                            SSID[32];           /* Reference to SSID name */

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
    char 		     MacFilterMode[12];
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

struct
_COSA_DML_WIFI_AP_FULL
{
    COSA_DML_WIFI_AP_CFG            Cfg;
    COSA_DML_WIFI_AP_INFO           Info;

    /* USGv2 Extensions */ 
    QUEUE_HEADER                    MacFilterList;
    ULONG                           ulMacFilterNextInsNum;
    ANSC_HANDLE                     hIrepFolderMacFilt;
    ANSC_HANDLE                     hPoamMacFlitDm;
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
    ANSC_IPV4_ADDRESS               RadiusServerIPAddr;
    ULONG                           RadiusServerPort;
    char                            RadiusSecret[64];
    /* USGv2 Extensions */
    int                             RadiusReAuthInterval;
    int                             DefaultKey;
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
    char                            WEPKey[8 * 2 + 1];
}
COSA_DML_WEPKEY_64BIT, *PCOSA_DML_WEPKEY_64BIT;

typedef struct
_COSA_DML_WEPKEY_128BIT
{
    char                            WEPKey[16 * 2 + 1];
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
    char                            OperatingStandard[64];
    char                            OperatingChannelBandwidth[64];
    int                             SNR;
    char                            InterferenceSources[64];
    ULONG                           DataFramesSentAck;
    ULONG                           DataFramesSentNoAck;
    ULONG                           BytesSent;
    ULONG                           BytesReceived;

    int                             RSSI;
    int                             MinRSSI;
    int                             MaxRSSI;
    unsigned int                    Disassociations;
    unsigned int                    AuthenticationFailures;
}_struct_pack_;

typedef struct _COSA_DML_WIFI_AP_ASSOC_DEVICE COSA_DML_WIFI_AP_ASSOC_DEVICE,  *PCOSA_DML_WIFI_AP_ASSOC_DEVICE;

/*
 * Structure definitions for WiFi Device Provisioning Protocol
 */
#define COSA_DML_WIFI_DPP_STA_MAX                                   16

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
    UCHAR           EnrolleeResponderStatus[32];
        UINT                    NumChannels;
    UINT            Channels[32];
    CHAR            InitiatorBootstrapSubjectPublicKeyInfo[256];
    CHAR            ResponderBootstrapSubjectPublicKeyInfo[256];
}_struct_pack_;

typedef struct _COSA_DML_WIFI_DPP_STA_CFG COSA_DML_WIFI_DPP_STA_CFG, *PCOSA_DML_WIFI_DPP_STA_CFG;

struct
_COSA_DML_WIFI_DPP_STA_FULL
{
    COSA_DML_WIFI_DPP_STA_CFG         Cfg[COSA_DML_WIFI_DPP_STA_MAX];
}_struct_pack_;

typedef struct _COSA_DML_WIFI_DPP_STA_FULL COSA_DML_WIFI_DPP_STA_FULL, *PCOSA_DML_WIFI_DPP_STA_FULL;

ANSC_STATUS
CosaDmlWiFi_startDPP(PCOSA_DML_WIFI_DPP_STA_CFG pWifiDppSta, ULONG apIns);

#endif // !defined(_HUB4_PRODUCT_REQ_)


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

ANSC_STATUS CosaDmlWiFi_GetATMOptions(PCOSA_DML_WIFI_ATM  pATM);
ANSC_STATUS CosaWifiRegGetATMInfo( ANSC_HANDLE   hThisObject);

//<<

/*
 *  Structure definitions for WiFi Radius Settings
 */
//LNT_EMU
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

typedef  struct _COSA_DML_WIFI_RadiusSetting COSA_DML_WIFI_RadiusSetting,  *PCOSA_DML_WIFI_RadiusSetting;//LNT_EMU

/*
 *  Structure definitions for WiFi BandSteering Settings
 */
//LNT_EMU
struct
_COSA_DML_WIFI_BANDSTEERING_OPTION
{
    BOOLEAN                          bEnable;
    BOOLEAN                          bCapability;
    CHAR                                                         BandHistory[ COSA_DML_WIFI_MAX_BAND_STEERING_HISTORY_NUM ];
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_BANDSTEERING_OPTION COSA_DML_WIFI_BANDSTEERING_OPTION, *PCOSA_DML_WIFI_BANDSTEERING_OPTION;//LNT_EMU

struct
_COSA_DML_WIFI_BANDSTEERING_SETTINGS
{
    int                                 InstanceNumber;
    int                                 UtilizationThreshold;
    int                                 RSSIThreshold;
        int                                                     PhyRateThreshold;
}_struct_pack_;

typedef  struct _COSA_DML_WIFI_BANDSTEERING_SETTINGS COSA_DML_WIFI_BANDSTEERING_SETTINGS, *PCOSA_DML_WIFI_BANDSTEERING_SETTINGS;//LNT_EMU

static int                          g_macFiltCnt = 0;//LNT_EMU                                                              
static COSA_DML_WIFI_AP_MAC_FILTER  g_macFiltTab[MAX_MAC_FILT] = {
{ 1, "MacFilterTable1", "00:1a:2b:aa:bb:cc", "Dummy-Host" },
}  ;//LNT_EMU     

struct hostDetails   //LNT_EMU
{
        char hostName[20];
        char InterfaceType[50];
};


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
CosaDmlWiFiRegionInit
  (
    PCOSA_DATAMODEL_RDKB_WIFIREGION PWiFiRegion
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
CosaDmlWiFiRadioGetDinfo
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber,
        PCOSA_DML_WIFI_RADIO_DINFO  pInfo
    );

ANSC_STATUS
CosaDmlWiFiRadioGetStats
    (
        ANSC_HANDLE                 hContext,
        ULONG                       ulInstanceNumber,
        PCOSA_DML_WIFI_RADIO_STATS  pStats
    );

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

//LNT_EMU
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
        ULONG               wlanIndex
    );

ANSC_STATUS
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
CosaDmlWiFiApSecGetEntry
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APSEC_FULL   pEntry
    );

ANSC_STATUS
CosaDmlWiFiApSecSetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APSEC_CFG    pCfg
    );

ANSC_STATUS
CosaDmlWiFiApSecGetCfg
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APSEC_CFG    pCfg
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
CosaDmlWiFiApWpsGetInfo
    (
        ANSC_HANDLE                 hContext,
        char*                       pSsid,
        PCOSA_DML_WIFI_APWPS_INFO   pInfo
    );

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
    );


/*
 * WiFi AP MAC Filter
 */
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
        ULONG                       wlanIndex
    );

ANSC_STATUS
CosaDmlWiFiApSetMFQueue
    (
        QUEUE_HEADER *mfQueue,
        ULONG         apIns
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

#endif
