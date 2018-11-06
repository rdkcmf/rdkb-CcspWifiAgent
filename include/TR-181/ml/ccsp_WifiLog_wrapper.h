/************************************************************************************
  If not stated otherwise in this file or this component's Licenses.txt file the
  following copyright and licenses apply:

  Copyright 2018 RDK Management

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
**************************************************************************/
#ifndef  _CCSP_WIFILOG_WRPPER_H_ 
#define  _CCSP_WIFILOG_WRPPER_H_

#include "ccsp_custom_logs.h"
#include "cosa_wifi_apis.h"
extern ANSC_HANDLE bus_handle;
extern char g_Subsystem[32];

#define WRITELOG WriteWiFiLog(pTempChar1);

/*
 * Logging wrapper APIs g_Subsystem
 */
#define  CcspTraceBaseStr(arg ...)                                                             \
            do {                                                                            \
                snprintf(pTempChar1, 4095, arg);                                         	\
            } while (FALSE)
			
#define CcspWifiTrace(msg)                         \
{\
	                char*   pTempChar1 = (char*)malloc(4096);                                     \
                if ( pTempChar1 )                                                             \
                {                                                                            \
                    CcspTraceBaseStr msg;						\
		    WRITELOG	\
                    free(pTempChar1);                                                         \
                }\
}				
#define  CcspWifiEventTrace(msg)                                                              \
{                                                                                             \
                char*   pTempChar1 = (char*)malloc(4096);                                     \
                if ( pTempChar1 )                                                             \
                {                                                                             \
                    CcspTraceBaseStr msg;                                                     \
                    WriteLog(pTempChar1,bus_handle,"eRT.","Device.LogAgent.WifiEventLogMsg"); \
                    free(pTempChar1);                                                         \
                }                                                                             \
}


 //   WriteLog(msg,bus_handle,g_Subsystem,"Device.LogAgent.WifiLogMsg")
#endif
