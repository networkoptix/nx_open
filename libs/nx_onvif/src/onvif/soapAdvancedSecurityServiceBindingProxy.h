/* soapAdvancedSecurityServiceBindingProxy.h
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

#ifndef soapAdvancedSecurityServiceBindingProxy_H
#define soapAdvancedSecurityServiceBindingProxy_H
#include "soapH.h"

    class SOAP_CMAC AdvancedSecurityServiceBindingProxy {
      public:
        /// Context to manage proxy IO and data
        struct soap *soap;
        bool soap_own; ///< flag indicating that this context is owned by this proxy when context is shared
        /// Endpoint URL of service 'AdvancedSecurityServiceBindingProxy' (change as needed)
        const char *soap_endpoint;
        /// Variables globally declared in ../result/interim/onvif.h_, if any
        /// Construct a proxy with new managing context
        AdvancedSecurityServiceBindingProxy();
        /// Copy constructor
        AdvancedSecurityServiceBindingProxy(const AdvancedSecurityServiceBindingProxy& rhs);
        /// Construct proxy given a shared managing context
        AdvancedSecurityServiceBindingProxy(struct soap*);
        /// Constructor taking an endpoint URL
        AdvancedSecurityServiceBindingProxy(const char *endpoint);
        /// Constructor taking input and output mode flags for the new managing context
        AdvancedSecurityServiceBindingProxy(soap_mode iomode);
        /// Constructor taking endpoint URL and input and output mode flags for the new managing context
        AdvancedSecurityServiceBindingProxy(const char *endpoint, soap_mode iomode);
        /// Constructor taking input and output mode flags for the new managing context
        AdvancedSecurityServiceBindingProxy(soap_mode imode, soap_mode omode);
        /// Destructor deletes non-shared managing context only (use destroy() to delete deserialized data)
        virtual ~AdvancedSecurityServiceBindingProxy();
        /// Initializer used by constructors
        virtual void AdvancedSecurityServiceBindingProxy_init(soap_mode imode, soap_mode omode);
        /// Return a copy that has a new managing context with the same engine state
        virtual AdvancedSecurityServiceBindingProxy *copy();
        /// Copy assignment
        AdvancedSecurityServiceBindingProxy& operator=(const AdvancedSecurityServiceBindingProxy&);
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
        virtual int GetServiceCapabilities(_onvifAdvancedSecurity__GetServiceCapabilities *onvifAdvancedSecurity__GetServiceCapabilities, _onvifAdvancedSecurity__GetServiceCapabilitiesResponse &onvifAdvancedSecurity__GetServiceCapabilitiesResponse)
        { return this->GetServiceCapabilities(NULL, NULL, onvifAdvancedSecurity__GetServiceCapabilities, onvifAdvancedSecurity__GetServiceCapabilitiesResponse); }
        virtual int GetServiceCapabilities(const char *soap_endpoint, const char *soap_action, _onvifAdvancedSecurity__GetServiceCapabilities *onvifAdvancedSecurity__GetServiceCapabilities, _onvifAdvancedSecurity__GetServiceCapabilitiesResponse &onvifAdvancedSecurity__GetServiceCapabilitiesResponse);
    };
#endif
