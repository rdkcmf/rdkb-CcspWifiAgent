
#ifndef _WIFI_WEBCONF_H_
#define _WIFI_WEBCONF_H_

typedef struct
{
    char  ssid_name[64];
    bool  enable;       
    bool  ssid_advertisement_enabled;
    bool  ssid_changed;
} webconf_ssid_t;

typedef struct
{
    char   passphrase[64];
    char   encryption_method[16];       
    char   mode_enabled[32];
    bool   sec_changed;
} webconf_security_t;

typedef struct
{
    webconf_ssid_t ssid_2g;
    webconf_security_t security_2g; 
    webconf_ssid_t ssid_5g;
    webconf_security_t security_5g;
    char    subdoc_name[32];
    uint64_t  version;
    uint16_t   transaction_id;
} webconf_wifi_t;

typedef struct
{
    bool hostapd_restart;
    bool init_radio;
    bool sec_changed;
} webconf_apply_t;

int wifi_WebConfigSet(const void *buf, size_t len);

#endif

