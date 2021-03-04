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

if CCSP_ARCH_PC
OPT_SRCDIR = source-pc
else 
OPT_SRCDIR = source
endif
SUBDIRS = $(OPT_SRCDIR)

lib_LTLIBRARIES=libwifi.la

if CCSP_ARCH_PC
libwifi_la_CPPFLAGS = \
    -D_ANSC_LINUX \
    -I$(top_srcdir)/source-pc/TR-181/ml \
    -I$(top_srcdir)/source-pc/TR-181/sbapi \
    -I$(top_srcdir)/include/TR-181/ml \
    -I$(top_srcdir)/source-pc/Custom \
    -I$(top_srcdir)/include
else
libwifi_la_CPPFLAGS = \
    -D_ANSC_LINUX \
    -I$(top_srcdir)/source/TR-181/ml \
    -I$(top_srcdir)/source/TR-181/sbapi \
    -I$(top_srcdir)/source/TR-181/lib \
    -I$(top_srcdir)/source/TR-181/lib/common \
    -I$(top_srcdir)/include/TR-181/ml \
    -I$(top_srcdir)/source/Custom \
    -I$(top_srcdir)/include
endif

libwifi_la_LDFLAGS =
libwifi_la_SOURCES =
if CCSP_ARCH_PC
EXTRA_DEPENDENCIES = \
        source-pc/TR-181/sbapi/libCcspWifiAgent_sbapi.la \
        source-pc/TR-181/ml/libCcspWifiAgent_ml.la
else
EXTRA_DEPENDENCIES = \
        source/TR-181/sbapi/libCcspWifiAgent_sbapi.la \
        source/TR-181/ml/libCcspWifiAgent_ml.la \
        source/TR-181/lib/common/libCcspWifiAgent_common.la \
        source/TR-181/lib/const/libCcspWifiAgent_const.la \
        source/TR-181/lib/ds/libCcspWifiAgent_ds.la \
        source/TR-181/lib/json_util/libCcspWifiAgent_jsutil.la \
        source/TR-181/lib/log/libCcspWifiAgent_log.la \
        source/TR-181/lib/osa/libCcspWifiAgent_osa.la \
        source/TR-181/lib/ovsdb/libCcspWifiAgent_ovsdb.la \
        source/TR-181/lib/pjs/libCcspWifiAgent_pjs.la \
        source/TR-181/lib/schema/libCcspWifiAgent_schema.la 
endif

libwifi_la_DEPENDENCIES = \
	$(EXTRA_DEPENDENCIES)

libwifi_la_LIBADD = $(libwifi_la_DEPENDENCIES)
