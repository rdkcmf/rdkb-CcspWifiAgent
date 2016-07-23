#! /bin/sh

BINPATH="/usr/bin"

echo "Inside process monitor script"
loop=1
Subsys="eRT."	
check_dmesg=""
time=0
newline="
"
while [ $loop -eq 1 ]
do
	sleep 300
	
	cd /usr/ccsp/wifi
	WiFi_PID=`pidof CcspWifiSsp`
	if [ "$WiFi_PID" == "" ]; then
		echo "WiFi process is not running, restarting it"
		cd /usr/ccsp/wifi
		export LD_LIBRARY_PATH=$PWD:.:$PWD/../../lib:$PWD/../../.:/lib:/usr/lib:$LD_LIBRARY_PATH
                dmcli eRT setv Device.LogAgent.WifiLogMsg string "RDK_LOG_ERROR,RDKB_PROCESS_CRASHED : WiFiAgent_process is not running, need restart $newline"
		sh /etc/ath/fast_down.sh
		$BINPATH/CcspWifiSsp -subsys $Subsys &
                sleep 60
	else
		WIFI_RESTART=0
		APUP_PID=`pidof apup`
		if [ "$APUP_PID" == "" ]; then
                         
			HOSTAPD_PID=`pidof hostapd`
			if [ "$HOSTAPD_PID" == "" ]; then
                        WIFI_RESTART=1
			echo "Hostapd process is not running, restarting WiFi"
                	dmcli eRT setv Device.LogAgent.WifiLogMsg string "RDK_LOG_ERROR,RDKB_PROCESS_CRASHED : Hostapd_process is not running, restarting WiFi $newline"
			#dmcli eRT setv Device.X_CISCO_COM_DeviceControl.RebootDevice string Wifi"
                       
			fi
                        
			check_ap_enable5=`cfg -e | grep AP_ENABLE_2=1 | cut -d"=" -f2`
			check_radio_enable5=`cfg -e | grep AP_RADIO_ENABLED=1 | cut -d"=" -f2`
			check_interface_up5=`ifconfig | grep ath1`
			check_ap_enable2=`cfg -e | grep AP_ENABLE=1 | cut -d"=" -f2`
                        check_radio_enable2=`cfg -e | grep AP_RADIO_ENABLED_2=1 | cut -d"=" -f2`
                        check_interface_up2=`ifconfig | grep ath0`
                        check_interface_iw2=`iwconfig ath0 | grep Access | awk '{print $6}'`
                        check_interface_iw5=`iwconfig ath1 | grep Access | awk '{print $6}'`
                        
			tmp=`dmesg | grep "resetting hardware for Rx stuck"`
                        if [ "$tmp" == "$check_dmesg" ]; then 
				check_dmesg=""
                        else
				check_dmesg="$tmp"
			fi
			time=$(($time + 300))
			if [ $time -eq 1800 ]; then
				check_apstats_iw2_p_req=`apstats -v -i ath0 | grep "Rx Probe request" | awk '{print $5}'`
				check_apstats_iw2_p_res=`apstats -v -i ath0 | grep "Tx Probe response" | awk '{print $5}'`
				check_apstats_iw2_au_req=`apstats -v -i ath0 | grep "Rx auth request" | awk '{print $5}'`
				check_apstats_iw2_au_resp=`apstats -v -i ath0 | grep "Tx auth response" | awk '{print $5}'`
				 
				check_apstats_iw5_p_req=`apstats -v -i ath1 | grep "Rx Probe request" | awk '{print $5}'`
				check_apstats_iw5_p_res=`apstats -v -i ath1 | grep "Tx Probe response" | awk '{print $5}'`
				check_apstats_iw5_au_req=`apstats -v -i ath1 | grep "Rx auth request" | awk '{print $5}'`
				check_apstats_iw5_au_resp=`apstats -v -i ath1 | grep "Tx auth response" | awk '{print $5}'`
                        
				dmcli eRT setv Device.LogAgent.WifiLogMsg string "RDK_LOG_ERROR,2G_counters:$check_apstats_iw2_p_req,$check_apstats_iw2_p_res,$check_apstats_iw2_au_req,$check_apstats_iw2_au_resp $newline"
				dmcli eRT setv Device.LogAgent.WifiLogMsg string "RDK_LOG_ERROR,5G_counters:$check_apstats_iw5_p_req,$check_apstats_iw5_p_res,$check_apstats_iw5_au_req,$check_apstats_iw5_au_resp $newline"
                        	time=0
			fi
                        

			if [ "$check_ap_enable2" == "1" ] && [ "$check_radio_enable2" == "1" ] && [ "$check_interface_iw2" == "Not-Associated" ]; then
				echo "ath0 is Not-Associated, restarting WiFi"
				dmcli eRT setv Device.LogAgent.WifiLogMsg string "RKDB_LOG_ERROR: ath0 is Not-Associated, restarting WiFi $newline"
                        	WIFI_RESTART=1
				#dmcli eRT setv Device.X_CISCO_COM_DeviceControl.RebootDevice string Wifi
			fi

			if [ "$check_ap_enable5" == "1" ] && [ "$check_radio_enable5" == "1" ] && [ "$check_interface_iw5" == "Not-Associated" ]; then
				echo "ath1 is Not-Associated, restarting WiFi"
				dmcli eRT setv Device.LogAgent.WifiLogMsg string "RKDB_LOG_ERROR: ath1 is Not-Associated, restarting WiFi $newline"
                        	WIFI_RESTART=1
				#dmcli eRT setv Device.X_CISCO_COM_DeviceControl.RebootDevice string Wifi
			fi


			if [ "$check_ap_enable5" == "1" ] && [ "$check_radio_enable5" == "1" ] && [ "$check_interface_up5" == "" ]; then
				echo "ath1 is down, restarting WiFi"
				dmcli eRT setv Device.LogAgent.WifiLogMsg string "RKDB_LOG_ERROR: ath1 is down, restarting WiFi $newline"
                        	WIFI_RESTART=1
				#dmcli eRT setv Device.X_CISCO_COM_DeviceControl.RebootDevice string Wifi
			fi
		
			if [ "$check_ap_enable2" == "1" ] && [ "$check_radio_enable2" == "1" ] && [ "$check_interface_up2" == "" ]; then
				echo "ath0 is down, restarting WiFi"
				dmcli eRT setv Device.LogAgent.WifiLogMsg string "RKDB_LOG_ERROR: ath0 is down, restarting WiFi $newline"
                        	WIFI_RESTART=1
				#dmcli eRT setv Device.X_CISCO_COM_DeviceControl.RebootDevice string Wifi
			fi
			
			if [ "$check_dmesg" != "" ]; then
				echo "resetting wifi hardware for Rx stuck"
				dmcli eRT setv Device.LogAgent.WifiLogMsg string "RKDB_LOG_ERROR: resetting WiFi hardware for Rx stuck $newline"
			fi

			if [ "$WIFI_RESTART" == "1" ]; then
				dmcli eRT setv Device.X_CISCO_COM_DeviceControl.RebootDevice string Wifi
                                sleep 60
			fi
		fi
	fi
			

        cd /usr/ccsp/harvester
        Harvester_PID=`pidof harvester`
        if [ "$Harvester_PID" == "" ]; then
                echo "Harvester process is not running, restarting it"
                $BINPATH/harvester &
        fi
done

