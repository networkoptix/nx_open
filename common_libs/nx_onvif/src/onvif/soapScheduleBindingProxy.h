/* soapScheduleBindingProxy.h
   Generated by gSOAP 2.8.66 for ../result/interim/onvif.h_

gSOAP XML Web services tools
Copyright (C) 2000-2018, Robert van Engelen, Genivia Inc. All Rights Reserved.
The soapcpp2 tool and its generated software are released under the GPL.
This program is released under the GPL with the additional exemption that
compiling, linking, and/or using OpenSSL is allowed.
--------------------------------------------------------------------------------
A commercial use license is available from Genivia Inc., contact@genivia.com
--------------------------------------------------------------------------------
*/

#ifndef soapScheduleBindingProxy_H
#define soapScheduleBindingProxy_H
#include "soapH.h"

    class SOAP_CMAC ScheduleBindingProxy {
      public:
        /// Context to manage proxy IO and data
        struct soap *soap;
        bool soap_own; ///< flag indicating that this context is owned by this proxy when context is shared
        /// Endpoint URL of service 'ScheduleBindingProxy' (change as needed)
        const char *soap_endpoint;
        /// Variables globally declared in ../result/interim/onvif.h_, if any
        /// Construct a proxy with new managing context
        ScheduleBindingProxy();
        /// Copy constructor
        ScheduleBindingProxy(const ScheduleBindingProxy& rhs);
        /// Construct proxy given a shared managing context
        ScheduleBindingProxy(struct soap*);
        /// Constructor taking an endpoint URL
        ScheduleBindingProxy(const char *endpoint);
        /// Constructor taking input and output mode flags for the new managing context
        ScheduleBindingProxy(soap_mode iomode);
        /// Constructor taking endpoint URL and input and output mode flags for the new managing context
        ScheduleBindingProxy(const char *endpoint, soap_mode iomode);
        /// Constructor taking input and output mode flags for the new managing context
        ScheduleBindingProxy(soap_mode imode, soap_mode omode);
        /// Destructor deletes non-shared managing context only (use destroy() to delete deserialized data)
        virtual ~ScheduleBindingProxy();
        /// Initializer used by constructors
        virtual void ScheduleBindingProxy_init(soap_mode imode, soap_mode omode);
        /// Return a copy that has a new managing context with the same engine state
        virtual ScheduleBindingProxy *copy();
        /// Copy assignment
        ScheduleBindingProxy& operator=(const ScheduleBindingProxy&);
        /// Delete all deserialized data (uses soap_destroy() and soap_end())
        virtual void destroy();
        /// Delete all deserialized data and reset to default
        virtual void reset();
        /// Disables and removes SOAP Header from message by setting soap->header = NULL
        virtual void soap_noheader();
        /// Add SOAP Header to message
        virtual void soap_header(char *wsa5__MessageID, struct wsa5__RelatesToType *wsa5__RelatesTo, struct wsa5__EndpointReferenceType *wsa5__From, struct wsa5__EndpointReferenceType *wsa5__ReplyTo, struct wsa5__EndpointReferenceType *wsa5__FaultTo, char *wsa5__To, char *wsa5__Action, struct chan__ChannelInstanceType *chan__ChannelInstance, struct wsdd__AppSequenceType *wsdd__AppSequence, struct _wsse__Security *wsse__Security, char *SubscriptionId);
        /// Get SOAP Header structure (i.e. soap->header, which is NULL when absent)
        virtual ::SOAP_ENV__Header *soap_header();
        /// Get SOAP Fault structure (i.e. soap->fault, which is NULL when absent)
        virtual ::SOAP_ENV__Fault *soap_fault();
        /// Get SOAP Fault string (NULL when absent)
        virtual const char *soap_fault_string();
        /// Get SOAP Fault detail as string (NULL when absent)
        virtual const char *soap_fault_detail();
        /// Close connection (normally automatic, except for send_X ops)
        virtual int soap_close_socket();
        /// Force close connection (can kill a thread blocked on IO)
        virtual int soap_force_close_socket();
        /// Print fault
        virtual void soap_print_fault(FILE*);
    #ifndef WITH_LEAN
    #ifndef WITH_COMPAT
        /// Print fault to stream
        virtual void soap_stream_fault(std::ostream&);
    #endif
        /// Write fault to buffer
        virtual char *soap_sprint_fault(char *buf, size_t len);
    #endif
        /// Web service operation 'GetServiceCapabilities' (returns SOAP_OK or error code)
        virtual int GetServiceCapabilities(_onvifScedule__GetServiceCapabilities *onvifScedule__GetServiceCapabilities, _onvifScedule__GetServiceCapabilitiesResponse &onvifScedule__GetServiceCapabilitiesResponse)
        { return this->GetServiceCapabilities(NULL, NULL, onvifScedule__GetServiceCapabilities, onvifScedule__GetServiceCapabilitiesResponse); }
        virtual int GetServiceCapabilities(const char *soap_endpoint, const char *soap_action, _onvifScedule__GetServiceCapabilities *onvifScedule__GetServiceCapabilities, _onvifScedule__GetServiceCapabilitiesResponse &onvifScedule__GetServiceCapabilitiesResponse);
        /// Web service operation 'GetScheduleState' (returns SOAP_OK or error code)
        virtual int GetScheduleState(_onvifScedule__GetScheduleState *onvifScedule__GetScheduleState, _onvifScedule__GetScheduleStateResponse &onvifScedule__GetScheduleStateResponse)
        { return this->GetScheduleState(NULL, NULL, onvifScedule__GetScheduleState, onvifScedule__GetScheduleStateResponse); }
        virtual int GetScheduleState(const char *soap_endpoint, const char *soap_action, _onvifScedule__GetScheduleState *onvifScedule__GetScheduleState, _onvifScedule__GetScheduleStateResponse &onvifScedule__GetScheduleStateResponse);
        /// Web service operation 'GetScheduleInfo' (returns SOAP_OK or error code)
        virtual int GetScheduleInfo(_onvifScedule__GetScheduleInfo *onvifScedule__GetScheduleInfo, _onvifScedule__GetScheduleInfoResponse &onvifScedule__GetScheduleInfoResponse)
        { return this->GetScheduleInfo(NULL, NULL, onvifScedule__GetScheduleInfo, onvifScedule__GetScheduleInfoResponse); }
        virtual int GetScheduleInfo(const char *soap_endpoint, const char *soap_action, _onvifScedule__GetScheduleInfo *onvifScedule__GetScheduleInfo, _onvifScedule__GetScheduleInfoResponse &onvifScedule__GetScheduleInfoResponse);
        /// Web service operation 'GetScheduleInfoList' (returns SOAP_OK or error code)
        virtual int GetScheduleInfoList(_onvifScedule__GetScheduleInfoList *onvifScedule__GetScheduleInfoList, _onvifScedule__GetScheduleInfoListResponse &onvifScedule__GetScheduleInfoListResponse)
        { return this->GetScheduleInfoList(NULL, NULL, onvifScedule__GetScheduleInfoList, onvifScedule__GetScheduleInfoListResponse); }
        virtual int GetScheduleInfoList(const char *soap_endpoint, const char *soap_action, _onvifScedule__GetScheduleInfoList *onvifScedule__GetScheduleInfoList, _onvifScedule__GetScheduleInfoListResponse &onvifScedule__GetScheduleInfoListResponse);
        /// Web service operation 'GetSchedules' (returns SOAP_OK or error code)
        virtual int GetSchedules(_onvifScedule__GetSchedules *onvifScedule__GetSchedules, _onvifScedule__GetSchedulesResponse &onvifScedule__GetSchedulesResponse)
        { return this->GetSchedules(NULL, NULL, onvifScedule__GetSchedules, onvifScedule__GetSchedulesResponse); }
        virtual int GetSchedules(const char *soap_endpoint, const char *soap_action, _onvifScedule__GetSchedules *onvifScedule__GetSchedules, _onvifScedule__GetSchedulesResponse &onvifScedule__GetSchedulesResponse);
        /// Web service operation 'GetScheduleList' (returns SOAP_OK or error code)
        virtual int GetScheduleList(_onvifScedule__GetScheduleList *onvifScedule__GetScheduleList, _onvifScedule__GetScheduleListResponse &onvifScedule__GetScheduleListResponse)
        { return this->GetScheduleList(NULL, NULL, onvifScedule__GetScheduleList, onvifScedule__GetScheduleListResponse); }
        virtual int GetScheduleList(const char *soap_endpoint, const char *soap_action, _onvifScedule__GetScheduleList *onvifScedule__GetScheduleList, _onvifScedule__GetScheduleListResponse &onvifScedule__GetScheduleListResponse);
        /// Web service operation 'CreateSchedule' (returns SOAP_OK or error code)
        virtual int CreateSchedule(_onvifScedule__CreateSchedule *onvifScedule__CreateSchedule, _onvifScedule__CreateScheduleResponse &onvifScedule__CreateScheduleResponse)
        { return this->CreateSchedule(NULL, NULL, onvifScedule__CreateSchedule, onvifScedule__CreateScheduleResponse); }
        virtual int CreateSchedule(const char *soap_endpoint, const char *soap_action, _onvifScedule__CreateSchedule *onvifScedule__CreateSchedule, _onvifScedule__CreateScheduleResponse &onvifScedule__CreateScheduleResponse);
        /// Web service operation 'SetSchedule' (returns SOAP_OK or error code)
        virtual int SetSchedule(_onvifScedule__SetSchedule *onvifScedule__SetSchedule, _onvifScedule__SetScheduleResponse &onvifScedule__SetScheduleResponse)
        { return this->SetSchedule(NULL, NULL, onvifScedule__SetSchedule, onvifScedule__SetScheduleResponse); }
        virtual int SetSchedule(const char *soap_endpoint, const char *soap_action, _onvifScedule__SetSchedule *onvifScedule__SetSchedule, _onvifScedule__SetScheduleResponse &onvifScedule__SetScheduleResponse);
        /// Web service operation 'ModifySchedule' (returns SOAP_OK or error code)
        virtual int ModifySchedule(_onvifScedule__ModifySchedule *onvifScedule__ModifySchedule, _onvifScedule__ModifyScheduleResponse &onvifScedule__ModifyScheduleResponse)
        { return this->ModifySchedule(NULL, NULL, onvifScedule__ModifySchedule, onvifScedule__ModifyScheduleResponse); }
        virtual int ModifySchedule(const char *soap_endpoint, const char *soap_action, _onvifScedule__ModifySchedule *onvifScedule__ModifySchedule, _onvifScedule__ModifyScheduleResponse &onvifScedule__ModifyScheduleResponse);
        /// Web service operation 'DeleteSchedule' (returns SOAP_OK or error code)
        virtual int DeleteSchedule(_onvifScedule__DeleteSchedule *onvifScedule__DeleteSchedule, _onvifScedule__DeleteScheduleResponse &onvifScedule__DeleteScheduleResponse)
        { return this->DeleteSchedule(NULL, NULL, onvifScedule__DeleteSchedule, onvifScedule__DeleteScheduleResponse); }
        virtual int DeleteSchedule(const char *soap_endpoint, const char *soap_action, _onvifScedule__DeleteSchedule *onvifScedule__DeleteSchedule, _onvifScedule__DeleteScheduleResponse &onvifScedule__DeleteScheduleResponse);
        /// Web service operation 'GetSpecialDayGroupInfo' (returns SOAP_OK or error code)
        virtual int GetSpecialDayGroupInfo(_onvifScedule__GetSpecialDayGroupInfo *onvifScedule__GetSpecialDayGroupInfo, _onvifScedule__GetSpecialDayGroupInfoResponse &onvifScedule__GetSpecialDayGroupInfoResponse)
        { return this->GetSpecialDayGroupInfo(NULL, NULL, onvifScedule__GetSpecialDayGroupInfo, onvifScedule__GetSpecialDayGroupInfoResponse); }
        virtual int GetSpecialDayGroupInfo(const char *soap_endpoint, const char *soap_action, _onvifScedule__GetSpecialDayGroupInfo *onvifScedule__GetSpecialDayGroupInfo, _onvifScedule__GetSpecialDayGroupInfoResponse &onvifScedule__GetSpecialDayGroupInfoResponse);
        /// Web service operation 'GetSpecialDayGroupInfoList' (returns SOAP_OK or error code)
        virtual int GetSpecialDayGroupInfoList(_onvifScedule__GetSpecialDayGroupInfoList *onvifScedule__GetSpecialDayGroupInfoList, _onvifScedule__GetSpecialDayGroupInfoListResponse &onvifScedule__GetSpecialDayGroupInfoListResponse)
        { return this->GetSpecialDayGroupInfoList(NULL, NULL, onvifScedule__GetSpecialDayGroupInfoList, onvifScedule__GetSpecialDayGroupInfoListResponse); }
        virtual int GetSpecialDayGroupInfoList(const char *soap_endpoint, const char *soap_action, _onvifScedule__GetSpecialDayGroupInfoList *onvifScedule__GetSpecialDayGroupInfoList, _onvifScedule__GetSpecialDayGroupInfoListResponse &onvifScedule__GetSpecialDayGroupInfoListResponse);
        /// Web service operation 'GetSpecialDayGroups' (returns SOAP_OK or error code)
        virtual int GetSpecialDayGroups(_onvifScedule__GetSpecialDayGroups *onvifScedule__GetSpecialDayGroups, _onvifScedule__GetSpecialDayGroupsResponse &onvifScedule__GetSpecialDayGroupsResponse)
        { return this->GetSpecialDayGroups(NULL, NULL, onvifScedule__GetSpecialDayGroups, onvifScedule__GetSpecialDayGroupsResponse); }
        virtual int GetSpecialDayGroups(const char *soap_endpoint, const char *soap_action, _onvifScedule__GetSpecialDayGroups *onvifScedule__GetSpecialDayGroups, _onvifScedule__GetSpecialDayGroupsResponse &onvifScedule__GetSpecialDayGroupsResponse);
        /// Web service operation 'GetSpecialDayGroupList' (returns SOAP_OK or error code)
        virtual int GetSpecialDayGroupList(_onvifScedule__GetSpecialDayGroupList *onvifScedule__GetSpecialDayGroupList, _onvifScedule__GetSpecialDayGroupListResponse &onvifScedule__GetSpecialDayGroupListResponse)
        { return this->GetSpecialDayGroupList(NULL, NULL, onvifScedule__GetSpecialDayGroupList, onvifScedule__GetSpecialDayGroupListResponse); }
        virtual int GetSpecialDayGroupList(const char *soap_endpoint, const char *soap_action, _onvifScedule__GetSpecialDayGroupList *onvifScedule__GetSpecialDayGroupList, _onvifScedule__GetSpecialDayGroupListResponse &onvifScedule__GetSpecialDayGroupListResponse);
        /// Web service operation 'CreateSpecialDayGroup' (returns SOAP_OK or error code)
        virtual int CreateSpecialDayGroup(_onvifScedule__CreateSpecialDayGroup *onvifScedule__CreateSpecialDayGroup, _onvifScedule__CreateSpecialDayGroupResponse &onvifScedule__CreateSpecialDayGroupResponse)
        { return this->CreateSpecialDayGroup(NULL, NULL, onvifScedule__CreateSpecialDayGroup, onvifScedule__CreateSpecialDayGroupResponse); }
        virtual int CreateSpecialDayGroup(const char *soap_endpoint, const char *soap_action, _onvifScedule__CreateSpecialDayGroup *onvifScedule__CreateSpecialDayGroup, _onvifScedule__CreateSpecialDayGroupResponse &onvifScedule__CreateSpecialDayGroupResponse);
        /// Web service operation 'SetSpecialDayGroup' (returns SOAP_OK or error code)
        virtual int SetSpecialDayGroup(_onvifScedule__SetSpecialDayGroup *onvifScedule__SetSpecialDayGroup, _onvifScedule__SetSpecialDayGroupResponse &onvifScedule__SetSpecialDayGroupResponse)
        { return this->SetSpecialDayGroup(NULL, NULL, onvifScedule__SetSpecialDayGroup, onvifScedule__SetSpecialDayGroupResponse); }
        virtual int SetSpecialDayGroup(const char *soap_endpoint, const char *soap_action, _onvifScedule__SetSpecialDayGroup *onvifScedule__SetSpecialDayGroup, _onvifScedule__SetSpecialDayGroupResponse &onvifScedule__SetSpecialDayGroupResponse);
        /// Web service operation 'ModifySpecialDayGroup' (returns SOAP_OK or error code)
        virtual int ModifySpecialDayGroup(_onvifScedule__ModifySpecialDayGroup *onvifScedule__ModifySpecialDayGroup, _onvifScedule__ModifySpecialDayGroupResponse &onvifScedule__ModifySpecialDayGroupResponse)
        { return this->ModifySpecialDayGroup(NULL, NULL, onvifScedule__ModifySpecialDayGroup, onvifScedule__ModifySpecialDayGroupResponse); }
        virtual int ModifySpecialDayGroup(const char *soap_endpoint, const char *soap_action, _onvifScedule__ModifySpecialDayGroup *onvifScedule__ModifySpecialDayGroup, _onvifScedule__ModifySpecialDayGroupResponse &onvifScedule__ModifySpecialDayGroupResponse);
        /// Web service operation 'DeleteSpecialDayGroup' (returns SOAP_OK or error code)
        virtual int DeleteSpecialDayGroup(_onvifScedule__DeleteSpecialDayGroup *onvifScedule__DeleteSpecialDayGroup, _onvifScedule__DeleteSpecialDayGroupResponse &onvifScedule__DeleteSpecialDayGroupResponse)
        { return this->DeleteSpecialDayGroup(NULL, NULL, onvifScedule__DeleteSpecialDayGroup, onvifScedule__DeleteSpecialDayGroupResponse); }
        virtual int DeleteSpecialDayGroup(const char *soap_endpoint, const char *soap_action, _onvifScedule__DeleteSpecialDayGroup *onvifScedule__DeleteSpecialDayGroup, _onvifScedule__DeleteSpecialDayGroupResponse &onvifScedule__DeleteSpecialDayGroupResponse);
    };
#endif
