##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2015 RDK Management
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
#######################################################################
#   Copyright [2014] [Cisco Systems, Inc.]
# 
#   Licensed under the Apache License, Version 2.0 (the \"License\");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
# 
#       http://www.apache.org/licenses/LICENSE-2.0
# 
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an \"AS IS\" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#######################################################################

BINPATH="/usr/bin"

killall CcspWifiSsp

# have IP address for dbus config generated
vconfig add eth0 500
ifconfig eth0.500 192.168.101.3

source /etc/utopia/service.d/log_capture_path.sh
export LD_LIBRARY_PATH=$PWD:.:$PWD/../../lib:$PWD/../../.:/lib:/usr/lib:$LD_LIBRARY_PATH
export LOG4C_RCPATH=/etc


# enable core files on atom
ulimit -c unlimited
echo "/tmp/core.%e.%p" > /proc/sys/kernel/core_pattern

cp -f ccsp_msg.cfg /tmp

if [ "x"$1 = "x" ];then
    sleep 10
fi

Subsys="eRT."

echo "Elected subsystem is $Subsys"

sleep 1



if [ -e ./wifi ]; then
	cd wifi 
	if [ "x"$Subsys = "x" ];then
        echo "****STARTING CCSPWIFI WITHOUT SUBSYS***"
    	$BINPATH/CcspWifiSsp &
	else
        echo "****STARTING CCSPWIFI WITH SUBSYS***"
    	echo "$BINPATH/CcspWifiSsp -subsys $Subsys &"
    	$BINPATH/CcspWifiSsp -subsys $Subsys &
	fi
fi

echo "starting process monitor script"
sh /usr/ccsp/wifi/process_monitor_atom.sh &

