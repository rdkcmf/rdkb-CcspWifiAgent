/**********************************************************************
   Copyright [2014] [Cisco Systems, Inc.]
 
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at
 
       http://www.apache.org/licenses/LICENSE-2.0
 
   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
**********************************************************************/

/*****************************************************************************
**
** athcfg_api.c
**
** This is a translation layer used to process RPC commands and tranlsate
** them to native wireless APIs.
**
** Copyright (c) 2011, Atheros Communications Inc.
**
** Permission to use, copy, modify, and/or distribute this software for any
** purpose with or without fee is hereby granted, provided that the above
** copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
** WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
** ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
** WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
** ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
** OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**
******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>

#define WPA_CONFIG_ROOT "/nvram/etc/wpa2"

typedef enum bool {
    false = 0,
    true  = 1,
} bool;

typedef struct WLAN_basicStats {
    unsigned int Wlan_totalBytesSent;
    unsigned int Wlan_totalBytesReceived;
    unsigned int Wlan_totalPacketsSent;
    unsigned int Wlan_totalPacketsReceived;
    unsigned int Wlan_totalAssociations;
} WLAN_basicStats;

typedef struct WLAN_stats {
    unsigned int Wlan_ErrorsSent;
    unsigned int Wlan_ErrorReceived;
    unsigned int Wlan_UnicastPacketsSent;
    unsigned int Wlan_UnicastPacketsReceived;
    unsigned int Wlan_DiscardPacketsSent;
    unsigned int Wlan_DiscardPacketsReceived;
    unsigned int Wlan_MulticastPacketsSent;
    unsigned int Wlan_MulticastPacketsReceived;
    unsigned int Wlan_BroadcastPacketsSent;
    unsigned int Wlan_BroadcastPacketsReceived;
    unsigned int Wlan_UnknownProtPacketsReceived;
} WLAN_stats;

typedef struct WLAN_Device {
    unsigned char  Wlan_devMacAddress[6];
    char Wlan_devIPAddress[64];
    bool Wlan_devAssociatedDeviceAuthenticationState;
    int    Wlan_devSignalStrength;
    int    Wlan_devTxRate;
    int    Wlan_devRxRate;
} WLAN_Device;

#define MAC_ADDR_SIZE 18 

static int athcfg_executecmd(const char *caller, char *cmd, char *retBuf)
{
    FILE *f;
    char *ptr = retBuf;

    if((f = popen(cmd, "r")) == NULL) {
        printf("%s: popen %s error\n",caller, cmd);
        return -1;
    }

    while(!feof(f))
    {
        *ptr = 0;
        fgets(ptr,120,f);

        //printf("ptr = %p, len %d, s %s\n", ptr, strlen(ptr), ptr);

        if(strlen(ptr) == 0)
        {
            break;
        }
        ptr += strlen(ptr);
    }
    pclose(f);

    return 0;
}

#define USGV2
#ifdef USGV2
static int iwpriv_getbyname(int index, char *paraName, char *retBuf)
{
    char cmd[64];
    char buf[128];
    char *pos;

    sprintf(cmd, "iwpriv ath%d %s",index, paraName);
    
    if(athcfg_executecmd(__func__,cmd,buf))
        return -1;

    pos = buf;
    if((pos=strstr(pos,":"))!=NULL)
    {
        pos +=1;
        strcpy(retBuf, pos);
    } else {
        return -1;
    }

    return 0;
}
static int iwpriv_setbyname(int index, char *paraName, char *value)
{
    char cmd[64];
    char buf[128];
    
    sprintf(cmd, "iwpriv ath%d %s %s",index, paraName, value );
    
    if(athcfg_executecmd(__func__,cmd,buf))
        return -1;

    return 0;
}
#endif

static int athcfg_getbyname(int index, char *paraName, char *retBuf)
{
    char cmd[64];
    char buf[128];
    char *pos;

    if(index >= 1) {
        if((strstr(paraName, "AP_PRIMARY_KEY")!=NULL) ||
           (strstr(paraName, "AP_WEP_MODE")!=NULL))
            sprintf(cmd, "cfg -s | grep %s_%d:=",paraName, /*radio 1*/ 1);
        else if (strstr(paraName, "WEP_RADIO_NUM")!=NULL)
            sprintf(cmd, "cfg -s | grep %s:=",paraName);
        else
            sprintf(cmd, "cfg -s | grep %s_%d:=",paraName,index+1);
    } else {
        if((strstr(paraName, "AP_PRIMARY_KEY")!=NULL) ||
           (strstr(paraName, "AP_WEP_MODE")!=NULL))
            sprintf(cmd, "cfg -s | grep %s_%d:=",paraName, /*radio 0*/ 0);
        else if (strstr(paraName, "WEP_RADIO_NUM")!=NULL)
            sprintf(cmd, "cfg -s | grep %s:=",paraName);
        else
            sprintf(cmd, "cfg -s | grep %s:=",paraName);
    }
    
    if(athcfg_executecmd(__func__,cmd,buf))
        return -1;

    pos = buf;
    if((pos=strstr(pos,":="))!=NULL)
    {
        pos +=2;
        strcpy(retBuf, pos);
    } else {
        return -1;
    }

    return 0;
}

void add_backslash(char *input, char *output)
{
    while(*input!='\0'){
        /* Single quotes don't require a backslash. */
        if (strncmp(input, "\"", 1)==0) {
            strcat(output, "\\\"");	/* two for shell, one for here */
        } else if (strncmp(input, "`", 1)==0) {
            strcat(output, "\\`");	/* two for shell */
        } else if (strncmp(input, "\\", 1)==0) {
            strcat(output, "\\\\");	/* two for shell, one for here */
        } else if (strncmp(input, "$", 1)==0) {
            strcat(output, "\\$");	/* two for shell */
        } else
            strncat(output, input, 1);
        input++;
    }
}

static int athcfg_setbyname(int index, char *paraName, char *val)
{
    char cmd[128];
    char buf[128];
    char val_enc[128];
    strcpy(val_enc, "");
    add_backslash(val, val_enc);

    /* Index can refer to radio index in some cases, like WEP setting */
    if(index >= 1) {  
        if((strstr(paraName, "AP_PRIMARY_KEY")!=NULL) ||
           (strstr(paraName, "AP_WEP_MODE")!=NULL))
            sprintf(cmd, "cfg -a %s_%d='%s';cfg -c",paraName, /* radio num */ 1, val);
        else if (strstr(paraName, "WEP_RADIO_NUM")!=NULL)
            sprintf(cmd, "cfg -a %s='%s';cfg -c",paraName, val);
        else
            sprintf(cmd, "cfg -a %s_%d=\"%s\";cfg -c",paraName,index+1,val_enc);
    } else {
        if((strstr(paraName, "AP_PRIMARY_KEY")!=NULL) ||
           (strstr(paraName, "AP_WEP_MODE")!=NULL))
            sprintf(cmd, "cfg -a %s_%d='%s';cfg -c",paraName, /*radio num */ 0, val);
        else if (strstr(paraName, "WEP_RADIO_NUM")!=NULL)
            sprintf(cmd, "cfg -a %s='%s';cfg -c",paraName, val);
        else
            sprintf(cmd, "cfg -a %s=\"%s\";cfg -c",paraName,val_enc);
    }

    if(athcfg_executecmd(__func__,cmd,buf))
        return -1;

    return 0;
}

static int athwps_getbyname(int index, char *paraName, char *retBuf)
{
    char tmp[64];
    char buf[256];
    char *pos;

    retBuf[0] = '\0';

    sprintf(tmp, "cat %s/WSC_ath%d.conf 2>/dev/null | grep %s",WPA_CONFIG_ROOT, index, paraName);
    athcfg_executecmd(__func__,tmp,buf);

    if((pos=strstr(buf, "="))!=NULL) {
        pos +=1;
        strcpy(retBuf, pos);
    } else {
        return -1;
    }

    return 0;
}

static int athwps_setbyname(int index, char *paraName, char *val)
{
    FILE *f1, *f2;
    char buff[256];
    char fname[64], tmpfname[64];
    char cmd[64];
    char *vPtr;
    int found = 0;
    char *pos;

    /* fexist ?? */
    sprintf(cmd, "ls %s | grep ath%d",WPA_CONFIG_ROOT, index);
    athcfg_executecmd(__func__,cmd,buff);
    if(!strlen(buff)) {
        printf("%s: file ath%d not exist\n",__func__, index);
        return -1;
    }
    
    sprintf(fname, "%s/WSC_ath%d.conf",WPA_CONFIG_ROOT, index);
    sprintf(tmpfname, "%s/WSC_ath%d.conf.tmp",WPA_CONFIG_ROOT, index);
    f1 = fopen(fname, "r");
    if(!f1) {
        printf("%s: open %s failed\n",__func__,fname);
        return -1;
    }
    f2 = fopen(tmpfname, "w");
    if(!f2) {
        fclose(f1);
        printf("%s: open %s failed\n",__func__,tmpfname);
        return -1;
    }

    while(!feof(f1)) {
        fgets(buff,256,f1);
        
        if(buff[0] == 0) {
            break;
        }

        vPtr=strchr(buff,0x0a);
        if(!vPtr) {
            break;
        } else {
            *vPtr = 0;
        }

        if((pos=strstr(buff, paraName))!=NULL) {
            pos += strlen(paraName);
            if(*pos != '=') {
                fprintf(f2, "%s\n",buff);
                continue;
            }
            found = 1;
            fprintf(f2, "%s=%s\n",paraName,val);
        } 
        else {
            fprintf(f2, "%s\n",buff);
        }
    }

    if(!found) {
        /* add to the last line */
        fprintf(f2, "%s=%s\n",paraName,val);
    }

    /* force the buffers to be flushed to the storage device */
    fsync(fileno(f2));

    fclose(f1);
    fclose(f2);
    /* overwrite */
    sprintf(cmd, "rm %s;mv %s %s",fname, tmpfname, fname);
    athcfg_executecmd(__func__,cmd,buff);
    
    return 0;
}

static int athwps_delbyname(int index, char *paraName)
{
    FILE *f1, *f2;
    char buff[256];
    char fname[64], tmpfname[64];
    char cmd[64];
    char *vPtr;
    int found = 0;
    char *pos;

    /* fexist ?? */
    sprintf(cmd, "ls %s | grep ath%d",WPA_CONFIG_ROOT, index);
    athcfg_executecmd(__func__,cmd,buff);
    if(!strlen(buff)) {
        printf("%s: file ath%d not exist\n",__func__, index);
        return -1;
    }
    
    sprintf(fname, "%s/WSC_ath%d.conf",WPA_CONFIG_ROOT, index);
    sprintf(tmpfname, "%s/WSC_ath%d.conf.tmp",WPA_CONFIG_ROOT, index);
    f1 = fopen(fname, "r");
    if(!f1) {
        printf("%s: open %s failed\n",__func__,fname);
        return -1;
    }
    f2 = fopen(tmpfname, "w");
    if(!f2) {
        fclose(f1);
        printf("%s: open %s failed\n",__func__,tmpfname);
        return -1;
    }

    while(!feof(f1)) {
        fgets(buff,256,f1);
        
        if(buff[0] == 0) {
            break;
        }

        vPtr=strchr(buff,0x0a);
        if(!vPtr) {
            break;
        } else {
            *vPtr = 0;
        }

        if((pos=strstr(buff, paraName))!=NULL) {
            pos += strlen(paraName);
            if(*pos != '=') {
                fprintf(f2, "%s\n",buff);
                continue;
            }
            // Don't write to tmp file since we are removing this variable
            found = 1;
        } 
        else {
            fprintf(f2, "%s\n",buff);
        }
    }

    /* force the buffers to be flushed to the storage device */
    fsync(fileno(f2));

    fclose(f1);
    fclose(f2);
    /* overwrite if entry was present */
    if (found == 1)
    {
        sprintf(cmd, "rm %s;mv %s %s",fname, tmpfname, fname);
    } else {
    /* otherwise remove the tmp file */
        sprintf(cmd, "rm %s",tmpfname);
    }

    athcfg_executecmd(__func__,cmd,buff);
    
    return 0;
}

static int check_env(void)
{
    char tmp[256];
    char *path;
    
    /* check env */
    path = getenv("PATH");
    if(strstr(path, "/etc/ath") == NULL) {
        sprintf(tmp, "%s:/etc/ath",path);
        setenv("PATH", tmp, 1);
    }

    return 0; 
}   

int wlan_getActiveAthIndex(int radioIndex, int *activeIndex)
{
    int athIndex;
    char buf[1024];
    char cmd[128];


    // Find the first ath that is up on the given radio
    for (athIndex = radioIndex; athIndex < 16; athIndex+=2) {

        sprintf(cmd,"iwconfig ath%d 2>&1", athIndex);
        athcfg_executecmd(__func__, cmd, buf);
        // printf("iwconfig ath%d returned \n %s \n", athIndex,buf);
        
        if (strstr(buf,"No such device") == NULL) { 
            *activeIndex = athIndex;
            return 0;
        }
    }
    return 1;
}

int wlan_isActiveAth(int athIndex)
{
    char buf[1024];
    char cmd[128];

    sprintf(cmd,"iwconfig ath%d 2>&1", athIndex);
    athcfg_executecmd(__func__, cmd, buf);
    // printf("iwconfig ath%d returned \n %s \n", athIndex,buf);

    if (strstr(buf,"No such device") == NULL) { 
        return true;
    } 

    return false;
}

int wlan_getRadioIndex(int index, int *radio_idx)
{
    char tmp[32];

    check_env();
    athcfg_getbyname(index, "AP_RADIO_ID", tmp);

    if (strstr(tmp, "1") != NULL) {
        *radio_idx=1;
    }
    else {
        *radio_idx=0;
    }

    return 0;
}

int wlan_setRadioIndex(int index, int *radio_idx)
{
    char tmp[32];
    
    check_env();

    sprintf(tmp,"%d",radio_idx);
    athcfg_setbyname(index, "AP_RADIO_ID", tmp);

    return 0;
}

static int athcfg_get_radio(int index)
{
    char tmp[32];
    int radio_idx;

    check_env();
    athcfg_getbyname(index, "AP_RADIO_ID", tmp);

    if (strstr(tmp, "1") != NULL) {
        radio_idx=1;
    }
    else {
        radio_idx=0;
    }

    return radio_idx;
}

    
int wlan_init()
{
    char buf[1024];
    
    check_env();
    
    if(athcfg_executecmd(__func__, "apup > /dev/null 2>&1", buf))
        return -1;

    return 0;
 
}

int wlan_initRadio(int index)
{
    char buf[1024];
    char cmd[32];
    
    check_env();
    if (index < 0 || index > 1)
        return -1;
    
    sprintf(cmd,"apup %d > /dev/null 2>&1",index);
    if(athcfg_executecmd(__func__, cmd, buf))
        return -1;

    return 0;
}

int wlan_initVap(int radio, int vap)
{
    char buf[10024];
    char cmd[64];
    
    check_env();
    if (radio < 0 || radio > 1)
        return -1;
    
    if (vap < 0 || vap > 15)
        return -1;
    
    sprintf(cmd,"apup %d ath%d > /dev/null 2>&1",radio, vap);
    
    if(athcfg_executecmd(__func__, cmd, buf))
        return -1;

    return 0;
}

int wlan_createVap(int vap, int radio, char *essid, bool hideSsid)
{
    char buf[1024];
    char cmd[128];
    
    check_env();
    if (radio < 0 || radio > 1)
        return -1;
    
    if (vap < 0 || vap > 15)
        return -1;
    
    sprintf(cmd,"wlanconfig ath%d create wlandev wifi%d wlanmode ap", vap, radio);
    printf("%s: %s\n", __func__,  cmd);
    if(athcfg_executecmd(__func__, cmd, buf))
        return -1;

    sprintf(cmd,"iwpriv ath%d dbgLVL 0x100", vap);
    if(athcfg_executecmd(__func__, cmd, buf))
        return -1;

    sprintf(cmd,"iwconfig ath%d essid %s mode master", vap, essid);
    if(athcfg_executecmd(__func__, cmd, buf))
        return -1;

    sprintf(cmd,"iwpriv ath%d hide_ssid %d", vap, hideSsid);
    if(athcfg_executecmd(__func__, cmd, buf))
        return -1;

    // 11n configuration section increase queue length
    sprintf(cmd,"ifconfig ath%d txqueuelen 1000", vap);
    if(athcfg_executecmd(__func__, cmd, buf))
        return -1;

    return 0;
}

int wlan_deleteVap(int vap)
{
    char buf[1024];
    char cmd[128];
    
    sprintf(cmd,"wlanconfig ath%d destroy", vap);
    printf("%s: %s\n", __func__,  cmd);
    if(athcfg_executecmd(__func__, cmd, buf))
        return -1;

    sprintf(cmd,"sed -i 's/\\/nvram\\/etc\\/wpa2\\/WSC_ath%d.conf//g' /tmp/conf_filename", vap);
    if(athcfg_executecmd(__func__, cmd, buf))
        return -1;

    sprintf(cmd,"sed -i 's/\\/tmp\\//secath%d//g' /tmp/conf_filename", vap);
    if(athcfg_executecmd(__func__, cmd, buf))
        return -1;

    return 0;
}

int wlan_removeSecFromConfFile(int vap)
{
    char buf[1024];
    char cmd[128];
    
    sprintf(cmd,"sed -i 's/\\/nvram\\/etc\\/wpa2\\/WSC_ath%d.conf //g' /tmp/conf_filename", vap);
    if(athcfg_executecmd(__func__, cmd, buf))
        return -1;

    sprintf(cmd,"sed -i 's/\\/tmp\\/secath%d //g' /tmp/conf_filename", vap);
    if(athcfg_executecmd(__func__, cmd, buf))
        return -1;

    return 0;
}

int wlan_reset()
{
    char buf[1024];

    check_env();
    if(athcfg_executecmd(__func__, "apdown > /dev/null 2>&1", buf))
        return -1;
    if(athcfg_executecmd(__func__, "apup > /dev/null 2>&1", buf))
        return -1;

    return 0; 

}

int wlan_down()
{
    char buf[1024];
    
    check_env();

    if(athcfg_executecmd(__func__, "apdown > /dev/null 2>&1", buf))
        return -1;

    return 0;
 
}

int wlan_createSecurityFile(int vap, bool createWpsCfg)
{
    char buf[1024];
    char cmd[128];

    bool wepAth0;
    bool wepAth1;


    // Make sure there is no entry for the SSID in /tmp/conf_filename
    wlan_removeSecFromConfFile(vap);

    // Create WSC_ath*.conf file.  WPS can be off if security mode = None, but still want WSC type file
    if (createWpsCfg == true)
    { 
        int wpsCfg = 0;

        wlan_getWpsEnable(vap, &wpsCfg);

        if(wpsCfg == 0) 
       {
            sprintf(cmd,"iwpriv ath%d wps 0", vap);
            athcfg_executecmd(__func__, cmd, buf);
        } else {
            sprintf(cmd,"iwpriv ath%d wps 1", vap);
            athcfg_executecmd(__func__, cmd, buf);
        }

        // Always regenerate WSC file to ensure that it is accurate and not corrupt
        sprintf(cmd, "cfg -t%d /nvram/etc/ath/WSC.conf > /nvram/etc/wpa2/WSC_ath%d.conf",vap+1, vap);
        athcfg_executecmd(__func__, cmd, buf);

        sprintf(cmd,"echo -e \"/nvram/etc/wpa2/WSC_ath%d.conf \\c\\h\" >> /tmp/conf_filename", vap);
        athcfg_executecmd(__func__, cmd, buf);

    } else
    {
        sprintf(cmd,"iwpriv ath%d wps 0", vap);
        athcfg_executecmd(__func__, cmd, buf);

        sprintf(cmd,"cfg -t%d /nvram/etc/ath/PSK.ap_bss ath%d > /tmp/secath%d", vap+1, vap, vap);
        athcfg_executecmd(__func__, cmd, buf);

        sprintf(cmd,"echo -e \"/tmp/secath%d \\c\\h\" >> /tmp/conf_filename", vap);
        athcfg_executecmd(__func__, cmd, buf);
    }

    return 0;
}

int wlan_setEncryptionOff(int vap )
{
    char buf[1024];
    char cmd[128];

    printf("%s : ath%d\n", __func__, vap);

    sprintf(cmd,"iwconfig ath%d enc off", vap);
    printf("%s: %s\n", __func__, cmd);
    athcfg_executecmd(__func__, cmd, buf);

    sprintf(cmd,"iwpriv ath%d authmode 1", vap);
    printf("%s: %s\n", __func__, cmd);
    athcfg_executecmd(__func__, cmd, buf);

    sprintf(cmd,"iwpriv ath%d wps 0", vap);
    athcfg_executecmd(__func__, cmd, buf);

    return 0;
}

int wlan_setAuthMode(int vap, int mode )
{
    char buf[1024];
    char cmd[128];

    printf("%s : ath%d\n", __func__, vap);

    sprintf(cmd,"iwpriv ath%d authmode %d", vap, mode);
    printf("%s: %s\n", __func__, cmd);
    athcfg_executecmd(__func__, cmd, buf);

    return 0;
}

int wlan_startHostapd()
{
    char buf[1024];
    char cmd[1024];
    char confFile[1024];
    char secFile[32];
    int i = 0;

    check_env();

    memset(confFile,0,sizeof(confFile));

    athcfg_executecmd(__func__, "cat /tmp/conf_filename", buf);
    for (i = 0; i < 16; i++)
    {
        snprintf(secFile,32,"WSC_ath%d",i);
        if (strstr(buf,secFile) != 0)
        {
            sprintf(confFile,"%s /nvram/etc/wpa2/%s.conf", confFile, secFile );
            printf("adding  %s\n", secFile );
            
        } else {
            snprintf(secFile,32,"secath%d",i);
            if (strstr(buf,secFile) != 0)
            {
                sprintf(confFile,"%s /tmp/%s", confFile, secFile );
                printf("adding  %s\n", secFile );
            } 
        }
    }
    printf("%s: %s\n", __func__, confFile);

    if (strlen(confFile) != 0)
    {
        // sprintf(cmd,"hostapd  -B `cat /tmp/conf_filename` -e /nvram/etc/wpa2/entropy -P /tmp/hostapd.pid 1>&2");
        sprintf(cmd,"hostapd  -B %s -e /nvram/etc/wpa2/entropy -P /tmp/hostapd.pid 1>&2", confFile);
        printf("%s: %s\n", __func__, cmd);
        if (athcfg_executecmd(__func__, cmd, buf))
            return -1;
    } else
    {
        // printf("%s: hostapd was not started because there where no SSID configured with WPA \n", __func__);
        #if 1
        sprintf(cmd,"hostapd  -B -e /nvram/etc/wpa2/entropy -P /tmp/hostapd.pid 1>&2");
        printf("%s: %s\n", __func__, cmd);
        if (athcfg_executecmd(__func__, cmd, buf))
            return -1;
        #endif
    }

    return 0;

}

int wlan_stopHostapd()
{
    char buf[1024];

    check_env();

    printf("%s : stopping hostapd\n", __func__);

    // Sleep 4 seconds after stopping hostapd so that the drivers can properly clean up
    if (athcfg_executecmd(__func__, "pkill hostapd; sleep 4", buf))
        return -1;

    return 0;

}

int wlan_ifconfigDown(int vap)
{
    char buf[1024];
    char cmd[32];

    sprintf(cmd,"ifconfig ath%d down", vap);
    printf("%s: %s\n", __func__, cmd);
    athcfg_executecmd(__func__, cmd, buf);

    return 0;
}

int wlan_ifconfigUp(int vap)
{
    char buf[1024];
    char cmd[32];

    sprintf(cmd,"ifconfig ath%d up 2>/dev/null", vap);
    printf("%s: %s\n", __func__, cmd);
    athcfg_executecmd(__func__, cmd, buf);

    return 0;
}
int wlan_getNumberOfEntries(int *numEntries)
{
    char buf[1024];
    int num = 0;
    char *pos;

    check_env();
    if(athcfg_executecmd(__func__, "ifconfig | grep ath | cut -f1 -d' '", buf))
        return -1;

    pos = buf;
    while((pos=strstr(pos,"ath"))!=NULL)
    {
        pos +=3;
        num++;
    } 

    *numEntries = num;

    return 0;
}

int wlan_getEnable(int index, bool *enable)
{
    char tmp[32];

    check_env();
    athcfg_getbyname(index, "AP_ENABLE", tmp);
    *enable = (atoi(tmp) == 0) ? false : true;

    return 0;
}

int wlan_setEnable(int index, bool enable)
{
    char tmp[32];
    
    check_env();
    sprintf(tmp,"%d",enable);
    athcfg_setbyname(index, "AP_ENABLE", tmp);

    /* SSID set if AP_ENABLE */
    if (enable && (athcfg_getbyname(index, "AP_SSID", tmp) < 0)) {
        sprintf(tmp, "Atheros_XSpan_%d", index);
        athcfg_setbyname(index, "AP_SSID", tmp);
    }

    return 0;

}

int wlan_getStatus(int index, char *status)
{
    char tmp[64];
    char buf[1024];
    char *pos;

    check_env();

    if(index < 0)
        strcpy(status, "Error");
    else {
        sprintf(tmp, "iwconfig ath%d",index);
        athcfg_executecmd(__func__,tmp,buf);

        pos = buf;
        if((pos=strstr(pos,"Access Point:"))!=NULL) {
            strcpy(status, "Up");
        } else {
            strcpy(status, "Disable");
        } 
    }

    return 0;
} 

int wlan_getName(int index, char *name)
{
    check_env();
    if(index < 0) {
        memset(name, 0 , sizeof(name));
        return 0;
    }

    sprintf(name, "ath%d",index);

    return 0;
}

int wlan_getBSSID(int index, char *bssid)
{
    char cmd[64];
    char buf[1024];
    char *pos;
    char tmp[32];
    bool enabled;
    int  count = 0;

    check_env();

    memset(bssid, 0 , sizeof(bssid));

    athcfg_getbyname(index, "AP_ENABLE", tmp);
    enabled = atoi(tmp);

    if ( (index < 0) || (enabled == false) ) {
        return -1;
    }

    sprintf(cmd, "ifconfig ath%d",index);
    athcfg_executecmd(__func__,cmd,buf);

    pos = buf;
    if((pos=strstr(pos,"HWaddr"))!=NULL)
    {
        pos +=7;
        memcpy(bssid, pos, 17);
        bssid[17] = 0;
    }

    return 0;
}

int wlan_getBaseBSSID(int index, char *bssid)
{
    char cmd[64];
    char buf[1024];
    char *pos;
    char tmp[32];
    bool enabled;
    int  count = 0;

    check_env();

    memset(bssid, 0 , sizeof(bssid));

    sprintf(cmd, "ifconfig -a wifi%d | grep HWaddr",index);
    athcfg_executecmd(__func__,cmd,buf);

    pos = buf;
    if((pos=strstr(pos,"HWaddr"))!=NULL) { 
        // Convert dashes to semicolons
        char *dash = strchr(pos, '-');
        while (dash!=NULL) {
            *dash = ':';
            dash = strchr(dash, '-');
        }

        pos +=7;
        memcpy(bssid, pos, 17);
        bssid[17] = 0;
    }

    return 0;
}

int wlan_getMaxBitRate(int index, char *maxBitRate)
{
    char cmd[64];
    char buf[1024];
    char *pos;

    check_env();

    if(index < 0) {
        memset(maxBitRate, 0 , sizeof(maxBitRate));
        return -1;
    }

    sprintf(cmd, "iwconfig ath%d",index);
    athcfg_executecmd(__func__,cmd,buf);

    pos = buf;
    if((pos=strstr(pos,"Bit Rate:"))!=NULL)
    {
        pos +=9;
        memcpy(maxBitRate, pos, 9);
        maxBitRate[9] = 0;
    } else {
        memset(maxBitRate, 0 , sizeof(maxBitRate));
    }

    return 0;
}

int wlan_setMaxBitRate(int index, char *maxBitRate)
{   
    check_env();
    /* support Auto for now */
    return 0;
}

int wlan_getChannel(int index, unsigned int *channel)
{
    char cmd[64];
    char buf[1024];
    char *pos;
    char freq[10];
    int  val;
    int athIndex;

    check_env();

    if(index < 0) {
        memset(channel, 0 , sizeof(channel));
        return -1;
    }

    // find active SSID, if there is none get channel from cfg file.
    if (wlan_getActiveAthIndex(index, &athIndex) == 0) { 
        sprintf(cmd, "iwconfig ath%d",athIndex);
    athcfg_executecmd(__func__,cmd,buf);
    } else {
        athcfg_getbyname(index, "AP_PRIMARY_CH", buf);
        *channel = atoi(buf);
        return 0;
    }

    pos = buf;
    if((pos=strstr(pos,"Frequency:"))!=NULL)
    {
        pos +=10;
        
        memcpy(freq, pos, 5);
        freq[5] = 0;

        pos = strstr(freq,".");
        *pos = 0;
        val = atoi(pos -1) * 1000; 

        if (val == 5000) {
            if ((!strcmp((pos + 3)," ")) || (!strcmp((pos + 3),"G"))) {
                *(pos + 3) = '0';
            }
            if (!strncmp((pos + 2)," ", 1)) {
                *(pos + 2) = '0';
            }
            val += atoi(pos + 1);
            *channel = (val - 5000)/5;
        }
        else {
            val += atoi(pos + 1);
            *channel = (val - 2407)/5;
        }
    }

    return 0;
}

int wlan_setChannel(int index, unsigned int channel)
{
    char tmp[32];
    bool enable;
    
    check_env();

    sprintf(tmp,"%d",channel);

    athcfg_setbyname(index, "AP_PRIMARY_CH", tmp);
    
    return 0;
}

int wlan_pushChannel(int athIndex, unsigned int channel)
{
    char tmp[128];
    char buf[1024];

    sprintf(tmp, "iwconfig ath%d freq %d",athIndex,channel);
    printf("%s: %s\n", __func__, tmp);
    athcfg_executecmd(__func__,tmp,buf);

    return 0;
}

int wlan_getAutoChannelEnable(int index, bool *enable)
{
    char tmp[32];
    char *pos = NULL;

    check_env();
    athcfg_getbyname(index, "AP_PRIMARY_CH", tmp);

    if ( atoi(tmp) == 0 ) {
	*enable = true;
    } else {
	*enable = false;
    } 

    return 0;
}

int wlan_setAutoChannelEnable(int index, bool enable)
{
    char tmp[32];
    int radio_idx;

    radio_idx = athcfg_get_radio(index);

    check_env();

    if (radio_idx == 1)
        index = 1;
    else
        index = 0;

    if(enable) 
    {
	sprintf(tmp,"%d",0);
	athcfg_setbyname(index, "AP_PRIMARY_CH", tmp);
    } else {
	if (index == 1) 
		sprintf(tmp,"%d",40);
	else 
		sprintf(tmp,"%d",6);

        athcfg_setbyname(index, "AP_PRIMARY_CH", tmp);
    }

    return 0;
}

int wlan_getSSID(int index, char *ssid)
{
    char tmp[64];

    check_env();
    if(!athcfg_getbyname(index, "AP_SSID", tmp))
    {
        strcpy(ssid,tmp);
        // remove new line character
        if ( strstr(ssid,"\n") != NULL) { ssid[strlen(ssid)-1] = 0; }
    }
    else
        strcpy(ssid,"");

    return 0; 
}

int wlan_setSSID(int index, char *ssid)
{
    char tmp[64];
    int wps_state;

    check_env();
    athcfg_setbyname(index, "AP_SSID", ssid);
    /* update wps configuration file */
    athcfg_getbyname(index, "WPS_ENABLE", tmp);
    wps_state = atoi(tmp);
    if(wps_state) {
        athwps_setbyname(index, "ssid", ssid);
    }
    return 0;
}

int wlan_pushSSID(int index, char *ssid)
{
    char tmp[128];
    char buf[1024];
    char val_enc[128];
    strcpy(val_enc, "");
    add_backslash(ssid, val_enc);

    sprintf(tmp, "iwconfig ath%d essid \"%s\"",index,val_enc);
    athcfg_executecmd(__func__,tmp,buf);

    return 0;
}

int wlan_getBeaconType(int index, char *beaconType)
{
    char tmp[64];    

    check_env();
    athcfg_getbyname(index, "AP_SECMODE", tmp);
    if(strstr(tmp,"None")!=NULL) {
        strcpy(beaconType, "None");
        return 0;
    }
    if(strstr(tmp,"WEP")!=NULL) {
        strcpy(beaconType, "Basic");
        return 0;
    } 
    if(strstr(tmp,"WPA")!=NULL) {
        athcfg_getbyname(index, "AP_WPA", tmp);
        if(strstr(tmp,"2")!=NULL) {
            strcpy(beaconType, "11i");
            return 0;
        } else if(strstr(tmp,"3")!=NULL) {
            strcpy(beaconType, "WPAand11i");
            return 0;
        } else {
            strcpy(beaconType, "WPA");
        }
    }
    return 0;
}

int wlan_setBeaconType(int index, char *beaconType)
{
    char tmp[64];
    char cmd[64];
    char buf[1500];
    int wpa_enabled = 0;
    int wpa_type = 0;
    int wps_state;

    check_env();
    if (strstr(beaconType, "None")!=NULL)
    {
        athcfg_setbyname(index, "AP_SECMODE", "None");    

        /* delete WSC config file */
        sprintf(cmd, "rm %s/WSC_ath%d.conf; rm /tmp/secath%d", WPA_CONFIG_ROOT, index, index);
        athcfg_executecmd(__func__,cmd,buf); 
        return 0;
    } else if (strstr(beaconType, "Basic")!=NULL)
    {
        athcfg_setbyname(index, "AP_SECMODE", "WEP");
    } else
    {
        if (strstr(beaconType, "WPAand11i")!=NULL)
        {
            athcfg_setbyname(index, "AP_SECMODE", "WPA");
            athcfg_setbyname(index, "AP_WPA", "3");
            wpa_enabled = 1;
            wpa_type = 3;
        } else if (strstr(beaconType, "11i")!=NULL)
        {
            athcfg_setbyname(index, "AP_SECMODE", "WPA");
            athcfg_setbyname(index, "AP_WPA", "2");
            wpa_enabled = 1;
            wpa_type = 2;
        } else if (strstr(beaconType, "WPA")!=NULL)
        {
            athcfg_setbyname(index, "AP_SECMODE", "WPA");
            athcfg_setbyname(index, "AP_WPA", "1");
            wpa_enabled = 1;
            wpa_type = 1;
        }
        /* update wps configuration file */
        athcfg_getbyname(index, "WPS_ENABLE", tmp);
        wps_state = atoi(tmp);
        if (wps_state)
        {
            sprintf(tmp, "%d", wpa_type);
            athwps_setbyname(index, "wpa", tmp);
            if (wpa_enabled)
            {
                athwps_setbyname(index, "wpa_key_mgmt", "WPA-PSK");
            } else
            {
                athwps_setbyname(index, "wpa_key_mgmt", "None");
            }
        }
    }

    return 0;      
}

int wlan_getMacAddressControlEnabled(int index, bool *enabled)
{
    char tmp[32];

    check_env();
    if (!athcfg_getbyname(index, "ACL_MODE", tmp))
        *enabled = (atoi(tmp) == 0) ? false : true;
    else
        *enabled = false;   
    
    return 0;
}

int wlan_setMacAddressControlEnabled(int index, bool enabled)
{
    char tmp[32];

    check_env();
    sprintf(tmp,"%d",enabled);
    athcfg_setbyname(index, "ACL_MODE", tmp);

    return 0;
}

int wlan_getStandard(int index, char *standard, bool *gOnly, bool *nOnly, bool *acOnly)
{
    char tmp[32];
    char tmp2[32];
    char tmp3[32];
    char tmp4[32];

    check_env();
    athcfg_getbyname(index, "AP_CHMODE", tmp);
    athcfg_getbyname(index, "PUREG", tmp2);
    if(atoi(tmp2)==1) {
        *gOnly = true;
    } else {
        *gOnly = false;
    }

    athcfg_getbyname(index, "PUREN", tmp3);
    if(atoi(tmp3)==1) {
        *nOnly = true;
    } else {
        *nOnly = false;
    }

    athcfg_getbyname(index, "PURE11AC", tmp4);
    if(atoi(tmp4)==1) {
        *acOnly = true;
    } else {
        *acOnly = false;
    }

    if(strstr(tmp, "11AC")!=NULL) {
        strcpy(standard, "ac");
    }
    else if(strstr(tmp, "11A")!=NULL) {
        strcpy(standard, "a");
    } 
    else if(strstr(tmp, "11B")!=NULL) {
        strcpy(standard, "b");
    }
    else if(strstr(tmp, "11G")!=NULL) {
        strcpy(standard, "g");
    }
    else 
    {
	strcpy(standard, "n");
    }

    return 0;
}

int wlan_getWepKeyIndex(int index, unsigned int *keyIndex)
{
    char tmp[32];
    int radio_idx;

    check_env();

    radio_idx = athcfg_get_radio(index);

    athcfg_getbyname(radio_idx, "AP_PRIMARY_KEY", tmp);
    *keyIndex = atoi(tmp);

    return 0;
}

int wlan_setWepKeyIndex(int index, unsigned int keyIndex)
{
    char tmp[32];
    int radio_idx;

    check_env();

    radio_idx = athcfg_get_radio(index);

    sprintf(tmp,"%d",keyIndex);
    athcfg_setbyname(radio_idx, "AP_PRIMARY_KEY", tmp);

    return 0;
}

int wlan_pushWepKeyIndex(int index, unsigned int keyIndex)
{
    char tmp[128];
    char buf[1024];
    char key[50];
    int radio_idx;

    radio_idx = athcfg_get_radio(index);

    sprintf(tmp, "iwconfig ath%d enc [%d]",index,keyIndex);
    athcfg_executecmd(__func__,tmp,buf);

    return 0;
}

int wlan_getWepEncryptionLevel(int index, char *encLevel)
{
    check_env();
    strcpy(encLevel, "Disabled,40-bit,104-bit");

    return 0;
}

int wlan_getBasicEncryptionModes(int index, char *encModes)
{
    char tmp[32];

    check_env();
    athcfg_getbyname(index, "AP_SECMODE", tmp);
    if(strstr(tmp, "WEP")!=NULL)
        strcpy(encModes, "WEPEncryption");
    else
        strcpy(encModes, "None");

    return 0; 
}

int wlan_setBasicEncryptionModes(int index, char *encModes)
{
    char tmp[32];

    check_env();
    if(strstr(encModes, "WEPEncryption")!=NULL)
        sprintf(tmp, "%s", "WEP");
    else
        sprintf(tmp, "%s", "None");
    athcfg_setbyname(index, "AP_SECMODE", tmp);

    return 0;
}

int wlan_getBasicAuthenticationModes(int index, char *authMode)
{
    char tmp[32];
    int radio_idx;

    radio_idx = athcfg_get_radio(index);

    check_env();

    athcfg_getbyname(index, "AP_SECMODE", tmp);

    if(strstr(tmp, "None")!=NULL)
        strcpy(authMode, "None");
    else if(strstr(tmp, "WEP")!=NULL) {
        if(!athcfg_getbyname(radio_idx, "AP_WEP_MODE", tmp)) {
            if(atoi(tmp)==2)
                strcpy(authMode, "SharedAuthentication");
            else
                strcpy(authMode, "None");
        }
        else
            strcpy(authMode, "None");
    }
    else {
        athcfg_getbyname(index, "AP_SECFILE", tmp);
        if(strstr(tmp, "EAP")!=NULL)
            strcpy(authMode, "EAPAuthentication");
        else
            strcpy(authMode, "None");
    }
    
    return 0;          
}

int wlan_setBasicAuthenticationModes(int index, char *authMode)
{
    char tmp[32];
    int radio_idx;

    check_env();

    radio_idx = athcfg_get_radio(index);

    if(strstr(authMode, "None")!=NULL) 
    {
        athcfg_getbyname(index, "AP_SECMODE", tmp);
        if(strstr(tmp, "WEP")!=NULL) 
        {
            sprintf(tmp, "%d", 1);
            athcfg_setbyname(radio_idx, "AP_WEP_MODE", tmp);
        }
    }
    else if(strstr(authMode, "SharedAuthentication")!=NULL) 
    {
        sprintf(tmp, "%d", 2);
        athcfg_setbyname(radio_idx, "AP_WEP_MODE", tmp);    
    }
    else if(strstr(authMode, "EAPAuthentication")!=NULL) 
    {
        sprintf(tmp, "%s", "EAP");
        athcfg_setbyname(index, "AP_SECFILE", tmp);
    }

    return 0;
}

int wlan_getWpaEncryptionModes(int index, char *encModes)
{
    char tmp[32];

    check_env();
    athcfg_getbyname(index, "AP_CYPHER", tmp);
    if((strstr(tmp, "TKIP CCMP")!=NULL) || (strstr(tmp, "CCMP TKIP")!=NULL))
        strcpy(encModes, "TKIPandAESEncryption");
    else if(strstr(tmp, "TKIP")!=NULL)
        strcpy(encModes, "TKIPEncryption");
    else if(strstr(tmp, "CCMP")!=NULL)
        strcpy(encModes, "AESEncryption");
    else 
        strcpy(encModes, "");
    return 0;    
}

int wlan_setWpaEncryptionModes(int index, char *encModes)
{
    char tmp[32], tmp2[32];
    int wps_state;

    check_env();
    if(strstr(encModes, "TKIPandAESEncryption")!=NULL) {
        sprintf(tmp, "%s", "TKIP CCMP");
        athcfg_setbyname(index, "AP_CYPHER", tmp);
    }
    else if(strstr(encModes, "TKIPEncryption")!=NULL) {
        sprintf(tmp, "%s", "TKIP");
        athcfg_setbyname(index, "AP_CYPHER", tmp);
    }
    else if(strstr(encModes, "AESEncryption")!=NULL) {
        sprintf(tmp, "%s", "CCMP");
        athcfg_setbyname(index, "AP_CYPHER", tmp);
    } else {
        printf("%s: unknown wpa encryption %s\n",__func__,encModes);
        return -1;
    }
    /* update wps configuration file */
    athcfg_getbyname(index, "WPS_ENABLE", tmp2);
    wps_state = atoi(tmp2);
    if(wps_state) {
	if(strstr(encModes, "TKIPandAESEncryption")!=NULL) {
            // remove quotes from the string
	    sprintf(tmp, "%s", "TKIP CCMP");
	}
        athwps_setbyname(index, "wpa_pairwise", tmp);
    }    
    
    return 0;
}

int wlan_getWpaAuthenticationModes(int index, char *authMode)
{
    char tmp[32];

    check_env();
    athcfg_getbyname(index, "AP_SECFILE", tmp);
    if(strstr(tmp, "PSK")!=NULL)
        strcpy(authMode, "PSKAuthentication");
    else if(strstr(tmp, "EAP")!=NULL)
        strcpy(authMode, "EAPAuthentication");

    return 0;
}

int wlan_WpaBasicAuthenticationModes(int index, char *authMode)
{
    char tmp[32];

    check_env();
    if(strstr(authMode, "PSKAuthentication")!=NULL) {
        sprintf(tmp, "%s", "PSK");
        athcfg_setbyname(index, "AP_SECFILE", tmp);
    }
    else if(strstr(authMode, "EAPAuthentication")!=NULL) {
        sprintf(tmp, "%s", "EAP");
        athcfg_setbyname(index, "AP_SECFILE", tmp);
    }

    return 0;
}

int wlan_getWpaRekeyInterval(int vap,  int *rekeyInterval)
{
    char tmp[32];

    check_env();

    athcfg_getbyname(vap, "AP_WPA_GROUP_REKEY", tmp);
    if (strlen(tmp)) {
        *rekeyInterval = atoi(tmp);
    }
    return 0;
}
int wlan_setWpaRekeyInterval(int vap,  int rekeyInterval)
{
    char tmp[32];

    check_env();

    sprintf(tmp, "%d", rekeyInterval);
    athcfg_setbyname(vap, "AP_WPA_GROUP_REKEY", tmp);

    return 0;
}

int wlan_getPossibleChannels(int index, char *channels)
{
    char *buf;
    char channel_list[32];
    char tmp[64];
    char *pos, *pos2;
    int i;
    int start_channel, end_channel, last_channel;
    int len;
    int ch_no;
    int ch_index;
    int athIndex = -1;
    bool createdAth = false;

    check_env();
    if(index < 0) {
        memset(channels, 0, sizeof(channels));
        return -1;
    }
    
    buf = malloc(1500*sizeof(char));
    if(buf == NULL) {
        printf("%s: allocate memory failed\n",__func__);
        return -1;
    }
    memset(channel_list, 0, sizeof(channel_list));

    if (wlan_getActiveAthIndex(index,&athIndex) == 1) {
        // If there are no active SSIDs on this radio activate the first one temporarily
        // to get the channel list, then destroy
        athIndex = index; 
        createdAth = true;
        wlan_createVap(athIndex,index,"Temp",true);
    }

    sprintf(tmp, "iwlist ath%d freq | grep Channel",athIndex);
    athcfg_executecmd(__func__, tmp, buf);

    pos = buf;
    while((pos=strstr(pos,"Channel "))!=NULL)
    {
        pos +=8;
        if((pos2=strstr(pos," : "))==NULL)
            break;
        memcpy(tmp, pos, (pos2-pos));
        tmp[pos2-pos] = 0;
        ch_no = atoi(tmp);

#ifndef CISCO_INCLUDE_DFS
        if (ch_no > 48 && ch_no < 149) { // skip DFS channels
            continue;
        }
#endif
        ch_index = (int)((ch_no-1)/8);
        channel_list[ch_index] |= (0x1 << ((ch_no-1)%8));
    }

    pos = buf;
    start_channel = end_channel = last_channel = 0;
    for(i=1;i<=200;i++) {
        ch_index = (int)((i-1)/8);
        if((channel_list[ch_index] & (0x1 << ((i-1)%8)))) {
            if(start_channel == 0) {
                start_channel = end_channel = i;
            }
            else if(i != (end_channel+1)) {
                if(start_channel == end_channel) 
                    sprintf(tmp, "%d,",start_channel);
                else
                    sprintf(tmp, "%d-%d,",start_channel,end_channel);
                memcpy(pos, tmp, strlen(tmp));
                pos +=strlen(tmp);
                last_channel = end_channel;
                start_channel = end_channel = i;
            } else {
                end_channel = i;
            }
        }
    }

    if(last_channel == 0) {
        sprintf(tmp, "%d-%d",start_channel,end_channel);
        memcpy(pos, tmp, strlen(tmp));
        pos +=strlen(tmp);  
    }
    else if (last_channel != 0 && last_channel != end_channel) {
        if(start_channel == end_channel) 
            sprintf(tmp, "%d",start_channel);
        else
            sprintf(tmp, "%d-%d",start_channel,end_channel);
        memcpy(pos, tmp, strlen(tmp));
        pos +=strlen(tmp);  
    }   

    if(pos != buf) {
        len = (pos - buf);
        memcpy(channels, buf, len);
        channels[len] = 0;
    }
    free(buf);

    if (createdAth == true) {
        wlan_deleteVap(athIndex);
    }

    return 0;
}

int wlan_getBasicDataTransmitRates(int index, char *txRates)
{

    char buf[512];
    char tmp[64];
    char *pos, *pos2;

    check_env();
    if(index < 0) {
        sprintf(txRates, "%s", "");
        return -1;
    }

    sprintf(tmp, "iwconfig ath%d", index);
    athcfg_executecmd(__func__,tmp,buf);

    if (strstr(buf, "IEEE 802.11b")!= NULL) {
        sprintf(txRates, "%s", "1,2,5.5,11");
    } else if(((strstr(buf, "IEEE 802.11g"))!= NULL) || ((strstr(buf, "IEEE 802.11ng"))!= NULL)) {
    sprintf(txRates, "%s", "1,2,5.5,11,6,9,12,18,24,36,48,54");
    } else {
        /* 11A case */
        sprintf(txRates, "%s", "6,9,12,18,24,36,48,54");
    }

    return 0;

}

int wlan_setBasicDataTransmitRates(int index, char *txRates)
{
    check_env();      
    /* support 1,2,5.5,11,6,9,12,18,24,36,48,54 by default */
    return 0; 
}

int wlan_getOperationaDataTransmitRates(int index, char *txRates)
{
    char buf[512];
    char tmp[64];
    char *pos, *pos2;

    check_env();
    if(index < 0) {
        sprintf(txRates, "%s", "");
        return -1;
    }

    sprintf(tmp, "iwconfig ath%d", index);
    athcfg_executecmd(__func__,tmp,buf);

    if (strstr(buf, "IEEE 802.11b")!= NULL) {
        sprintf(txRates, "%s", "1,2,5.5,11");
    } else if(strstr(buf, "IEEE 802.11g")!= NULL) {
    sprintf(txRates, "%s", "1,2,5.5,11,6,9,12,18,24,36,48,54");
    } else if ((strstr(buf, "IEEE 802.11ng"))!= NULL) {
        sprintf(txRates, "%s", "1,2,5.5,11,6,9,12,18,24,36,48,54,MCS0-MSC23");
    } else if ((strstr(buf, "IEEE 802.11a"))!= NULL) {
        sprintf(txRates, "%s", "6,9,12,18,24,36,48,54");
    } else if ((strstr(buf, "IEEE 802.11na"))!= NULL) {
        sprintf(txRates, "%s", "6,9,12,18,24,36,48,54, MCS0-MCS23");
    }

    return 0;
}

int wlan_setOperationaDataTransmitRates(int index, char *channels)
{
    check_env();
    /* support 1,2,5.5,11,6,9,12,18,24,36,48,54 by default */
    return 0;
}

int wlan_getSsidAdvertisementEnabled(int index, bool *enabled)
{
    char tmp[32];

    check_env();

    if (!athcfg_getbyname(index, "AP_HIDESSID", tmp))
        *enabled = ( atoi(tmp) == 0) ? true: false;
    else
        *enabled = true;
    return 0;
} 

int wlan_setSsidAdvertisementEnabled(int index, bool enabled)
{
    char tmp[32];
    check_env();

    sprintf(tmp, "%d", ! enabled);
    athcfg_setbyname(index, "AP_HIDESSID", tmp);

    return 0;
}

int wlan_pushSsidAdvertisementEnabled(int index, bool enabled)
{
    char tmp[128];
    char buf[1024];

    sprintf(tmp, "iwpriv ath%d hide_ssid %d",index,!enabled);
    athcfg_executecmd(__func__,tmp,buf);

    return 0;
}

int wlan_getRadioActive(int index, bool *active)
{
    int athIndex;

    if (wlan_getActiveAthIndex(index, &athIndex) == 0) { 
        *active = true;
    } else {
        *active = false;
    }

    return 0; 
}

int wlan_getRadioEnabled(int index, bool *enabled)
{
    char tmp[32];

    check_env();
    athcfg_getbyname(index, "RADIO_ENABLE", tmp);
    *enabled = (atoi(tmp) == 0) ? false : true;

    return 0; 
}

int wlan_setRadioEnabled(int index, bool enabled)
{
    char tmp[32];

    check_env();
    sprintf(tmp,"%d",enabled);
    athcfg_setbyname(index, "RADIO_ENABLE", tmp);
    return 0;
}

int wlan_getTransmitPowerSupported(int index, char *power)
{
    check_env();
    /* 8 stpes */
    sprintf(power, "%s", "0,12.5,25,37.5,50,62.5,75,87.5,100");

    return 0;
}

int wlan_getTransmitPower(int index, unsigned int *power)
{
    char buf[512];
    char tmp[64];
    char *pos, *pos2;
    int athIndex;

    check_env();
    if(index < 0) {
        memset(power, 0, sizeof(power));
        return 0;
    }

    // Find the first ath that is up on the given radio
    for (athIndex = index; athIndex < 16; athIndex+=2) {
        bool enabled;
        wlan_getVapEnable(athIndex, &enabled);
       
        if (enabled == true) {
            break;
        }
    }
    if (athIndex >= 16 ) {
        return 1;
    }

    /* the unit is dbm */
    sprintf(tmp, "iwlist ath%d txpower | grep 'Current Tx-Power'", athIndex);
    athcfg_executecmd(__func__,tmp,buf);

    // if we have a Current Tx-Power and it's not off, then get the value
    if ( ((pos=strstr(buf, "Current Tx-Power"))!=NULL) && (strstr(buf,"off") == NULL) ) {
        pos += (strlen("Current Tx-Power") + 1);
        pos2 = strstr(pos, " ");
        memcpy(tmp, pos, (pos2-pos));
        tmp[(pos2-pos)] = 0;
        *power = atoi(tmp);
    } else {
        *power = 0;
    }

    return 0;
}

int wlan_setTransmitPower(int index,  unsigned int power)
{
    char buf[512];
    char cmd[64];
    int athIndex;

    check_env();
    if(index < 0) {
        return 0;
    }

    // Find the first ath that is up on the given radio
    for (athIndex = index; athIndex < 16; athIndex+=2) {
        bool enabled;
        wlan_getVapEnable(athIndex, &enabled);
        
        if (enabled == true) {
            break;
        }
    }
    if (athIndex >= 16 ) {
        return 1;
    }

    /* the unit is dbm */

    /* would cause segmentation fault, fix me */
    sprintf(cmd, "iwconfig ath%d txpower %d", athIndex, power);
    athcfg_executecmd(__func__,cmd,buf);

    return 0; 
}

int wlan_getAutoRateFallbackEnable(int index, bool *enable)
{
    char tmp[32];

    check_env();
    athcfg_getbyname(index, "RATECTL", tmp);
    if(strstr(tmp, "auto")!=NULL)
        *enable = true;
    else
        *enable = false;

    return 0;
}

int wlan_setAutoRateFallbackEnable(int index, bool enable)
{
    char tmp[32];

    check_env();
    if(enable)
        sprintf(tmp, "%s", "auto");
    else
        sprintf(tmp, "%s", "fix");
    athcfg_setbyname(index, "RATECTL", tmp);

    return 0;
}

int wlan_getBasicStats(int index, WLAN_basicStats *stats)
{
    char buf[1500];
    char tmp[64];    
    char *pos, *pos2;    
    unsigned int count;
    unsigned int assoc_cnt = 0;

    check_env();

    memset(stats, 0, sizeof(stats));

    if(index < 0) {
        return -1;
    }

    /* get statistics */
    sprintf(tmp, "ifconfig ath%d", index);
    athcfg_executecmd(__func__,tmp,buf);

    pos = buf;
    if((pos=strstr(pos,"RX packets:"))==NULL){
        printf("%s: get rx packets statistics failed\n",__func__);
        return -1;
    }
    pos +=strlen("RX packets:");
    pos2 = strstr(pos," ");
    memcpy(tmp, pos, (pos2-pos));
    tmp[pos2-pos] = 0;
    count = atoi(tmp);
    stats->Wlan_totalPacketsReceived = count;
    
    if((pos=strstr(pos,"TX packets:"))==NULL){
        printf("%s: get tx packets statistics failed\n",__func__);
        return -1;
    }
    pos +=strlen("TX packets:");
    pos2 = strstr(pos," ");
    memcpy(tmp, pos, (pos2-pos));
    tmp[pos2-pos] = 0;
    count = atoi(tmp);
    stats->Wlan_totalPacketsSent = count;

    if((pos=strstr(pos,"RX bytes:"))==NULL){
        printf("%s: get rx bytes statistics failed\n",__func__);
        return -1;
    }
    pos +=strlen("RX bytes:");
    pos2 = strstr(pos," ");
    memcpy(tmp, pos, (pos2-pos));
    tmp[pos2-pos] = 0;
    count = atoi(tmp);
    stats->Wlan_totalBytesReceived = count;

    if((pos=strstr(pos,"TX bytes:"))==NULL){
        printf("%s: get tx bytes statistics failed\n",__func__);
        return -1;
    }
    pos +=strlen("TX bytes:");
    pos2 = strstr(pos," ");
    memcpy(tmp, pos, (pos2-pos));
    tmp[pos2-pos] = 0;
    count = atoi(tmp);
    stats->Wlan_totalBytesSent = count;

    /* get assoc number */  
    wlan_getAssocDevicesNum(index, &assoc_cnt);
    stats->Wlan_totalAssociations = assoc_cnt;  

    return 0;
}

int wlan_getStats(int index, WLAN_stats *stats)
{
    char buf[1500];
    char tmp[64];    
    char *pos_begin, *pos, *pos2;
    unsigned int count;

    check_env();

    memset(stats, 0, sizeof(stats));

    if(index < 0) {
        return -1;
    }

    /* get statistics */
    sprintf(tmp, "apstats -v -i ath%d", index);
    athcfg_executecmd(__func__,tmp,buf);

	pos_begin = buf;

    pos = pos_begin;
    if((pos=strstr(pos,"Tx failures                     ="))==NULL){
        printf("%s: get Wlan_ErrorSent failed\n",__func__);
        // return -1;
    } else {
        pos +=strlen("Tx errors                       =");
        pos2 = strstr(pos,"\n");
        memcpy(tmp, pos, (pos2-pos));
        tmp[pos2-pos] = 0;
        count = atoi(tmp);
        stats->Wlan_ErrorsSent = count;
    }
	
	pos = pos_begin;
    if((pos=strstr(pos,"Rx errors                       ="))==NULL){
        printf("%s: get Wlan_ErrorReceived failed\n",__func__);
        // return -1;
    } else {
        pos +=strlen("Rx errors                       =");
        pos2 = strstr(pos,"\n");
        memcpy(tmp, pos, (pos2-pos));
        tmp[pos2-pos] = 0;
        count = atoi(tmp);
        stats->Wlan_ErrorReceived = count;
    }


	pos = pos_begin;
    if((pos=strstr(pos,"Tx Unicast Data Packets         ="))==NULL){
        printf("%s: get wlan_UnicastPacketsSent failed\n",__func__);
        // return -1;
    } else {
        pos +=strlen("Tx Unicast Data Packets         =");
        pos2 = strstr(pos,"\n");
        memcpy(tmp, pos, (pos2-pos));
        tmp[pos2-pos] = 0;
        count = atoi(tmp);
        stats->Wlan_UnicastPacketsSent = count;
    }

	pos = pos_begin;
    if((pos=strstr(pos,"Rx Unicast Data Packets         ="))==NULL){
        printf("%s: get wlan_UnicastPacketsReceived failed\n",__func__);
        // return -1;
    } else {
        pos +=strlen("Rx Unicast Data Packets         =");
        pos2 = strstr(pos,"\n");
        memcpy(tmp, pos, (pos2-pos));
        tmp[pos2-pos] = 0;
        count = atoi(tmp);
        stats->Wlan_UnicastPacketsReceived= count;
    }


	pos = pos_begin;
    if((pos=strstr(pos,"Tx Dropped                      ="))==NULL){
        printf("%s: get Wlan_DiscardPacketsSent failed\n",__func__);
        // return -1;
    } else {
        pos +=strlen("Tx Dropped                      =");
        pos2 = strstr(pos,"\n");
        memcpy(tmp, pos, (pos2-pos));
        tmp[pos2-pos] = 0;
        count = atoi(tmp);
        stats->Wlan_DiscardPacketsSent= count;
    }


	pos = pos_begin;
    if((pos=strstr(pos,"Rx Dropped                      ="))==NULL){
        printf("%s: get Wlan_DiscardPacketsReceived failed\n",__func__);
        // return -1;
    } else {
        pos +=strlen("Rx Dropped                      =");
        pos2 = strstr(pos,"\n");
        memcpy(tmp, pos, (pos2-pos));
        tmp[pos2-pos] = 0;
        count = atoi(tmp);
        stats->Wlan_DiscardPacketsReceived= count;
    }


	pos = pos_begin;
    if((pos=strstr(pos,"Tx Multi/Broadcast Data Packets ="))==NULL){
        printf("%s: get Wlan_MulticastPacketsSent failed\n",__func__);
        // return -1;
    } else {
        pos +=strlen("Tx Multi/Broadcast Data Packets =");
        pos2 = strstr(pos,"\n");
        memcpy(tmp, pos, (pos2-pos));
        tmp[pos2-pos] = 0;
        count = atoi(tmp);
        stats->Wlan_MulticastPacketsSent= count;
    }


	pos = pos_begin;
    if((pos=strstr(pos,"Rx Multi/Broadcast Data Packets ="))==NULL){
        printf("%s: get Wlan_MulticastPacketsReceived failed\n",__func__);
        // return -1;
    } else {
        pos +=strlen("Rx Multi/Broadcast Data Packets =");
        pos2 = strstr(pos,"\n");
        memcpy(tmp, pos, (pos2-pos));
        tmp[pos2-pos] = 0;
        count = atoi(tmp);
        stats->Wlan_MulticastPacketsReceived= count;
    }


	pos = pos_begin;
    if((pos=strstr(pos,"Tx Broadcast Data Packets       ="))==NULL){
        printf("%s: get Wlan_BroadcastPacketsSent failed\n",__func__);
        // return -1;
    } else {
        pos +=strlen("Tx Broadcast Data Packets       =");
        pos2 = strstr(pos,"\n");
        memcpy(tmp, pos, (pos2-pos));
        tmp[pos2-pos] = 0;
        count = atoi(tmp);
        stats->Wlan_BroadcastPacketsSent= count;
    }


	pos = pos_begin;
    if((pos=strstr(pos,"Rx Broadcast Data Packets       ="))==NULL){
        printf("%s: get Wlan_BroadcastPacketsReceived failed\n",__func__);
        return -1;
    } else {
        pos +=strlen("Rx Broadcast Data Packets       =");
        pos2 = strstr(pos,"\n");
        memcpy(tmp, pos, (pos2-pos));
        tmp[pos2-pos] = 0;
        count = atoi(tmp);
        stats->Wlan_BroadcastPacketsReceived= count;
    }

	stats->Wlan_UnknownProtPacketsReceived=0;

    return 0;

}

int wlan_getAssocDevicesNum(int index, unsigned int *devNum)
{
    char buf[1500];
    char tmp[128];
    unsigned int assoc_cnt = 0;
    char *pos;

    check_env();
    if(index < 0) {
        *devNum = 0;
        return -1;
    }
    
    // CISCO CHANGE to fix buffer overflow issue
    sprintf(tmp, "wlanconfig ath%d list sta 2>/dev/null | grep -v HTCAPS -c", index);
    athcfg_executecmd(__func__,tmp,buf);

    if (strlen(buf) > 0) {
        *devNum = atoi(buf);
    }

    #if 0
    pos = buf;
    if((pos=strstr(pos,"HTCAPS"))==NULL){
        *devNum = 0;
    } else {
        pos +=7; /*start from next line*/
        while((pos=strstr(pos,"\n"))!=NULL){
            pos +=1;
            assoc_cnt++;
        }
        *devNum = assoc_cnt;
    }
    #endif

    return 0;
}

int wlan_getAssocDevice(int index, int devIndex, WLAN_Device *dev)
{

    char buf[1500];
    char tmp[128];
    char *pos;

    check_env();
    if (index < 0) {
        memset(dev, 0, sizeof(dev));
        return -1;
    }

    sprintf(tmp, "wlanconfig ath%d list sta 2>/dev/null | grep -v HTCAP | grep -v HTCAP -n | grep \"^%d:\"", index, devIndex);
    athcfg_executecmd(__func__,tmp,buf);

    pos = buf;
    if (pos == NULL) {
        memset(dev->Wlan_devMacAddress, 0, 6);
        memset(dev->Wlan_devIPAddress, 0, 64);
        dev->Wlan_devAssociatedDeviceAuthenticationState = 0;
    } else {
        if ((pos=strstr(pos,":")) != NULL) {
            pos++; 

            if (strlen(pos) >= MAC_ADDR_SIZE-1) {
                int i;
                for (i=0;i<6;i++) {
                    memcpy(tmp, pos, 2); 
                    tmp[2] = 0; 
                    dev->Wlan_devMacAddress[i] = strtol(tmp, (char**)NULL, 0x10 /*in Hex*/); 
                    pos+=3;  
                } // for

                memset(dev->Wlan_devIPAddress, 0, 64);
                dev->Wlan_devAssociatedDeviceAuthenticationState = 1;
            }
        }
    }

    return 0;
}

int wlan_getAllAssocDevices(int index, int *numDevices, WLAN_Device ***wlanDevices)
{

    char buf[1500];
    char cmd[150];
    char tmp[10];
    unsigned int assoc_cnt = 0;
    char *pos;
    int i;
    FILE *f;
    WLAN_Device **devices = NULL;
    bool first = true;

    *numDevices = 0;
    *wlanDevices = NULL;

    check_env();

    if (index < 0) {
        return -1;
    }

    sprintf(cmd,  "wlanconfig ath%d list sta  2>/dev/null | grep -v HTCAP >/tmp/ath%dDevices.txt; cat /tmp/ath%dDevices.txt | wc -l" , index, index, index);
    athcfg_executecmd(__func__,cmd,buf);

    // Get count and alloc memory for wlanDevices
    *numDevices = atoi(buf);

    if (*numDevices > 0) {
        int i = 0;
        devices = (WLAN_Device *) calloc (*numDevices, sizeof(WLAN_Device *));
        *wlanDevices = devices;
        for (i = 0; i < *numDevices; i++) {
            devices[i] = (WLAN_Device *) calloc (1, sizeof(WLAN_Device));
        }
    } else {
        // There are no devices, there is nothing left to do
        *wlanDevices = NULL;
        return 0;
    }

    sprintf(cmd, "cat /tmp/ath%dDevices.txt" , index);
    if ((f = popen(cmd, "r")) == NULL) {
        printf("%s: popen %s error\n",__func__, cmd);
        return -1;
    }

    while (!feof(f)) {
        pos = buf;
        *pos = 0;
        fgets(pos,200,f);

        if (strlen(pos) == 0) {
            break;
        }
        // printf("%s: buf \n%s \n", __func__, pos);

        if (assoc_cnt >= *numDevices) {
            break;
        }

        { 
            char *mac=strtok(pos," ");
            char *aid = strtok('\0'," ");
            char *chan = strtok('\0'," ");
            char *txrate = strtok('\0'," ");
            char *rxrate = strtok('\0'," ");
            char *rssi = strtok('\0'," ");

            // Should be Mac Address line
            if (mac) { 
                sscanf(mac, "%x:%x:%x:%x:%x:%x",
                       (unsigned int *)&devices[assoc_cnt]->Wlan_devMacAddress[0], 
                       (unsigned int *)&devices[assoc_cnt]->Wlan_devMacAddress[1], 
                       (unsigned int *)&devices[assoc_cnt]->Wlan_devMacAddress[2], 
                       (unsigned int *)&devices[assoc_cnt]->Wlan_devMacAddress[3], 
                       (unsigned int *)&devices[assoc_cnt]->Wlan_devMacAddress[4], 
                       (unsigned int *)&devices[assoc_cnt]->Wlan_devMacAddress[5] );
            }

            memset(devices[assoc_cnt]->Wlan_devIPAddress, 0, 64);
            devices[assoc_cnt]->Wlan_devAssociatedDeviceAuthenticationState = 1;

            devices[assoc_cnt]->Wlan_devSignalStrength =  (rssi != NULL) ? atoi(rssi) - 100 : 0;
            devices[assoc_cnt]->Wlan_devTxRate =  (txrate != NULL) ? atoi(strtok(txrate,"M")) : 0; 
            devices[assoc_cnt]->Wlan_devRxRate =  (rxrate != NULL) ? atoi(strtok(rxrate,"M")) : 0;
            
            assoc_cnt++;      
        }
    }
    pclose(f);

    return 0;
}

int wlan_kickAssocDevice(int index, WLAN_Device *dev)
{

    char buf[1500];
    char tmp[64];
    unsigned int assoc_cnt = 0;
    char *pos;
    int i;

    check_env();
    if(index < 0) {
        return -1;
    }

    sprintf(tmp, "iwpriv ath%d kickmac %02x:%02x:%02x:%02x:%02x:%02x", index,
            dev->Wlan_devMacAddress[0],
            dev->Wlan_devMacAddress[1],
            dev->Wlan_devMacAddress[2],
            dev->Wlan_devMacAddress[3],
            dev->Wlan_devMacAddress[4],
            dev->Wlan_devMacAddress[5]);
    printf("%s: %s\n", __func__, tmp);
    athcfg_executecmd(__func__,tmp,buf);

    return 0;
}

char* strupper( char* s )
{
  char *p;
  for (p = s; *p; ++p)
    *p = toupper( *p );
  return s;
}

char* strlower( char* s )
{
  char *p;
  for (p = s; *p; ++p)
    *p = tolower( *p );
  return s;
}

int wlan_kickAclAssocDevices(int index, bool denyList)
{

    char buf[1500];
    char staList[1500];
    char aclList[1500];
    char aclDevice[20];
    char tmp[128];
    unsigned int assoc_cnt = 0;
    unsigned int numAclDevices = 0;
    char *pos;
    int i;

    check_env();
    if(index < 0) {
        return -1;
    }


    wlan_getAclDeviceNum(index, &numAclDevices);
    printf("%s: wlan_getAclDeviceNum %d -> %d\n",__func__, index, numAclDevices );
    wlan_getAssocDevicesNum(index, &assoc_cnt);
    printf("%s: wlan_getAssocDevicesNum %d -> %d\n",__func__, index, assoc_cnt );

    // if there are no devices connected there is nothing to do
    if (assoc_cnt == 0) {
        return 0;
    }

    sprintf(tmp, "wlanconfig ath%d list sta 2>/dev/null | grep -v HTCAP | cut -f1 -d' '", index);
    printf("%s: %s\n", __func__, tmp);
    athcfg_executecmd(__func__,tmp,staList);
    printf("%s: \n %s\n", __func__, staList);

    sprintf(tmp, "iwpriv ath%d getmac", index);
    printf("%s: %s\n", __func__, tmp);
    athcfg_executecmd(__func__,tmp,aclList);
    strlower(aclList);            
    printf("%s: \n %s\n", __func__, aclList);

    if (  denyList == true ) {

        for (i = 0; i < numAclDevices; i++) {
            wlan_getAclDevice(index, i, aclDevice);
            strlower(aclDevice);            

            if ( strstr(staList,aclDevice) ) {
                sprintf(tmp, "iwpriv ath%d kickmac %s", index, aclDevice);
                printf("%s:  %s\n", __func__, tmp );
                athcfg_executecmd(__func__,tmp,buf);
            }
        }
    } else {
        char devMac[20];

        pos = staList;

        for (i = 0; i < assoc_cnt && pos!=NULL; i++) {

            memcpy(devMac, pos, 17);
            devMac[17] = '\0';

            // if there are no devices on the allow list or
            // the associated device is NOT in the allow list 
            // kick it off
            if ( (numAclDevices == 0) || (strstr(aclList,devMac) == NULL) ) {
                sprintf(tmp, "iwpriv ath%d kickmac %s", index, devMac);
                printf("%s:  %s\n", __func__, tmp );
                athcfg_executecmd(__func__,tmp,buf);
            }

            // Advance to the next line
            pos=strstr(pos,"\n");
            if (pos != NULL) pos ++;
        }
    }

    return 0;
}

/* Only support one Radio ?? */
int wlan_getWepKey(int index, int key_idx, char *key)
{
    char tmp[256];
    int radio_idx;
    char par[32];

    check_env();

    radio_idx = athcfg_get_radio(index);

    sprintf(par, "WEP_RADIO_NUM%d_KEY_%d", radio_idx, key_idx);

    if(!athcfg_getbyname(1, par, tmp))
    {
        // remove newline so that it's not copied as part of the key
        char *newline = strstr(tmp,"\n");
        if (newline) *newline = 0;

        strcpy(key, tmp);
    } else
        memset(key, 0, sizeof(key));

    return 0;
}

int wlan_setWepKey(int index, int key_idx, char *key)
{
    char tmp[32];
    char par[32];
    int radio_idx;

    check_env();

    radio_idx = athcfg_get_radio(index);

    /* From current design, VAPs on same radio have the same tx key index */
    sprintf(par, "WEP_RADIO_NUM%d_KEY_%d", /* radio index */ radio_idx, key_idx);
    athcfg_setbyname(radio_idx, par, key);

    /* WEP Key in HEX */
    sprintf(par, "WEP_RADIO_NUM%d_IS_HEX_%d", /* radio index */ radio_idx, key_idx);

    if (strlen(key) == 10 || strlen(key) == 26 ) {
	athcfg_setbyname(radio_idx, par, "1");
    } else {
	athcfg_setbyname(radio_idx, par, "0");
    }

    return 0;
}

int wlan_pushWepKey(int index, int key_idx)
{
    char tmp[128];
    char buf[1024];
    char key[200];
    int radio_idx;

    check_env();

    if (wlan_getWepKey(index,key_idx,key) != 0)
        return -1;

    radio_idx = athcfg_get_radio(index);

    sprintf(tmp, "cfg -h %s 1; echo $?",key);
    athcfg_executecmd(__func__,tmp,buf);
    bool hex = atoi(buf);

    // Always set key as index 1, to compensate for Qualcomm bug with other keys.
    sprintf(tmp, "iwconfig ath%d enc [1] %s%s",index, (hex==false) ? "s:" : "" ,key);
    athcfg_executecmd(__func__, tmp, buf);

    return 0;
}

int wlan_getPreSharedKey(int index, char *psk)
{
    char tmp[66];

    check_env();
    if (!athcfg_getbyname(index, "PSK_KEY", tmp))
    {
        // remove newline so that it's not copied as part of the key
        char *newline = strstr(tmp,"\n");
        if (newline) *newline = 0;
        if (strlen(tmp) == 64) {
            strcpy(psk, tmp);
        } else {
            strcpy(psk, ""); /* Key not PreSharedKey, but passphrase */
        }

    } else
        strcpy(psk, ""); /* No Key found */
        
    return 0;
}

int wlan_setPreSharedKey(int index, char *psk)
{
    char tmp[65];
    int wps_state;    
    int pskLen = strlen(psk);

    check_env();
    athcfg_setbyname(index, "PSK_KEY", psk);
    /* update wps configuration file */
    athcfg_getbyname(index, "WPS_ENABLE", tmp);

    wps_state = atoi(tmp);
    if(wps_state) {
	athwps_delbyname(index, "wpa_passphrase");
	athwps_setbyname(index, "wpa_psk", psk);
    }

    return 0;
}

int wlan_getKeyPassphrase(int index, char *psk)
{
    char tmp[66];

    check_env();
    if (!athcfg_getbyname(index, "PSK_KEY", tmp))
    {
        // remove newline so that it's not copied as part of the key
        char *newline = strstr(tmp,"\n");
        if (newline) *newline = 0;

        if (strlen(tmp) < 64) {
            strcpy(psk, tmp);
        } else {
            strcpy(psk, ""); /* Key not passphrase, but PreSharedKey */
        }
    } else
        strcpy(psk,""); /* No key found */

    return 0;
}

int wlan_setKeyPassphrase(int index, char *psk)
{
    char tmp[65];
    int wps_state;
    int pskLen = strlen(psk);

    check_env();
    athcfg_setbyname(index, "PSK_KEY", psk);
    /* update wps configuration file */
    athcfg_getbyname(index, "WPS_ENABLE", tmp);

    wps_state = atoi(tmp);
    if(wps_state) {
	athwps_delbyname(index, "wpa_psk");
	athwps_setbyname(index, "wpa_passphrase", psk);
    }

    return 0;    
}

int wlan_getWmmSupported(int index, bool *supported)
{
    char buf[128];
    char tmp[64];
    char *pos;

    check_env();

    if(index < 0)  
        return -1;

    sprintf(tmp, "iwpriv ath%d get_wmm",index);

    athcfg_executecmd(__func__,tmp,buf);

    if((pos=strstr(buf, "get_wmm:"))!=NULL) {
        pos +=strlen("get_wmm:");
        *supported = (atoi(pos) == 0) ? false : true;
    }

    return 0;
}

int wlan_getWmmUapsdSupported(int index, bool *supported)
{
    char buf[128];
    char tmp[64];
    char *pos;

    check_env();

    if(index < 0)  
        return -1;

    sprintf(tmp, "iwpriv ath%d get_uapsd",index);

    athcfg_executecmd(__func__,tmp,buf);

    if((pos=strstr(buf, "get_uapsd:"))!=NULL) {
        pos +=strlen("get_uapsd:");
        *supported = (atoi(pos) == 0) ? false : true;
    }

    return 0;
}

int wlan_setWmmEnable(int index, bool enable)
{
    char buf[64];
    char cmd[64];
    
    check_env();
    if(index < 0)
        return 0;
    
    sprintf(cmd, "iwpriv ath%d wmm %d",index, enable); 

    return athcfg_executecmd(__func__,cmd,buf);
}

int wlan_setWmmUapsdEnable(int index, bool enable)
{
    char buf[64];
    char cmd[64];

    check_env();
    if(index < 0)
        return 0;
    
    sprintf(cmd, "iwpriv ath%d uapsd %d",index, enable);

    return athcfg_executecmd(__func__,cmd,buf);
}

int wlan_getWmmOgAifsn(int index, int class, unsigned int *aifsn)
{
    char buf[128];
    char tmp[64];
    char *pos;
    int isbss = 0;

    check_env();
    if(index < 0)  
        return -1;

    sprintf(tmp, "iwpriv ath%d get_aifs %d %d",index,class,isbss);

    athcfg_executecmd(__func__,tmp,buf);
    if((pos=strstr(buf, "get_aifs:"))!=NULL) {
        pos +=strlen("get_aifs:");
        *aifsn = atoi(pos);
    }
    return 0;
}

int wlan_setWmmOgAifsn(int index, int class, unsigned int aifsn)
{
    char buf[128];
    char cmd[64];
    int isbss = 0;

    check_env();
    if(index < 0)
        return 0;

    sprintf(cmd, "iwpriv ath%d aifs %d %d %d",index,class,isbss,aifsn);

    athcfg_executecmd(__func__,cmd,buf);

    return 0;
}

int wlan_getWmmOgEcwMin(int index, int class, unsigned int *ecwMin)
{
    char buf[128];
    char cmd[64];
    char *pos;
    int isbss = 0;

    check_env();
    if(index < 0)
        return 0;

    sprintf(cmd, "iwpriv ath%d get_cwmin %d %d",index,class,isbss);

    athcfg_executecmd(__func__,cmd,buf);
    if((pos=strstr(buf, "get_cwmin:"))!=NULL) {
        pos +=strlen("get_cwmin:");
        *ecwMin = atoi(pos);
    }
    return 0;      
} 

int wlan_setWmmOgEcwMin(int index, int class, unsigned int ecwMin)
{
    char buf[128];
    char cmd[64];
    int isbss = 0;

    check_env();
    if(index < 0)
        return 0;

    sprintf(cmd, "iwpriv ath%d cwmin %d %d %d",index,class,isbss,ecwMin);

    athcfg_executecmd(__func__,cmd,buf);

    return 0;
}

int wlan_getWmmOgEcwMax(int index, int class, unsigned int *ecwMax)
{
    char buf[128];
    char cmd[64];
    char *pos;
    int isbss = 0;

    check_env();
    if(index < 0)
        return 0;

    sprintf(cmd, "iwpriv ath%d get_cwmax %d %d",index,class,isbss);

    athcfg_executecmd(__func__,cmd,buf);
    if((pos=strstr(buf, "get_cwmax:"))!=NULL) {
        pos +=strlen("get_cwmax:");
        *ecwMax  = atoi(pos);
    }
    return 0;
}

int wlan_setWmmOgEcwMax(int index, int class, unsigned int ecwMax)
{
    char buf[128];
    char cmd[64];
    int isbss = 0;

    check_env();
    if(index < 0)
        return 0;

    sprintf(cmd, "iwpriv ath%d cwmax %d %d %d",index,class,isbss,ecwMax);

    athcfg_executecmd(__func__,cmd,buf);

    return 0;
}

int wlan_getWmmOgTxOp(int index, int class, unsigned int *txOp)
{
    char buf[128];
    char cmd[64];
    char *pos;
    int isbss = 0;

    check_env();
    if(index < 0)
        return 0;

    sprintf(cmd, "iwpriv ath%d get_txoplimit %d %d",index,class,isbss);

    athcfg_executecmd(__func__,cmd,buf);
    if((pos=strstr(buf, "get_txoplimit:"))!=NULL) {
        pos +=strlen("get_txoplimit:");
        *txOp  = atoi(pos) >> 5;
    }
    return 0;
}

int wlan_setWmmOgTxOp(int index, int class, unsigned int txOp)
{
    char buf[128];
    char cmd[64];
    int isbss = 0;

    check_env();
    if(index < 0)
        return 0;

    txOp = txOp << 5;
    sprintf(cmd, "iwpriv ath%d txoplimit %d %d %d",index,class,isbss,txOp);

    athcfg_executecmd(__func__,cmd,buf);

    return 0;
}

int wlan_getWmmOgAckPolicy(int index, int class, unsigned int *policy)
{
    char buf[128];
    char cmd[64];
    char *pos;
    int isbss = 0;

    check_env();
    if(index < 0)
        return 0;

    sprintf(cmd, "iwpriv ath%d get_noackpolicy %d %d",index,class,isbss);

    athcfg_executecmd(__func__,cmd,buf);
    if((pos=strstr(buf, "get_noackpolicy:"))!=NULL) {
        pos +=strlen("get_noackpolicy:");
        *policy  = !(atoi(pos));
    }
    return 0;
}

int wlan_setWmmOgAckPolicy(int index, int class, unsigned int policy)
{
    char buf[128];
    char cmd[64];
    int isbss = 0;

    check_env();
    if(index < 0)
        return 0;

    sprintf(cmd, "iwpriv ath%d noackpolicy %d %d %d",index,class,isbss,!policy);

    athcfg_executecmd(__func__,cmd,buf);

    return 0;
}

int wlan_getWmmIcAifsn(int index, int class, unsigned int *aifsn)
{
    char buf[128];
    char tmp[64];
    char *pos;
    int isbss = 1;

    check_env();
    if(index < 0)
        return 0;

    sprintf(tmp, "iwpriv ath%d get_aifs %d %d",index,class,isbss);

    athcfg_executecmd(__func__,tmp,buf);
    if((pos=strstr(buf, "get_aifs:"))!=NULL) {
        pos +=strlen("get_aifs:");
        *aifsn = atoi(pos);
    }
    return 0;
}

int wlan_setWmmIcAifsn(int index, int class, unsigned int aifsn)
{
    char buf[128];
    char cmd[64];
    int isbss = 1;

    check_env();
    if(index < 0)
        return 0;

    sprintf(cmd, "iwpriv ath%d aifs %d %d %d",index,class,isbss,aifsn);

    athcfg_executecmd(__func__,cmd,buf);

    return 0;
}

int wlan_getWmmIcEcwMin(int index, int class, unsigned int *ecwMin)
{
    char buf[128];
    char cmd[64];
    char *pos;
    int isbss = 1;

    check_env();
    if(index < 0)
        return 0;

    sprintf(cmd, "iwpriv ath%d get_cwmin %d %d",index,class,isbss);

    athcfg_executecmd(__func__,cmd,buf);
    if((pos=strstr(buf, "get_cwmin:"))!=NULL) {
        pos +=strlen("get_cwmin:");
        *ecwMin = atoi(pos);
    }
    return 0;
}

int wlan_setWmmIcEcwMin(int index, int class, unsigned int ecwMin)
{
    char buf[128];
    char cmd[64];
    int isbss = 1;

    check_env();
    if(index < 0)
        return 0;

    sprintf(cmd, "iwpriv ath%d cwmin %d %d %d",index,class,isbss,ecwMin);

    athcfg_executecmd(__func__,cmd,buf);

    return 0;
}

int wlan_getWmmIcEcwMax(int index, int class, unsigned int *ecwMax)
{
    char buf[128];
    char cmd[64];
    char *pos;
    int isbss = 1;

    check_env();
    if(index < 0)
        return 0;

    sprintf(cmd, "iwpriv ath%d get_cwmax %d %d",index,class,isbss);

    athcfg_executecmd(__func__,cmd,buf);
    if((pos=strstr(buf, "get_cwmax:"))!=NULL) {
        pos +=strlen("get_cwmax:");
        *ecwMax  = atoi(pos);
    }
    return 0;       
}

int wlan_setWmmIcEcwMax(int index, int class, unsigned int ecwMax)
{
    char buf[128];
    char cmd[64];
    int isbss = 1;

    check_env();
    if(index < 0)
        return 0;

    sprintf(cmd, "iwpriv ath%d cwmax %d %d %d",index,class,isbss,ecwMax);

    athcfg_executecmd(__func__,cmd,buf);

    return 0;
}

int wlan_getWmmIcTxOp(int index, int class, unsigned int *txOp)
{
    char buf[128];
    char cmd[64];
    char *pos;
    int isbss = 1;

    check_env();
    if(index < 0)
        return 0;

    sprintf(cmd, "iwpriv ath%d get_txoplimit %d %d",index,class,isbss);

    athcfg_executecmd(__func__,cmd,buf);
    if((pos=strstr(buf, "get_txoplimit:"))!=NULL) {
        pos +=strlen("get_txoplimit:");
        *txOp  = atoi(pos) >> 5;
    }
    return 0;
}

int wlan_setWmmIcTxOp(int index, int class, unsigned int txOp)
{
    char buf[128];
    char cmd[64];
    int isbss = 1;

    check_env();
    if(index < 0)
        return 0;

    txOp = txOp << 5;
    sprintf(cmd, "iwpriv ath%d txoplimit %d %d %d",index,class,isbss,txOp);

    athcfg_executecmd(__func__,cmd,buf);

    return 0;
}

int wlan_getWmmIcAckPolicy(int index, int class, unsigned int *policy)
{
    char buf[128];
    char cmd[64];
    char *pos;
    int isbss = 1;

    check_env();
    if(index < 0)
        return 0;

    sprintf(cmd, "iwpriv ath%d get_noackpolicy %d %d",index,class,isbss);

    athcfg_executecmd(__func__,cmd,buf);
    if((pos=strstr(buf, "get_noackpolicy:"))!=NULL) {
        pos +=strlen("get_noackpolicy:");
        *policy  = !(atoi(pos));
    }
    return 0;
}

int wlan_setWmmIcAckPolicy(int index, int class, unsigned int policy)
{
    char buf[128];
    char cmd[64];
    int isbss = 1;

    check_env();
    if(index < 0)
        return 0;

    sprintf(cmd, "iwpriv ath%d noackpolicy %d %d %d",index,class,isbss,!policy);

    athcfg_executecmd(__func__,cmd,buf);

    return 0;
}

int wlan_getWpsEnable(int index, int *enable)
{
    char tmp[64];

    check_env();
    athcfg_getbyname(index, "WPS_ENABLE", tmp);

    /* WPS_ENABLE = 0/1/2 => DISABLE/UN-CONFIGURE/CONFIGURE */
    if(atoi(tmp) > 0)
        *enable = atoi(tmp);
    else
        *enable = 0;

    return 0; 
}

int wlan_setWpsEnable(int index, int enable)
{
    char par[32];
    char buf[256];

    check_env();
    sprintf(par, "%d", enable);

    athcfg_setbyname(index, "WPS_ENABLE", par);

    return 0;
}

int wlan_getWpsDeviceName(int index, char *devName)
{
    check_env();
    if(athwps_getbyname(index, "friendly_name", devName)<0) {
        memset(devName, 0, sizeof(devName));
        return -1;
    }
    return 0;
}

int wlan_getWpsDevicePassword(int index, unsigned int *password)
{
    char tmp[64];

    check_env();
    
    int ret = athwps_getbyname(index, "ap_pin", tmp);
    if (ret != 0) {
        athcfg_getbyname(index, "WSC_PIN", tmp);
    }
    *password = atoi(tmp);

    return 0;
}

int wlan_setWpsDevicePassword(int index, unsigned int password)
{
    char tmp[64];
    
    check_env();
    sprintf(tmp, "%d", password);
    athcfg_setbyname(index, "WSC_PIN", tmp);
    return athwps_setbyname(index, "ap_pin", tmp);
}

int wlan_getWpsUuid(int index, char *uuid)
{
    check_env();
    if(athwps_getbyname(index, "uuid", uuid)<0) {
        memset(uuid, 0, sizeof(uuid));
        return -1;
    }
    return 0;
}

int wlan_getWpsVersion(int index, unsigned int *ver)
{
    check_env();

    /* WPS 2.0  support */
    *ver = 2;

    return 0;
}

int wlan_getWpsConfigMethodsSupported(int index, char *method)
{
    check_env();
    /* support label display push_button keypad */ 
    sprintf(method, "%s", "Label,Display,PushButton,Keypad");

    return 0;
}

int wlan_getWpsConfigMethodsEnabled(int index, char *method)
{
    char buf[256];

    check_env();
    memset(method, 0, sizeof(method));
    if(athwps_getbyname(index, "config_methods", buf)<0) {
        // There is no security so return the default
        wlan_getWpsConfigMethodsSupported(index,method);
        return 0;
    }
    if(strstr(buf, "label")!=NULL)
        strcat(method, "Label,");
    if(strstr(buf, "display")!=NULL)
        strcat(method, "Display,");
    if(strstr(buf, "push_button")!=NULL)
        strcat(method, "PushButton,");
    if(strstr(buf, "keypad")!=NULL)
        strcat(method, "Keypad,");

    if(strlen(method))
        method[strlen(method)-1] = 0;

    return 0;
}

int wlan_setWpsConfigMethodsEnabled(int index, char *method)
{
    char tmp[256];

    check_env();
    memset(tmp, 0, sizeof(tmp));
    if(strstr(method, "Label")!=NULL)
        strcat(tmp, "label ");
    if(strstr(method, "Display")!=NULL)
        strcat(tmp, "display ");
    if(strstr(method, "PushButton")!=NULL)
        strcat(tmp, "push_button ");
    if(strstr(method, "Keypad")!=NULL)
        strcat(tmp, "keypad ");
    
    if(strlen(tmp)) {
        tmp[strlen(tmp)-1] =0;
        return athwps_setbyname(index, "config_methods", tmp);    
    }
    
    return 0;
}

int wlan_getWpsSetupLockedState(int index, char *state)
{
    char buf[256];
    
    check_env();
    if(athwps_getbyname(index, "ap_setup_locked", buf)<0) {
        memset(state, 0, sizeof(state));
        return -1;
    }
    if(strlen(buf)) {
        if(atoi(buf) > 0)
            sprintf(state, "%s", "LockedByLocalManagement");
        else
            sprintf(state, "%s", "Unlocked");    
    } else { 
        sprintf(state, "%s", "Unlocked");
    }

    return 0;
}

int wlan_getWpsSetupLock(int index, bool *lock)
{
    char buf[256];

    check_env();
    if(athwps_getbyname(index, "ap_setup_locked", buf) < 0) {
        *lock = 0;
        return -1;
    }
    if(strlen(buf)) {
        if(atoi(buf) > 0)
            *lock = true;
        else
            *lock = false;
    } else {
        *lock = false;
    }

    return 0;
}

int wlan_setWpsSetupLock(int index, bool lock)
{
    char tmp[32];

    check_env();
    sprintf(tmp, "%d", lock);
    return athwps_setbyname(index, "ap_setup_locked", tmp);
}

int wlan_getWpsConfigurationState(int index, char *state)
{
    char buf[256];
    char cmd[64];
    char *pos;

    check_env();
    if(index < 0)
        return 0;

    sprintf(cmd, "hostapd_cli -i ath%d get_config", index);
    athcfg_executecmd(__func__,cmd,buf);

    if((pos=strstr(buf, "wps_state="))!=NULL) {
        if(strstr(pos, "not configured")!=NULL)
            sprintf(state, "%s", "Not configured");
        else if (strstr(pos, "configured")!=NULL)
            sprintf(state, "%s", "configured");
    } else
        sprintf(state, "%s", "Not configured");

    printf("%s: %s\n",__func__,state);


    return 0;
}

int wlan_getWpsLastConfigurationError(int index, char *error)
{
    check_env();
    // TODO
    sprintf(error, "%s", "NoError");

    return 0;
}

int wlan_getWpsRegistrarNumEntries(int index, unsigned int *numEntries)
{
    check_env();
    //TODO
    *numEntries = 1;

    return 0;
}

int wlan_getWpsRegistrarEstablished(int index, bool *established)
{
    check_env();

    //TODO
    if(index < 0)
        return 0;

    /* The state of Registrar or Enrollee is transitional while doing WPS operation, 
     * current we return registrar establish by default
     */
    *established = true;


    return 0; 
}

int wlan_getWpsRegistrarEnabled(int index, int registrarIndex, bool *enabled)
{
    check_env();
    //TODO

    if(index < 0)
        return 0;

    /* The state of Registrar or Enrollee is transitional while doing WPS operation, 
     * current we return registrar enable by default
     */
    *enabled = true;

    return 0;
}

int wlan_setWpsRegistrarEnabled(int index, int registrarIndex, bool enabled)
{
    check_env();
    //TODO
    return 0;
}

int wlan_getWpsRegistrarUuid(int index, int registrarIndex, char *uuid)
{
    check_env();
    //TODO
    if(athwps_getbyname(index, "uuid", uuid)<0) {
        memset(uuid, 0, sizeof(uuid));
        return -1;
    }
    return 0;
}

int wlan_getWpsRegistrarDeviceName(int index, int registrarIndex, char *devName)
{
    check_env();
    //TODO
    if(athwps_getbyname(index, "device_name", devName)<0) {
        memset(devName, 0, sizeof(devName));
        return -1;
    }
    return 0;
}


int wlan_setWpsPbcTrigger(int index)
{
    char buf[256];
    char cmd[64];
    bool enable;
    int  wpsEnable;
    
    check_env();

    /* return ERROR if interface or WPS not enabled*/
/*
    wlan_getEnable(index, &enable);
    if (!enable) return -1;

    wlan_getWpsEnable(index, &wpsEnable);
    if (!wpsEnable) return -1;

    if(index < 0)
        return 0;
*/

    sprintf(cmd, "hostapd_cli wps_cancel;usleep 500000;hostapd_cli wps_pbc");
    athcfg_executecmd(__func__,cmd,buf);

	printf("%s: executed WPS command %s\n", __func__, cmd);

    if((strstr(buf, "OK"))!=NULL) {
        return 0;
    } 
    else
        return -1;

}

int wlan_setWpsEnrolleePin(int index, char *pin)
{
    char buf[256];
    char cmd[128];
    bool enable;
    int  wpsEnable;
    
    check_env();

    /* return ERROR if interface or WPS not enabled*/
/*
    wlan_getEnable(index, &enable);
    if (!enable) return -1;

    wlan_getWpsEnable(index, &wpsEnable);
    if (!wpsEnable) return -1;

    if(index < 0)
        return 0;
*/

    sprintf(cmd, "hostapd_cli wps_cancel;usleep 500000;hostapd_cli wps_pin any %s", pin);
    athcfg_executecmd(__func__,cmd,buf);

	printf("%s: executed WPS command %s\n", __func__, cmd);

    if((strstr(buf, "OK"))!=NULL) {
        return 0;
    } 
    else
        return -1;

}


int wlan_getSsidQosEnabled(int radio, bool *enable)
{
    char buf[128];
    char cmd[64];
	char *pos;
    char tmp[2];

    check_env();
    if(radio < 0)
        return 0;

    sprintf(cmd, "iwpriv wifi%d get_ssidqos_ena",radio);

    athcfg_executecmd(__func__,cmd,buf);

	pos = buf;
    if((pos=strstr(buf, "get_ssidqos_ena:"))!=NULL) {
        pos += strlen("get_ssidqos_ena:");
        strncpy(tmp, pos, 1);
		*(tmp+1) = 0;
        *enable = (atoi(tmp) == 0) ? false : true;
    }

    return 0;
}


int wlan_setSsidQosEnabled(int radio, bool enabled)
{
    char buf[128];
    char cmd[64];

    check_env();
    if(radio < 0)
        return 0;

    sprintf(cmd, "iwpriv wifi%d ssidqos_ena %d",radio,enabled);

    athcfg_executecmd(__func__,cmd,buf);

    return 0;

}


int wlan_getSsidQosLevel(int index, char *lvl)
{
    char buf[128];
    char cmd[64];
	char *pos;
    char tmp[3];

    check_env();
    if(index < 0)
        return 0;

    sprintf(cmd, "iwpriv ath%d get_ssidqos_lvl",index);

    athcfg_executecmd(__func__,cmd,buf);

	pos = buf;
    if((pos=strstr(buf, "get_ssidqos_lvl:"))!=NULL) {
        pos += strlen("get_ssidqos_lvl:");
        strncpy(tmp, pos, 2);
		*(tmp+2) = 0;
		strcpy(lvl, tmp);
    }

    return 0;
}

int wlan_setSsidQosLevel(int index, char *lvl)
{
    char buf[128];
    char cmd[64];

    check_env();
    if(index < 0)
        return 0;

    sprintf(cmd, "iwpriv ath%d ssidqos_lvl %s",index,lvl);

    athcfg_executecmd(__func__,cmd,buf);

    return 0;

}

#ifdef USGV2
int wlan_getBeaconInterval(int index, int *bintval)
{
    int ret; 
    char tmp[32];

    check_env();

    ret = iwpriv_getbyname(index, "get_bintval", tmp);
    if (ret == 0) {
        *bintval = atoi(tmp);
    }
    return 0; 
}

int wlan_setBeaconInterval(int index, int bintval)
{
    int ret;
    char tmp[32];

    check_env();
    sprintf(tmp,"%d",bintval);
    ret = iwpriv_setbyname(index, "bintval", tmp);
    return ret;
}

int wlan_getDTIMInterval(int index, int *dtimPeriod)
{
    int ret; 
    char tmp[32];

    check_env();
    ret = iwpriv_getbyname(index, "get_dtim_period", tmp);
    if (ret == 0) {
        *dtimPeriod = atoi(tmp);
    }
    return 0; 
}
int wlan_setDTIMInterval(int index, int dtimPeriod)
{
    int ret;
    char tmp[32];

    check_env();
    sprintf(tmp,"%d",dtimPeriod);
    ret = iwpriv_setbyname(index, "dtim_period", tmp);
    return ret;
}
int wlan_getCtsProtectionEnable(int index, bool *enabled)
{
    int ret; 
    char tmp[32];

    check_env();
    ret = iwpriv_getbyname(index, "get_protmode", tmp);
    if (ret == 0) {
        *enabled = (atoi(tmp) == 0) ? false : true;
    }
    return 0; 
}
int wlan_setCtsProtectionEnable(int index, bool enabled)
{
    int ret;
    char tmp[32];

    check_env();
    sprintf(tmp,"%d",enabled);
    ret = iwpriv_setbyname(index, "protmode", tmp);
    return ret;
}
int wlan_getObssCoexistenceEnable(int index, bool *enabled)
{
    int ret; 
    char tmp[32];

    check_env();
    ret = iwpriv_getbyname(index, "g_disablecoext", tmp);
    if (ret == 0) {
        bool disabled =  (atoi(tmp) == 0) ? false : true;
        if (disabled == true) {
            *enabled = false;
        } else {
            *enabled = true;
        }
        
    }
    return 0; 
}
int wlan_setObssCoexistenceEnable(int index, bool enabled)
{
    int ret;
    char tmp[32];

    check_env();
    sprintf(tmp,"%d",!enabled);
    ret = iwpriv_setbyname(index, "disablecoext", tmp);
    return ret;
}
int wlan_getMode(int index, char *mode)
{
    int ret; 
    char tmp[32];

    check_env();
    ret = iwpriv_getbyname(index, "get_mode", tmp);
    if (ret == 0) {
        // remove newline so that it's not copied as part of the key
        char *newline = strstr(tmp,"\n");
        if (newline) *newline = 0;

        strcpy(mode,tmp);
    }
    return 0; 
}
int wlan_setMode(int index, char *mode)
{
    int ret;

    check_env();
    ret = iwpriv_setbyname(index, "mode", mode);
    return ret;
}
int wlan_getFragThresh(int index, unsigned int *threshold)
{
    char cmd[64];
    char buf[1024];
    char *pos;

    check_env();

    if(index < 0) {
        memset(threshold, 0 , sizeof(threshold));
        return -1;
    }

    sprintf(cmd, "iwconfig ath%d",index);
    athcfg_executecmd(__func__,cmd,buf);

    pos = buf;
    
    if((pos=strstr(pos,"Fragment thr"))!=NULL)
    {
        pos +=strlen("Fragment thr:");
        if (strncmp(pos, "off", strlen("off")) == 0)
        {
            memset(threshold, 0 , sizeof(threshold));
        } else {
            char *end;

            if ((end=strstr(pos," ")) != NULL)
            {
                end = '\0';
            }
            *threshold = atoi(pos);
        }
    } else {
        memset(threshold, 0 , sizeof(threshold));
    }

    return 0;
}
int wlan_setFragThresh(int index,  unsigned int threshold)
{
    char buf[512];
    char cmd[64];

    check_env();
    if(index < 0) {
        return 0;
    }

    /* would cause segmentation fault, fix me */
    if (threshold > 0)
    {
        sprintf(cmd, "iwconfig ath%d frag %d", index, threshold);
    } else {
        sprintf(cmd, "iwconfig ath%d frag off", index );
    }
    athcfg_executecmd(__func__,cmd,buf);

    return 0; 
}
int wlan_getRTSThresh(int index, unsigned int *threshold)
{
    char cmd[64];
    char buf[1024];
    char *pos;

    check_env();

    if(index < 0) {
        memset(threshold, 0 , sizeof(threshold));
        return -1;
    }

    sprintf(cmd, "iwconfig ath%d",index);
    athcfg_executecmd(__func__,cmd,buf);

    pos = buf;
    // Need actaul string
    if((pos=strstr(pos,"RTS thr"))!=NULL)
    {
        pos +=strlen("RTS thr:");
        if (strncmp(pos, "off", strlen("off")) == 0)
        {
            memset(threshold, 0 , sizeof(threshold));
        } else {
            char *end;

            if ((end = strstr(pos," ")))
            {
                end = '\0';
            }
            *threshold = atoi(pos);
        }
    } else {
        memset(threshold, 0 , sizeof(threshold));
    }

    return 0;
}
int wlan_setRTSThresh(int index,  unsigned int threshold)
{
    char buf[512];
    char cmd[64];

    check_env();
    if(index < 0) {
        return 0;
    }

    if (threshold > 0)
    {
        sprintf(cmd, "iwconfig ath%d rts %d", index, threshold);
    } else {
        sprintf(cmd, "iwconfig ath%d rts off", index);
    }
    athcfg_executecmd(__func__,cmd,buf);

    return 0; 
}

int wlan_getChannelMode(int index, char * channelMode)
{
    int radio_idx;
    bool enable;
    
    check_env();

    radio_idx = athcfg_get_radio(index);

    if (radio_idx == 1)
        index = 1;
    else
        index = 0;

    athcfg_getbyname(index, "AP_CHMODE", channelMode);

    return 0;
}

int wlan_setChannelMode(int index, char * channelMode, bool gOnlyFlag, bool nOnlyFlag, bool acOnlyFlag)
{
    int radio_idx;
    bool enable;
    char tmp[8];
    
    check_env();

    radio_idx = athcfg_get_radio(index);

    if (radio_idx == 1)
        index = 1;
    else
        index = 0;

    sprintf(tmp,"%d",gOnlyFlag);
    athcfg_setbyname(index, "PUREG", tmp);

    sprintf(tmp,"%d",nOnlyFlag);
    athcfg_setbyname(index, "PUREN", tmp);

    sprintf(tmp,"%d",acOnlyFlag);
    athcfg_setbyname(index, "PURE11AC", tmp);

    athcfg_setbyname(index, "AP_CHMODE", channelMode);

    return 0;
}

int wlan_pushChannelMode(int athIndex)
{
    char tmp[128];
    char buf[1024];
    char chanMode[32];
    char pureG[32];
    char pureN[32];
    char pureAC[32];
    int radioIndex      = -1;

    wlan_getRadioIndex(athIndex,&radioIndex);

    athcfg_getbyname(radioIndex, "AP_CHMODE", chanMode);

    sprintf(tmp, "iwpriv ath%d mode %s",athIndex,chanMode);
    athcfg_executecmd(__func__,tmp,buf);

    athcfg_getbyname(radioIndex, "PUREG", pureG);
    sprintf(tmp, "iwpriv ath%d pureg %s",athIndex,pureG);
    athcfg_executecmd(__func__,tmp,buf);

    athcfg_getbyname(radioIndex, "PUREN", pureN);
    sprintf(tmp, "iwpriv ath%d puren %s",athIndex,pureN);
    athcfg_executecmd(__func__,tmp,buf);

    athcfg_getbyname(radioIndex, "PURE11AC", pureAC);
    sprintf(tmp, "iwpriv ath%d pure11ac %s",athIndex,pureAC);
    athcfg_executecmd(__func__,tmp,buf);

    if (strstr(chanMode,"11NG")) {
        sprintf(tmp, "iwpriv wifi%d ForBiasAuto 1",radioIndex);
        athcfg_executecmd(__func__,tmp,buf);
    }

    if (strstr(chanMode,"11NG") ||
        strstr(chanMode,"11G")) {
        sprintf(tmp, "iwpriv ath%d vap_doth 0",athIndex);
        athcfg_executecmd(__func__,tmp,buf);
    }

    if (strstr(chanMode,"PLUS")) {
        sprintf(tmp, "iwpriv ath%d chextoffset 1",athIndex);
        athcfg_executecmd(__func__,tmp,buf);
    } else if (strstr(chanMode,"MINUS")) {
        sprintf(tmp, "iwpriv ath%d chextoffset -1",athIndex);
        athcfg_executecmd(__func__,tmp,buf);
    } else {
        sprintf(tmp, "iwpriv ath%d chextoffset 0",athIndex);
        athcfg_executecmd(__func__,tmp,buf);
    }

    return 0;
}
int wlan_getNumberOfRadios(int *numEntries)
{
    char buf[1024];
    int num = 0;
    char *pos;

    check_env();
    if(athcfg_executecmd(__func__, "ifconfig | grep wifi | cut -f1 -d' '", buf))
        return -1;

    pos = buf;
    while((pos=strstr(pos,"wifi"))!=NULL)
    {
        pos +=3;
        num++;
    } 

    *numEntries = num;

    return 0;
}

int wlan_getSSIDEntry(int index, char *entry) 
{
    char buf[1024];
    int count = 0;
    char *pos, *end;

    check_env();
    if(athcfg_executecmd(__func__, "ifconfig | grep ath | cut -f1 -d' '", buf))
        return -1;

    pos = buf;
    while((end=strstr(pos,"\n"))!=NULL)
    {
        if (count == index)
        {
            memset(end,0,1);
	    sprintf(entry, "%s", pos);
            break;
        } else {
            pos = end;
            pos++;
            count++;
        }
    } 

    return 0;
}
int wlan_getIndexFromName( char *inputAthName, int *sSIDIndex )
{
    char ssid[32];
    int num = 0;
    char *pos;

    *sSIDIndex = -1;
    sscanf(inputAthName,"ath%d",sSIDIndex);

    return 0;
}
int wlan_getFrequency(int index, char *freq)
{
    char cmd[64];
    char buf[1024];
    char *pos;

    check_env();

    if(index < 0) {
        freq[0] = 0;
        return -1;
    }

    sprintf(cmd, "iwconfig ath%d",index);
    athcfg_executecmd(__func__,cmd,buf);

    pos = buf;
    if((pos=strstr(pos,"Frequency:"))!=NULL)
    {
        pos +=10;
        memcpy(freq, pos, 5);
        freq[5] = 0;
    }

    return 0;
}
int wlan_setFrequency(int index, char *freq)
{
    char cmd[64];
    char buf[1024];
    char *pos;

    check_env();

    if(index < 0) {
        freq[0] = 0;
        return -1;
    }

    sprintf(cmd, "iwconfig ath%d freq %s",index, freq);
    athcfg_executecmd(__func__,cmd,buf);

    return 0;
}

int wlan_getMaxStations(int index, int *maxStations)
{
    int ret; 
    char tmp[32];

    check_env();
    ret = iwpriv_getbyname(index, "get_maxsta", tmp);
    if (ret == 0) {
        *maxStations = atoi(tmp);
    } else {
        *maxStations = 30;
    }
    return 0; 
}
int wlan_setMaxStations(int index, int maxStations)
{
    int ret;
    char tmp[32];

    check_env();
    sprintf(tmp,"%d",maxStations);
    if (maxStations >= 255)
    {
        maxStations = 255;
    }
    ret = iwpriv_setbyname(index, "maxsta", tmp);
    return ret;
}

int wlan_getCountryCode(int index, char *code)
{
    int ret; 
    char cmd[64];
    char buf[1024];
    int radio_idx;

    check_env();

    radio_idx = athcfg_get_radio(index);

    if (radio_idx == 1)
        index = 1;
    else
        index = 0;
    sprintf(cmd, "iwpriv wifi%d getCountry",index);

    ret = athcfg_executecmd(__func__,cmd,buf);
    if (ret == 0)
    {
	char *pos = NULL;
	if((pos=strstr(buf,"getCountry:"))!=NULL)
	{
	    pos += strlen("getCountry:");      

	    // remove newline so that it's not copied as part of the key
	    char *newline = strstr(pos,"\n");
	    if (newline) *newline = 0;

	    strcpy(code,pos);
	}
    }
    return 0; 
}
int wlan_setCountryCode(int index, char *code)
{
    int ret;
    char cmd[64];
    char buf[1024];
    int radio_idx;

    check_env();

    radio_idx = athcfg_get_radio(index);

    if (radio_idx == 1)
        index = 1;
    else
        index = 0;

    sprintf(cmd, "iwpriv wifi%d setCountry %s",index, code);

    ret = athcfg_executecmd(__func__,cmd,buf);
    return 0;
}

int wlan_getMacAddressControlMode(int index, int *mode)
{
    int ret;
    char cmd[64];
    char buf[1024];
    char *pos;

    check_env();
    
    sprintf(cmd, "iwpriv ath%d get_maccmd",index);

    ret = athcfg_executecmd(__func__,cmd,buf);

    if ((pos = strstr(buf,"get_maccmd:")) != NULL) {
       pos += strlen("get_maccmd:");
       *mode = atoi(pos);
    } else {
        *mode = 0;   
    }
    return 0;
}

int wlan_setMacAddressControlMode(int index, int mode)
{
    int ret;
    char cmd[64];
    char buf[1024];

    check_env();

    sprintf(cmd, "iwpriv ath%d maccmd %d",index, mode);

    ret = athcfg_executecmd(__func__,cmd,buf);

    return 0;
}
int wlan_getAclDeviceNum(int index, unsigned int *devNum)
{
    char buf[1500];
    char tmp[64];
    unsigned int assoc_cnt = 0;
    char *pos;

    check_env();
    if(index < 0) {
        *devNum = 0;
        return -1;
    }
    
    sprintf(tmp, "iwpriv ath%d getmac", index);
    athcfg_executecmd(__func__,tmp,buf);

    pos = buf;
    while((pos=strstr(pos,"\n"))!=NULL){
	assoc_cnt++;
	pos +=1;
    }
    *devNum = assoc_cnt;

    return 0;
}
int wlan_getAclDevice(int index, int devIndex, char *dev)
{
    char buf[1500];
    char tmp[64];
    unsigned int assoc_cnt = 0;
    char *pos;
    int i;

    memset(dev, 0, 18);

    check_env();
    if(index < 0) {
        memset(dev, 0, sizeof(dev));
        return -1;
    }

    sprintf(tmp, "iwpriv ath%d getmac", index);
    athcfg_executecmd(__func__,tmp,buf);

    pos = buf;
    pos = strstr(pos,"getmac:");
    pos += 7;

    do {
        // move past newline feed
        if (assoc_cnt != 0) pos++; 

	if(assoc_cnt == devIndex) {
            pos = strstr(pos,":");
            pos -= 2;
	    memcpy(dev, pos, 17);
            dev[17] = '\0';
            break;
	}
	assoc_cnt++;      
    } while((pos=strstr(pos,"\n"))!=NULL);

    return 0;
}
int wlan_addAclDevice(int index, char *dev)
{
    char buf[1500];
    char tmp[64];
    unsigned int assoc_cnt = 0;
    char *pos;
    int i;

    check_env();
    if(index < 0) {
        return -1;
    }

    sprintf(tmp, "iwpriv ath%d addmac %s", index, dev);
    printf("%s: %s \n", __func__, tmp);

    athcfg_executecmd(__func__,tmp,buf);

    return 0;
}

int wlan_delAclDevice(int index, char *dev)
{
    char buf[1500];
    char tmp[64];
    unsigned int assoc_cnt = 0;
    char *pos;
    int i;

    check_env();
    if(index < 0) {
        return -1;
    }

    sprintf(tmp, "iwpriv ath%d delmac %s", index, dev);
    athcfg_executecmd(__func__,tmp,buf);

    return 0;
}

int wlan_getVapEnable(int index, bool *enable)
{
    char buf[1024];
    char tmp[64];
    int num = 0;
    char *pos;

    check_env();
    sprintf(tmp, "ifconfig ath%d 2>/dev/null | cut -f1 -d' '", index);
    if(athcfg_executecmd(__func__, tmp, buf))
    {
        *enable = false;
        return -1;
    }

    pos = buf;
    if ((pos=strstr(pos,"ath"))!=NULL)
    {
        *enable = true;
    }  else {
        *enable = false;
    }

    return 0;
}
int wlan_factoryReset()
{
    char buf[1500];

    athcfg_executecmd(__func__,"apdown 1>&2",buf);
    athcfg_executecmd(__func__,"rm -rf /nvram/etc",buf);
    athcfg_executecmd(__func__,"rm -rf /tmp/.apcfg",buf);
    athcfg_executecmd(__func__,"rm -rf /nvram/wifiStart.sh",buf);
    athcfg_executecmd(__func__,"rm -rf /tmp/secath*",buf);

    return 0;
}
int wlan_factoryResetRadios()
{
    char buf[1500];

    athcfg_executecmd(__func__,"apcfgRadioReset 1>&2",buf);

    return 0;
}
int wlan_factoryRestart()
{
    char buf[1500];

    athcfg_executecmd(__func__,"cd /etc/init.d/; /etc/init.d/wifiStart.sh start >/dev/null 2>&1",buf);

    return 0;
}

int wlan_createCfgFile()
{
    char buf[1500];

    athcfg_executecmd(__func__,"cd /etc/init.d/; /etc/init.d/wifiStart.sh createcfg >/dev/null 2>&1",buf);

    return 0;
}
int wlan_getSTBCEnable(int index,bool *enable)
{
    int ret; 
    char cmd[64];
    char buf[1024];
    int radio_idx;

    check_env();

    sprintf(cmd, "iwpriv wifi%d get_txstbc",index);

    ret = athcfg_executecmd(__func__,cmd,buf);
    if (ret == 0)
    {
	char *pos = NULL;
	if((pos=strstr(buf,"get_txstbc:"))!=NULL)
	{
	    pos += strlen("get_txstbc:");      

	    // remove newline so that it's not copied as part of the key
	    char *newline = strstr(pos,"\n");
	    if (newline) *newline = 0;

            *enable = (atoi(pos) == 1) ? true : false;
	}
    }
    return 0;
}

int wlan_setSTBCEnable(int index,bool enable)
{
    char buf[1500];
    char tmp[64];
    int radio_idx;

    check_env();

    if(index < 0) {
        return -1;
    }

    sprintf(tmp, "iwpriv wifi%d txstbc %d", index, ((enable==true) ? 1 : 0));
    athcfg_executecmd(__func__,tmp,buf);
    sprintf(tmp, "iwpriv wifi%d rxstbc %d", index, ((enable==true) ? 1 : 0));
    athcfg_executecmd(__func__,tmp,buf);

    return 0;
}

int wlan_getAMSDUEnable(int index,bool *enable)
{
    int ret; 
    char cmd[64];
    char buf[1024];
    int radio_idx;

    check_env();

    radio_idx = athcfg_get_radio(index);

    if (radio_idx == 1)
        index = 1;
    else
        index = 0;

    sprintf(cmd, "iwpriv wifi%d getAMSDU",index);

    ret = athcfg_executecmd(__func__,cmd,buf);
    if (ret == 0)
    {
	char *pos = NULL;
	if((pos=strstr(buf,"getAMSDU:"))!=NULL)
	{
	    pos += strlen("getAMSDU:");      

	    // remove newline so that it's not copied as part of the key
	    char *newline = strstr(pos,"\n");
	    if (newline) *newline = 0;

            *enable = (atoi(pos) == 1) ? true : false;
	}
    }
    return 0;
}

int wlan_setAMSDUEnable(int index, bool enable)
{
    char buf[1500];
    char tmp[64];
    int radio_idx;

    check_env();

    radio_idx = athcfg_get_radio(index);

    if (radio_idx == 1)
        index = 1;
    else
        index = 0;

    if(index < 0) {
        return -1;
    }

    sprintf(tmp, "iwpriv wifi%d AMSDU %d", index, ((enable==true) ? 1 : 0));
    athcfg_executecmd(__func__,tmp,buf);

    return 0;
}

int wlan_cancelWps(int index)
{
    char buf[256];
    char cmd[64];
    bool enable;
    int  wpsEnable;
    
    check_env();

    /* return ERROR if interface or WPS not enabled*/
/*
    wlan_getEnable(index, &enable);
    if (!enable) return -1;

    wlan_getWpsEnable(index, &wpsEnable);
    if (!wpsEnable) return -1;

    if(index < 0)
        return 0;
*/

    sprintf(cmd, "hostapd_cli wps_cancel");
    athcfg_executecmd(__func__,cmd,buf);

    printf("%s: executed WPS command %s\n", __func__, cmd);

    if((strstr(buf, "OK"))!=NULL) {
        return 0;
    } 
    else
        return -1;

}

int wlan_getGuardInterval(int index, bool *enable)
{
    int ret; 
    char cmd[64];
    char buf[1024];

    check_env();

    *enable = true; // Ena

    sprintf(cmd, "iwpriv ath%d get_shortgi",index);

    ret = athcfg_executecmd(__func__,cmd,buf);
    if (ret == 0) {
        char *pos = NULL;
        if ((pos=strstr(buf,"get_shortgi:"))!=NULL) {
            pos += strlen("get_shortgi:");      

            // remove newline so that it's not copied as part of the key
            char *newline = strstr(pos,"\n");
            if (newline) *newline = 0;

            *enable = (atoi(pos) == 1) ? true : false;
        }
    }
    return 0;
}

int wlan_setGuardInterval(int index, bool enable)
{
    char buf[1500];
    char tmp[64];
    int radio_idx;

    check_env();

    sprintf(tmp, "iwpriv ath%d shortgi %d", index, ((enable==true) ? 1 : 0));
    athcfg_executecmd(__func__,tmp,buf);

    return 0;
}

int wlan_getTxChainMask(int index, int *numStreams)
{
    int radio_idx;
    bool enable;
    char tmp[64];
    int mask = 0;
    
    check_env();

    radio_idx = athcfg_get_radio(index);

    if (radio_idx == 1)
        index = 1;
    else
        index = 0;

    athcfg_getbyname(index, "TX_CHAINMASK", tmp);
    mask = atoi(tmp);
    switch (mask) {
    case 1: *numStreams = 1; break; // b001
    case 3: *numStreams = 2; break; // b011
    case 7: *numStreams = 3; break; // b111
    default: return -1;
    }

    return 0;
}

int wlan_setTxChainMask(int index, int numStreams)
{
    int radio_idx;
    bool enable;
    char tmp[8];
    int mask = 0;
    
    check_env();

    radio_idx = athcfg_get_radio(index);

    if (radio_idx == 1)
        index = 1;
    else
        index = 0;

    switch (numStreams) {
    case 1: mask = 1; break;    // b001
    case 2: mask = 3; break;    // b011
    case 3: mask = 7; break;    // b111
    default: return -1;
    }
    sprintf(tmp, "%d", mask);
    athcfg_setbyname(index, "TX_CHAINMASK", tmp);

    return 0;
}

int wlan_pushTxChainMask(int index, int numStreams)
{
    char tmp[128];
    char buf[1024];
    int mask = 0;
    
    switch (numStreams) {
    case 1: mask = 1; break;    // b001
    case 2: mask = 3; break;    // b011
    case 3: mask = 7; break;    // b111
    default: return -1;
    }

    sprintf(tmp, "iwpriv wifi%d txchainmask %d", index, mask);
    athcfg_executecmd(__func__,tmp,buf);

    return 0;
}

int wlan_getRxChainMask(int index, int *numStreams)
{
    int radio_idx;
    bool enable;
    char tmp[64];
    int mask = 0;
    
    check_env();

    radio_idx = athcfg_get_radio(index);

    if (radio_idx == 1)
        index = 1;
    else
        index = 0;

    athcfg_getbyname(index, "RX_CHAINMASK", tmp);
    mask = atoi(tmp);
    switch (mask) {
    case 1: *numStreams = 1; break; // b001
    case 3: *numStreams = 2; break; // b011
    case 7: *numStreams = 3; break; // b111
    default: return -1;
    }

    return 0;
}

int wlan_setRxChainMask(int index, int numStreams)
{
    int radio_idx;
    bool enable;
    char tmp[8];
    int mask = 0;
    
    check_env();

    radio_idx = athcfg_get_radio(index);

    if (radio_idx == 1)
        index = 1;
    else
        index = 0;

    switch (numStreams) {
    case 1: mask = 1; break;    // b001
    case 2: mask = 3; break;    // b011
    case 3: mask = 7; break;    // b111
    default: return -1;
    }
    sprintf(tmp, "%d", mask);
    athcfg_setbyname(index, "RX_CHAINMASK", tmp);

    return 0;
}

int wlan_pushRxChainMask(int index, int numStreams)
{
    char tmp[128];
    char buf[1024];
    int mask = 0;
    int athIndex;
    
    switch (numStreams) {
    case 1: mask = 1; break;    // b001
    case 2: mask = 3; break;    // b011
    case 3: mask = 7; break;    // b111
    default: return -1;
    }
    if (wlan_getActiveAthIndex(index,&athIndex) == 0) {
        sprintf(tmp, "iwpriv wifi%d rxchainmask %d", index, mask);
        athcfg_executecmd(__func__,tmp,buf);
    }

    return 0;
}

int wlan_getStartMode(char *mode)
{
    char tmp[32];

    check_env();
    athcfg_getbyname(0, "AP_STARTMODE", tmp);
    sprintf(mode,"%s", tmp);

    return 0;
}

int wlan_setStartMode(char *mode)
{
    check_env();
 
    athcfg_setbyname(0, "AP_STARTMODE", mode);

    return 0;
}

int wlan_getVlanId(int index, int *vlanId)
{
    char tmp[32];

    athcfg_getbyname(index, "AP_VLAN", tmp);
    *vlanId = atoi(tmp);

    return 0;
}

int wlan_setVlanId(int index, int vlanId)
{
    char tmp[8];
    sprintf(tmp, "%d", vlanId);
    athcfg_setbyname(index, "AP_VLAN", tmp);

    return 0;
}

int wlan_getBridgeInfo(int index, char *bridge, char *ip, char *subnet)
{
    int wps_state;
    char tmp[64];

    athcfg_getbyname(index, "AP_BRNAME", bridge);
    // remove new line character
    if ( strstr(bridge,"\n") != NULL) { bridge[strlen(bridge)-1] = 0; }

    athcfg_getbyname(index, "AP_VIPADDR", ip);
    // remove new line character
    if ( strstr(ip,"\n") != NULL) { ip[strlen(ip)-1] = 0; }
    athcfg_getbyname(index, "AP_VIPSUBNET", subnet);
    // remove new line character
    if ( strstr(subnet,"\n") != NULL) { subnet[strlen(subnet)-1] = 0; }

    return 0;
}
int wlan_pushBridgeInfo(int index)
{
    char ip[32]; 
    char subnet[32]; 
    char bridge[32];
    int vlanId;
    char tmp[128];
    char buf[1024];

    wlan_getBridgeInfo(index,bridge,ip,subnet);
    wlan_getVlanId(index,&vlanId);

    sprintf(tmp, "cfgVlan ath%d %s %d %s ", index, bridge, vlanId, ip);
    printf("%s: cfgVlan ath%d %s %d %s ", __func__, index, bridge, vlanId, ip);
    athcfg_executecmd(__func__,tmp,buf);

    return 0;
}
int wlan_setBridgeInfo(int index, char *bridge, char *ip, char *subnet)
{
    int wps_state;
    char tmp[64];

    athcfg_setbyname(index, "AP_BRNAME", bridge);
    athcfg_setbyname(index, "AP_VIPADDR", ip);
    athcfg_setbyname(index, "AP_VIPSUBNET", subnet);

    /* update wps configuration file */
    athcfg_getbyname(index, "WPS_ENABLE", tmp);
    wps_state = atoi(tmp);
    if(wps_state) {
        athwps_setbyname(index, "bridge", bridge);
        athwps_setbyname(index, "upnp_iface", bridge);
    }
    return 0;
}
int wlan_resetVlanCfg(int index)
{
    char tmp[128];
    char buf[1024];

    // remove old config
    sprintf(tmp, "removeVlan ath%d ", index);
    printf("%s: %s ", __func__, tmp);
    athcfg_executecmd(__func__,tmp,buf);

    // push new config
    wlan_pushBridgeInfo(index);
}
int wlan_getApBridging(int index, bool *enable)
{
    char buf[1500];
    char tmp[64];
    int ret; 

    check_env();

    sprintf(tmp, "iwpriv ath%d get_ap_bridge", index);
    ret = athcfg_executecmd(__func__,tmp,buf);

    // code to parse response
    if (ret == 0)
    {
        char *pos = NULL;
        if((pos=strstr(buf,"get_ap_bridge:"))!=NULL)
        {
            pos += strlen("get_ap_bridge:");      

            // remove newline so that it's not copied as part of the key
            char *newline = strstr(pos,"\n");
            if (newline) *newline = 0;

                *enable = (atoi(pos) == 1) ? true : false;
        }
    }

    return 0;
}
int wlan_setApBridging(int index, bool enable)
{
    char buf[1500];
    char tmp[64];

    check_env();

    sprintf(tmp, "iwpriv ath%d ap_bridge %d", index, ((enable==true) ? 1 : 0));
    athcfg_executecmd(__func__,tmp,buf);

    return 0;
}

int wlan_getChannelsInUse(int radioIndex, char *channelsInUse)
{
    char buf[1500];
    char tmp[64];
    int athIndex;
    int ret; 
    int channel;
    int first = true;

    channelsInUse[0] = 0;

    check_env();

    // Find the first ath that is up on the given radio
    for (athIndex = radioIndex; athIndex < 16; athIndex+=2) {
        bool enabled;
        wlan_getVapEnable(athIndex, &enabled);
        if (enabled == true) {
            break;
        }
    }

    if (athIndex < 16 ) {

        sprintf(tmp, "iwlist ath%d scan last | grep Channel | sort -u", athIndex);
        ret = athcfg_executecmd(__func__,tmp,buf);

        int buflen = strlen(buf);

        if (ret == 0) {
            char *pos = buf;

            while ((pos=strstr(pos,"Channel"))!=NULL) {
                pos += strlen("Channel ");      

                char *endParn = strstr(pos,")");
                if (endParn) *endParn = 0;

                channel = atoi(pos);
                if (first == true) {
                    sprintf(channelsInUse, "%d", channel);
                    first = false;
                } else {
                    sprintf(channelsInUse, "%s,%d", channelsInUse, channel);
                }
                pos = endParn+1;
            }
        }
        return 0;
    } else {
        // No VAPs were enabled for this radio so we could not get channels in use
        return 1;
    }

}

static int getNextCell(FILE *f, char *retBuf, int size)
{
    char *ptr = retBuf;
    bool foundStart = false,  foundEnd = false;
    int bytesRead = 0;

    while (!feof(f) && (bytesRead < size) && (foundEnd == false))
    {
        int len = 0;
        *ptr = 0;
        fgets(ptr,120,f);

        len = strlen(ptr);
        bytesRead += len;

        // printf("ptr = %p, len %d, s %s\n", ptr, strlen(ptr), ptr);

        if(len == 0)
        {
            break;
        }
        if(strstr(ptr,"Cell")) {
            if(foundStart == false) {
                foundStart = true;
            } else {
                int end = strlen(retBuf);
                foundEnd = true;
                // Set file pointer back to the beginning of the next cell.
                fseek(f,-len,SEEK_CUR);
                if ((end-len) > -1) {
                    retBuf[end-len] = 0;
                }
            }
        }
        ptr += len;
    } 

    if(foundStart == true) {
        return 0;
    } else {
        if (feof(f)) {
            pclose(f);
        }
        return 1;
    }
}

int wlan_scanApChannels(int radioIndex, char *scanData)
{
    int numCells = 0;
    int offset = 0;
    char buf[1500];
    char tmp[64];
    int athIndex;
    int ret; 
    char mac[20];
    char ssid[40];
    char mode[40];
    char secMode[40];
    int channel = 0;
    int rssi = 0;
    int count = 1;

    scanData[0] = 0;

    check_env();

    // Find the first ath that is up on the given radio
    for (athIndex = radioIndex; athIndex < 16; athIndex+=2) {
        bool enabled;
        wlan_getVapEnable(athIndex, &enabled);
        if (enabled == true) {
            break;
        }
    }
    if (athIndex >= 16 ) {
        return 1;
    }

    FILE *file;

    sprintf(tmp, "iwlist ath%d scan last >/tmp/ath%dScanApFile", athIndex, athIndex);
    ret = athcfg_executecmd(__func__,tmp,buf);

    sprintf(tmp, "/tmp/ath%dScanApFile", athIndex);
    if((file = fopen(tmp, "r")) == NULL) {
        printf("%s: fopen %s error\n",__func__, tmp);
        return -1;
    }
    ret = getNextCell(file, buf, 1500);

    while (ret == 0) {
        char temp[40];
        char *pos = buf;
        char *end = NULL;

        numCells++;

        // Address: 00:03:7F:12:3D:5C
        if ((pos = strstr(buf,"Address: "))) 
        {
            pos += strlen("Address: ");      
            strncpy(mac,pos,17);
            mac[17] = 0;
        }

        // phy_mode=IEEE80211_MODE_11NA_HT40MINUS
        if ((pos = strstr(buf,"phy_mode=")))
        {
            pos += strlen("phy_mode=");      
            end = strstr(pos,"\n");
            strncpy(mode,pos,end-pos);
            mode[end-pos] = 0;

            if (strstr(mode,"11AC")) {
                sprintf(mode,"802.11ac");
            } else if (strstr(mode,"11A")) {
                sprintf(mode,"802.11a");
            } else if (strstr(mode,"11B")) {
                sprintf(mode,"802.11b");
            } else if (strstr(mode,"11G")) {
                sprintf(mode,"802.11b/g");
            } else if (strstr(mode,"11NG")) {
                sprintf(mode,"802.11b/g/n");
            } else if (strstr(mode,"11NA")) {
                sprintf(mode,"802.11a/n");
            }
        }

        // ESSID:"Atheros_XSpan_5G"
        if ((pos = strstr(buf,"ESSID:\"")))
        {
            pos += strlen("ESSID:\"");      
            end = strstr(pos,"\"");
            strncpy(ssid,pos,end-pos);
            ssid[end-pos] = 0;
        }

        // Signal level=-78 dBm
        if ((pos = strstr(buf,"Signal level=")))
        {
            pos += strlen("Signal level=");      
            end = strstr(pos," ");
            strncpy(temp,pos,end-pos);
            temp[end-pos] = 0; 
            rssi = atoi(temp);
        }

        // Frequency:5.2 GHz (Channel 40)
        if ((pos = strstr(buf,"Channel ")))
        {
            pos += strlen("Channel ");      
            end = strstr(pos,")");
            strncpy(temp,pos,end-pos);
            temp[end-pos] = 0; 
            channel = atoi(temp);
        }

        if ((pos = strstr(buf,"Encryption key:off"))) {
            sprintf(secMode,"None");
        } else {
            bool wpa = (strstr(buf,"WPA Version") != NULL) ? true : false;
            bool wpa2 = (strstr(buf,"WPA2 Version") != NULL) ? true : false;
            bool tkip = (strstr(buf,"TKIP") != NULL) ? true : false;
            bool ccmp = (strstr(buf,"CCMP") != NULL) ? true : false;
            bool psk = (strstr(buf,"PSK") != NULL) ? true : false;

            if (wpa == false && wpa2 == false) {
                sprintf(secMode, "WEP");
            } else {
                sprintf(secMode, "%s%s%s%s", 
                        (wpa == true && wpa2 == true) ? "WPA/WPA2" : ((wpa == true) ? "WPA" : "WPA2" ),
                        (psk == true) ? "-PSK" : "", 
                        (ccmp == true) ? " AES-CCMP" : "",
                        (tkip == true) ? " TKIP" : "");
            }
        }

        sprintf(&scanData[offset], "%s|%s|%s|%d|%d|%s\n", ssid, secMode, mode, rssi, channel, mac);
        offset = strlen(scanData);
        
        ret = getNextCell(file, buf, 1500);
    }

    return 0;
}

int wlan_getWirelessOnOffButton(int index, bool *enable)
{
    char tmp[32];

    check_env();
    athcfg_getbyname(index, "WIRELESSBUTTON", tmp);
    *enable = (atoi(tmp) == 0) ? false : true;

    return 0;
}

int wlan_setWirelessOnOffButton(int index, bool enable)
{
    char tmp[32];
    
    check_env();
    sprintf(tmp,"%d",enable);
    athcfg_setbyname(index, "WIRELESSBUTTON",tmp);

    return 0;
}

int wlan_getSupportedFrequencyBands(int radioIndex, char *frequencyBands)
{
    char buf[1500];
    char tmp[64];

    check_env();

    frequencyBands[0] = '\0';

    // 2.4GHz
    sprintf(tmp, "grep 168c0030 /proc/bus/pci/devices | head -c 4 | grep %s", (radioIndex == 0) ? "0200" : "0300");
    athcfg_executecmd(__func__,tmp,buf);
    
    if (strlen(buf) != 0) {
       strcpy(frequencyBands,"2.4G");
    } else {
        // 5G 11n
        sprintf(tmp, "grep 168c0033 /proc/bus/pci/devices | head -c 4 | grep %s", (radioIndex == 0) ? "0200" : "0300");
        athcfg_executecmd(__func__,tmp,buf);
        
        if (strlen(buf) != 0) {
           strcpy(frequencyBands,"5G_11N");
        } else {
            // 5G 11ac
            sprintf(tmp, "grep 168c003c /proc/bus/pci/devices | head -c 4 | grep %s", (radioIndex == 0) ? "0200" : "0300");
            athcfg_executecmd(__func__,tmp,buf);
           
            if (strlen(buf) != 0) {
               strcpy(frequencyBands,"5G_11AC");
            } 
        }
    }
    printf("%s: frequencyBands = %s\n", __func__, frequencyBands);
    
    return 0;
}

int wlan_getRouterEnable(int index, bool *routerEnabled)
{
    char tmp[32];
    
    check_env();
    
    athcfg_getbyname(index, "AP_ROUTERENABLED", tmp);
    *routerEnabled =  (atoi(tmp) == 0) ? false : true;

    return 0;
}

int wlan_setRouterEnable(int index, bool routerEnabled)
{
    char tmp[32];
    
    check_env();
    sprintf(tmp,"%d",routerEnabled);
    athcfg_setbyname(index, "AP_ROUTERENABLED", tmp);

    return 0;
}

int wlan_getEnableOnline(int index, bool *enabled)
{
    char tmp[32];
    
    check_env();
    
    athcfg_getbyname(index, "AP_ENABLEONLINE", tmp);
    *enabled = (atoi(tmp) == 0) ? false : true;

    return 0;
}

int wlan_setEnableOnline(int index, bool enabled)
{
    char tmp[32];
    
    check_env();
    sprintf(tmp,"%d",enabled);
    athcfg_setbyname(index, "AP_ENABLEONLINE", tmp);

    return 0;
}

/* This function requires 009-wifi_led_ctrl.patch from QCA SF#01618945. */
int wlan_setLED(int index, bool enabled)
{
    char tmp[128];
    char buf[128];

    sprintf(tmp, "iwpriv wifi%d set_led_state %d", index, enabled);
    athcfg_executecmd(__func__,tmp,buf);
    return 0;
}

int wlan_pushDefaultValues(int index)
{
    char tmp[128];
    char buf[1024];

    // iwpriv wifi$IFNUM HALDbg $HALDEBUG
    sprintf(tmp, "iwpriv wifi%d HALDbg 0", index);
    athcfg_executecmd(__func__,tmp,buf);

    //iwpriv wifi$IFNUM ATHDebug $ATHDEBUG
    sprintf(tmp, "iwpriv wifi%d ATHDebug 0", index);
    athcfg_executecmd(__func__,tmp,buf);

    // Set Aggregation State
    sprintf(tmp, "iwpriv wifi%d AMPDU 1", index);
    athcfg_executecmd(__func__,tmp,buf);
    sprintf(tmp, "iwpriv wifi%d AMPDUFrames 32", index);
    athcfg_executecmd(__func__,tmp,buf);
    sprintf(tmp, "iwpriv wifi%d AMPDULim 50000", index);
    athcfg_executecmd(__func__,tmp,buf);

    // 11n configuration section increase queue length
    sprintf(tmp, "ifconfig wifi%d txqueuelen 1000", index);
    athcfg_executecmd(__func__,tmp,buf);

    return 0;
}
#endif
#ifndef _COSA_INTEL_USG_ATOM_
static const char *commands_help =
"Commands:\n"
"   init                                                Initializes the WLAN AP and starts forwarding data\n"
"   reset                                               delete all VAPs and remove all wifi related modules\n"
"   getNumberOfEntries                                  output the number of the created WLAN virtual AP interface\n"
"   getEnable                       index               get VAP enable flag on VAP with index\n"
"   setEnable                       index val[0/1]      set VAP enable flag on VAP with index\n"
"   getStatus                       index               get VAP status - Up/Error/ Disable\n"
"   getName                         index               get VAP interface name (athX)\n"
"   getBSSID                        index               get MAC address on VAP with index\n"
"   getMaxBitRate                   index               get max transmission rate on VAP with index\n"
"   setMaxBitRate                   index val           set max transmission rate on VAP with index\n"
"   getChannel                      index               get current working channel on VAP with index\n"
"   setChannel                      index val[channel]  set channel\n"
"   getAutoChannelEnable            index               get auto channel selection flag on VAP with index\n"
"   setAutoChannelEnable            index val[0/1]      set auto channel selection flag on VAP with index\n"
"   getSSID                         index               get SSID per VAP with index\n"
"   setSSID                         index val[string]   set SSID per VAP with index\n"
"   getBeaconType                   index               Beacon type string, which denotes which security setting\n"
"                                                       None (No Encryption)\n"
"                                                       Basic (WEP encryption)\n"
"                                                       WPA (TKIP enc)\n"
"                                                       11i (AES enc)\n"
"                                                       WPAand11i (TKIP & AES enc)\n"
"   setBeaconType                   index val[string]   set beacon type (security) string\n"
"   getMacAddressControlEnabled     index               get ACL enabled flag per VAP with index\n"
"   setMacAddressControlEnabled     index val[0/1]      set ACL enabled flag per VAP with index\n"
"   getStandard                     index               get 802.11 standard per VAP with index\n"
"   getWepKeyIndex                  index               get WEP key index (value 1-4) per VAP with index\n"
"   setWepKeyIndex                  index val[1-4]      set WEP key index (value 1-4) per VAP with index\n"
"   getWepEncryptionLevel           index               get encryption level, 40-bit/104-bit/disable\n"
"   getBasicEncryptionModes         index               get basic encryption, None or WEPEncryption\n"
"   setBasicEncryptionModes         index val[string]   set basic encryption mode, None or WEPEncryption\n"
"   getBasicAuthenticationModes     index               get VAP basic authentication modes\n"
"                                                       None (Open Authentication)\n"
"                                                       EAPAuthentication (802.1x)\n"
"                                                       SharedAuthentication      \n"
"   setBasicAuthenticationModes     index               set VAP basic authentication modes\n"
"   getWpaEncryptionModes           index               get WPA encryption per VAP index\n"
"                                                       TKIPEncryption\n"
"                                                       AESEncryption\n"
"                                                       TKIPandAESEncryption \n"
"   setWpaEncryptionModes           index               set WPA encryption per VAP index\n"
"   getWpaAuthenticationModes       index               get WPA authentication mode per VAP index\n"
"                                                       PSKAuthentication\n"
"                                                       EAPAuthentication\n"
"   WpaBasicAuthenticationModes     index               set WPA authentication mode per VAP index\n"
"   getPossibleChannels             index               get possible channel list per VAP with index\n"
"   getBasicDataTransmitRates       index               get basic data transmission rate\n"
"   setBasicDataTransmitRates       index val           set basic data transmit rate\n"
"   getOperationDataTransmitRates   index               get operational data transmission rate\n"
"   setOperationDataTransmitRates   index val           set operational data transmit rate\n"
"   getSsidAdvertisementEnabled     index               get SSID advertisement flag per VAP with index\n"
"   setSsidAdvertisementEnabled     index val[0/1]      set SSID advertisement flag per VAP with index\n"
"   getRadioEnabled                 index               get radio enable flag per VAP with index\n"
"   setRadioEnabled                 index val[0/1]      set radio enable flag per VAP with index\n"
"   getTransmitPowerSupported       index               get transmit power support list\n"
"   getTransmitPower                index               get transmit power per VAP with index\n"
"   setTransmitPower                index val[int]      set transmit power per VAP with index\n"
"   getAutoRateFallbackEnable       index               get auto rate fall back enable flag per VAP with index\n"
"   setAutoRateFallbackEnable       index val[0/1]      set auto rate fall back enable flag per VAP with index\n"
"   getBasicStats                   index               get network basic statistics per VAP with index\n"
"   getStats                        index               get network statistics per VAP with index\n"
"   getAssocDeviceNum               index               get association device number per VAP with index\n"
"   GetAssocDevice                  index val[int]      set get associated device information per VAP with index\n"
"   GetWepKey                       index               get WEP key value per VAP with index\n"
"   SetWepKey                       index val[int]      set WEP key value per VAP with index\n"
"   getPreSharedKey                 index               get pre-shared key value per VAP with index\n"
"   setPreSharedKey                 index val[int]      set pre-shared key value per VAP with index\n"
"   getKeyPassphrase                index               get pre-shared key passphrase value per VAP with index\n"
"   setKeyPassphrase                index val[int]      set pre-shared key passphrase value per VAP with index\n"
"   getWmmSupported                 index               get WMM support flag on VAP with index\n"
"   getWmmUapsdSupported            index               get WMM UAPSD support flag on VAP with index\n"
"   setWmmEnable                    index val[0/1]      set WMM support flag on VAP with index\n"
"   setWmmUapsdEnable               index val[0/1]      set WMM UAPSD support flag on VAP with index\n"
"   getWmmOgAifsn                   index class         get Arbitration Inter Frame Spacing per VAP with index\n"
"   setWmmOgAifsn                   index class val[int]set Arbitration Inter Frame Spacing per VAP with index\n"
"   getWmmOgEcwMin                  index class         get exponent of minimum Contention Window per VAP with index\n"
"   setWmmOgEcwMin                  index class val[int]set exponent of minimum Contention Window per VAP with index\n"
"   getWmmOgEcwMax                  index class         get exponent of maximum Contention Window per VAP with index\n"
"   setWmmOgEcwMax                  index class val[int]set exponent of maximum Contention Window per VAP with index\n"
"   getWmmOgTxOp                    index class         get transmit opportunity per VAP with index, in multiple of 32 microseconds\n"
"   setWmmOgTxop                    index class val[int]set transmit opportunity per VAP with index, in multiple of 32 microseconds\n"
"   getWmmOgAckPolicy               index class         get ack policy per VAP with index\n"
"   setWmmOgAckPolicy               index class val[int]set ack policy per VAP with index\n"
"   getWpsEnable                    index               get WPS enable flag per VAP with index\n"
"   setWpsEnable                    index val[0/1]      set WPS enable flag per VAP with index\n"
"   getWpsDeviceName                index               get user-friendly description of the device per VAP with index\n"
"   getWpsDevicePassword            index               get PIN code (AP PIN) per VAP with index\n"
"   setWpsDevicePassword            index val[int]      set PIN code (AP PIN) per VAP with index\n"
"   getWpsUuid                      index               get WPS uuid per VAP with index\n"
"   getWpsVersion                   index               get WPS Version\n"
"   getWpsConfigMethodsSupported    index               get WPS configuration methods supported by the device\n"
"                                                       Label,Display,PushButton,Keypad\n"
"   setWpsConfigMethodsEnabled      index val[string]   set WPS configuration methods that currently enabled by the device\n"
"   getWpsSetupLockedState          index               get Lock State of WPS\n"
"   GetWpsSetupLock                 index               get WPS lock value\n"
"   setWpsSetupLock                 index val[0/1]      set WPS lock value\n"
"   getWpsConfigurationState        index               get WPS configuration state\n"
"                                                       Not configured\n"
"                                                       Configured    \n"
"   setWpsPbcTrigger                index               WPS Method Push Button trigger \n"
"   setWpsEnrolleePin               index val[string]   WPS Enrollee Pin setup \n"
"   getSsidQosEnabled               radio val[0/1]      get SSID Based QoS feature enable\n"
"   setSsidQosEnabled               radio val[0/1]      set SSID Based QoS feaure enable\n"
"   getSsidQosLevel                 index               get SSID Based QoS feature Level\n"
"   setSsidQosLevel                 index val[BK/BE/VI/VO]  set SSID Based QoS feaure Level\n"
"   getNumberOfRadios                                   Returns the number of radios\n"
"   getBeaconInterval               index               get Beacon Interval\n"
"   setBeaconInterval               index val[int]      set Beacon Interval\n"
"   getDTIMInterval                 index               get DTIM Interval\n"
"   setDTIMInterval                 index val[int]      set DTIM Interval\n"
"   getCtsProtEnable                index               get CTS Protection enable\n"
"   setCtsProtEnable                index val[0/1]      set CTS Protection enable\n"
"   getObssCoexEnable               index               get Coexistence enable\n" 
"   setObssCoexEnable               index val[0/1]      set Coexistence enable\n" 
"   getMode                         index               get mode\n"
"   setMode                         index val[string]   set mode\n"
"   getFragThresh                   index               get Fragmentation Threshold\n"
"   setFragThresh                   index val[int]      set Fragmentation Threshold\n"
"   getRTSThresh                    index               get RTS Threshold\n"
"   setRTSThresh                    index val[int]      set RTS Threshold\n"
"   getChnlMode                     index               get Channel Mode\n"
"   setChnlMode                     index val[string]   set Channel Mode\n"
"   getIndexFromName                val[string]         get SSID index from SSID name\n"
"   setBridgeInfo                     index val[string]   set VAP Vlan Id\n"
"   setVlanId                        index bridge[string]  ip[string] subnet[string]  set VAP Bridge Id\n"
"   getApBridging                 index val[0/1]  get ap bridging enabled flag"
"   setApBridging                 index val[0/1]  set ap bridging enabled flag"
"   getStartMode                   gets cfg start mode, multi, multivlan, etc "
"   setStartMode                   mode[string] sets cfg start mode, multi, multivlan, etc "
"   factoryReset                                        reset wifi to Factory Defaults \n"
"   factoryRestart                                      restart wifi with Factory Defaults \n"
"   createCfgFile                                       recreate wifi configuration files without restarting radios \n";

int main(int argc,char **argv)
{
    if(argc <= 1) {
        printf("help\n");
        fprintf(stderr,"%s", commands_help);

        exit(-1);
    } 

    if(strstr(argv[1], "init")!=NULL) {
        return wlan_init();
    }
    if(strstr(argv[1], "reset")!=NULL) {
        return wlan_reset();
    }
    if(strstr(argv[1], "getNumberOfEntries")!=NULL) {
        int num;

        wlan_getNumberOfEntries(&num);
        printf("NumberOfEntries: %d\n",num);
    }
    if(strstr(argv[1], "getEnable")!=NULL) {
        int index = atoi(argv[2]);
        bool enable;

        wlan_getEnable(index, &enable);
        printf("index %d enable = %d\n",index, enable);

    }
    if(strstr(argv[1], "setEnable")!=NULL) {
        int index = atoi(argv[2]);
        bool enable = atoi(argv[3]);

        wlan_setEnable(index, enable);
        printf("set index %d enable = %d\n",index, enable);
    }
    if(strstr(argv[1], "getStatus")!=NULL) {
        int index = atoi(argv[2]);
        char status[64]; 

        wlan_getStatus(index, status);
        printf("index %d status %s\n",index, status);
    }
    if(strstr(argv[1], "getName")!=NULL) {
        int index = atoi(argv[2]);
        char name[64];        

        wlan_getName(index, name);
        printf("index %d name %s\n",index, name); 
    }
    if(strstr(argv[1], "getBSSID")!=NULL) {
        int index = atoi(argv[2]);
        char bssid[64];

        wlan_getBSSID(index, bssid);
        printf("index %d bssid %s\n",index, bssid); 
    }
    if(strstr(argv[1], "getMaxBitRate")!=NULL) {
        int index = atoi(argv[2]);
        char maxBitRate[128];

        wlan_getMaxBitRate(index, maxBitRate);
        printf("index %d maxBitRate %s\n",index, maxBitRate);
    }
    if(strstr(argv[1], "setMaxBitRate")!=NULL) {
        int index = atoi(argv[2]);
        char *maxBitRate = argv[3];

        wlan_setMaxBitRate(index, maxBitRate);
        printf("set index %d maxBitRate %s\n",index, maxBitRate);
    }
    if (strstr(argv[1], "getChannelsInUse") != NULL) {
        int index = atoi(argv[2]);
        char channels[512];
        wlan_getChannelsInUse(index, channels);
        printf("Called wlan_getChannelsInUse index %d returned %s\n", index, channels);
    }
    if(strstr(argv[1], "getChannel")!=NULL) {
        int index = atoi(argv[2]);
        unsigned int channel;

        wlan_getChannel(index, &channel);
        printf("index %d channel %d\n",index, channel);
        
    }
    if(strstr(argv[1], "setChannel")!=NULL) {
        int index = atoi(argv[2]);
        unsigned int channel = atoi(argv[3]);
        int ret;
            
        ret = wlan_setChannel(index, channel);

        if (ret == 0)
            printf("set index %d channel %d\n",index, channel);
        else 
            printf("VAP not enabled, set channel fail\n");
    }
    if(strstr(argv[1], "getAutoChannelEnable")!=NULL) {
        int index = atoi(argv[2]);
        bool enable;

        wlan_getAutoChannelEnable(index, &enable);
        printf("index %d autochannel %d\n",index, enable);
    }
    if(strstr(argv[1], "setAutoChannelEnable")!=NULL) {
        int index = atoi(argv[2]);
        bool enable = atoi(argv[3]);
    
        wlan_setAutoChannelEnable(index, enable);
        printf("set index %d autochannel %d\n",index, enable);
    }
    if(strstr(argv[1], "getSSID")!=NULL) {
        int index = atoi(argv[2]);
        char ssid[64];

        wlan_getSSID(index, ssid);
        printf("index %d ssid %s\n",index, ssid);
    }
    if(strstr(argv[1], "setSSID")!=NULL) {
        int index = atoi(argv[2]);
        char *ssid= argv[3];

        wlan_setSSID(index, ssid);
        printf("set index %d ssid %s\n",index, ssid);
    }
    if(strstr(argv[1], "getBeaconType")!=NULL) {
        int index = atoi(argv[2]);
        char beaconType[64];
        
        wlan_getBeaconType(index, beaconType);
        printf("index %d beacontype %s\n",index, beaconType);
    }
    if(strstr(argv[1], "setBeaconType")!=NULL) {
        int index = atoi(argv[2]);
        char *beaconType = argv[3];

        wlan_setBeaconType(index, beaconType);
        printf("set index %d beacontype %s\n",index, beaconType);
    }
// working to set ACL_MODE, but iwpriv athX get_maccmd does not know about it
    if(strstr(argv[1], "getMacAddressControlEnabled")!=NULL) {
        int index = atoi(argv[2]);
        bool enable;

        wlan_getMacAddressControlEnabled(index, &enable);
        printf("index %d acl %d\n",index, enable);
    }
// working to set ACL_MODE, but iwpriv athX get_maccmd does not know about it
    if(strstr(argv[1], "setMacAddressControlEnabled")!=NULL) {
        int index = atoi(argv[2]);
        bool enable = atoi(argv[3]);

        wlan_setMacAddressControlEnabled(index, enable);
        printf("set index %d acl %d\n",index, enable);
    }
    if(strstr(argv[1], "getStandard")!=NULL) {
        int index = atoi(argv[2]);
        char standard[64];
        bool gOnly;
        bool nOnly;
        bool acOnly;
        
        wlan_getStandard(index, standard, &gOnly, &nOnly, &acOnly);
        printf("index %d standard %s%s%s%s\n",index, standard, (gOnly==true) ? "(gOnly)":"", (nOnly==true) ? "(nOnly)":"",  (acOnly==true) ? "(acOnly)":"");
    }
    if(strstr(argv[1], "getWepKeyIndex")!=NULL) {
        int index = atoi(argv[2]);
        unsigned int keyIndex;

        wlan_getWepKeyIndex(index, &keyIndex);
        printf("index %d WepKeyIndex %d\n",index, keyIndex);
    } 
    if(strstr(argv[1], "setWepKeyIndex")!=NULL) {
        int index = atoi(argv[2]);
        unsigned int keyIndex = atoi(argv[3]);

        wlan_setWepKeyIndex(index, keyIndex);
        printf("set index %d WepKeyIndex %d\n",index, keyIndex);
    }
    if(strstr(argv[1], "getWepEncryptionLevel")!=NULL) {
        int index = atoi(argv[2]);
        char encLevel[64];

        wlan_getWepEncryptionLevel(index, encLevel);
        printf("index %d encLevel %s\n",index, encLevel);
    }
    if(strstr(argv[1], "getBasicEncryptionModes")!=NULL) {
        int index = atoi(argv[2]);
        char encModes[64];
        
        wlan_getBasicEncryptionModes(index, encModes);
        printf("index %d BasicEncModes %s\n",index, encModes);
    }
    if(strstr(argv[1], "setBasicEncryptionModes")!=NULL) {
        int index = atoi(argv[2]);
        char *encModes = argv[3];

        wlan_setBasicEncryptionModes(index, encModes);
        printf("set index %d BasicEncModes %s\n",index, encModes);
    }
    if(strstr(argv[1], "getBasicAuthenticationModes")!=NULL) {
        int index = atoi(argv[2]);
        char authMode[64];

        wlan_getBasicAuthenticationModes(index, authMode);
        printf("index %d BasicAuthModes %s\n",index, authMode);
    }
    if(strstr(argv[1], "setBasicAuthenticationModes")!=NULL) {
        int index = atoi(argv[2]);
        char *authMode= argv[3];

        wlan_setBasicAuthenticationModes(index, authMode);
        printf("set index %d BasicAuthModes %s\n",index, authMode);
    }
    if(strstr(argv[1], "getWpaEncryptionModes")!=NULL) {
        int index = atoi(argv[2]);
        char encModes[64];

        wlan_getWpaEncryptionModes(index, encModes);
        printf("index %d WpaEncModes %s\n",index, encModes);
        
    }
    if(strstr(argv[1], "setWpaEncryptionModes")!=NULL) {
        int index = atoi(argv[2]);
        char *encModes = argv[3];

        wlan_setWpaEncryptionModes(index, encModes);
        printf("set index %d WpaEncModes %s\n",index, encModes);

    }
    if(strstr(argv[1], "getWpaAuthenticationModes")!=NULL) {
        int index = atoi(argv[2]);
        char authMode[64];

        wlan_getWpaAuthenticationModes(index, authMode);
        printf("index %d WpsAuthModes %s\n",index, authMode);
    }
    if(strstr(argv[1], "WpaBasicAuthenticationModes")!=NULL) {
        int index = atoi(argv[2]);
        char *authMode = argv[3];

        wlan_WpaBasicAuthenticationModes(index, authMode);
        printf("set index %d WpsAuthModes %s\n",index, authMode);
    }
    if(strstr(argv[1], "getPossibleChannels")!=NULL) {
        int index = atoi(argv[2]);
        char channel[128];
        
        wlan_getPossibleChannels(index, channel);
        printf("index %d possible channel %s\n",index, channel);
    }
    if(strstr(argv[1], "getBasicDataTransmitRates")!=NULL) {
        int index = atoi(argv[2]);
        char txRate[128];

        wlan_getBasicDataTransmitRates(index, txRate);
        printf("index %d BasicTxRate %s\n",index, txRate);
    }  
    if(strstr(argv[1], "setBasicDataTransmitRates")!=NULL) {
        int index = atoi(argv[2]);
        char *txRate = argv[3];

        wlan_setBasicDataTransmitRates(index, txRate);
        printf("set index %d BasicTxRate %s\n",index, txRate);
    }
    if(strstr(argv[1], "getOperationDataTransmitRates")!=NULL) {
        int index = atoi(argv[2]);
        char txRate[128];
    
        wlan_getOperationaDataTransmitRates(index, txRate);
        printf("index %d OperationTxRate %s\n",index, txRate);
    }
    if(strstr(argv[1], "setOperationDataTransmitRates")!=NULL) {
        int index = atoi(argv[2]);
        char *txRate = argv[3];

        wlan_setOperationaDataTransmitRates(index, txRate);
        printf("set index %d OperationTxRate %s\n",index, txRate);
    }
    if(strstr(argv[1], "getSsidAdvertisementEnabled")!=NULL) {
        int index = atoi(argv[2]);
        bool enable;

        wlan_getSsidAdvertisementEnabled(index, &enable);
        printf("index %d SsidAdvertisement %d\n",index, enable);
    }
    if(strstr(argv[1], "setSsidAdvertisementEnabled")!=NULL) {
        int index = atoi(argv[2]);
        bool enable = atoi(argv[3]);

        wlan_setSsidAdvertisementEnabled(index, enable);
        printf("set index %d SsidAdvertisement %d\n",index, enable);
    }
    if(strstr(argv[1], "getRadioEnabled")!=NULL) {
        int index = atoi(argv[2]);
        bool enable;

        wlan_getRadioEnabled(index, &enable);
        printf("index %d radioenable %d\n",index, enable);
    }
    if(strstr(argv[1], "setRadioEnabled")!=NULL) {
        int index = atoi(argv[2]);
        bool enable = atoi(argv[3]);

        wlan_setRadioEnabled(index, enable);
        printf("set index %d radioenable %d\n",index, enable);
    }
    if(strstr(argv[1], "getTransmitPowerSupported")!=NULL) {
        int index = atoi(argv[2]);
        char power[128];

        wlan_getTransmitPowerSupported(index, power);
        printf("index %d supported TxPwr %s\n",index, power);
    }
    if(strstr(argv[1], "getTransmitPower")!=NULL) {
        int index = atoi(argv[2]);
        unsigned int power;

        wlan_getTransmitPower(index, &power);
        printf("index %d TxPwr %d\n",index, power);
    }
    if(strstr(argv[1], "setTransmitPower")!=NULL) {
        int index = atoi(argv[2]);
        unsigned int power = atoi(argv[3]);

        wlan_setTransmitPower(index, power);
        printf("set index %d TxPwr %d\n",index, power);
    }
    if(strstr(argv[1], "getAutoRateFallbackEnable")!=NULL) {
        int index = atoi(argv[2]);
        bool enable;
    
        wlan_getAutoRateFallbackEnable(index, &enable);
        printf("index %d AutoRate %d\n",index, enable);
    }
    if(strstr(argv[1], "setAutoRateFallbackEnable")!=NULL) {
        int index = atoi(argv[2]);
        bool enable = atoi(argv[3]);

        wlan_setAutoRateFallbackEnable(index, enable);
        printf("set index %d AutoRate %d\n",index, enable);
    }
    if(strstr(argv[1], "getBasicStats")!=NULL) {
        int index = atoi(argv[2]);
        WLAN_basicStats stats;

        wlan_getBasicStats(index, &stats);
        printf("index %d BasicStats\n", index);
        printf("\ttotalBytesSent: %d\n", stats.Wlan_totalBytesSent);
        printf("\ttotalBytesReceived: %d\n", stats.Wlan_totalBytesReceived);
        printf("\ttotalPacketsSent: %d\n", stats.Wlan_totalPacketsSent);
        printf("\ttotalPacketsReceived: %d\n", stats.Wlan_totalPacketsReceived);
        printf("\ttotalAssociations: %d\n", stats.Wlan_totalAssociations);       
    }
    if(strstr(argv[1], "getStats")!=NULL) {
        int index = atoi(argv[2]);
        WLAN_stats stats;

        wlan_getStats(index, &stats);
        printf("index %lu Stats\n", index);
		printf("\tErrorSend: %lu\n", stats.Wlan_ErrorsSent);
		printf("\tErrorReceived: %lu\n", stats.Wlan_ErrorReceived);
		printf("\tUnicastPacketsSend: %lu\n", stats.Wlan_UnicastPacketsSent);
		printf("\tUnicastPacketsReceived: %lu\n", stats.Wlan_UnicastPacketsReceived);
		printf("\tDiscardPacketsSent: %lu\n", stats.Wlan_DiscardPacketsSent);		 
		printf("\tDiscardPacketsReceived: %lu\n", stats.Wlan_DiscardPacketsReceived);		 
		printf("\tMulticastPacketsSent: %lu\n", stats.Wlan_MulticastPacketsSent);		 
		printf("\tMulticastPacketsReceived: %lu\n", stats.Wlan_MulticastPacketsReceived);		 
		printf("\tBroadcastPacketsSent: %lu\n", stats.Wlan_BroadcastPacketsSent);		 
		printf("\tBroadcastPacketsReceived: %lu\n", stats.Wlan_BroadcastPacketsReceived);		 
		printf("\tWlan_UnknownProtoPacketsReceived: %lu\n", stats.Wlan_UnknownProtPacketsReceived);		 
    }
	
    if(strstr(argv[1], "getAssocDeviceNum")!=NULL) {
        int index = atoi(argv[2]);
        unsigned int num;
        
        wlan_getAssocDevicesNum(index, &num);
        printf("index %d AssocNum %d\n",index, num);
    }
    if(strstr(argv[1], "GetAssocDevice")!=NULL) {
        int index = atoi(argv[2]);
        int devIndex = atoi(argv[3]);
        WLAN_Device dev;
    
        wlan_getAssocDevice(index, devIndex, &dev);
        printf("index %d devIndex %d info:\n",index, devIndex);
        printf("%02x:%02x:%02x:%02x:%02x:%02x (%d)\n",dev.Wlan_devMacAddress[0], dev.Wlan_devMacAddress[1],
        dev.Wlan_devMacAddress[2], dev.Wlan_devMacAddress[3],
        dev.Wlan_devMacAddress[4], dev.Wlan_devMacAddress[5],
        dev.Wlan_devAssociatedDeviceAuthenticationState);
    }
    if (strstr(argv[1], "getAllAssocDevices")!=NULL) {
        int index = atoi(argv[2]);
        int numDevices;
        WLAN_Device **dev;  

        wlan_getAllAssocDevices(index, &numDevices, &dev);
        printf("index %d numDevies %d info:\n",index, numDevices);
        int i = 0;
        for (i = 0; i < numDevices; i++) {
            printf("%02x:%02x:%02x:%02x:%02x:%02x (%d)\n",
                   dev[i]->Wlan_devMacAddress[0], dev[i]->Wlan_devMacAddress[1],
                   dev[i]->Wlan_devMacAddress[2], dev[i]->Wlan_devMacAddress[3],
                   dev[i]->Wlan_devMacAddress[4], dev[i]->Wlan_devMacAddress[5],
                   dev[i]->Wlan_devAssociatedDeviceAuthenticationState);
        }
    }
    if(strstr(argv[1], "GetWepKey")!=NULL) {
        int index = atoi(argv[2]);
        char key[128];
        int keyIndex = atoi(argv[3]);

        wlan_getWepKey(index, keyIndex, key);
        printf("index %d wepKey[%d] %s\n", index, keyIndex, key);
    }
    if(strstr(argv[1], "SetWepKey")!=NULL) {
        int index = atoi(argv[2]);
        int keyIndex = atoi(argv[3]);
        char *key = argv[4];

        wlan_setWepKey(index, keyIndex, key);
        printf("set index %d wepKey[%d] %s\n", index, keyIndex, key);
    }
    if(strstr(argv[1], "getPreSharedKey")!=NULL) {
        int index = atoi(argv[2]);
        char key[128];

        wlan_getPreSharedKey(index, key);
        printf("index %d PSKkey %s\n", index, key);
    }
    if(strstr(argv[1], "setPreSharedKey")!=NULL) {
        int index = atoi(argv[2]);
        char *key = argv[3];
        int keyIndex = 0;

        wlan_setPreSharedKey(index, key);
        printf("set index %d PSKkey %s\n", index, key);
    }
    if(strstr(argv[1], "getKeyPassphrase")!=NULL) {
        int index = atoi(argv[2]);
        char psk[128];

        wlan_getKeyPassphrase(index, psk);
        printf("index %d pskpassphrase %s\n", index, psk);
    }
    if(strstr(argv[1], "setKeyPassphrase")!=NULL) {
        int index = atoi(argv[2]);
        char *psk = argv[3];

        wlan_setKeyPassphrase(index, psk);
        printf("set index %d pskpassphrase %s\n", index, psk);
    }

    /* WMM */
    if(strstr(argv[1], "getWmmSupported")!=NULL) {
        int index = atoi(argv[2]);
        bool support;

        wlan_getWmmSupported(index, &support);
        printf("index %d WmmSupported %d\n",index, support);
    } 
    if(strstr(argv[1], "getWmmUapsdSupported")!=NULL) {
        int index = atoi(argv[2]);
        bool support;

        wlan_getWmmUapsdSupported(index, &support);
        printf("index %d WmmUapsdSupported %d\n",index, support);
    }
    if(strstr(argv[1], "setWmmEnable")!=NULL) {
        int index = atoi(argv[2]);
        bool enable = atoi(argv[3]);
    
        wlan_setWmmEnable(index, enable);
        printf("set index %d WmmEnable %d\n",index, enable);
    }
    if(strstr(argv[1], "setWmmUapsdEnable")!=NULL) {
        int index = atoi(argv[2]);
        bool enable = atoi(argv[3]);

        wlan_setWmmUapsdEnable(index, enable);
        printf("set index %d WmmUapsdEnable %d\n",index, enable);
    }    

    /* Packets transmit from AP towards stations, class stands for AC */
    if(strstr(argv[1], "getWmmOgAifsn")!=NULL) {
        int index = atoi(argv[2]);
        int class = atoi(argv[3]);
        unsigned int aifsn;

        wlan_getWmmOgAifsn(index, class, &aifsn);
        printf("index %d class %d OgAifsn %d\n",index, class, aifsn);
    }
    if(strstr(argv[1], "setWmmOgAifsn")!=NULL) {
        int index = atoi(argv[2]);
        int class = atoi(argv[3]);
        unsigned int aifsn = atoi(argv[4]);

        wlan_setWmmOgAifsn(index, class, aifsn);
        printf("set index %d class %d OgAifsn %d\n",index, class, aifsn);
    }
    if(strstr(argv[1], "getWmmOgEcwMin")!=NULL) {
        int index = atoi(argv[2]);
        int class = atoi(argv[3]);
        unsigned int ecwMin;

        wlan_getWmmOgEcwMin(index, class, &ecwMin);
        printf("index %d class %d OgEcwMin %d\n",index, class, ecwMin);
    }
    if(strstr(argv[1], "setWmmOgEcwMin")!=NULL) {
        int index = atoi(argv[2]);
        int class = atoi(argv[3]);
        unsigned int ecwMin = atoi(argv[4]);

        wlan_setWmmOgEcwMin(index, class, ecwMin);
        printf("set index %d class %d OgEcwMin %d\n",index, class, ecwMin);
    }

    if(strstr(argv[1], "getWmmOgEcwMax")!=NULL) {
        int index = atoi(argv[2]);
        int class = atoi(argv[3]);
        unsigned int ecwMax;

        wlan_getWmmOgEcwMin(index, class, &ecwMax);
        printf("index %d class %d OgEcwMax %d\n",index, class, ecwMax);
    }

    if(strstr(argv[1], "setWmmOgEcwMax")!=NULL) {
        int index = atoi(argv[2]);
        int class = atoi(argv[3]);
        unsigned int ecwMax = atoi(argv[4]);

        wlan_setWmmOgEcwMin(index, class, ecwMax);
        printf("set index %d class %d OgEcwMax %d\n",index, class, ecwMax);
    }

    /* WMM txop */
    if(strstr(argv[1], "getWmmOgTxOp")!=NULL) {
        int index = atoi(argv[2]);
        int class = atoi(argv[3]);
        unsigned int txOp;

        wlan_getWmmOgTxOp(index, class, &txOp);
        printf("index %d class %d OgTxOp %d\n",index, class, txOp);
    }

    if(strstr(argv[1], "setWmmOgTxop")!=NULL) {
        int index = atoi(argv[2]);
        int class = atoi(argv[3]);
        unsigned int txOp = atoi(argv[4]);

        wlan_setWmmOgTxOp(index, class, txOp);
        printf("set index %d class %d OgTxOp %d\n",index, class, txOp);
    }

    /* WMM Ack Policy */
    if(strstr(argv[1], "getWmmOgAckPolicy")!=NULL) {
        int index = atoi(argv[2]);
        int class = atoi(argv[3]);
        unsigned int ackPolicy;

        wlan_getWmmOgAckPolicy(index, class, &ackPolicy);
        printf("index %d class %d OgAckPolicy %d\n",index, class, ackPolicy);
    }

    if(strstr(argv[1], "setWmmOgAckPolicy")!=NULL) {
        int index = atoi(argv[2]);
        int class = atoi(argv[3]);
        unsigned int ackPolicy = atoi(argv[4]);

        wlan_setWmmOgAckPolicy(index, class, ackPolicy);
        printf("set index %d class %d OgAckPolicy %d\n",index, class, ackPolicy);
    }

    if(strstr(argv[1], "getWpsEnable")!=NULL) {
        int index = atoi(argv[2]);
        int enable;

        wlan_getWpsEnable(index, &enable);
        printf("index %d WpsEnable %d\n",index, enable);
    }
    if(strstr(argv[1], "setWpsEnable")!=NULL) {
        int index = atoi(argv[2]);
        bool enable = atoi(argv[3]);

        wlan_setWpsEnable(index, enable);
        printf("set index %d WpsEnable %d\n",index, enable);
    }      
    if(strstr(argv[1], "getWpsDeviceName")!=NULL) {
        int index = atoi(argv[2]);
        char devName[128];

        wlan_getWpsDeviceName(index, devName);
        printf("index %d WpsDeviceName %s\n",index, devName);
    }
    if(strstr(argv[1], "getWpsDevicePassword")!=NULL) {
        int index = atoi(argv[2]);
        unsigned int devpwd;

        wlan_getWpsDevicePassword(index, &devpwd);
        printf("index %d WpsDevicePassword %d\n",index, devpwd);
    }
    if(strstr(argv[1], "setWpsDevicePassword")!=NULL) {
        int index = atoi(argv[2]);
        unsigned int devpwd = atoi(argv[3]);

        wlan_setWpsDevicePassword(index, devpwd);
        printf("set index %d WpsDevicePassword %d\n",index, devpwd);
    }
    if(strstr(argv[1], "getWpsUuid")!=NULL) {
        int index = atoi(argv[2]);
        char uuid[128];

        wlan_getWpsUuid(index, uuid);
        printf("index %d WpsUuid %s\n",index, uuid);
    }
    if(strstr(argv[1], "getWpsVersion")!=NULL) {
        int index = atoi(argv[2]);
        unsigned int version;

        wlan_getWpsVersion(index, &version);
        printf("index %d WpsVersion %d\n",index, version);    
    }
    if(strstr(argv[1], "getWpsConfigMethodsSupported")!=NULL) {
        int index = atoi(argv[2]);
        char method[128];

        wlan_getWpsConfigMethodsSupported(index, method);
        printf("index %d WpsConfigMethodsSupported %s\n",index, method);
    }
    if(strstr(argv[1], "getWpsConfigMethodsEnabled")!=NULL) {
        int index = atoi(argv[2]);
        char method[128];

        wlan_getWpsConfigMethodsEnabled(index, method);
        printf("index %d WpsConfigMethodsEnabled %s\n",index, method);
    }
    if(strstr(argv[1], "setWpsConfigMethodsEnabled")!=NULL) {
        int index = atoi(argv[2]);
        char *method = argv[3];

        wlan_setWpsConfigMethodsEnabled(index, method);
        printf("set index %d WpsConfigMethodsEnabled %s\n",index, method);
    }
    if(strstr(argv[1], "getWpsSetupLockedState")!=NULL) {
        int index = atoi(argv[2]);
        char state[64];

        wlan_getWpsSetupLockedState(index, state);
        printf("index %d WpsSetupLockedState %s\n",index, state);
    }
    if(strstr(argv[1], "GetWpsSetupLock")!=NULL) {
        int index = atoi(argv[2]);
        bool state;

        wlan_getWpsSetupLock(index, &state);
        printf("index %d WpsSetupLock %d\n",index, state);
    }
    if(strstr(argv[1], "setWpsSetupLock")!=NULL) {
        int index = atoi(argv[2]);
        bool state = atoi(argv[3]);

        wlan_setWpsSetupLock(index, state);
        printf("set index %d WpsSetupLock %d\n",index, state);
    } 

    if(strstr(argv[1], "getWpsConfigurationState")!=NULL) {
        int index = atoi(argv[2]);
        char state[64];

        wlan_getWpsConfigurationState(index, state);
        printf("set index %d getWpsConfigurationState %s\n",index, state);
    } 

    if(strstr(argv[1], "setWpsPbcTrigger")!=NULL) {
        int ret;
        int index = atoi(argv[2]);

        ret = wlan_setWpsPbcTrigger(index);
        printf("set WPS Trigger, index %d, %s\n",index, ret? "Error": "SUCCESS");
    }

    if(strstr(argv[1], "setWpsEnrolleePin")!=NULL) {
        int ret;
        int index = atoi(argv[2]);
        char *pin = argv[3];

        ret = wlan_setWpsEnrolleePin(index, pin);
        printf("set WPS Enrolee Pin, index %d, pin = %s, %s\n",index, pin, ret? "Error": "SUCCESS");
    }

    if(strstr(argv[1], "getSsidQosEnabled")!=NULL) {
        int radio = atoi(argv[2]);
        bool enable;

        wlan_getSsidQosEnabled(radio, &enable);
        printf("get radio %d getSsidQosEnabled %d\n",radio, enable);
    } 

    if(strstr(argv[1], "setSsidQosEnabled")!=NULL) {
        int radio = atoi(argv[2]);
        bool enable = atoi(argv[3]);

        wlan_setSsidQosEnabled(radio, enable);
        printf("set radio %d setSsidQosEnabled %d\n",radio, enable);
    } 

    if(strstr(argv[1], "getSsidQosLevel")!=NULL) {
        int index = atoi(argv[2]);
        char lvl[64];

        wlan_getSsidQosLevel(index, lvl);
        printf("get index %d getSsidQosLevel %s\n",index, lvl);
    } 

    if(strstr(argv[1], "setSsidQosLevel")!=NULL) {
        int index = atoi(argv[2]);
        char *lvl = argv[3];

        wlan_setSsidQosLevel(index, lvl);
        printf("set index %d setSsidQosLevel %s\n",index, lvl);
    } 

#ifdef USGV2
    if (strstr(argv[1], "getNumberOfRadios") != NULL) {
       int numRadios = 0;
       wlan_getNumberOfRadios(&numRadios);
       printf("get number of radios %d\n", numRadios);
    }

    if (strstr(argv[1], "getBeaconInterval") != NULL) {
        int index = atoi(argv[2]);
        int bintval = 0;
        wlan_getBeaconInterval(index, &bintval);
        printf("get index %d beacon Interval %d\n", index, bintval );
    }

    if (strstr(argv[1], "setBeaconInterval") != NULL) {
        int index = atoi(argv[2]);
        int bintval = atoi(argv[3]);
        wlan_setBeaconInterval(index, bintval);
        printf("set index %d beacon Interval %d\n",  index, bintval );
    }

    if (strstr(argv[1], "getDTIMInterval") != NULL) {
        int index = atoi(argv[2]);
        int dtimPeriod = 0; 
        wlan_getDTIMInterval(index, &dtimPeriod);
        printf("get index %d DTIMInterval %d\n",  index, dtimPeriod );
    }

    if (strstr(argv[1], "setDTIMInterval") != NULL) {
        int index = atoi(argv[2]);
        int dtimPeriod = atoi(argv[3]); 
        wlan_setDTIMInterval(index, dtimPeriod);
        printf("set index %d DTIMInterval %d\n",  index, dtimPeriod );
    }

    if (strstr(argv[1], "getCtsProtEnable") != NULL) {
        int index = atoi(argv[2]);
        bool enabled = false;
        wlan_getCtsProtectionEnable(index, &enabled);
        printf("get index %d CtsProtectionEnable %s\n",  index, (enabled == true) ? "TRUE" : "FALSE" );
    }

    if (strstr(argv[1], "setCtsProtEnable") != NULL) {
        int index = atoi(argv[2]);
        bool enabled = atoi(argv[3]); 
        wlan_setCtsProtectionEnable(index, enabled);
        printf("set index %d CtsProtectionEnable %s\n",  index, (enabled == true) ? "TRUE" : "FALSE" );
    }

    if (strstr(argv[1], "getObssCoexEnable") != NULL) {
        int index = atoi(argv[2]);
        bool enabled = false;
        wlan_getObssCoexistenceEnable(index, &enabled);
        printf("get index %d ObssCoexistenceEnable %s\n",  index, (enabled == true) ? "TRUE" : "FALSE" );
    }

    if (strstr(argv[1], "setObssCoexEnable") != NULL) {
        int index = atoi(argv[2]);
        bool enabled = atoi(argv[3]);
        wlan_setObssCoexistenceEnable(index, enabled);
        printf("set index %d ObssCoexistenceEnable %s\n",  index, (enabled == true) ? "TRUE" : "FALSE" );
    }

    if (strstr(argv[1], "getMode") != NULL) {
        int index = atoi(argv[2]);
        char mode[64];
        wlan_getMode(index, mode);
        printf("get index %d Mode %s\n",  index, mode );
    }

    if (strstr(argv[1], "setMode") != NULL) {
        int index = atoi(argv[2]);
        char *mode = argv[3];
        wlan_setMode(index, mode);
        printf("set index %d Mode %s\n",  index, mode );
    }

// working
    if (strstr(argv[1], "getFragThresh") != NULL) {
        int index = atoi(argv[2]);
        unsigned int threshold;
        wlan_getFragThresh(index, &threshold);
        printf("get index %d FragThresh %d\n",  index, threshold );
    }

// working
    if (strstr(argv[1], "setFragThresh") != NULL) {
        int index = atoi(argv[2]);
        unsigned int threshold = atoi(argv[3]);
        wlan_setFragThresh(index, threshold);
        printf("set index %d FragThresh %d\n",  index, threshold );
    }
 
// working
    if (strstr(argv[1], "getRTSThresh") != NULL) {
        int index = atoi(argv[2]);
        unsigned int threshold;
        wlan_getRTSThresh(index, &threshold);
        printf("get index %d RTSThresh %d\n",  index, threshold );
    }

// working
    if (strstr(argv[1], "setRTSThresh") != NULL) {
        int index = atoi(argv[2]);
        unsigned int threshold  = atoi(argv[3]);
        wlan_setRTSThresh(index, threshold);
        printf("set index %d RTSThresh %d\n",  index, threshold );
    }

// working
    if (strstr(argv[1], "getChnlMode") != NULL) {
        int index = atoi(argv[2]);
        char chnMode[64];
        wlan_getChannelMode(index, chnMode);
        printf("get index %d ChannelMode %s\n",  index, chnMode );
    }

// working
    if (strstr(argv[1], "setChnlMode") != NULL) {
        int index = atoi(argv[2]);
        bool gOnlyFlag = atoi(argv[4]);
        bool nOnlyFlag = atoi(argv[4]);
        bool acOnlyFlag = atoi(argv[4]);
        wlan_setChannelMode(index, argv[3], gOnlyFlag, nOnlyFlag, acOnlyFlag);
        printf("set index %d ChannelMode %s (%s%s%s)  \n",  index, argv[3], (gOnlyFlag==true) ? "-Only" : "", (nOnlyFlag==true) ? "-Only" : "", (acOnlyFlag==true) ? "-Only" : "" );
    }

// working
    if (strstr(argv[1], "getIndexFromName") != NULL) {
        int index;
        wlan_getIndexFromName(argv[2], &index );
        printf("get index for SSID %s = %d \n", argv[2],  index );
    }

// not working
    if (strstr(argv[1], "getAclMode") != NULL) {
        int index = atoi(argv[2]);
        int mode;
        wlan_getMacAddressControlMode(index, &mode);
        printf("get index %d MacAddress Control Mode %s\n",  index, mode );
    }

    if (strstr(argv[1], "setAclMode") != NULL) {
        int index = atoi(argv[2]);
        int mode = atoi(argv[3]);
        wlan_setMacAddressControlMode(index, mode);
        printf("set index %d MacAddress Control Mode %d\n",  index, mode );
    }

    if (strstr(argv[1], "getFrequency") != NULL) {
        int index = atoi(argv[2]);
        char freq[64];
        wlan_getFrequency(index, freq);
        printf("get index %d Frequency %s\n",  index, freq );
    }

    if (strstr(argv[1], "setFrequency") != NULL) {
        int index = atoi(argv[2]);
        wlan_setFrequency(index, argv[3]);
        printf("set index %d Frequency %d\n",  index, argv[3] );
    }

    if (strstr(argv[1], "getMaxStations") != NULL) {
        int index = atoi(argv[2]);
        int stations;
        wlan_getMaxStations(index, &stations);
        printf("get index %d Max Stations %d\n",  index, stations );
    }

    if (strstr(argv[1], "setMaxStations") != NULL) {
        int index = atoi(argv[2]);
        int stations = atoi(argv[3]);
        wlan_setMaxStations(index, stations);
        printf("set index %d Max Stations %d\n",  index, stations );
    }

    if (strstr(argv[1], "getCountryCode") != NULL) {
        int index = atoi(argv[2]);
        char code[64];
        wlan_getCountryCode(index, code);
        printf("get index %d Country Code %s\n",  index, code );
    }

    if (strstr(argv[1], "setCountryCode") != NULL) {
        int index = atoi(argv[2]);
        wlan_setCountryCode(index, argv[3]);
        printf("set index %d Country Code %d\n",  index, argv[3] );
    }

    if (strstr(argv[1], "getMacAddressControlMode") != NULL) {
        int index = atoi(argv[2]);
        int mode;
        wlan_getMacAddressControlMode(index, &mode);
        printf("get index %d MacAddress Control Mode %d\n",  index, mode );
    }

    if (strstr(argv[1], "setMacAddressControlMode") != NULL) {
        int index = atoi(argv[2]);
        int mode = atoi(argv[3]);
        wlan_setMacAddressControlMode(index, mode);
        printf("set index %d MacAddress Control Mode %d\n",  index, mode );
    }

    if (strstr(argv[1], "getAclNumDevices") != NULL) {
        int index = atoi(argv[2]);
        unsigned int numDevices;
        wlan_getAclDeviceNum(index, &numDevices);
        printf("get index %d Acl num devices %d\n",  index, numDevices );
    }

    if (strstr(argv[1], "getAclDevice") != NULL) {
        int index = atoi(argv[2]);
        int devIndex = atoi(argv[3]);
        char device[64];
        wlan_getAclDevice(index, devIndex, device);
        printf("index %d get Acl Device [%d] %s\n",  index, devIndex, device );
    }

    if (strstr(argv[1], "addAclDevice") != NULL) {
        int index = atoi(argv[2]);
        wlan_addAclDevice(index, argv[3]);
        printf("index %d add Acl Device %s\n",  index, argv[3] );
    }

    if (strstr(argv[1], "delAclDevice") != NULL) {
        int index = atoi(argv[2]);
        wlan_delAclDevice(index, argv[3]);
        printf("index %d delete Acl Device %s\n",  index, argv[3] );
    }

    if (strstr(argv[1], "factoryReset") != NULL) {
        wlan_factoryReset();
        printf("Reset Wifi to Factory Fresh Defaults\n");
    }

    if (strstr(argv[1], "factoryRestart") != NULL) {
        wlan_factoryRestart();
        printf("Restart Wifi to Factory Fresh Defaults\n");
    }

    if (strstr(argv[1], "createCfgFile") != NULL) {
        wlan_createCfgFile();
        printf("Recreate Wifi configuration files without restarting radios \n");
    }

    if (strstr(argv[1], "kickAcl") != NULL) {
        int index = atoi(argv[2]);
        bool denyList = atoi(argv[3]);
        wlan_kickAclAssocDevices(index, denyList);
        printf("Called wlan_kickAclAssocDevices \n");
    }

    if (strstr(argv[1], "setVlanId") != NULL) {
        int index = atoi(argv[2]);
        int vlan = atoi(argv[3]);
        wlan_setVlanId(index, vlan);
        printf("Called wlan_setVlanId \n");
    }

    if (strstr(argv[1], "setBridgeInfo") != NULL) {
        int index = atoi(argv[2]);
        wlan_setBridgeInfo(index, argv[3], argv[4], argv[5]);
        printf("Called wlan_setBridgeInfo \n");
    }

    if (strstr(argv[1], "getStartMode") != NULL) {
        char mode[32];
        wlan_getStartMode(mode);
        printf("Called wlan_getStartMode returned mode = %s \n", mode);
    }

    if (strstr(argv[1], "setStartMode") != NULL) {
        wlan_setStartMode(argv[2]);
        printf("Called wlan_setStartMode returned mode = %s \n", argv[2]);
    }
    
    if (strstr(argv[1], "getApBridging") != NULL) {
        int index = atoi(argv[2]);
        char mode[32];
        wlan_getApBridging(index, mode);
        printf("Called wlan_getApBridging returned mode = %s \n", mode);
    }

    if (strstr(argv[1], "setApBridging") != NULL) {
        int index = atoi(argv[2]);
        bool  enabled = atoi(argv[3]);
        wlan_setApBridging(index, enabled);
        printf("Called wlan_setApBridging index %d to %s \n", index, (enabled == true) ? "TRUE" : "FALSE" );
    }

    if(strstr(argv[1], "getRouterEnable")!=NULL) {
        int index = atoi(argv[2]);
        bool enable;

        wlan_getRouterEnable(index, &enable);
        printf("index %d enable = %d\n",index, enable);

    }

    if(strstr(argv[1], "setRouterEnable")!=NULL) {
        int index = atoi(argv[2]);
        bool enable = atoi(argv[3]);

        wlan_setRouterEnable(index, enable);
        printf("set index %d enable = %d\n",index, enable);
    }
#endif

    return 0;
}
#endif
