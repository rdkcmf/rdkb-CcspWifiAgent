#ifndef _WIFI_EASY_CONNECT_H
#define _WIFI_EASY_CONNECT_H

typedef struct {
    unsigned int    num;
    unsigned int    channels[32];
} wifi_easy_connect_best_enrollee_channels_t;

#define MAX_DPP_VAP	2

typedef struct {
	void 	*reconf_ctx;
	char	reconf_pub_key[512];
} wifi_easy_connect_reconfig_t;

typedef struct {
	void	*csign_inst;
	unsigned char sign_key_hash[512];
} wifi_easy_connect_csign_t;

typedef struct {
	PCOSA_DATAMODEL_WIFI	wifi_dml;
    queue_t              *queue;
    pthread_cond_t       cond;
    pthread_mutex_t      lock;
    pthread_t            tid;
	wifi_easy_connect_reconfig_t	reconfig[MAX_DPP_VAP];
	wifi_easy_connect_csign_t		csign[MAX_DPP_VAP];
    wifi_easy_connect_best_enrollee_channels_t    channels_on_ap[2];
} wifi_easy_connect_t;

#define str(x) #x
#define enum_str(x) str(x)

#endif //_WIFI_EASY_CONNECT_H
