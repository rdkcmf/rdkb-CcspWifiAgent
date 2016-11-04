#!/bin/sh
#This script is used to log parameters for each AP
ssid1="ath0"
ssid2="ath1"
tm=`date "+%s"`

sta1=`wlanconfig $ssid1 list sta | grep -v ADDR | tr -s " " | awk '{$6=$6-95; print}' | cut -d' ' -f1,4,5,6`
sta2=`wlanconfig $ssid2 list sta | grep -v ADDR | tr -s " " | awk '{$6=$6-95; print}' | cut -d' ' -f1,4,5,6`

if [ "$sta1" != "" ] ; then
	mac1=`echo "$sta1" | cut -d' ' -f1 | tr '\n' ','`
	echo "$tm WIFI_MAC_1:$mac1"
	rssi1=`echo "$sta1" | cut -d' ' -f4 | tr '\n' ','`
	echo "$tm WIFI_RSSI_1:$rssi1"
	rxrate1=`echo "$sta1" | cut -d' ' -f3 | tr '\n' ','`
	echo "$tm WIFI_RXCLIENTS_1:$rxrate1"
	txrate1=`echo "$sta1" | cut -d' ' -f2 | tr '\n' ','`
	echo "$tm WIFI_TXCLIENTS_1:$txrate1"

	echo "$sta1" | cut -d' ' -f3 | tr -d 'M' > /tmp/rxx1
	echo "$sta1" | cut -d' ' -f2 | tr -d 'M' > /tmp/txx1
	rxtxd1=`awk '{printf ("%s ", $0); getline < "/tmp/txx1"; print $0 }' /tmp/rxx1 | awk '{print ($1 - $2)}' |  tr '\n' ','`
	rm /tmp/rxx1 /tmp/txx1
	echo "$tm WIFI_RXTXCLIENTDELTA_1:$rxtxd1"
fi

if [ "$sta2" != "" ] ; then
        mac2=`echo "$sta2" | cut -d' ' -f1 | tr '\n' ','`
        echo "$tm WIFI_MAC_2:$mac2"
        rssi2=`echo "$sta2" | cut -d' ' -f4 | tr '\n' ','`
        echo "$tm WIFI_RSSI_2:$rssi2"
        rxrate2=`echo "$sta2" | cut -d' ' -f3 | tr '\n' ','`
        echo "$tm WIFI_RXCLIENTS_2:$rxrate2"
        txrate2=`echo "$sta2" | cut -d' ' -f2 | tr '\n' ','`
        echo "$tm WIFI_TXCLIENTS_2:$txrate2"

	echo "$sta2" | cut -d' ' -f3 | tr -d 'M' > /tmp/rxx2
        echo "$sta2" | cut -d' ' -f2 | tr -d 'M' > /tmp/txx2
        rxtxd2=`awk '{printf ("%s ", $0); getline < "/tmp/txx2"; print $0 }' /tmp/rxx2 | awk '{print ($1 - $2)}' |  tr '\n' ','`
	rm /tmp/rxx2 /tmp/txx2
        echo "$tm WIFI_RXTXCLIENTDELTA_2:$rxtxd2"
fi


