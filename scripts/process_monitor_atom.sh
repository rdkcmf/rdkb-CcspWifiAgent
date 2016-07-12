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
	if [ "$WiFi_PID" == "" ]; then
		echo "WiFi process is not running, restarting it"
		export LD_LIBRARY_PATH=$PWD:.:$PWD/../../lib:$PWD/../../.:/lib:/usr/lib:$LD_LIBRARY_PATH
                dmcli eRT setv Device.LogAgent.WifiLogMsg string "RDK_LOG_ERROR,RDKB_PROCESS_CRASHED : WiFiAgent_process is not running, need restart"
		$BINPATH/CcspWifiSsp -subsys $Subsys &
	else
		APUP_PID=`pidof apup`
		if [ "$APUP_PID" == "" ]; then
			check_ap_enable5=`cfg -e | grep AP_ENABLE_2=1 | cut -d"=" -f2`
			check_radio_enable5=`cfg -e | grep AP_RADIO_ENABLED=1 | cut -d"=" -f2`
			check_interface_up5=`ifconfig | grep ath1`
			check_ap_enable2=`cfg -e | grep AP_ENABLE=1 | cut -d"=" -f2`
                        check_radio_enable2=`cfg -e | grep AP_RADIO_ENABLED_2=1 | cut -d"=" -f2`
                        check_interface_up2=`ifconfig | grep ath0`


			if [ "$check_ap_enable5" == "1" ] && [ "$check_radio_enable5" == "1" ] && [ "$check_interface_up5" == "" ]; then
				echo "ath1 is down, restarting WiFi"
				dmcli eRT setv Device.LogAgent.WifiLogMsg string "[RKDB_PLATFORM_ERROR]: ath1 is down, restarting WiFi"
				dmcli eRT setv Device.X_CISCO_COM_DeviceControl.RebootDevice string Wifi
			elif [ "$check_ap_enable2" == "1" ] && [ "$check_radio_enable2" == "1" ] && [ "$check_interface_up2" == "" ]; then
                                echo "ath0 is down, restarting WiFi"
                                dmcli eRT setv Device.LogAgent.WifiLogMsg string "[RKDB_PLATFORM_ERROR]: ath0 is down, restarting WiFi"
                                dmcli eRT setv Device.X_CISCO_COM_DeviceControl.RebootDevice string Wifi
                        else
				HOSTAPD_PID=`pidof hostapd`
				if [ "$HOSTAPD_PID" = "" ]; then
				echo "Hostapd process is not running, restarting WiFi"
                		dmcli eRT setv Device.LogAgent.WifiLogMsg string "RDK_LOG_ERROR,RDKB_PROCESS_CRASHED : Hostapd_process is not running, restarting WiFi"
				dmcli eRT setv Device.X_CISCO_COM_DeviceControl.RebootDevice string Wifi
				fi
			fi
		fi
	fi

        cd /usr/ccsp/harvester
        Harvester_PID=`pidof harvester`
        if [ "$Harvester_PID" = "" ]; then
                echo "Harvester process is not running, restarting it"
                $BINPATH/harvester &
        fi
done

