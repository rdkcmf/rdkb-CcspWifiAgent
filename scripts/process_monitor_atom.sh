#! /bin/sh
. /etc/device.properties
touch /rdklogs/logs/hostapd_error_log.txt
touch /rdklogs/logs/ap_init.txt.0
BINPATH="/usr/bin"
TELNET_SCRIPT_PATH="/usr/ccsp"
RDKLOGGER_PATH="/rdklogger"
echo "Inside process monitor script"
loop=1
Subsys="eRT."	
check_dmesg=""
check_dmesg_acl_in=""
check_dmesg_acl_out=""
check_dmesg_wps_gpio_dump=""
check_dmesg_deauth=""
time=0
WIFI_RESTART=0
AP_UP_COUNTER=0
FASTDOWN_COUNTER=0
rc=0
newline="
"
if [ -e /rdklogger/log_capture_path_atom.sh ]
then
	source /rdklogger/log_capture_path_atom.sh 
else
	echo_t()
	{
        	echo $1
	}
fi

while [ $loop -eq 1 ]
do
	sleep 300
 	if [ -f /lib/rdk/wifi_config_profile_check.sh ];then
		source /lib/rdk/wifi_config_profile_check.sh 
                rc=$?
                if [ "$rc" == "0" ]; then
			APUP_PID=`pidof apup`
 			if [ -f /etc/ath/fast_down.sh ];then
                		FASTDOWN_PID=`pidof fast_down.sh`
			else	
                		FASTDOWN_PID=`pidof apdown`
                	fi
			if [ "$APUP_PID" == "" ] && [ "$FASTDOWN_PID" == "" ]; then
				dmcli eRT setv Device.X_CISCO_COM_DeviceControl.RebootDevice string Wifi
                                continue
			fi
                fi
        fi
			
	cd /usr/ccsp/wifi
	WiFi_PID=`pidof CcspWifiSsp`
	if [ "$WiFi_PID" == "" ]; then
		echo_t "WiFi process is not running, restarting it"
		cd /usr/ccsp/wifi
		export LD_LIBRARY_PATH=$PWD:.:$PWD/../../lib:$PWD/../../.:/lib:/usr/lib:$LD_LIBRARY_PATH
                echo_t "RDKB_PROCESS_CRASHED : WiFiAgent_process is not running, need restart"
 		if [ -f /etc/ath/fast_down.sh ];then
                	FASTDOWN_PID=`pidof fast_down.sh`
		else	
                	FASTDOWN_PID=`pidof apdown`
                fi
		APUP_PID=`pidof apup`
		if [ $FASTDOWN_COUNTER -eq 0 ] && [ "$APUP_PID" == "" ] && [ "$FASTDOWN_PID" == "" ]; then
 			if [ -f /etc/ath/fast_down.sh ];then
				/etc/ath/fast_down.sh
                		rc=$?
			else
				/etc/ath/apdown
			fi
                        FASTDOWN_COUNTER=$(($FASTDOWN_COUNTER + 1))
		fi
                
                if [ "$rc" != "0" ]; then
			echo_t "fast_down did not succeed, will retry"
                else
			FASTDOWN_COUNTER=0        
                fi
 		if [ -f /etc/ath/fast_down.sh ];then
                	FASTDOWN_PID=`pidof fast_down.sh`
		else	
                	FASTDOWN_PID=`pidof apdown`
                fi
		APUP_PID=`pidof apup`
		if [ "$APUP_PID" == "" ] && [ "$FASTDOWN_PID" == "" ] && [ $FASTDOWN_COUNTER -eq 0 ]; then
			$BINPATH/CcspWifiSsp -subsys $Subsys &
                	sleep 60
                else 
			echo_t "WiFiAgent_process restart was skipped, fastdown or apup in progress"
		fi
	else
        	WIFI_RESTART=0
		APUP_PID=`pidof apup`
 		if [ -f /etc/ath/fast_down.sh ];then
                	FASTDOWN_PID=`pidof fast_down.sh`
		else	
                	FASTDOWN_PID=`pidof apdown`
                fi
		check_radio=`cfg -e | grep AP_RADIO_ENABLED`
                if [ "$check_radio" != "" ]; then
			check_radio_enable5=`cfg -e | grep AP_RADIO_ENABLED=1 | cut -d"=" -f2`
                	check_radio_enable2=`cfg -e | grep AP_RADIO_ENABLED_2=1 | cut -d"=" -f2`
		else
			check_radio_enable5=`cfg -e | grep RADIO_ENABLE_2=1 | cut -d"=" -f2`
                	check_radio_enable2=`cfg -e | grep RADIO_ENABLE=1 | cut -d"=" -f2`
		fi
                check_radio_intf_up=`cat /rdklogs/logs/ap_init.txt.0 | grep "PCI rescan's were met without successfull recovery, exiting apup"`
			
		if [ "$check_radio_enable5" == "1" ] || [ "$check_radio_enable2" == "1" ]; then
			if [ "$APUP_PID" == "" ] && [ "$FASTDOWN_PID" == "" ] && [ $FASTDOWN_COUNTER -eq 0 ]; then
                                AP_UP_COUNTER=0
				HOSTAPD_PID=`pidof hostapd`
				if [ "$HOSTAPD_PID" == "" ]; then
				WIFI_RESTART=1
				echo_t "RDKB_PROCESS_CRASHED : Hostapd_process is not running, restarting WiFi"
			       
				else
					check_ap_enable5=`cfg -e | grep AP_ENABLE_2=1 | cut -d"=" -f2`
					check_interface_up5=`ifconfig | grep ath1`
					check_ap_enable2=`cfg -e | grep AP_ENABLE=1 | cut -d"=" -f2`
					check_interface_up2=`ifconfig | grep ath0`
					check_interface_iw2=`iwconfig ath0 | grep Access | awk '{print $6}'`
					check_interface_iw5=`iwconfig ath1 | grep Access | awk '{print $6}'`
					check_hostapd_ath0=`cat /proc/$HOSTAPD_PID/cmdline | grep ath0`
					check_hostapd_ath1=`cat /proc/$HOSTAPD_PID/cmdline | grep ath1`
					check_wps_ath0=`cfg -e | grep WPS_ENABLE=0`
					check_wps_ath1=`cfg -e | grep WPS_ENABLE_2=0`
					check_ap_sec_mode_2=`cfg -e | grep AP_SECMODE=WPA`
					check_ap_sec_mode_5=`cfg -e | grep AP_SECMODE_2=WPA`
					check_ap_tkip_cfg=$(cfg -s | grep 'TKIP\\')                     
                			if [ "$check_ap_tkip_cfg" != "" ]; then                         
                        			echo_t "TKIP has backslash"  
                                                continue      
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
                                                check_lspci_2g_pcie=`lspci | grep "03:00.0 Class 0200: 168c:abcd"` 
                                                check_target_asserted=`dmesg | grep "TARGET ASSERTED"` 
					
						echo_t "2G_counters:$check_apstats_iw2_p_req,$check_apstats_iw2_p_res,$check_apstats_iw2_au_req,$check_apstats_iw2_au_resp"
						echo_t "5G_counters:$check_apstats_iw5_p_req,$check_apstats_iw5_p_res,$check_apstats_iw5_au_req,$check_apstats_iw5_au_resp"
						tmp=`dmesg | grep "resetting hardware for Rx stuck"`
						tmp_acl_in=`dmesg | grep "added in acl list"`
						tmp_acl_out=`dmesg | grep "removed from acl list"`
                                                tmp_dmesg_dump=`dmesg | grep "wps_gpio Tainted"`
                                                tmp_dmesg_deauth=`dmesg | grep "mlme_deauth_reason"`
						if [ "$tmp" == "$check_dmesg" ]; then 
							check_dmesg=""
						else
							check_dmesg="$tmp"
						fi
						if [ "$check_dmesg" != "" ]; then
							echo_t "Resetting WiFi hardware for Rx stuck"
						fi
						check_dmesg="$tmp"

						if [ "$tmp_acl_in" == "$check_dmesg_acl_in" ]; then 
							check_dmesg_acl_in=""
						else
							check_dmesg_acl_in="$tmp_acl_in"
						fi
						if [ "$check_dmesg_acl_in" != "" ]; then
							echo_t "$check_dmesg_acl_in"
						fi
						check_dmesg_acl_in="$tmp_acl_in"

						if [ "$tmp_acl_out" == "$check_dmesg_acl_out" ]; then 
							check_dmesg_acl_out=""
						else
							check_dmesg_acl_out="$tmp_acl_out"
						fi
						if [ "$check_dmesg_acl_out" != "" ]; then
							echo_t "$check_dmesg_acl_out"
						fi
						check_dmesg_acl_out="$tmp_acl_out"     
						
						if [ "$tmp_dmesg_dump" == "$check_dmesg_wps_gpio_dump" ]; then 
							check_dmesg_wps_gpio_dump=""
						else
							check_dmesg_wps_gpio_dump="$tmp_dmesg_dump"
						fi
						if [ "$check_dmesg_wps_gpio_dump" != "" ]; then
							echo_t "ATOM_WIFI_KERNEL_MODULE_DUMP : wps_gpio related kernel_dump seen"
						fi
                                                check_dmesg_wps_gpio_dump="$tmp_dmesg_dump"
						if [ "$tmp_dmesg_deauth" == "$check_dmesg_deauth" ]; then 
							check_dmesg_deauth=""
						else
							check_dmesg_deauth="$tmp_dmesg_deauth"
						fi
						if [ "$check_dmesg_deauth" != "" ]; then
							echo_t "check_dmesg_deauth"
						fi
						check_dmesg_deauth="$tmp_dmesg_deauth"      
						if [ "$check_lspci_2g_pcie" != "" ]; then
							echo_t "PCIE device_class of 2G radio is wrong"
                                                        
						fi
						if [ "$check_target_asserted" != "" ]; then
							echo_t "TARGET_ASSERTED"
                                                      
						fi
						if [ "$check_radio_intf_up" != "" ]; then
							echo_t "RADIO_NOT_UP"
                                                       
						fi
						if [ "$check_lspci_2g_pcie" != "" ] || [ "$check_target_asserted" != "" ] || [ "$check_radio_intf_up" != "" ]; then
							echo_t "skipping hostapd restart"
							continue
						fi
						time=0
					fi
					if [ "$check_radio_enable2" == "1" ] && [ "$check_ap_enable2" == "1" ] && [ "$check_ap_sec_mode_2" != "" ] && [ "$check_wps_ath0" == "" ] && [ "$check_hostapd_ath0" == "" ]; then
						echo_t "Hostapd incorrect config, restarting WiFi"
						#WIFI_RESTART=1 currently monitoring this
					fi
					if [ "$check_radio_enable5" == "1" ] && [ "$check_ap_enable5" == "1" ] && [ "$check_ap_sec_mode_5" != "" ] && [ "$check_wps_ath1" == "" ] && [ "$check_hostapd_ath1" == "" ]; then
						echo_t "Hostapd incorrect config, restarting WiFi"
						#WIFI_RESTART=1 currently monitoring this
					fi
					

					if [ "$check_ap_enable2" == "1" ] && [ "$check_radio_enable2" == "1" ] && [ "$check_interface_iw2" == "Not-Associated" ]; then
						echo_t "ath0 is Not-Associated, restarting WiFi"
						WIFI_RESTART=1
					fi

					if [ "$check_ap_enable5" == "1" ] && [ "$check_radio_enable5" == "1" ] && [ "$check_interface_iw5" == "Not-Associated" ]; then
						echo_t "ath1 is Not-Associated, restarting WiFi"
						WIFI_RESTART=1
					fi


					if [ "$check_ap_enable5" == "1" ] && [ "$check_radio_enable5" == "1" ] && [ "$check_interface_up5" == "" ]; then
						echo_t "ath1 is down, restarting WiFi"
						WIFI_RESTART=1
					fi
				
					if [ "$check_ap_enable2" == "1" ] && [ "$check_radio_enable2" == "1" ] && [ "$check_interface_up2" == "" ]; then
						echo_t "ath0 is down, restarting WiFi"
						WIFI_RESTART=1
					fi
				fi
				
		     else
                                AP_UP_COUNTER=$(($AP_UP_COUNTER + 1))
				echo_t "APUP stuck : APUP is running"
				if [ $AP_UP_COUNTER -eq 3 ]; then
					echo_t "APUP stuck : restarting WiFi"
                                        AP_UP_COUNTER=0
					#kill -9 $APUP_PID
					#WIFI_RESTART=1
				fi
			
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
                echo_t "Harvester process is not running, restarting it"
                $BINPATH/harvester &
        fi

	Telnet_Pid=`pidof telnetd`
	if [ "$Telnet_Pid" == "" ]; then
		if [ -f $TELNET_SCRIPT_PATH/telnet-start ]
		then
			echo_t "RDKB_PROCESS_CRASHED : Telnet is not running, restarting Telnet"
			$TELNET_SCRIPT_PATH/telnet-start
		else
			echo_t "Telnet script not found"
		fi
	fi

	Dropbear_Pid=`pidof dropbear`
	if [ "$Dropbear_Pid" = "" ]
	then
		if [ -f $TELNET_SCRIPT_PATH/dropbear-start ]
		then
		   echo_t "RDKB_PROCESS_CRASHED : dropbear is not running in ATOM, restarting dropbear"
		   $TELNET_SCRIPT_PATH/dropbear-start
		else
			echo_t "dropbear script not found"
		fi
	fi

       if [ "$BOX_TYPE" = "XB3" ] && [ -f "/etc/webgui_atom.sh" ]
       then
          Lighttpd_PID=`pidof lighttpd`
          if [ "$Lighttpd_PID" = "" ]
          then
          	isPortKilled=`netstat -anp | awk '/:51515 */ {split($NF,a,"/"); print a[2],a[1]}' | cut -f2 -d" "`
		if [ "$isPortKilled" != "" ]
		    then
			 Process_PID=`netstat -anp | awk '/:51515 */ {split($NF,a,"/"); print a[2],a[1]}'`
		         echo "[`getDateTime`] Port 51515 is still alive. Killing processes $Process_PID"
		         kill -9 $isPortKilled
		 fi         	
             echo_t "RDKB_PROCESS_CRASHED : Lighttpd is not running in ATOM, restarting lighttpd"
             sh /etc/webgui_atom.sh &
          fi
       fi
   
       if [ -f $TELNET_SCRIPT_PATH/arping_to_arm ]
       then
           $TELNET_SCRIPT_PATH/arping_to_arm
       else
           echo_t "arping_to_arm not found"
       fi


done

