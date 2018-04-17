#!/bin/sh
######################################################################################
# If not stated otherwise in this file or this component's Licenses.txt file the
# following copyright and licenses apply:

#  Copyright 2018 RDK Management

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#######################################################################################
#This script is used to log parameters for each AP

source /etc/log_timestamp.sh

if [ -f /etc/device.properties ]
then
    source /etc/device.properties
fi

print_connected_client_info()
{
	trflag=$2;
	nrflag=$3;
	stflag=$4;
	snflag=$5;

	AP=$(( $1 + 1 ))
	RADIO=$(( $1 % 2 ))

	if [ "$BOX_TYPE" == "XB3" ] || [ "$BOX_TYPE" == "XB6" ] ; then
		sta1=`wifi_api wifi_getApAssociatedDeviceDiagnosticResult3 $1`
	else
		sta1=`wifi_api wifi_getApAssociatedDeviceDiagnosticResult $1`
	fi

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
        for i in $(echo $mac1 | sed "s/,/ /g")
        do
            fa_info=`dmesg | grep FA_INFO_$i | tail -1`
            if [ "$fa_info" != "" ] ; then
                echo_t "WIFI_UAPSD_$AP:`echo $fa_info | cut -d"," -f1 |  cut -d":" -f7`"
                echo_t "WIFI_TX_DISCARDS_CNT_$AP:`echo $fa_info | cut -d"," -f2`"
                echo_t "WIFI_TX_PKTS_CNT_$AP:`echo $fa_info | cut -d"," -f3`"
                echo_t "WIFI_BMP_CLR_CNT_$AP:`echo $fa_info | cut -d"," -f4`"
                echo_t "WIFI_BMP_SET_CNT_$AP:`echo $fa_info | cut -d"," -f5`"
                echo_t "WIFI_TIM_$AP:`echo $fa_info | cut -d"," -f6`"
                echo_t "WIFI_AID_$AP:`echo $fa_info | cut -d"," -f7`"
            fi

            fa_data_stats=`dmesg | grep FA_LMAC_DATA_STATS_$i | tail -1`
            if [ "$fa_data_stats" != "" ] ; then
                echo_t "WIFI_DATA_QUEUED_CNT_$AP:`echo $fa_data_stats | cut -d"," -f1 |  cut -d":" -f7`"
                echo_t "WIFI_DATA_DROPPED_CNT_$AP:`echo $fa_data_stats | cut -d"," -f2`"
                echo_t "WIFI_DATA_DEQUED_TX_CNT_$AP:`echo $fa_data_stats | cut -d"," -f3`"
                echo_t "WIFI_DATA_DEQUED_DROPPED_CNT_$AP:`echo $fa_data_stats | cut -d"," -f4`"
                echo_t "WIFI_DATA_EXP_DROPPED_CNT_$AP:`echo $fa_data_stats | cut -d"," -f5`"
            fi

            fa_mgmt_stats=`dmesg | grep FA_LMAC_MGMT_STATS_$i | tail -1`
            if [ "$fa_mgmt_stats" != "" ] ; then
                echo_t "WIFI_MGMT_QUEUED_CNT_$AP:`echo $fa_mgmt_stats | cut -d"," -f1 |  cut -d":" -f7`"
                echo_t "WIFI_MGMT_DROPPED_CNT_$AP:`echo $fa_mgmt_stats | cut -d"," -f2`"
                echo_t "WIFI_MGMT_DEQUED_TX_CNT_$AP:`echo $fa_mgmt_stats | cut -d"," -f3`"
                echo_t "WIFI_MGMT_DEQUED_DROPPED_CNT_$AP:`echo $fa_mgmt_stats | cut -d"," -f4`"
                echo_t "WIFI_MGMT_EXP_DROPPED_CNT_$AP:`echo $fa_mgmt_stats | cut -d"," -f5`"
            fi
        done
		echo_t "WIFI_MAC_$AP:$mac1"
		echo_t "WIFI_MAC_$AP""_TOTAL_COUNT:$WIFI_MAC_1_Total_count"
		rssi1=`echo "$sta1" | grep cli_RSSI | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
		echo_t "WIFI_RSSI_$AP:$rssi1"

		if [ "$nrflag" == "1" ]; then
		  rssi1=`echo "$sta1" | grep cli_SignalStrength | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
		  echo_t "WIFI_NORMALIZED_RSSI_$AP:$rssi1"
		fi
		if [ "$snflag" == "1" ]; then
                  snr=`echo "$sta1" | grep cli_SNR | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
                  echo_t "WIFI_SNR_$AP:$snr"
                fi
		if [ "$trflag" == "1" ]; then
		  txrate1=`echo "$sta1" | grep cli_LastDataDownlinkRate | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
		  echo_t "WIFI_TXCLIENTS_$AP:$txrate1"
		  rxrate1=`echo "$sta1" | grep cli_LastDataUplinkRate | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
		  echo_t "WIFI_RXCLIENTS_$AP:$rxrate1"
		fi

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

		if [ "$stflag" == "1" ]; then
			txbyte=`echo "$sta1" | grep cli_BytesSent | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
			echo_t "WIFI_BYTESSENTCLIENTS_$AP:$txbyte"

			rxbyte=`echo "$sta1" | grep cli_BytesReceived | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
			echo_t "WIFI_BYTESRECEIVEDCLIENTS_$AP:$rxbyte"

			txpkt=`echo "$sta1" | grep cli_PacketsSent | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
			echo_t "WIFI_PACKETSSENTCLIENTS_$AP:$txpkt"

			rxpkt=`echo "$sta1" | grep cli_PacketsReceived | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
			echo_t "WIFI_PACKETSRECEIVEDCLIENTS_$AP:$rxpkt"

			txerr=`echo "$sta1" | grep cli_ErrorsSent | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
			echo_t "WIFI_ERRORSSENT_$AP:$txerr"

			txcnt=`echo "$sta1" | grep cli_RetransCount | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
			echo_t "WIFI_RETRANSCOUNT_$AP:$txcnt"

			frcnt=`echo "$sta1" | grep cli_FailedRetransCount | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
			echo_t "WIFI_FAILEDRETRANSCOUNT_$AP:$frcnt"

			rtcnt=`echo "$sta1" | grep cli_RetryCount | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
			echo_t "WIFI_RETRYCOUNT_$AP:$rtcnt"

			mrcnt=`echo "$sta1" | grep cli_MultipleRetryCount | cut -d '=' -f 2 | tr -d ' ' | tr '\n' ','`
			echo_t "WIFI_MULTIPLERETRYCOUNT_$AP:$mrcnt"
		fi
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
getarray() {
  i=$1;
  a=("0" "0" "0" "0" "0" "0" "0" "0" "0" "0" "0" "0" "0" "0" "0" "0")
  st=($(echo ${i} | tr "," " "))
  for e in "${st[@]}"; do
    a[$(($e-1))]=1;
  done
  echo "${a[0]} ${a[1]} ${a[2]} ${a[3]} ${a[4]} ${a[5]} ${a[6]} ${a[7]} ${a[8]} ${a[9]} ${a[10]} ${a[11]} ${a[12]} ${a[13]} ${a[14]} ${a[15]}"
}

TxRxRateList=`psmcli get dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WHIX.TxRxRateList`
if [ "$TxRxRateList" == "" ]; then
	TxRxRateList="1,2"
	psmcli set  dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WHIX.TxRxRateList "1,2"
fi
trlist=($(getarray "$TxRxRateList"))

NormalizedRssiList=`psmcli get dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WHIX.NormalizedRssiList`
if [ "$NormalizedRssiList" == "" ]; then
	NormalizedRssiList="1,2"
	psmcli set dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WHIX.NormalizedRssiList "1,2"
fi
nrlist=($(getarray "$NormalizedRssiList"))

CliStatList=`psmcli get dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WHIX.CliStatList`
clilist=($(getarray "$CliStatList"))

SnrList=`psmcli get dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WIFI_TELEMETRY.SNRList`
if [ "$SnrList" == "" ]; then
        SnrList="1,2"
        psmcli set dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WIFI_TELEMETRY.SNRList "1,2"
fi
snrlist=($(getarray "$SnrList"))

#ath0
print_connected_client_info 0 "${trlist[0]}" "${nrlist[0]}" "${clilist[0]}" "${snrlist[0]}"

#ath1
print_connected_client_info 1 "${trlist[1]}" "${nrlist[1]}" "${clilist[1]}" "${snrlist[1]}"

#ath2
print_connected_client_info 2 "${trlist[2]}" "${nrlist[2]}" "${clilist[2]}" "${snrlist[2]}"

#ath3
print_connected_client_info 3 "${trlist[3]}" "${nrlist[3]}" "${clilist[3]}" "${snrlist[3]}"

#ath4
print_connected_client_info 4 "${trlist[4]}" "${nrlist[4]}" "${clilist[4]}" "${snrlist[4]}"

#ath5
print_connected_client_info 5 "${trlist[5]}" "${nrlist[5]}" "${clilist[5]}" "${snrlist[5]}"

#ath6
print_connected_client_info 6 "${trlist[6]}" "${nrlist[6]}" "${clilist[6]}" "${snrlist[6]}"

#ath7
print_connected_client_info 7 "${trlist[7]}" "${nrlist[7]}" "${clilist[7]}" "${snrlist[7]}"

