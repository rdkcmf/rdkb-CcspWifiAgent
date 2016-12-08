#!/bin/sh
#This script is used to log parameters for each radio
tm=`date "+%s"`

buf=`wifi_api wifi_getBandSteeringEnable  | grep "TRUE"`


if [ "$buf" == "" ] ; then
	buf="false"
else
	buf="true"
fi

echo "$tm BANDSTEERING_ENABLE_STATUS:$buf"

if [ -f /lib/rdk/wifi_bs_viable_check.sh ]; then
        source /lib/rdk/wifi_bs_viable_check.sh
        rc=$?
        if [ "$rc" == "0" ]; then
                echo "$tm RDKB_BANDSTEERING_DISABLED_STATUS:false"
        else
                echo "$tm RDKB_BANDSTEERING_DISABLED_STATUS:true"
        fi
else
        echo "$tm RDKB_BANDSTEERING_DISABLED_STATUS:false"
fi
