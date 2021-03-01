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
if CCSP_ARCH_ARM
AM_CFLAGS += -D_ANSC_LITTLE_ENDIAN_
endif

if CCSP_ARCH_ATOM
AM_CFLAGS += -D_ANSC_LITTLE_ENDIAN_
endif

if CCSP_ARCH_PC
AM_CFLAGS += -D_ANSC_LITTLE_ENDIAN_
endif
AM_CFLAGS += -DWBCFG_MULTI_COMP_SUPPORT
AM_LDFLAGS = -L$(top_builddir)/../CcspCommonLibrary/lib
AM_LDFLAGS += -lccsp_common
AM_LDFLAGS += -L$(top_builddir)/../hal/lib
AM_LDFLAGS += -lhal_wifi -lsecure_wrapper
AM_LDFLAGS += -L$(top_builddir)/source/sysevent/lib
AM_LDFLAGS += -lsysevent
AM_LDFLAGS += -lwebconfig_framework
AM_LDFLAGS += -lev

AM_CPPFLAGS = -Wall -Werror
ACLOCAL_AMFLAGS = -I m4
hardware_platform = i686-linux-gnu

noinst_LTLIBRARIES = libCcspWifiAgent_sbapi.la
libCcspWifiAgent_sbapi_la_CPPFLAGS = -I$(top_srcdir)/../CcspCommonLibrary/source/ccsp/custom -I$(top_srcdir)/../CcspCommonLibrary/source/ccsp/include -I$(top_srcdir)/../CcspCommonLibrary/source/debug_api/include -I$(top_srcdir)/../CcspCommonLibrary/source/cosa/include -I$(top_srcdir)/../CcspCommonLibrary/source/cosa/include/linux -I$(top_srcdir)/../CcspCommonLibrary/source/ccsp/components/include -I$(top_srcdir)/../CcspCommonLibrary/source/cosa/package/slap/include -I$(top_srcdir)/../hal/include -I$(top_srcdir)/../CcspCommonLibrary/source/util_api/http/include -I$(top_srcdir)/../CcspCommonLibrary/source/util_api/ansc/include -I$(top_srcdir)/source/WifiSsp -I$(top_srcdir)/source/TR-181/ml -I$(top_srcdir)/source/TR-181/sbapi -I$(top_srcdir)/include/TR-181/ml -I$(top_srcdir)/../CcspCommonLibrary/source/ccsp/components/common/MessageBusHelper/include -I$(top_srcdir)/../CcspCommonLibrary/source/ccsp/components/common/PoamIrepFolder -I$(top_srcdir)/source/Custom -I$(top_srcdir)/./include -I$(top_srcdir)/../Utopia/source/include/sysevent -I$(top_srcdir)/source/TR-181/lib/ovsdb -I$(top_srcdir)/source/TR-181/lib/inc -I$(top_srcdir)/source/TR-181/lib/json_util -I$(top_srcdir)/source/TR-181/lib/ds -I$(top_srcdir)/source/TR-181/lib/common -I$(top_srcdir)/source/TR-181/lib/pjs -I$(top_srcdir)/source/TR-181/lib/log -I$(top_srcdir)/source/TR-181/lib/const -I$(top_srcdir)/source/TR-181/lib/schema -I$(top_srcdir)/source/TR-181/lib/osp -I$(top_srcdir)/source/TR-181/lib/osa -I$(top_srcdir)/source/TR-181/lib/psfs -I$(top_srcdir)/source/TR-181/lib/qm $(CPPFLAGS)
libCcspWifiAgent_sbapi_la_SOURCES = cosa_dbus_api.c cosa_wifi_apis.c pack_file.c cosa_logging_apis.c wifi_monitor.c collection.c wifi_easy_connect.c wifi_associated_devices_msmt.c wifi_single_client_msmt.c webpa_interface.c webpa_interface_without_seshat.c webpa_interface_with_seshat.c wifi_webconfig.c cosa_wifi_passpoint.c wifi_data_plane.c wifi_8021x.c wifi_validator.c jsonconv.c wifi_ovsdb.c
libCcspWifiAgent_sbapi_la_LDFLAGS = -lccsp_common -lhal_wifi -lsysevent -lavro -lmsgpackc -ltrower-base64 -luuid -llibparodus -lutapi -lutctx -lsecure_wrapper -lwebconfig_framework -lprint_uptime -lev

if FEATURE_HOSTAP_AUTHENTICATOR
AM_CFLAGS += -DCONFIG_WPS -DCONFIG_IEEE80211R_AP -DCONFIG_IEEE80211R -DCONFIG_ETH_P_OUI -DCONFIG_IEEE80211W
AM_CFLAGS += -DEAP_SERVER_FAST

libCcspWifiAgent_sbapi_la_CPPFLAGS += -I${PKG_CONFIG_SYSROOT_DIR}${includedir}/rdk-wifi-libhostap/src/ -I${PKG_CONFIG_SYSROOT_DIR}${includedir}/rdk-wifi-libhostap/src/ap -I${PKG_CONFIG_SYSROOT_DIR}${includedir}/rdk-wifi-libhostap/src/utils -I$(PKG_CONFIG_SYSROOT_DIR)$(includedir)/qca/qca-wifi/os/linux/include/ -DEXTERNAL_USE_ONLY

libCcspWifiAgent_sbapi_la_SOURCES += wifi_hostap_drv_ops.c wifi_hostapd_glue.c
libCcspWifiAgent_sbapi_la_LDFLAGS += -lhostap
endif
