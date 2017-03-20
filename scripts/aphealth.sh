#!/bin/sh
#This script is used to log parameters for each AP
ssid1="ath0"
ssid2="ath1"
logfolder="/tmp/wifihealth"
oneMB=1048576;
period=60;
source /etc/log_timestamp.sh

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

echo_t "WIFI_RX_1:$rx1_mb"
echo_t "WIFI_RXDELTA_1:$rx1d_mb"

echo_t "WIFI_RX_2:$rx2_mb"
echo_t "WIFI_RXDELTA_2:$rx2d_mb"


if [ -e "/sbin/iwconfig" ] ; then
	l1=`iwconfig $ssid1 | grep "Link Quality"`
	l2=`iwconfig $ssid2 | grep "Link Quality"`
	if [ "$l1" != "" ] ; then
		lq1=`echo $l1 | cut -d"=" -f2 | cut -d" " -f1`
		sl1=`echo $l1 | cut -d"=" -f3 | cut -d" " -f1`
		nl1=`echo $l1 | cut -d"=" -f4 | cut -d" " -f1`
		echo_t "WIFI_LINKQUALITY_1:$lq1"
		echo_t "WIFI_SIGNALLEVEL_1:$sl1"
		echo_t "WIFI_NOISELEVEL_1:$nl1"
	fi
	if [ "$l2" != "" ] ; then
		lq2=`echo $l2 | cut -d"=" -f2 | cut -d" " -f1`
		sl2=`echo $l2 | cut -d"=" -f3 | cut -d" " -f1`
		nl2=`echo $l2 | cut -d"=" -f4 | cut -d" " -f1`
		echo_t "WIFI_LINKQUALITY_2:$lq2"
		echo_t "WIFI_SIGNALLEVEL_2:$sl2"
		echo_t "WIFI_NOISELEVEL_2:$nl2"
	fi
fi

check_radio=`cfg -e | grep AP_RADIO_ENABLED`
if [ "$check_radio" != "" ]; then

	excess_retry_0=`cat "$logfolder/excess_retry_0"`
	phy_2_0=`cat "$logfolder/phy_2_0"`
	phy_5_0=`cat "$logfolder/phy_5_0"`
	CRC_2_0=`cat "$logfolder/CRC_2_0"`
	CRC_5_0=`cat "$logfolder/CRC_5_0"`

	if [ "$excess_retry_0" == "" ] ; then
		excess_retry_0=0;
	fi
	if [ "$phy_2_0" == "" ] ; then
		phy_2_0=0;
	fi
	if [ "$phy_5_0" == "" ] ; then
		phy_5_0=0;
	fi
	if [ "$CRC_2_0" == "" ] ; then
		CRC_2_0=0;
	fi
	if [ "$CRC_5_0" == "" ] ; then
		CRC_5_0=0;
	fi
	excess_retry_1=`athstats | grep "excess retries" | cut -d":" -f2`
        phy_2_1=`apstats -r -i wifi1 | grep "Rx PHY errors" | cut -d"=" -f2`
        phy_5_1=`apstats -r -i wifi0 | grep "Rx PHY errors" | cut -d"=" -f2`
        CRC_2_1=`apstats -r -i wifi1 | grep "Rx CRC errors" | cut -d"=" -f2`
        CRC_5_1=`apstats -r -i wifi0 | grep "Rx CRC errors" | cut -d"=" -f2`

	if [ "$excess_retry_1" == "" ] ; then
		excess_retry_1=0;
	fi
	if [ "$phy_2_1" == "" ] ; then
		phy_2_1=0;
	fi
	if [ "$phy_5_1!" == "" ] ; then
		phy_5_1=0;
	fi
	if [ "$CRC_2_1" == "" ] ; then
		CRC_2_1=0;
	fi
	if [ "$CRC_5_1" == "" ] ; then
		CRC_5_1=0;
	fi

	echo "$excess_retry_1" > $logfolder/excess_retry_0;
	echo "$phy_2_1" > $logfolder/phy_2_0;
	echo "$phy_5_1" > $logfolder/phy_5_0;
	echo "$CRC_2_1" > $logfolder/CRC_2_0;
	echo "$CRC_5_1" > $logfolder/CRC_5_0;

	excess_retry_1d=$(($excess_retry_1-$excess_retry_0))
	phy_2_1d=$(($phy_2_1-$phy_2_0))
	phy_5_1d=$(($phy_5_1-$phy_5_0))
	CRC_2_1d=$(($CRC_2_1-$CRC_2_0))
	CRC_5_1d=$(($CRC_5_1-$CRC_5_0))


	excess_retry_1d_mb=$(($excess_retry_1d/$period))
	phy_2_1d_mb=$(($phy_2_1d/$period))
	phy_5_1d_mb=$(($phy_5_1d/$period))
	CRC_2_1d_mb=$(($CRC_2_1d/$period))
	CRC_5_1d_mb=$(($CRC_5_1d/$period))
	echo_t "WIFI_EXCESS_RETRYCOUNT_PER_MINUTE:$excess_retry_1d_mb"
	echo_t "WIFI_PHY_ERROR_PER_MINUTE_1:$phy_2_1d_mb"
	echo_t "WIFI_PHY_ERROR_PER_MINUTE_2:$phy_5_1d_mb"
	echo_t "WIFI_CRC_ERROR_PER_MINUTE_1:$CRC_2_1d_mb"
	echo_t "WIFI_CRC_ERROR_PER_MINUTE_2:$CRC_5_1d_mb"
        stats=`athstats | grep "Tx MCS STATS:" -A 2 | grep "mcs 0- mcs 4 STATS:" | cut -d":" -f2`
	stats=`echo -n "${stats//[[:space:]]/}"`
        echo_t "WIFI_MCS_TX_0_TO_4:$stats"
	stats=`athstats | grep "Tx MCS STATS:" -A 2 | grep "mcs 5- mcs 9 STATS:" | cut -d":" -f2`
	stats=`echo -n "${stats//[[:space:]]/}"`
        echo_t "WIFI_MCS_TX_5_TO_9:$stats"
        stats=`athstats | grep "Rx MCS STATS:" -A 2 | grep "mcs 0- mcs 4 STATS:" | cut -d":" -f2`
	stats=`echo -n "${stats//[[:space:]]/}"`
        echo_t "WIFI_MCS_RX_0_TO_4:$stats"
	stats=`athstats | grep "Rx MCS STATS:" -A 2 | grep "mcs 5- mcs 9 STATS:" | cut -d":" -f2`
	stats=`echo -n "${stats//[[:space:]]/}"`
        echo_t "WIFI_MCS_RX_5_TO_9:$stats"
fi

#beacon rate
for i in 0 2 4 6 8 10; do
	a=`wifi_api wifi_getApEnable 1 | grep -i TRUE`
	if [ "$?" == "0" ]; then
		br=`wifi_api wifi_getApBeaconRate $i | grep -i Mbps | sed 's/out_str: //' | sed 's/Mbps//'`
		if [ "$br" != "" ]; then
			ins=$((i+1));
			echo_t "WIFI_BEACON_RATE_$ins:$br"
		fi
	fi
done
