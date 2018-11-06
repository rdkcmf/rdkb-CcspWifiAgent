#! /bin/sh
#Prints mesh status every 12 hours
source /etc/log_timestamp.sh

enable=`syscfg get mesh_enable`
if [ "$enable" == "true" ]; then
 echo_t "Meshwifi has been enabled"  >> /rdklogs/logs/MeshAgentLog.txt.0
 Pods_2=`wifi_api wifi_getApAssociatedDeviceDiagnosticResult 12 | grep Total_STA  | cut -d":" -f2`
 if [ $Pods_2 == ""]; then
  Pods_2=0
 fi
 Pods_5=`wifi_api wifi_getApAssociatedDeviceDiagnosticResult 13 | grep Total_STA  | cut -d":" -f2`
 if [ $Pods_5 == ""]; then
  Pods_5=0
 fi
 #echo_t "Pods connected count: 2.4GHz= $Pods_2, 5GHz= $Pods_5" >> /rdklogs/logs/MeshAgentLog.txt.0
 echo_t "CONNECTED_Pods:Total_WiFi-2.4G_Pods=$Pods_2" >> /rdklogs/logs/MeshAgentLog.txt.0
 echo_t "CONNECTED_Pods:Total_WiFi-5.0G_Pods=$Pods_5" >> /rdklogs/logs/MeshAgentLog.txt.0
else
 echo_t "Meshwifi has been disabled"  >> /rdklogs/logs/MeshAgentLog.txt.0
fi
