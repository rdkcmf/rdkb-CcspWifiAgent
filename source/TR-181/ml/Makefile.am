AM_CFLAGS = -D_ANSC_LINUX
AM_CFLAGS += -D_ANSC_USER
AM_CFLAGS += -D_ANSC_LITTLE_ENDIAN_
AM_LDFLAGS = -lccsp_common
AM_LDFLAGS += -lhal_wifi

AM_CPPFLAGS = -Wall -Werror
ACLOCAL_AMFLAGS = -I m4
hardware_platform = i686-linux-gnu

noinst_LTLIBRARIES = libCcspWifiAgent_ml.la
libCcspWifiAgent_ml_la_CPPFLAGS = -I$(top_srcdir)/source/TR-181/ml -I$(top_srcdir)/source/TR-181/sbapi -I$(top_srcdir)/include/TR-181/ml -I$(top_srcdir)/./include -I$(top_srcdir)/source/Custom $(CPPFLAGS)
libCcspWifiAgent_ml_la_SOURCES = cosa_apis_util.c plugin_main.c cosa_apis_busutil.c plugin_main_apis.c cosa_wifi_internal.c cosa_wifi_dml.c
libCcspWifiAgent_ml_la_LDFLAGS = -lccsp_common -lhal_wifi
