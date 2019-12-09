#! /bin/sh
##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2018 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
#Prints mesh status every 12 hours
source /etc/log_timestamp.sh

enable=`syscfg get mesh_enable`
if [ "$enable" == "true" ]; then
 echo_t "Meshwifi has been enabled"  >> /rdklogs/logs/MeshAgentLog.txt.0
 Pods_12=`wifi_api wifi_getApAssociatedDeviceDiagnosticResult 12 | grep Total_STA  | cut -d":" -f2`
 #if Pods_12 is empty, assign 0 to it.
 if [ ! -n "$Pods_12" ]; then 
  Pods_12=0
 fi
 Pods_13=`wifi_api wifi_getApAssociatedDeviceDiagnosticResult 13 | grep Total_STA  | cut -d":" -f2`
 if [ ! -n "$Pods_13" ]; then
  Pods_13=0
 fi
 #echo_t "Pods connected count: 2.4GHz= $Pods_12, 5GHz= $Pods_13" >> /rdklogs/logs/MeshAgentLog.txt.0
 echo_t "CONNECTED_Pods:Total_WiFi-2.4G_Pods=$Pods_12" >> /rdklogs/logs/MeshAgentLog.txt.0
 echo_t "CONNECTED_Pods:Total_WiFi-5.0G_Pods=$Pods_13" >> /rdklogs/logs/MeshAgentLog.txt.0
else
 echo_t "Meshwifi has been disabled"  >> /rdklogs/logs/MeshAgentLog.txt.0
fi

#Print mac address of wifi and ethernet pod macs

#WiFi Pod
maclist="POD_BACKHAUL_MAC:"
linktype="POD_BACKHAUL_LINK:"
ports="POD_BACKHAUL_PORT:"
pod_found=false

for vap in 12 13; do
 if [ $Pods_$vap -gt 0 ]; then
  cliMac=`wifi_api wifi_getApAssociatedDeviceDiagnosticResult $vap | grep -i cli_MACAddress | cut -d'=' -f2`
  for mac in $cliMac; do 
   maclist="$maclist $mac,"
   linktype="$linktype WiFi,"
   ports="$ports $vap"
   pod_found=true
  done
 else
  echo "Nothing on $vap vap"
 fi
done

#Ethernet Pod
pod_mac=`/usr/plume/tools/ovsh s Node_Config | grep -i value | cut -d'|' -f2`
for i in $(echo $pod_mac | sed "s/,/ /g"); do
 podmac2=$(echo $i | sed 's/\(..\)/\1:/g;s/:$//' )
 echo "checking $podmac2"
 for port in 1 2 3 4; do
  check=`dmcli eRT getv Device.Ethernet.Interface.$port. | grep -i $podmac2`
  if [ "$check" != "" ]; then
   echo "Pod mac $podmac2 found in $port address"
   maclist="$maclist $podmac2,"
  linktype="$linktype Ethernet,"
  ports="$ports $port"
  pod_found=true
  fi
 done 
done

echo $maclist
if $pod_found; then
 echo_t "$maclist"  >> /rdklogs/logs/MeshAgentLog.txt.0 
 echo_t "$linktype" >> /rdklogs/logs/MeshAgentLog.txt.0 
 echo_t "$ports"    >> /rdklogs/logs/MeshAgentLog.txt.0
fi 
