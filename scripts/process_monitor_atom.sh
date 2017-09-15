#! /bin/sh
. /etc/device.properties
touch /rdklogs/logs/authenticator_error_log.txt
touch /rdklogs/logs/ap_init.txt.0
touch /tmp/acl_add_file1
touch /tmp/acl_add_file2
BINPATH="/usr/bin"
TELNET_SCRIPT_PATH="/usr/ccsp"
RDKLOGGER_PATH="/rdklogger"
echo "Inside process monitor script"
loop=1
Subsys="eRT."	
check_dmesg=""
check_dmesg_wps_gpio_dump=""
check_dmesg_deauth=""
time=0
WIFI_RESTART=0
HOSTAPD_RESTART=0
AP_UP_COUNTER=0
FASTDOWN_COUNTER=0
LOOP_COUNTER=0
HOSTAPD_RESTART_COUNTER=0
rc=0
NTP_CONF="/tmp/ntp.conf"
newline="
"
DEVICE_MODEL=`grep DEVICE_MODEL /etc/device.properties | cut -d"=" -f2`

if [ -e /rdklogger/log_capture_path_atom.sh ]
then
	source /rdklogger/log_capture_path_atom.sh 
else
	echo_t()
	{
        	echo $1
	}
fi

restart_wifi()
{
	cd /usr/ccsp/wifi
	export LD_LIBRARY_PATH=$PWD:.:$PWD/../../lib:$PWD/../../.:/lib:/usr/lib:$LD_LIBRARY_PATH
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
	cd -
}

while [ $loop -eq 1 ]
do
	uptime=`cat /proc/uptime | awk '{ print $1 }' | cut -d"." -f1`
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
                		echo_t "resetting radios"
				dmcli eRT setv Device.X_CISCO_COM_DeviceControl.RebootDevice string Wifi
                                continue
			fi
                fi
        fi
			
	cd /usr/ccsp/wifi
	WiFi_PID=`pidof CcspWifiSsp`
	if [ "$WiFi_PID" == "" ]; then
		echo_t "WiFi process is not running, restarting it"
		echo_t "RDKB_PROCESS_CRASHED : WiFiAgent_process is not running, need restart"
		restart_wifi
	else
	  radioenable=`dmcli eRT getv Device.WiFi.Radio.1.Enable`
	  radioenable_error=`echo $radioenable | grep "Execution fail"`
	  if [ "$radioenable_error" != "" ]; then
	    echo_t "wifi parameter timed out or failed"
		wifi_name_query=`dmcli eRT getv com.cisco.spvtg.ccsp.wifi.Name`
		wifi_name_error=`echo $wifi_name_query | grep "Execution fail"`
		if [ "$wifi_name_error" != "" ]; then
		  echo_t "WiFi process is not responding. Restarting it"
		  kill -9 $WiFi_PID
		  restart_wifi
		fi
	  else
        	WIFI_RESTART=0
                HOSTAPD_RESTART=0
		if [ "$DEVICE_MODEL" == "TCHXB3" ]; then
			check_wps=`cfg -e | grep WPS_ENABLE`                                              
			check_wps_1=`echo $check_wps | grep "WPS_ENABLE=1"`                                                            
			if [ "$check_wps_1" != "" ] ; then                                                                             
				echo_t "WPS config incorrect in ssid 1"
			fi                                                                                                             
			check_wps_2=`echo $check_wps | grep "WPS_ENABLE_2=1"`                                                          
			if [ "$check_wps_2" != "" ] ; then                                                                             
				echo_t "WPS config incorrect in ssid 2"
			fi                                                                                                             
			for (( c=3; c<=16; c++ ))                                                                                      
			do                                                                                                             
				check_wps_3=`echo $check_wps | grep "WPS_ENABLE_$c=0"`                                                 
				if [ "$check_wps_3" == "" ] ; then  
					echo_t "WPS config incorrect in ssid $c"
				fi
			done
		fi
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
		if [ "$check_radio" != "" ]; then
			radio5g=`lspci -mvk | grep 02:00.0`
			radio2g=`lspci -mvk | grep 03:00.0`
			if [ "$radio5g" == "" ] && [ "$radio2g" == "" ]; then
				echo_t "Both WiFi Radios missing"
			else
				if [ "$radio5g" == "" ] ;then
					echo_t "5G WiFi Radios missing"
				else
					if [ "$radio2g" == "" ];then
						echo_t "2G WiFi Radios missing"
					fi
				fi
			fi
		else
			radio2g=`lspci -mvk | grep 02:00.0`
			radio5g=`lspci -mvk | grep 03:00.0`
			if [ "$radio5g" == "" ] && [ "$radio2g" == "" ]; then
				echo_t "Both WiFi Radios missing"
			else 
				if [ "$radio5g" == "" ];then
				echo_t "5G WiFi Radios missing"
				else
					if [ "$radio2g" == "" ];then
						echo_t "2G WiFi Radios missing"
					fi
				fi
			fi
		fi
			
		if [ "$check_radio_enable5" == "1" ] || [ "$check_radio_enable2" == "1" ]; then
			if [ "$APUP_PID" == "" ] && [ "$FASTDOWN_PID" == "" ] && [ $FASTDOWN_COUNTER -eq 0 ]; then
                                AP_UP_COUNTER=0
				HOSTAPD_PID=`pidof hostapd`
				if [ "$HOSTAPD_PID" == "" ] && [ $HOSTAPD_RESTART_COUNTER -lt 4 ]; then
					WIFI_RESTART=1
                                        HOSTAPD_RESTART=1
					echo_t "RDKB_PROCESS_NOT_RUNNING : Hostapd_process is not running, will Reset Radios, this will restart hostapd"
			       
				else
					HOSTAPD_RESTART_COUNTER=0
					check_ap_tkip_cfg=$(cfg -s | grep 'TKIP\\')                     
                			if [ "$check_ap_tkip_cfg" != "" ]; then                         
                        			echo_t "TKIP has backslash"  
                                                continue      
                			fi  
					tmp_acl_in=`dmesg | grep "acl list"`
					echo $tmp_acl_in >/tmp/acl_add_file2
					sed -i 's/list/list\n/g' /tmp/acl_add_file2
					if [ "$tmp_acl_in" != "" ]; then
						while read -r LINE; do
						if [ "$LINE" != "" ]; then
							printtime=`echo $LINE | cut -d "[" -f2 | cut -d "]" -f1`
							match=`cat /tmp/acl_add_file1 | grep $printtime`
							if [ "$match" == "" ] && [ "$printtime" != "" ]; then
								echo $LINE >> /rdklogs/logs/authenticator_error_log.txt
								echo $LINE >> /tmp/acl_add_file1
							fi
						fi
						done < /tmp/acl_add_file2
					else
						echo >/tmp/acl_add_file1
					fi
                                        tmp_acl_in=`dmesg | grep "RDKB_WIFI_DRIVER_LOG"`
					echo $tmp_acl_in >/tmp/acl_add_file4
					sed -i 's/LOG_END/LOG_END\n/g' /tmp/acl_add_file4
					if [ "$tmp_acl_in" != "" ]; then
						while read -r LINE; do
						if [ "$LINE" != "" ]; then
  					                match=`echo "$LINE" |cut -f2 -d "]"`
							match2=`cat /tmp/acl_add_file3 | grep "$match"`
		
							if [ "$match2" == "" ]; then
								echo $LINE >> /rdklogs/logs/authenticator_error_log.txt
								echo $LINE >> /tmp/acl_add_file3
							fi
						fi
						done < /tmp/acl_add_file4
					else
						echo >/tmp/acl_add_file3
					fi
					check_ap_enable5=`cfg -e | grep AP_ENABLE_2=1 | cut -d"=" -f2`
					check_interface_up5=`ifconfig | grep ath1`
					check_ap_enable2=`cfg -e | grep AP_ENABLE=1 | cut -d"=" -f2`
					check_ap_enable_ath2=`cfg -e | grep AP_ENABLE_3=1 | cut -d"=" -f2`
					check_interface_up2=`ifconfig | grep ath0`
					check_interface_up2=`ifconfig | grep ath2`
					check_interface_iw2=`iwconfig ath0 | grep Access | awk '{print $6}'`
					check_interface_iw5=`iwconfig ath1 | grep Access | awk '{print $6}'`
					check_interface_iw_ath2=`iwconfig ath2 | grep Access | awk '{print $6}'`
					check_hostapd_ath0=`cat /proc/$HOSTAPD_PID/cmdline | grep ath0`
					check_hostapd_ath1=`cat /proc/$HOSTAPD_PID/cmdline | grep ath1`
					check_wps_ath0=`cfg -e | grep WPS_ENABLE=0`
					check_wps_ath1=`cfg -e | grep WPS_ENABLE_2=0`
					check_ap_sec_mode_2=`cfg -e | grep AP_SECMODE=WPA`
					check_ap_sec_mode_5=`cfg -e | grep AP_SECMODE_2=WPA`
					if [ "$check_radio_enable2" == "1" ] && [ "$check_ap_enable2" == "1" ] && [ "$check_ap_sec_mode_2" != "" ] && [ "$check_wps_ath0" == "" ] && [ "$check_hostapd_ath0" == "" ]; then
						echo_t "Hostapd incorrect config"
						#WIFI_RESTART=1 currently monitoring this
					fi
					if [ "$check_radio_enable5" == "1" ] && [ "$check_ap_enable5" == "1" ] && [ "$check_ap_sec_mode_5" != "" ] && [ "$check_wps_ath1" == "" ] && [ "$check_hostapd_ath1" == "" ]; then
						echo_t "Hostapd incorrect config"
						#WIFI_RESTART=1 currently monitoring this
					fi
					

					if [ "$check_ap_enable2" == "1" ] && [ "$check_radio_enable2" == "1" ] && [ "$check_interface_iw2" == "Not-Associated" ]; then
						echo_t "ath0 is Not-Associated, flapping ath0 interface"
						ifconfig ath0 down
						ifconfig ath0 up
                                                check_interface_iw2=`iwconfig ath0 | grep Access | awk '{print $6}'`
						if [ "$check_interface_iw2" == "Not-Associated" ] && [ "$(pidof apup)" == "" ] && [ "$(pidof fastdown)" == "" ] && [ "$(pidof apdown)" == "" ] && [ "$(pidof hostapd)" != "" ]; then
								echo_t "ath0 is Not-Associated, restarting radios"
								WIFI_RESTART=1
						fi
					fi

					if [ "$check_ap_enable5" == "1" ] && [ "$check_radio_enable5" == "1" ] && [ "$check_interface_iw5" == "Not-Associated" ]; then
						echo_t "ath1 is Not-Associated, flapping ath1 interface"
						ifconfig ath1 down
						ifconfig ath1 up
                                                check_interface_iw5=`iwconfig ath1 | grep Access | awk '{print $6}'`
						if [ "$check_interface_iw5" == "Not-Associated" ] && [ "$(pidof apup)" == "" ] && [ "$(pidof fastdown)" == "" ] && [ "$(pidof apdown)" == "" ] && [ "$(pidof hostapd)" != "" ]; then
								echo_t "ath1 is Not-Associated, restarting radios"
								WIFI_RESTART=1
						fi
					fi

					if [ "$check_ap_enable_ath2" == "1" ] && [ "$check_radio_enable2" == "1" ] && [ "$check_interface_iw_ath2" == "Not-Associated" ]; then
						echo_t "ath2 is Not-Associated, flapping ath2 interface"
						ifconfig ath2 down
						ifconfig ath2 up
                                                check_interface_iw_ath2=`iwconfig ath2 | grep Access | awk '{print $6}'`
						if [ "$check_interface_iw_ath2" == "Not-Associated" ] && [ "$(pidof apup)" == "" ] && [ "$(pidof fastdown)" == "" ] && [ "$(pidof apdown)" == "" ] && [ "$(pidof hostapd)" != "" ]; then
								echo_t "ath2 is Not-Associated, restarting radios"
								WIFI_RESTART=1
						fi
					fi


					if [ "$check_ap_enable5" == "1" ] && [ "$check_radio_enable5" == "1" ] && [ "$check_interface_up5" == "" ] && [ "$(pidof hostapd)" != "" ]; then
						check_interface_up5=`ifconfig | grep ath1`
						if [ "$check_interface_up5" == "" ] && [ "$(pidof apup)" == "" ] && [ "$(pidof fastdown)" == "" ] && [ "$(pidof apdown)" == "" ] && [ "$(pidof hostapd)" != "" ]; then
							echo_t "ath1 is down, restarting radios"
							WIFI_RESTART=1
						fi
					fi
				
					if [ "$check_ap_enable2" == "1" ] && [ "$check_radio_enable2" == "1" ] && [ "$check_interface_up2" == "" ] && [ "$(pidof hostapd)" != "" ]; then
						check_interface_up2=`ifconfig | grep ath0`
						if [ "$check_interface_up2" == "" ] && [ "$(pidof apup)" == "" ] && [ "$(pidof fastdown)" == "" ] && [ "$(pidof apdown)" == "" ] && [ "$(pidof hostapd)" != "" ]; then
							echo_t "ath0 is down, restarting radios"
							WIFI_RESTART=1
						fi
					fi
					time=$(($time + 300))
					if [ $time -eq 1800 ]; then
						time=0
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
                                                tmp_dmesg_dump=`dmesg | grep "wps_gpio Tainted"`
						if [ "$tmp" == "$check_dmesg" ]; then 
							check_dmesg=""
						else
							check_dmesg="$tmp"
						fi
						if [ "$check_dmesg" != "" ]; then
							echo_t "Resetting WiFi hardware for Rx stuck"
						fi
						check_dmesg="$tmp"

						if [ "$tmp_dmesg_dump" == "$check_dmesg_wps_gpio_dump" ]; then 
							check_dmesg_wps_gpio_dump=""
						else
							check_dmesg_wps_gpio_dump="$tmp_dmesg_dump"
						fi
						if [ "$check_dmesg_wps_gpio_dump" != "" ]; then
							echo_t "ATOM_WIFI_KERNEL_MODULE_DUMP : wps_gpio related kernel_dump seen"
						fi
                                                check_dmesg_wps_gpio_dump="$tmp_dmesg_dump"
						if [ "$check_lspci_2g_pcie" != "" ]; then
							echo_t "PCIE device_class of 2G radio has abcd in it"
						fi
						if [ "$check_target_asserted" != "" ]; then
							echo_t "TARGET_ASSERTED"
                                                      
						fi
						if [ "$check_radio_intf_up" != "" ]; then
							echo_t "RADIO_NOT_UP"
                                                       
						fi
						if [ "$check_target_asserted" != "" ] || [ "$check_radio_intf_up" != "" ]; then
							echo_t "skipping hostapd restart"
							continue
						fi
					fi
					
				fi
			else
				AP_UP_COUNTER=$(($AP_UP_COUNTER + 1))
				echo_t "APUP is running"
				if [ $AP_UP_COUNTER -eq 3 ]; then
					echo_t "APUP stuck"
					# extra precaution to force module removal in extreme case of apup stuck permanently, apdown does not force removal
					#module dependencies have been taken care of and being removed in correct order
					# wifi will restart cleanly
					# when actual root cause is fixed this block will not execute
                                        killall hostapd
                                        sleep 1
                                        WiFi_PID=`pidof CcspWifiSsp`
                                        kill -9 $WiFi_PID
                                        sleep 1
                                        killall lbd
                                        sleep 1
                                        killall radstat
					sleep 1
                                        rmmod -f ath_pktlog
                                        sleep 1
                                        rmmod -f umac
                                        sleep 1
                                        rmmod -f hst_tx99
                                         sleep 1
					rmmod -f ath_dfs
                                        sleep 1
                                        rmmod -f ath_dev  
                                        sleep 1   
					rmmod -f ath_spectral
                                        sleep 1
					rmmod -f ath_rate_atheros  
                                        sleep 1
					rmmod -f ath_hal
                                        sleep 1
					rmmod -f asf 
                                        sleep 1   
					rmmod -f adf
                                        sleep 1
                                        WiFi_PID=`pidof CcspWifiSsp`
                                        kill -9 $WiFi_PID
                                        #kill -9 $HOSTAPD_PID
                                        AP_UP_COUNTER=0
					kill -9 $APUP_PID
                                        restart_wifi
					#WIFI_RESTART=1
				fi
			fi
			if [ "$WIFI_RESTART" == "1" ]; then
				sleep 60
				check_ap_enable5=`cfg -e | grep AP_ENABLE_2=1 | cut -d"=" -f2`
				check_ap_enable2=`cfg -e | grep AP_ENABLE=1 | cut -d"=" -f2`
				check_radio=`cfg -e | grep AP_RADIO_ENABLED`
				if [ "$check_radio" != "" ]; then
					check_radio_enable5=`cfg -e | grep AP_RADIO_ENABLED=1 | cut -d"=" -f2`
					check_radio_enable2=`cfg -e | grep AP_RADIO_ENABLED_2=1 | cut -d"=" -f2`
				else
					check_radio_enable5=`cfg -e | grep RADIO_ENABLE_2=1 | cut -d"=" -f2`
					check_radio_enable2=`cfg -e | grep RADIO_ENABLE=1 | cut -d"=" -f2`
				fi
				is_at_least_one_radio_and_ssid_up=0
				if [ "$check_radio_enable2" == "1" ] && [ "$check_ap_enable2" == "1" ]; then
					is_at_least_one_radio_and_ssid_up=1
				else 
					if [ "$check_radio_enable5" == "1" ] && [ "$check_ap_enable5" == "1" ]; then
						is_at_least_one_radio_and_ssid_up=1
					fi
				fi
				echo $is_at_least_one_radio_and_ssid_up
                                LOOP_COUNTER=0
				while [ $LOOP_COUNTER -lt 3 ] ; do
					if [ "$is_at_least_one_radio_and_ssid_up" == "1" ] && [ $uptime -gt 1800 ] && [ "$(pidof CcspWifiSsp)" != "" ] && [ "$(pidof apup)" == "" ] && [ "$(pidof fastdown)" == "" ] && [ "$(pidof apdown)" == "" ]  && [ "$(pidof aphealth_log.sh)" == "" ]; then
						if [ "$(pidof hostapd)" != "" ] && [ "$HOSTAPD_RESTART" == "1" ]; then
                                                	break
						fi
						echo_t "resetting radios"
						dmcli eRT setv Device.X_CISCO_COM_DeviceControl.RebootDevice string Wifi
						HOSTAPD_RESTART_COUNTER=$(($HOSTAPD_RESTART_COUNTER + 1))
                                                sleep 60
                                                break
					else
						echo_t "eligibility check before resetting radios failed"
                                                LOOP_COUNTER=$(($LOOP_COUNTER + 1))
						sleep 60
					fi
				done
				if [ "$(pidof hostapd)" == "" ] && [ "$(pidof apup)" == "" ] && [ "$(pidof fastdown)" == "" ] && [ "$(pidof apdown)" == "" ]; then
                                	ifconfig ath0 up
					ifconfig ath1 up
					HOSTAPD_RESTART_COUNTER=$(($HOSTAPD_RESTART_COUNTER + 1))
					killall hostapd; rm /var/run/hostapd/*; sleep 2; hostapd `cat /tmp/conf_filename` -e /tmp/entropy -P /tmp/hostapd.pid 2>&1 &
				fi
			fi
		fi
	  fi
	fi

	if [ -f "/etc/PARODUS_ENABLE" ]; then
	    cd /usr/ccsp/webpa
	    Webpa_PID=`pidof webpa`
	    if [ "$Webpa_PID" == "" ]; then
		echo_t "WebPA_process is not running, restarting it "
		if [ -f /usr/bin/webpa ];then
		 /usr/bin/webpa &
		fi 
	    fi
	fi
        
        if [ -e $BINPATH/CcspMdcSsp ]; then
            cd /usr/ccsp/mdc
	    MDC_PID=`pidof CcspMdcSsp`
	    if [ "$MDC_PID" = "" ]; then
		echo "MDC process is not running, restarting it"
		export LD_LIBRARY_PATH=$PWD:.:$PWD/../../lib:$PWD/../../.:/lib:/usr/lib:$LD_LIBRARY_PATH
		$BINPATH/CcspMdcSsp -subsys $Subsys &
	    fi
        fi

	cd /usr/ccsp/harvester
        Harvester_PID=`pidof harvester`
        if [ "$Harvester_PID" == "" ]; then
                echo_t "Harvester process is not running, restarting it"
                $BINPATH/harvester &
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
   
       if [ -f $TELNET_SCRIPT_PATH/arping_to_host ]
       then
           $TELNET_SCRIPT_PATH/arping_to_host
       else
           echo_t "arping_to_host not found"
       fi
	
	#Checking the ntpd is running or not in ATOM
	NTPD_PID=`pidof ntpd`
	if [ "$NTPD_PID" = "" ] && [ $uptime -gt 900 ]; then
			echo "[`getDateTime`] RDKB_PROCESS_CRASHED : NTPD is not running in ATOM, restarting NTPD"
			ntpd -c $NTP_CONF -g
	fi

#Checking if rpcserver is running
	RPCSERVER_PID=`pidof rpcserver`
	if [ "$RPCSERVER_PID" = "" ] && [ -f /usr/bin/rpcserver ]; then
			echo "[`getDateTime`] RDKB_PROCESS_CRASHED : RPCSERVER is not running on ATOM, restarting "
			/usr/bin/rpcserver &
	fi

done

