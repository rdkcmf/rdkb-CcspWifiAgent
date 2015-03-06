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

/**************************************************************************

    module: cosa_wifi_dml.c

        For COSA Data Model Library Development

    -------------------------------------------------------------------

    copyright:

        Cisco Systems, Inc.
        All Rights Reserved.

    -------------------------------------------------------------------

    description:

        This file implementes back-end apis for the COSA Data Model Library

        *  CosaWifiCreate
        *  CosaWifiInitialize
        *  CosaWifiRemove
    -------------------------------------------------------------------

    environment:

        platform independent

    -------------------------------------------------------------------

    author:

        Richard Yang

    -------------------------------------------------------------------

    revision:

        01/11/2011    initial revision.

**************************************************************************/

#include "cosa_apis.h"
#include "cosa_wifi_apis.h"
#include "cosa_wifi_internal.h"
#include "plugin_main_apis.h"

extern void* g_pDslhDmlAgent;

/**********************************************************************

    caller:     owner of the object

    prototype:

        ANSC_HANDLE
        CosaWifiCreate
            (
            );

    description:

        This function constructs cosa wifi object and return handle.

    argument:  

    return:     newly created wifi object.

**********************************************************************/

ANSC_HANDLE
CosaWifiCreate
    (
        VOID
    )
{
    ANSC_STATUS                     returnStatus = ANSC_STATUS_SUCCESS;
	PCOSA_DATAMODEL_WIFI            pMyObject    = (PCOSA_DATAMODEL_WIFI)NULL;

    /*
     * We create object by first allocating memory for holding the variables and member functions.
     */
    pMyObject = (PCOSA_DATAMODEL_WIFI)AnscAllocateMemory(sizeof(COSA_DATAMODEL_WIFI));

    if ( !pMyObject )
    {
        return  (ANSC_HANDLE)NULL;
    }

    /*
     * Initialize the common variables and functions for a container object.
     */
    pMyObject->Oid               = COSA_DATAMODEL_WIFI_OID;
    pMyObject->Create            = CosaWifiCreate;
    pMyObject->Remove            = CosaWifiRemove;
    pMyObject->Initialize        = CosaWifiInitialize;

    pMyObject->Initialize   ((ANSC_HANDLE)pMyObject);

    return  (ANSC_HANDLE)pMyObject;
}

/**********************************************************************

    caller:     self

    prototype:

        ANSC_STATUS
        CosaWifiInitialize
            (
                ANSC_HANDLE                 hThisObject
            );

    description:

        This function initiate  cosa wifi object and return handle.

    argument:	ANSC_HANDLE                 hThisObject
            This handle is actually the pointer of this object
            itself.

    return:     operation status.

**********************************************************************/

ANSC_STATUS
CosaWifiInitialize
    (
        ANSC_HANDLE                 hThisObject
    )
{
    ANSC_STATUS                     returnStatus        = ANSC_STATUS_SUCCESS;
    PCOSA_DATAMODEL_WIFI            pMyObject           = (PCOSA_DATAMODEL_WIFI)hThisObject;
    ULONG                           uIndex              = 0; 
    ULONG                           uMacFiltIdx         = 0; 
    ULONG                           uSsidCount          = 0;
    ULONG                           uApCount            = 0;    
    ULONG                           uMacFiltCount       = 0;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoCOSA     = (PPOAM_IREP_FOLDER_OBJECT )NULL;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoWifi     = (PPOAM_IREP_FOLDER_OBJECT )NULL;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoWifiSsid = (PPOAM_IREP_FOLDER_OBJECT )NULL;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoWifiAP   = (PPOAM_IREP_FOLDER_OBJECT )NULL;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoMacFilt  = (PPOAM_IREP_FOLDER_OBJECT )NULL;
    PCOSA_DML_WIFI_RADIO            pWifiRadio          = NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid           = (PCOSA_DML_WIFI_SSID      )NULL;
    PCOSA_DML_WIFI_AP               pWifiAp             = (PCOSA_DML_WIFI_AP        )NULL;        
    PCOSA_DML_WIFI_AP_MAC_FILTER    pMacFilt            = (PCOSA_DML_WIFI_AP_MAC_FILTER        )NULL;        
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj            = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pMacFiltLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PSLAP_VARIABLE                  pSlapVariable       = (PSLAP_VARIABLE           )NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntry         = (PSINGLE_LINK_ENTRY       )NULL;
    
    PCOSA_PLUGIN_INFO               pPluginInfo         = (PCOSA_PLUGIN_INFO        )g_pCosaBEManager->hCosaPluginInfo;
    PSLAP_OBJECT_DESCRIPTOR         pObjDescriptor      = (PSLAP_OBJECT_DESCRIPTOR  )NULL;
    COSAGetHandleProc               pProc               = (COSAGetHandleProc        )NULL;
    /*ULONG                           ulRole              = LPC_ROLE_NONE;*/
    /*PPOAM_COSAWIFIDM_OBJECT*/ANSC_HANDLE         pPoamWiFiDm         = (/*PPOAM_COSAWIFIDM_OBJECT*/ANSC_HANDLE  )NULL;
    /*PSLAP_COSAWIFIDM_OBJECT*/ANSC_HANDLE         pSlapWifiDm         = (/*PSLAP_COSAWIFIDM_OBJECT*/ANSC_HANDLE  )NULL;

#if 0
    pProc = (COSAGetHandleProc)pPluginInfo->AcquireFunction("COSAGetLPCRole");
    
    if (pProc)
    {
        ulRole = (ULONG)(*pProc)();
        CcspTraceWarning(("CosaWifiInitialize - LPC role is %d...\n", ulRole));
    }
    
#ifdef _COSA_SIM_

    if ( ulRole == LPC_ROLE_PARTY ) 
    {
        CcspTraceWarning(("CosaWifiInitialize - AcquireFunction COSACreateSlapObject..."));
        pProc = (COSAGetHandleProc)pPluginInfo->AcquireFunction("COSACreateSlapObject");
    }
    else if ( ulRole == LPC_ROLE_MANAGER ) 
    {
        CcspTraceWarning(("CosaWifiInitialize - AcquireFunction COSAAcquirePoamObject..."));
        pProc = (COSAGetHandleProc)pPluginInfo->AcquireFunction("COSAAcquirePoamObject");
    }
    
    if ((ulRole == LPC_ROLE_MANAGER)||(ulRole == LPC_ROLE_PARTY))
    {
        if (NULL != pProc)
        {
            CcspTraceWarning(("succeeded!\n"));
        }
        else
        {
            CcspTraceWarning(("failed!\n"));
        }
    }

    if ( ulRole == LPC_ROLE_PARTY ) 
    {
        CcspTraceWarning(("CosaWifiInitialize - create slap object..."));
        pObjDescriptor = (PSLAP_OBJECT_DESCRIPTOR)SlapCosaWifiDmGetSlapObjDescriptor((ANSC_HANDLE)NULL);
        pSlapWifiDm    = (*pProc)(pObjDescriptor);
        if (NULL != pSlapWifiDm)
        {
#ifdef _COSA_SIM_        
            g_pCosaBEManager->has_wifi_slap = 1;
#endif            
            CcspTraceWarning(("succeeded!\n"));
        }
        else
        {
            CcspTraceWarning(("failed!\n"));
        }
    }
    else if ( ulRole == LPC_ROLE_MANAGER )
    {
        CcspTraceWarning(("CosaWifiInitialize - create poam object..."));
        pObjDescriptor = (PSLAP_OBJECT_DESCRIPTOR)PoamCosaWifiDmGetPoamObjDescriptor((ANSC_HANDLE)NULL);
        pPoamWiFiDm    = (*pProc)(pObjDescriptor);
        if (NULL != pPoamWiFiDm)
        {
            CcspTraceWarning(("succeeded!\n"));
        }
        else
        {
            CcspTraceWarning(("failed!\n"));
        }
    }
    else if ( ulRole == LPC_ROLE_NONE )
    {
#ifdef _COSA_SIM_        
            g_pCosaBEManager->has_wifi_slap = 1;
#endif      
    }
#elif _COSA_DRG_CNS_
    if ( (ulRole != LPC_ROLE_NONE) && (ulRole != LPC_ROLE_INVALID))
    {
        CcspTraceWarning(("CosaWifiInitialize - AcquireFunction COSACreateSlapObject..."));
    
        pProc = (COSAGetHandleProc)pPluginInfo->AcquireFunction("COSACreateSlapObject");
        
        if (NULL != pProc)
        {
            CcspTraceWarning(("succeeded!\n"));
        }
        else
        {
            CcspTraceWarning(("failed!\n"));
        }
        
        CcspTraceWarning(("CosaWifiInitialize - create slap object..."));
            
        pObjDescriptor = (PSLAP_OBJECT_DESCRIPTOR)SlapCosaWifiDmGetSlapObjDescriptor((ANSC_HANDLE)NULL);
        
        pSlapWifiDm    = (*pProc)(pObjDescriptor);
        
        if (NULL != pSlapWifiDm)
        {
            CcspTraceWarning(("succeeded!\n"));
        }
        else
        {
            CcspTraceWarning(("failed!\n"));
        }
    }
#elif _COSA_DRG_TPG_

    if ( (ulRole != LPC_ROLE_NONE) && (ulRole != LPC_ROLE_INVALID))
    {
        CcspTraceWarning(("CosaWifiInitialize - AcquireFunction COSAAcquirePoamObject..."));
        
        pProc = (COSAGetHandleProc)pPluginInfo->AcquireFunction("COSAAcquirePoamObject");
        
        if (NULL != pProc)
        {
            CcspTraceWarning(("succeeded!\n"));
        }
        else
        {
            CcspTraceWarning(("failed!\n"));
        }
        
        CcspTraceWarning(("CosaWifiInitialize - create poam object..."));
        
        pObjDescriptor = (PSLAP_OBJECT_DESCRIPTOR)PoamCosaWifiDmGetPoamObjDescriptor((ANSC_HANDLE)NULL);
        
        pPoamWiFiDm    = (*pProc)(pObjDescriptor);
        
        if (NULL != pPoamWiFiDm)
        {
            CcspTraceWarning(("succeeded!\n"));
        }
        else
        {
            CcspTraceWarning(("failed!\n"));
        }
    }
#endif
#endif

    pMyObject->hPoamWiFiDm = (ANSC_HANDLE)pPoamWiFiDm;
    pMyObject->hSlapWiFiDm = (ANSC_HANDLE)pSlapWifiDm;

    returnStatus = CosaDmlWiFiInit((ANSC_HANDLE)pMyObject->hPoamWiFiDm, NULL);
    
    if ( returnStatus != ANSC_STATUS_SUCCESS )
    {
        CcspTraceWarning(("CosaWifiInitialize - WiFi failed to initialize. Is WiFi supposed to start?\n"));
        
        return  returnStatus;
    }
    
    /* Initiation all functions */
    pMyObject->RadioCount  = 0;
    pMyObject->pRadio      = NULL;
    
    /* next instance starts from 1 */
    pMyObject->ulSsidNextInstance = 1;
    pMyObject->ulAPNextInstance   = 1;

    AnscInitializeQueue(&pMyObject->SsidQueue);
    AnscInitializeQueue(&pMyObject->AccessPointQueue);
    
    /*Get Radio Info*/
    pMyObject->RadioCount = CosaDmlWiFiRadioGetNumberOfEntries((ANSC_HANDLE)pMyObject->hPoamWiFiDm);

    if ( pMyObject->RadioCount != 0 )
    {
        pMyObject->pRadio = (PCOSA_DML_WIFI_RADIO)AnscAllocateMemory( sizeof(COSA_DML_WIFI_RADIO) * pMyObject->RadioCount);
        
        for( uIndex = 0; uIndex < pMyObject->RadioCount; uIndex++)
        {
            pWifiRadio = pMyObject->pRadio+uIndex;
            
            if ( CosaDmlWiFiRadioGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, uIndex, &pWifiRadio->Radio) == ANSC_STATUS_SUCCESS )
            {
                if ( pWifiRadio->Radio.Cfg.InstanceNumber == 0 )
                {
                    /* Generate Default InstanceNumber and Alias */
                    _ansc_sprintf(pWifiRadio->Radio.Cfg.Alias, "wl%d", uIndex );
                    
                    CosaDmlWiFiRadioSetValues
                        (
                            (ANSC_HANDLE)pMyObject->hPoamWiFiDm,
                            uIndex, 
                            uIndex + 1, 
                            pWifiRadio->Radio.Cfg.Alias
                        );            
                    
                    pWifiRadio->Radio.Cfg.InstanceNumber = uIndex + 1;
                }
            }
        }
    }
    
    /*Read configuration*/
    pMyObject->hIrepFolderCOSA = g_GetRegistryRootFolder(g_pDslhDmlAgent);
    pPoamIrepFoCOSA = (PPOAM_IREP_FOLDER_OBJECT)pMyObject->hIrepFolderCOSA;

    if ( !pPoamIrepFoCOSA )
    {
        returnStatus = ANSC_STATUS_FAILURE;

        goto  EXIT;
    }
    
    /*Get Wifi entry*/
    pPoamIrepFoWifi = 
        (PPOAM_IREP_FOLDER_OBJECT)pPoamIrepFoCOSA->GetFolder
            (
                (ANSC_HANDLE)pPoamIrepFoCOSA,
                COSA_IREP_FOLDER_NAME_WIFI
            );

    if ( !pPoamIrepFoWifi )
    {
        pPoamIrepFoWifi =
            pPoamIrepFoCOSA->AddFolder
                (
                    (ANSC_HANDLE)pPoamIrepFoCOSA,
                    COSA_IREP_FOLDER_NAME_WIFI,
                    0
                );
    }

    if ( !pPoamIrepFoWifi )
    {
        returnStatus = ANSC_STATUS_FAILURE;

        goto  EXIT;
    }
    else
    {
        pMyObject->hIrepFolderWifi = (ANSC_HANDLE)pPoamIrepFoWifi;
    }

    /*Get Wifi.SSID entry*/
    pPoamIrepFoWifiSsid = 
        (PPOAM_IREP_FOLDER_OBJECT)pPoamIrepFoWifi->GetFolder
            (
                (ANSC_HANDLE)pPoamIrepFoWifi,
                COSA_IREP_FOLDER_NAME_WIFI_SSID
            );

    if ( !pPoamIrepFoWifiSsid )
    {
        pPoamIrepFoWifiSsid =
            pPoamIrepFoWifi->AddFolder
                (
                    (ANSC_HANDLE)pPoamIrepFoWifi,
                    COSA_IREP_FOLDER_NAME_WIFI_SSID,
                    0
                );
    }

    if ( !pPoamIrepFoWifiSsid )
    {
        returnStatus = ANSC_STATUS_FAILURE;

        goto  EXIT;
    }
    else
    {
        pMyObject->hIrepFolderWifiSsid = (ANSC_HANDLE)pPoamIrepFoWifiSsid;
    }
    
    /*Ssid Next Instance Number*/
    if ( TRUE )
    {
        pSlapVariable =
            (PSLAP_VARIABLE)pPoamIrepFoWifiSsid->GetRecord
                (
                    (ANSC_HANDLE)pPoamIrepFoWifiSsid,
                    COSA_DML_RR_NAME_WifiSsidNextInsNunmber,
                    NULL
                );

        if ( pSlapVariable )
        {
            pMyObject->ulSsidNextInstance = pSlapVariable->Variant.varUint32;

            SlapFreeVariable(pSlapVariable);
        }
    }
    
    /*Get Wifi.AccessPoint entry*/
    pPoamIrepFoWifiAP = 
        (PPOAM_IREP_FOLDER_OBJECT)pPoamIrepFoWifi->GetFolder
            (
                (ANSC_HANDLE)pPoamIrepFoWifi,
                COSA_IREP_FOLDER_NAME_WIFI_AP
            );

    if ( !pPoamIrepFoWifiAP )
    {
        pPoamIrepFoWifiAP =
            pPoamIrepFoWifi->AddFolder
                (
                    (ANSC_HANDLE)pPoamIrepFoWifi,
                    COSA_IREP_FOLDER_NAME_WIFI_AP,
                    0
                );
    }

    if ( !pPoamIrepFoWifiAP )
    {
        returnStatus = ANSC_STATUS_FAILURE;

        goto  EXIT;
    }
    else
    {
        pMyObject->hIrepFolderWifiAP = (ANSC_HANDLE)pPoamIrepFoWifiAP;
    }
    
    /*AP Next Instance Number*/
    if ( TRUE )
    {
        pSlapVariable =
            (PSLAP_VARIABLE)pPoamIrepFoWifiAP->GetRecord
                (
                    (ANSC_HANDLE)pPoamIrepFoWifiAP,
                    COSA_DML_RR_NAME_WifiAPNextInsNunmber,
                    NULL
                );

        if ( pSlapVariable )
        {
            pMyObject->ulAPNextInstance = pSlapVariable->Variant.varUint32;

            SlapFreeVariable(pSlapVariable);
        }
    }

    /*Initialize middle layer*/
    /* First the SSID part */
    uSsidCount = CosaDmlWiFiSsidGetNumberOfEntries((ANSC_HANDLE)pMyObject->hPoamWiFiDm);
    
    for( uIndex = 0; uIndex < uSsidCount; uIndex++)
    {
        pWifiSsid = (PCOSA_DML_WIFI_SSID)AnscAllocateMemory(sizeof(COSA_DML_WIFI_SSID));
        
        if (!pWifiSsid)
        {
            return ANSC_STATUS_RESOURCES;
        }
        
        /*retrieve data from backend*/
        CosaDmlWiFiSsidGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, uIndex, &pWifiSsid->SSID);

        if (TRUE)
        {
            pLinkObj = (PCOSA_CONTEXT_LINK_OBJECT)AnscAllocateMemory(sizeof(COSA_CONTEXT_LINK_OBJECT));
            
            if (!pLinkObj)
            {
                AnscFreeMemory(pWifiSsid);
                
                return ANSC_STATUS_RESOURCES;
            }
            
            if ( pWifiSsid->SSID.Cfg.InstanceNumber != 0 )
            {
                pLinkObj->InstanceNumber = pWifiSsid->SSID.Cfg.InstanceNumber;
                
                if (pMyObject->ulSsidNextInstance <= pWifiSsid->SSID.Cfg.InstanceNumber)
                {
                    pMyObject->ulSsidNextInstance = pWifiSsid->SSID.Cfg.InstanceNumber + 1;
                    
                    if (pMyObject->ulSsidNextInstance == 0)
                    {
                        pMyObject->ulSsidNextInstance = 1;
                    }
                }
            }
            else
            {
                pLinkObj->InstanceNumber = pMyObject->ulSsidNextInstance;
                
                pMyObject->ulSsidNextInstance++;
                
                if (pMyObject->ulSsidNextInstance == 0)
                {
                    pMyObject->ulSsidNextInstance = 1;
                }
                
                /* Generate Alias */
                _ansc_sprintf(pWifiSsid->SSID.Cfg.Alias, "SSID%d", pLinkObj->InstanceNumber);
                
                CosaDmlWiFiSsidSetValues
                    (
                        (ANSC_HANDLE)pMyObject->hPoamWiFiDm,
                        uIndex, 
                        pLinkObj->InstanceNumber, 
                        pWifiSsid->SSID.Cfg.Alias
                    );
                /* Set the instance number to the object also */
                pWifiSsid->SSID.Cfg.InstanceNumber = pLinkObj->InstanceNumber;
            }
            
            pLinkObj->hContext     = (ANSC_HANDLE)pWifiSsid;
            pLinkObj->hParentTable = NULL;
            pLinkObj->bNew         = FALSE;
            pLinkObj->hPoamIrepUpperFo = (ANSC_HANDLE)NULL;
            pLinkObj->hPoamIrepFo      = (ANSC_HANDLE)NULL;
            
            CosaSListPushEntryByInsNum((PSLIST_HEADER)&pMyObject->SsidQueue, pLinkObj);
        }
    }
    
    /*Then the AP part*/
    for (uIndex = 0; uIndex < uSsidCount; uIndex++)
    {
        pWifiAp = (PCOSA_DML_WIFI_AP)AnscAllocateMemory(sizeof(COSA_DML_WIFI_AP));
        
        if (!pWifiAp)
        {
            return ANSC_STATUS_RESOURCES;
        }
        
        pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->SsidQueue, uIndex);
        pLinkObj    = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);
        
        pWifiSsid   = pLinkObj->hContext;

#ifndef _COSA_INTEL_USG_ATOM_
        CosaDmlWiFiApGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->AP);   
        CosaDmlWiFiApSecGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->SEC);
        CosaDmlWiFiApWpsGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->WPS);
        CosaDmlWiFiApMfGetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->MF);
#else
        CosaDmlWiFiApGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->AP);   
        CosaDmlWiFiApSecGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->SEC);
        CosaDmlWiFiApWpsGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->WPS);
        CosaDmlWiFiApMfGetCfg((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->MF);
#endif

        if (TRUE)
        {
            pLinkObj = (PCOSA_CONTEXT_LINK_OBJECT)AnscAllocateMemory(sizeof(COSA_CONTEXT_LINK_OBJECT));
            
            if (!pLinkObj)
            {
                AnscFreeMemory(pWifiAp);
                
                return ANSC_STATUS_RESOURCES;
            }
            
            if ( pWifiAp->AP.Cfg.InstanceNumber != 0 )
            {
                pLinkObj->InstanceNumber = pWifiAp->AP.Cfg.InstanceNumber;
                
                if (pMyObject->ulAPNextInstance <= pWifiAp->AP.Cfg.InstanceNumber)
                {
                    pMyObject->ulAPNextInstance = pWifiAp->AP.Cfg.InstanceNumber + 1;
                    
                    if (pMyObject->ulAPNextInstance == 0)
                    {
                        pMyObject->ulAPNextInstance = 1;
                    }
                }
            }
            else
            {
                pLinkObj->InstanceNumber = pMyObject->ulAPNextInstance;
                
                pMyObject->ulAPNextInstance++;
                
                if ( pMyObject->ulAPNextInstance == 0 )
                {
                    pMyObject->ulAPNextInstance = 1;
                }
                
                /*Generate Alias*/
                _ansc_sprintf(pWifiAp->AP.Cfg.Alias, "AccessPoint%d", pLinkObj->InstanceNumber);
                
                CosaDmlWiFiApSetValues
                (
                    (ANSC_HANDLE)pMyObject->hPoamWiFiDm,
                    uIndex, 
                    pLinkObj->InstanceNumber, 
                    pWifiAp->AP.Cfg.Alias
                );
                /* Set the instance number to the object also */
                pWifiAp->AP.Cfg.InstanceNumber = pLinkObj->InstanceNumber;
            }
            
            pLinkObj->hContext      = (ANSC_HANDLE)pWifiAp;
            pLinkObj->hParentTable  = NULL;
            pLinkObj->bNew          = FALSE;
            pLinkObj->hPoamIrepUpperFo = (ANSC_HANDLE)NULL;
            pLinkObj->hPoamIrepFo      = (ANSC_HANDLE)NULL;
            
            CosaSListPushEntryByInsNum((PSLIST_HEADER)&pMyObject->AccessPointQueue, pLinkObj);
        }
        
        /* Device.WiFi.AccessPoint.{i}.X_CISCO_COM_MacFilterTable.{i}. */
        pPoamIrepFoMacFilt = 
            (PPOAM_IREP_FOLDER_OBJECT)pPoamIrepFoWifiAP->GetFolder
                (
                    (ANSC_HANDLE)pPoamIrepFoWifiAP,
                    COSA_IREP_FOLDER_NAME_MAC_FILT_TAB
                );

        if ( !pPoamIrepFoMacFilt )
        {
            pPoamIrepFoMacFilt = 
                pPoamIrepFoWifiAP->AddFolder
                    (
                        (ANSC_HANDLE)pPoamIrepFoWifiAP,
                        COSA_IREP_FOLDER_NAME_MAC_FILT_TAB,
                        0
                    );
        }

        if ( !pPoamIrepFoMacFilt )
        {
            returnStatus = ANSC_STATUS_FAILURE;
            goto EXIT;
        }
        else
        {
            pWifiAp->AP.hIrepFolderMacFilt = (ANSC_HANDLE)pPoamIrepFoMacFilt;
        }

        if ( TRUE )
        {
            pSlapVariable = 
                (PSLAP_VARIABLE)pPoamIrepFoMacFilt->GetRecord
                    (
                        (ANSC_HANDLE)pPoamIrepFoMacFilt,
                        COSA_DML_RR_NAME_MacFiltTabNextInsNunmber,
                        NULL
                    );

            if ( pSlapVariable )
            {
                pWifiAp->AP.ulMacFilterNextInsNum = pSlapVariable->Variant.varUint32;
                SlapFreeVariable(pSlapVariable);
            } else {
                pWifiAp->AP.ulMacFilterNextInsNum = 1;
            }
        }

        AnscInitializeQueue(&pWifiAp->AP.MacFilterList);
        CosaDmlWiFiApSetMFQueue(&pWifiAp->AP.MacFilterList, (ANSC_HANDLE)pWifiAp->AP.Cfg.InstanceNumber);

        uMacFiltCount = CosaDmlMacFilt_GetNumberOfEntries((ANSC_HANDLE)pWifiAp->AP.Cfg.InstanceNumber);
        for (uMacFiltIdx = 0; uMacFiltIdx < uMacFiltCount; uMacFiltIdx++)
        {
            pMacFilt = (PCOSA_DML_WIFI_AP_MAC_FILTER)AnscAllocateMemory(sizeof(COSA_DML_WIFI_AP_MAC_FILTER));
            if (!pMacFilt)
                return ANSC_STATUS_RESOURCES;

            CosaDmlMacFilt_GetEntryByIndex(pWifiAp->AP.Cfg.InstanceNumber, uMacFiltIdx, pMacFilt);

            pMacFiltLinkObj = (PCOSA_CONTEXT_LINK_OBJECT)AnscAllocateMemory(sizeof(COSA_CONTEXT_LINK_OBJECT));
            if ( !pMacFiltLinkObj )
            {
                AnscFreeMemory(pMacFilt);
                return ANSC_STATUS_RESOURCES;
            }

            if (pMacFilt->InstanceNumber != 0)
            {
                pMacFiltLinkObj->InstanceNumber = pMacFilt->InstanceNumber;
                if (pWifiAp->AP.ulMacFilterNextInsNum <= pMacFilt->InstanceNumber)
                {
                    pWifiAp->AP.ulMacFilterNextInsNum = pMacFilt->InstanceNumber + 1;
                    if (pWifiAp->AP.ulMacFilterNextInsNum == 0)
                        pWifiAp->AP.ulMacFilterNextInsNum = 1;
                }
            }
            else
            {
                pMacFiltLinkObj->InstanceNumber = pWifiAp->AP.ulMacFilterNextInsNum;
                pWifiAp->AP.ulMacFilterNextInsNum++;
                if (pWifiAp->AP.ulMacFilterNextInsNum == 0)
                    pWifiAp->AP.ulMacFilterNextInsNum = 1;

                _ansc_sprintf(pMacFilt->Alias, "MacFilterTable%d", pMacFiltLinkObj->InstanceNumber);

                CosaDmlMacFilt_SetValues(pWifiAp->AP.Cfg.InstanceNumber, 
                        uMacFiltIdx,
                        pMacFiltLinkObj->InstanceNumber, 
                        pMacFilt->Alias);
            }

            pMacFiltLinkObj->hContext       = (ANSC_HANDLE)pMacFilt;
            pMacFiltLinkObj->hParentTable   = (ANSC_HANDLE)pWifiAp;
            pMacFiltLinkObj->bNew           = FALSE;

            CosaSListPushEntryByInsNum((PSLIST_HEADER)&pWifiAp->AP.MacFilterList, pMacFiltLinkObj);
        }
    }
    
    /*Load the newly added but not yet commited entries, if exist*/
    CosaWifiRegGetSsidInfo((ANSC_HANDLE)pMyObject);
    
    /*Load orphan AP entries*/
    CosaWifiRegGetAPInfo((ANSC_HANDLE)pMyObject);

    if (pWifiAp != NULL)
        CosaWifiRegGetMacFiltInfo(pWifiAp);
    
EXIT:
	return returnStatus;
}

/**********************************************************************

    caller:     self

    prototype:

        ANSC_STATUS
        CosaWifiReInitialize
            (
                ANSC_HANDLE                 hThisObject
            );

    description:

        This function initiate  cosa wifi object and return handle.

    argument:	ANSC_HANDLE                 hThisObject
            This handle is actually the pointer of this object
            itself.

    return:     operation status.

**********************************************************************/

ANSC_STATUS
CosaWifiReInitialize
    (
        ANSC_HANDLE                 hThisObject,
        ULONG                       uRadioIndex
    )
{
    ANSC_STATUS                     returnStatus        = ANSC_STATUS_SUCCESS;
    PCOSA_DATAMODEL_WIFI            pMyObject           = (PCOSA_DATAMODEL_WIFI)hThisObject;
    ULONG                           uIndex              = 0;
    ULONG                           uApIndex            = 0; 
    ULONG                           uSsidCount          = 0;
    ULONG                           uApCount            = 0;    
    PCOSA_DML_WIFI_RADIO            pWifiRadio          = NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid           = (PCOSA_DML_WIFI_SSID      )NULL;
    PCOSA_DML_WIFI_AP               pWifiAp             = (PCOSA_DML_WIFI_AP        )NULL;        
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj            = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntry         = (PSINGLE_LINK_ENTRY       )NULL;

    returnStatus = CosaDmlWiFiInit((ANSC_HANDLE)pMyObject->hPoamWiFiDm, &uRadioIndex);

    if ( returnStatus != ANSC_STATUS_SUCCESS )
    {
        CcspTraceWarning(("CosaWifiInitialize - WiFi failed to initialize. Is WiFi supposed to start?\n"));

        return  returnStatus;
    }


    if ( uRadioIndex >= 0 && uRadioIndex <= pMyObject->RadioCount)
    {
        pWifiRadio = pMyObject->pRadio+uRadioIndex;

        CosaDmlWiFiRadioGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, uRadioIndex, &pWifiRadio->Radio);
    }

    /* First the SSID part */
    uSsidCount = CosaDmlWiFiSsidGetNumberOfEntries((ANSC_HANDLE)pMyObject->hPoamWiFiDm);
    for( uIndex = 0; uIndex < uSsidCount; uIndex++)
    {
        pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->SsidQueue, uIndex);
        pLinkObj    = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);
        pWifiSsid   = pLinkObj->hContext;
        UCHAR    PathName[64] = {0};

        if (!pWifiSsid)
        {
            return ANSC_STATUS_RESOURCES;
        }

        sprintf(PathName, "wifi%d", uRadioIndex);
        if (AnscEqualString(pWifiSsid->SSID.Cfg.WiFiRadioName, PathName, TRUE)) {
            /*retrieve data from backend*/
            CosaDmlWiFiSsidGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, uIndex, &pWifiSsid->SSID);

            sprintf(PathName, "Device.WiFi.SSID.%d.", pLinkObj->InstanceNumber);
	    for (uApIndex = 0; uApIndex < uSsidCount; uApIndex++)
	    {
                pSLinkEntry = AnscQueueGetEntryByIndex(&pMyObject->AccessPointQueue, uApIndex);
                pLinkObj    = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);
                pWifiAp   = pLinkObj->hContext;

		if (AnscEqualString(pWifiAp->AP.Cfg.SSID, PathName, TRUE))
		{
#ifndef _COSA_INTEL_USG_ATOM_
                    CosaDmlWiFiApGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->AP);   
                    CosaDmlWiFiApSecGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->SEC);
                    CosaDmlWiFiApWpsGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.Cfg.SSID, &pWifiAp->WPS);
#else
                    CosaDmlWiFiApGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->AP);   
                    CosaDmlWiFiApSecGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->SEC);
                    CosaDmlWiFiApWpsGetEntry((ANSC_HANDLE)pMyObject->hPoamWiFiDm, pWifiSsid->SSID.StaticInfo.Name, &pWifiAp->WPS);
#endif

                    break;
                }
            }
        }
    }

    EXIT:
    return returnStatus;
}

/**********************************************************************

    caller:     self

    prototype:

        ANSC_STATUS
        CosaWifiRemove
            (
                ANSC_HANDLE                 hThisObject
            );

    description:

        This function initiate  cosa wifi object and return handle.

    argument:	ANSC_HANDLE                 hThisObject
            This handle is actually the pointer of this object
            itself.

    return:     operation status.

**********************************************************************/
ANSC_STATUS
CosaWifiRemove
    (
        ANSC_HANDLE                 hThisObject
    )
{
    ANSC_STATUS                     returnStatus = ANSC_STATUS_SUCCESS;
    PCOSA_DATAMODEL_WIFI            pMyObject    = (PCOSA_DATAMODEL_WIFI)hThisObject;
    PSINGLE_LINK_ENTRY              pSLinkEntry  = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pLinkObj     = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PCOSA_DML_WIFI_AP               pWifiAp      = (PCOSA_DML_WIFI_AP        )NULL;
    PCOSA_PLUGIN_INFO               pPlugInfo    = (PCOSA_PLUGIN_INFO        )g_pCosaBEManager->hCosaPluginInfo;
    COSAGetHandleProc               pProc        = (COSAGetHandleProc        )NULL;
    PSLAP_OBJECT_DESCRIPTOR       pObjDescriptor    = (PSLAP_OBJECT_DESCRIPTOR )NULL;

    /* Remove Poam or Slap resounce */
    
	/* Remove necessary resounce */
    if ( pMyObject->pRadio )
        AnscFreeMemory( pMyObject->pRadio );
    
    /* Remove all SSID entry */
    pSLinkEntry = AnscQueueGetFirstEntry(&pMyObject->SsidQueue);
    while (pSLinkEntry)
    {
        pLinkObj      = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);
        pSLinkEntry   = AnscQueueGetNextEntry(pSLinkEntry);
        
        AnscQueuePopEntryByLink(&pMyObject->SsidQueue, &pLinkObj->Linkage);
        
        if (pLinkObj->hContext)
        {
            AnscFreeMemory(pLinkObj->hContext);
        }
        if (pLinkObj)
        {
            AnscFreeMemory(pLinkObj);
        }
    }

    /* Remove all AccessPoint entry */
    pSLinkEntry = AnscQueueGetFirstEntry(&pMyObject->AccessPointQueue);
    while (pSLinkEntry)
    {
        pLinkObj      = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSLinkEntry);
        pSLinkEntry   = AnscQueueGetNextEntry(pSLinkEntry);
        pWifiAp       = pLinkObj->hContext;
        
        AnscQueuePopEntryByLink(&pMyObject->AccessPointQueue, &pLinkObj->Linkage);
        
        if (pWifiAp->AssocDevices)
        {
            AnscFreeMemory(pWifiAp->AssocDevices);
        }
        if (pLinkObj->hContext)
        {
            AnscFreeMemory(pLinkObj->hContext);
        }
        if (pLinkObj)
        {
            AnscFreeMemory(pLinkObj);
        }
    }
        
    /* Remove self */
    AnscFreeMemory((ANSC_HANDLE)pMyObject);

	return returnStatus;
}

/**********************************************************************

    caller:     owner of this object

    prototype:

        ANSC_STATUS
        CosaWifiRegGetSsidInfo
            (
                ANSC_HANDLE                 hThisObject
            );

    description:

        This function is called to retrieve Dslm policy parameters.

    argument:   ANSC_HANDLE                 hThisObject
                This handle is actually the pointer of this object
                itself.

                ANSC_HANDLE                 hDdnsInfo
                Specifies the Dslm policy parameters to be filled.

    return:     status of operation.

**********************************************************************/

ANSC_STATUS
CosaWifiRegGetSsidInfo
    (
        ANSC_HANDLE                 hThisObject
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject            = (PCOSA_DATAMODEL_WIFI     )hThisObject;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoWifiSsid  = (PPOAM_IREP_FOLDER_OBJECT )pMyObject->hIrepFolderWifiSsid;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoWifiSsidE = (PPOAM_IREP_FOLDER_OBJECT )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pCosaContext         = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntry          = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid            = (PCOSA_DML_WIFI_SSID      )NULL;
    PSLAP_VARIABLE                  pSlapVariable        = (PSLAP_VARIABLE           )NULL;
    ULONG                           ulEntryCount         = 0;
    ULONG                           ulIndex              = 0;
    ULONG                           uInstanceNumber      = 0;
    char*                           pFolderName          = NULL;
    char*                           pName                = NULL;

    if ( !pPoamIrepFoWifiSsid )
    {
        return ANSC_STATUS_FAILURE;
    }
    
    /* Load the newly added but not yet commited entries */
    ulEntryCount = pPoamIrepFoWifiSsid->GetFolderCount((ANSC_HANDLE)pPoamIrepFoWifiSsid);

    for ( ulIndex = 0; ulIndex < ulEntryCount; ulIndex++ )
    {
        /*which instance*/
        pFolderName =
            pPoamIrepFoWifiSsid->EnumFolder
                (
                    (ANSC_HANDLE)pPoamIrepFoWifiSsid,
                    ulIndex
                );

        if ( !pFolderName )
        {
            continue;
        }

        uInstanceNumber = _ansc_atol(pFolderName);

        if ( uInstanceNumber == 0 )
        {
            continue;
        }

        pPoamIrepFoWifiSsidE = pPoamIrepFoWifiSsid->GetFolder((ANSC_HANDLE)pPoamIrepFoWifiSsid, pFolderName);

        AnscFreeMemory(pFolderName);

        if ( !pPoamIrepFoWifiSsidE )
        {
            continue;
        }

        if ( TRUE )
        {
            pSlapVariable =
                (PSLAP_VARIABLE)pPoamIrepFoWifiSsidE->GetRecord
                    (
                        (ANSC_HANDLE)pPoamIrepFoWifiSsidE,
                        COSA_DML_RR_NAME_WifiSsidName,
                        NULL
                    );

            if ( pSlapVariable )
            {
                pName = AnscCloneString(pSlapVariable->Variant.varString);

                SlapFreeVariable(pSlapVariable);
            }
        }

        pCosaContext = (PCOSA_CONTEXT_LINK_OBJECT)AnscAllocateMemory(sizeof(COSA_CONTEXT_LINK_OBJECT));

        if ( !pCosaContext )
        {
            AnscFreeMemory(pName);

            return ANSC_STATUS_FAILURE;
        }

        pWifiSsid = (PCOSA_DML_WIFI_SSID)AnscAllocateMemory(sizeof(COSA_DML_WIFI_SSID));

        if ( !pWifiSsid )
        {
            AnscFreeMemory(pCosaContext);

            AnscFreeMemory(pName);
            
            return ANSC_STATUS_FAILURE;
        }

        //CosaDmlWiFiSsidGetEntryByName(NULL, pName, pWifiSsid);

        AnscCopyString(pWifiSsid->SSID.StaticInfo.Name, pName);
        AnscCopyString(pWifiSsid->SSID.Cfg.Alias, pName);
    
        pCosaContext->hContext         = (ANSC_HANDLE)pWifiSsid;
        pCosaContext->hParentTable     = NULL;
        pCosaContext->InstanceNumber   = uInstanceNumber;
        pCosaContext->bNew             = TRUE;
        pCosaContext->hPoamIrepUpperFo = (ANSC_HANDLE)pPoamIrepFoWifiSsid;
        pCosaContext->hPoamIrepFo      = (ANSC_HANDLE)pPoamIrepFoWifiSsidE;

        CosaSListPushEntryByInsNum((PSLIST_HEADER)&pMyObject->SsidQueue, pCosaContext);
        
        if ( pName )
        {
            AnscFreeMemory(pName);
            pName = NULL;
        }
    }


    return ANSC_STATUS_SUCCESS;
}


/**********************************************************************

    caller:     owner of this object

    prototype:

        ANSC_STATUS
        CosaWifiRegAddSsidInfo
            (
                ANSC_HANDLE                 hThisObject,
                ANSC_HANDLE                 hCosaContext
            );

    description:

        This function is called to configure Dslm policy parameters.

    argument:   ANSC_HANDLE                 hThisObject
                This handle is actually the pointer of this object
                itself.

                ANSC_HANDLE                 hDdnsInfo
                Specifies the Dslm policy parameters to be filled.

    return:     status of operation.

**********************************************************************/
ANSC_STATUS
CosaWifiRegAddSsidInfo
    (
        ANSC_HANDLE                 hThisObject,
        ANSC_HANDLE                 hCosaContext
    )
{
    ANSC_STATUS                     returnStatus         = ANSC_STATUS_SUCCESS;
    PCOSA_DATAMODEL_WIFI            pMyObject            = (PCOSA_DATAMODEL_WIFI     )hThisObject;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoWifiSsid  = (PPOAM_IREP_FOLDER_OBJECT )pMyObject->hIrepFolderWifiSsid;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoWifiSsidE = (PPOAM_IREP_FOLDER_OBJECT )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pCosaContext         = (PCOSA_CONTEXT_LINK_OBJECT)hCosaContext;
    PCOSA_DML_WIFI_SSID             pWifiSsid            = (PCOSA_DML_WIFI_SSID      )NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntry          = (PSINGLE_LINK_ENTRY       )NULL;
    PSLAP_VARIABLE                  pSlapVariable        = (PSLAP_VARIABLE           )NULL;
    ULONG                           ulEntryCount         = 0;
    ULONG                           ulIndex              = 0;
    ULONG                           uInstanceNumber      = 0;
    char                            FolderName[16]       = {0};

    if ( !pPoamIrepFoWifiSsid || !hCosaContext)
    {
        return ANSC_STATUS_FAILURE;
    }
    else
    {
        pPoamIrepFoWifiSsid->EnableFileSync((ANSC_HANDLE)pPoamIrepFoWifiSsid, FALSE);
    }

    if ( TRUE )
    {
        SlapAllocVariable(pSlapVariable);

        if ( !pSlapVariable )
        {
            returnStatus = ANSC_STATUS_RESOURCES;

            goto  EXIT1;
        }
    }
    
    /*Next Instance*/
    if ( TRUE )
    {
        returnStatus = 
            pPoamIrepFoWifiSsid->DelRecord
                (
                    (ANSC_HANDLE)pPoamIrepFoWifiSsid,
                    COSA_DML_RR_NAME_WifiSsidNextInsNunmber
                );        
        
        pSlapVariable->Syntax            = SLAP_VAR_SYNTAX_uint32;
        pSlapVariable->Variant.varUint32 = pMyObject->ulSsidNextInstance;

        returnStatus =
            pPoamIrepFoWifiSsid->AddRecord
                (
                    (ANSC_HANDLE)pPoamIrepFoWifiSsid,
                    COSA_DML_RR_NAME_WifiSsidNextInsNunmber,
                    SYS_REP_RECORD_TYPE_UINT,
                    SYS_RECORD_CONTENT_DEFAULT,
                    pSlapVariable,
                    0
                );

        SlapCleanVariable(pSlapVariable);
        SlapInitVariable (pSlapVariable);
    }

        pWifiSsid    = (PCOSA_DML_WIFI_SSID)pCosaContext->hContext;

        _ansc_sprintf(FolderName, "%d", pCosaContext->InstanceNumber);

        pPoamIrepFoWifiSsidE =
            pPoamIrepFoWifiSsid->AddFolder
                (
                    (ANSC_HANDLE)pPoamIrepFoWifiSsid,
                    FolderName,
                    0
                );

        if ( !pPoamIrepFoWifiSsidE )
        {
            returnStatus = ANSC_STATUS_FAILURE;
            
            goto EXIT1;
        }

        if ( TRUE )
        {
            pSlapVariable->Syntax            = SLAP_VAR_SYNTAX_string;
            pSlapVariable->Variant.varString = AnscCloneString(pWifiSsid->SSID.StaticInfo.Name);

            returnStatus =
                pPoamIrepFoWifiSsidE->AddRecord
                    (
                        (ANSC_HANDLE)pPoamIrepFoWifiSsidE,
                        COSA_DML_RR_NAME_WifiSsidName,
                        SYS_REP_RECORD_TYPE_ASTR,
                        SYS_RECORD_CONTENT_DEFAULT,
                        pSlapVariable,
                        0
                    );

            SlapCleanVariable(pSlapVariable);
            SlapInitVariable (pSlapVariable);
        }

        pCosaContext->hPoamIrepUpperFo = (ANSC_HANDLE)pPoamIrepFoWifiSsid;
        pCosaContext->hPoamIrepFo      = (ANSC_HANDLE)pPoamIrepFoWifiSsidE;
        
EXIT1:

    if ( pSlapVariable )
    {
        SlapFreeVariable(pSlapVariable);
    }

    pPoamIrepFoWifiSsid->EnableFileSync((ANSC_HANDLE)pPoamIrepFoWifiSsid, TRUE);

    return returnStatus;
}

/**********************************************************************

    caller:     owner of this object

    prototype:

        ANSC_STATUS
        CosaWifiRegDelSsidInfo
            (
                ANSC_HANDLE                 hThisObject,
                ANSC_HANDLE                 hCosaContext
            );

    description:

        This function is called to configure Dslm policy parameters.

    argument:   ANSC_HANDLE                 hThisObject
                This handle is actually the pointer of this object
                itself.

                ANSC_HANDLE                 hDdnsInfo
                Specifies the Dslm policy parameters to be filled.

    return:     status of operation.

**********************************************************************/
ANSC_STATUS
CosaWifiRegDelSsidInfo
    (
        ANSC_HANDLE                 hThisObject,
        ANSC_HANDLE                 hCosaContext
    )
{
    ANSC_STATUS                     returnStatus         = ANSC_STATUS_SUCCESS;
    PCOSA_DATAMODEL_WIFI            pMyObject            = (PCOSA_DATAMODEL_WIFI     )hThisObject;
    PCOSA_CONTEXT_LINK_OBJECT       pCosaContext         = (PCOSA_CONTEXT_LINK_OBJECT)hCosaContext;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoWifiSsid  = (PPOAM_IREP_FOLDER_OBJECT )pCosaContext->hPoamIrepUpperFo;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoWifiSsidE = (PPOAM_IREP_FOLDER_OBJECT )pCosaContext->hPoamIrepFo;
    UCHAR                           FolderName[16]       = {0};

    if ( !pPoamIrepFoWifiSsid || !pPoamIrepFoWifiSsidE)
    {
        return ANSC_STATUS_FAILURE;
    }
    else
    {
        pPoamIrepFoWifiSsid->EnableFileSync((ANSC_HANDLE)pPoamIrepFoWifiSsid, FALSE);
    }

    if ( TRUE )
    {
        pPoamIrepFoWifiSsidE->Close((ANSC_HANDLE)pPoamIrepFoWifiSsidE);
        
        pPoamIrepFoWifiSsid->DelFolder
            (
                (ANSC_HANDLE)pPoamIrepFoWifiSsid, 
                pPoamIrepFoWifiSsidE->GetFolderName((ANSC_HANDLE)pPoamIrepFoWifiSsidE)
            );

        AnscFreeMemory(pPoamIrepFoWifiSsidE);
    }

    pPoamIrepFoWifiSsid->EnableFileSync((ANSC_HANDLE)pPoamIrepFoWifiSsid, TRUE);
    
    return returnStatus;
}

/**********************************************************************

    caller:     owner of this object

    prototype:

        ANSC_STATUS
        CosaWifiRegGetAPInfo
            (
                ANSC_HANDLE                 hThisObject
            );

    description:

        This function is called to retrieve Dslm policy parameters.

    argument:   ANSC_HANDLE                 hThisObject
                This handle is actually the pointer of this object
                itself.

                ANSC_HANDLE                 hDdnsInfo
                Specifies the Dslm policy parameters to be filled.

    return:     status of operation.

**********************************************************************/

ANSC_STATUS
CosaWifiRegGetAPInfo
    (
        ANSC_HANDLE                 hThisObject
    )
{
    PCOSA_DATAMODEL_WIFI            pMyObject            = (PCOSA_DATAMODEL_WIFI     )hThisObject;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoWifiAP    = (PPOAM_IREP_FOLDER_OBJECT )pMyObject->hIrepFolderWifiAP;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoWifiAPE   = (PPOAM_IREP_FOLDER_OBJECT )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pCosaContext         = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntry          = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_DML_WIFI_AP               pWifiAP              = (PCOSA_DML_WIFI_AP        )NULL;
    PSLAP_VARIABLE                  pSlapVariable        = (PSLAP_VARIABLE           )NULL;
    ULONG                           ulEntryCount         = 0;
    ULONG                           ulIndex              = 0;
    ULONG                           uInstanceNumber      = 0;
    char*                           pFolderName          = NULL;
    char*                           pName                = NULL;
    char*                           pSsidReference       = NULL;

    if ( !pPoamIrepFoWifiAP )
    {
        return ANSC_STATUS_FAILURE;
    }

    ulEntryCount = pPoamIrepFoWifiAP->GetFolderCount((ANSC_HANDLE)pPoamIrepFoWifiAP);

    for ( ulIndex = 0; ulIndex < ulEntryCount; ulIndex++ )
    {
        /*which instance*/
        pFolderName =
            pPoamIrepFoWifiAP->EnumFolder
                (
                    (ANSC_HANDLE)pPoamIrepFoWifiAP,
                    ulIndex
                );

        if ( !pFolderName )
        {
            continue;
        }

        uInstanceNumber = _ansc_atol(pFolderName);

        if ( uInstanceNumber == 0 )
        {
            continue;
        }

        pPoamIrepFoWifiAPE = pPoamIrepFoWifiAP->GetFolder((ANSC_HANDLE)pPoamIrepFoWifiAP, pFolderName);

        AnscFreeMemory(pFolderName);

        if ( !pPoamIrepFoWifiAPE )
        {
            continue;
        }

        if ( TRUE )
        {
            pSlapVariable =
                (PSLAP_VARIABLE)pPoamIrepFoWifiAPE->GetRecord
                    (
                        (ANSC_HANDLE)pPoamIrepFoWifiAPE,
                        COSA_DML_RR_NAME_WifiAPSSID,
                        NULL
                    );

            if ( pSlapVariable )
            {
                pName = AnscCloneString(pSlapVariable->Variant.varString);

                SlapFreeVariable(pSlapVariable);
            }
        }

        if ( TRUE )
        {
            pSlapVariable =
                (PSLAP_VARIABLE)pPoamIrepFoWifiAPE->GetRecord
                    (
                        (ANSC_HANDLE)pPoamIrepFoWifiAPE,
                        COSA_DML_RR_NAME_WifiAPSSIDReference,
                        NULL
                    );

            if ( pSlapVariable )
            {
                pSsidReference = AnscCloneString(pSlapVariable->Variant.varString);

                SlapFreeVariable(pSlapVariable);
            }
        }        
        
        pCosaContext = (PCOSA_CONTEXT_LINK_OBJECT)AnscAllocateMemory(sizeof(COSA_CONTEXT_LINK_OBJECT));

        if ( !pCosaContext )
        {
            AnscFreeMemory(pName);
            AnscFreeMemory(pSsidReference);

            return ANSC_STATUS_FAILURE;
        }

        pWifiAP = (PCOSA_DML_WIFI_AP)AnscAllocateMemory(sizeof(COSA_DML_WIFI_AP));

        if ( !pWifiAP )
        {
            AnscFreeMemory(pCosaContext);

            return ANSC_STATUS_FAILURE;
        }

        AnscCopyString(pWifiAP->AP.Cfg.SSID, pSsidReference);

        pCosaContext->InstanceNumber   = uInstanceNumber;
        pCosaContext->bNew             = TRUE;
        pCosaContext->hContext         = (ANSC_HANDLE)pWifiAP;
        pCosaContext->hParentTable     = NULL;
        pCosaContext->hPoamIrepUpperFo = (ANSC_HANDLE)pPoamIrepFoWifiAP;
        pCosaContext->hPoamIrepFo      = (ANSC_HANDLE)pPoamIrepFoWifiAPE;

        CosaSListPushEntryByInsNum((PSLIST_HEADER)&pMyObject->AccessPointQueue, pCosaContext);
        
        if ( pName )
        {
            AnscFreeMemory(pName);
            pName = NULL;
        }
    
        if (pSsidReference)
        {
            AnscFreeMemory(pSsidReference);
            pSsidReference = NULL;
        }
    }

    return ANSC_STATUS_SUCCESS;
}


/**********************************************************************

    caller:     owner of this object

    prototype:

        ANSC_STATUS
        CosaWifiRegAddAPInfo
            (
                ANSC_HANDLE                 hThisObject,
                ANSC_HANDLE                 hCosaContext
            );

    description:

        This function is called to configure Dslm policy parameters.

    argument:   ANSC_HANDLE                 hThisObject
                This handle is actually the pointer of this object
                itself.

                ANSC_HANDLE                 hDdnsInfo
                Specifies the Dslm policy parameters to be filled.

    return:     status of operation.

**********************************************************************/

ANSC_STATUS
CosaWifiRegAddAPInfo
    (
        ANSC_HANDLE                 hThisObject,
        ANSC_HANDLE                 hCosaContext
    )
{
    ANSC_STATUS                     returnStatus         = ANSC_STATUS_SUCCESS;
    PCOSA_DATAMODEL_WIFI            pMyObject            = (PCOSA_DATAMODEL_WIFI     )hThisObject;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoWifiAP    = (PPOAM_IREP_FOLDER_OBJECT )pMyObject->hIrepFolderWifiAP;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoWifiAPE   = (PPOAM_IREP_FOLDER_OBJECT )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pCosaContext         = (PCOSA_CONTEXT_LINK_OBJECT)hCosaContext;
    PCOSA_DML_WIFI_AP               pWifiAP              = (PCOSA_DML_WIFI_AP        )NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntry          = (PSINGLE_LINK_ENTRY       )NULL;
    PSINGLE_LINK_ENTRY              pSsidSLinkEntry      = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pSSIDLinkObj         = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PCOSA_DML_WIFI_SSID             pWifiSsid            = (PCOSA_DML_WIFI_SSID      )NULL;
    PSLAP_VARIABLE                  pSlapVariable        = (PSLAP_VARIABLE           )NULL;
    ULONG                           ulEntryCount         = 0;
    ULONG                           ulIndex              = 0;
    ULONG                           uInstanceNumber      = 0;
    char                            FolderName[16]       = {0};
    UCHAR                           PathName[64]         = {0};

    if ( !pPoamIrepFoWifiAP || !pCosaContext)
    {
        return ANSC_STATUS_FAILURE;
    }
    else
    {
        pPoamIrepFoWifiAP->EnableFileSync((ANSC_HANDLE)pPoamIrepFoWifiAP, FALSE);
    }

    if ( TRUE )
    {
        SlapAllocVariable(pSlapVariable);

        if ( !pSlapVariable )
        {
            returnStatus = ANSC_STATUS_RESOURCES;

            goto  EXIT1;
        }
    }

    /*Next Instance*/
    if ( TRUE )
    {
        returnStatus = 
        pPoamIrepFoWifiAP->DelRecord
            (
                (ANSC_HANDLE)pPoamIrepFoWifiAP,
                COSA_DML_RR_NAME_WifiAPNextInsNunmber
            );
                
        pSlapVariable->Syntax            = SLAP_VAR_SYNTAX_uint32;
        pSlapVariable->Variant.varUint32 = pMyObject->ulAPNextInstance;

        returnStatus =
            pPoamIrepFoWifiAP->AddRecord
                (
                    (ANSC_HANDLE)pPoamIrepFoWifiAP,
                    COSA_DML_RR_NAME_WifiAPNextInsNunmber,
                    SYS_REP_RECORD_TYPE_UINT,
                    SYS_RECORD_CONTENT_DEFAULT,
                    pSlapVariable,
                    0
                );

        SlapCleanVariable(pSlapVariable);
        SlapInitVariable (pSlapVariable);
    }

    pWifiAP      = (PCOSA_DML_WIFI_AP)pCosaContext->hContext;

    _ansc_sprintf(FolderName, "%d", pCosaContext->InstanceNumber);

    pPoamIrepFoWifiAPE =
        pPoamIrepFoWifiAP->AddFolder
            (
                (ANSC_HANDLE)pPoamIrepFoWifiAP,
                FolderName,
                0
            );

    if ( !pPoamIrepFoWifiAPE )
    {
        returnStatus = ANSC_STATUS_FAILURE;
        
        goto  EXIT1;
    }


    /*Store the corresponding SSID and SSIDReference*/
    if ( TRUE )
    {
        pSlapVariable->Syntax            = SLAP_VAR_SYNTAX_string;

        pSsidSLinkEntry = AnscQueueGetFirstEntry(&pMyObject->SsidQueue);
    
        while ( pSsidSLinkEntry )
        {
            pSSIDLinkObj = ACCESS_COSA_CONTEXT_LINK_OBJECT(pSsidSLinkEntry);
            pWifiSsid    = (PCOSA_DML_WIFI_SSID)pSSIDLinkObj->hContext;
    
            sprintf(PathName, "Device.WiFi.SSID.%d.", pSSIDLinkObj->InstanceNumber);
    
            /*see whether the corresponding SSID entry exists*/
            if ( AnscEqualString(pWifiAP->AP.Cfg.SSID, PathName, TRUE) )
            {
                break;
            }
    
            pSsidSLinkEntry             = AnscQueueGetNextEntry(pSsidSLinkEntry);
        }

        if (pSsidSLinkEntry)
        {
            pSlapVariable->Variant.varString = AnscCloneString(pWifiSsid->SSID.Cfg.SSID);

            returnStatus =
                pPoamIrepFoWifiAPE->AddRecord
                    (
                        (ANSC_HANDLE)pPoamIrepFoWifiAPE,
                        COSA_DML_RR_NAME_WifiAPSSID,
                        SYS_REP_RECORD_TYPE_ASTR,
                        SYS_RECORD_CONTENT_DEFAULT,
                        pSlapVariable,
                        0
                    );

            SlapCleanVariable(pSlapVariable);
            SlapInitVariable (pSlapVariable);
        
            pSlapVariable->Syntax            = SLAP_VAR_SYNTAX_string;
            pSlapVariable->Variant.varString = AnscCloneString(PathName);

            returnStatus =
                pPoamIrepFoWifiAPE->AddRecord
                    (
                        (ANSC_HANDLE)pPoamIrepFoWifiAPE,
                        COSA_DML_RR_NAME_WifiAPSSIDReference,
                        SYS_REP_RECORD_TYPE_ASTR,
                        SYS_RECORD_CONTENT_DEFAULT,
                        pSlapVariable,
                        0
                    );

            SlapCleanVariable(pSlapVariable);
            SlapInitVariable (pSlapVariable);
        }
        else
        {
            returnStatus = ANSC_STATUS_FAILURE;
        
            goto  EXIT1;                
        }
    }


    pCosaContext->hPoamIrepUpperFo = (ANSC_HANDLE)pPoamIrepFoWifiAP;
    pCosaContext->hPoamIrepFo      = (ANSC_HANDLE)pPoamIrepFoWifiAPE;

EXIT1:

    if ( pSlapVariable )
    {
        SlapFreeVariable(pSlapVariable);
    }

    pPoamIrepFoWifiAP->EnableFileSync((ANSC_HANDLE)pPoamIrepFoWifiAP, TRUE);

    return returnStatus;
}

/**********************************************************************

    caller:     owner of this object

    prototype:

        ANSC_STATUS
        CosaWifiRegDelAPInfo
            (
                ANSC_HANDLE                 hThisObject,
                ANSC_HANDLE                 hCosaContext
            );

    description:

        This function is called to configure Dslm policy parameters.

    argument:   ANSC_HANDLE                 hThisObject
                This handle is actually the pointer of this object
                itself.

                ANSC_HANDLE                 hDdnsInfo
                Specifies the Dslm policy parameters to be filled.

    return:     status of operation.

**********************************************************************/

ANSC_STATUS
CosaWifiRegDelAPInfo
    (
        ANSC_HANDLE                 hThisObject,
        ANSC_HANDLE                 hCosaContext
    )
{
    ANSC_STATUS                     returnStatus         = ANSC_STATUS_SUCCESS;
    PCOSA_DATAMODEL_WIFI            pMyObject            = (PCOSA_DATAMODEL_WIFI     )hThisObject;
    PCOSA_CONTEXT_LINK_OBJECT       pCosaContext         = (PCOSA_CONTEXT_LINK_OBJECT)hCosaContext;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoWifiAP    = (PPOAM_IREP_FOLDER_OBJECT )pCosaContext->hPoamIrepUpperFo;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoWifiAPE   = (PPOAM_IREP_FOLDER_OBJECT )pCosaContext->hPoamIrepFo;
    UCHAR                           FolderName[16]       = {0};

    if ( !pPoamIrepFoWifiAP || !pPoamIrepFoWifiAPE )
    {
        return ANSC_STATUS_FAILURE;
    }
    else
    {
        pPoamIrepFoWifiAP->EnableFileSync((ANSC_HANDLE)pPoamIrepFoWifiAP, FALSE);
    }

    if ( TRUE )
    {
        pPoamIrepFoWifiAPE->Close((ANSC_HANDLE)pPoamIrepFoWifiAPE);
        
        pPoamIrepFoWifiAP->DelFolder
            (
                (ANSC_HANDLE)pPoamIrepFoWifiAP, 
                pPoamIrepFoWifiAPE->GetFolderName((ANSC_HANDLE)pPoamIrepFoWifiAPE)
            );

        AnscFreeMemory(pPoamIrepFoWifiAPE);
    }

    pPoamIrepFoWifiAP->EnableFileSync((ANSC_HANDLE)pPoamIrepFoWifiAP, TRUE);
    
    return returnStatus;
}

ANSC_STATUS
CosaDmlWiFiApMfSetMacList
    (
        CHAR        *maclist,
        UCHAR       *mac,
        ULONG       *numList
    )
{
    int     i = 0;
    char *buf = NULL;
    unsigned char macAddr[COSA_DML_WIFI_MAX_MAC_FILTER_NUM][6];

    buf = strtok(maclist, ",");
    while(buf != NULL)
    {
        if(CosaUtilStringToHex(buf, macAddr[i], 6) != ANSC_STATUS_SUCCESS)
        {
            *numList = 0;
            return ANSC_STATUS_FAILURE;
        }
        i++;
        buf = strtok(NULL, ",");
    }
    *numList = i;
    memcpy(mac, macAddr, 6*i);
    
    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaDmlWiFiApMfGetMacList
    (
        UCHAR       *mac,
        CHAR        *maclist,
        ULONG       numList
    )
{
    int     i = 0;
    int     j = 0;
    char macAddr[COSA_DML_WIFI_MAX_MAC_FILTER_NUM][18];

    for(i; i<numList; i++){
        if(i > 0)
            strcat(maclist, ",");
        sprintf(macAddr[i], "%02x:%02x:%02x:%02x:%02x:%02x", mac[j], mac[j+1], mac[j+2], mac[j+3], mac[j+4], mac[j+5]);
        strcat(maclist, macAddr[i]);
        j +=6;
    }
    return ANSC_STATUS_SUCCESS;
}


ANSC_STATUS
CosaWifiRegGetMacFiltInfo
    (
        ANSC_HANDLE                 hThisObject
    )
{
    PCOSA_DML_WIFI_AP_FULL          pMyObject            = (PCOSA_DML_WIFI_AP_FULL     )hThisObject;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoMacFilt   = (PPOAM_IREP_FOLDER_OBJECT )pMyObject->hIrepFolderMacFilt;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoMacFiltE  = (PPOAM_IREP_FOLDER_OBJECT )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pCosaContext         = (PCOSA_CONTEXT_LINK_OBJECT)NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntry          = (PSINGLE_LINK_ENTRY       )NULL;
    PCOSA_DML_WIFI_AP_MAC_FILTER    pMacFilt             = (PCOSA_DML_WIFI_AP_MAC_FILTER      )NULL;
    PSLAP_VARIABLE                  pSlapVariable        = (PSLAP_VARIABLE           )NULL;
    ULONG                           ulEntryCount         = 0;
    ULONG                           ulIndex              = 0;
    ULONG                           uInstanceNumber      = 0;
    char*                           pFolderName          = NULL;
    char*                           pName                = NULL;

    if ( !pPoamIrepFoMacFilt )
    {
        return ANSC_STATUS_FAILURE;
    }
    
    /* Load the newly added but not yet commited entries */
    ulEntryCount = pPoamIrepFoMacFilt->GetFolderCount((ANSC_HANDLE)pPoamIrepFoMacFilt);

    for ( ulIndex = 0; ulIndex < ulEntryCount; ulIndex++ )
    {
        /*which instance*/
        pFolderName =
            pPoamIrepFoMacFilt->EnumFolder
                (
                    (ANSC_HANDLE)pPoamIrepFoMacFilt,
                    ulIndex
                );

        if ( !pFolderName )
        {
            continue;
        }

        uInstanceNumber = _ansc_atol(pFolderName);

        if ( uInstanceNumber == 0 )
        {
            continue;
        }

        pPoamIrepFoMacFiltE = pPoamIrepFoMacFilt->GetFolder((ANSC_HANDLE)pPoamIrepFoMacFilt, pFolderName);

        AnscFreeMemory(pFolderName);

        if ( !pPoamIrepFoMacFiltE )
        {
            continue;
        }

        if ( TRUE )
        {
            pSlapVariable =
                (PSLAP_VARIABLE)pPoamIrepFoMacFiltE->GetRecord
                    (
                        (ANSC_HANDLE)pPoamIrepFoMacFiltE,
                        COSA_DML_RR_NAME_MacFiltTab,
                        NULL
                    );

            if ( pSlapVariable )
            {
                pName = AnscCloneString(pSlapVariable->Variant.varString);

                SlapFreeVariable(pSlapVariable);
            }
        }

        pCosaContext = (PCOSA_CONTEXT_LINK_OBJECT)AnscAllocateMemory(sizeof(COSA_CONTEXT_LINK_OBJECT));

        if ( !pCosaContext )
        {
            AnscFreeMemory(pName);

            return ANSC_STATUS_FAILURE;
        }

        pMacFilt = (PCOSA_DML_WIFI_AP_MAC_FILTER)AnscAllocateMemory(sizeof(COSA_DML_WIFI_AP_MAC_FILTER));

        if ( !pMacFilt )
        {
            AnscFreeMemory(pCosaContext);

            AnscFreeMemory(pName);
            
            return ANSC_STATUS_FAILURE;
        }

        AnscCopyString(pMacFilt->Alias, pName);
    
        pCosaContext->hContext         = (ANSC_HANDLE)pMacFilt;
        pCosaContext->hParentTable     = NULL;
        pCosaContext->InstanceNumber   = uInstanceNumber;
        pCosaContext->bNew             = TRUE;
        pCosaContext->hPoamIrepUpperFo = (ANSC_HANDLE)pPoamIrepFoMacFilt;
        pCosaContext->hPoamIrepFo      = (ANSC_HANDLE)pPoamIrepFoMacFiltE;

        CosaSListPushEntryByInsNum((PSLIST_HEADER)&pMyObject->MacFilterList, pCosaContext);
        
        if ( pName )
        {
            AnscFreeMemory(pName);
            pName = NULL;
        }
    }


    return ANSC_STATUS_SUCCESS;
}

ANSC_STATUS
CosaWifiRegAddMacFiltInfo
    (
        ANSC_HANDLE                 hThisObject,
        ANSC_HANDLE                 hCosaContext
    )
{
    ANSC_STATUS                     returnStatus         = ANSC_STATUS_SUCCESS;
    PCOSA_DML_WIFI_AP_FULL          pMyObject            = (PCOSA_DML_WIFI_AP_FULL     )hThisObject;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoMacFilt  = (PPOAM_IREP_FOLDER_OBJECT )pMyObject->hIrepFolderMacFilt;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoMacFiltE = (PPOAM_IREP_FOLDER_OBJECT )NULL;
    PCOSA_CONTEXT_LINK_OBJECT       pCosaContext         = (PCOSA_CONTEXT_LINK_OBJECT)hCosaContext;
    PCOSA_DML_WIFI_AP_MAC_FILTER    pMacFilt            = (PCOSA_DML_WIFI_AP_MAC_FILTER      )NULL;
    PSINGLE_LINK_ENTRY              pSLinkEntry          = (PSINGLE_LINK_ENTRY       )NULL;
    PSLAP_VARIABLE                  pSlapVariable        = (PSLAP_VARIABLE           )NULL;
    ULONG                           ulEntryCount         = 0;
    ULONG                           ulIndex              = 0;
    ULONG                           uInstanceNumber      = 0;
    char                            FolderName[16]       = {0};

    if ( !pPoamIrepFoMacFilt || !hCosaContext)
    {
        return ANSC_STATUS_FAILURE;
    }
    else
    {
        pPoamIrepFoMacFilt->EnableFileSync((ANSC_HANDLE)pPoamIrepFoMacFilt, FALSE);
    }

    if ( TRUE )
    {
        SlapAllocVariable(pSlapVariable);

        if ( !pSlapVariable )
        {
            returnStatus = ANSC_STATUS_RESOURCES;

            goto  EXIT1;
        }
    }
    
    /*Next Instance*/
    if ( TRUE )
    {
        returnStatus = 
            pPoamIrepFoMacFilt->DelRecord
                (
                    (ANSC_HANDLE)pPoamIrepFoMacFilt,
                    COSA_DML_RR_NAME_MacFiltTabNextInsNunmber
                );        
        
        pSlapVariable->Syntax            = SLAP_VAR_SYNTAX_uint32;
        pSlapVariable->Variant.varUint32 = pMyObject->ulMacFilterNextInsNum;

        returnStatus =
            pPoamIrepFoMacFilt->AddRecord
                (
                    (ANSC_HANDLE)pPoamIrepFoMacFilt,
                    COSA_DML_RR_NAME_MacFiltTabNextInsNunmber,
                    SYS_REP_RECORD_TYPE_UINT,
                    SYS_RECORD_CONTENT_DEFAULT,
                    pSlapVariable,
                    0
                );

        SlapCleanVariable(pSlapVariable);
        SlapInitVariable (pSlapVariable);
    }

        pMacFilt    = (PCOSA_DML_WIFI_AP_MAC_FILTER)pCosaContext->hContext;

        _ansc_sprintf(FolderName, "%d", pCosaContext->InstanceNumber);

        pPoamIrepFoMacFiltE =
            pPoamIrepFoMacFilt->AddFolder
                (
                    (ANSC_HANDLE)pPoamIrepFoMacFilt,
                    FolderName,
                    0
                );

        if ( !pPoamIrepFoMacFiltE )
        {
            returnStatus = ANSC_STATUS_FAILURE;
            
            goto EXIT1;
        }

        if ( TRUE )
        {
            pSlapVariable->Syntax            = SLAP_VAR_SYNTAX_string;
            pSlapVariable->Variant.varString = AnscCloneString(pMacFilt->Alias);

            returnStatus =
                pPoamIrepFoMacFiltE->AddRecord
                    (
                        (ANSC_HANDLE)pPoamIrepFoMacFiltE,
                        COSA_DML_RR_NAME_MacFiltTab,
                        SYS_REP_RECORD_TYPE_ASTR,
                        SYS_RECORD_CONTENT_DEFAULT,
                        pSlapVariable,
                        0
                    );

            SlapCleanVariable(pSlapVariable);
            SlapInitVariable (pSlapVariable);
        }

        pCosaContext->hPoamIrepUpperFo = (ANSC_HANDLE)pPoamIrepFoMacFilt;
        pCosaContext->hPoamIrepFo      = (ANSC_HANDLE)pPoamIrepFoMacFiltE;
        
EXIT1:

    if ( pSlapVariable )
    {
        SlapFreeVariable(pSlapVariable);
    }

    pPoamIrepFoMacFilt->EnableFileSync((ANSC_HANDLE)pPoamIrepFoMacFilt, TRUE);

    return returnStatus;
}

ANSC_STATUS
CosaWifiRegDelMacFiltInfo
    (
        ANSC_HANDLE                 hThisObject,
        ANSC_HANDLE                 hCosaContext
    )
{
    ANSC_STATUS                     returnStatus         = ANSC_STATUS_SUCCESS;
    PCOSA_DML_WIFI_AP_FULL          pMyObject            = (PCOSA_DML_WIFI_AP_FULL     )hThisObject;
    PCOSA_CONTEXT_LINK_OBJECT       pCosaContext         = (PCOSA_CONTEXT_LINK_OBJECT)hCosaContext;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoMacFilt  = (PPOAM_IREP_FOLDER_OBJECT )pCosaContext->hPoamIrepUpperFo;
    PPOAM_IREP_FOLDER_OBJECT        pPoamIrepFoMacFiltE = (PPOAM_IREP_FOLDER_OBJECT )pCosaContext->hPoamIrepFo;
    UCHAR                           FolderName[16]       = {0};

    if ( !pPoamIrepFoMacFilt || !pPoamIrepFoMacFiltE)
    {
        return ANSC_STATUS_FAILURE;
    }
    else
    {
        pPoamIrepFoMacFilt->EnableFileSync((ANSC_HANDLE)pPoamIrepFoMacFilt, FALSE);
    }

    if ( TRUE )
    {
        pPoamIrepFoMacFiltE->Close((ANSC_HANDLE)pPoamIrepFoMacFiltE);
        
        pPoamIrepFoMacFilt->DelFolder
            (
                (ANSC_HANDLE)pPoamIrepFoMacFilt, 
                pPoamIrepFoMacFiltE->GetFolderName((ANSC_HANDLE)pPoamIrepFoMacFiltE)
            );

        AnscFreeMemory(pPoamIrepFoMacFiltE);
    }

    pPoamIrepFoMacFilt->EnableFileSync((ANSC_HANDLE)pPoamIrepFoMacFilt, TRUE);
    
    return returnStatus;
}
