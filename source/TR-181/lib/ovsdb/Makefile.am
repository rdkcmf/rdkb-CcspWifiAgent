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
AM_CFLAGS = -DJUDYL -DJUDYNEXT -DREAL_TIME_DEBUG -DRDK_EM_CTRL -DCONFIG_INSTALL_PREFIX

noinst_LTLIBRARIES = libCcspWifiAgent_ovsdb.la

libCcspWifiAgent_ovsdb_la_SOURCES = ovsdb.c ovsdb_method.c ovsdb_update.c ovsdb_sync.c ovsdb_sync_api.c ovsdb_table.c ovsdb_cache.c ovsdb_utils.c

libCcspWifiAgent_ovsdb_la_CPPFLAGS = -I$(top_srcdir)/source/TR-181/lib/common \
                                     -I$(top_srcdir)/source/TR-181/lib/const \
                                     -I$(top_srcdir)/source/TR-181/lib/ds \
                                     -I$(top_srcdir)/source/TR-181/lib/inc \
                                     -I$(top_srcdir)/source/TR-181/lib/json_util \
                                     -I$(top_srcdir)/source/TR-181/lib/log \
                                     -I$(top_srcdir)/source/TR-181/lib/osa \
                                     -I$(top_srcdir)/source/TR-181/lib/pjs \
                                     -I$(top_srcdir)/source/TR-181/lib/qm \
                                     -I$(top_srcdir)/source/TR-181/lib/schema
