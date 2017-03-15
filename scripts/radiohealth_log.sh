#!/bin/sh
uptime=`cat /proc/uptime | awk '{ print $1 }' | cut -d"." -f1`
echo "before running radiohealth.sh printing top output" >> /rdklogs/logs/AtomConsolelog.txt
top -n1 >> /rdklogs/logs/AtomConsolelog.txt
if [ $uptime -gt 1800 ] && [ "$(pidof CcspWifiSsp)" != "" ] && [ "$(pidof hostapd)" != "" ]  && [ "$(pidof apup)" == "" ] && [ "$(pidof fastdown)" == "" ] && [ "$(pidof apdown)" == "" ]  && [ "$(pidof aphealth.sh)" == "" ] && [ "$(pidof stahealth.sh)"  == "" ] && [ "$(pidof radiohealth.sh)" == "" ] && [ "$(pidof aphealth_log.sh)" == "" ] && [ "$(pidof radiohealth_log.sh)" == "" ] && [ "$(pidof stahealth_log.sh)" == "" ] && [ "$(pidof bandsteering.sh)" == "" ] && [ "$(pidof bandsteering_log.sh)" == "" ] && [ "$(pidof l2shealth_log.sh)" == "" ] && [ "$(pidof l2shealth.sh)" == "" ]  && [ "$(pidof log_mem_cpu_info_atom.sh)" == "" ] ; then
        /usr/ccsp/wifi/radiohealth.sh >> /rdklogs/logs/wifihealth.txt
        echo "after running radiohealth.sh printing top output" >> /rdklogs/logs/AtomConsolelog.txt
        top -n1 >> /rdklogs/logs/AtomConsolelog.txt
else
        echo "skipping radioheath.sh run" >> /rdklogs/logs/AtomConsolelog.txt
	
fi


