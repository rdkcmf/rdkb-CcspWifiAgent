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
                dmcli eRT setv Device.LogAgent.WifiLogMsg string "RDK_LOG_ERROR,RDKB_PROCESS_CRASHED : WiFiAgent_process is not running, need restart"
		$BINPATH/CcspWifiSsp -subsys $Subsys &
	fi

	check_ap_enable=`cfg -e | grep AP_ENABLE_2=1 | cut -d"=" -f2`
	check_interface_up=`ifconfig | grep ath1 | cut -d"=" -f2`

	if [ "$check_ap_enable" == "1" ] && [ "$check_interface_up" == "" ]
	then
		dmcli eRT setv Device.LogAgent.WifiLogMsg string "[RKDB_PLATFORM_ERROR]: ath1 is down, restarting WiFi"
		dmcli eRT setv Device.X_CISCO_COM_DeviceControl.RebootDevice string Wifi
	fi

	HOSTAPD_PID=`pidof hostapd`
	if [ "$HOSTAPD_PID" = "" ]; then
		echo "Hostapd process is not running, restarting it"
                dmcli eRT setv Device.LogAgent.WifiLogMsg string "RDK_LOG_ERROR,RDKB_PROCESS_CRASHED : Hostapd_process is not running, need restart"
                killall hostapd
                hostapd  -B `cat /tmp/conf_filename` -e /nvram/etc/wpa2/entropy -P /tmp/hostapd.pid 1>&2

	fi

        cd /usr/ccsp/harvester
        Harvester_PID=`pidof harvester`
        if [ "$Harvester_PID" = "" ]; then
                echo "Harvester process is not running, restarting it"
                $BINPATH/harvester &
        fi
done

