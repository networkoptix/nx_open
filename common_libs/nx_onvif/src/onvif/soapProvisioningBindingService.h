/* soapProvisioningBindingService.h
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

#ifndef soapProvisioningBindingService_H
#define soapProvisioningBindingService_H
#include "soapH.h"

    class SOAP_CMAC ProvisioningBindingService {
      public:
        /// Context to manage service IO and data
        struct soap *soap;
        bool soap_own;  ///< flag indicating that this context is owned by this service when context is shared
        /// Variables globally declared in ../result/interim/onvif.h_, if any
        /// Construct a service with new managing context
        ProvisioningBindingService();
        /// Copy constructor
        ProvisioningBindingService(const ProvisioningBindingService&);
        /// Construct service given a shared managing context
        ProvisioningBindingService(struct soap*);
        /// Constructor taking input+output mode flags for the new managing context
        ProvisioningBindingService(soap_mode iomode);
        /// Constructor taking input and output mode flags for the new managing context
        ProvisioningBindingService(soap_mode imode, soap_mode omode);
        /// Destructor deletes non-shared managing context only (use destroy() to delete deserialized data)
        virtual ~ProvisioningBindingService();
        /// Delete all deserialized data (with soap_destroy() and soap_end())
        virtual void destroy();
        /// Delete all deserialized data and reset to defaults
        virtual void reset();
        /// Initializer used by constructors
        virtual void ProvisioningBindingService_init(soap_mode imode, soap_mode omode);
        /// Return a copy that has a new managing context with the same engine state
        virtual ProvisioningBindingService *copy() SOAP_PURE_VIRTUAL_COPY;
        /// Copy assignment
        ProvisioningBindingService& operator=(const ProvisioningBindingService&);
        /// Close connection (normally automatic)
        virtual int soap_close_socket();
        /// Force close connection (can kill a thread blocked on IO)
        virtual int soap_force_close_socket();
        /// Return sender-related fault to sender
        virtual int soap_senderfault(const char *string, const char *detailXML);
        /// Return sender-related fault with SOAP 1.2 subcode to sender
        virtual int soap_senderfault(const char *subcodeQName, const char *string, const char *detailXML);
        /// Return receiver-related fault to sender
        virtual int soap_receiverfault(const char *string, const char *detailXML);
        /// Return receiver-related fault with SOAP 1.2 subcode to sender
        virtual int soap_receiverfault(const char *subcodeQName, const char *string, const char *detailXML);
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
        /// Disables and removes SOAP Header from message by setting soap->header = NULL
        virtual void soap_noheader();
        /// Add SOAP Header to message
        virtual void soap_header(char *wsa5__MessageID, struct wsa5__RelatesToType *wsa5__RelatesTo, struct wsa5__EndpointReferenceType *wsa5__From, struct wsa5__EndpointReferenceType *wsa5__ReplyTo, struct wsa5__EndpointReferenceType *wsa5__FaultTo, char *wsa5__To, char *wsa5__Action, struct chan__ChannelInstanceType *chan__ChannelInstance, struct wsdd__AppSequenceType *wsdd__AppSequence, struct _wsse__Security *wsse__Security, char *subscriptionID);
        /// Get SOAP Header structure (i.e. soap->header, which is NULL when absent)
        virtual ::SOAP_ENV__Header *soap_header();
    #ifndef WITH_NOIO
        /// Run simple single-thread (iterative, non-SSL) service on port until a connection error occurs (returns SOAP_OK or error code), use this->bind_flag = SO_REUSEADDR to rebind for immediate rerun
        virtual int run(int port);
    #if defined(WITH_OPENSSL) || defined(WITH_GNUTLS)
        /// Run simple single-thread SSL service on port until a connection error occurs (returns SOAP_OK or error code), use this->bind_flag = SO_REUSEADDR to rebind for immediate rerun
        virtual int ssl_run(int port);
    #endif
        /// Bind service to port (returns master socket or SOAP_INVALID_SOCKET)
        virtual SOAP_SOCKET bind(const char *host, int port, int backlog);
        /// Accept next request (returns socket or SOAP_INVALID_SOCKET)
        virtual SOAP_SOCKET accept();
    #if defined(WITH_OPENSSL) || defined(WITH_GNUTLS)
        /// When SSL is used, after accept() should perform and accept SSL handshake
        virtual int ssl_accept();
    #endif
    #endif
        /// After accept() serve this request (returns SOAP_OK or error code)
        virtual int serve();
        /// Used by serve() to dispatch a request (returns SOAP_OK or error code)
        virtual int dispatch();
        virtual int dispatch(struct soap *soap);
        ///
        /// Service operations are listed below (you should define these)
        /// Note: compile with -DWITH_PURE_VIRTUAL for pure virtual methods
        ///
        /// Web service operation 'GetServiceCapabilities' (returns SOAP_OK or error code)
        virtual int GetServiceCapabilities(_onvifProvisioning__GetServiceCapabilities *onvifProvisioning__GetServiceCapabilities, _onvifProvisioning__GetServiceCapabilitiesResponse &onvifProvisioning__GetServiceCapabilitiesResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'PanMove' (returns SOAP_OK or error code)
        virtual int PanMove(_onvifProvisioning__PanMove *onvifProvisioning__PanMove, _onvifProvisioning__PanMoveResponse &onvifProvisioning__PanMoveResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'TiltMove' (returns SOAP_OK or error code)
        virtual int TiltMove(_onvifProvisioning__TiltMove *onvifProvisioning__TiltMove, _onvifProvisioning__TiltMoveResponse &onvifProvisioning__TiltMoveResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'ZoomMove' (returns SOAP_OK or error code)
        virtual int ZoomMove(_onvifProvisioning__ZoomMove *onvifProvisioning__ZoomMove, _onvifProvisioning__ZoomMoveResponse &onvifProvisioning__ZoomMoveResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'RollMove' (returns SOAP_OK or error code)
        virtual int RollMove(_onvifProvisioning__RollMove *onvifProvisioning__RollMove, _onvifProvisioning__RollMoveResponse &onvifProvisioning__RollMoveResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'FocusMove' (returns SOAP_OK or error code)
        virtual int FocusMove(_onvifProvisioning__FocusMove *onvifProvisioning__FocusMove, _onvifProvisioning__FocusMoveResponse &onvifProvisioning__FocusMoveResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'Stop' (returns SOAP_OK or error code)
        virtual int Stop(_onvifProvisioning__Stop *onvifProvisioning__Stop, _onvifProvisioning__StopResponse &onvifProvisioning__StopResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetUsage' (returns SOAP_OK or error code)
        virtual int GetUsage(_onvifProvisioning__GetUsage *onvifProvisioning__GetUsage, _onvifProvisioning__GetUsageResponse &onvifProvisioning__GetUsageResponse) SOAP_PURE_VIRTUAL;
    };
#endif
