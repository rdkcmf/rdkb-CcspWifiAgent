AM_CFLAGS = -D_ANSC_LINUX
AM_CFLAGS += -D_ANSC_USER
AM_CFLAGS += -D_ANSC_LITTLE_ENDIAN_
AM_LDFLAGS = -L$(top_builddir)/../CcspCommonLibrary/lib
AM_LDFLAGS += -lccsp_common
AM_LDFLAGS += -L$(top_builddir)/../hal/lib
AM_LDFLAGS += -lhal_wifi

AM_CPPFLAGS = -Wall -Werror
ACLOCAL_AMFLAGS = -I m4
hardware_platform = i686-linux-gnu

noinst_LTLIBRARIES = libCcspWifiAgent_sbapi.la
libCcspWifiAgent_sbapi_la_CPPFLAGS = -I/var/tmp/pc-rdkb/include/dbus-1.0 -I$(top_builddir)/../CcspCommonLibrary/source/ccsp/custom -I$(top_builddir)/../CcspCommonLibrary/source/ccsp/include -I$(top_builddir)/../CcspCommonLibrary/source/debug_api/include -I$(top_builddir)/../CcspCommonLibrary/source/cosa/include -I$(top_builddir)/../CcspCommonLibrary/source/cosa/include/linux -I$(top_builddir)/../CcspCommonLibrary/source/ccsp/components/include -I$(top_builddir)/../CcspCommonLibrary/source/cosa/package/slap/include -I$(top_builddir)/../hal/include -I$(top_builddir)/../CcspCommonLibrary/source/util_api/http/include -I$(top_builddir)/../CcspCommonLibrary/source/util_api/ansc/include -I$(top_builddir)/source/TR-181/ml -I$(top_builddir)/source/TR-181/sbapi -I$(top_builddir)/include/TR-181/ml -I$(top_builddir)/../CcspCommonLibrary/source/ccsp/components/common/MessageBusHelper/include -I$(top_builddir)/../CcspCommonLibrary/source/ccsp/components/common/PoamIrepFolder -I$(top_builddir)/./include -I$(top_builddir)/source/Custom $(CPPFLAGS)
libCcspWifiAgent_sbapi_la_SOURCES = pack_file.c cosa_dbus_api.c cosa_wifi_apis.c
libCcspWifiAgent_sbapi_la_LDFLAGS =  -L$(top_builddir)/../CcspCommonLibrary/lib -lccsp_common -L$(top_builddir)/../hal/lib -lhal_wifi