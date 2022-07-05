/************************************************************************************
  If not stated otherwise in this file or this component's LICENSE file the
  following copyright and licenses apply:

  Copyright 2021 RDK Management

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

**************************************************************************/

/*************************************************************************
  Some material from
  Copyright (c) 2003-2014, Jouni Malinen <j@w1.fi>
  Licensed under the BSD-3 License

**************************************************************************/

#include "plugin_main_apis.h"
#include "cosa_wifi_apis.h"
#include "wifi_hostap_auth.h"
#include "cosa_wifi_internal.h"
#include <cjson/cJSON.h>
#include "wifi_hal_rdk.h"
#include "safec_lib_common.h"
#include <sys/stat.h>

#define MAC_LEN 19

#ifdef HOSTAPD_2_10
    #define HOSTAPD_VERSION 210
#else
    #define HOSTAPD_VERSION 209
#endif
/*********************************************
*           GLOBAL VARIABLES                 *
*********************************************/

//Check whether eloop init is already done or not.
static int is_eloop_init_done = 0;

void hapd_reset_ap_interface(int apIndex);
void hapd_wpa_deinit(int ap_index);
void libhostapd_wpa_deinit(int ap_index);
void convert_apindex_to_interface(int idx, char *iface, int len);

#if defined (FEATURE_SUPPORT_RADIUSGREYLIST)
#if !defined (_XB7_PRODUCT_REQ_)
char *cmmac; //Global variable to get the cmmac of the gateway
#endif

/****************************************************
*           FUNCTION DEFINITION(S)                  *
****************************************************/

/* RDKB-30263 Grey List control from RADIUS
This function is to execute the command and retrieve the value
@arg1 - cmd - command to execute on syste,
@arg2 - retBuf - Buffer to hold the data
@ags3 - retBufSize - buffer size
return - success 0 Failue 1
*/
int _syscmd(char *cmd, char *retBuf, int retBufSize)
{
    FILE *f;
    char *ptr = retBuf;
    int bufSize=retBufSize, bufbytes=0, readbytes=0;

    if ((f = popen(cmd, "r")) == NULL) {
        wpa_printf(MSG_ERROR, "popen %s error\n", cmd);
        return -1;
    }

    while(!feof(f)) {
        *ptr = 0;
        if(bufSize>=128) {
                bufbytes=128;
        } else {
                bufbytes=bufSize-1;
        }
        fgets(ptr,bufbytes,f);
        readbytes=strlen(ptr);
        if( readbytes== 0)
                break;
        bufSize-=readbytes;
        ptr += readbytes;
    }
    pclose(f);
    return 0;
}
#endif //FEATURE_SUPPORT_RADIUSGREYLIST

#if defined (FEATURE_SUPPORT_RADIUSGREYLIST) && !defined(_XB7_PRODUCT_REQ_)
extern void wifi_del_mac_handler(void *eloop_ctx, void *timeout_ctx);
extern void wifi_add_mac_handler(char *addr, struct hostapd_data *hapd);

/* RDKB-30263 Grey List control from RADIUS
This function is to read the mac from /nvram/greylist_mac.txt when the process starts/restart,
add the client mac to the greylist and run 24hrs timers for those client mac*/

void greylist_cache_timeout (struct hostapd_data *hapd)
{

    FILE *fptr;
    char date[10] = {0}, get_time[10] = {0}, mac[18] = {0};
    char buffer1[26] = {0}, time_buf[26] = {0};
    struct tm time1, time2;
    time_t newtime;
    struct tm *timeinfo;
    time_t t1, t2;
    int index = 0;
    struct greylist_mac *mac_data;
    u32 timeleft=0;
    u32 timeout=TWENTYFOUR_HR_IN_SEC;

    /* Open the file to get the list of greylisted mac*/
    if ((fptr = fopen("/nvram/greylist_mac.txt", "r")) == NULL) {
        wpa_printf(MSG_DEBUG,"No such file, no greylist client\n");
        return;
    }
    /* Read each mac from the file and add the mac to the greylist*/
    while ((fscanf(fptr, "%s %s %s %d", date, get_time, mac, &index)) == 4) {  // Read the time and mac from the file
        mac_data = os_zalloc(sizeof(*mac_data));
        if (mac_data == NULL) {
            wpa_printf(MSG_DEBUG, "unable to allocate memory "
                    "failed");
            fclose(fptr);
            return;
        }
        timeleft = 0;
        timeout = TWENTYFOUR_HR_IN_SEC;
        os_memcpy(mac_data->mac,mac,sizeof(mac));
        snprintf(time_buf,20,"%s %s",date,get_time);
        time ( &newtime );                                       //Get the curent time to calculate the remaining time
        timeinfo = localtime ( &newtime );
        strftime(buffer1, 26, "%Y-%m-%d %H:%M:%S", timeinfo);    // convert the time to this format 2020-11-09 13:17:56
        if (!strptime(buffer1, "%Y-%m-%d %T", &time1))
            printf("\nstrptime failed-1\n");
        if (!strptime(time_buf, "%Y-%m-%d %T", &time2))
            printf("\nstrptime failed-2\n");
        t1 = mktime(&time1);
        t2 = mktime(&time2);

        wifi_add_mac_handler(mac, hapd);                    // Add the mac to the greylist
        timeleft=comparetime(t1,t2,mac);              // calculate the time leftout for the client from the 24 hrs of time
        if (timeleft  > TWENTYFOUR_HR_IN_SEC ) {     // Due to some reason client entry is not removed after 24hrs force the clienttimeout to 0
            wpa_printf(MSG_DEBUG,"Timeout exceeded :%d",timeleft);
            timeout = 0;
        }
        else {
            timeout -= timeleft;
        }
        wpa_printf(MSG_DEBUG, "eloop timeout for greylist :%s :timeout:%d",mac_data->mac,timeout);
        eloop_cancel_timeout(wifi_del_mac_handler, mac_data, hapd); //cancel the timer if the timer is already running for the client mac
        eloop_register_timeout(timeout, 0, wifi_del_mac_handler, mac_data, hapd); //start the timer for the client mac
    }
    fclose(fptr);
    return;
}
#endif //FEATURE_SUPPORT_RADIUSGREYLIST

/* Description:
 *      The API is used set vap(athX) interface up or down.
 * Arguments:
 *      ifname - Interface name(athX) to be updated.
 *      dev_up - Switch up/down the respective interface.
 *         1 - UP 0 - Down.
 */

#if !defined(_XB7_PRODUCT_REQ_)
int linux_set_iface_flags(const char *ifname, int dev_up)
{

    return wifi_setIfaceFlags(ifname, dev_up);
}
#endif

/* Description:
 *      The API is used to receive assoc req frame received from client and forward to
 *      lib hostapd authenticator.
 * Arguments:
 *      ap_index - Index of Vap in which frame is received
 *      sta - client station mac-address.
 *      reason - Reason for disassoc and deauth.
 *      frame - Assoc resp frames.
 *      frame_len - Assoc resp length
 */
int hapd_process_assoc_req_frame(unsigned int ap_index, mac_address_t sta, unsigned char *frame, unsigned int frame_len)
{
    struct hostapd_data *hapd;
#if defined (_XB7_PRODUCT_REQ_)
    hapd = g_hapd_glue[ap_index].hapd;
#else
    hapd = &g_hapd_glue[ap_index].hapd;
#endif

    if (!hapd || !hapd->started)
        return -1;

    const struct ieee80211_mgmt *mgmt;
    int len = frame_len;
    u16 fc, stype;
    int ielen = 0;
    const u8 *iebuf = NULL;
    int reassoc = 0;

    mgmt = (const struct ieee80211_mgmt *) frame;

    if (len < (int)IEEE80211_HDRLEN) {
        wpa_printf(MSG_INFO, "%s:%d: ASSOC REQ HEADER is not proper \n", __func__, __LINE__);
        return -1;
    }

    fc = le_to_host16(mgmt->frame_control);

    if (WLAN_FC_GET_TYPE(fc) != WLAN_FC_TYPE_MGMT) {
        wpa_printf(MSG_INFO, "%s:%d: ASSOC REQ HEADER is not ASSOC mgt REQ\n", __func__, __LINE__);
        return -1;
    }

    stype = WLAN_FC_GET_STYPE(fc);

    switch (stype) {
        case WLAN_FC_STYPE_ASSOC_REQ:
            if (len < (int)( IEEE80211_HDRLEN + sizeof(mgmt->u.assoc_req)))
                break;
            ielen = len - (IEEE80211_HDRLEN + sizeof(mgmt->u.assoc_req));
            iebuf = mgmt->u.assoc_req.variable;
            reassoc = 0;
            break;

        case WLAN_FC_STYPE_REASSOC_REQ:
            wpa_printf(MSG_INFO, "%s:%d: REASSOC REQ ieee802_11_mgmt \n", __func__, __LINE__);
            if (len < (int) (IEEE80211_HDRLEN + sizeof(mgmt->u.reassoc_req)))
                break;
            ielen = len - (IEEE80211_HDRLEN + sizeof(mgmt->u.reassoc_req));
            iebuf = mgmt->u.reassoc_req.variable;
            reassoc = 1;
            break;
    }
    hostapd_notif_assoc(hapd, mgmt->sa,
            iebuf,
            ielen,
            reassoc);
    return 0;
}

/* Description:
 *      The API is used to receive assoc rsp frame received from client and forward to
 *      lib hostapd authenticator.
 * Arguments:
 *      ap_index - Index of Vap in which frame is received
 *      sta - client station mac-address.
 *      reason - Reason for disassoc and deauth.
 *      frame - Assoc resp frames.
 *      frame_len - Assoc resp length
 * !!Currently not used!!
 */
int hapd_process_assoc_rsp_frame(unsigned int ap_index, mac_address_t sta, unsigned char *frame, unsigned int frame_len)
{
   //	ieee802_11_mgmt_cb(&g_hapd_glue[ap_index].hapd, frame, frame_len, WLAN_FC_STYPE_ASSOC_RESP, 1);
   return 0;
}

/* Description:
 *      The API is used to receive disassoc/deauth frame received from client and forward to
 *      lib hostapd authenticator.
 * Arguments:
 *      ap_index - Index of Vap in which frame is received
 *      sta - client station mac-address.
 *      reason - Reason for disassoc and deauth.
 */
int hapd_process_disassoc_frame(unsigned int ap_index, mac_address_t sta, int reason)
{
#if !defined (_XB7_PRODUCT_REQ_)
    struct hostapd_data *hapd;
    hapd = &g_hapd_glue[ap_index].hapd;

    if (!hapd || !hapd->started)
        return -1;

    hostapd_notif_disassoc(hapd, sta);
#endif
    return 0;
}

/* Description:
 *      The API is used to receive AUTH req/resp frames from data plane and forward to
 *      lib hostapd authenticator.
 * Arguments:
 *      ap_index - Index of Vap in which frame is received
 *      sta - client station mac-address.
 *      frame - Auth req/resp frames.
 *      frame_len - Auth req/resp length
 *      dir - Direction of frame - uplink or downlink.
 *          uplink - From Client to AP
 *          dwonlink - AP to Client
 *
 */
int hapd_process_auth_frame(unsigned int ap_index, mac_address_t sta, unsigned char *frame, unsigned int frame_len, wifi_direction_t dir)
{
#if defined (_XB7_PRODUCT_REQ_)
    struct hostapd_data *hapd = g_hapd_glue[ap_index].hapd;
#else
    struct hostapd_data *hapd = &g_hapd_glue[ap_index].hapd;
#endif

    if (!hapd || !hapd->started)
        return -1;

    const struct ieee80211_mgmt *mgmt;
    union wpa_event_data event;
    int len = frame_len;

    mgmt = (const struct ieee80211_mgmt *) frame;

    if (len < (int) (IEEE80211_HDRLEN + sizeof(mgmt->u.auth))) {
        wpa_printf(MSG_ERROR, "%s:%d AUTH HDR LEN is not proper END\n", __func__, __LINE__);
        return -1;
    }
    os_memset(&event, 0, sizeof(event));
    if (le_to_host16(mgmt->u.auth.auth_alg) == WLAN_AUTH_SAE) {
        event.rx_mgmt.frame = frame;
        event.rx_mgmt.frame_len = len;
        return -1;
    }
    os_memcpy(event.auth.peer, mgmt->sa, ETH_ALEN);
    os_memcpy(event.auth.bssid, mgmt->bssid, ETH_ALEN);
    event.auth.auth_type = le_to_host16(mgmt->u.auth.auth_alg);
    event.auth.status_code = le_to_host16(mgmt->u.auth.status_code);
    event.auth.auth_transaction = le_to_host16(mgmt->u.auth.auth_transaction);
    event.auth.ies = mgmt->u.auth.variable;
    event.auth.ies_len = len - IEEE80211_HDRLEN - sizeof(mgmt->u.auth);

    if (dir == wifi_direction_uplink) {
        hostapd_notif_auth(hapd, &event.auth);
        //return ieee802_11_mgmt(&g_hapd_glue[ap_index].hapd, frame, frame_len, NULL);
    } else if (dir == wifi_direction_downlink) {
        //ieee802_11_mgmt_cb(&g_hapd_glue[ap_index].hapd, frame, frame_len, WLAN_FC_STYPE_AUTH, 1);
        //return;
    }
    return 0;
}

/* Description:
 *      The API is used to receive EAPOL frames from data plane and forward to
 *      lib hostapd authenticator.
 * Arguments:
 *      ap_index - Index of Vap in which frame is received
 *      sta - client station mac-address.
 *      data - EAPOL data frames.
 *      data_len - EAPOL data length
 *
 */
int hapd_process_eapol_frame(unsigned int ap_index, mac_address_t sta, unsigned char *data, unsigned int data_len)
{
#if defined (_XB7_PRODUCT_REQ_)
    struct hostapd_data *hapd = g_hapd_glue[ap_index].hapd;
#else
    struct hostapd_data *hapd = &g_hapd_glue[ap_index].hapd;
#endif

    ieee802_1x_receive(hapd, sta, data, data_len);
    return 0;
}

//Thread for calling hostap eloop_run_thread
/* Description:
 *      The API is used start the eloop after all the eloop registered timer.
 *      Make sure it is called only once for any number of VAP(s) enabled.
 * Arguments: None
 */
static void* eloop_run_thread(void *data)
{
    eloop_run();
    wpa_printf(MSG_INFO,"%s:%d: Started eloop mechanism\n", __func__, __LINE__);
    pthread_detach(pthread_self());
    pthread_exit(0);
}

/* Description:
 *      The API is used to create thread for starting the eloop after
 *      all the eloop registered timer.
 * Arguments: None
 */
void hapd_wpa_run()
{
    pthread_t tid;
    if (pthread_create(&tid, NULL, eloop_run_thread, NULL))
    {
        wpa_printf(MSG_ERROR,"%s:%d: failing creating eloop run \n", __func__, __LINE__);
    }

}

void update_default_oem_configs(int apIndex, struct hostapd_bss_config *bss)
{
    cJSON *json = NULL, *jsonObj = NULL, *jsonData = NULL;
    FILE *fp = NULL;
    char *data = NULL;
    int len = 0;

    fp = fopen("/usr/ccsp/wifi/LibHostapdConfigFile.json", "r");
    if (fp != NULL)
    {
        fseek( fp, 0, SEEK_END );
        len = ftell(fp);
        fseek(fp, 0, SEEK_SET );

        data = ( char* )malloc( sizeof(char) * (len + 1) );
        if (!data)
        {
            wpa_printf(MSG_ERROR, "%s:%d Unable to allocate memory of len %d", __func__, __LINE__, len);
            fclose(fp);
            return;
        }
        memset( data, 0, ( sizeof(char) * (len + 1) ));
        fread( data, 1, len, fp);

        fclose(fp);

        if (strlen(data) != 0)
        {
            json = cJSON_Parse(data);
            if (!json)
            {
                wpa_printf(MSG_ERROR, "%s:%d Unable to parse JSON data", __func__, __LINE__);
                free(data);
                return;
            }
            /* WPS Configs */
            jsonObj = cJSON_GetObjectItem(json, "WPS");
            if (jsonObj != NULL)
            {
                jsonData = cJSON_GetObjectItem(jsonObj, "device_name");
                bss->device_name = strdup(jsonData ? jsonData->valuestring : "RDKB");

                jsonData = cJSON_GetObjectItem(jsonObj, "manufacturer");
                bss->manufacturer = strdup(jsonData ? jsonData->valuestring : "RDKB XB Communications, Inc.");

                jsonData = cJSON_GetObjectItem(jsonObj, "model_name");
                bss->model_name = strdup(jsonData ? jsonData->valuestring : "APxx");

                jsonData = cJSON_GetObjectItem(jsonObj, "model_number");
                bss->model_number = strdup(jsonData ? jsonData->valuestring : "APxx-xxx");

                jsonData = cJSON_GetObjectItem(jsonObj, "serial_number");
                bss->serial_number = strdup(jsonData ? jsonData->valuestring : "000000");

                jsonData = cJSON_GetObjectItem(jsonObj, "device_type");
                if (!jsonData || wps_dev_type_str2bin(jsonData->valuestring, bss->device_type))
                {
                    wpa_printf(MSG_ERROR,"Error in device type configs - %d\n", __LINE__);
                    return;
                }

                jsonData = cJSON_GetObjectItem(jsonObj, "friendly_name");
                bss->friendly_name = strdup(jsonData ? jsonData->valuestring : "RDKBxx");

                jsonData = cJSON_GetObjectItem(jsonObj, "manufacturer_url");
                bss->manufacturer_url = strdup(jsonData ? jsonData->valuestring : "http://manufacturer.url.here");

                jsonData = cJSON_GetObjectItem(jsonObj, "model_description");
                bss->model_description = strdup(jsonData ? jsonData->valuestring : "Model description here");

                jsonData = cJSON_GetObjectItem(jsonObj, "model_url");
                bss->model_url = strdup(jsonData ? jsonData->valuestring : "http://model.url.here");
            }

#if !defined(_XB7_PRODUCT_REQ_)
            jsonObj = cJSON_GetObjectItem(json, "BSS");
            if (jsonObj != NULL)
            {
#if defined (FEATURE_SUPPORT_RADIUSGREYLIST)
                jsonData = cJSON_GetObjectItem(jsonObj, "ap_vlan");
                if (jsonData != NULL && cJSON_GetArraySize(jsonData))
                    bss->ap_vlan = cJSON_GetArrayItem(jsonData, apIndex)->valueint;
#endif /* FEATURE_SUPPORT_RADIUSGREYLIST */

                jsonData = cJSON_GetObjectItem(jsonObj, "bridge");
                if (jsonData != NULL && cJSON_GetArraySize(jsonData))
                    snprintf(bss->bridge, IFNAMSIZ + 1, "%s", cJSON_GetArrayItem(jsonData, apIndex)->valuestring);
            }
            else
            {
                wpa_printf(MSG_ERROR, "%s:%d Unable to Parse sub object item\n", __func__, __LINE__);
            }
#endif //_XB7_PRODUCT_REQ_
            cJSON_Delete(json);
        }
        free(data);
        data = NULL;

    } else
        wpa_printf(MSG_ERROR, "%s:%d Unable to open config file", __func__, __LINE__);
}

/* Description:
 *      The API is used to init default values for radius params (auth and acct server)
 *      only if iee802_1x is enabled..
 * Arguments:
 *      conf - Allocated bss config struct
 */
void update_radius_config(struct hostapd_bss_config *conf)
{
     if (conf->radius == NULL) {
         //radius configuration call this as radius_init in glue.c
         conf->radius = malloc(sizeof(struct hostapd_radius_servers));

         memset(conf->radius, '\0', sizeof(struct hostapd_radius_servers));
     }

#if !defined(_XB7_PRODUCT_REQ_)
     if (conf->ieee802_1x)
     {
#endif
	conf->radius->num_auth_servers = 1;

	struct hostapd_radius_server *pri_auth_serv = NULL, *sec_auth_serv = NULL;
	char *auth_serv_addr = "127.0.0.1";
	//authentication server
	pri_auth_serv = malloc(sizeof(struct hostapd_radius_server));
	if (inet_aton(auth_serv_addr, &pri_auth_serv->addr.u.v4)) {
	        pri_auth_serv->addr.af = AF_INET;
	}
	pri_auth_serv->port = 1812;
	pri_auth_serv->shared_secret = (u8 *) os_strdup("radius");
	pri_auth_serv->shared_secret_len = os_strlen("radius");
	conf->radius->auth_server = pri_auth_serv;

	sec_auth_serv = malloc(sizeof(struct hostapd_radius_server));
	if (inet_aton(auth_serv_addr, &sec_auth_serv->addr.u.v4)) {
	        sec_auth_serv->addr.af = AF_INET;
	}
	sec_auth_serv->port = 1812;
	sec_auth_serv->shared_secret = (u8 *) os_strdup("radius");
	sec_auth_serv->shared_secret_len = os_strlen("radius");
	conf->radius->auth_servers =sec_auth_serv;

	conf->radius->msg_dumps = 1;

	//accounting server
	conf->radius->num_acct_servers = 0;
/* Account servers not needed as AAA server doesn't handle RADIUS accounting packets */
#if 0
	accnt_serv = malloc(sizeof(struct hostapd_radius_server));
	if (inet_aton(accnt_serv_addr, &accnt_serv->addr.u.v4)) {
	        accnt_serv->addr.af = AF_INET;
	}
	accnt_serv->port = 1813;
	accnt_serv->shared_secret = (u8 *) os_strdup("radius");
	accnt_serv->shared_secret_len = os_strlen("radius");
	conf->radius->acct_server = accnt_serv;
	conf->radius->acct_servers = accnt_serv;
#endif
	conf->radius->force_client_addr =0;
#if !defined(_XB7_PRODUCT_REQ_)
    }
#endif
}

/* Description:
 *      Construct key_mgmt value from TR-181 cache for
 *      saving it in hostap authenticator.
 * Arguments:
 *      encryptionMethod - TR-181 cache value.
 *      Device.WiFi.AccessPoint.{i}.Security.ModeEnabled.
 *
 */
static int hostapd_tr181_config_parse_key_mgmt(int modeEnabled)
{
    char *conf_value = NULL;
    int val = 0;

    switch(modeEnabled)
    {
        case COSA_DML_WIFI_SECURITY_WPA2_Personal:
        case COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal:
        {
            conf_value = "WPA-PSK";
            break;
        }
        case COSA_DML_WIFI_SECURITY_WPA2_Enterprise:
        case COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise:
        {
            conf_value = "WPA-EAP";
            break;
        }
        case COSA_DML_WIFI_SECURITY_None:
        {
            conf_value = "NONE";
            break;
        }
        case COSA_DML_WIFI_SECURITY_WPA3_Personal:
        case COSA_DML_WIFI_SECURITY_WPA3_Personal_Transition:
        {
            conf_value = "WPA-PSK SAE";
            break;
        }
        case COSA_DML_WIFI_SECURITY_WPA3_Enterprise:
        {
            conf_value = "WPA-EAP SAE";
            break;
        }
    }

    if (os_strcmp(conf_value, "WPA-PSK") == 0)
        val |= WPA_KEY_MGMT_PSK;
    else if (os_strcmp(conf_value, "WPA-EAP") == 0)
        val |= WPA_KEY_MGMT_IEEE8021X;
#ifdef CONFIG_IEEE80211R
//Defined
    else if (os_strcmp(conf_value, "FT-PSK") == 0)
        val |= WPA_KEY_MGMT_FT_PSK;
    else if (os_strcmp(conf_value, "FT-EAP") == 0)
        val |= WPA_KEY_MGMT_FT_IEEE8021X;
#endif /* CONFIG_IEEE80211R */
#ifdef CONFIG_IEEE80211W
//Defined
    else if (os_strcmp(conf_value, "WPA-PSK-SHA256") == 0)
        val |= WPA_KEY_MGMT_PSK_SHA256;
    else if (os_strcmp(conf_value, "WPA-EAP-SHA256") == 0)
         val |= WPA_KEY_MGMT_IEEE8021X_SHA256;
#endif /* CONFIG_IEEE80211W */
#if defined (CONFIG_SAE) && defined (WIFI_HAL_VERSION_3)
    else if (os_strcmp(conf_value, "WPA-PSK SAE") == 0) {
         val |= WPA_KEY_MGMT_PSK;
         val |= WPA_KEY_MGMT_SAE;
    }
    else if (os_strcmp(conf_value, "WPA-EAP SAE") == 0) {
        val |= WPA_KEY_MGMT_IEEE8021X;
        val |= WPA_KEY_MGMT_SAE;
    }
    else if (os_strcmp(conf_value, "FT-SAE") == 0)
         val |= WPA_KEY_MGMT_FT_SAE;
#endif /* CONFIG_SAE */
#ifdef CONFIG_SUITEB
//Not Defined
    else if (os_strcmp(conf_value, "WPA-EAP-SUITE-B") == 0)
         val |= WPA_KEY_MGMT_IEEE8021X_SUITE_B;
#endif /* CONFIG_SUITEB */
#ifdef CONFIG_SUITEB192
//Not Defined
    else if (os_strcmp(conf_value, "WPA-EAP-SUITE-B-192") == 0)
         val |= WPA_KEY_MGMT_IEEE8021X_SUITE_B_192;
#endif /* CONFIG_SUITEB192 */
    else if (os_strcmp(conf_value, "NONE") == 0)
         val |= WPA_KEY_MGMT_NONE;
    else {
         wpa_printf(MSG_ERROR, "Line %d: invalid key_mgmt '%s'",
                     __LINE__, conf_value);
         return -1;
    }
    return val;
}

/* Description:
 *      Construct cipher value from TR-181 cache for
 *      saving it in hostap authenticator.
 * Arguments:
 *      encryptionMethod - TR-181 cache value.
 *      Device.WiFi.AccessPoint.{i}.Security.X_CISCO_COM_EncryptionMethod
 *
 */
static int hostapd_tr181_config_parse_cipher(int encryptionMethod)
{
    char *conf_value = NULL;

    switch(encryptionMethod)
    {
        case COSA_DML_WIFI_AP_SEC_TKIP:
            conf_value = "TKIP";
            break;
        case COSA_DML_WIFI_AP_SEC_AES:
            conf_value = "CCMP";
            break;
        case COSA_DML_WIFI_AP_SEC_AES_TKIP:
            conf_value = "TKIP CCMP";
            break;
        default:
            wpa_printf(MSG_ERROR, "Wrong encryption method configured\n");
            return -1;
    }

    int val = wpa_parse_cipher(conf_value);
    if (val < 0) {
        wpa_printf(MSG_ERROR, "Line %d: invalid cipher '%s'.",
                   __LINE__, conf_value);
        return -1;
    }
    if (val == 0) {
        wpa_printf(MSG_ERROR, "Line %d: no cipher values configured.",
                   __LINE__);
        return -1;
    }
    return val;
}

/* Description:
 *      Delete all WPS config allocated sources during config parser of hostapd init.
 *      This API will only deinit hostapd_bss_config WPS configuration,
 * Arguments:
 *      conf - Allocated hostapd_bss_config structure.
 *
 * Note: Init all the source to NULL after freed.
 */
void hostapd_config_free_wps(struct hostapd_bss_config *conf)
{

#ifdef CONFIG_WPS
//Defined
    os_free(conf->device_name);
    conf->device_name = NULL;

    os_free(conf->manufacturer);
    conf->manufacturer = NULL;

    os_free(conf->model_name);
    conf->model_name = NULL;

    os_free(conf->model_number);
    conf->model_number = NULL;

    os_free(conf->serial_number);
    conf->serial_number = NULL;

    os_free(conf->config_methods);
    conf->config_methods = NULL;

    os_free(conf->ap_pin);
    conf->ap_pin = NULL;

    os_free(conf->friendly_name);
    conf->friendly_name = NULL;

    os_free(conf->manufacturer_url);
    conf->manufacturer_url = NULL;

    os_free(conf->model_description);
    conf->model_description = NULL;

    os_free(conf->model_url);
    conf->model_url = NULL;
#endif //CONFIG_WPS
}

/* Description:
 *      Convert authentication mode config string in TR-181 cache to integer config for
 *      saving it in hostap authenticator.
 * Arguments:
 *      modeEnabled - TR-181 cache value.
 *      Device.WiFi.AccessPoint.1.Security.ModeEnabled
 *
 */
static int hostapd_tr181_wpa_update(int modeEnabled)
{
    switch(modeEnabled)
    {
        case COSA_DML_WIFI_SECURITY_WPA2_Personal:
        case COSA_DML_WIFI_SECURITY_WPA2_Enterprise:
        case COSA_DML_WIFI_SECURITY_WPA3_Personal:
        case COSA_DML_WIFI_SECURITY_WPA3_Personal_Transition:
        {
            return 2;
        }
        case COSA_DML_WIFI_SECURITY_WPA_WPA2_Personal:
        case COSA_DML_WIFI_SECURITY_WPA_WPA2_Enterprise:
        case COSA_DML_WIFI_SECURITY_WPA3_Enterprise:
        {
            return 3;
        }
        case COSA_DML_WIFI_SECURITY_None:
        {
            return 0;
        }
    }
    return -1;
}

/* Description:
 *      Convert MFP config string in TR-181 cache to integer config for
 *      saving it in hostap authenticator.
 * Arguments:
 *      mfpConfig - TR-181 cache value.
 *      Device.WiFi.AccessPoint.{i}.Security.MFPConfig
 *
 */
static int hostapd_tr181_update_mfp_config(char *mfpConfig)
{
    if (strcmp(mfpConfig, "Optional") == 0)
        return  MGMT_FRAME_PROTECTION_OPTIONAL;
    else if (strcmp(mfpConfig, "Required") == 0)
        return MGMT_FRAME_PROTECTION_REQUIRED;

    return NO_MGMT_FRAME_PROTECTION;
}

/* Description:
 *      Delete all allocated sources during config parser of hostapd init.
 *      This API will only deinit hostapd_bss_config conf structure,
 * Arguments:
 *      conf - Allocated hostapd_bss_config structure.
 *
 * Note: Init all the source to NULL after freed.
 */
void hostapd_config_free_bss(struct hostapd_bss_config *conf)
{
#if 0
    struct hostapd_eap_user *user, *prev_user;
#endif
    if (conf == NULL)
            return;

    wpa_printf(MSG_INFO,"%s - %d Start of Deinit API \n", __func__, __LINE__);
    hostapd_config_clear_wpa_psk(&conf->ssid.wpa_psk);

    if (conf->ssid.wpa_passphrase)
    {
        str_clear_free(conf->ssid.wpa_passphrase);
        conf->ssid.wpa_passphrase = NULL;
        conf->ssid.wpa_passphrase_set = 0;
    }

    if (conf->ssid.wpa_psk_file)
    {
        os_free(conf->ssid.wpa_psk_file);
        conf->ssid.wpa_psk_file = NULL;
    }
#if HOSTAPD_VERSION >= 210
#ifdef CONFIG_WEP
    hostapd_config_free_wep(&conf->ssid.wep);
#endif
#else
     hostapd_config_free_wep(&conf->ssid.wep);
#endif
    os_free(conf->ctrl_interface);
    conf->ctrl_interface = NULL;
#if 0
/* We are not currently using it, might be needing it for future purpose */
    user = conf->eap_user;
    while (user) {
            prev_user = user;
            user = user->next;
            hostapd_config_free_eap_user(prev_user);
    }
    os_free(conf->eap_user_sqlite);

    os_free(conf->eap_req_id_text);
    os_free(conf->erp_domain);
    os_free(conf->accept_mac);
    os_free(conf->deny_mac);
    os_free(conf->rsn_preauth_interfaces);
#endif
    if (conf->nas_identifier != NULL)
    {
        os_free(conf->nas_identifier);
        conf->nas_identifier = NULL;
    }

    if (conf->radius) {
        hostapd_config_free_radius(conf->radius->auth_servers,
                                   conf->radius->num_auth_servers);
/* Account servers not needed as AAA server doesn't handle RADIUS accounting packets */
#if 0
        hostapd_config_free_radius(conf->radius->acct_servers,
                                   conf->radius->num_acct_servers);
#endif
        if (conf->radius_auth_req_attr)
            hostapd_config_free_radius_attr(conf->radius_auth_req_attr);
        if (conf->radius_acct_req_attr)
            hostapd_config_free_radius_attr(conf->radius_acct_req_attr);
        conf->radius_auth_req_attr = NULL;
        conf->radius_acct_req_attr = NULL;
        os_free(conf->radius_server_clients);
        os_free(conf->radius);
        conf->radius_server_clients = NULL;
        conf->radius = NULL;
    }

    hostapd_config_free_wps(conf);
    wpa_printf(MSG_INFO,"%s - %d End of Deinit API \n", __func__, __LINE__);
}

/* Description:
 *      Update WPS related TR-181 cache to hostapd_bss_config structure of hostap authenticator.
 * Arguments:
 *      ap_index   Index of Vap for which default config and init has to be updated.
 *      pWifiAp    Device.WiFi.AccessPoint.{i} cache structure.
 */
int hapd_update_wps_config(int apIndex, PCOSA_DML_WIFI_AP pWifiAp)
{
    struct hostapd_data *hapd;
    struct hostapd_bss_config *bss;

#if defined (_XB7_PRODUCT_REQ_)
    hapd = g_hapd_glue[apIndex].hapd;
#else
    hapd = &g_hapd_glue[apIndex].hapd;
#endif /*_XB7_PRODUCT_REQ_ */
    bss = hapd->conf;

#ifdef CONFIG_WPS
//Defined
    wpa_printf(MSG_DEBUG,"Start WPS configurations %p - %p\n", hapd, bss);
#if defined (_XB7_PRODUCT_REQ_)
    if (pWifiAp->WPS.Cfg.ConfigMethodsEnabled == COSA_DML_WIFI_WPS_METHOD_PushButton) {
        bss->config_methods = os_strdup("display virtual_push_button physical_push_button push_button virtual_display");
#else
    if (pWifiAp->WPS.Cfg.ConfigMethodsEnabled == COSA_DML_WIFI_WPS_METHOD_PushButton) {
        bss->config_methods = os_strdup("push_button");
#endif
    } else  if (pWifiAp->WPS.Cfg.ConfigMethodsEnabled == COSA_DML_WIFI_WPS_METHOD_Pin) {
        bss->config_methods = os_strdup("keypad label display");
    } else if (pWifiAp->WPS.Cfg.ConfigMethodsEnabled == (COSA_DML_WIFI_WPS_METHOD_PushButton|COSA_DML_WIFI_WPS_METHOD_Pin) ) {
        bss->config_methods = os_strdup("label display push_button keypad");
    }
    bss->ap_pin = os_strdup(pWifiAp->WPS.Info.X_CISCO_COM_Pin);
#if defined (_XB7_PRODUCT_REQ_)
    bss->wps_cred_processing = 2;
#else
    bss->wps_cred_processing = 1;
#endif
    bss->pbc_in_m1 = 1;
#endif /* CONFIG_WPS */
    return 0;
}

/* Description:
 *      Update TR-181 cache to hostapd_bss_config structure of hostap authenticator.
 * Arguments:
 *      ap_index        Index of Vap for which default config and init has to be updated.
 *      bss             Allocated hostapd_bss_config struct pointer.
 *      pWifiAp         Device.WiFi.AccessPoint.{i} cache structure.
 *      pWifiSsid       Device.WiFi.SSID.{i} cache structure.
 *      pWifiRadioFull  Device.WiFi.Radio.{i} cache structure.
 */
int update_tr181_config_param(int ap_index, struct hostapd_bss_config *bss, PCOSA_DATAMODEL_WIFI pWifi, PCOSA_DML_WIFI_SSID pWifiSsid, PCOSA_DML_WIFI_AP pWifiAp, PCOSA_DML_WIFI_RADIO_FULL pWifiRadioFull)
{
#if defined (_XB7_PRODUCT_REQ_)
    char ifname[8] = {0};
    convert_apindex_to_interface(ap_index, ifname, sizeof(ifname));
    os_strlcpy(bss->iface, ifname, sizeof(bss->iface));

    const char *strings[] = { "brlan0", "brlan0", "brlan1", "brlan1", "brlan2", "brlan3", "br106", "br106", "brlan4", "brlan5", "br106", "br106", "brlan112", "brlan113", "brlan1", "brlan1" };
    snprintf(bss->bridge, IFNAMSIZ + 1, strings[ap_index]);
    wpa_printf(MSG_ERROR, "%s:%d apIndex:%d iface:%s bridge:%s", __func__, __LINE__, ap_index, ifname, bss->bridge);

   char MACAddress[18] = {0};
   snprintf(MACAddress, sizeof(MACAddress), "%02x:%02x:%02x:%02x:%02x:%02x", pWifiSsid->SSID.StaticInfo.BSSID[0], pWifiSsid->SSID.StaticInfo.BSSID[1], pWifiSsid->SSID.StaticInfo.BSSID[2], pWifiSsid->SSID.StaticInfo.BSSID[3], pWifiSsid->SSID.StaticInfo.BSSID[4], pWifiSsid->SSID.StaticInfo.BSSID[5]);
   if (!(hwaddr_aton(MACAddress, bss->bssid)))
        wpa_printf(MSG_DEBUG, "BSSID -" MACSTR "\n", MAC2STR(bss->bssid));

#else
    snprintf(bss->iface, sizeof(bss->iface), "ath%d", ap_index);
    wpa_printf(MSG_DEBUG, "%s:%d: apIndex:%d iface:%s\n", __func__, __LINE__, ap_index, bss->iface);
#endif


    errno_t rc = -1;
    rc = sprintf_s(bss->vlan_bridge, sizeof(bss->vlan_bridge) , "vlan%d", ap_index);
    if(rc < EOK)
    {
        ERR_CHK(rc);
    }

    bss->ctrl_interface = strdup("/var/run/hostapd");
    bss->ctrl_interface_gid_set = 1;

    //ssid
    {
        rc = strcpy_s((char *)bss->ssid.ssid, sizeof(bss->ssid.ssid) , pWifiSsid->SSID.Cfg.SSID);
        ERR_CHK(rc);
        bss->ssid.ssid_len = strlen((const char *)bss->ssid.ssid);

        if (!bss->ssid.ssid_len)
            bss->ssid.ssid_set = 0;
        else
            bss->ssid.ssid_set = 1;
        bss->ssid.utf8_ssid = 0;
    }
    //dtim_period
    bss->dtim_period = pWifiRadioFull->Cfg.DTIMInterval;
    //max_num_sta
    bss->max_num_sta = pWifiAp->AP.Cfg.BssMaxNumSta;
    //macaddr_acl
    //bss->macaddr_acl = pWifiAp->MF.bEnabled;
    bss->macaddr_acl = 0; //ACL will be handled by driver, '0' - ACCEPT_UNLESS_DENIED

    //ignore_broadcast_ssid
    bss->ignore_broadcast_ssid = !pWifiAp->AP.Cfg.SSIDAdvertisementEnabled;

    bss->wps_state = pWifiAp->WPS.Cfg.bEnabled ? WPS_STATE_CONFIGURED : 0;
//    bss->wps_state = 0;
    if (bss->wps_state && bss->ignore_broadcast_ssid) {
        bss->wps_state = 0;
    }

    //wme_enabled
    bss->wmm_enabled = pWifiAp->AP.Cfg.WMMEnable;

    /* bss_transition */
    bss->bss_transition = pWifiAp->AP.Cfg.BSSTransitionActivated;

#if defined (FEATURE_SUPPORT_RADIUSGREYLIST)
    /* Grey listing */
    bss->rdk_greylist = pWifi->bEnableRadiusGreyList;
#endif

    //ieee8021x
    bss->wpa_key_mgmt = hostapd_tr181_config_parse_key_mgmt(pWifiAp->SEC.Cfg.ModeEnabled);
#ifdef CONFIG_IEEE80211W
//Defined
    bss->ieee80211w = hostapd_tr181_update_mfp_config(pWifiAp->SEC.Cfg.MFPConfig);
#endif
#if defined (_XB7_PRODUCT_REQ_)
    if (bss->ieee80211w == MGMT_FRAME_PROTECTION_REQUIRED)
    {
        switch (bss->wpa_key_mgmt)
        {
             case WPA_KEY_MGMT_IEEE8021X:
             case WPA_KEY_MGMT_FT_IEEE8021X:
             case WPA_KEY_MGMT_IEEE8021X_SHA256:
             {
                   bss->wpa_key_mgmt = WPA_KEY_MGMT_IEEE8021X_SHA256;
                   break;
             }
             case WPA_KEY_MGMT_PSK:
             case WPA_KEY_MGMT_PSK_SHA256:
             case WPA_KEY_MGMT_FT_PSK:
             {
                   bss->wpa_key_mgmt = WPA_KEY_MGMT_PSK_SHA256;
                   break;
             }
             case (WPA_KEY_MGMT_PSK | WPA_KEY_MGMT_SAE):
             {
                   bss->wpa_key_mgmt = (WPA_KEY_MGMT_PSK_SHA256 | WPA_KEY_MGMT_SAE);
                   break;
             }
             case (WPA_KEY_MGMT_IEEE8021X | WPA_KEY_MGMT_SAE):
             {
                   bss->wpa_key_mgmt = (WPA_KEY_MGMT_IEEE8021X_SHA256 | WPA_KEY_MGMT_SAE);
                   break;
             }
             default:
             {
                   bss->wpa_key_mgmt = WPA_KEY_MGMT_PSK_SHA256;
                   break;
             }
        }
    }
#endif /* _XB7_PRODUCT_REQ_ */

    if (bss->wpa_key_mgmt != -1)
    {
        switch (bss->wpa_key_mgmt)
        {
            case WPA_KEY_MGMT_PSK:
            case WPA_KEY_MGMT_FT_PSK:
            case WPA_KEY_MGMT_PSK_SHA256:
            case (WPA_KEY_MGMT_PSK | WPA_KEY_MGMT_SAE):
            case (WPA_KEY_MGMT_PSK_SHA256 | WPA_KEY_MGMT_SAE):
            {
                bss->ieee802_1x = 0;
                bss->eap_server = 1;
                break;
            }
            case WPA_KEY_MGMT_IEEE8021X:
            case WPA_KEY_MGMT_FT_IEEE8021X:
            case WPA_KEY_MGMT_IEEE8021X_SHA256:
            case (WPA_KEY_MGMT_IEEE8021X | WPA_KEY_MGMT_SAE):
            case (WPA_KEY_MGMT_IEEE8021X_SHA256 | WPA_KEY_MGMT_SAE):
            {
                bss->ieee802_1x = 1;
                bss->eap_server = 0;
                break;
            }
            case WPA_KEY_MGMT_NONE:
            {
                bss->ieee802_1x = 1;
                bss->eap_server = 0;
                break;
            }
            default:
                bss->ieee802_1x = 1;
                bss->eap_server = 1;
                break;
        }
        wpa_printf(MSG_DEBUG,"wpa_key_mgmt - %d ieee802_1x - %d", bss->wpa_key_mgmt, bss->ieee802_1x);
    }
    else
    {
        bss->wpa = 0;
        wpa_printf(MSG_ERROR,"%s:%d wpa_key_mgmt config update failed", __func__, __LINE__);
        return 0;
    }

    bss->wpa = hostapd_tr181_wpa_update(pWifiAp->SEC.Cfg.ModeEnabled);
    //wpa_pairwise
    bss->wpa_pairwise = hostapd_tr181_config_parse_cipher(pWifiAp->SEC.Cfg.EncryptionMethod);
    wpa_printf(MSG_DEBUG,"wpa - %d wpa_pairwise - %d", bss->wpa, bss->wpa_pairwise);

    //wpa_group_rekey
    if (ap_index == 10 || ap_index == 11)
    {
        if (pWifiAp->SEC.Cfg.RekeyingInterval)
            bss->wpa_gmk_rekey = pWifiAp->SEC.Cfg.RekeyingInterval;
    }
    else
    {
        bss->wpa_group_rekey = pWifiAp->SEC.Cfg.RekeyingInterval;
    }

    //wpa_strict_rekey
    bss->wpa_strict_rekey = 0;
    /* For ap Index 12 and 13, strict rekey is set as '1' in the hostapd.conf */
    if (ap_index == 12 || ap_index == 13)
        bss->wpa_strict_rekey = 1;

#if !defined(_XB7_PRODUCT_REQ_) //remove if any EAP patch is added for xb7
    // # EAP/EAPOL custom timeout and retry values
    {
        bss->rdkb_eapol_key_timeout = pWifiAp->SEC.Cfg.uiEAPOLKeyTimeout;
        bss->rdkb_eapol_key_retries = pWifiAp->SEC.Cfg.uiEAPOLKeyRetries;
        bss->rdkb_eapidentity_request_timeout = pWifiAp->SEC.Cfg.uiEAPIdentityRequestTimeout;
        bss->rdkb_eapidentity_request_retries = pWifiAp->SEC.Cfg.uiEAPIdentityRequestRetries;
        bss->rdkb_eap_request_timeout = pWifiAp->SEC.Cfg.uiEAPRequestTimeout;
        bss->rdkb_eap_request_retries = pWifiAp->SEC.Cfg.uiEAPRequestRetries;
    }
#endif

    if (bss->ieee802_1x)
    {
        //radius_retry_primary_interval
        //disable_pmksa_caching
        bss->disable_pmksa_caching = pWifiAp->AP.RadiusSetting.bPMKCaching;

        if (bss->ieee802_1x && !bss->eap_server && !bss->radius->auth_servers) {
            wpa_printf(MSG_ERROR,"Invalid IEEE 802.1X configuration (no EAP authenticator configured).");
            return -1;
        }

        char output[256] = {0};
        _syscmd("sh /usr/sbin/deviceinfo.sh -eip", output, 256);
    
        //own_ip_addr
        if (inet_aton(output, &bss->own_ip_addr.u.v4)) {
            bss->own_ip_addr.af = AF_INET;
        }
        // nas_identifier=
        {
            memset(output, '\0', sizeof(output));
            _syscmd("sh /usr/sbin/deviceinfo.sh -emac", output, 256);
            if (output[strlen(output) - 1] == '\n')
            {
                output[strlen(output) - 1] = '\0';
            }

            wpa_printf(MSG_DEBUG,"%s-%d Updating NAS identifier %s\n", __func__, __LINE__, output);
            bss->nas_identifier = strdup(output);
#ifdef CISCO_XB3_PLATFORM_CHANGES
                //# Called Station ID: RG_MAC:SSID_name
                //~eROUTER_MACADDR#:~radius_acct_req_attr=30:s:~!ROUTER_MACADDR~:~!AP_SSID#~
                //radius_acct_req_attr

            memset(output, '\0', sizeof(output));
            snprintf(output, sizeof(output), "30:s:%s:%s", bss->nas_identifier, bss->ssid.ssid);
            bss->radius_acct_req_attr = hostapd_parse_radius_attr(output);
            bss->radius_auth_req_attr = hostapd_parse_radius_attr(output);
#endif //CISCO_XB3_PLATFORM_CHANGES
       }

       //auth_server_addr
       /* Primary Server */
       {
           if (inet_aton((const char *)pWifiAp->SEC.Cfg.RadiusServerIPAddr, &bss->radius->auth_servers->addr.u.v4)) {
               bss->radius->auth_servers->addr.af = AF_INET;
           }
           bss->radius->auth_servers->port = pWifiAp->SEC.Cfg.RadiusServerPort;
           if (pWifiAp->SEC.Cfg.RadiusServerPort && pWifiAp->SEC.Cfg.RadiusSecret)
           {
               //auth_server_shared_secret
               bss->radius->auth_servers->shared_secret = (u8 *) os_strdup(pWifiAp->SEC.Cfg.RadiusSecret);
               bss->radius->auth_servers->shared_secret_len = os_strlen((const char *)bss->radius->auth_servers->shared_secret);
           }
       }

       /* Secondary Server */
       {
           if (inet_aton((const char *)pWifiAp->SEC.Cfg.SecondaryRadiusServerIPAddr, &bss->radius->auth_server->addr.u.v4)) {
               bss->radius->auth_server->addr.af = AF_INET;
           }
           //auth_server_port
           bss->radius->auth_server->port = pWifiAp->SEC.Cfg.SecondaryRadiusServerPort;
    
           if (pWifiAp->SEC.Cfg.SecondaryRadiusServerPort && pWifiAp->SEC.Cfg.SecondaryRadiusSecret)
           {
               //auth_server_shared_secret
               bss->radius->auth_server->shared_secret = (u8 *) os_strdup(pWifiAp->SEC.Cfg.SecondaryRadiusSecret);
               bss->radius->auth_server->shared_secret_len = os_strlen((const char *)bss->radius->auth_server->shared_secret);
           }
       }

       // # EAP/EAPOL custom timeout and retry values
#if !defined(_XB7_PRODUCT_REQ_)        //remove if any radius patch is added for xb7
       bss->max_auth_attempts = pWifiAp->AP.RadiusSetting.iMaxAuthenticationAttempts;
       bss->blacklist_timeout = pWifiAp->AP.RadiusSetting.iBlacklistTableTimeout;
    
       if (pWifiAp->AP.RadiusSetting.iIdentityRequestRetryInterval)
           bss->identity_request_retry_interval = pWifiAp->AP.RadiusSetting.iIdentityRequestRetryInterval;
    
       if (pWifiAp->AP.RadiusSetting.iRadiusServerRetries)
           bss->radius->radius_server_retries = pWifiAp->AP.RadiusSetting.iRadiusServerRetries;
#endif
#if defined (FEATURE_SUPPORT_RADIUSGREYLIST)
       //Radius DAS settings
       bss->radius_das_port = pWifiAp->SEC.Cfg.RadiusDASPort;
       if (pWifiAp->SEC.Cfg.RadiusDASPort && pWifiAp->SEC.Cfg.RadiusDASSecret && strlen(pWifiAp->SEC.Cfg.RadiusDASSecret))
       {
           bss->radius_das_shared_secret = (u8 *) os_strdup(pWifiAp->SEC.Cfg.RadiusDASSecret);
           bss->radius_das_shared_secret_len = os_strlen((const char *)bss->radius_das_shared_secret);
       }

       if (strcmp((char *)pWifiAp->SEC.Cfg.RadiusDASIPAddr, "0.0.0.0") != 0)
       {
           if (inet_aton((const char *)pWifiAp->SEC.Cfg.RadiusDASIPAddr, &bss->radius_das_client_addr.u.v4)) {
               bss->radius_das_client_addr.af = AF_INET;
           }
       }
#endif /* FEATURE_SUPPORT_RADIUSGREYLIST */
    } //if(ieee8021x)
    else
    {
        wpa_printf(MSG_DEBUG, "%s:%d: ### Updating the WPA key \n", __func__, __LINE__);
        //wpa_passphrase
        /* PASSPHRASE CHECK */
        if (os_strlen((const char *)pWifiAp->SEC.Cfg.KeyPassphrase) > 0)
        {
            int len = os_strlen((const char *)pWifiAp->SEC.Cfg.KeyPassphrase);
            if (len < 8 || len > 63) {
                wpa_printf(MSG_ERROR, "Line %d: invalid WPA passphrase length %d (expected 8..63)",
                           __LINE__, len);
                return 1;
            }

            bss->ssid.wpa_passphrase = strdup((const char *)pWifiAp->SEC.Cfg.KeyPassphrase);
            bss->ssid.wpa_passphrase_set = 1;
         }
         else
         {
             wpa_printf(MSG_ERROR, "Line %d: WPA passphrase cannot be empty\n", __LINE__);
             return -1;
         }
         //set osen
         bss->osen = 0;
    } //!ieee8021x

#ifdef CONFIG_HS20
//Not defined
    bss->rdk_hs20 = pWifiAp->AP.Cfg.IEEE80211uCfg.PasspointCfg.Status;
#endif

    if (bss->wps_state)
    {
        wpa_printf(MSG_DEBUG,"Start WPS configurations \n");
        return hapd_update_wps_config(ap_index, pWifiAp);
    }

    wpa_printf(MSG_ERROR, "%s:%d Exit", __func__, __LINE__);
    return 0;
}

/* Description:
 *      The API is used to create the ath%d interface for respective ap_index.
 * Arguments:
 *      ap_index - Index of Vap for which default config and init has to be updated.
 *      hostapd_data - Allocated hostapd_data parent struct for all config.
 *
 * Note: Hapd should have valid address before calling this API. ap_index should be
 *      0 - 15 only
 */
#if !defined(_XB7_PRODUCT_REQ_)
void driver_init(int ap_index, struct hostapd_data *hapd)
{
    struct wpa_init_params params;
    struct hostapd_bss_config *conf = hapd->conf;
    int radio_index = -1;

    u8 *b = conf->bssid;

    os_memset(&params, 0, sizeof(params));
    params.bssid = b;
    params.ifname = hapd->conf->iface;
    params.driver_params = hapd->iconf->driver_params;

    params.own_addr = hapd->own_addr;

    wifi_getApRadioIndex(ap_index, &radio_index);
    wifi_createAp(ap_index, radio_index, (char *)hapd->conf->ssid.ssid, hapd->conf->ignore_broadcast_ssid);
    hapd->drv_priv = hapd->driver->hapd_init(hapd, &params);
    os_free(params.bridge);
}
#endif
/* Description:
 *      The API is used to init default values for interface struct and max bss per vap
 *            hostapd_iface - conf
 * Arguments:
 *      ap_index - Index of Vap for which default config and init has to be updated.
 *      iface - Allocated hostapd_iface struct pointer
 *      hapd - Allocated hostapd_data struct pointer
 *
 * Note: iface should have valid address before calling this API. ap_index should be
 *      0 - 15 only
 */
void update_hostapd_iface(int ap_index, struct hostapd_iface *iface, struct hostapd_data *hapd)
{
    iface->conf = hapd->iconf;
    iface->num_bss = hapd->iconf->num_bss;
    iface->bss = os_calloc(hapd->iconf->num_bss, sizeof(struct hostapd_data *));
    iface->bss[0] = hapd;

    iface->drv_flags |= WPA_DRIVER_FLAGS_INACTIVITY_TIMER;
    iface->drv_flags |= WPA_DRIVER_FLAGS_DEAUTH_TX_STATUS;

    dl_list_init(&iface->sta_seen);
}

/* Description:
 *      The API is used to init default values for hostapd_config
 *            hostapd_config - conf
 *      beacon interval, TX/RX wmm queues, acs related configs
 * Arguments:
 *      ap_index - Index of Vap for which default config and init has to be updated.
 *      conf - Allocated hostapd_config struct pointer
 *
 * Note: conf should have valid address before calling this API. ap_index should be
 *      0 - 15 only
 */
void update_hostapd_iconf(int ap_index, struct hostapd_config *conf)
{
    conf->num_bss = 1;

    conf->beacon_int = 100;
    conf->rts_threshold = -2; /* use driver default: 2347 */
    conf->fragm_threshold = -2; /* user driver default: 2346 */
    /* Set to invalid value means do not add Power Constraint IE */
    conf->local_pwr_constraint = -1;

    conf->spectrum_mgmt_required = 0;
    const int aCWmin = 4, aCWmax = 10;
    const struct hostapd_wmm_ac_params ac_bk =
    { aCWmin, aCWmax, 7, 0, 0 }; /* background traffic */
    const struct hostapd_wmm_ac_params ac_be =
    { aCWmin, aCWmax, 3, 0, 0 }; /* best effort traffic */
    const struct hostapd_wmm_ac_params ac_vi = /* video traffic */
    { aCWmin - 1, aCWmin, 2, 3008 / 32, 0 };
    const struct hostapd_wmm_ac_params ac_vo = /* voice traffic */
    { aCWmin - 2, aCWmin - 1, 2, 1504 / 32, 0 };
    const struct hostapd_tx_queue_params txq_bk =
    { 7, ecw2cw(aCWmin), ecw2cw(aCWmax), 0 };
    const struct hostapd_tx_queue_params txq_be =
    { 3, ecw2cw(aCWmin), 4 * (ecw2cw(aCWmin) + 1) - 1, 0};
    const struct hostapd_tx_queue_params txq_vi =
    { 1, (ecw2cw(aCWmin) + 1) / 2 - 1, ecw2cw(aCWmin), 30};
    const struct hostapd_tx_queue_params txq_vo =
    { 1, (ecw2cw(aCWmin) + 1) / 4 - 1,
        (ecw2cw(aCWmin) + 1) / 2 - 1, 15};

    conf->wmm_ac_params[0] = ac_be;
    conf->wmm_ac_params[1] = ac_bk;
    conf->wmm_ac_params[2] = ac_vi;
    conf->wmm_ac_params[3] = ac_vo;

    conf->tx_queue[0] = txq_vo;
    conf->tx_queue[1] = txq_vi;
    conf->tx_queue[2] = txq_be;
    conf->tx_queue[3] = txq_bk;

    conf->ht_capab = HT_CAP_INFO_SMPS_DISABLED;

    conf->ap_table_max_size = 255;
    conf->ap_table_expiration_time = 60;
    conf->track_sta_max_age = 180;

    conf->acs = 0;
    conf->acs_ch_list.num = 0;
#ifdef CONFIG_ACS
//Not defined
    conf->acs_num_scans = 5;
#endif /* CONFIG_ACS */

#ifdef CONFIG_IEEE80211AX
//Not defined
    conf->he_op.he_rts_threshold = HE_OPERATION_RTS_THRESHOLD_MASK >>
        HE_OPERATION_RTS_THRESHOLD_OFFSET;
    /* Set default basic MCS/NSS set to single stream MCS 0-7 */
    conf->he_op.he_basic_mcs_nss_set = 0xfffc;
#endif /* CONFIG_IEEE80211AX */

    /* The third octet of the country string uses an ASCII space character
     * by default to indicate that the regulations encompass all
     * environments for the current frequency band in the country. */
#if defined (_XB7_PRODUCT_REQ_)
    snprintf(conf->country, sizeof(conf->country), "US");
#else
    wifi_getRadioCountryCode(ap_index, conf->country);
#endif

    conf->rssi_reject_assoc_rssi = 0;
    conf->rssi_reject_assoc_timeout = 30;

#ifdef CONFIG_AIRTIME_POLICY
//Not defined
    conf->airtime_update_interval = AIRTIME_DEFAULT_UPDATE_INTERVAL;
    conf->airtime_mode = AIRTIME_MODE_STATIC;
#endif /* CONFIG_AIRTIME_POLICY */
#if defined (_XB7_PRODUCT_REQ_)
    conf->ieee80211h = 1;
    conf->ieee80211d = 1;
#else
    conf->ieee80211h = 0;
    conf->ieee80211d = 0;
#endif
}

/* Description:
 *      The API is used to init default values for security and bss related configs
 *            hostapd_bss_config - conf
 *      This will also init queue/list for necessary STA connections.
 * Arguments:
 *      ap_index - Index of Vap for which default config and init has to be updated.
 *      bss - Allocated hostapd_bss_config struct pointer
 *
 * Note: bss should have valid address before calling this API. ap_index should be
 *      0 - 15 only
 */
void update_hostapd_bss_config(int ap_index, struct hostapd_bss_config *bss)
{
    //HOSTAPD_MODULE_IEEE80211|HOSTAPD_MODULE_IEEE8021X|HOSTAPD_MODULE_RADIUS|HOSTAPD_MODULE_WPA;

    /* Set to -1 as defaults depends on HT in setup */
    dl_list_init(&bss->anqp_elem);

    bss->logger_syslog_level = HOSTAPD_LEVEL_INFO;
    bss->logger_stdout_level = HOSTAPD_LEVEL_INFO;
    bss->logger_syslog =  -1;
    bss->logger_stdout =  -1;

    bss->auth_algs = WPA_AUTH_ALG_OPEN | WPA_AUTH_ALG_SHARED;
#if HOSTAPD_VERSION >= 210
#ifdef CONFIG_WEP
    bss->wep_rekeying_period = 300;
    /* use key0 in individual key and key1 in broadcast key */
    bss->broadcast_key_idx_min = 1;
    bss->broadcast_key_idx_max = 2;
#endif
#else
    bss->wep_rekeying_period = 300;
    /* use key0 in individual key and key1 in broadcast key */
    bss->broadcast_key_idx_min = 1;
    bss->broadcast_key_idx_max = 2;
#endif
    bss->eap_reauth_period = 3600;

    bss->wpa_group_rekey = 600;
    bss->wpa_gmk_rekey = 86400;
    bss->wpa_group_update_count = 4;
    bss->wpa_pairwise_update_count = 4;
    bss->wpa_disable_eapol_key_retries =
        DEFAULT_WPA_DISABLE_EAPOL_KEY_RETRIES;
    bss->wpa_key_mgmt = WPA_KEY_MGMT_PSK;
    bss->wpa_pairwise = WPA_CIPHER_TKIP;
    bss->wpa_group = WPA_CIPHER_TKIP;
    bss->rsn_pairwise = 0;

    bss->max_num_sta = MAX_STA_COUNT;

    bss->dtim_period = 2;

    bss->radius_server_auth_port = 1812;
#if !defined(_XB7_PRODUCT_REQ_)        //remove if any radius patch is added for xb7
    bss->radius->radius_server_retries = RADIUS_CLIENT_MAX_RETRIES;
    bss->radius->radius_max_retry_wait = RADIUS_CLIENT_MAX_WAIT;
#endif
    bss->eap_sim_db_timeout = 1;
    bss->eap_sim_id = 3;
    bss->ap_max_inactivity = AP_MAX_INACTIVITY;
    bss->eapol_version = EAPOL_VERSION;

    bss->max_listen_interval = 65535;

    bss->pwd_group = 19; /* ECC: GF(p=256) */

#ifdef CONFIG_IEEE80211W
//Defined
    bss->assoc_sa_query_max_timeout = 1000;
    bss->assoc_sa_query_retry_timeout = 201;
    bss->group_mgmt_cipher = WPA_CIPHER_AES_128_CMAC;
#endif /* CONFIG_IEEE80211W */
#ifdef EAP_SERVER_FAST
//Defined
    /* both anonymous and authenticated provisioning */
    bss->eap_fast_prov = 3;
    bss->pac_key_lifetime = 7 * 24 * 60 * 60;
    bss->pac_key_refresh_time = 1 * 24 * 60 * 60;
#endif /* EAP_SERVER_FAST */

    /* Set to -1 as defaults depends on HT in setup */
    bss->wmm_enabled = -1;

#ifdef CONFIG_IEEE80211R_AP
//Defined
    bss->ft_over_ds = 1;
    bss->rkh_pos_timeout = 86400;
    bss->rkh_neg_timeout = 60;
    bss->rkh_pull_timeout = 1000;
    bss->rkh_pull_retries = 4;
    bss->r0_key_lifetime = 1209600;
#endif /* CONFIG_IEEE80211R_AP */

    bss->radius_das_time_window = 300;
#if HOSTAPD_VERSION >= 210
    bss->anti_clogging_threshold = 5;
#else
    bss->sae_anti_clogging_threshold = 5;
#endif
    bss->sae_sync = 5;

    bss->gas_frag_limit = 1400;

#ifdef CONFIG_FILS
//Not Defined
    dl_list_init(&bss->fils_realms);
    bss->fils_hlp_wait_time = 30;
    bss->dhcp_server_port = DHCP_SERVER_PORT;
    bss->dhcp_relay_port = DHCP_SERVER_PORT;
#endif /* CONFIG_FILS */

    bss->broadcast_deauth = 1;

#ifdef CONFIG_MBO
//Not Defined
    bss->mbo_cell_data_conn_pref = -1;
#endif /* CONFIG_MBO */

    /* Disable TLS v1.3 by default for now to avoid interoperability issue.
     * This can be enabled by default once the implementation has been fully
     * completed and tested with other implementations. */
    bss->tls_flags = TLS_CONN_DISABLE_TLSv1_3;

    bss->send_probe_response = 1;

#ifdef CONFIG_HS20
//Not Defined
    bss->hs20_release = (HS20_VERSION >> 4) + 1;
#endif /* CONFIG_HS20 */

#ifdef CONFIG_MACSEC
//Not Defined
    bss->mka_priority = DEFAULT_PRIO_NOT_KEY_SERVER;
    bss->macsec_port = 1;
#endif /* CONFIG_MACSEC */

    /* Default to strict CRL checking. */
    bss->check_crl_strict = 1;
}

/* Description:
 *      The API is used to init default values for following hostapd stuctures.
 *            hostapd_config - iconf
 *            hostapd_bss_config - conf
 *            hostapd_iface - iface
 *      This will also init queue/list for necessary STA connections.
 * Arguments:
 *      ap_index - Index of Vap for which default config and init has to be updated.
 *      hostapd_data - Allocated hostapd_data parent struct for all config
 *      ModeEnabled - Security mode enabled for respective vap Index.
 *
 * Note: Hapd should have valid address before calling this API. ap_index should be
 *      0 - 15 only
 */
void update_config_defaults(int ap_index, struct hostapd_data *hapd, int ModeEnabled)
{
#if defined (_XB7_PRODUCT_REQ_)
    hapd->iconf = g_hapd_glue[ap_index].conf;
    hapd->iface = g_hapd_glue[ap_index].iface;
    hapd->conf = g_hapd_glue[ap_index].bss_conf;
#else
    hapd->iconf = &g_hapd_glue[ap_index].conf;
    hapd->iface = &g_hapd_glue[ap_index].iface;
    hapd->conf = &g_hapd_glue[ap_index].bss_conf;
#endif

    hapd->new_assoc_sta_cb = hostapd_new_assoc_sta;
    hapd->ctrl_sock = -1;

    dl_list_init(&hapd->ctrl_dst);
    dl_list_init(&hapd->nr_db);
    hapd->dhcp_sock = -1;
#ifdef CONFIG_IEEE80211R_AP
//Defined
    dl_list_init(&hapd->l2_queue);
    dl_list_init(&hapd->l2_oui_queue);
#endif /* CONFIG_IEEE80211R_AP */
#if defined (CONFIG_SAE) && defined (WIFI_HAL_VERSION_3)
//Not Defined
    dl_list_init(&hapd->sae_commit_queue);
#endif /* CONFIG_SAE */

    hapd->iconf->bss = os_calloc(1, sizeof(struct hostapd_bss_config *));
    if (hapd->iconf->bss == NULL) {
        os_free(hapd->iconf->bss);
        os_free(hapd->iconf);
        return;
    }
    hapd->iconf->bss[0] = hapd->conf;

    hapd->conf->wpa_key_mgmt = hostapd_tr181_config_parse_key_mgmt(ModeEnabled);
    if ((hapd->conf->wpa_key_mgmt == WPA_KEY_MGMT_PSK) ||
           (hapd->conf->wpa_key_mgmt == WPA_KEY_MGMT_PSK_SHA256) ||
           (hapd->conf->wpa_key_mgmt == WPA_KEY_MGMT_FT_PSK) ||
           (hapd->conf->wpa_key_mgmt == (WPA_KEY_MGMT_PSK | WPA_KEY_MGMT_SAE)) ||
           (hapd->conf->wpa_key_mgmt == (WPA_KEY_MGMT_PSK_SHA256 | WPA_KEY_MGMT_SAE)))
    {
        hapd->conf->ieee802_1x = 0;
    }
    else
        hapd->conf->ieee802_1x = 1;

    //UPDATE radius server
    update_radius_config(hapd->conf);

    update_hostapd_bss_config(ap_index, hapd->conf);

    update_hostapd_iconf(ap_index, hapd->iconf);

    update_hostapd_iface(ap_index, hapd->iface, hapd);

    update_default_oem_configs(ap_index, hapd->conf);

    hapd->driver = (const struct wpa_driver_ops *)&g_hapd_glue[ap_index].driver_ops;
}

int update_tr181_ipc_config(int apIndex, wifi_hal_cmd_t cmd, void *value)
{
    PCOSA_DATAMODEL_WIFI pMyObject = (PCOSA_DATAMODEL_WIFI)g_pCosaBEManager->hWifi;
    PSINGLE_LINK_ENTRY              pSLinkEntrySsid = (PSINGLE_LINK_ENTRY       )NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntryAp   = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_DML_WIFI_AP               pWifiAp         = (PCOSA_DML_WIFI_AP        )NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid       = (PCOSA_DML_WIFI_SSID      )NULL;
    struct hostapd_data *hapd = NULL;
    wpa_printf(MSG_ERROR, "%s:%d Enter", __func__, __LINE__);
    errno_t rc = -1;

    if(!pMyObject) {
        wpa_printf(MSG_ERROR, "%s Data Model object is NULL!\n",__FUNCTION__);
        return -1;
    }

    if (!pMyObject->bEnableHostapdAuthenticator)
        return -1; /* return if native hostapd is enabled */

    if((pSLinkEntrySsid = AnscQueueGetEntryByIndex(&pMyObject->SsidQueue, apIndex)) == NULL) {
        wpa_printf(MSG_ERROR, "%s SSID data Model object not found!\n",__FUNCTION__);
        return -1;
    }

    if((pSLinkEntryAp = AnscQueueGetEntryByIndex(&pMyObject->AccessPointQueue, apIndex)) == NULL) {
        wpa_printf(MSG_ERROR, "%s AP data Model object not found!\n",__FUNCTION__);
        return -1;
    }

    if((pWifiSsid = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntrySsid)->hContext) == NULL) {
        wpa_printf(MSG_ERROR, "%s Error linking Data Model object!\n",__FUNCTION__);
        return -1;
    }

    if((pWifiAp = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntryAp)->hContext) == NULL) {
        wpa_printf(MSG_ERROR, "%s Error linking AP Data Model object!\n",__FUNCTION__);
        return -1;
    }

#if defined (_XB7_PRODUCT_REQ_)
    hapd = g_hapd_glue[apIndex].hapd;
#else
    hapd = &g_hapd_glue[apIndex].hapd;
#endif /*_XB7_PRODUCT_REQ_ */

    /* For Re-init case, allow execution even hapd->started is 0*/
    if (cmd != wifi_hal_cmd_mesh_reconfig && (!hapd || (!hapd->started && cmd != wifi_hal_cmd_start_stop_hostapd))) {
            wpa_printf(MSG_ERROR, "%s:%d hostapd not started", __func__, __LINE__);
	    return -1;
    }

    switch (cmd) {
        case wifi_hal_cmd_push_ssid:
            wpa_printf(MSG_ERROR, "%s:%d push ssid from HAL", __func__, __LINE__);
            strncpy(pWifiSsid->SSID.Cfg.SSID, value, COSA_DML_WIFI_MAX_SSID_NAME_LEN);
            rc = strcpy_s((char *)hapd->conf->ssid.ssid, sizeof(hapd->conf->ssid.ssid) , pWifiSsid->SSID.Cfg.SSID);
            ERR_CHK(rc);
            hapd->conf->ssid.ssid_len = strlen((char *)hapd->conf->ssid.ssid);
            if (!hapd->conf->ssid.ssid_len)
                hapd->conf->ssid.ssid_set = 0;
            else
                hapd->conf->ssid.ssid_set = 1;
            hapd->conf->ssid.utf8_ssid = 0;

//#if defined (_XB7_PRODUCT_REQ_)
//                hostapd_reload_config(hapd->iface);
//#endif /*_XB7_PRODUCT_REQ_ */
            /*
             * Need hostapd reconfig after tr181 and hapd config update,
             * here hostapd_pushSSID.sh is already doing reconfig.
             */
            break;
        case wifi_hal_cmd_bss_transition:
            wpa_printf(MSG_ERROR, "%s:%d change in bss transition from HAL", __func__, __LINE__);
            hapd->conf->bss_transition = *((int *)value);
            if (hapd->conf->bss_transition != pWifiAp->AP.Cfg.BSSTransitionActivated && hapd->started)
            {
                hostapd_reload_config(hapd->iface);
            }
            break;
        case wifi_hal_cmd_interworking:
            wpa_printf(MSG_ERROR, "%s:%d change in interworking from HAL", __func__, __LINE__);
#ifdef CONFIG_HS20
            hapd->conf->rdk_hs20 = *((int *)value);
#endif
            pWifiAp->AP.Cfg.IEEE80211uCfg.PasspointCfg.Status = *((int *)value);
            break;
        case wifi_hal_cmd_greylisting:
            wpa_printf(MSG_ERROR, "%s:%d change in grey list from HAL", __func__, __LINE__);
#if defined (FEATURE_SUPPORT_RADIUSGREYLIST)
            hapd->conf->rdk_greylist = *((int *)value);
            pMyObject->bEnableRadiusGreyList = *((int *)value);
#endif
            break;
        case wifi_hal_cmd_start_stop_hostapd:
            wpa_printf(MSG_ERROR, "%s:%d start stop hostapd cmd from HAL", __func__, __LINE__);
            if (*((int *)value))
            {
                if (!hapd->started)
                {
#ifdef WIFI_HAL_VERSION_3
#if defined (_XB7_PRODUCT_REQ_)
                    wpa_printf(MSG_ERROR, "%s:%d start hostapd on apIndex:%d", __func__, __LINE__, apIndex);
                    libhostapd_wpa_init(pMyObject, pWifiAp, pWifiSsid, &(pMyObject->pRadio+getRadioIndexFromAp(apIndex))->Radio);
#else
                    hapd_wpa_init(pMyObject, pWifiAp, pWifiSsid, &(pMyObject->pRadio+getRadioIndexFromAp(apIndex))->Radio);
#endif /* _XB7_PRODUCT_REQ_ */

#else /* WIFI_HAL_VERSION_3 */
#if defined (_XB7_PRODUCT_REQ_)
                    wpa_printf(MSG_ERROR, "%s:%d start hostapd on apIndex:%d", __func__, __LINE__, apIndex);
                    libhostapd_wpa_init(pMyObject, pWifiAp, pWifiSsid, (apIndex % 2 == 0) ? &(pMyObject->pRadio+0)->Radio : &(pMyObject->pRadio+1)->Radio);
#else
                    hapd_wpa_init(pMyObject, pWifiAp, pWifiSsid, (apIndex % 2 == 0) ? &(pMyObject->pRadio+0)->Radio : &(pMyObject->pRadio+1)->Radio);
#endif /* _XB7_PRODUCT_REQ_ */
#endif /* WIFI_HAL_VERSION_3 */
                }
            }
            else
            {
                if (hapd->started)
                {
#if defined (_XB7_PRODUCT_REQ_)
                    wpa_printf(MSG_ERROR, "%s:%d stop hostapd on apIndex:%d", __func__, __LINE__, apIndex);
                    libhostapd_wpa_deinit(apIndex);
#else
                    hapd_wpa_deinit(apIndex);
#endif
                }
            }
            break;
        case wifi_hal_cmd_push_passphrase:
            wpa_printf(MSG_ERROR, "%s:%d update passphrase from HAL", __func__, __LINE__);
            strncpy((char *)pWifiAp->SEC.Cfg.KeyPassphrase, value, 64);
            int len = os_strlen((char *)pWifiAp->SEC.Cfg.KeyPassphrase);
            if (len < 8 || len > 63) {
                wpa_printf(MSG_ERROR, "Line %d: invalid WPA passphrase length %d (expected 8..63)",
                           __LINE__, len);
                return -1;
            }
            if (hapd->conf->ssid.wpa_passphrase)
            {
                os_free(hapd->conf->ssid.wpa_passphrase);
                hapd->conf->ssid.wpa_passphrase = NULL;
            }
            hapd->conf->ssid.wpa_passphrase = strdup((char *)pWifiAp->SEC.Cfg.KeyPassphrase);
            hapd->conf->ssid.wpa_passphrase_set = 1;
            break;
        case wifi_hal_cmd_mesh_reconfig:
#if defined (_XB7_PRODUCT_REQ_)
            if (*((int *)value))
            {
                if (!hapd)
                {
#ifdef WIFI_HAL_VERSION_3
                    wpa_printf(MSG_ERROR, "%s:%d start hostapd on apIndex:%d", __func__, __LINE__, apIndex);
                    libhostapd_wpa_init(pMyObject, pWifiAp, pWifiSsid, &(pMyObject->pRadio+getRadioIndexFromAp(apIndex))->Radio);
#else
                    libhostapd_wpa_init(pMyObject, pWifiAp, pWifiSsid, (apIndex % 2 == 0) ? &(pMyObject->pRadio+0)->Radio : &(pMyObject->pRadio+1)->Radio);
#endif /* WIFI_HAL_VERSION_3 */
                }
                else
                {
                    wpa_printf(MSG_ERROR, "%s:%d stop hostapd on apIndex:%d", __func__, __LINE__, apIndex);
                    libhostapd_wpa_deinit(apIndex);
#ifdef WIFI_HAL_VERSION_3
                    wpa_printf(MSG_ERROR, "%s:%d start hostapd on apIndex:%d", __func__, __LINE__, apIndex);
                    libhostapd_wpa_init(pMyObject, pWifiAp, pWifiSsid, &(pMyObject->pRadio+getRadioIndexFromAp(apIndex))->Radio);
#else /* WIFI_HAL_VERSION_3 */
                    libhostapd_wpa_init(pMyObject, pWifiAp, pWifiSsid, (apIndex % 2 == 0) ? &(pMyObject->pRadio+0)->Radio : &(pMyObject->pRadio+1)->Radio);
#endif /* WIFI_HAL_VERSION_3 */
                }
                wifi_nvramCommit();
            }
#endif /* _XB7_PRODUCT_REQ_ */
            break;
        default:
            break;
    }

    wpa_printf(MSG_ERROR, "%s:%d Exit", __func__, __LINE__);
    return 0;
}

/* Description:
 *      The API is used to init lib hostap authenticator log file system.
 * Arguments
 *      None
 */
void hapd_init_log_files()
{
    wpa_debug_open_file(HOSTAPD_LOG_FILE_PATH);
#if !(defined CISCO_XB3_PLATFORM_CHANGES)
#if !defined(_XB7_PRODUCT_REQ_)
    rdk_debug_open_file();
#endif
#endif
#ifndef CONFIG_CTRL_IFACE_UDP
    unlink("/var/run/hostapd");
#endif
}

#define HAL_RADIO_NUM_RADIOS        2
#define HAL_AP_IDX_TO_HAL_RADIO(apIdx)  (apIdx) % HAL_RADIO_NUM_RADIOS
#define HAL_AP_IDX_TO_SSID_IDX(apIdx)  (apIdx) / HAL_RADIO_NUM_RADIOS

void convert_apindex_to_interface(int idx, char *iface, int len)
{
        if (NULL == iface) {
                wpa_printf(MSG_INFO, "%s:%d: input_string parameter error!!!\n", __func__, __LINE__);
                return;
        }

        memset(iface, 0, len);
        if (idx < HAL_RADIO_NUM_RADIOS) {
                snprintf(iface, len, "wl%d", idx);
        } else {
                snprintf(iface, len, "wl%d.%d", HAL_AP_IDX_TO_HAL_RADIO(idx), HAL_AP_IDX_TO_SSID_IDX(idx));
        }
}

/* Description:
 *      The API is used to init lib hostap authenticator.
 *      TR181 Radio,SSID,AccessPoint structure cache is used to init hostap
 *      authenticator necessary params.
 *      Additional to TR-181 cache, this will init all other necessary params
 *      with default values.
 * Arguments:
 *      pWifiAp         Device.WiFi.AccessPoint.{i} cache structure.
 *      pWifiSsid       Device.WiFi.SSID.{i} cache structure.
 *      pWifiRadioFull  Device.WiFi.Radio.{i} cache structure.
 */

#if defined (_XB7_PRODUCT_REQ_)
#ifndef HOSTAPD_CLEANUP_INTERVAL
#define HOSTAPD_CLEANUP_INTERVAL 10
#endif /* HOSTAPD_CLEANUP_INTERVAL */

int hostapd_periodic_call(struct hostapd_iface *iface, void *ctx);
void hostapd_periodic(void *eloop_ctx, void *timeout_ctx);
int hostapd_driver_init(struct hostapd_iface *iface);

extern struct hapd_global global;

int libhostapd_global_init()
{
        int i;
        wpa_printf(MSG_ERROR, "%s:%d Start", __func__, __LINE__);
        os_memset(&global, 0, sizeof(global));
     
        if (!is_eloop_init_done)
        {
                if ( eap_server_register_methods() == 0) {
                        wpa_printf(MSG_DEBUG, "%s:%d: EAP methods registered \n", __func__, __LINE__);
                }   else {
                        wpa_printf(MSG_DEBUG, "%s:%d: failed to register EAP methods \n", __func__, __LINE__);
                }

                wpa_printf(MSG_INFO, "%s:%d: Setting up eloop", __func__, __LINE__);
                if (eloop_init() < 0)
                {
                        wpa_printf(MSG_ERROR, "%s:%d: Failed to setup eloop\n", __func__, __LINE__);
                        return -1;
                }
                is_eloop_init_done = 1;
        }   

        random_init(NULL);
        wpa_printf(MSG_ERROR, "%s:%d: random init successful", __func__, __LINE__);

        for (i = 0; wpa_drivers[i]; i++) {
                global.drv_count++;
        }

	wpa_debug_level = MSG_DEBUG;
        wpa_printf(MSG_ERROR, "%s:%d: global.drv_count :%d wpa_debug_level:%d", __func__, __LINE__, global.drv_count, wpa_debug_level);
        if (global.drv_count == 0) {
                wpa_printf(MSG_ERROR, "No drivers enabled");
                return -1;
        }
        global.drv_priv = os_calloc(global.drv_count, sizeof(void *));
        if (global.drv_priv == NULL)
                return -1;

        wpa_printf(MSG_ERROR, "%s:%d: global init successful", __func__, __LINE__);
        return 0;
}

//customized equivalent of hostapd_config_read
struct hostapd_config * libhostapd_config_read(const char *fname, int apIndex)
{
#if defined (_XB7_PRODUCT_REQ_)
    char ifname[8] = {0};
#endif

    g_hapd_glue[apIndex].conf = hostapd_config_defaults();
    if (g_hapd_glue[apIndex].conf == NULL) {
        return NULL;
    }

    /* set default driver based on configuration */
    g_hapd_glue[apIndex].conf->driver = wpa_drivers[0];
    if (g_hapd_glue[apIndex].conf->driver == NULL) {
        wpa_printf(MSG_ERROR, "No driver wrappers registered!");
        hostapd_config_free(g_hapd_glue[apIndex].conf);
        return NULL;
    }

    g_hapd_glue[apIndex].conf->last_bss = g_hapd_glue[apIndex].conf->bss[0];

#if defined (_XB7_PRODUCT_REQ_)
    convert_apindex_to_interface(apIndex, ifname, sizeof(ifname));

    os_strlcpy(g_hapd_glue[apIndex].conf->bss[0]->iface, ifname,sizeof(g_hapd_glue[apIndex].conf->bss[0]->iface));
#endif

    return g_hapd_glue[apIndex].conf;
}

//customized equivalent of hostapd_init
struct hostapd_iface * libhostapd_init(struct hapd_interfaces *interfaces,
                    const char *config_file, int apIndex)
{
    size_t i;

    g_hapd_glue[apIndex].iface = hostapd_alloc_iface();
    if (g_hapd_glue[apIndex].iface == NULL)
        goto fail;

    g_hapd_glue[apIndex].iface->config_fname = (char *)config_file;

    g_hapd_glue[apIndex].conf = libhostapd_config_read(NULL, apIndex);
    if (g_hapd_glue[apIndex].conf == NULL)
        goto fail;

    g_hapd_glue[apIndex].iface->conf = g_hapd_glue[apIndex].conf;

    g_hapd_glue[apIndex].iface->num_bss = g_hapd_glue[apIndex].conf->num_bss;
    g_hapd_glue[apIndex].iface->bss = os_calloc(g_hapd_glue[apIndex].conf->num_bss,
                    sizeof(struct hostapd_data *));

    if (g_hapd_glue[apIndex].iface->bss == NULL)
        goto fail;

    for (i = 0; i < g_hapd_glue[apIndex].conf->num_bss; i++) {
        g_hapd_glue[apIndex].hapd = g_hapd_glue[apIndex].iface->bss[i] = 
            hostapd_alloc_bss_data(g_hapd_glue[apIndex].iface, g_hapd_glue[apIndex].conf,
                           g_hapd_glue[apIndex].conf->bss[i]);
        if (g_hapd_glue[apIndex].hapd == NULL)
            goto fail;
        g_hapd_glue[apIndex].hapd->msg_ctx = g_hapd_glue[apIndex].hapd;
    }

    return g_hapd_glue[apIndex].iface;

fail:
    wpa_printf(MSG_ERROR, "Failed to set up interface with %s",
           config_file);
    if (g_hapd_glue[apIndex].conf)
        hostapd_config_free(g_hapd_glue[apIndex].conf);
    if (g_hapd_glue[apIndex].iface) {
        os_free(g_hapd_glue[apIndex].iface->config_fname);
        os_free(g_hapd_glue[apIndex].iface->bss);
        wpa_printf(MSG_DEBUG, "%s: free iface %p",
               __func__, g_hapd_glue[apIndex].iface);
        os_free(g_hapd_glue[apIndex].iface);
    }
    return NULL;
}

//customized equivalent of hostapd_interface_init
struct hostapd_iface *
libhostapd_interface_init(struct hapd_interfaces *interfaces, const char *if_name,
               const char *config_fname, int debug, int apIndex)
{
    int k;

    wpa_printf(MSG_ERROR, "%s:%d Enter", __func__, __LINE__);

    g_hapd_glue[apIndex].iface = libhostapd_init(interfaces, config_fname, apIndex);
    if (!g_hapd_glue[apIndex].iface)
        return NULL;

    if (if_name) {
        os_strlcpy(g_hapd_glue[apIndex].iface->conf->bss[0]->iface, if_name,
               sizeof(g_hapd_glue[apIndex].iface->conf->bss[0]->iface));
    }

    g_hapd_glue[apIndex].iface->interfaces = interfaces;

    for (k = 0; k < debug; k++) {
        if (g_hapd_glue[apIndex].iface->bss[0]->conf->logger_stdout_level > 0)
            g_hapd_glue[apIndex].iface->bss[0]->conf->logger_stdout_level--;
    }

    if (g_hapd_glue[apIndex].iface->conf->bss[0]->iface[0] == '\0') {
        if(!hostapd_drv_none(g_hapd_glue[apIndex].iface->bss[0])) {
            wpa_printf(MSG_ERROR,
                  "Interface name not specified in %s, nor by '-i' parameter",
                  config_fname);
            hostapd_interface_deinit_free(g_hapd_glue[apIndex].iface);
            return NULL;
        }
    }
    wpa_printf(MSG_ERROR, "%s:%d Exit", __func__, __LINE__);
    return g_hapd_glue[apIndex].iface;
}

//customized equivalent of hostapd_global_deinit
void libhostapd_global_deinit()
{
       int i;
       wpa_printf(MSG_ERROR, "%s:%d Enter", __func__, __LINE__);

       for (i = 0; wpa_drivers[i] && global.drv_priv; i++) {
               if (!global.drv_priv[i])
                       continue;
               wpa_drivers[i]->global_deinit(global.drv_priv[i]);
       }
       os_free(global.drv_priv);
       global.drv_priv = NULL;

#ifdef EAP_SERVER_TNC
       tncs_global_deinit();
#endif /* EAP_SERVER_TNC */

       random_deinit();

       eap_server_unregister_methods();

#ifndef CONFIG_CTRL_IFACE_UDP
    unlink("/var/run/hostapd");
#endif /* !CONFIG_CTRL_IFACE_UDP */

#if !defined(_XB7_PRODUCT_REQ_)
       rdk_debug_close_file();

       wpa_debug_close_file();
#endif

#if !defined (_XB7_PRODUCT_REQ_)
       wifi_callback_deinit();
#endif

       wpa_printf(MSG_ERROR, "%s:%d Exit", __func__, __LINE__);
}

void libupdate_hostapd_iface(int ap_index, struct hostapd_iface *iface, struct hostapd_data *hapd)
{
	iface->conf = hapd->iconf;
    	iface->num_bss = hapd->iconf->num_bss;
    	iface->bss[0] = hapd;

    	iface->drv_flags |= WPA_DRIVER_FLAGS_INACTIVITY_TIMER;
	iface->drv_flags |= WPA_DRIVER_FLAGS_DEAUTH_TX_STATUS;
}

/* libhostapd_wpa_init is wrt hostapd.2.9 version
 *  this function handles the interface init 
 */
int libhostapd_wpa_init(PCOSA_DATAMODEL_WIFI pWifi, PCOSA_DML_WIFI_AP pWifiAp, PCOSA_DML_WIFI_SSID  pWifiSsid, PCOSA_DML_WIFI_RADIO_FULL pWifiRadioFull)
{
        int ret = 1;
        size_t i;
        int debug = 0;
        size_t num_bss_configs = 0;
        int start_ifaces_in_sync = 0;
        char **if_names = NULL;
        size_t if_names_size = 0;

        int ap_index = pWifiAp->AP.Cfg.InstanceNumber - 1;

        wpa_printf(MSG_ERROR, "%s:%d Enter", __func__, __LINE__);

        os_memset(&g_hapd_glue[ap_index].interfaces, 0, sizeof(struct hapd_interfaces));
        g_hapd_glue[ap_index].interfaces.reload_config = hostapd_reload_config;
        g_hapd_glue[ap_index].interfaces.config_read_cb = hostapd_config_read;
        g_hapd_glue[ap_index].interfaces.for_each_interface = hostapd_for_each_interface;
        g_hapd_glue[ap_index].interfaces.ctrl_iface_init = hostapd_ctrl_iface_init;
        g_hapd_glue[ap_index].interfaces.ctrl_iface_deinit = hostapd_ctrl_iface_deinit;
        g_hapd_glue[ap_index].interfaces.driver_init = hostapd_driver_init;
        g_hapd_glue[ap_index].interfaces.global_iface_path = NULL;
        g_hapd_glue[ap_index].interfaces.global_iface_name = NULL;
        g_hapd_glue[ap_index].interfaces.global_ctrl_sock = -1;
        dl_list_init(&g_hapd_glue[ap_index].interfaces.global_ctrl_dst);
#ifdef CONFIG_ETH_P_OUI
        dl_list_init(&g_hapd_glue[ap_index].interfaces.eth_p_oui);
#endif /* CONFIG_ETH_P_OUI */

        g_hapd_glue[ap_index].interfaces.count = 1;
        if (g_hapd_glue[ap_index].interfaces.count || num_bss_configs) {
                g_hapd_glue[ap_index].interfaces.iface = os_calloc(g_hapd_glue[ap_index].interfaces.count + num_bss_configs,
                                             sizeof(struct hostapd_iface *));
                if (g_hapd_glue[ap_index].interfaces.iface == NULL) {
                        wpa_printf(MSG_ERROR, "malloc failed");
                        return -1;
                }
        }

        /* Allocate and parse configuration for full interface files */
        for (i = 0; i < g_hapd_glue[ap_index].interfaces.count; i++) {
                char *if_name = NULL;

                if (i < if_names_size)
                        if_name = if_names[i];

                g_hapd_glue[ap_index].interfaces.iface[i] = libhostapd_interface_init(&g_hapd_glue[ap_index].interfaces,
                                                             if_name,
                                                             NULL,
                                                             debug, ap_index);
                if (!g_hapd_glue[ap_index].interfaces.iface[i]) {
                        wpa_printf(MSG_ERROR, "Failed to initialize interface");
                        goto out;
                }
                if (start_ifaces_in_sync)
                        g_hapd_glue[ap_index].interfaces.iface[i]->need_to_start_in_sync = 1;
        }


        g_hapd_glue[ap_index].bss_conf = g_hapd_glue[ap_index].conf->last_bss;

        /* radius malloc done in default init libhostapd_config_read->hostapd_config_defaults->hostapd_config_defaults_bss
         * now update radius defaults such as auth server ip, port & accnt serveer ip, port (whereas in native case parsed) 
         */
        if(pWifiAp->SEC.Cfg.ModeEnabled != COSA_DML_WIFI_SECURITY_None)
            g_hapd_glue[ap_index].bss_conf->wpa_key_mgmt = hostapd_tr181_config_parse_key_mgmt(pWifiAp->SEC.Cfg.ModeEnabled);

        if ((g_hapd_glue[ap_index].bss_conf->wpa_key_mgmt == WPA_KEY_MGMT_PSK) ||
              (g_hapd_glue[ap_index].bss_conf->wpa_key_mgmt == WPA_KEY_MGMT_PSK_SHA256) ||
              (g_hapd_glue[ap_index].bss_conf->wpa_key_mgmt == WPA_KEY_MGMT_FT_PSK) ||
              (g_hapd_glue[ap_index].bss_conf->wpa_key_mgmt == (WPA_KEY_MGMT_PSK | WPA_KEY_MGMT_SAE)) ||
              (g_hapd_glue[ap_index].bss_conf->wpa_key_mgmt == (WPA_KEY_MGMT_PSK_SHA256 | WPA_KEY_MGMT_SAE)))
        {
            g_hapd_glue[ap_index].bss_conf->ieee802_1x = 0;
        }
        else
            g_hapd_glue[ap_index].bss_conf->ieee802_1x = 1;

        wpa_printf(MSG_ERROR, "%s:%d wpa_key_mgmt:%d ieee802_1x:%d", __func__, __LINE__, g_hapd_glue[ap_index].bss_conf->wpa_key_mgmt, g_hapd_glue[ap_index].bss_conf->ieee802_1x);

        update_radius_config(g_hapd_glue[ap_index].bss_conf);
 
        g_hapd_glue[ap_index].conf->hw_mode = (HAL_AP_IDX_TO_HAL_RADIO(ap_index)?HOSTAPD_MODE_IEEE80211A:HOSTAPD_MODE_IEEE80211G);
        g_hapd_glue[ap_index].conf->channel = (HAL_AP_IDX_TO_HAL_RADIO(ap_index)?36:1);

  	update_hostapd_bss_config(ap_index, g_hapd_glue[ap_index].hapd->conf);
        update_hostapd_iconf(ap_index, g_hapd_glue[ap_index].hapd->iconf);
        update_default_oem_configs(ap_index, g_hapd_glue[ap_index].hapd->conf);
        libupdate_hostapd_iface(ap_index, g_hapd_glue[ap_index].hapd->iface, g_hapd_glue[ap_index].hapd);
 
        //update tr181, oem 
        wpa_printf(MSG_ERROR, "%s:%d update config", __func__, __LINE__);
        if (update_tr181_config_param(ap_index, g_hapd_glue[ap_index].bss_conf, pWifi, pWifiSsid, pWifiAp, pWifiRadioFull))
        {
                wpa_printf(MSG_ERROR, "%s:%d: Hosapd config update failed\n", __func__, __LINE__);
                return -1;
        }
        if (!g_hapd_glue[ap_index].bss_conf->wpa)
        {
            /* Open-Auth handling, for greylist */
            g_hapd_glue[ap_index].bss_conf->ieee802_1x = 0;
            g_hapd_glue[ap_index].bss_conf->eap_server = 0;
            g_hapd_glue[ap_index].bss_conf->auth_algs = WPA_AUTH_ALG_OPEN;
        }

        /* own_addr */
        memcpy(g_hapd_glue[ap_index].hapd->own_addr, pWifiSsid->SSID.StaticInfo.BSSID, ETH_ALEN);

        wpa_printf(MSG_ERROR, "%s:%d setup interface for ap_index:%d hwaddr - " MACSTR "\n", __func__, __LINE__, ap_index, MAC2STR(g_hapd_glue[ap_index].hapd->own_addr));
        ret = hostapd_enable_iface(g_hapd_glue[ap_index].interfaces.iface[0]);
        if (ret < 0) {
                wpa_printf(MSG_ERROR,
                                "Failed to enable interface on config reload");
                goto out;
        }

        wpa_printf(MSG_ERROR, "%s:%d setup interface done for ap_index:%d", __func__, __LINE__, ap_index); 
        hostapd_global_ctrl_iface_init(&g_hapd_glue[ap_index].interfaces);

        ret = 0;
#if defined (FEATURE_SUPPORT_RADIUSGREYLIST)
        greylist_load(&g_hapd_glue[ap_index].interfaces);
#endif

        wpa_printf(MSG_ERROR, "%s:%d Exit", __func__, __LINE__);
        return ret;
 out:
        wpa_printf(MSG_ERROR, "%s:%d out Exit", __func__, __LINE__);

        /* Deinitialize all interfaces */
        for (i = 0; i < g_hapd_glue[ap_index].interfaces.count; i++) {
                if (!g_hapd_glue[ap_index].interfaces.iface[i])
                        continue;
                g_hapd_glue[ap_index].interfaces.iface[i]->driver_ap_teardown =
                        !!(g_hapd_glue[ap_index].interfaces.iface[i]->drv_flags &
                           WPA_DRIVER_FLAGS_AP_TEARDOWN_SUPPORT);
                hostapd_interface_deinit_free(g_hapd_glue[ap_index].interfaces.iface[i]);
        }
        os_free(g_hapd_glue[ap_index].interfaces.iface);

        if (g_hapd_glue[ap_index].interfaces.eloop_initialized)
                eloop_cancel_timeout(hostapd_periodic, &g_hapd_glue[ap_index].interfaces, NULL);

        wpa_printf(MSG_ERROR, "%s:%d Exit", __func__, __LINE__);
        return ret;
}
#else
int hapd_wpa_init(PCOSA_DATAMODEL_WIFI pWifi, PCOSA_DML_WIFI_AP pWifiAp, PCOSA_DML_WIFI_SSID  pWifiSsid, PCOSA_DML_WIFI_RADIO_FULL pWifiRadioFull)
{
    struct hostapd_data *hapd;
    struct hostapd_bss_config *conf;
    int ap_index = pWifiAp->AP.Cfg.InstanceNumber - 1;
    int flush_old_stations = 1;
    struct driver_data *drv = NULL;

#if defined (_XB7_PRODUCT_REQ_)
    hapd = g_hapd_glue[ap_index].hapd;
#else
    hapd = &g_hapd_glue[ap_index].hapd;
#endif

    memset(hapd, '\0', sizeof(struct hostapd_data));

    wpa_printf(MSG_INFO, "%s:%d Hosapd start configuration !!!\n", __func__, __LINE__);

    if (!is_eloop_init_done)
    {
        wpa_printf(MSG_INFO, "%s:%d: Setting up eloop\n", __func__, __LINE__);
        if (eloop_init() < 0)
        {
            wpa_printf(MSG_ERROR, "%s:%d: Failed to setup eloop\n", __func__, __LINE__);
            return -1;
        }
        is_eloop_init_done = 1;
    }

    if ( eap_server_register_methods() == 0) {
	    wpa_printf(MSG_DEBUG, "%s:%d: EAP Methods registered \n", __func__, __LINE__);
    }	else {
	    wpa_printf(MSG_DEBUG, "%s:%d: EAP Methods not successful \n", __func__, __LINE__);
    }

    update_config_defaults(ap_index, hapd, pWifiAp->SEC.Cfg.ModeEnabled);

    if (update_tr181_config_param(ap_index, hapd->conf, pWifi, pWifiSsid, pWifiAp, pWifiRadioFull))
    {
        wpa_printf(MSG_ERROR, "%s:%d: Hosapd config update failed\n", __func__, __LINE__);
        return -1;
    }

    if (!hapd->conf->wpa)
    {
        /* Open-Auth handling, for greylist */
        hapd->conf->ieee802_1x = 0;
        hapd->conf->eap_server = 0;
        hapd->conf->auth_algs = WPA_AUTH_ALG_OPEN;
    }

    //validate security params - changed to set after tr181 configs
    hostapd_set_security_params(hapd->conf, 1);

    //validate_config_params
    if (hostapd_config_check(hapd->iconf, 1) < 0)
    {
        wpa_printf(MSG_ERROR,"%s - %d Config check Failed\n", __func__, __LINE__);
        return -1;
    }

    // Moved to main, so driver init will be called after config parsing
    update_hostapd_driver(ap_index, hapd);
    driver_init(ap_index, hapd);

    if (hapd->drv_priv == NULL)
    {
        wpa_printf(MSG_ERROR, "%s:%d: Hosapd strucut CONFIG updatesuccess, but failed to add drv_priv\n", __func__, __LINE__);
        return -1;
    }
    conf = hapd->conf;

    hapd->interface_added = 1;
    hapd->started = 1;
    hapd->disabled = 0;
    hapd->reenable_beacon = 0;
    hapd->csa_in_progress = 0;

#ifdef CONFIG_DEBUG_LINUX_TRACING
//Not enabled, will be enabled in future if needed.
    if (wpa_debug_open_linux_tracing() < 0) {
        wpa_printf(MSG_ERROR, "%s:%d: Failed to open stdout\n", __func__, __LINE__);

    }
#endif

#ifdef CONFIG_MESH
//Not Defined
    if (hapd->iface->mconf == NULL)
        flush_old_stations = 0;
#endif /* CONFIG_MESH */

    if (flush_old_stations) {
        hostapd_flush_old_stations(hapd, WLAN_REASON_PREV_AUTH_NOT_VALID);
    }

    hostapd_set_privacy(hapd,0);

    hostapd_broadcast_wep_clear(hapd);

    if (hostapd_setup_encryption(conf->iface, hapd)) {
        wpa_printf(MSG_ERROR, "%s:%d: calling before hostapd_setup_encryption\n", __func__, __LINE__);
        return -1;
    }

    if (!hostapd_drv_none(hapd)) {
        wpa_printf(MSG_INFO, "Using interface %s with hwaddr " MACSTR
                " and ssid \"%s\"\n",
                conf->iface, MAC2STR(hapd->own_addr),
                wpa_ssid_txt(conf->ssid.ssid, conf->ssid.ssid_len));
    }

    if (hostapd_setup_wpa_psk(conf)) {
        wpa_printf(MSG_ERROR, "WPA-PSK setup failed.");
        return -1;
    }

    /* Set SSID for the kernel driver (to be used in beacon and probe
     * response frames) */
    if (hostapd_set_ssid(hapd, conf->ssid.ssid, conf->ssid.ssid_len)) {
        wpa_printf(MSG_ERROR, "Could not set SSID for kernel driver \n");
        return -1;
    }

#ifndef CONFIG_NO_RADIUS
    hapd->radius = radius_client_init(hapd, conf->radius);
    if (hapd->radius == NULL) {
        wpa_printf(MSG_ERROR,"%s:%d: RADIUS client initialization failed\n", __func__, __LINE__);
        return -1;
    }

    if (conf->radius_das_port && conf->radius_das_shared_secret_len &&
        conf->rdk_greylist)
    {
        wpa_printf(MSG_DEBUG, "%s:%d RADIUS DAS client init\n", __func__, __LINE__);
        struct radius_das_conf das_conf;
        os_memset(&das_conf, 0, sizeof(das_conf));
        das_conf.port = conf->radius_das_port;
        das_conf.shared_secret = conf->radius_das_shared_secret;
        das_conf.shared_secret_len =
                conf->radius_das_shared_secret_len;
        das_conf.client_addr = &conf->radius_das_client_addr;
        das_conf.time_window = conf->radius_das_time_window;
        das_conf.require_event_timestamp =
                conf->radius_das_require_event_timestamp;
        das_conf.require_message_authenticator =
                conf->radius_das_require_message_authenticator;
        das_conf.ctx = hapd;
        das_conf.disconnect = hostapd_das_disconnect;
#ifdef CONFIG_HS20
        das_conf.coa = hostapd_das_coa;
#else
        das_conf.coa = NULL;
#endif //CONFIG_HS20
        hapd->radius_das = radius_das_init(&das_conf);
        if (hapd->radius_das == NULL) {
                wpa_printf(MSG_ERROR, "RADIUS DAS initialization "
                           "failed.");
                return -1;
        }
    }
#endif /* CONFIG_NO_RADIUS */

    hapd->iface->interfaces = os_calloc(1, sizeof(struct hapd_interfaces));
    hapd->iface->interfaces->iface = os_calloc(1 , sizeof(struct hostapd_iface *));
    hapd->iface->interfaces->iface[0] = hapd->iface;
    hapd->iface->interfaces->for_each_interface = hostapd_for_each_interface;
    hapd->iface->interfaces->count = 1;

    if (hostapd_init_wps(hapd, conf))
    {
        wpa_printf(MSG_ERROR, "%s:%d: WPS init Failed\n", __func__, __LINE__);
        return -1;
    }

    if (ieee802_1x_init(hapd)) {
        wpa_printf(MSG_ERROR,"%s:%d: Failed to init ieee802_1x\n", __func__, __LINE__);
        return -1;
    }

    if ((conf->wpa || conf->osen) && hostapd_setup_wpa(hapd) < 0) {
        wpa_printf(MSG_ERROR,"%s:%d: Failed to setup WPA\n", __func__, __LINE__);
        return -1;
    }

    if (ieee802_11_set_beacon(hapd) < 0) {
        wpa_printf(MSG_ERROR, "%s:%d: calling ieee802_11_set_beacon failed \n", __func__, __LINE__);
        return -1;
    }

    if (hapd->wpa_auth && wpa_init_keys(hapd->wpa_auth) < 0) {
        wpa_printf(MSG_ERROR, "%s:%d: hostapd wpa_init_keys Failed\n", __func__, __LINE__);
        return -1;
    }

    if (hostapd_driver_commit(hapd) < 0) {
        wpa_printf(MSG_ERROR, "%s: Failed to commit driver "
                "configuration \n", __func__);
        return -1;
    }

    drv = hapd->drv_priv;
    if (ap_index == 12 || ap_index == 13)
        wifi_hostApRecvEther(ap_index, hapd->conf->bridge, ETH_P_EAPOL);
    else
        wifi_hostApRecvEther(ap_index, drv->iface, ETH_P_EAPOL);

#ifndef CONFIG_CTRL_IFACE_UDP
//Not Defined
    char *fname;

    fname = (char *)hostapd_ctrl_iface_path(hapd);
    wpa_printf(MSG_DEBUG, "%s:%d fname :%s \n", __func__, __LINE__, fname);
    if (fname)
        unlink(fname);
    os_free(fname);

    if (hapd->conf->ctrl_interface &&
        rmdir(hapd->conf->ctrl_interface) < 0) {
        if (errno == ENOTEMPTY) {
                wpa_printf(MSG_INFO, "Control interface "
                   "directory not empty - leaving it "
                   "behind \n");
        } else {
                wpa_printf(MSG_DEBUG,
                   "rmdir[ctrl_interface=%s]: %s \n",
                   hapd->conf->ctrl_interface,
                   strerror(errno));
        }
    }
#endif /* !CONFIG_CTRL_IFACE_UDP */

    if (hostapd_ctrl_iface_init(hapd)) {
        wpa_printf(MSG_ERROR,"Failed to setup control interface for %s \n",
                hapd->conf->iface);
        return -1;
    }
    wpa_printf(MSG_DEBUG,"setup control interface for %s Successful. Hostapd WPA setup is Successful\n",
            hapd->conf->iface);
   
#if defined (FEATURE_SUPPORT_RADIUSGREYLIST) && !defined(_XB7_PRODUCT_REQ_)
    //Clear the cache of greylist client on bootup
    greylist_cache_timeout(hapd);
#endif

    return 0;
}

/* Description:
 *      The API is used to de-init lib hostap authenticator per vap index.
 *      Should handle all necessary deinit of hostap API(s) and eloop registered
 *      timeout, if any.
 * Arguments:
 *      ap_index: Index of the VAP to be de-init.
 */
void hapd_wpa_deinit(int ap_index)
{
    struct hostapd_data *hapd;
    struct driver_data *drv;

#if defined (_XB7_PRODUCT_REQ_)
    hapd = g_hapd_glue[ap_index].hapd;
#else
    hapd = &g_hapd_glue[ap_index].hapd;
#endif /*_XB7_PRODUCT_REQ_ */
    drv = hapd->drv_priv;

    wpa_printf(MSG_INFO, "%s:%d: Starting De-init - %p\n", __func__, __LINE__, hapd);
    if (hapd->started == 0)
    {
        wpa_printf(MSG_DEBUG,"%s - %d lib hostapd is not started, no need deinit\n", __func__, __LINE__);
        return;
    }
    hapd->started = 0;
    hapd->beacon_set_done = 0;

#if defined (CONFIG_SAE) && defined (WIFI_HAL_VERSION_3)
/* SAE is not defined now, but will be needed in feature for WPA3 support */
    {
        struct hostapd_sae_commit_queue *q;

        while ((q = dl_list_first(&hapd->sae_commit_queue,
                              struct hostapd_sae_commit_queue,
                              list))) {
            dl_list_del(&q->list);
            os_free(q);
        }
    }
    eloop_cancel_timeout(auth_sae_process_commit, hapd, NULL);
    wpa_printf(MSG_DEBUG, "%s:%d: End eloop_cancel_timeout \n", __func__, __LINE__);
#endif /* CONFIG_SAE */

#if !defined(_XB7_PRODUCT_REQ_)
    wifi_hostApIfaceUpdateSigPselect(ap_index, FALSE);
#endif

    //flush old stations
    hostapd_flush_old_stations(hapd, WLAN_REASON_PREV_AUTH_NOT_VALID);
    hostapd_broadcast_wep_clear(hapd);

#ifndef CONFIG_NO_RADIUS
     if (hapd->radius)
     {
         radius_client_deinit(hapd->radius);
         hapd->radius = NULL;
     }
     if (hapd->radius_das)
     {
         radius_das_deinit(hapd->radius_das);
         hapd->radius_das = NULL;
     }
 #endif /* CONFIG_NO_RADIUS */

    hostapd_deinit_wps(hapd);
    hostapd_deinit_wpa(hapd);
    hostapd_config_free_bss(hapd->conf);
    sta_track_deinit(hapd->iface);
    os_free(hapd->iface->bss);
    os_free(drv);
    hostapd_ctrl_iface_deinit(hapd);
#ifndef CONFIG_CTRL_IFACE_UDP
    char *fname = NULL;
    fname = (char *)hostapd_ctrl_iface_path(hapd);
    if (fname)
            unlink(fname);
    os_free(fname);

    if (hapd->conf->ctrl_interface &&
        rmdir(hapd->conf->ctrl_interface) < 0) {
            if (errno == ENOTEMPTY) {
                    wpa_printf(MSG_INFO, "Control interface "
                               "directory not empty - leaving it "
                               "behind");
            } else {
                    wpa_printf(MSG_ERROR,
                               "rmdir[ctrl_interface=%s]: %s",
                               hapd->conf->ctrl_interface,
                               strerror(errno));
            }
    }
#endif /* !CONFIG_CTRL_IFACE_UDP */
    os_free(hapd->iface->interfaces);

    memset(&g_hapd_glue[ap_index].hapd, 0, sizeof(struct hostapd_data));

    wpa_printf(MSG_DEBUG, "%s:%d: End De-init Successfully \n", __func__, __LINE__);
}
#endif /* _XB7_PRODUCT_REQ_ */

#if defined (_XB7_PRODUCT_REQ_)
void libhostap_eloop_deinit()
{
       if (is_eloop_init_done) {
           eloop_terminate();
           wpa_printf(MSG_ERROR, "%s:%d calling eloop_destroy", __func__, __LINE__);
           eloop_destroy();
	   is_eloop_init_done = 0;
       }

}

void libhostapd_wpa_deinit(int ap_index)
{
    wpa_printf(MSG_INFO, "%s:%d: Starting De-init for apIndex:%d\n", __func__, __LINE__, ap_index);

    if (!g_hapd_glue[ap_index].hapd)
    {
        wpa_printf(MSG_INFO, "%s:%d: Return, hapd not started on apIndex:%d\n", __func__, __LINE__, ap_index);
        return;
    }
//    hostapd_global_ctrl_iface_deinit(&g_hapd_glue[ap_index].interfaces);


    unsigned int i = 0;
    for (i = 0; i < g_hapd_glue[ap_index].interfaces.count; i++) {
	    if (!g_hapd_glue[ap_index].interfaces.iface[i])
		    continue;
	    g_hapd_glue[ap_index].interfaces.iface[i]->driver_ap_teardown =
		    !!(g_hapd_glue[ap_index].interfaces.iface[i]->drv_flags &
				    WPA_DRIVER_FLAGS_AP_TEARDOWN_SUPPORT);
	    hostapd_interface_deinit_free(g_hapd_glue[ap_index].interfaces.iface[i]);
    }
    os_free(g_hapd_glue[ap_index].interfaces.iface);

    if (g_hapd_glue[ap_index].interfaces.eloop_initialized)
	    eloop_cancel_timeout(hostapd_periodic, &g_hapd_glue[ap_index].interfaces, NULL);

    if (g_hapd_glue[ap_index].hapd)
    {
        wpa_printf(MSG_ERROR, "%s:%d hapd is not properly deleted, hapd->started %d\n", __func__, __LINE__, g_hapd_glue[ap_index].hapd->started);
        g_hapd_glue[ap_index].hapd = NULL;
    }
    else {
        wpa_printf(MSG_ERROR, "%s:%d hapd is deleted\n", __func__, __LINE__);
    }

    wpa_printf(MSG_DEBUG, "%s:%d: End De-init Successfully \n", __func__, __LINE__);
}
#endif

/* Description:
 *      The API is used to de-init lib hostap eloop params, close debugs files and
 *      handle all necessary deinit which are common for all VAP(s)
 *      Should be called during RFC switch or complete deinit of lib hostap.
 * Arguments:
 *      None
 */
void deinit_eloop()
{
    if (is_eloop_init_done)
    {
        eloop_terminate();
        eloop_destroy();
        is_eloop_init_done = 0;
        wpa_printf(MSG_DEBUG, "%s:%d: Called deinit_loop\n", __func__, __LINE__);
    }
#ifndef CONFIG_CTRL_IFACE_UDP
    unlink("/var/run/hostapd");
#endif /* !CONFIG_CTRL_IFACE_UDP */
#if !(defined CISCO_XB3_PLATFORM_CHANGES)
#if !defined(_XB7_PRODUCT_REQ_)
    rdk_debug_close_file();
#endif
#endif
    wpa_debug_close_file();
#if !defined (_XB7_PRODUCT_REQ_)
    wifi_callback_deinit();
#endif
}

#if defined (FEATURE_SUPPORT_RADIUSGREYLIST)
void init_lib_hostapd_greylisting()
{
#if !defined (_XB7_PRODUCT_REQ_)
    //RDKB-30263 Grey List control from RADIUS 
    cmmac = (char *) malloc (MAC_LEN*sizeof(char));
    memset(cmmac, '\0', MAC_LEN);
    /* execute the script /usr/sbin/deviceinfo.sh to get the cmmac of the device*/
    _syscmd("sh /usr/sbin/deviceinfo.sh -cmac",cmmac, MAC_LEN);
    wpa_printf(MSG_DEBUG,"CM MAC is :%s\n",cmmac);
    //RDKB-30263 Grey List control from RADIUS END 
#endif
}

void deinit_lib_hostapd_greylisting()
{
#if !defined (_XB7_PRODUCT_REQ_)
        os_free(cmmac);
#endif
}
#endif //FEATURE_SUPPORT_RADIUSGREYLIST

int hapd_reload_ssid(int apIndex, char *ssid)
{
    struct hostapd_data *hapd;
    struct hostapd_bss_config *conf;

#if defined (_XB7_PRODUCT_REQ_)
    hapd = g_hapd_glue[apIndex].hapd;
#else
    hapd = &g_hapd_glue[apIndex].hapd;
#endif /*_XB7_PRODUCT_REQ_ */

    if (!hapd || !hapd->started)
        return -1;

    hostapd_flush_old_stations(hapd, WLAN_REASON_PREV_AUTH_NOT_VALID);

    conf = hapd->conf;
    hostapd_config_clear_wpa_psk(&conf->ssid.wpa_psk);
    conf->ssid.wpa_psk = NULL;

    memset(conf->ssid.ssid, '\0', sizeof(conf->ssid.ssid));
    snprintf((char *)conf->ssid.ssid, sizeof(conf->ssid.ssid), "%s", ssid);

    conf->ssid.ssid_len = strlen(ssid);
    conf->ssid.ssid_set = 1;
    conf->ssid.utf8_ssid = 0;

    if (hostapd_setup_wpa_psk(conf))
    {
        wpa_printf(MSG_ERROR,"%s:%d Unable to set WPA PSK\n", __func__, __LINE__);
        return -1;
    }
    if (hostapd_set_ssid(hapd, conf->ssid.ssid, conf->ssid.ssid_len))
    {
        wpa_printf(MSG_ERROR,"%s:%d Unable to set SSID for kernel driver\n", __func__, __LINE__);
        return -1;
    }
    wifi_setSSIDName(apIndex, (char *)conf->ssid.ssid);
    return 0;
}

int hapd_reload_authentication(int apIndex, char *keyPassphrase)
{
    struct hostapd_data *hapd;
    struct hostapd_bss_config *conf;

#if defined (_XB7_PRODUCT_REQ_)
    hapd = g_hapd_glue[apIndex].hapd;
#else
    hapd = &g_hapd_glue[apIndex].hapd;
#endif /*_XB7_PRODUCT_REQ_ */

    if (!hapd || !hapd->started)
        return -1;

    conf = hapd->conf;
    if (os_strlen(keyPassphrase) > 0)
    {

        if (conf->ssid.wpa_passphrase)
            str_clear_free(conf->ssid.wpa_passphrase);

        hostapd_flush_old_stations(hapd, WLAN_REASON_PREV_AUTH_NOT_VALID);

        hostapd_config_clear_wpa_psk(&conf->ssid.wpa_psk);
        conf->ssid.wpa_psk = NULL;

        conf->ssid.wpa_passphrase = strdup(keyPassphrase);
        conf->ssid.wpa_passphrase_set = 1;
        hostapd_setup_wpa_psk(conf);

        if ((hapd->conf->wpa || hapd->conf->osen) && hapd->wpa_auth == NULL) {
            hostapd_setup_wpa(hapd);
            if (hapd->wpa_auth)
                wpa_init_keys(hapd->wpa_auth);
        } else if (hapd->conf->wpa) {
            const u8 *wpa_ie;
            size_t wpa_ie_len;
            hostapd_reconfig_wpa(hapd);
            wpa_ie = wpa_auth_get_wpa_ie(hapd->wpa_auth, &wpa_ie_len);
            if (hostapd_set_generic_elem(hapd, wpa_ie, wpa_ie_len))
                    wpa_printf(MSG_ERROR,"Failed to configure WPA IE for "
                               "the kernel driver.\n");
        } else if (hapd->wpa_auth) {
            wpa_deinit(hapd->wpa_auth);
            hapd->wpa_auth = NULL;
            hostapd_set_privacy(hapd, 0);
            hostapd_setup_encryption(hapd->conf->iface, hapd);
            hostapd_set_generic_elem(hapd, (u8 *) "", 0);
        }
    }
#if !defined(_XB7_PRODUCT_REQ_)
    wifi_setApSecurityKeyPassphrase(apIndex, conf->ssid.wpa_passphrase);
#endif
    return 0;
}

int hapd_reload_encryption_method(int apIndex, int encryptionMethod)
{
    struct hostapd_data *hapd;
    struct hostapd_bss_config *conf;

#if defined (_XB7_PRODUCT_REQ_)
    hapd = g_hapd_glue[apIndex].hapd;
#else
    hapd = &g_hapd_glue[apIndex].hapd;
#endif /*_XB7_PRODUCT_REQ_ */

    if (!hapd || !hapd->started)
        return -1;

    conf = hapd->conf;
    hostapd_flush_old_stations(hapd, WLAN_REASON_PREV_AUTH_NOT_VALID);

    hostapd_config_clear_wpa_psk(&conf->ssid.wpa_psk);
    conf->ssid.wpa_psk = NULL;
    hostapd_deinit_wpa(hapd);

    conf->wpa_pairwise = hostapd_tr181_config_parse_cipher(encryptionMethod);
    switch(encryptionMethod)
    {
        case COSA_DML_WIFI_AP_SEC_TKIP:
            wifi_setApWpaEncryptionMode(apIndex, "TKIPEncryption");
            break;
        case COSA_DML_WIFI_AP_SEC_AES:
            wifi_setApWpaEncryptionMode(apIndex, "AESEncryption");
            break;
        case COSA_DML_WIFI_AP_SEC_AES_TKIP:
            wifi_setApWpaEncryptionMode(apIndex, "TKIPandAESEncryption");
            break;
        default:
            wpa_printf(MSG_ERROR,"Wrong encryption method configured\n");
            return -1;
    }

    conf->rsn_pairwise = 0; //Re-init back to defaults
    hostapd_set_security_params(hapd->conf, 1);
    if ((hapd->conf->wpa || hapd->conf->osen) && hapd->wpa_auth == NULL)
    {

        hostapd_set_privacy(hapd,0);
        hostapd_broadcast_wep_clear(hapd);
        if (hostapd_setup_encryption(conf->iface, hapd) || hostapd_setup_wpa_psk(conf) ||
             ieee802_1x_init(hapd) || (hostapd_setup_wpa(hapd) < 0))
        {
            wpa_printf(MSG_ERROR, "%s:%d: Failed to change the encryption method\n", __func__, __LINE__);
            return -1;
        }
        if (hapd->wpa_auth)
            wpa_init_keys(hapd->wpa_auth);
    }

    wpa_printf(MSG_INFO,"Reconfigured interface %s\n", hapd->conf->iface);

    hostapd_update_wps(hapd);
    ieee802_11_set_beacon(hapd);

    return 0;
}

/* Radius Related Configs */
static int radius_reinit_auth(struct hostapd_data *hapd)
{

    if (hapd->radius)
    {
        radius_client_deinit(hapd->radius);
        hapd->radius = radius_client_init(hapd, hapd->conf->radius);
        if (hapd->radius != NULL)
        {
            wpa_printf(MSG_INFO,"%s:%d Radius param reconfigured successfully\n", __func__, __LINE__);
            return 0;
        }
    }

    wpa_printf(MSG_INFO,"%s:%d Radius is not configured\n", __func__, __LINE__);
    return -1;
}

static void hapd_reload_radius_server_ip(struct hostapd_data *hapd, struct hostapd_bss_config *conf, char *radiusServerIPAddr, BOOL isPrimary)
{
    if (isPrimary) {
        if (inet_aton(radiusServerIPAddr, &conf->radius->auth_servers->addr.u.v4)) {
            conf->radius->auth_servers->addr.af = AF_INET;
        }
    }
    else {
        if (inet_aton(radiusServerIPAddr, &conf->radius->auth_server->addr.u.v4)) {
            conf->radius->auth_server->addr.af = AF_INET;
        }
    }
}

static void hapd_reload_radius_server_secret(struct hostapd_data *hapd, struct hostapd_bss_config *conf, char *radiusSecret, BOOL isPrimary)
{
    if (isPrimary)
    {
        if (conf->radius->auth_servers->shared_secret)
            os_free(conf->radius->auth_servers->shared_secret);

        conf->radius->auth_servers->shared_secret = (u8 *) os_strdup(radiusSecret);
        conf->radius->auth_servers->shared_secret_len = os_strlen(radiusSecret);
    }
    else
    {
        if (conf->radius->auth_server->shared_secret)
            os_free(conf->radius->auth_server->shared_secret);

        conf->radius->auth_server->shared_secret = (u8 *) os_strdup(radiusSecret);
        conf->radius->auth_server->shared_secret_len = os_strlen(radiusSecret);
    }
}

static void hapd_reload_radius_server_port(struct hostapd_data *hapd, struct hostapd_bss_config *conf, int radiusServerPort, BOOL isPrimary)
{
    if (isPrimary)
        conf->radius->auth_servers->port = radiusServerPort;
    else
        conf->radius->auth_server->port = radiusServerPort;
}

int hapd_reload_radius_param(int apIndex, char *radiusSecret, char *radiusServerIPAddr, int radiusServerPort, int pmkCaching, BOOL isPrimary, int configParam)
{
    struct hostapd_data *hapd;
    struct hostapd_bss_config *conf;
    wifi_radius_setting_t radSettings;

#if defined (_XB7_PRODUCT_REQ_)
    hapd = g_hapd_glue[apIndex].hapd;
#else
    hapd = &g_hapd_glue[apIndex].hapd;
#endif /*_XB7_PRODUCT_REQ_ */

    if (!hapd || !hapd->started || !hapd->conf->ieee802_1x)
        return -1;

    conf = hapd->conf;
    memset(&radSettings, '\0', sizeof(wifi_radius_setting_t));

    //TODO: radius_retry_primary_interval and eap_reauth_period related datamodel
    switch(configParam)
    {
        case COSA_WIFI_HAPD_RADIUS_SERVER_IP:
            hapd_reload_radius_server_ip(hapd, conf, radiusServerIPAddr, isPrimary);

            if (isPrimary)
                wifi_setApSecurityRadiusServer(apIndex, radiusServerIPAddr, conf->radius->auth_servers->port, (char *)conf->radius->auth_servers->shared_secret);
            else
                wifi_setApSecuritySecondaryRadiusServer(apIndex, radiusServerIPAddr, conf->radius->auth_servers->port, (char *)conf->radius->auth_servers->shared_secret);
            break;
        case COSA_WIFI_HAPD_RADIUS_SERVER_SECRET:
            hapd_reload_radius_server_secret(hapd, conf, radiusSecret, isPrimary);

            if (isPrimary)
                wifi_setApSecurityRadiusServer(apIndex, (char *)&conf->radius->auth_servers->addr.u.v4, conf->radius->auth_servers->port, radiusSecret);
            else
                wifi_setApSecuritySecondaryRadiusServer(apIndex, (char *)&conf->radius->auth_servers->addr.u.v4, conf->radius->auth_servers->port, radiusSecret);
            break;
        case COSA_WIFI_HAPD_RADIUS_SERVER_PORT:
            hapd_reload_radius_server_port(hapd, conf, radiusServerPort, isPrimary);

            if (isPrimary)
                wifi_setApSecurityRadiusServer(apIndex, (char *)&conf->radius->auth_servers->addr.u.v4, radiusServerPort, (char *)conf->radius->auth_servers->shared_secret);
            else
                wifi_setApSecuritySecondaryRadiusServer(apIndex, (char *)&conf->radius->auth_servers->addr.u.v4, radiusServerPort, (char *)conf->radius->auth_servers->shared_secret);
            break;
        case COSA_WIFI_HAPD_RADIUS_SERVER_PMK_CACHING:
            conf->disable_pmksa_caching = pmkCaching;
            radSettings.PMKCaching = pmkCaching;
            wifi_setApSecurityRadiusSettings(apIndex, &radSettings);
            break;
        default:
            wpa_printf(MSG_ERROR,"%s:%d Wrong configure param passed - %d\n", __func__, __LINE__, configParam);
            return -1;
    }
    hostapd_flush_old_stations(hapd, WLAN_REASON_PREV_AUTH_NOT_VALID);
    return radius_reinit_auth(hapd);
}

int hapd_reload_wps_config(int apIndex, int configParam, int wpsState, int ConfigMethodsEnabled, int WpsPushButton)
{
    struct hostapd_data *hapd;
    struct hostapd_bss_config *conf;
    BOOL isWpsPrevEnabled = FALSE;

#if defined (_XB7_PRODUCT_REQ_)
    hapd = g_hapd_glue[apIndex].hapd;
#else
    hapd = &g_hapd_glue[apIndex].hapd;
#endif /*_XB7_PRODUCT_REQ_ */

    if (!hapd || !hapd->started)
        return -1;

    conf = hapd->conf;
    if (!conf)
    {
        wpa_printf(MSG_DEBUG,"%s:%d Init was not happenend successfully, So can't change the param\n", __func__, __LINE__);
        return -1;
    }

    //Send De-auth, only if WPS is enabled previously
    if (hapd->conf->wps_state)
    {
        hostapd_flush_old_stations(hapd, WLAN_REASON_PREV_AUTH_NOT_VALID);
        isWpsPrevEnabled = TRUE;
    }

    switch(configParam)
    {
        case COSA_WIFI_HAPD_WPS_STATE:
            if (wpsState)
                conf->wps_state = WPS_STATE_CONFIGURED;
            else
                conf->wps_state = WPS_STATE_NOT_CONFIGURED;
            wifi_setApWpsEnable(apIndex, conf->wps_state);
            break;
        case COSA_WIFI_HAPD_WPS_CONFIG_METHODS:
            if (conf->config_methods != NULL)
                os_free(conf->config_methods);

            if (ConfigMethodsEnabled == COSA_DML_WIFI_WPS_METHOD_PushButton) {
                conf->config_methods = os_strdup("push_button");
                wifi_setApWpsConfigMethodsEnabled(apIndex, "PushButton");
            } else  if (ConfigMethodsEnabled == COSA_DML_WIFI_WPS_METHOD_Pin) {
                conf->config_methods = os_strdup("keypad label display");
                wifi_setApWpsConfigMethodsEnabled(apIndex, "Keypad,Label,Display");
            } else if (ConfigMethodsEnabled == (COSA_DML_WIFI_WPS_METHOD_PushButton|COSA_DML_WIFI_WPS_METHOD_Pin) ) {
                conf->config_methods = os_strdup("label display push_button keypad");
                wifi_setApWpsConfigMethodsEnabled(apIndex, "PushButton,Keypad,Label,Display");
            }
            break;
        case COSA_WIFI_HAPD_WPS_PUSH_BUTTON:
            conf->pbc_in_m1 = WpsPushButton;
            break;
        default:
            wpa_printf(MSG_ERROR,"%s : %d Error, undefined config was sent - %d\n", __func__, __LINE__, configParam);
            return -1;
    }

    if (isWpsPrevEnabled && conf->wps_state == WPS_STATE_NOT_CONFIGURED)
        hostapd_deinit_wps(hapd);
    else if (isWpsPrevEnabled)
        hostapd_update_wps(hapd);
    else
        hostapd_init_wps(hapd, conf);

    ieee802_11_set_beacon(hapd);
    return 0;
}

void hapd_reload_bss_transition(int apIndex, BOOL bss_transition)
{
   struct hostapd_data *hapd;

#if defined (_XB7_PRODUCT_REQ_)
    hapd = g_hapd_glue[apIndex].hapd;
#else
    hapd = &g_hapd_glue[apIndex].hapd;
#endif /*_XB7_PRODUCT_REQ_ */

   if (!hapd || !hapd->started)
       return;

   hapd->conf->bss_transition = bss_transition;
   hostapd_reload_config(hapd->iface);
}

void hapd_reset_ap_interface(int apIndex)
{
    struct hostapd_data *hapd;
    int radio_index = -1;

    wpa_printf(MSG_INFO,"%s - %d Resetting the ap :%d interface\n", __func__, __LINE__,apIndex);
#if defined (_XB7_PRODUCT_REQ_)
    hapd = g_hapd_glue[apIndex].hapd;
#else
    hapd = &g_hapd_glue[apIndex].hapd;
#endif /*_XB7_PRODUCT_REQ_ */
    //Reset the AP index

    //RDKB-35373 To avoid resetting iface which is not pre-initialized
    if (!hapd || !hapd->started)
        return;

    //Reset the AP index
    wifi_disableApEncryption(apIndex);

    wifi_deleteAp(apIndex);

    wifi_getApRadioIndex(apIndex, &radio_index);
    wifi_createAp(apIndex, radio_index, (char *)hapd->conf->ssid.ssid, 0);
    wifi_ifConfigUp(apIndex);
}
