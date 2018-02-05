#!/bin/sh
#This script is used to log parameters for each AP

source /etc/log_timestamp.sh

if [ -f /etc/device.properties ]
then
    source /etc/device.properties
fi

print_connected_client_info()
{
	AP=$(( $1 + 1 ))
	RADIO=$(( $1 % 2 ))
	sta1=`wifi_api wifi_getApAssociatedDeviceDiagnosticResult $1`

	WIFI_MAC_1_Total_count=`echo "$sta1" | grep Total_STA | cut -d':' -f2`
	if [ "$sta1" != "" ] && [ "$WIFI_MAC_1_Total_count" != "0" ] ; then
		if [ "$BOX_TYPE" = "XB3" ]; then
			mac1=`echo "$sta1" | grep cli_MACAddress | cut -d ' ' -f3`
		else
			mac1=`echo "$sta1" | grep cli_MACAddress | cut -d '=' -f2`
		fi
		if [ "$mac1" == "" ] ; then
			mac1=`echo "$sta1" | grep cli_MACAddress | cut -d ' ' -f2`
		fi
		mac1=`echo "$mac1" | tr '\n' ','`
		echo_t "WIFI_MAC_$AP:$mac1"
		echo_t "WIFI_MAC_$AP""_TOTAL_COUNT:$WIFI_MAC_1_Total_count"
		rssi1=`echo "$sta1" | grep cli_RSSI | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
		echo_t "WIFI_RSSI_$AP:$rssi1"
		rssi1=`echo "$sta1" | grep cli_SignalStrength | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
		echo_t "WIFI_NORMALIZED_RSSI_$AP:$rssi1"

		rxrate1=`echo "$sta1" | grep cli_LastDataDownlinkRate | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
		echo_t "WIFI_RXCLIENTS_$AP:$rxrate1"
		txrate1=`echo "$sta1" | grep cli_LastDataUplinkRate | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
		echo_t "WIFI_TXCLIENTS_$AP:$txrate1"
		channel=`wifi_api wifi_getRadioChannel $RADIO`
		ch=`echo "$channel" | grep channel`
		if [ "$ch" != "" ] ; then
			channel=`echo "$ch" | cut -d ' ' -f4`
		fi
		echo_t "WIFI_CHANNEL_$AP:$channel"
		channel_width_1=`echo "$sta1" | grep cli_OperatingChannelBandwidth | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
		echo_t "WIFI_CHANNEL_WIDTH_$AP:$channel_width_1"

		echo "$sta1" | grep cli_LastDataDownlinkRate | cut -d '=' -f 2 | tr -d ' ' > /tmp/rxx1
		echo "$sta1" | grep cli_LastDataUplinkRate | cut -d '=' -f 2 | tr -d ' ' > /tmp/txx1
		rxtxd1=`awk '{printf ("%s ", $0); getline < "/tmp/txx1"; print $0 }' /tmp/rxx1 | awk '{print ($1 - $2)}' |  tr '\n' ','`
		rm /tmp/rxx1 /tmp/txx1
		echo_t "WIFI_RXTXCLIENTDELTA_$AP:$rxtxd1"
	else
		echo_t "WIFI_MAC_$AP""_TOTAL_COUNT:0"
		channel=`wifi_api wifi_getRadioChannel $RADIO`
		ch=`echo "$channel" | grep channel`
		if [ "$ch" != "" ] ; then
			channel=`echo "$ch" | cut -d ' ' -f4`
		fi
		echo_t "WIFI_CHANNEL_$AP:$channel"
	fi
        if [ "$AP" == "1" ] || [ "$AP" == "2" ] ; then
                
                #adding transmit power and countrycode 
		
		if [ "$AP" == "1" ];then
			gettxpower=` iwlist ath0 txpower |grep "Current Tx-Power=" | cut -d '=' -f2 | cut -f 1 -d " "`
			getcountrycode=`iwpriv ath0 get_countrycode | cut -d ':' -f2`
			echo_t "WIFI_COUNTRY_CODE_$AP:$getcountrycode"
			echo_t "WIFI_TX_PWR_dBm_$AP:$gettxpower"
		else
			gettxpower=` iwlist ath1 txpower |grep "Current Tx-Power=" | cut -d '=' -f2 | cut -f 1 -d " "`
			getcountrycode=`iwpriv ath1 get_countrycode | cut -d ':' -f2`
			echo_t "WIFI_COUNTRY_CODE_$AP:$getcountrycode"
			echo_t "WIFI_TX_PWR_dBm_$AP:$gettxpower"
		fi

                
		#temporary workaround for ACL disable issue
                
		buf=`wifi_api wifi_getBandSteeringEnable  | grep "TRUE"`
                if [ -f /lib/rdk/wifi_bs_viable_check.sh ]; then
                	getmac_1=`wifi_api wifi_getApMacAddressControlMode 0`
                	getmac_2=`wifi_api wifi_getApMacAddressControlMode 1`
                	if [ "$AP" == "1" ];then
                       		echo_t "WIFI_ACL_$AP:$getmac_1"
                        	getmac=`echo "$getmac_1" | grep 2`
               		else
                        	echo_t "WIFI_ACL_$AP:$getmac_2"
                        	getmac=`echo "$getmac_2" | grep 2`
                	fi
                        /lib/rdk/wifi_bs_viable_check.sh > /dev/null
                        rc=$?
                        if [ "$rc" == "0" ]; then
                                if [ "$buf" == "TRUE" ] && [ "$getmac" == "" ]; then
                                        echo_t "Enabling BS to fix ACL inconsitency"
                                        wifi_api wifi_setBandSteeringEnable 0
                                        wifi_api wifi_setBandSteeringEnable 1
                                fi
                        fi
                fi

                acs_1=`dmcli eRT getv Device.WiFi.Radio.$AP.AutoChannelEnable | grep true`
                if [ "$acs_1" != "" ]; then
                        echo_t "WIFI_ACS_$AP:true"
                else
                        echo_t "WIFI_ACS_$AP:false"
                fi
        fi
}

# Print connected client information for required interfaces (eg. ath0 , ath1 etc)

#ath0
print_connected_client_info 0

#ath1
print_connected_client_info 1

#ath2
print_connected_client_info 2

#ath3
print_connected_client_info 3

#ath6
print_connected_client_info 6

#ath7
print_connected_client_info 7

