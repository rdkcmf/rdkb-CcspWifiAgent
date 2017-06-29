#!/bin/sh
uptime=`cat /proc/uptime | awk '{ print $1 }' | cut -d"." -f1`
echo "before running aphealth.sh printing top output" >> /rdklogs/logs/AtomConsolelog.txt.0
top -n1 > /tmp/top.txt
cat /tmp/top.txt >> /rdklogs/logs/AtomConsolelog.txt.0
if [ $uptime -gt 1800 ] && [ "$(pidof CcspWifiSsp)" != "" ] && [ "$(pidof hostapd)" != "" ]  && [ "$(pidof apup)" == "" ] && [ "$(pidof fastdown)" == "" ] && [ "$(pidof apdown)" == "" ]  && [ "$(pidof aphealth.sh)" == "" ] && [ "$(pidof stahealth.sh)"  == "" ] && [ "$(pidof radiohealth.sh)" == "" ] && [ "$(pidof bandsteering.sh)" == "" ] && [ "$(pidof l2shealth_log.sh)" == "" ] && [ "$(pidof l2shealth.sh)" == "" ]  && [ "$(pidof log_mem_cpu_info_atom.sh)" == "" ] && [ "$(pidof dailystats.sh)" == "" ] && [ "$(pidof dailystats_log.sh)" == "" ] ; then
        /usr/ccsp/wifi/aphealth.sh >> /rdklogs/logs/wifihealth.txt
        /usr/ccsp/wifi/stahealth.sh >> /rdklogs/logs/wifihealth.txt
        /usr/ccsp/wifi/radiohealth.sh >> /rdklogs/logs/wifihealth.txt
        /usr/ccsp/wifi/bandsteering.sh >> /rdklogs/logs/bandsteering_periodic_status.txt
        echo "after running aphealth.sh printing top output" >> /rdklogs/logs/AtomConsolelog.txt.0
	top -n1 > /tmp/top.txt
	cat /tmp/top.txt>> /rdklogs/logs/AtomConsolelog.txt.0
else
        echo "skipping apheath.sh run" >> /rdklogs/logs/AtomConsolelog.txt.0
fi


