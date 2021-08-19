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
AM_CFLAGS = -D_ANSC_LINUX
AM_CFLAGS += -D_ANSC_USER

AM_LDFLAGS = -L$(top_builddir)/source/sysevent/lib
AM_LDFLAGS += -lsysevent 


AM_CPPFLAGS = -Wall -Werror
ACLOCAL_AMFLAGS = -I m4
hardware_platform = i686-linux-gnu

bin_PROGRAMS = wifi_events_consumer
wifi_events_consumer_CPPFLAGS = $(CPPFLAGS)  -I$(top_srcdir)/./include -I${PKG_CONFIG_SYSROOT_DIR}$(includedir)/rbus -I$(top_srcdir)/../hal/include -I$(top_srcdir)/source/TR-181/sbapi  -I$(top_srcdir)/source/TR-181/lib/common
wifi_events_consumer_SOURCES = wifievents_consumer_sample.c
wifi_events_consumer_LDFLAGS = -lsysevent -lrbus
