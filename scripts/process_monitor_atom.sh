#! /bin/sh

BINPATH="/usr/bin"

echo "Inside process monitor script"
loop=1
Subsys="eRT."
while [ $loop -eq 1 ]
do
	sleep 300
	cd /usr/ccsp/wifi
	WiFi_PID=`pidof CcspWifiSsp`
	if [ "$WiFi_PID" = "" ]; then
		echo "WiFi process is not running, restarting it"
		export LD_LIBRARY_PATH=$PWD:.:$PWD/../../lib:$PWD/../../.:/lib:/usr/lib:$LD_LIBRARY_PATH
		$BINPATH/CcspWifiSsp -subsys $Subsys &
	fi

	HOSTAPD_PID=`pidof hostapd`
	if [ "$HOSTAPD_PID" = "" ]; then
		echo "Hostapd process is not running, restarting it"
		hostapd -B `cat /tmp/conf_filename` &
	fi

        cd /usr/ccsp/harvester
        Harvester_PID=`pidof harvester`
        if [ "$Harvester_PID" = "" ]; then
                echo "Harvester process is not running, restarting it"
                $BINPATH/harvester &
        fi
done

