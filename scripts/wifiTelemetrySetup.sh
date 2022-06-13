##########################################################################
# If not stated otherwise in this file or this component's LICENSE
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

if [ -f /etc/device.properties ]
then
    source /etc/device.properties
fi

if [ "$IS_BCI" != "yes" ];
then
	CRONFILE=$CRON_SPOOL"/root"
	CRONFILE_BK="/tmp/cron_tab$$.txt"
	ENTRY_ADDED=0

	echo "Start monitoring wifi system statistics"
	if [ -f $CRONFILE ]
	then
		# Dump existing cron jobs to a file & add new job
		crontab -l -c $CRON_SPOOL > $CRONFILE_BK

		# Check whether specific cron jobs are existing or not
		existing_aphealth=$(grep "aphealth_log.sh" $CRONFILE_BK)
		existing_dailystats=$(grep "dailystats_log.sh" $CRONFILE_BK)
		existing_radiostats=$(grep "radiohealth.sh" $CRONFILE_BK)

		if [ -z "$existing_aphealth" ]; then
			echo "35 * * * *  /usr/ccsp/wifi/aphealth_log.sh" >> $CRONFILE_BK
			ENTRY_ADDED=1
		fi

		if [ -z "$existing_dailystats" ]; then
			echo "47 2 * * *  /usr/ccsp/wifi/dailystats_log.sh" >> $CRONFILE_BK
			ENTRY_ADDED=1
		fi
        
		if [ -z "$existing_radiostats" ]; then
			echo "*/15 * * * * sh /usr/ccsp/wifi/radiohealth.sh" >> $CRONFILE_BK
			ENTRY_ADDED=1
		fi

		if [ $ENTRY_ADDED -eq 1 ]; then
			crontab $CRONFILE_BK -c $CRON_SPOOL
		fi
	rm -rf $CRONFILE_BK
	fi
else
	CRONPATH="/var/spool/cron/crontabs/"
	CRONFILE=$CRONPATH"root"
	CRONFILE_BK="/tmp/cron_tab.txt"

	echo "Start monitoring system statistics"
	CRON_PID=`pidof crond`
	if [ "$CRON_PID" = "" ]
	then
    		if [ -f $CRONFILE ]
    		then
        		grep -nr "/usr/ccsp/wifi/radiohealth.sh" $CRONFILE
        		if [ $? -ne 0 ]; then
            			echo "*/15 * * * *  /usr/ccsp/wifi/radiohealth.sh" >> $CRONFILE
        		fi
        		grep -nr "/usr/ccsp/wifi/aphealth_log.sh" $CRONFILE
        		if [ $? -ne 0 ]; then
            			echo "1 * * * *  /usr/ccsp/wifi/aphealth_log.sh" >> $CRONFILE
         		fi
    		else
        		if [ ! -d $CRONPATH ]
        		then
            			mkdir $CRONPATH
        		fi
				touch $CRONFILE
        		echo "*/15 * * * *  /usr/ccsp/wifi/radiohealth.sh" >> $CRONFILE
        		echo "1 * * * *  /usr/ccsp/wifi/aphealth_log.sh" >> $CRONFILE
    		fi
    		crond -c $CRONPATH -l 9
	else
    		crontab -l -c $CRONPATH > $CRONFILE_BK
    		grep -nr "/usr/ccsp/wifi/radiohealth.sh" $CRONFILE_BK
    		if [ $? -ne 0 ]; then
        		echo "*/15 * * * *  /usr/ccsp/wifi/radiohealth.sh" >> $CRONFILE_BK
    		fi
    			grep -nr "/usr/ccsp/wifi/aphealth_log.sh" $CRONFILE_BK
    		if [ $? -ne 0 ]; then
        		echo "1 * * * *  /usr/ccsp/wifi/aphealth_log.sh" >> $CRONFILE_BK
    		fi
    		crontab $CRONFILE_BK -c $CRONPATH
    		rm -rf $CRONFILE_BK
	fi
fi
