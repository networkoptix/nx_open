/* soapActionEngineBindingProxy.h
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

#ifndef soapActionEngineBindingProxy_H
#define soapActionEngineBindingProxy_H
#include "soapH.h"

    class SOAP_CMAC ActionEngineBindingProxy {
      public:
        /// Context to manage proxy IO and data
        struct soap *soap;
        bool soap_own; ///< flag indicating that this context is owned by this proxy when context is shared
        /// Endpoint URL of service 'ActionEngineBindingProxy' (change as needed)
        const char *soap_endpoint;
        /// Variables globally declared in ../result/interim/onvif.h_, if any
        /// Construct a proxy with new managing context
        ActionEngineBindingProxy();
        /// Copy constructor
        ActionEngineBindingProxy(const ActionEngineBindingProxy& rhs);
        /// Construct proxy given a shared managing context
        ActionEngineBindingProxy(struct soap*);
        /// Constructor taking an endpoint URL
        ActionEngineBindingProxy(const char *endpoint);
        /// Constructor taking input and output mode flags for the new managing context
        ActionEngineBindingProxy(soap_mode iomode);
        /// Constructor taking endpoint URL and input and output mode flags for the new managing context
        ActionEngineBindingProxy(const char *endpoint, soap_mode iomode);
        /// Constructor taking input and output mode flags for the new managing context
        ActionEngineBindingProxy(soap_mode imode, soap_mode omode);
        /// Destructor deletes non-shared managing context only (use destroy() to delete deserialized data)
        virtual ~ActionEngineBindingProxy();
        /// Initializer used by constructors
        virtual void ActionEngineBindingProxy_init(soap_mode imode, soap_mode omode);
        /// Return a copy that has a new managing context with the same engine state
        virtual ActionEngineBindingProxy *copy();
        /// Copy assignment
        ActionEngineBindingProxy& operator=(const ActionEngineBindingProxy&);
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
        /// Web service operation 'GetSupportedActions' (returns SOAP_OK or error code)
        virtual int GetSupportedActions(_onvifActionEngine__GetSupportedActions *onvifActionEngine__GetSupportedActions, _onvifActionEngine__GetSupportedActionsResponse &onvifActionEngine__GetSupportedActionsResponse)
        { return this->GetSupportedActions(NULL, NULL, onvifActionEngine__GetSupportedActions, onvifActionEngine__GetSupportedActionsResponse); }
        virtual int GetSupportedActions(const char *soap_endpoint, const char *soap_action, _onvifActionEngine__GetSupportedActions *onvifActionEngine__GetSupportedActions, _onvifActionEngine__GetSupportedActionsResponse &onvifActionEngine__GetSupportedActionsResponse);
        /// Web service operation 'GetActions' (returns SOAP_OK or error code)
        virtual int GetActions(_onvifActionEngine__GetActions *onvifActionEngine__GetActions, _onvifActionEngine__GetActionsResponse &onvifActionEngine__GetActionsResponse)
        { return this->GetActions(NULL, NULL, onvifActionEngine__GetActions, onvifActionEngine__GetActionsResponse); }
        virtual int GetActions(const char *soap_endpoint, const char *soap_action, _onvifActionEngine__GetActions *onvifActionEngine__GetActions, _onvifActionEngine__GetActionsResponse &onvifActionEngine__GetActionsResponse);
        /// Web service operation 'CreateActions' (returns SOAP_OK or error code)
        virtual int CreateActions(_onvifActionEngine__CreateActions *onvifActionEngine__CreateActions, _onvifActionEngine__CreateActionsResponse &onvifActionEngine__CreateActionsResponse)
        { return this->CreateActions(NULL, NULL, onvifActionEngine__CreateActions, onvifActionEngine__CreateActionsResponse); }
        virtual int CreateActions(const char *soap_endpoint, const char *soap_action, _onvifActionEngine__CreateActions *onvifActionEngine__CreateActions, _onvifActionEngine__CreateActionsResponse &onvifActionEngine__CreateActionsResponse);
        /// Web service operation 'DeleteActions' (returns SOAP_OK or error code)
        virtual int DeleteActions(_onvifActionEngine__DeleteActions *onvifActionEngine__DeleteActions, _onvifActionEngine__DeleteActionsResponse &onvifActionEngine__DeleteActionsResponse)
        { return this->DeleteActions(NULL, NULL, onvifActionEngine__DeleteActions, onvifActionEngine__DeleteActionsResponse); }
        virtual int DeleteActions(const char *soap_endpoint, const char *soap_action, _onvifActionEngine__DeleteActions *onvifActionEngine__DeleteActions, _onvifActionEngine__DeleteActionsResponse &onvifActionEngine__DeleteActionsResponse);
        /// Web service operation 'ModifyActions' (returns SOAP_OK or error code)
        virtual int ModifyActions(_onvifActionEngine__ModifyActions *onvifActionEngine__ModifyActions, _onvifActionEngine__ModifyActionsResponse &onvifActionEngine__ModifyActionsResponse)
        { return this->ModifyActions(NULL, NULL, onvifActionEngine__ModifyActions, onvifActionEngine__ModifyActionsResponse); }
        virtual int ModifyActions(const char *soap_endpoint, const char *soap_action, _onvifActionEngine__ModifyActions *onvifActionEngine__ModifyActions, _onvifActionEngine__ModifyActionsResponse &onvifActionEngine__ModifyActionsResponse);
        /// Web service operation 'GetServiceCapabilities' (returns SOAP_OK or error code)
        virtual int GetServiceCapabilities(_onvifActionEngine__GetServiceCapabilities *onvifActionEngine__GetServiceCapabilities, _onvifActionEngine__GetServiceCapabilitiesResponse &onvifActionEngine__GetServiceCapabilitiesResponse)
        { return this->GetServiceCapabilities(NULL, NULL, onvifActionEngine__GetServiceCapabilities, onvifActionEngine__GetServiceCapabilitiesResponse); }
        virtual int GetServiceCapabilities(const char *soap_endpoint, const char *soap_action, _onvifActionEngine__GetServiceCapabilities *onvifActionEngine__GetServiceCapabilities, _onvifActionEngine__GetServiceCapabilitiesResponse &onvifActionEngine__GetServiceCapabilitiesResponse);
        /// Web service operation 'GetActionTriggers' (returns SOAP_OK or error code)
        virtual int GetActionTriggers(_onvifActionEngine__GetActionTriggers *onvifActionEngine__GetActionTriggers, _onvifActionEngine__GetActionTriggersResponse &onvifActionEngine__GetActionTriggersResponse)
        { return this->GetActionTriggers(NULL, NULL, onvifActionEngine__GetActionTriggers, onvifActionEngine__GetActionTriggersResponse); }
        virtual int GetActionTriggers(const char *soap_endpoint, const char *soap_action, _onvifActionEngine__GetActionTriggers *onvifActionEngine__GetActionTriggers, _onvifActionEngine__GetActionTriggersResponse &onvifActionEngine__GetActionTriggersResponse);
        /// Web service operation 'CreateActionTriggers' (returns SOAP_OK or error code)
        virtual int CreateActionTriggers(_onvifActionEngine__CreateActionTriggers *onvifActionEngine__CreateActionTriggers, _onvifActionEngine__CreateActionTriggersResponse &onvifActionEngine__CreateActionTriggersResponse)
        { return this->CreateActionTriggers(NULL, NULL, onvifActionEngine__CreateActionTriggers, onvifActionEngine__CreateActionTriggersResponse); }
        virtual int CreateActionTriggers(const char *soap_endpoint, const char *soap_action, _onvifActionEngine__CreateActionTriggers *onvifActionEngine__CreateActionTriggers, _onvifActionEngine__CreateActionTriggersResponse &onvifActionEngine__CreateActionTriggersResponse);
        /// Web service operation 'DeleteActionTriggers' (returns SOAP_OK or error code)
        virtual int DeleteActionTriggers(_onvifActionEngine__DeleteActionTriggers *onvifActionEngine__DeleteActionTriggers, _onvifActionEngine__DeleteActionTriggersResponse &onvifActionEngine__DeleteActionTriggersResponse)
        { return this->DeleteActionTriggers(NULL, NULL, onvifActionEngine__DeleteActionTriggers, onvifActionEngine__DeleteActionTriggersResponse); }
        virtual int DeleteActionTriggers(const char *soap_endpoint, const char *soap_action, _onvifActionEngine__DeleteActionTriggers *onvifActionEngine__DeleteActionTriggers, _onvifActionEngine__DeleteActionTriggersResponse &onvifActionEngine__DeleteActionTriggersResponse);
        /// Web service operation 'ModifyActionTriggers' (returns SOAP_OK or error code)
        virtual int ModifyActionTriggers(_onvifActionEngine__ModifyActionTriggers *onvifActionEngine__ModifyActionTriggers, _onvifActionEngine__ModifyActionTriggersResponse &onvifActionEngine__ModifyActionTriggersResponse)
        { return this->ModifyActionTriggers(NULL, NULL, onvifActionEngine__ModifyActionTriggers, onvifActionEngine__ModifyActionTriggersResponse); }
        virtual int ModifyActionTriggers(const char *soap_endpoint, const char *soap_action, _onvifActionEngine__ModifyActionTriggers *onvifActionEngine__ModifyActionTriggers, _onvifActionEngine__ModifyActionTriggersResponse &onvifActionEngine__ModifyActionTriggersResponse);
    };
#endif
