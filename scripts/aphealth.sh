#!/bin/sh
#This script is used to log parameters for each AP
ssid1="ath0"
ssid2="ath1"
logfolder="/tmp/wifihealth"
oneMB=1048576;

if [ ! -d "$logfolder" ] ; then
	mkdir "$logfolder";
fi

rx1_0=`cat "$logfolder/rx1_0"`
rx2_0=`cat "$logfolder/rx2_0"`

if [ "$rx1_0" == "" ] ; then
	rx1_0=0;
fi
if [ "$rx2_0" == "" ] ; then
        rx2_0=0;
fi

rx1=`ifconfig "$ssid1" | grep "RX bytes" | cut -d':' -f2 | cut -d' ' -f1`
rx2=`ifconfig "$ssid2" | grep "RX bytes" | cut -d':' -f2 | cut -d' ' -f1`

if [ "$rx1" == "" ] ; then
        rx1=0;
fi
if [ "$rx2" == "" ] ; then
        rx2=0;
fi

echo "$rx1" > $logfolder/rx1_0;
echo "$rx2" > $logfolder/rx2_0;

rx1d=$(($rx1-$rx1_0))
rx2d=$(($rx2-$rx2_0))

rx1_mb=$(($rx1/$oneMB))
rx2_mb=$(($rx2/$oneMB))

rx1d_mb=$(($rx1d/$oneMB))
rx2d_mb=$(($rx1d/$oneMB))

echo "WIFI_RX_1:$rx1_mb"
echo "WIFI_RXDELTA_1:$rx1d_mb"

echo "WIFI_RX_2:$rx2_mb"
echo "WIFI_RXDELTA_2:$rx2d_mb"
