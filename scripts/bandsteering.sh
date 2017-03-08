#!/bin/sh
#This script is used to log parameters for each radio
source /etc/log_timestamp.sh

buf=`wifi_api wifi_getBandSteeringEnable  | grep "TRUE"`


if [ "$buf" == "" ] ; then
	buf="false"
else
	buf="true"
fi

echo_t "BANDSTEERING_ENABLE_STATUS:$buf"

if [ -f /lib/rdk/wifi_bs_viable_check.sh ]; then
        source /lib/rdk/wifi_bs_viable_check.sh
        rc=$?
        if [ "$rc" == "0" ]; then
                echo_t "RDKB_BANDSTEERING_DISABLED_STATUS:false"
        else
                echo_t "RDKB_BANDSTEERING_DISABLED_STATUS:true"
        fi
else
        echo_t "RDKB_BANDSTEERING_DISABLED_STATUS:false"
fi
