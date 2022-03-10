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
AM_CFLAGS =

ACLOCAL_AMFLAGS = -I m4

noinst_LTLIBRARIES = libCcspWifiAgent_ml.la

libCcspWifiAgent_ml_la_CPPFLAGS = \
    -I$(top_srcdir)/source/TR-181/ml \
    -I$(top_srcdir)/source/TR-181/sbapi \
    -I$(top_srcdir)/include/TR-181/ml \
    -I$(top_srcdir)/source/TR-181/lib/common \
    -I$(top_srcdir)/source/WifiSsp \
    -I$(top_srcdir)/source/Custom

libCcspWifiAgent_ml_la_SOURCES = plugin_main.c cosa_apis_util.c plugin_main_apis.c cosa_wifi_dml.c cosa_apis_busutil.c cosa_wifi_internal.c cosa_logging_internal.c cosa_logging_dml.c cosa_harvester_dml.c
libCcspWifiAgent_ml_la_LDFLAGS = -lccsp_common -lhal_wifi -lsecure_wrapper
