AM_CFLAGS = -D_ANSC_LINUX
AM_CFLAGS += -D_ANSC_USER
AM_CFLAGS += -D_ANSC_LITTLE_ENDIAN_
AM_LDFLAGS = -lccsp_common
AM_LDFLAGS += -lhal_wifi

AM_CPPFLAGS = -Wall -Werror
ACLOCAL_AMFLAGS = -I m4
hardware_platform = i686-linux-gnu
bin_PROGRAMS = CcspWifiSsp
CcspWifiSsp_CPPFLAGS = -I$(top_srcdir)/source/TR-181/ml -I$(top_srcdir)/source/TR-181/sbapi -I$(top_srcdir)/include/TR-181/ml -I$(top_srcdir)/./include -I$(top_srcdir)/source/Custom $(CPPFLAGS)
CcspWifiSsp_SOURCES = ssp_messagebus_interface.c ssp_main.c ssp_action.c
CcspWifiSsp_LDFLAGS = -lccsp_common -lhal_wifi
