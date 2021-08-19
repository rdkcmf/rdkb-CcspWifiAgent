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
#                                              -*- Autoconf -*-
# Process this file with autoconf to produce configure script.
#

AC_PREREQ([2.65])
AC_INIT([wifi], [1.0], [BUG-REPORT-ADDRESS])
AM_INIT_AUTOMAKE
LT_INIT
GTEST_ENABLE_FLAG = ""

AC_ARG_ENABLE([gtestapp],
             AS_HELP_STRING([--enable-gtestapp],[enable Gtest support (default is no)]),
             [
              case "${enableval}" in
               yes) GTEST_SUPPORT_ENABLED=true
                    GTEST_ENABLE_FLAG="-DGTEST_ENABLE"
                    m4_if(m4_sysval,[0],[AC_CONFIG_FILES([source/test/Makefile])]);;
               no) GTEST_SUPPORT_ENABLED=false AC_MSG_ERROR([Gtest support is disabled]);;
               *) AC_MSG_ERROR([bad value ${enableval} for --enable-gtestapp ]);;
              esac
             ],
             [echo "Gtestapp is disabled"])
AM_CONDITIONAL([WITH_GTEST_SUPPORT], [test x$GTEST_SUPPORT_ENABLED = xtrue])
AC_PREFIX_DEFAULT(`pwd`)
AC_ENABLE_SHARED
AC_DISABLE_STATIC

AC_ARG_ENABLE([rdk-wifi-libhostap],
	[AS_HELP_STRING([--enable-rdk-wifi-libhostap],[enable])],
	[
	 case x"${enableval}" in
	 xyes) CCSP_HOSTAP_AUTH=yes ;;
	 xno) AC_MSG_ERROR([hostap authenticator is disabled]) ;;
	 *) AC_MSG_ERROR([bad value ${enableval} for --enable-hostap-auth ]) ;;
	 esac
	 ],
[CCSP_HOSTAP_AUTH=''])
if test x"${CCSP_HOSTAP_AUTH}" != x; then
  AC_DEFINE_UNQUOTED(CCSP_HOSTAP_AUTH, "$CCSP_HOSTAP_AUTH",
                     [The CCSP platform device])
fi
AM_CONDITIONAL(FEATURE_HOSTAP_AUTHENTICATOR, test x"$CCSP_HOSTAP_AUTH" = xyes)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AM_PROG_CC_C_O
AM_PROG_LIBTOOL(libtool)

SYSTEMD_CFLAGS=" "
SYSTEMD_LDFLAGS=" "

AC_ARG_ENABLE([notify],
        AS_HELP_STRING([--enable-notify],[enable systemd notify]),
        [
          case "${enableval}" in
           yes) SYSTEMD_CFLAGS="-DENABLE_SD_NOTIFY"
                SYSTEMD_LDFLAGS="-lsystemd" ;;
           no) AC_MSG_ERROR([systemd notify is disabled]) ;;
          *) AC_MSG_ERROR([bad value ${enableval} for --enable-notify ]) ;;
           esac
           ],
         [echo "systemd notify is disabled."])

AC_SUBST(SYSTEMD_CFLAGS)
AC_SUBST(SYSTEMD_LDFLAGS)

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE

# Checks for library functions.
AC_FUNC_MALLOC

AC_CONFIG_FILES(
	source/TR-181/lib/common/Makefile
	source/TR-181/lib/const/Makefile
	source/TR-181/lib/ds/Makefile
	source/TR-181/lib/json_util/Makefile
	source/TR-181/lib/log/Makefile
	source/TR-181/lib/osa/Makefile
	source/TR-181/lib/ovsdb/Makefile
	source/TR-181/lib/pjs/Makefile
	source/TR-181/lib/schema/Makefile
	source/TR-181/lib/Makefile
	source/TR-181/sbapi/Makefile
	source/TR-181/ml/Makefile
	source/TR-181/Makefile
	source/WifiSsp/Makefile
	source/Makefile
	source/sampleapps/Makefile
	Makefile
)

AC_SUBST(GTEST_ENABLE_FLAG)
AC_OUTPUT

