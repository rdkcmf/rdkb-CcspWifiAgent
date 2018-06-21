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
#This script is used to log parameters for each radio

LOG_FOLDER="/rdklogs/logs"
WiFi_Health_LogFile="$LOG_FOLDER/wifihealth.txt"
source /etc/log_timestamp.sh

exec 3>&1 4>&2 >>$WiFi_Health_LogFile 2>&1

logfolder="/tmp/wifihealth"
INDEX_LIST="0 2 4 6 8 10 12 14"
Interfaces_Active=""

if [ ! -d "$logfolder" ] ; then
	mkdir "$logfolder";
fi
THRESHOLD_REACHED=0
RADIO_UTIL_2G=`wifi_api wifi_getRadioBandUtilization 0`
RADIO_UTIL_5G=`wifi_api wifi_getRadioBandUtilization 1`

CHANNEL_THREASHOLD_2G=`psmcli get eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.1.SetChanUtilThreshold`
if [ "$CHANNEL_THREASHOLD_2G" == "" ] ; then
	CHANNEL_THREASHOLD_2G=0;
fi

#short term workaround before wifi_api avaliabel
#bu1=`rdk_band_steering_hal wifi_getRadioBandUtilization 0 | grep ":" | cut -d":" -f2 | tr -d " %"`
#bu2=`rdk_band_steering_hal wifi_getRadioBandUtilization 1 | grep ":" | cut -d":" -f2 | tr -d " %"`

if [ "$RADIO_UTIL_2G" == "" ] ; then
	RADIO_UTIL_2G=0;
fi
if [ "$RADIO_UTIL_5G" == "" ] ; then
        RADIO_UTIL_5G=0;
fi

echo_t "WIFI_BANDUTILIZATION_1:$RADIO_UTIL_2G"
echo_t "WIFI_BANDUTILIZATION_2:$RADIO_UTIL_5G"

if [ "$RADIO_UTIL_2G" -ge "$CHANNEL_THREASHOLD_2G" ];then
	THRESHOLD_REACHED=1
	sleep 60;
	RADIO_UTIL_2G=`wifi_api wifi_getRadioBandUtilization 0`
	echo_t "WIFI_BANDUTILIZATION_1_ITER1:$RADIO_UTIL_2G"
	if [ "$RADIO_UTIL_2G" -lt "$CHANNEL_THREASHOLD_2G" ];then
		THRESHOLD_REACHED=0
	fi
	sleep 120
	RADIO_UTIL_2G=`wifi_api wifi_getRadioBandUtilization 0`
	echo_t "WIFI_BANDUTILIZATION_1_ITER2:$RADIO_UTIL_2G"
	if [ "$RADIO_UTIL_2G" -lt "$CHANNEL_THREASHOLD_2G" ];then
		THRESHOLD_REACHED=0
	fi

fi

if [ "$THRESHOLD_REACHED" -eq 0 ];then
	exit
fi

ChanUtilSelfHealEnable_2G=`psmcli get eRT.com.cisco.spvtg.ccsp.Device.WiFi.Radio.1.ChanUtilSelfHealEnable`
if [ "$ChanUtilSelfHealEnable_2G" == "" ] ; then
	ChanUtilSelfHealEnable_2G=0;
fi

if [ "$ChanUtilSelfHealEnable_2G" != "0" ];
then
	lastActiontakentimeforChanUtil=`syscfg get lastActiontakentimeforChanUtil`
	if [ "$lastActiontakentimeforChanUtil" != "" ];then
		currTime=$(date -u +"%s")
		time_diff_in_seconds=$(($currTime-$lastActiontakentimeforChanUtil))
		Num_Of_Secs_PerDay=86400
		echo "time_diff_in_seconds = $time_diff_in_seconds"
		if [ "$time_diff_in_seconds" -ge "$Num_Of_Secs_PerDay" ];then
			lastActiontakentimeforChanUtil=0;
		fi
	fi
	
	if [ "$lastActiontakentimeforChanUtil" == "" ] || [ "$lastActiontakentimeforChanUtil" == "0" ];then
		echo_t "ChanUtilSelfHealEnable value is $ChanUtilSelfHealEnable_2G"
		if [ "$ChanUtilSelfHealEnable_2G" = "1" ];then
			echo_t "WIFI_BANDUTILIZATION : Threshold value is reached, resetting 2.4 WiFi"
			for index in $INDEX_LIST
			do
				interface_state=`ifconfig ath$index | grep UP`
				echo "ath$index : $interface_state"
				if [ "$interface_state" != "" ];then
					ifconfig ath$index down
					Interfaces_Active="$Interfaces_Active $index"
				fi
			done
			sleep 1
		
			for index in $Interfaces_Active
			do
				iwconfig ath$index channel 0
			done

			sleep 1

			for index in $Interfaces_Active
			do
				ifconfig ath$index up
			done
		
		elif [ "$ChanUtilSelfHealEnable_2G" = "2" ]; then
				echo_t "WIFI_BANDUTILIZATION : Threshold value is reached, resetting WiFi"
				dmcli eRT setv Device.X_CISCO_COM_DeviceControl.RebootDevice string Wifi
		else
				echo_t "WIFI_BANDUTILIZATION : Wrong value is set to ChanUtilSelfHealEnable"
		fi
		
		if [ "$ChanUtilSelfHealEnable_2G" = "1" ] || [ "$ChanUtilSelfHealEnable_2G" = "2" ];then
			storeWiFiRebootTime=$(date -u +"%s")
			syscfg set lastActiontakentimeforChanUtil "$storeWiFiRebootTime"
			syscfg commit
		fi
	fi
fi
