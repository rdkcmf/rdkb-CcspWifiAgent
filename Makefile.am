SUBDIRS = source

lib_LTLIBRARIES=libwifi.la
libwifi_la_LDFLAGS=
libwifi_la_SOURCES=
libwifi_la_DEPENDENCIES= \
	source/TR-181/ml/libCcspWifiAgent_ml.la \
	source/TR-181/sbapi/libCcspWifiAgent_sbapi.la

libwifi_la_LIBADD=$(libwifi_la_DEPENDENCIES)
