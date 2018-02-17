#!/bin/sh

logfolder="/tmp/wifihealth"

if [ ! -d "$logfolder" ] ; then
        mkdir "$logfolder";
fi


LogInterval_def=3600

if [ -f /etc/device.properties ]; then
    source /etc/device.properties
fi
source /etc/log_timestamp.sh

t=`date -u +"%s"`
while [ "$t" -lt 1518653903 ] ; do
    sleep 60
    t=`date -u +"%s"`
done

t0=$t
m=0;
if [ -f /nvram/wifihealth/up_t0 ]; then
	t=`cat /nvram/wifihealth/up_t0`
	rm /nvram/wifihealth/up_t0
	if [ "$t" -lt 1518653903 ]; then
		m=$(($t0/60-$t/60))
		t0=$t
	fi
fi

a=(0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0)
if [ -f /nvram/wifihealth/up_ct ]; then
	a=($(cat /nvram/wifihealth/up_ct))
	rm /nvram/wifihealth/up_ct
fi

LogInterval=$LogInterval_def;
forever=1;
timeleft=$LogInterval;
c=0
while [ "$forever" -eq "1" ]; do
	sleep 60
	m=$(($m+1));
	c=$(($c+1));
	timeleft=$(($timeleft-60));

	#current_time:ct_0;...ct_15
	t=`date -u +"%s"`
	line=""
	for i in {0..15}; do
		up=`wifi_api wifi_getApStatus $i`
		if [ "$up" == "Up" ]; then
			a[$i]=$((a[$i]+1))
		fi
		line="$line ${a[$i]}"
	done
	#echo "$t" > /tmp/wifihealth/up_t
	echo "$line" > /tmp/wifihealth/up_ct

	if [ "$c" -gt 4 ]; then
		c=0
 		LogInterval_old=$LogInterval;
		LogInterval=`psmcli get dmsb.device.deviceinfo.X_RDKCENTRAL-COM_WHIX.LogInterval`
        	if [ "$LogInterval" == "" ] ; then
                	LogInterval=$LogInterval_def
        	fi
		timeleft=$(($timeleft+$LogInterval-$LogInterval_old))
	fi

	if [ "$timeleft" -le 0 ]; then
		if [ "$m" -lt 1 ]; then
			m=1;
		fi
		idx=0
		line="WIFI_VAP_PERCENT_UP:"
	        for i in {0..15}; do
                        a[$i]=$((a[$i]*100/$m))
			idx=$(($i+1))
                	line="$line$idx,${a[$i]};"
        	done
		echo_t "$line" >> /rdklogs/logs/wifihealth.txt

		a=(0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0)
		t0=$t
		m=0
		echo "$t0" > /tmp/wifihealth/up_t0
		echo "" > /tmp/wifihealth/up_ct
		timeleft=$LogInterval;
	fi
done
