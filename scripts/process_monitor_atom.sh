#! /bin/sh
######################################################################################
# If not stated otherwise in this file or this component's Licenses.txt file the
# following copyright and licenses apply:

#  Copyright 2018 RDK Management

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#######################################################################################
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
check_dmesg_NF_TIMEOUT=""
check_dmesg_RATE=""
time=0
prev_apstats_iw2_tx_pkts=0
prev_apstats_iw2_rx_pkts=0
prev_apstats_iw5_tx_pkts=0
prev_apstats_iw5_rx_pkts=0
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
MODEL_NUM=`grep MODEL_NUM /etc/device.properties | cut -d "=" -f2`
MESH_ENABLE=`syscfg get mesh_enable`
export LD_LIBRARY_PATH=/usr/ccsp/wifi:/usr/ccsp/mdc:.:/usr:/usr/ccsp:/lib:/usr/lib:$LD_LIBRARY_PATH

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
    check_apstats_iw2_p_req=`apstats -v -i ath0 | grep "Rx Probe request" | awk '{print $5}'`
    check_apstats_iw2_p_res=`apstats -v -i ath0 | grep "Tx Probe response" | awk '{print $5}'`
    check_apstats_iw2_au_req=`apstats -v -i ath0 | grep "Rx auth request" | awk '{print $5}'`
   	check_apstats_iw2_au_resp=`apstats -v -i ath0 | grep "Tx auth response" | awk '{print $5}'`
   	check_apstats_iw2_tx_pkts=`apstats -v -i ath0 | grep "Tx Data Packets" | awk '{print $5}'`
    check_apstats_iw2_tx_bytes=`apstats -v -i ath0 | grep "Tx Data Bytes" | awk '{print $5}'`
    check_apstats_iw2_rx_pkts=`apstats -v -i ath0 | grep "Rx Data Packets" | awk '{print $5}'`
    check_apstats_iw2_rx_bytes=`apstats -v -i ath0 | grep "Rx Data Bytes" | awk '{print $5}'`

   check_apstats_iw5_p_req=`apstats -v -i ath1 | grep "Rx Probe request" | awk '{print $5}'`
   check_apstats_iw5_p_res=`apstats -v -i ath1 | grep "Tx Probe response" | awk '{print $5}'`
   check_apstats_iw5_au_req=`apstats -v -i ath1 | grep "Rx auth request" | awk '{print $5}'`
   check_apstats_iw5_au_resp=`apstats -v -i ath1 | grep "Tx auth response" | awk '{print $5}'`
   check_apstats_iw5_tx_pkts=`apstats -v -i ath1 | grep "Tx Data Packets" | awk '{print $5}'`
   check_apstats_iw5_tx_bytes=`apstats -v -i ath1 | grep "Tx Data Bytes" | awk '{print $5}'`
   check_apstats_iw5_rx_pkts=`apstats -v -i ath1 | grep "Rx Data Packets" | awk '{print $5}'`
   check_apstats_iw5_rx_bytes=`apstats -v -i ath1 | grep "Rx Data Bytes" | awk '{print $5}'`

                        echo_t "2G_AuthRequest:$check_apstats_iw2_au_req" >> /rdklogs/logs/wifihealth.txt
                        echo_t "2G_AuthResponse:$check_apstats_iw2_au_resp" >> /rdklogs/logs/wifihealth.txt
                        echo_t "2G_TxPackets:$check_apstats_iw2_tx_pkts" >> /rdklogs/logs/wifihealth.txt
                        echo_t "2G_TxBytes:$check_apstats_iw2_tx_bytes" >> /rdklogs/logs/wifihealth.txt
                        echo_t "2G_Tx_Packet_Delta:`expr $check_apstats_iw2_tx_pkts - $prev_apstats_iw2_tx_pkts`" >> /rdklogs/logs/wifihealth.txt
                        prev_apstats_iw2_tx_pkts=$check_apstats_iw2_tx_pkts
                        echo_t "2G_RxPackets:$check_apstats_iw2_rx_pkts" >> /rdklogs/logs/wifihealth.txt
                        echo_t "2G_RxBytes:$check_apstats_iw2_rx_bytes" >> /rdklogs/logs/wifihealth.txt
                        echo_t "2G_Rx_Packet_Delta:`expr $check_apstats_iw2_rx_pkts - $prev_apstats_iw2_rx_pkts`" >> /rdklogs/logs/wifihealth.txt
                        prev_apstats_iw2_rx_pkts=$check_apstats_iw2_rx_pkts

                        echo_t "5G_ProbeRequest:$check_apstats_iw5_p_req" >> /rdklogs/logs/wifihealth.txt
                        echo_t "5G_ProbeResponse:$check_apstats_iw5_p_res" >> /rdklogs/logs/wifihealth.txt
                        echo_t "5G_AuthRequest:$check_apstats_iw5_au_req" >> /rdklogs/logs/wifihealth.txt
                        echo_t "5G_AuthResponse:$check_apstats_iw5_au_resp" >> /rdklogs/logs/wifihealth.txt
                        echo_t "5G_TxPackets:$check_apstats_iw5_tx_pkts" >> /rdklogs/logs/wifihealth.txt
                        echo_t "5G_TxBytes:$check_apstats_iw5_tx_bytes" >> /rdklogs/logs/wifihealth.txt
                        echo_t "5G_Tx_Packet_Delta:`expr $check_apstats_iw5_tx_pkts - $prev_apstats_iw5_tx_pkts`" >> /rdklogs/logs/wifihealth.txt
                        prev_apstats_iw5_tx_pkts=$check_apstats_iw5_tx_pkts
                        echo_t "5G_RxPackets:$check_apstats_iw5_rx_pkts" >> /rdklogs/logs/wifihealth.txt
                        echo_t "5G_RxBytes:$check_apstats_iw5_rx_bytes" >> /rdklogs/logs/wifihealth.txt
                        echo_t "5G_Rx_Packet_Delta:`expr $check_apstats_iw5_rx_pkts - $prev_apstats_iw5_rx_pkts`" >> /rdklogs/logs/wifihealth.txt
                        prev_apstats_iw5_rx_pkts=$check_apstats_iw5_rx_pkts
                        iwpriv ath0 get_dl_fa_stats
                        iwpriv ath1 get_dl_fa_stats
                        vap_activity=`dmesg | grep VAP_ACTIVITY_ath0 | tail -1`
                        if [ "$vap_activity" != "" ] ; then
                            echo_t "WIFI_PS_CLIENTS_1:`echo $vap_activity | cut -d"," -f2 `"  >> /rdklogs/logs/wifihealth.txt
                            echo_t "WIFI_PS_CLIENTS_DATA_QUEUE_LEN_1:`echo $vap_activity | cut -d"," -f3`"  >> /rdklogs/logs/wifihealth.txt
                            echo_t "WIFI_PS_CLIENTS_DATA_QUEUE_BYTES_1:`echo $vap_activity | cut -d"," -f4`"  >> /rdklogs/logs/wifihealth.txt
                            echo_t "WIFI_PS_CLIENTS_DATA_FRAME_LEN_1:`echo $vap_activity | cut -d"," -f5`"  >> /rdklogs/logs/wifihealth.txt
                            echo_t "WIFI_PS_CLIENTS_DATA_FRAME_COUNT_1:`echo $vap_activity | cut -d"," -f6`"  >> /rdklogs/logs/wifihealth.txt
                        fi

                        vap_activity=`dmesg | grep VAP_ACTIVITY_ath1 | tail -1`
                        if [ "$vap_activity" != "" ] ; then
                            echo_t "WIFI_PS_CLIENTS_2:`echo $vap_activity | cut -d"," -f2 `"  >> /rdklogs/logs/wifihealth.txt
                            echo_t "WIFI_PS_CLIENTS_DATA_QUEUE_LEN_2:`echo $vap_activity | cut -d"," -f3`"  >> /rdklogs/logs/wifihealth.txt
                            echo_t "WIFI_PS_CLIENTS_DATA_QUEUE_BYTES_2:`echo $vap_activity | cut -d"," -f4`"  >> /rdklogs/logs/wifihealth.txt
                            echo_t "WIFI_PS_CLIENTS_DATA_FRAME_LEN_2:`echo $vap_activity | cut -d"," -f5`"  >> /rdklogs/logs/wifihealth.txt
                            echo_t "WIFI_PS_CLIENTS_DATA_FRAME_COUNT_2:`echo $vap_activity | cut -d"," -f6`"  >> /rdklogs/logs/wifihealth.txt
                        fi
                        tmp=`dmesg | grep "NF Timeout AR_PHY_AGC_CONTROL"`
                        if [ "$tmp" == "$check_dmesg_NF_TIMEOUT" ]; then
                            check_dmesg_NF_TIMEOUT=""
                        else
                            check_dmesg_NF_TIMEOUT="$tmp"
                        fi
                        if [ "$check_dmesg_NF_TIMEOUT" != "" ]; then
                            echo_t "NF Calibration issue occured"
                        fi
                        check_dmesg_NF_TIMEOUT="$tmp"
                        tmp=`dmesg | grep "WAL_DBGID_TX_AC_BUFFER_SET ( 0xdead"`
                        if [ "$tmp" == "$check_dmesg_RATE" ]; then
                            check_dmesg_RATE=""
                        else
                            check_dmesg_RATE="$tmp"
                        fi
                        if [ "$check_dmesg_RATE" != "" ]; then
                            echo_t "Rate Adaptation failure seen"
                        fi
                        check_dmesg_RATE="$tmp"
                        tmp=`dmesg | grep "having same AID"`
                        if [ "$tmp" == "$check_dmesg_duplicate_aid" ]; then 
                            check_dmesg_duplicate_aid=""
                        else
                            check_dmesg_duplicate_aid="$tmp"
                        fi
                        if [ "$check_dmesg_duplicate_aid" != "" ]; then
                            echo_t "Duplicate AID issue seen"
                        fi
                        check_dmesg_duplicate_aid="$tmp"
                        wlanconfig ath0 list sta | grep -v "AID" > /tmp/aid
                        awk 'x[$2]++ == 1 { print  "WLAN_CONFIG_AID_2G is duplicated"}' /tmp/aid >> /rdklogs/logs/AtomConsolelog.txt.0
                        wlanconfig ath1 list sta | grep -v "AID" > /tmp/aid
                        awk 'x[$2]++ == 1 { print  "WLAN_CONFIG_AID_5G is duplicated"}' /tmp/aid >> /rdklogs/logs/AtomConsolelog.txt.0

                        

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

#MESH-492 Checking if Mesh bridges br12/br13 has a valid IP for XB3 devices
	if [ "$MESH_ENABLE" == "true" ]; then
 	 if [ "$MODEL_NUM" == "DPC3941" ] || [ "$MODEL_NUM" == "TG1682G" ] || [ "$MODEL_NUM" == "DPC3939" ] || [ "$MODEL_NUM" == "TG1682" ]; then
        	MESHBR24_IP=`ifconfig br12 | grep 'inet addr' | cut -d: -f2 | awk '{print $1}'`
                MESHBR50_IP=`ifconfig br13 | grep 'inet addr' | cut -d: -f2 | awk '{print $1}'`
                if [ "$MESHBR24_IP" != "169.254.0.1" ] || [ "$MESHBR50_IP" != "169.254.1.1" ]; then
			echo_t "[RDKB_PLATFORM_ERROR] : Mesh Bridges lost IP addresses, Setting the mesh IPs now"
                	sh /usr/ccsp/wifi/mesh_setip.sh
                else
                	echo_t "RDKB_SELFHEAL : Mesh Bridges have valid IPs"
		fi
	 fi
        fi
#Checking if rpcserver is running
	RPCSERVER_PID=`pidof rpcserver`
	if [ "$RPCSERVER_PID" = "" ] && [ -f /usr/bin/rpcserver ]; then
			echo "[`getDateTime`] RDKB_PROCESS_CRASHED : RPCSERVER is not running on ATOM, restarting "
			/usr/bin/rpcserver &
	fi
interface=1
#Checking if ping to ARM is not failing
	while [ $interface -eq 1 ]
	do
	        if [ "$BOX_TYPE" = "XB3" ]; then
        	        PING_RES=`ping -I $PEER_PING_INTF -c 2 -w 10 $ARM_INTERFACE_IP`
                	CHECK_PING_RES=`echo $PING_RES | grep "packet loss" | cut -d"," -f3 | cut -d"%" -f1`
			if [ "$CHECK_PING_RES" = "" ]
			then
				check_if_eth0_500_up=`ifconfig eth0.500 | grep UP `
				check_if_eth0_500_ip=`ifconfig eth0.500 | grep inet `
				if [ "$check_if_eth0_500_up" = "" ] || [ "$check_if_eth0_500_ip" = "" ]
                        	then
                                	echo_t "[RDKB_PLATFORM_ERROR] : eth0.500 is not up, setting to recreate interface"
                                	rpc_ifconfig eth0.500 >/dev/null 2>&1
                        	fi
                        	PING_RES=`ping -I $PEER_PING_INTF -c 2 -w 10 $ARM_INTERFACE_IP`
                        	CHECK_PING_RES=`echo $PING_RES | grep "packet loss" | cut -d"," -f3 | cut -d"%" -f1`
                        	if [ "$CHECK_PING_RES" != "" ]
                        	then
                                	if [ "$CHECK_PING_RES" -ne 100 ]
                                	then
                                        	echo_t "[RDKB_PLATFORM_ERROR] : eth0.500 is up,Ping to Peer IP is success"
                                        	break
                                	fi
                               		echo_t "[RDKB_PLATFORM_ERROR] : ARM ping Interface is down"
				fi
			else
                        	if [ "$CHECK_PING_RES" -ne 100 ]
                        	then
                                	echo_t "RDKB_SELFHEAL : Ping to Peer IP from ATOM is success"
                                	break
                        	fi
                        	echo_t "[RDKB_PLATFORM_ERROR] : ARM ping Interface is down"
			fi
		fi
		interface=`expr $interface + 1`
	done
done

