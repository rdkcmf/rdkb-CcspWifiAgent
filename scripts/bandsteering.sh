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

