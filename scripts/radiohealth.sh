#!/bin/sh
#This script is used to log parameters for each radio
source /etc/log_timestamp.sh
logfolder="/tmp/wifihealth"

if [ ! -d "$logfolder" ] ; then
	mkdir "$logfolder";
fi

bu1=`wifi_api wifi_getRadioBandUtilization 0`
bu2=`wifi_api wifi_getRadioBandUtilization 1`

#short term workaround before wifi_api avaliabel
#bu1=`rdk_band_steering_hal wifi_getRadioBandUtilization 0 | grep ":" | cut -d":" -f2 | tr -d " %"`
#bu2=`rdk_band_steering_hal wifi_getRadioBandUtilization 1 | grep ":" | cut -d":" -f2 | tr -d " %"`

if [ "$bu1" == "" ] ; then
	bu1=0;
fi
if [ "$bu2" == "" ] ; then
        bu2=0;
fi

echo_t "WIFI_BANDUTILIZATION_1:$bu1"
echo_t "WIFI_BANDUTILIZATION_2:$bu2"

