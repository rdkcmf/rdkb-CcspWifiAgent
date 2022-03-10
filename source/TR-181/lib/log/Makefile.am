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

noinst_LTLIBRARIES = libCcspWifiAgent_log.la

libCcspWifiAgent_log_la_SOURCES = log.c log_syslog.c log_stdout.c log_traceback.c

libCcspWifiAgent_log_la_CPPFLAGS = -I$(top_srcdir)/source/TR-181/lib/common \
                                   -I$(top_srcdir)/source/TR-181/lib/const \
                                   -I$(top_srcdir)/source/TR-181/lib/ds \
                                   -I$(top_srcdir)/source/TR-181/lib/inc \
                                   -I$(top_srcdir)/source/TR-181/lib/json_util \
                                   -I$(top_srcdir)/source/TR-181/lib/osa \
                                   -I$(top_srcdir)/source/TR-181/lib/ovsdb \
                                   -I$(top_srcdir)/source/TR-181/lib/pjs \
                                   -I$(top_srcdir)/source/TR-181/lib/qm \
                                   -I$(top_srcdir)/source/TR-181/lib/schema

libCcspWifiAgent_log_la_LDFLAGS = -lev
