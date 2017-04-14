#!/bin/sh
#This script is used to log parameters for each AP
ssid1="ath0"
ssid2="ath1"
source /etc/log_timestamp.sh

sta1=`wlanconfig $ssid1 list sta | grep -v ADDR | tr -s " " | awk '{$6=$6-95; print}' | cut -d' ' -f1,4,5,6,18`
sta2=`wlanconfig $ssid2 list sta | grep -v ADDR | tr -s " " | awk '{$6=$6-95; print}' | cut -d' ' -f1,4,5,6,18`

if [ "$sta1" != "" ] ; then
	mac1=`echo "$sta1" | cut -d' ' -f1 | tr '\n' ','`
	echo_t "WIFI_MAC_1:$mac1"
	WIFI_MAC_1_Total_count=`grep -o "," <<<"$mac1" | wc -l`
        echo_t "WIFI_MAC_1_TOTAL_COUNT:$WIFI_MAC_1_Total_count"
	rssi1=`echo "$sta1" | cut -d' ' -f4 | tr '\n' ','`
	echo_t "WIFI_RSSI_1:$rssi1"
	rxrate1=`echo "$sta1" | cut -d' ' -f3 | tr '\n' ','`
	echo_t "WIFI_RXCLIENTS_1:$rxrate1"
	txrate1=`echo "$sta1" | cut -d' ' -f2 | tr '\n' ','`
	echo_t "WIFI_TXCLIENTS_1:$txrate1"
	channel=`iwlist ath0 channel | grep Current | awk '{print $5}' | cut -d")" -f1`
	echo_t "WIFI_CHANNEL_1:$channel"
	channel_width_1=`echo "$sta1" | cut -d' ' -f5 | tr '\n' ','`
	echo_t "WIFI_CHANNEL_WIDTH_1:$channel_width_1"

	echo "$sta1" | cut -d' ' -f3 | tr -d 'M' > /tmp/rxx1
	echo "$sta1" | cut -d' ' -f2 | tr -d 'M' > /tmp/txx1
	rxtxd1=`awk '{printf ("%s ", $0); getline < "/tmp/txx1"; print $0 }' /tmp/rxx1 | awk '{print ($1 - $2)}' |  tr '\n' ','`
	rm /tmp/rxx1 /tmp/txx1
	echo_t "WIFI_RXTXCLIENTDELTA_1:$rxtxd1"
else 
	echo_t "WIFI_MAC_1_TOTAL_COUNT:0"
	channel=`iwlist ath0 channel | grep Current | awk '{print $5}' | cut -d")" -f1`
	echo_t "WIFI_CHANNEL_1:$channel"
fi

getmac_1=`iwpriv ath0 get_maccmd | cut -d":" -f2`
echo_t "WIFI_ACL_1:$getmac_1"
acs_1=`dmcli eRT getv Device.WiFi.Radio.1.AutoChannelEnable | grep true`
if [ "$acs_1" != "" ]; then
	echo_t "WIFI_ACS_1:true"
else
	echo_t "WIFI_ACS_1:false"
fi

channel_width_1=`echo "$sta1" | cut -d' ' -f3 | tr '\n' ','`
if [ "$sta2" != "" ] ; then
        mac2=`echo "$sta2" | cut -d' ' -f1 | tr '\n' ','`
        echo_t "WIFI_MAC_2:$mac2"
	WIFI_MAC_2_Total_count=`grep -o "," <<<"$mac2" | wc -l`
        echo_t "WIFI_MAC_2_TOTAL_COUNT:$WIFI_MAC_2_Total_count"
        rssi2=`echo "$sta2" | cut -d' ' -f4 | tr '\n' ','`
        echo_t "WIFI_RSSI_2:$rssi2"
        rxrate2=`echo "$sta2" | cut -d' ' -f3 | tr '\n' ','`
        echo_t "WIFI_RXCLIENTS_2:$rxrate2"
        txrate2=`echo "$sta2" | cut -d' ' -f2 | tr '\n' ','`
        echo_t "WIFI_TXCLIENTS_2:$txrate2"
	channel=`iwlist ath1 channel | grep Current | awk '{print $5}' | cut -d")" -f1`
	echo_t "WIFI_CHANNEL_2:$channel"
	channel_width_2=`echo "$sta2" | cut -d' ' -f5 | tr '\n' ','`
	echo_t "WIFI_CHANNEL_WIDTH_2:$channel_width_2"

	echo "$sta2" | cut -d' ' -f3 | tr -d 'M' > /tmp/rxx2
        echo "$sta2" | cut -d' ' -f2 | tr -d 'M' > /tmp/txx2
        rxtxd2=`awk '{printf ("%s ", $0); getline < "/tmp/txx2"; print $0 }' /tmp/rxx2 | awk '{print ($1 - $2)}' |  tr '\n' ','`
	rm /tmp/rxx2 /tmp/txx2
        echo_t "WIFI_RXTXCLIENTDELTA_2:$rxtxd2"
else 
	echo_t "WIFI_MAC_2_TOTAL_COUNT:0"
	channel=`iwlist ath1 channel | grep Current | awk '{print $5}' | cut -d")" -f1`
	echo_t "WIFI_CHANNEL_2:$channel"
fi

getmac_2=`iwpriv ath1 get_maccmd | cut -d":" -f2`
echo_t "WIFI_ACL_2:$getmac_2"

acs_2=`dmcli eRT getv Device.WiFi.Radio.2.AutoChannelEnable | grep true`
if [ "$acs_2" != "" ]; then
	echo_t "WIFI_ACS_2:true"
else
	echo_t "WIFI_ACS_2:false"
fi
