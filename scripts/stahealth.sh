#!/bin/sh
#This script is used to log parameters for each AP
ssid1="0"
ssid2="1"
source /etc/log_timestamp.sh

sta1=`wifi_api wifi_getApAssociatedDeviceDiagnosticResult $ssid1`
sta2=`wifi_api wifi_getApAssociatedDeviceDiagnosticResult $ssid2`

WIFI_MAC_1_Total_count=`echo "$sta1" | grep Total_STA | cut -d':' -f2`
if [ "$sta1" != "" ] && [ "$WIFI_MAC_1_Total_count" != "0" ] ; then
	mac1=`echo "$sta1" | grep cli_MACAddress | cut -d ' ' -f3`
	if [ "$mac1" == "" ] ; then
		mac1=`echo "$sta1" | grep cli_MACAddress | cut -d ' ' -f2`
	fi
	mac1=`echo "$mac1" | tr '\n' ','`
	echo_t "WIFI_MAC_1:$mac1"
        echo_t "WIFI_MAC_1_TOTAL_COUNT:$WIFI_MAC_1_Total_count"
	rssi1=`echo "$sta1" | grep cli_RSSI | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
	echo_t "WIFI_RSSI_1:$rssi1"
	rxrate1=`echo "$sta1" | grep cli_LastDataDownlinkRate | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
	echo_t "WIFI_RXCLIENTS_1:$rxrate1"
	txrate1=`echo "$sta1" | grep cli_LastDataUplinkRate | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
	echo_t "WIFI_TXCLIENTS_1:$txrate1"
	channel=`wifi_api wifi_getRadioChannel $ssid1`
	ch=`echo "$channel" | grep channel`
	if [ "$ch" != "" ] ; then
		channel=`echo "$ch" | cut -d ' ' -f4`
	fi
	echo_t "WIFI_CHANNEL_1:$channel"
	channel_width_1=`echo "$sta1" | grep cli_OperatingChannelBandwidth | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
	echo_t "WIFI_CHANNEL_WIDTH_1:$channel_width_1"

	echo "$sta1" | grep cli_LastDataDownlinkRate | cut -d '=' -f 2 | tr -d ' ' > /tmp/rxx1
	echo "$sta1" | grep cli_LastDataUplinkRate | cut -d '=' -f 2 | tr -d ' ' > /tmp/txx1
	rxtxd1=`awk '{printf ("%s ", $0); getline < "/tmp/txx1"; print $0 }' /tmp/rxx1 | awk '{print ($1 - $2)}' |  tr '\n' ','`
	rm /tmp/rxx1 /tmp/txx1
	echo_t "WIFI_RXTXCLIENTDELTA_1:$rxtxd1"
else 
	echo_t "WIFI_MAC_1_TOTAL_COUNT:0"
	channel=`wifi_api wifi_getRadioChannel $ssid1`
	ch=`echo "$channel" | grep channel`
	if [ "$ch" != "" ] ; then
		channel=`echo "$ch" | cut -d ' ' -f4`
	fi
	echo_t "WIFI_CHANNEL_1:$channel"
fi

getmac_1=`psmcli get eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.1.MacFilterMode`
echo_t "WIFI_ACL_1:$getmac_1"
getmac_1=`echo "$getmac_1" | grep 0`
#temporary workaround for ACL disable issue
buf=`wifi_api wifi_getBandSteeringEnable  | grep "TRUE"`
if [ "$buf" != "" ] && [ "$getmac_1" != "" ] ; then
	echo_t "Enabling BS to fix ACL inconsitency"
	dmcli eRT setv Device.WiFi.X_RDKCENTRAL-COM_BandSteering.Enable bool false
	dmcli eRT setv Device.WiFi.X_RDKCENTRAL-COM_BandSteering.Enable bool true
fi
acs_1=`dmcli eRT getv Device.WiFi.Radio.1.AutoChannelEnable | grep true`
if [ "$acs_1" != "" ]; then
	echo_t "WIFI_ACS_1:true"
else
	echo_t "WIFI_ACS_1:false"
fi

WIFI_MAC_2_Total_count=`echo "$sta2" | grep Total_STA | cut -d':' -f2`
if [ "$sta2" != "" ] && [ "$WIFI_MAC_2_Total_count" != "0" ] ; then
	mac2=`echo "$sta2" | grep cli_MACAddress | cut -d ' ' -f3`
	if [ "$mac2" == "" ] ; then
		mac2=`echo "$sta2" | grep cli_MACAddress | cut -d ' ' -f2`
	fi
	mac2=`echo "$mac2" | tr '\n' ','`
	echo_t "WIFI_MAC_2:$mac2"
	WIFI_MAC_2_Total_count=`echo "$sta2" | grep Total_STA | cut -d':' -f2`
        echo_t "WIFI_MAC_2_TOTAL_COUNT:$WIFI_MAC_2_Total_count"
	rssi2=`echo "$sta2" | grep cli_RSSI | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
	echo_t "WIFI_RSSI_2:$rssi2"
	rxrate2=`echo "$sta2" | grep cli_LastDataDownlinkRate | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
	echo_t "WIFI_RXCLIENTS_2:$rxrate2"
	txrate2=`echo "$sta2" | grep cli_LastDataUplinkRate | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
	echo_t "WIFI_TXCLIENTS_2:$txrate2"
	channel=`wifi_api wifi_getRadioChannel $ssid2`
	ch=`echo "$channel" | grep channel`
	if [ "$ch" != "" ] ; then
		channel=`echo "$ch" | cut -d ' ' -f4`
	fi
	echo_t "WIFI_CHANNEL_2:$channel"
	channel_width_2=`echo "$sta2" | grep cli_OperatingChannelBandwidth | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
	echo_t "WIFI_CHANNEL_WIDTH_2:$channel_width_2"

	echo "$sta2" | grep cli_LastDataDownlinkRate | cut -d '=' -f 2 | tr -d ' ' > /tmp/rxx2
	echo "$sta2" | grep cli_LastDataUplinkRate | cut -d '=' -f 2 | tr -d ' ' > /tmp/txx2
	rxtxd2=`awk '{printf ("%s ", $0); getline < "/tmp/txx2"; print $0 }' /tmp/rxx2 | awk '{print ($1 - $2)}' |  tr '\n' ','`
	rm /tmp/rxx2 /tmp/txx2
	echo_t "WIFI_RXTXCLIENTDELTA_2:$rxtxd2"
else 
	echo_t "WIFI_MAC_2_TOTAL_COUNT:0"
	channel=`wifi_api wifi_getRadioChannel $ssid2`
	ch=`echo "$channel" | grep channel`
	if [ "$ch" != "" ] ; then
		channel=`echo "$ch" | cut -d ' ' -f4`
	fi
	echo_t "WIFI_CHANNEL_2:$channel"
fi

getmac_2=`psmcli get eRT.com.cisco.spvtg.ccsp.tr181pa.Device.WiFi.AccessPoint.2.MacFilterMode`
echo_t "WIFI_ACL_2:$getmac_2"
getmac_2=`echo "$getmac_2" | grep 0`
#temporary workaround for ACL disable issue
if [ "$buf" != "" ] && [ "$getmac_2" != "" ] ; then
	echo_t "Enabling BS to fix ACL inconsitency"
	dmcli eRT setv Device.WiFi.X_RDKCENTRAL-COM_BandSteering.Enable bool false
	dmcli eRT setv Device.WiFi.X_RDKCENTRAL-COM_BandSteering.Enable bool true
fi
acs_2=`dmcli eRT getv Device.WiFi.Radio.2.AutoChannelEnable | grep true`
if [ "$acs_2" != "" ]; then
	echo_t "WIFI_ACS_2:true"
else
	echo_t "WIFI_ACS_2:false"
fi
