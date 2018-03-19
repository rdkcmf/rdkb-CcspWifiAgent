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
#zhicheng_qiu@comcast.com

if [ -f /etc/device.properties ]
then
    source /etc/device.properties
fi

uptime=`cat /proc/uptime | awk '{ print $1 }' | cut -d"." -f1`
while [ "$uptime" -lt 600 ] ; do
    sleep 60
    uptime=`cat /proc/uptime | awk '{ print $1 }' | cut -d"." -f1`
done

forever=1;
while [ "$forever" -eq "1" ]; do
	LogInterval=`psmcli get dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WHIX.LogInterval`
	if [ "$LogInterval" == "" ] ; then
		LogInterval=3600
		psmcli set dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WHIX.LogInterval 3600
	fi

	timeleft=$LogInterval
	while [ "$timeleft" -gt 0 ] ; do
		sleep 300
		LogInterval_old=$LogInterval;
		LogInterval=`psmcli get dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WHIX.LogInterval`
	        if [ "$LogInterval" == "" ] ; then
        	        LogInterval=3600
        	fi
		timeleft=$(($timeleft-300+$LogInterval-$LogInterval_old))
	done;

	echo "before running stahealth.sh printing top output" >> $WIFI_CONSOLE_LOG_NAME
	top -n1 >> $WIFI_CONSOLE_LOG_NAME


	ru=0;
	if [ "$(pidof CcspWifiSsp)" != "" ] && [ "$(pidof aphealth.sh)" == "" ] && [ "$(pidof stahealth.sh)"  == "" ] && [ "$(pidof radiohealth.sh)" == "" ] && [ "$(pidof aphealth_log.sh)" == "" ] && [ "$(pidof radiohealth_log.sh)" == "" ] && [ "$(pidof bandsteering.sh)" == "" ] && [ "$(pidof bandsteering_log.sh)" == "" ] && [ "$(pidof log_mem_cpu_info_atom.sh)" == "" ] && [ "$(pidof dailystats.sh)" == "" ] && [ "$(pidof dailystats_log.sh)" == "" ] ; then
		if [ "$BOX_TYPE" == "XB3" ]; then
			if [ "$(pidof hostapd)" != "" ]  && [ "$(pidof apup)" == "" ] && [ "$(pidof fastdown)" == "" ] && [ "$(pidof apdown)" == "" ]; then
				ru=1;
			fi
		else
			ru=1;
		fi
	fi

	if [ "$ru" == "1" ]; then
		/usr/ccsp/wifi/stahealth.sh >> /rdklogs/logs/wifihealth.txt
		echo "after running stahealth.sh printing top output" >> $WIFI_CONSOLE_LOG_NAME
		top -n1 >> $WIFI_CONSOLE_LOG_NAME
	else
		echo "skipping staheath.sh run" >> $WIFI_CONSOLE_LOG_NAME
	fi
done
