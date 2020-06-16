#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "common/wpa_ctrl.h"
#include "common/sae.h"
#include "common/dpp.h"
#include "radius/radius.h"
#include "radius/radius_client.h"
#include "p2p/p2p.h"
#include "fst/fst.h"
#include "crypto/crypto.h"
#include "hostapd.h"
#include "accounting.h"
#include "ieee802_1x.h"
#include "ieee802_11.h"
#include "ieee802_11_auth.h"
#include "wpa_auth.h"
#include "preauth_auth.h"
#include "ap_config.h"
#include "beacon.h"
#include "ap_mlme.h"
#include "vlan_init.h"
#include "p2p_hostapd.h"
#include "ap_drv_ops.h"
#include "gas_serv.h"
#include "wnm_ap.h"
#include "mbo_ap.h"
#include "ndisc_snoop.h"
#include "sta_info.h"
#include "vlan.h"
#include "wps_hostapd.h"
#include "wifi_hal.h"
#include <sys/ioctl.h>
#include <linux/if.h>

static struct hostapd_data g_hapd[MAX_VAP];

struct hostapd_data *
sta_track_seen_on(struct hostapd_iface *iface, const u8 *addr,
          const char *ifname)
{
       struct hostapd_data *hapd;

       return hapd;
}

void ap_list_process_beacon(struct hostapd_iface *iface,
                const struct ieee80211_mgmt *mgmt,
                struct ieee802_11_elems *elems,
                struct hostapd_frame_info *fi)
{

}

int vlan_remove_dynamic(struct hostapd_data *hapd, int vlan_id)
{
       return 0;
}

int vlan_compare(struct vlan_description *a, struct vlan_description *b)
{
       return 0;
}

struct hostapd_vlan * vlan_add_dynamic(struct hostapd_data *hapd,
                       struct hostapd_vlan *vlan,
                       int vlan_id,
                       struct vlan_description *vlan_desc)
{
       return NULL;
}


void handle_probe_req(struct hostapd_data *hapd,
              const struct ieee80211_mgmt *mgmt, size_t len,
              int ssi_signal)
{

}

void iapp_new_station(struct iapp_data *iapp, struct sta_info *sta)
{

}

void mlme_authenticate_indication(struct hostapd_data *hapd,
                  struct sta_info *sta)
{

}

void mlme_disassociate_indication(struct hostapd_data *hapd,
                  struct sta_info *sta, u16 reason_code)
{

}

void mlme_associate_indication(struct hostapd_data *hapd, struct sta_info *sta)
{

}

void mlme_reassociate_indication(struct hostapd_data *hapd,
                 struct sta_info *sta)
{

}

void sta_track_add(struct hostapd_iface *iface, const u8 *addr, int ssi_signal)
{
 
}

void ap_copy_sta_supp_op_classes(struct sta_info *sta,
                 const u8 *supp_op_classes,
                 size_t supp_op_classes_len)
{


}

int hostapd_set_drv_ieee8021x(struct hostapd_data *hapd, const char *ifname,
                  int enabled)
{
       return 0;
}

const struct hostapd_eap_user *
hostapd_get_eap_user(struct hostapd_data *hapd, const u8 *identity,
             size_t identity_len, int phase2)
{
       return NULL;
}

struct hostapd_radius_attr *
hostapd_config_get_radius_attr(struct hostapd_radius_attr *attr, u8 type)
{
    for (; attr; attr = attr->next) {
        if (attr->type == type)
            return attr;
    }
    return NULL;
}

int hostapd_is_dfs_required(struct hostapd_iface *iface)
{
       return 0;
}

int hostapd_drv_send_mlme(struct hostapd_data *hapd,
              const void *msg, size_t len, int noack)
{
       return 0;
}

int hostapd_allowed_address(struct hostapd_data *hapd, const u8 *addr,
                const u8 *msg, size_t len, u32 *session_timeout,
                u32 *acct_interim_interval,
                struct vlan_description *vlan_id,
                struct hostapd_sta_wpa_psk_short **psk,
                char **identity, char **radius_cui,
                int is_probe_req)
{
       return 1;
}

void hostapd_free_psk_list(struct hostapd_sta_wpa_psk_short *psk)
{

}

int hostapd_sta_add(struct hostapd_data *hapd,
            const u8 *addr, u16 aid, u16 capability,
            const u8 *supp_rates, size_t supp_rates_len,
            u16 listen_interval,
            const struct ieee80211_ht_capabilities *ht_capab,
            const struct ieee80211_vht_capabilities *vht_capab,
            const struct ieee80211_he_capabilities *he_capab,
            size_t he_capab_len,
            u32 flags, u8 qosinfo, u8 vht_opmode, int supp_p2p_ps,
            int set)
{
       return 0;
}

int hostapd_eid_wmm_valid(struct hostapd_data *hapd, const u8 *eid, size_t len)
{
       return 1;
}

u8 * hostapd_eid_ext_capab(struct hostapd_data *hapd, u8 *eid)
{
       return NULL;
}

u8 * hostapd_eid_bss_max_idle_period(struct hostapd_data *hapd, u8 *eid)
{
       return NULL;
}

u8 * hostapd_eid_qos_map_set(struct hostapd_data *hapd, u8 *eid)
{
       return NULL;
}

u8 * hostapd_eid_wmm(struct hostapd_data *hapd, u8 *eid)
{
       return NULL;
}

int hostapd_build_ap_extra_ies(struct hostapd_data *hapd,
                   struct wpabuf **beacon_ret,
                   struct wpabuf **proberesp_ret,
                   struct wpabuf **assocresp_ret)
{
       return 0;
}

int hostapd_set_freq_params(struct hostapd_freq_params *data,
                enum hostapd_hw_mode mode,
                int freq, int channel, int ht_enabled,
                int vht_enabled, int he_enabled,
                int sec_channel_offset,
                int oper_chwidth, int center_segment0,
                int center_segment1, u32 vht_caps,
                struct he_capabilities *he_cap)
{
       return 0;
}

int hostapd_set_authorized(struct hostapd_data *hapd,
               struct sta_info *sta, int authorized)
{
       return 0;
}


void hostapd_free_ap_extra_ies(struct hostapd_data *hapd,
                   struct wpabuf *beacon,
                   struct wpabuf *proberesp,
                   struct wpabuf *assocresp)
{
    wpabuf_free(beacon);
    wpabuf_free(proberesp);
    wpabuf_free(assocresp);
}


const u8 * hostapd_get_psk(const struct hostapd_bss_config *conf,
               const u8 *addr, const u8 *p2p_dev_addr,
               const u8 *prev_psk, int *vlan_id)
{
       return conf->ssid.wpa_psk;
}

const char * hostapd_get_vlan_id_ifname(struct hostapd_vlan *vlan, int vlan_id)
{
    return NULL;
}

int hostapd_vlan_valid(struct hostapd_vlan *vlan,
               struct vlan_description *vlan_desc)
{
       return 0;
}

void hostapd_wmm_action(struct hostapd_data *hapd,
            const struct ieee80211_mgmt *mgmt, size_t len)
{

}

void hostapd_handle_radio_measurement(struct hostapd_data *hapd,
                      const u8 *buf, size_t len)
{

}

int hostapd_set_sta_flags(struct hostapd_data *hapd, struct sta_info *sta)
{
       return 0;
}

int hostapd_set_wds_sta(struct hostapd_data *hapd, char *ifname_wds,
            const u8 *addr, int aid, int val)
{
       return 0;
}

void hostapd_rrm_beacon_req_tx_status(struct hostapd_data *hapd,
                      const struct ieee80211_mgmt *mgmt,
                      size_t len, int ok)
{

}

int hostapd_drv_sta_disassoc(struct hostapd_data *hapd,
                 const u8 *addr, int reason)
{
       return 0;
}

int hostapd_drv_set_key(const char *ifname, struct hostapd_data *hapd,
            enum wpa_alg alg, const u8 *addr,
            int key_idx, int set_tx,
            const u8 *seq, size_t seq_len,
            const u8 *key, size_t key_len)
{
       return 0;
}

int hostapd_drv_sta_deauth(struct hostapd_data *hapd,
               const u8 *addr, int reason)
{
    return 0;
}


int hostapd_set_privacy(struct hostapd_data *hapd, int enabled)
{
    return 0;
}


int hostapd_set_generic_elem(struct hostapd_data *hapd, const u8 *elem,
                 size_t elem_len)
{
    return 0;
}

int hostapd_get_seqnum(const char *ifname, struct hostapd_data *hapd,
               const u8 *addr, int idx, u8 *seq)
{
    return 0;
}

u32 hostapd_sta_flags_to_drv(u32 flags)
{
       return 0;
}

int airtime_policy_new_sta(struct hostapd_data *hapd, struct sta_info *sta)
{

}

struct wpa_ctrl * wpa_ctrl_open(const char *ctrl_path)
{
    return NULL;
}

int wpa_ctrl_request(struct wpa_ctrl *ctrl, const char *cmd, size_t cmd_len,
             char *reply, size_t *reply_len,
             void (*msg_cb)(char *msg, size_t len))
{
       return 0;
}

void wpa_ctrl_close(struct wpa_ctrl *ctrl)
{

}

static int wpa_ctrl_attach_helper(struct wpa_ctrl *ctrl, int attach)
{
       return 0;
}

int wpa_ctrl_get_fd(struct wpa_ctrl *ctrl)
{
    return 0;
}

int wpa_ctrl_recv(struct wpa_ctrl *ctrl, char *reply, size_t *reply_len)
{
    return 0;
}

int wpa_ctrl_attach(struct wpa_ctrl *ctrl)
{
       return 0;
}

int l2_packet_send(struct l2_packet_data *l2, const u8 *dst_addr, u16 proto,
           const u8 *buf, size_t len)
{
       return 0;
}

void mlme_deauthenticate_indication(struct hostapd_data *hapd,
                    struct sta_info *sta, u16 reason_code)
{

}

void mlme_michaelmicfailure_indication(struct hostapd_data *hapd,
                       const u8 *addr)
{

}

void hostapd_new_assoc_sta(struct hostapd_data *hapd, struct sta_info *sta,
               int reassoc)
{
    if (hapd->tkip_countermeasures) {
        hostapd_drv_sta_deauth(hapd, sta->addr,
                       WLAN_REASON_MICHAEL_MIC_FAILURE);
        return;
    }

    hostapd_prune_associations(hapd, sta->addr);
    ap_sta_clear_disconnect_timeouts(hapd, sta);

    /* IEEE 802.11F (IAPP) */
    if (hapd->conf->ieee802_11f)
        iapp_new_station(hapd->iapp, sta);

#ifdef CONFIG_P2P
    if (sta->p2p_ie == NULL && !sta->no_p2p_set) {
        sta->no_p2p_set = 1;
        hapd->num_sta_no_p2p++;
        if (hapd->num_sta_no_p2p == 1)
            hostapd_p2p_non_p2p_sta_connected(hapd);
    }
#endif /* CONFIG_P2P */

    airtime_policy_new_sta(hapd, sta);

    /* Start accounting here, if IEEE 802.1X and WPA are not used.
     * IEEE 802.1X/WPA code will start accounting after the station has
     * been authorized. */
    if (!hapd->conf->ieee802_1x && !hapd->conf->wpa && !hapd->conf->osen) {
        ap_sta_set_authorized(hapd, sta, 1);
        os_get_reltime(&sta->connected_time);
        accounting_sta_start(hapd, sta);
    }

    /* Start IEEE 802.1X authentication process for new stations */
    ieee802_1x_new_station(hapd, sta);
    if (reassoc) {
        if (sta->auth_alg != WLAN_AUTH_FT &&
            sta->auth_alg != WLAN_AUTH_FILS_SK &&
            sta->auth_alg != WLAN_AUTH_FILS_SK_PFS &&
            sta->auth_alg != WLAN_AUTH_FILS_PK &&
            !(sta->flags & (WLAN_STA_WPS | WLAN_STA_MAYBE_WPS)))
            wpa_auth_sm_event(sta->wpa_sm, WPA_REAUTH);
    } else
        wpa_auth_sta_associated(hapd->wpa_auth, sta->wpa_sm);

    if (hapd->iface->drv_flags & WPA_DRIVER_FLAGS_WIRED) {
        if (eloop_cancel_timeout(ap_handle_timer, hapd, sta) > 0) {
            wpa_printf(MSG_DEBUG,
                   "%s: %s: canceled wired ap_handle_timer timeout for "
                   MACSTR,
                   hapd->conf->iface, __func__,
                   MAC2STR(sta->addr));
        }
    } else if (!(hapd->iface->drv_flags &
             WPA_DRIVER_FLAGS_INACTIVITY_TIMER)) {
        wpa_printf(MSG_DEBUG,
               "%s: %s: reschedule ap_handle_timer timeout for "
               MACSTR " (%d seconds - ap_max_inactivity)",
               hapd->conf->iface, __func__, MAC2STR(sta->addr),
               hapd->conf->ap_max_inactivity);
        eloop_cancel_timeout(ap_handle_timer, hapd, sta);
        eloop_register_timeout(hapd->conf->ap_max_inactivity, 0,
                       ap_handle_timer, hapd, sta);
    }

#ifdef CONFIG_MACSEC
    if (hapd->conf->wpa_key_mgmt == WPA_KEY_MGMT_NONE &&
        hapd->conf->mka_psk_set)
        ieee802_1x_create_preshared_mka_hapd(hapd, sta);
    else
        ieee802_1x_alloc_kay_sm_hapd(hapd, sta);
#endif /* CONFIG_MACSEC */
}

       
int get_intrface_mac_address (int ap_index,  unsigned char* mac)
{
    int sock;
    struct ifreq ifr;

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Failed to create socket\n");
        return -1;
    }

    ifr.ifr_addr.sa_family = AF_INET;
    sprintf(ifr.ifr_name, "enp%ds3", ap_index);
    if (ioctl(sock, SIOCGIFHWADDR, &ifr) != 0) {
        close(sock);
        printf("ioctl failed to get hardware address\n");
        return -1;
    }

    memcpy(mac, (unsigned char *)ifr.ifr_hwaddr.sa_data, sizeof(mac_address_t));
    close(sock);
    
       return 0;
}

void update_hostapd_iface(int ap_index, struct hostapd_iface *iface)
{
       iface->drv_flags |= WPA_DRIVER_FLAGS_INACTIVITY_TIMER;

       dl_list_init(&iface->sta_seen);
}
       
void update_hostapd_iconf(int ap_index, struct hostapd_config *iconf)
{

}

void update_hostapd_bss_config(int ap_index, struct hostapd_bss_config *cfg)
{
       unsigned char *psk;

       sprintf(cfg->iface, "enp%ds3", ap_index);
       sprintf(cfg->bridge, "br%d", ap_index);
       sprintf(cfg->vlan_bridge, "vlan%d", ap_index);

       cfg->logger_stdout_level = HOSTAPD_LEVEL_DEBUG_VERBOSE;
       cfg->logger_stdout = 1;
       
       cfg->ieee802_1x = 0;
       //HOSTAPD_MODULE_IEEE80211|HOSTAPD_MODULE_IEEE8021X|HOSTAPD_MODULE_RADIUS|HOSTAPD_MODULE_WPA;
       cfg->eapol_version = EAPOL_VERSION;     
       cfg->auth_algs = WPA_AUTH_ALG_OPEN | WPA_AUTH_ALG_SHARED;

    //cfg->wep_rekeying_period = 300;
    /* use key0 in individual key and key1 in broadcast key */
    cfg->broadcast_key_idx_min = 1;
    cfg->broadcast_key_idx_max = 2;
    //cfg->eap_reauth_period = 3600;

    //cfg->wpa_gmk_rekey = 86400;
    cfg->wpa_group_update_count = 4;
    cfg->wpa_pairwise_update_count = 4;
#define DEFAULT_WPA_DISABLE_EAPOL_KEY_RETRIES 0
    cfg->wpa_disable_eapol_key_retries =
        DEFAULT_WPA_DISABLE_EAPOL_KEY_RETRIES;
    cfg->wpa_key_mgmt = WPA_KEY_MGMT_PSK;
    cfg->wpa_pairwise = WPA_CIPHER_CCMP;
    cfg->rsn_pairwise = WPA_CIPHER_CCMP;
    cfg->wpa_group = WPA_CIPHER_CCMP;

       cfg->wpa = WPA_PROTO_WPA | WPA_PROTO_RSN;
       cfg->wpa_key_mgmt = WPA_KEY_MGMT_PSK;

    //cfg->wpa_group_rekey = 86400;

    cfg->max_num_sta = MAX_STA_COUNT;
       cfg->max_listen_interval = 65535;

       memset(cfg->ssid.ssid, 0, sizeof(cfg->ssid.ssid));
       strcpy(cfg->ssid.ssid, "testdp2");
       cfg->ssid.ssid_len = strlen("testdp2");
       cfg->ssid.ssid_set = 1;
       cfg->ssid.utf8_ssid = 0;
       cfg->ssid.wpa_psk_set = 1;
       cfg->ssid.wpa_psk = malloc(32);
       memset(cfg->ssid.wpa_psk, 0x12, 32);

       cfg->radius_server_auth_port = 1812;    
       cfg->radius_das_time_window = 300;

       cfg->radius = malloc(sizeof(*cfg->radius));
}

int hapd_process_assoc_req_frame(unsigned int ap_index, mac_address_t sta, unsigned char *frame, unsigned int frame_len)
{
       return ieee802_11_mgmt(&g_hapd[ap_index], frame, frame_len, NULL);
}

int hapd_process_assoc_rsp_frame(unsigned int ap_index, mac_address_t sta, unsigned char *frame, unsigned int frame_len)
{
       ieee802_11_mgmt_cb(&g_hapd[ap_index], frame, frame_len, WLAN_FC_STYPE_ASSOC_RESP, 1);
}

int hapd_process_auth_frame(unsigned int ap_index, mac_address_t sta, unsigned char *frame, unsigned int frame_len, wifi_direction_t dir)
{
       if (dir == wifi_direction_uplink) {
               return ieee802_11_mgmt(&g_hapd[ap_index], frame, frame_len, NULL);
       } else if (dir == wifi_direction_downlink) {
               ieee802_11_mgmt_cb(&g_hapd[ap_index], frame, frame_len, WLAN_FC_STYPE_AUTH, 1);
               return;
       }
}

void hapd_wpa_run()
{
       eloop_run();
}

int hapd_wpa_init(int ap_index)
{
       struct hostapd_data *hapd;
       struct hostapd_bss_config *conf;        

       hapd = &g_hapd[ap_index];

       hapd->new_assoc_sta_cb = hostapd_new_assoc_sta;
       hapd->iface = (struct hostapd_iface *)malloc(sizeof(struct hostapd_iface));
       update_hostapd_iface(ap_index, hapd->iface);

       hapd->iconf = (struct hostapd_config *)malloc(sizeof(struct hostapd_config));
       update_hostapd_iconf(ap_index, hapd->iconf);

       hapd->conf = (struct hostapd_bss_config *)malloc(sizeof(struct hostapd_bss_config));
       update_hostapd_bss_config(ap_index, hapd->conf);
       conf = hapd->conf;

       hapd->interface_added = 1;
       hapd->started = 1;
       hapd->disabled = 0;
       hapd->reenable_beacon = 0;

       get_intrface_mac_address(ap_index, hapd->own_addr);     

       if (wpa_debug_open_linux_tracing() < 0) {
               wifi_dbg_print(1, "%s:%d: Failed to open stdout\n", __func__, __LINE__);

       }

       if (eloop_init() < 0) {
               wifi_dbg_print(1, "%s:%d: Failed to setup eloop\n", __func__, __LINE__);
        return -1;
       }

       if (hostapd_setup_wpa(hapd) < 0) {
               wifi_dbg_print(1, "%s:%d: Failed to setup WPA\n", __func__, __LINE__);
               return -1;              
       }

    hapd->radius = radius_client_init(hapd, conf->radius);
    if (hapd->radius == NULL) {
               wifi_dbg_print(1, "%s:%d: RADIUS client initialization failed\n", __func__, __LINE__);
        return -1;
    }

    if (ieee802_1x_init(hapd)) {
               wifi_dbg_print(1, "%s:%d: Failed to init ieee802_1x\n", __func__, __LINE__);
        return -1;
    }

       wifi_dbg_print(1, "%s:%d: hostapd WPA setup successful\n", __func__, __LINE__);

       return 0;
}
