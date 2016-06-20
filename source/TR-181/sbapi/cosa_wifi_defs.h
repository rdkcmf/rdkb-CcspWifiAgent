#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <net/ethernet.h>
#include <stdbool.h>

#define IWEVREGISTERED  0x8C03      /* Discovered a new node (AP mode) */
#define IWEVEXPIRED 0x8C04      /* Expired a node (AP mode) */
#define IW_EV_LCP_PK_LEN    (4)
/* Maximum bit rates in the range struct */
#define IW_MAX_BITRATES     32
#define IW_MAX_ENCODING_SIZES   8
#define IW_MAX_TXPOWER      8
#define IW_MAX_FREQUENCIES  32
#define SIOCIWLASTPRIV  0x8BFF
#define SIOCIWFIRST 0x8B00
#define SIOCIWLAST  SIOCIWLASTPRIV      /* 0x8BFF */


#define IW_HEADER_TYPE_NULL	0	/* Not available */
#define IW_HEADER_TYPE_CHAR	2	/* char [IFNAMSIZ] */
#define IW_HEADER_TYPE_UINT	4	/* __u32 */
#define IW_HEADER_TYPE_FREQ	5	/* struct iw_freq */
#define IW_HEADER_TYPE_ADDR	6	/* struct sockaddr */
#define IW_HEADER_TYPE_POINT	8	/* struct iw_point */
#define IW_HEADER_TYPE_PARAM	9	/* struct iw_param */
#define IW_HEADER_TYPE_QUAL	10	/* struct iw_quality */

/* Handling flags */
/* Most are not implemented. I just use them as a reminder of some
 * cool features we might need one day ;-) */
#define IW_DESCR_FLAG_NONE  0x0000  /* Obvious */
/* Wrapper level flags */
#define IW_DESCR_FLAG_DUMP  0x0001  /* Not part of the dump command */
#define IW_DESCR_FLAG_EVENT 0x0002  /* Generate an event on SET */
#define IW_DESCR_FLAG_RESTRICT  0x0004  /* GET : request is ROOT only */
                /* SET : Omit payload from generated iwevent */
#define IW_DESCR_FLAG_NOMAX 0x0008  /* GET : no limit on request size */
/* Driver level flags */
#define IW_DESCR_FLAG_WAIT  0x0100  /* Wait for driver event */

static const int event_type_size[] = {
    IW_EV_LCP_PK_LEN,   /* IW_HEADER_TYPE_NULL */
    0,   
    IW_EV_CHAR_PK_LEN,  /* IW_HEADER_TYPE_CHAR */
    0,   
    IW_EV_UINT_PK_LEN,  /* IW_HEADER_TYPE_UINT */
    IW_EV_FREQ_PK_LEN,  /* IW_HEADER_TYPE_FREQ */
    IW_EV_ADDR_PK_LEN,  /* IW_HEADER_TYPE_ADDR */
    0,   
    IW_EV_POINT_PK_LEN, /* Without variable payload */
    IW_EV_PARAM_PK_LEN, /* IW_HEADER_TYPE_PARAM */
    IW_EV_QUAL_PK_LEN,  /* IW_HEADER_TYPE_QUAL */
};
