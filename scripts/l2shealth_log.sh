#!/bin/sh
uptime=`cat /proc/uptime | awk '{ print $1 }' | cut -d"." -f1`
echo "before running l2shealth.sh printing top" >> /rdklogs/logs/AtomConsolelog.txt.0
top -n1 >> /rdklogs/logs/AtomConsolelog.txt.0
if [ $uptime -gt 1800 ] && [ "$(pidof CcspWifiSsp)" != "" ] && [ "$(pidof hostapd)" != "" ]  && [ "$(pidof apup)" == "" ] && [ "$(pidof fastdown)" == "" ] && [ "$(pidof apdown)" == "" ]  && [ "$(pidof aphealth.sh)" == "" ] && [ "$(pidof stahealth.sh)"  == "" ] && [ "$(pidof radiohealth.sh)" == "" ] && [ "$(pidof aphealth_log.sh)" == "" ] && [ "$(pidof bandsteering.sh)" == "" ] && [ "$(pidof l2shealth.sh)" == "" ]  && [ "$(pidof log_mem_cpu_info_atom.sh)" == "" ] ; then
	/usr/ccsp/wifi/l2shealth.sh >> /rdklogs/logs/AtomConsolelog.txt.0
        echo "top after running l2shealth.sh" >> /rdklogs/logs/AtomConsolelog.txt.0
        top -n1 >> /rdklogs/logs/AtomConsolelog.txt.0
else
        echo "skipping l2shealth.sh run" >> /rdklogs/logs/AtomConsolelog.txt.0
	
fi

