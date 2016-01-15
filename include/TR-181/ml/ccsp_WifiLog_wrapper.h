#ifndef  _CCSP_WIFILOG_WRPPER_H_ 
#define  _CCSP_WIFILOG_WRPPER_H_

#include "ccsp_custom_logs.h"
extern ANSC_HANDLE bus_handle;
extern char g_Subsystem[32];
/*
 * Logging wrapper APIs g_Subsystem
 */
#define  CcspTraceBaseStr(arg ...)                                                             \
            do {                                                                            \
                snprintf(pTempChar1, 4095, arg);                                         	\
            } while (FALSE)
			
#define  CcspWifiTrace(msg)                         \
{\
	                char*   pTempChar1 = (char*)malloc(4096);                                     \
                if ( pTempChar1 )                                                             \
                {                                                                            \
					char	sfn[32];														 \
                    CcspTraceBaseStr msg;                                                       \
				    WriteLog(pTempChar1,bus_handle,g_Subsystem,"Device.LogAgent.WifiLogMsg"); \
                    free(pTempChar1);                                                         \
                }\
}				
#define  CcspWifiEventTrace(msg)                                                              \
{                                                                                             \
                char*   pTempChar1 = (char*)malloc(4096);                                     \
                if ( pTempChar1 )                                                             \
                {                                                                             \
                    char    sfn[32];                                                          \
                    CcspTraceBaseStr msg;                                                     \
                    WriteLog(pTempChar1,bus_handle,"eRT.","Device.LogAgent.WifiEventLogMsg"); \
                    free(pTempChar1);                                                         \
                }                                                                             \
}


 //   WriteLog(msg,bus_handle,g_Subsystem,"Device.LogAgent.WifiLogMsg")
#endif