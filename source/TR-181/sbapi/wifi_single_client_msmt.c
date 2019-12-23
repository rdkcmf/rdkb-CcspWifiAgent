#include <avro.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include "cosa_apis.h"
#include "wifi_hal.h"
#include "collection.h"
#include "wifi_monitor.h"
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/un.h>
#include <assert.h>
#include <uuid/uuid.h>
#include "ansc_status.h"
#include <sysevent/sysevent.h>
#include "ccsp_base_api.h"
#include "harvester.h"

// UUID - 8b27dafc-0c4d-40a1-b62c-f24a34074914

// HASH - 4388e585dd7c0d32ac47e71f634b579b

uint8_t HASHVAL[16] = {0x43, 0x88, 0xe5, 0x85, 0xdd, 0x7c, 0x0d, 0x32,
                    0xac, 0x47, 0xe7, 0x1f, 0x63, 0x4b, 0x57, 0x9b
                   };

uint8_t UUIDVAL[16] = {0x8b, 0x27, 0xda, 0xfc, 0x0c, 0x4d, 0x40, 0xa1,
                    0xb6, 0x2c, 0xf2, 0x4a, 0x34, 0x07, 0x49, 0x14
                   };

// local data, load it with real data if necessary
char Report_Source[] = "wifi";
char CPE_TYPE_STR[] = "Gateway";

#define MAX_BUFF_SIZE	20480

#define MAGIC_NUMBER      0x85
#define MAGIC_NUMBER_SIZE 1
#define SCHEMA_ID_LENGTH  32
#define MAC_KEY_LEN 13

typedef enum {
	single_client_msmt_type_all,
	single_client_msmt_type_all_per_bssid,
	single_client_msmt_type_one,
} single_client_msmt_type_t;

static char *to_sta_key    (mac_addr_t mac, sta_key_t key) {
    snprintf(key, STA_KEY_LEN, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return (char *)key;
}

void upload_single_client_msmt_data(bssid_data_t *bssid_info, radio_data_t *radio_data, sta_data_t *sta_info)
{
	const char * serviceName = "wifi";
  	const char * dest = "event:raw.kestrel.reports.WifiSingleClient";
  	const char * contentType = "avro/binary"; // contentType "application/json", "avro/binary"
  	uuid_t transaction_id;
  	char trans_id[37];
	FILE *fp;
	char *buff, line[1024];
	int size;
	bssid_data_t *bssid_data;
	hash_map_t *sta_map;
	sta_data_t	*sta_data;
	wifi_monitor_t *monitor;
	single_client_msmt_type_t msmt_type;
  	
	avro_writer_t writer;
	avro_schema_t inst_msmt_schema = NULL;
	avro_schema_error_t	error = NULL;
	avro_value_iface_t  *iface = NULL;
  	avro_value_t  adr = {0}; /*RDKB-7463, CID-33353, init before use */
  	avro_value_t  adrField = {0}; /*RDKB-7463, CID-33485, init before use */
  	avro_value_t optional  = {0}; 

	if (bssid_info == NULL) { 
		if (sta_info != NULL) {
       		wifi_dbg_print(1, "%s:%d: Invalid arguments\n", __func__, __LINE__);
			return;
		} else {
			msmt_type = single_client_msmt_type_all;
		}

	} else {
		if (sta_info == NULL) {
			msmt_type = single_client_msmt_type_all_per_bssid;
		} else {
			msmt_type = single_client_msmt_type_one;
		}
	}
       	
	wifi_dbg_print(1, "%s:%d: Measurement Type: %d\n", __func__, __LINE__, msmt_type);
	monitor = get_wifi_monitor();

	/* open schema file */
	fp = fopen (WIFI_SINGLE_CLIENT_AVRO_FILENAME , "rb");
	if (fp == NULL)
	{
		wifi_dbg_print(1, "%s:%d: Unable to open schema file: %s\n", __func__, __LINE__, WIFI_SINGLE_CLIENT_AVRO_FILENAME);
		return;
	}

	/* seek through file and get file size*/
	fseek(fp , 0L , SEEK_END);
	size = ftell(fp);
	if(size < 0)
	{
		wifi_dbg_print(1, "%s:%d: ftell error\n", __func__, __LINE__);
		fclose(fp);
		return;
	}
	/*back to the start of the file*/
	rewind(fp);

	/* allocate memory for entire content */
	buff = malloc(size + 1);
	memset(buff, 0, size + 1);

	/* copy the file into the buffer */
	if (1 != fread(buff , size, 1 , fp))
	{
		fclose(fp);
		free(buff);
		wifi_dbg_print(1, "%s:%d: Unable to read schema file: %s\n", __func__, __LINE__, WIFI_SINGLE_CLIENT_AVRO_FILENAME);
		return ;
	}
	buff[size]='\0';
	fclose(fp);

	if (avro_schema_from_json(buff, strlen(buff), &inst_msmt_schema, &error))
	{
		free(buff);
		wifi_dbg_print(1, "%s:%d: Unable to parse steering schema, len: %d, error:%s\n", __func__, __LINE__, size, avro_strerror());
		return;
	}
	free(buff);

	//generate an avro class from our schema and get a pointer to the value interface
	iface = avro_generic_class_from_schema(inst_msmt_schema);

	avro_schema_decref(inst_msmt_schema);

	buff = malloc(MAX_BUFF_SIZE);
	memset(buff, 0, MAX_BUFF_SIZE);
  	buff[0] = MAGIC_NUMBER; /* fill MAGIC number = Empty, i.e. no Schema ID */

  	memcpy( &buff[MAGIC_NUMBER_SIZE], UUIDVAL, sizeof(UUIDVAL));
  	memcpy( &buff[MAGIC_NUMBER_SIZE + sizeof(UUIDVAL)], HASHVAL, sizeof(HASHVAL));

  	writer = avro_writer_memory((char*)&buff[MAGIC_NUMBER_SIZE + SCHEMA_ID_LENGTH], MAX_BUFF_SIZE - MAGIC_NUMBER_SIZE - SCHEMA_ID_LENGTH);
	avro_writer_reset(writer);
  	avro_generic_value_new(iface, &adr);

  	// timestamp - long
  	avro_value_get_by_name(&adr, "header", &adrField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
  	avro_value_get_by_name(&adrField, "timestamp", &adrField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
  	avro_value_set_branch(&adrField, 1, &optional);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());

  	struct timeval ts;
  	gettimeofday(&ts, NULL);

  	int64_t tstamp_av_main = ((int64_t) (ts.tv_sec) * 1000000) + (int64_t) ts.tv_usec;
  
  	tstamp_av_main = tstamp_av_main/1000;
  	avro_value_set_long(&optional, tstamp_av_main );

  	// uuid - fixed 16 bytes
  	uuid_generate_random(transaction_id);
  	uuid_unparse(transaction_id, trans_id);

  	avro_value_get_by_name(&adr, "header", &adrField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
  	avro_value_get_by_name(&adrField, "uuid", &adrField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
  	avro_value_set_branch(&adrField, 1, &optional);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
  	avro_value_set_fixed(&optional, transaction_id, 16);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
  	unsigned char *ptxn = (unsigned char*)transaction_id;
	wifi_dbg_print(1, "Report transaction uuid generated is %s\n", trans_id);
  	CcspTraceInfo(("Single client report transaction uuid generated is %s\n", trans_id ));

  	//source - string
  	avro_value_get_by_name(&adr, "header", &adrField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
  	avro_value_get_by_name(&adrField, "source", &adrField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
  	avro_value_set_branch(&adrField, 1, &optional);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
  	avro_value_set_string(&optional, Report_Source);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());

	const char *macStr = NULL;
	char CpemacStr[32];

  	//cpe_id block
  	/* MAC - Get CPE mac address, do it only pointer is NULL */
  	if ( macStr == NULL )
  	{
		macStr = getDeviceMac();
		strncpy( CpemacStr, macStr, sizeof(CpemacStr));
		wifi_dbg_print(1, "RDK_LOG_DEBUG, Received DeviceMac from Atom side: %s\n",macStr);
  	}

  	char CpeMacHoldingBuf[ 20 ] = {0};
  	unsigned char CpeMacid[ 7 ] = {0};
	unsigned int k;

  	for (k = 0; k < 6; k++ )
  	{
		/* copy 2 bytes */
		CpeMacHoldingBuf[ k * 2 ] = CpemacStr[ k * 2 ];
		CpeMacHoldingBuf[ k * 2 + 1 ] = CpemacStr[ k * 2 + 1 ];
		CpeMacid[ k ] = (unsigned char)strtol(&CpeMacHoldingBuf[ k * 2 ], NULL, 16);
  	}
	
  	avro_value_get_by_name(&adr, "cpe_id", &adrField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
  	avro_value_get_by_name(&adrField, "mac_address", &adrField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
  	avro_value_set_branch(&adrField, 1, &optional);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
  	avro_value_set_fixed(&optional, CpeMacid, 6);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
  	unsigned char *pMac = (unsigned char*)CpeMacid;
  	wifi_dbg_print(1, "RDK_LOG_DEBUG, mac_address = 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n", pMac[0], pMac[1], pMac[2], pMac[3], pMac[4], pMac[5] );
	
  	// cpe_type - string
  	avro_value_get_by_name(&adr, "cpe_id", &adrField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
  	avro_value_get_by_name(&adrField, "cpe_type", &adrField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
  	avro_value_set_branch(&adrField, 1, &optional);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
  	avro_value_set_string(&optional, CPE_TYPE_STR);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());

  	//Data Field block
	wifi_dbg_print(1, "data field\n");
  	avro_value_get_by_name(&adr, "data", &adrField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());

	//adrField now contains a reference to the Single Client WiFi ReportsArray
  	//Device Report

  	//Current Device Report Field
  	avro_value_t drField = {0}; /*RDKB-7463, CID-33269, init before use */

	//data block

        /*unsigned int i;
	for (i = 0; i < MAX_VAP; i++) 
	{
		if (msmt_type == single_client_msmt_type_all) {
		bssid_data = &monitor->bssid_data[i];
		} else {
		bssid_data = bssid_info;
		if (msmt_type == single_client_msmt_type_one) {
			sta_data = sta_info;
		} else {

		}
		}
	}*/
	wifi_dbg_print(1, "updating bssid_data and sta_data\n");
	bssid_data = bssid_info;
        sta_data = sta_info;
	
	if(sta_data == NULL)
	{
		wifi_dbg_print(1, "sta_data is empty\n");
	}
	else
	{
		//device_mac - fixed 6 bytes
		wifi_dbg_print(1, "adding cli_MACAddress field\n");
		avro_value_get_by_name(&adrField, "device_id", &drField, NULL);
		if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
		avro_value_get_by_name(&drField, "mac_address", &drField, NULL);
		if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
		avro_value_set_branch(&drField, 1, &optional);
		if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
		avro_value_set_fixed(&optional, sta_data->dev_stats.cli_MACAddress, 6);
		if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
	}

	//device_status - enum
	avro_value_get_by_name(&adrField, "device_id", &drField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "Avro error: %s\n",  avro_strerror());
	avro_value_get_by_name(&drField, "device_status", &drField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, " Avro error: %s\n",  avro_strerror());

	if((sta_data != NULL) && sta_data->dev_stats.cli_Active)
	{
		wifi_dbg_print(1,"active\n");
		avro_value_set_enum(&drField, avro_schema_enum_get_by_name(avro_value_get_schema(&drField), "Online"));
	}
	else
	{
		avro_value_set_enum(&drField, avro_schema_enum_get_by_name(avro_value_get_schema(&drField), "Offline"));
	}
	if (CHK_AVRO_ERR) wifi_dbg_print(1, " Avro error: %s\n",  avro_strerror());

	//timestamp - long
	avro_value_get_by_name(&adrField, "timestamp", &drField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
	avro_value_set_branch(&drField, 1, &optional);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
	avro_value_set_long(&optional, tstamp_av_main);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());

	memset(CpeMacHoldingBuf, 0, sizeof CpeMacHoldingBuf);
	memset(CpeMacid, 0, sizeof CpeMacid);
	char bssid[MAC_KEY_LEN];
	snprintf(bssid, MAC_KEY_LEN, "%02x%02x%02x%02x%02x%02x",
	    bssid_data->bssid[0], bssid_data->bssid[1], bssid_data->bssid[2],
	    bssid_data->bssid[3], bssid_data->bssid[4], bssid_data->bssid[5]);

	wifi_dbg_print(1, "BSSID for vap : %s\n",bssid);

    	for (k = 0; k < 6; k++ ) {
		/* copy 2 bytes */
		CpeMacHoldingBuf[ k * 2 ] = bssid[ k * 2 ];
		CpeMacHoldingBuf[ k * 2 + 1 ] = bssid[ k * 2 + 1 ];
		CpeMacid[ k ] = (unsigned char)strtol(&CpeMacHoldingBuf[ k * 2 ], NULL, 16);
    	}

    	// interface_mac
	avro_value_get_by_name(&adrField, "interface_mac", &drField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
	avro_value_set_fixed(&drField, CpeMacid, 6);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
	pMac = (unsigned char*)CpeMacid;
	wifi_dbg_print(1, "RDK_LOG_DEBUG, interface mac_address = 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n", pMac[0], pMac[1], pMac[2], pMac[3], pMac[4], pMac[5] );

	// vAP_index
	avro_value_get_by_name(&adrField, "vAP_index", &drField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "Avro error: %s\n",  avro_strerror());
	avro_value_set_branch(&drField, 1, &optional);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "Avro error: %s\n",  avro_strerror());
	if(monitor !=NULL)
	{
		avro_value_set_int(&optional, (monitor->inst_msmt.ap_index)+1);
	}
	else
	{
		avro_value_set_int(&optional, 0);
	}
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "Avro error: %s\n",  avro_strerror());

    	//interface metrics block
		if (msmt_type == single_client_msmt_type_one) {
			sta_data = sta_info;
		} else {
			sta_map = bssid_data->sta_map;
			sta_data = hash_map_get_first(sta_map); 
		}
	while (sta_data != NULL) {
    	//rx_rate
	avro_value_get_by_name(&adrField, "interface_metrics", &drField, NULL);
    	avro_value_set_branch(&drField, 1, &optional);
    	avro_value_get_by_name(&optional, "rx_rate", &drField, NULL);
    	avro_value_set_branch(&drField, 1, &optional);
	avro_value_set_int(&optional, (int)sta_data->dev_stats.cli_LastDataDownlinkRate);

    	//tx_rate
	avro_value_get_by_name(&adrField, "interface_metrics", &drField, NULL);
    	avro_value_set_branch(&drField, 1, &optional);
    	avro_value_get_by_name(&optional, "tx_rate", &drField, NULL);
    	avro_value_set_branch(&drField, 1, &optional);
	avro_value_set_int(&optional, (int)sta_data->dev_stats.cli_LastDataUplinkRate);

	//tx_packets
	avro_value_get_by_name(&adrField, "interface_metrics", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);
	avro_value_get_by_name(&optional, "tx_packets", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);
	avro_value_set_int(&optional, sta_data->dev_stats.cli_PacketsReceived);

	//rx_packets
	avro_value_get_by_name(&adrField, "interface_metrics", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);
	avro_value_get_by_name(&optional, "rx_packets", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);
	avro_value_set_int(&optional, sta_data->dev_stats.cli_PacketsSent);

	//tx_error_packets
	avro_value_get_by_name(&adrField, "interface_metrics", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);
	avro_value_get_by_name(&optional, "tx_error_packets", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);
	avro_value_set_int(&optional, sta_data->dev_stats.cli_ErrorsSent);

    	//retransmissions
	avro_value_get_by_name(&adrField, "interface_metrics", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);
	avro_value_get_by_name(&optional, "retransmissions", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);
	avro_value_set_int(&optional, sta_data->dev_stats.cli_Retransmissions);

	//channel_utilization_percent_5ghz
	avro_value_get_by_name(&adrField, "interface_metrics", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);
	avro_value_get_by_name(&optional, "channel_utilization_percent_5ghz", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);

	if(radio_data !=NULL)
	{
		wifi_dbg_print(1,"avro set radio_data->channelUtil_radio_2 to int\n");
		avro_value_set_int(&optional, (int)radio_data->channelUtil_radio_2);
	}
	else
	{
		avro_value_set_int(&optional, 0);
	}

	//channel_interference_percent_5ghz
	wifi_dbg_print(1,"channel_interference_percent_5ghz field\n");
	avro_value_get_by_name(&adrField, "interface_metrics", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);
	avro_value_get_by_name(&optional, "channel_interference_percent_5ghz", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);
	if(radio_data !=NULL)
	{
		avro_value_set_int(&optional, (int)radio_data->channelInterference_radio_2);
	}
	else
	{
		avro_value_set_int(&optional, 0);
	}

	//channel_noise_floor_5ghz
	avro_value_get_by_name(&adrField, "interface_metrics", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);
	avro_value_get_by_name(&optional, "channel_noise_floor_5ghz", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);

	if((monitor !=NULL) && ((monitor->inst_msmt.ap_index+1) == 2)) //Noise floor for vAP index 2 (5GHz)
	{
		//avro_value_set_int(&optional, (int)(sta_data->dev_stats.cli_SignalStrength - sta_data->dev_stats.cli_SNR));
		avro_value_set_int(&optional, (int)radio_data->NoiseFloor);
	}
	else
	{
		avro_value_set_int(&optional, 0);
	}

	//channel_utilization_percent_2_4ghz
	avro_value_get_by_name(&adrField, "interface_metrics", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);
	avro_value_get_by_name(&optional, "channel_utilization_percent_2_4ghz", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);
	if(radio_data !=NULL)
	{
		avro_value_set_int(&optional, (int)radio_data->channelUtil_radio_1);
	}
	else
	{
		avro_value_set_int(&optional, 0);
	}

	//channel_interference_percent_2_4ghz
	avro_value_get_by_name(&adrField, "interface_metrics", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);
	avro_value_get_by_name(&optional, "channel_interference_percent_2_4ghz", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);
	if(radio_data !=NULL)
	{
		avro_value_set_int(&optional, (int)radio_data->channelInterference_radio_1);
	}
	else
	{
		avro_value_set_int(&optional, 0);
	}

	//channel_noise_floor_2_4ghz
	avro_value_get_by_name(&adrField, "interface_metrics", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);
	avro_value_get_by_name(&optional, "channel_noise_floor_2_4ghz", &drField, NULL);
	avro_value_set_branch(&drField, 1, &optional);

	if((monitor!=NULL) && ((monitor->inst_msmt.ap_index+1) == 1)) //Noise floor for vAP index 1 (2.4GHz)
	{
		//avro_value_set_int(&optional, (int)(sta_data->dev_stats.cli_SignalStrength - sta_data->dev_stats.cli_SNR));
		avro_value_set_int(&optional, (int)radio_data->NoiseFloor);
	}
	else
	{
		avro_value_set_int(&optional, 0);
	}

    	//signal_strength
	avro_value_get_by_name(&adrField, "interface_metrics", &drField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
    	avro_value_set_branch(&drField, 1, &optional);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
    	avro_value_get_by_name(&optional, "signal_strength", &drField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
    	avro_value_set_branch(&drField, 1, &optional);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
	avro_value_set_int(&optional, (int)sta_data->dev_stats.cli_SignalStrength);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());

	//snr
	avro_value_get_by_name(&adrField, "interface_metrics", &drField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
	avro_value_set_branch(&drField, 1, &optional);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
	avro_value_get_by_name(&optional, "snr", &drField, NULL);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
	avro_value_set_branch(&drField, 1, &optional);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
	avro_value_set_int(&optional, (int)sta_data->dev_stats.cli_SNR);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());

		if (msmt_type != single_client_msmt_type_all) break;	
	}

        /* check for writer size, if buffer is almost full, skip trailing linklist */
        avro_value_sizeof(&adr, &size);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());
  
	//Thats the end of that
  	avro_value_write(writer, &adr);
	if (CHK_AVRO_ERR) wifi_dbg_print(1, "%s:%d: Avro error: %s\n", __func__, __LINE__, avro_strerror());

	wifi_dbg_print(1, "Avro packing done\n");
	char *json;
        if (!avro_value_to_json(&adr, 1, &json))
	{
		wifi_dbg_print(1,"json is %s\n", json);
		free(json);
        }
  	//Free up memory
  	avro_value_decref(&adr);
  	avro_writer_free(writer);

	size += MAGIC_NUMBER_SIZE + SCHEMA_ID_LENGTH;
	sendWebpaMsg(serviceName, dest, trans_id, contentType, buff, size);
	wifi_dbg_print(1, "Creating telemetry record successful\n");
}

void stream_client_msmt_data()
{
	wifi_monitor_t *monitor;
	hash_map_t  *sta_map;
	sta_data_t *data;
	mac_addr_str_t key;

	monitor = get_wifi_monitor();
	sta_map = monitor->bssid_data[monitor->inst_msmt.ap_index].sta_map;
	to_sta_key(monitor->inst_msmt.sta_mac, key);

	data = (sta_data_t *)hash_map_get(sta_map, key);
	if (data != NULL) {
		upload_single_client_msmt_data(&monitor->bssid_data[monitor->inst_msmt.ap_index], &monitor->radio_data, data);
	}
	
}
