#!/bin/sh
######################################################################################
# If not stated otherwise in this file or this component's LICENSE file the
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

#This script is used to set the L&F pass phrase into the wifi config

GetServiceUrl 5 /tmp/tmp5
configparammod /tmp/tmp5

lpf=`cat /tmp/tmp5.mod`;

rm -f /tmp/tmp5
rm -f /tmp/tmp5.mod

/usr/bin/wifi_api wifi_setApSecurityKeyPassphrase 6 $lpf
/usr/bin/wifi_api wifi_setApSecurityKeyPassphrase 7 $lpf
