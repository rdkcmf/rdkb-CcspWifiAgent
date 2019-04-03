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
 Pods_2=`wifi_api wifi_getApAssociatedDeviceDiagnosticResult 12 | grep Total_STA  | cut -d":" -f2`
 #if Pods_2 is empty, assign 0 to it.
 if [ ! -n "$Pods_2" ]; then 
  Pods_2=0
 fi
 Pods_5=`wifi_api wifi_getApAssociatedDeviceDiagnosticResult 13 | grep Total_STA  | cut -d":" -f2`
 if [ ! -n "$Pods_5" ]; then
  Pods_5=0
 fi
 #echo_t "Pods connected count: 2.4GHz= $Pods_2, 5GHz= $Pods_5" >> /rdklogs/logs/MeshAgentLog.txt.0
 echo_t "CONNECTED_Pods:Total_WiFi-2.4G_Pods=$Pods_2" >> /rdklogs/logs/MeshAgentLog.txt.0
 echo_t "CONNECTED_Pods:Total_WiFi-5.0G_Pods=$Pods_5" >> /rdklogs/logs/MeshAgentLog.txt.0
else
 echo_t "Meshwifi has been disabled"  >> /rdklogs/logs/MeshAgentLog.txt.0
fi
