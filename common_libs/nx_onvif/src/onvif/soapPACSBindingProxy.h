/* soapPACSBindingProxy.h
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

#ifndef soapPACSBindingProxy_H
#define soapPACSBindingProxy_H
#include "soapH.h"

    class SOAP_CMAC PACSBindingProxy {
      public:
        /// Context to manage proxy IO and data
        struct soap *soap;
        bool soap_own; ///< flag indicating that this context is owned by this proxy when context is shared
        /// Endpoint URL of service 'PACSBindingProxy' (change as needed)
        const char *soap_endpoint;
        /// Variables globally declared in ../result/interim/onvif.h_, if any
        /// Construct a proxy with new managing context
        PACSBindingProxy();
        /// Copy constructor
        PACSBindingProxy(const PACSBindingProxy& rhs);
        /// Construct proxy given a shared managing context
        PACSBindingProxy(struct soap*);
        /// Constructor taking an endpoint URL
        PACSBindingProxy(const char *endpoint);
        /// Constructor taking input and output mode flags for the new managing context
        PACSBindingProxy(soap_mode iomode);
        /// Constructor taking endpoint URL and input and output mode flags for the new managing context
        PACSBindingProxy(const char *endpoint, soap_mode iomode);
        /// Constructor taking input and output mode flags for the new managing context
        PACSBindingProxy(soap_mode imode, soap_mode omode);
        /// Destructor deletes non-shared managing context only (use destroy() to delete deserialized data)
        virtual ~PACSBindingProxy();
        /// Initializer used by constructors
        virtual void PACSBindingProxy_init(soap_mode imode, soap_mode omode);
        /// Return a copy that has a new managing context with the same engine state
        virtual PACSBindingProxy *copy();
        /// Copy assignment
        PACSBindingProxy& operator=(const PACSBindingProxy&);
        /// Delete all deserialized data (uses soap_destroy() and soap_end())
        virtual void destroy();
        /// Delete all deserialized data and reset to default
        virtual void reset();
        /// Disables and removes SOAP Header from message by setting soap->header = NULL
        virtual void soap_noheader();
        /// Add SOAP Header to message
        virtual void soap_header(char *wsa5__MessageID, struct wsa5__RelatesToType *wsa5__RelatesTo, struct wsa5__EndpointReferenceType *wsa5__From, struct wsa5__EndpointReferenceType *wsa5__ReplyTo, struct wsa5__EndpointReferenceType *wsa5__FaultTo, char *wsa5__To, char *wsa5__Action, struct chan__ChannelInstanceType *chan__ChannelInstance, struct wsdd__AppSequenceType *wsdd__AppSequence, struct _wsse__Security *wsse__Security, char *subscriptionID);
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
        virtual int GetServiceCapabilities(_onvifAccessControl__GetServiceCapabilities *onvifAccessControl__GetServiceCapabilities, _onvifAccessControl__GetServiceCapabilitiesResponse &onvifAccessControl__GetServiceCapabilitiesResponse)
        { return this->GetServiceCapabilities(NULL, NULL, onvifAccessControl__GetServiceCapabilities, onvifAccessControl__GetServiceCapabilitiesResponse); }
        virtual int GetServiceCapabilities(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__GetServiceCapabilities *onvifAccessControl__GetServiceCapabilities, _onvifAccessControl__GetServiceCapabilitiesResponse &onvifAccessControl__GetServiceCapabilitiesResponse);
        /// Web service operation 'GetAccessPointInfoList' (returns SOAP_OK or error code)
        virtual int GetAccessPointInfoList(_onvifAccessControl__GetAccessPointInfoList *onvifAccessControl__GetAccessPointInfoList, _onvifAccessControl__GetAccessPointInfoListResponse &onvifAccessControl__GetAccessPointInfoListResponse)
        { return this->GetAccessPointInfoList(NULL, NULL, onvifAccessControl__GetAccessPointInfoList, onvifAccessControl__GetAccessPointInfoListResponse); }
        virtual int GetAccessPointInfoList(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__GetAccessPointInfoList *onvifAccessControl__GetAccessPointInfoList, _onvifAccessControl__GetAccessPointInfoListResponse &onvifAccessControl__GetAccessPointInfoListResponse);
        /// Web service operation 'GetAccessPointInfo' (returns SOAP_OK or error code)
        virtual int GetAccessPointInfo(_onvifAccessControl__GetAccessPointInfo *onvifAccessControl__GetAccessPointInfo, _onvifAccessControl__GetAccessPointInfoResponse &onvifAccessControl__GetAccessPointInfoResponse)
        { return this->GetAccessPointInfo(NULL, NULL, onvifAccessControl__GetAccessPointInfo, onvifAccessControl__GetAccessPointInfoResponse); }
        virtual int GetAccessPointInfo(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__GetAccessPointInfo *onvifAccessControl__GetAccessPointInfo, _onvifAccessControl__GetAccessPointInfoResponse &onvifAccessControl__GetAccessPointInfoResponse);
        /// Web service operation 'GetAccessPointList' (returns SOAP_OK or error code)
        virtual int GetAccessPointList(_onvifAccessControl__GetAccessPointList *onvifAccessControl__GetAccessPointList, _onvifAccessControl__GetAccessPointListResponse &onvifAccessControl__GetAccessPointListResponse)
        { return this->GetAccessPointList(NULL, NULL, onvifAccessControl__GetAccessPointList, onvifAccessControl__GetAccessPointListResponse); }
        virtual int GetAccessPointList(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__GetAccessPointList *onvifAccessControl__GetAccessPointList, _onvifAccessControl__GetAccessPointListResponse &onvifAccessControl__GetAccessPointListResponse);
        /// Web service operation 'GetAccessPoints' (returns SOAP_OK or error code)
        virtual int GetAccessPoints(_onvifAccessControl__GetAccessPoints *onvifAccessControl__GetAccessPoints, _onvifAccessControl__GetAccessPointsResponse &onvifAccessControl__GetAccessPointsResponse)
        { return this->GetAccessPoints(NULL, NULL, onvifAccessControl__GetAccessPoints, onvifAccessControl__GetAccessPointsResponse); }
        virtual int GetAccessPoints(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__GetAccessPoints *onvifAccessControl__GetAccessPoints, _onvifAccessControl__GetAccessPointsResponse &onvifAccessControl__GetAccessPointsResponse);
        /// Web service operation 'CreateAccessPoint' (returns SOAP_OK or error code)
        virtual int CreateAccessPoint(_onvifAccessControl__CreateAccessPoint *onvifAccessControl__CreateAccessPoint, _onvifAccessControl__CreateAccessPointResponse &onvifAccessControl__CreateAccessPointResponse)
        { return this->CreateAccessPoint(NULL, NULL, onvifAccessControl__CreateAccessPoint, onvifAccessControl__CreateAccessPointResponse); }
        virtual int CreateAccessPoint(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__CreateAccessPoint *onvifAccessControl__CreateAccessPoint, _onvifAccessControl__CreateAccessPointResponse &onvifAccessControl__CreateAccessPointResponse);
        /// Web service operation 'SetAccessPoint' (returns SOAP_OK or error code)
        virtual int SetAccessPoint(_onvifAccessControl__SetAccessPoint *onvifAccessControl__SetAccessPoint, _onvifAccessControl__SetAccessPointResponse &onvifAccessControl__SetAccessPointResponse)
        { return this->SetAccessPoint(NULL, NULL, onvifAccessControl__SetAccessPoint, onvifAccessControl__SetAccessPointResponse); }
        virtual int SetAccessPoint(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__SetAccessPoint *onvifAccessControl__SetAccessPoint, _onvifAccessControl__SetAccessPointResponse &onvifAccessControl__SetAccessPointResponse);
        /// Web service operation 'ModifyAccessPoint' (returns SOAP_OK or error code)
        virtual int ModifyAccessPoint(_onvifAccessControl__ModifyAccessPoint *onvifAccessControl__ModifyAccessPoint, _onvifAccessControl__ModifyAccessPointResponse &onvifAccessControl__ModifyAccessPointResponse)
        { return this->ModifyAccessPoint(NULL, NULL, onvifAccessControl__ModifyAccessPoint, onvifAccessControl__ModifyAccessPointResponse); }
        virtual int ModifyAccessPoint(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__ModifyAccessPoint *onvifAccessControl__ModifyAccessPoint, _onvifAccessControl__ModifyAccessPointResponse &onvifAccessControl__ModifyAccessPointResponse);
        /// Web service operation 'DeleteAccessPoint' (returns SOAP_OK or error code)
        virtual int DeleteAccessPoint(_onvifAccessControl__DeleteAccessPoint *onvifAccessControl__DeleteAccessPoint, _onvifAccessControl__DeleteAccessPointResponse &onvifAccessControl__DeleteAccessPointResponse)
        { return this->DeleteAccessPoint(NULL, NULL, onvifAccessControl__DeleteAccessPoint, onvifAccessControl__DeleteAccessPointResponse); }
        virtual int DeleteAccessPoint(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__DeleteAccessPoint *onvifAccessControl__DeleteAccessPoint, _onvifAccessControl__DeleteAccessPointResponse &onvifAccessControl__DeleteAccessPointResponse);
        /// Web service operation 'SetAccessPointAuthenticationProfile' (returns SOAP_OK or error code)
        virtual int SetAccessPointAuthenticationProfile(_onvifAccessControl__SetAccessPointAuthenticationProfile *onvifAccessControl__SetAccessPointAuthenticationProfile, _onvifAccessControl__SetAccessPointAuthenticationProfileResponse &onvifAccessControl__SetAccessPointAuthenticationProfileResponse)
        { return this->SetAccessPointAuthenticationProfile(NULL, NULL, onvifAccessControl__SetAccessPointAuthenticationProfile, onvifAccessControl__SetAccessPointAuthenticationProfileResponse); }
        virtual int SetAccessPointAuthenticationProfile(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__SetAccessPointAuthenticationProfile *onvifAccessControl__SetAccessPointAuthenticationProfile, _onvifAccessControl__SetAccessPointAuthenticationProfileResponse &onvifAccessControl__SetAccessPointAuthenticationProfileResponse);
        /// Web service operation 'DeleteAccessPointAuthenticationProfile' (returns SOAP_OK or error code)
        virtual int DeleteAccessPointAuthenticationProfile(_onvifAccessControl__DeleteAccessPointAuthenticationProfile *onvifAccessControl__DeleteAccessPointAuthenticationProfile, _onvifAccessControl__DeleteAccessPointAuthenticationProfileResponse &onvifAccessControl__DeleteAccessPointAuthenticationProfileResponse)
        { return this->DeleteAccessPointAuthenticationProfile(NULL, NULL, onvifAccessControl__DeleteAccessPointAuthenticationProfile, onvifAccessControl__DeleteAccessPointAuthenticationProfileResponse); }
        virtual int DeleteAccessPointAuthenticationProfile(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__DeleteAccessPointAuthenticationProfile *onvifAccessControl__DeleteAccessPointAuthenticationProfile, _onvifAccessControl__DeleteAccessPointAuthenticationProfileResponse &onvifAccessControl__DeleteAccessPointAuthenticationProfileResponse);
        /// Web service operation 'GetAreaInfoList' (returns SOAP_OK or error code)
        virtual int GetAreaInfoList(_onvifAccessControl__GetAreaInfoList *onvifAccessControl__GetAreaInfoList, _onvifAccessControl__GetAreaInfoListResponse &onvifAccessControl__GetAreaInfoListResponse)
        { return this->GetAreaInfoList(NULL, NULL, onvifAccessControl__GetAreaInfoList, onvifAccessControl__GetAreaInfoListResponse); }
        virtual int GetAreaInfoList(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__GetAreaInfoList *onvifAccessControl__GetAreaInfoList, _onvifAccessControl__GetAreaInfoListResponse &onvifAccessControl__GetAreaInfoListResponse);
        /// Web service operation 'GetAreaInfo' (returns SOAP_OK or error code)
        virtual int GetAreaInfo(_onvifAccessControl__GetAreaInfo *onvifAccessControl__GetAreaInfo, _onvifAccessControl__GetAreaInfoResponse &onvifAccessControl__GetAreaInfoResponse)
        { return this->GetAreaInfo(NULL, NULL, onvifAccessControl__GetAreaInfo, onvifAccessControl__GetAreaInfoResponse); }
        virtual int GetAreaInfo(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__GetAreaInfo *onvifAccessControl__GetAreaInfo, _onvifAccessControl__GetAreaInfoResponse &onvifAccessControl__GetAreaInfoResponse);
        /// Web service operation 'GetAreaList' (returns SOAP_OK or error code)
        virtual int GetAreaList(_onvifAccessControl__GetAreaList *onvifAccessControl__GetAreaList, _onvifAccessControl__GetAreaListResponse &onvifAccessControl__GetAreaListResponse)
        { return this->GetAreaList(NULL, NULL, onvifAccessControl__GetAreaList, onvifAccessControl__GetAreaListResponse); }
        virtual int GetAreaList(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__GetAreaList *onvifAccessControl__GetAreaList, _onvifAccessControl__GetAreaListResponse &onvifAccessControl__GetAreaListResponse);
        /// Web service operation 'GetAreas' (returns SOAP_OK or error code)
        virtual int GetAreas(_onvifAccessControl__GetAreas *onvifAccessControl__GetAreas, _onvifAccessControl__GetAreasResponse &onvifAccessControl__GetAreasResponse)
        { return this->GetAreas(NULL, NULL, onvifAccessControl__GetAreas, onvifAccessControl__GetAreasResponse); }
        virtual int GetAreas(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__GetAreas *onvifAccessControl__GetAreas, _onvifAccessControl__GetAreasResponse &onvifAccessControl__GetAreasResponse);
        /// Web service operation 'CreateArea' (returns SOAP_OK or error code)
        virtual int CreateArea(_onvifAccessControl__CreateArea *onvifAccessControl__CreateArea, _onvifAccessControl__CreateAreaResponse &onvifAccessControl__CreateAreaResponse)
        { return this->CreateArea(NULL, NULL, onvifAccessControl__CreateArea, onvifAccessControl__CreateAreaResponse); }
        virtual int CreateArea(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__CreateArea *onvifAccessControl__CreateArea, _onvifAccessControl__CreateAreaResponse &onvifAccessControl__CreateAreaResponse);
        /// Web service operation 'SetArea' (returns SOAP_OK or error code)
        virtual int SetArea(_onvifAccessControl__SetArea *onvifAccessControl__SetArea, _onvifAccessControl__SetAreaResponse &onvifAccessControl__SetAreaResponse)
        { return this->SetArea(NULL, NULL, onvifAccessControl__SetArea, onvifAccessControl__SetAreaResponse); }
        virtual int SetArea(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__SetArea *onvifAccessControl__SetArea, _onvifAccessControl__SetAreaResponse &onvifAccessControl__SetAreaResponse);
        /// Web service operation 'ModifyArea' (returns SOAP_OK or error code)
        virtual int ModifyArea(_onvifAccessControl__ModifyArea *onvifAccessControl__ModifyArea, _onvifAccessControl__ModifyAreaResponse &onvifAccessControl__ModifyAreaResponse)
        { return this->ModifyArea(NULL, NULL, onvifAccessControl__ModifyArea, onvifAccessControl__ModifyAreaResponse); }
        virtual int ModifyArea(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__ModifyArea *onvifAccessControl__ModifyArea, _onvifAccessControl__ModifyAreaResponse &onvifAccessControl__ModifyAreaResponse);
        /// Web service operation 'DeleteArea' (returns SOAP_OK or error code)
        virtual int DeleteArea(_onvifAccessControl__DeleteArea *onvifAccessControl__DeleteArea, _onvifAccessControl__DeleteAreaResponse &onvifAccessControl__DeleteAreaResponse)
        { return this->DeleteArea(NULL, NULL, onvifAccessControl__DeleteArea, onvifAccessControl__DeleteAreaResponse); }
        virtual int DeleteArea(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__DeleteArea *onvifAccessControl__DeleteArea, _onvifAccessControl__DeleteAreaResponse &onvifAccessControl__DeleteAreaResponse);
        /// Web service operation 'GetAccessPointState' (returns SOAP_OK or error code)
        virtual int GetAccessPointState(_onvifAccessControl__GetAccessPointState *onvifAccessControl__GetAccessPointState, _onvifAccessControl__GetAccessPointStateResponse &onvifAccessControl__GetAccessPointStateResponse)
        { return this->GetAccessPointState(NULL, NULL, onvifAccessControl__GetAccessPointState, onvifAccessControl__GetAccessPointStateResponse); }
        virtual int GetAccessPointState(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__GetAccessPointState *onvifAccessControl__GetAccessPointState, _onvifAccessControl__GetAccessPointStateResponse &onvifAccessControl__GetAccessPointStateResponse);
        /// Web service operation 'EnableAccessPoint' (returns SOAP_OK or error code)
        virtual int EnableAccessPoint(_onvifAccessControl__EnableAccessPoint *onvifAccessControl__EnableAccessPoint, _onvifAccessControl__EnableAccessPointResponse &onvifAccessControl__EnableAccessPointResponse)
        { return this->EnableAccessPoint(NULL, NULL, onvifAccessControl__EnableAccessPoint, onvifAccessControl__EnableAccessPointResponse); }
        virtual int EnableAccessPoint(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__EnableAccessPoint *onvifAccessControl__EnableAccessPoint, _onvifAccessControl__EnableAccessPointResponse &onvifAccessControl__EnableAccessPointResponse);
        /// Web service operation 'DisableAccessPoint' (returns SOAP_OK or error code)
        virtual int DisableAccessPoint(_onvifAccessControl__DisableAccessPoint *onvifAccessControl__DisableAccessPoint, _onvifAccessControl__DisableAccessPointResponse &onvifAccessControl__DisableAccessPointResponse)
        { return this->DisableAccessPoint(NULL, NULL, onvifAccessControl__DisableAccessPoint, onvifAccessControl__DisableAccessPointResponse); }
        virtual int DisableAccessPoint(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__DisableAccessPoint *onvifAccessControl__DisableAccessPoint, _onvifAccessControl__DisableAccessPointResponse &onvifAccessControl__DisableAccessPointResponse);
        /// Web service operation 'ExternalAuthorization' (returns SOAP_OK or error code)
        virtual int ExternalAuthorization(_onvifAccessControl__ExternalAuthorization *onvifAccessControl__ExternalAuthorization, _onvifAccessControl__ExternalAuthorizationResponse &onvifAccessControl__ExternalAuthorizationResponse)
        { return this->ExternalAuthorization(NULL, NULL, onvifAccessControl__ExternalAuthorization, onvifAccessControl__ExternalAuthorizationResponse); }
        virtual int ExternalAuthorization(const char *soap_endpoint, const char *soap_action, _onvifAccessControl__ExternalAuthorization *onvifAccessControl__ExternalAuthorization, _onvifAccessControl__ExternalAuthorizationResponse &onvifAccessControl__ExternalAuthorizationResponse);
    };
#endif
