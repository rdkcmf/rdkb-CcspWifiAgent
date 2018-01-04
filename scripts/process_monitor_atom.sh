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
COUNTRYCODE_RESET_COUNTER_2=0
COUNTRYCODE_RESET_COUNTER_5=0
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
		#self heal for countrycode 
                if [ "$DEVICE_MODEL" == "TCHXB3" ]; then
                        gettxpower=` iwlist ath0 txpower |grep "Current Tx-Power=" | cut -d '=' -f2 | cut -f 1 -d " "`
                        getcountrycode=`iwpriv ath0 get_countrycode | cut -d ':' -f2`
                        getcountrycodeval=`echo $getcountrycode | grep 841`
                        if [ "$getcountrycodeval" == "" ];then
				COUNTRYCODE_RESET_COUNTER_2=$(($COUNTRYCODE_RESET_COUNTER_2 + 1))
				if [ $COUNTRYCODE_RESET_COUNTER_2 -ge 2 ];then
					COUNTRYCODE_RESET_COUNTER_2=0
					CHANNEL=`cfg -s | grep AP_PRIMARY_CH:= | cut -d'=' -f2-`
					CHMODE=`cfg -s | grep AP_CHMODE:= | cut -d'=' -f2-`
					iwpriv wifi0 setCountryID 841
					iwpriv ath0 mode $CHMODE
					iwconfig ath0 channel $CHANNEL
					iwconfig ath0 txpower 30
                        		echo_t "WIFI_COUNTRY_CODE_$AP:$getcountrycode will reset country code"
				fi
			fi
			#gettxpower=$gettxpower+0	
			if [ $gettxpower -le 5 ];then		
                        	echo_t "WIFI_TX_PWR_dBm_$AP:$gettxpower low tx power detected"
			fi
	
		fi

			
		if [ -e "/lib/rdk/platform_process_monitor.sh" ] && [ -e "/lib/rdk/platform_ap_monitor.sh" ];then
			sh /lib/rdk/platform_process_monitor.sh 
			sh /lib/rdk/platform_ap_monitor.sh 
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
        cosa_start_PID=`pidof cosa_start.sh`
        if [ "$Harvester_PID" == "" ] && [ "$cosa_start_PID" == "" ]; then
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
          	isPortKilled=`netstat -anp | awk '/:21515 */ {split($NF,a,"/"); print a[2],a[1]}' | cut -f2 -d" "`
		if [ "$isPortKilled" != "" ]
		    then
			 Process_PID=`netstat -anp | awk '/:21515 */ {split($NF,a,"/"); print a[2],a[1]}'`
		         echo "[`getDateTime`] Port 21515 is still alive. Killing processes $Process_PID"
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

