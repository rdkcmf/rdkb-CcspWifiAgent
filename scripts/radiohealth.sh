#!/bin/sh
#This script is used to log parameters for each radio
logfolder="/tmp/wifihealth"

if [ ! -d "$logfolder" ] ; then
	mkdir "$logfolder";
fi

bu1=`wifi_api wifi_getRadioBandutilization 0"`
bu2=`wifi_api wifi_getRadioBandutilization 1"`

if [ "$bu1" == "" ] ; then
	bu1=0;
fi
if [ "$bu2" == "" ] ; then
        bu2=0;
fi

echo "WIFI_BANDUTILIZATION_1:$bu1"
echo "WIFI_BANDUTILIZATION_2:$bu2"

