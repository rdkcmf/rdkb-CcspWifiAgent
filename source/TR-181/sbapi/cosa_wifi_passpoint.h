#ifndef _WIFI_PASSPOINT_H
#define _WIFI_PASSPOINT_H

#define WIFI_PASSPOINT_DIR                  "/nvram/passpoint"
#define WIFI_PASSPOINT_GAS_CFG_FILE        "/nvram/passpoint/passpointGasCfg.json"
#define WIFI_PASSPOINT_DEFAULT_GAS_CFG     "{\"gasConfig\": [{ \"advertId\": 0, \"pauseForServerResp\": true, \"respTimeout\": 5000, \"comebackDelay\": 1000, \"respBufferTime\": 1000, \"queryRespLengthLimit\": 127 }]}"
#define WIFI_PASSPOINT_ANQP_CFG_FILE        "/nvram/passpoint/passpointAnqpCfg.json"
#define WIFI_PASSPOINT_DEFAULT_ANQP_CFG     "{\"InterworkingService\": {}}"
#define WIFI_PASSPOINT_HS2_CFG_FILE        "/nvram/passpoint/passpointHs2Cfg.json"
#define WIFI_PASSPOINT_DEFAULT_HS2_CFG     "{\"Passpoint\": {}}"

#include "collection.h"

static char *PasspointEnable   = "eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.%d.X_RDKCENTRAL-COM_Passpoint.Enable";

typedef struct {
    UCHAR apIndex;
    mac_address_t sta;
    unsigned char token;
    wifi_anqp_node_t *head;
} cosa_wifi_anqp_context_t;

typedef struct
{
    USHORT info_id;
    USHORT len;
    UCHAR  oi[3];
    UCHAR  wfa_type;
} __attribute__((packed)) wifi_vendor_specific_anqp_capabilities_t;

typedef struct {
    int    capabilityInfoLength;
    wifi_capabilityListANQP_t  capabilityInfo;//wifi_HS2_CapabilityList_t
    UCHAR  venueCount;
    int    venueInfoLength;
    UCHAR  *venueInfo;//wifi_venueName_t
    UCHAR  *ipAddressInfo;//wifi_ipAddressAvailabality_t
    UCHAR  realmCount;
    int    realmInfoLength;
    UCHAR  *realmInfo;//wifi_naiRealm_t
    int    gppInfoLength;
    UCHAR  *gppInfo;//wifi_3gppCellularNetwork_t
    int    roamInfoLength;
    UCHAR  *roamInfo;//wifi_roamingConsoritum_t
    int    domainInfoLength;
    UCHAR  *domainNameInfo;//wifi_domainName_t
} cosa_wifi_anqp_data_t;

typedef struct {
    BOOL   hs2Status;
    BOOL   gafDisable;
    BOOL   p2pDisable;
    int    capabilityInfoLength;
    wifi_HS2_CapabilityList_t  capabilityInfo;//wifi_HS2_CapabilityList_t
    int    opFriendlyNameInfoLength;
    UCHAR  *opFriendlyNameInfo;//wifi_HS2_OperatorFriendlyName_t
    int    connCapabilityLength;
    UCHAR  *connCapabilityInfo;//wifi_HS2_ConnectionCapability_t
    int    realmInfoLength;
    UCHAR  *realmInfo;//wifi_HS2_NAI_Home_Realm_Query_t
    wifi_HS2_WANMetrics_t wanMetricsInfo;
    UCHAR  passpointStats[1024];
    UINT   domainRespCount;
    UINT   realmRespCount;
    UINT   gppRespCount;
    UINT   domainFailedCount;
    UINT   realmFailedCount;
    UINT   gppFailedCount;
} cosa_wifi_hs2_data_t;
        
#endif //_WIFI_PASSPOINT_H
