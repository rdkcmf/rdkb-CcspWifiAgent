AM_CFLAGS = -D_ANSC_LINUX
AM_CFLAGS += -D_ANSC_USER
AM_CFLAGS += -D_ANSC_LITTLE_ENDIAN_
AM_LDFLAGS = -L$(top_srcdir)/../CcspCommonLibrary/lib
AM_LDFLAGS += -lccsp_common
AM_LDFLAGS += -L$(top_srcdir)/../hal/lib
AM_LDFLAGS += -lhal_wifi

AM_CPPFLAGS = -Wall -Werror
ACLOCAL_AMFLAGS = -I m4
hardware_platform = i686-linux-gnu
bin_PROGRAMS = CcspWifiSsp
CcspWifiSsp_CPPFLAGS = -I/var/tmp/pc-rdkb/include/dbus-1.0 -I$(top_srcdir)/../CcspCommonLibrary/source/ccsp/custom -I$(top_srcdir)/../CcspCommonLibrary/source/ccsp/include -I$(top_srcdir)/../CcspCommonLibrary/source/debug_api/include -I$(top_srcdir)/../CcspCommonLibrary/source/cosa/include -I$(top_srcdir)/../CcspCommonLibrary/source/cosa/include/linux -I$(top_srcdir)/../CcspCommonLibrary/source/ccsp/components/include -I$(top_srcdir)/../CcspCommonLibrary/source/cosa/package/slap/include -I$(top_srcdir)/../hal/include -I$(top_srcdir)/../CcspCommonLibrary/source/util_api/http/include -I$(top_srcdir)/../CcspCommonLibrary/source/util_api/ansc/include -I$(top_srcdir)/source/TR-181/ml -I$(top_srcdir)/source/TR-181/sbapi -I$(top_srcdir)/include/TR-181/ml -I$(top_srcdir)/../CcspCommonLibrary/source/ccsp/components/common/MessageBusHelper/include -I$(top_srcdir)/../CcspCommonLibrary/source/ccsp/components/common/PoamIrepFolder -I$(top_srcdir)/./include -I$(top_srcdir)/source/Custom $(CPPFLAGS)
CcspWifiSsp_SOURCES = ssp_messagebus_interface.c ssp_main.c ssp_action.c
CcspWifiSsp_LDFLAGS =  -L$(top_srcdir)/../CcspCommonLibrary/lib -lccsp_common -L$(top_srcdir)/../hal/lib -lhal_wifi