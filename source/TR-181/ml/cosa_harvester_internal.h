/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2019 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#ifndef  _COSA_HARVESTER_INTERNAL_H
#define  _COSA_HARVESTER_INTERNAL_H


#include "ansc_platform.h"
#include "ansc_string_util.h"

#define COSA_DATAMODEL_HARVESTER_OID                                                        1

/* Collection */

#define  COSA_DATAMODEL_HARVESTER_CLASS_CONTENT                             \
    /* start of WIFI object class content */                                \
    BOOLEAN                         bIDWEnabled;                        \
    BOOLEAN                         bIDWEnabledChanged;                 \
    ULONG                           uIDWPollingPeriod;                      \
    BOOLEAN                         bIDWPollingPeriodChanged;               \
    ULONG                           uIDWReportingPeriod;                    \
    BOOLEAN                         bIDWReportingPeriodChanged;             \
    ULONG                           uIDWPollingPeriodDefault;               \
    ULONG                           uIDWReportingPeriodDefault;             \
    ULONG                           uIDWOverrideTTL;                        \
    BOOLEAN                         bIDWDefaultPollingPeriodChanged;        \
    BOOLEAN                         bIDWDefaultReportingPeriodChanged;      \
    ULONG                           uIDWDefaultPollingPeriod;               \
    ULONG                           uIDWDefaultReportingPeriod;             \
    BOOLEAN                         bNAPEnabled;                       \
    BOOLEAN                         bNAPEnabledChanged;                \
    ULONG                           uNAPPollingPeriod;                     \
    BOOLEAN                         bNAPPollingPeriodChanged;              \
    ULONG                           uNAPReportingPeriod;                   \
    BOOLEAN                         bNAPReportingPeriodChanged;            \
    ULONG                           uNAPPollingPeriodDefault;              \
    ULONG                           uNAPReportingPeriodDefault;            \
    ULONG                           uNAPOverrideTTL;                       \
    BOOLEAN                         bNAPDefaultPollingPeriodChanged;        \
    BOOLEAN                         bNAPDefaultReportingPeriodChanged;      \
    ULONG                           uNAPDefaultPollingPeriod;               \
    ULONG                           uNAPDefaultReportingPeriod;             \
    BOOLEAN                         bNAPOnDemandEnabled;               \
    BOOLEAN                         bNAPOnDemandEnabledChanged;        \
    BOOLEAN                         bRISEnabled;                        \
    BOOLEAN                         bRISEnabledChanged;                 \
    ULONG                           uRISPollingPeriod;                      \
    BOOLEAN                         bRISPollingPeriodChanged;               \
    ULONG                           uRISReportingPeriod;                    \
    BOOLEAN                         bRISReportingPeriodChanged;             \
    ULONG                           uRISPollingPeriodDefault;               \
    ULONG                           uRISReportingPeriodDefault;             \
    ULONG                           uRISOverrideTTL;                        \
    BOOLEAN                         bRISDefaultPollingPeriodChanged;        \
    BOOLEAN                         bRISDefaultReportingPeriodChanged;      \
    ULONG                           uRISDefaultPollingPeriod;               \
    ULONG                           uRISDefaultReportingPeriod;




typedef  struct
_COSA_DATAMODEL_HARVESTER                                               
{
	COSA_DATAMODEL_HARVESTER_CLASS_CONTENT
}
COSA_DATAMODEL_HARVESTER,  *PCOSA_DATAMODEL_HARVESTER;

/*
    Standard function declaration 
*/

ANSC_STATUS
CosaDmlHarvesterInit
  (
        PCOSA_DML_WIFI_HARVESTER pHarvester
  );

#if 0
ANSC_HANDLE
CosaHarvesterCreate
    (
        VOID
    );

ANSC_STATUS
CosaHarvesterInitialize
    (
        ANSC_HANDLE                 hThisObject
    );

ANSC_STATUS
CosaHarvesterRemove
    (
        ANSC_HANDLE                 hThisObject
    );
#endif    

#endif 
