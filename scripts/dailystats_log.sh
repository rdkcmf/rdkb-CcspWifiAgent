#!/bin/sh
uptime=`cat /proc/uptime | awk '{ print $1 }' | cut -d"." -f1`
echo "before running dailystats.sh printing top output" >> /rdklogs/logs/AtomConsolelog.txt.0
top -n1 >> /rdklogs/logs/AtomConsolelog.txt.0
if [ $uptime -gt 1800 ] && [ "$(pidof CcspWifiSsp)" != "" ] && [ "$(pidof hostapd)" != "" ]  && [ "$(pidof apup)" == "" ] && [ "$(pidof fastdown)" == "" ] && [ "$(pidof apdown)" == "" ]  && [ "$(pidof aphealth.sh)" == "" ] && [ "$(pidof stahealth.sh)"  == "" ] && [ "$(pidof radiohealth.sh)" == "" ] && [ "$(pidof aphealth_log.sh)" == "" ] && [ "$(pidof radiohealth_log.sh)" == "" ] && [ "$(pidof stahealth_log.sh)" == "" ] && [ "$(pidof bandsteering.sh)" == "" ] && [ "$(pidof bandsteering_log.sh)" == "" ] && [ "$(pidof l2shealth_log.sh)" == "" ] && [ "$(pidof l2shealth.sh)" == "" ]  && [ "$(pidof log_mem_cpu_info_atom.sh)" == "" ] ; then
        /usr/ccsp/wifi/dailystats.sh >> /rdklogs/logs/wifihealth.txt
        echo "after running dailystats.sh printing top output" >> /rdklogs/logs/AtomConsolelog.txt
        top -n1 >> /rdklogs/logs/AtomConsolelog.txt.0
else
        echo "skipping dailystats.sh run" >> /rdklogs/logs/AtomConsolelog.txt.0
	
fi


