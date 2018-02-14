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
	                char pTempChar1[4096];                                     \
                    CcspTraceBaseStr msg;						\
		    WRITELOG	\
}				
#define  CcspWifiEventTrace(msg)                                                              \
{                                                                                             \
                char pTempChar1[4096];                                    \
                    CcspTraceBaseStr msg;                                                     \
                    WriteLog(pTempChar1,bus_handle,"eRT.","Device.LogAgent.WifiEventLogMsg"); \
}


 //   WriteLog(msg,bus_handle,g_Subsystem,"Device.LogAgent.WifiLogMsg")
#endif
