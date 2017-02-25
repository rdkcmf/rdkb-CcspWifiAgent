#!/bin/sh
#This script is used to log parameters for each AP
ssid1="ath0"
ssid2="ath1"
logfolder="/tmp/wifihealth"
oneMB=1048576;
tm=`date "+%s"`
period=60;

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

echo "$tm WIFI_RX_1:$rx1_mb"
echo "$tm WIFI_RXDELTA_1:$rx1d_mb"

echo "$tm WIFI_RX_2:$rx2_mb"
echo "$tm WIFI_RXDELTA_2:$rx2d_mb"


if [ -e "/sbin/iwconfig" ] ; then
	l1=`iwconfig $ssid1 | grep "Link Quality"`
	l2=`iwconfig $ssid2 | grep "Link Quality"`
	if [ "$l1" != "" ] ; then
		lq1=`echo $l1 | cut -d"=" -f2 | cut -d" " -f1`
		sl1=`echo $l1 | cut -d"=" -f3 | cut -d" " -f1`
		nl1=`echo $l1 | cut -d"=" -f4 | cut -d" " -f1`
		echo "$tm WIFI_LINKQUALITY_1:$lq1"
		echo "$tm WIFI_SIGNALLEVEL_1:$sl1"
		echo "$tm WIFI_NOISELEVEL_1:$nl1"
	fi
	if [ "$l2" != "" ] ; then
		lq2=`echo $l2 | cut -d"=" -f2 | cut -d" " -f1`
		sl2=`echo $l2 | cut -d"=" -f3 | cut -d" " -f1`
		nl2=`echo $l2 | cut -d"=" -f4 | cut -d" " -f1`
		echo "$tm WIFI_LINKQUALITY_2:$lq2"
		echo "$tm WIFI_SIGNALLEVEL_2:$sl2"
		echo "$tm WIFI_NOISELEVEL_2:$nl2"
	fi
fi

check_radio=`cfg -e | grep AP_RADIO_ENABLED`
if [ "$check_radio" != "" ]; then

	excess_retry_0=`cat "$logfolder/excess_retry_0"`

	if [ "$excess_retry_0" == "" ] ; then
		excess_retry_0=0;
	fi
	excess_retry_1=`athstats | grep "excess retries" | cut -d":" -f2`

	if [ "$excess_retry_1" == "" ] ; then
		excess_retry_1=0;
	fi

	echo "$excess_retry_1" > $logfolder/excess_retry_0;

	excess_retry_1d=$(($excess_retry_1-$excess_retry_0))


	excess_retry_1d_mb=$(($excess_retry_1d/$period))
	echo "$tm WIFI_EXCESS_RETRYCOUNT_PER_MINUTE:$excess_retry_1d_mb"
fi

#beacon rate
for i in 0 2 4 6 8 10; do
	a=`wifi_api wifi_getApEnable 1 | grep -i TRUE`
	if [ "$?" == "0" ]; then
		br=`wifi_api wifi_getApBeaconRate $i | grep -i Mbps | sed 's/out_str: //' | sed 's/Mbps//'`
		if [ "$br" != "" ]; then
			ins=$((i+1));
			echo "$tm WIFI_BEACON_RATE_$ins:$br"
		fi
	fi
done
