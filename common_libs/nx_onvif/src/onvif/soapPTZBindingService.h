/* soapPTZBindingService.h
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

#ifndef soapPTZBindingService_H
#define soapPTZBindingService_H
#include "soapH.h"

    class SOAP_CMAC PTZBindingService {
      public:
        /// Context to manage service IO and data
        struct soap *soap;
        bool soap_own;  ///< flag indicating that this context is owned by this service when context is shared
        /// Variables globally declared in ../result/interim/onvif.h_, if any
        /// Construct a service with new managing context
        PTZBindingService();
        /// Copy constructor
        PTZBindingService(const PTZBindingService&);
        /// Construct service given a shared managing context
        PTZBindingService(struct soap*);
        /// Constructor taking input+output mode flags for the new managing context
        PTZBindingService(soap_mode iomode);
        /// Constructor taking input and output mode flags for the new managing context
        PTZBindingService(soap_mode imode, soap_mode omode);
        /// Destructor deletes non-shared managing context only (use destroy() to delete deserialized data)
        virtual ~PTZBindingService();
        /// Delete all deserialized data (with soap_destroy() and soap_end())
        virtual void destroy();
        /// Delete all deserialized data and reset to defaults
        virtual void reset();
        /// Initializer used by constructors
        virtual void PTZBindingService_init(soap_mode imode, soap_mode omode);
        /// Return a copy that has a new managing context with the same engine state
        virtual PTZBindingService *copy() SOAP_PURE_VIRTUAL_COPY;
        /// Copy assignment
        PTZBindingService& operator=(const PTZBindingService&);
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
        virtual void soap_header(char *wsa5__MessageID, struct wsa5__RelatesToType *wsa5__RelatesTo, struct wsa5__EndpointReferenceType *wsa5__From, struct wsa5__EndpointReferenceType *wsa5__ReplyTo, struct wsa5__EndpointReferenceType *wsa5__FaultTo, char *wsa5__To, char *wsa5__Action, struct chan__ChannelInstanceType *chan__ChannelInstance, struct wsdd__AppSequenceType *wsdd__AppSequence, struct _wsse__Security *wsse__Security, char *SubscriptionId);
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
        virtual int GetServiceCapabilities(_onvifPtz__GetServiceCapabilities *onvifPtz__GetServiceCapabilities, _onvifPtz__GetServiceCapabilitiesResponse &onvifPtz__GetServiceCapabilitiesResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetConfigurations' (returns SOAP_OK or error code)
        virtual int GetConfigurations(_onvifPtz__GetConfigurations *onvifPtz__GetConfigurations, _onvifPtz__GetConfigurationsResponse &onvifPtz__GetConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetPresets' (returns SOAP_OK or error code)
        virtual int GetPresets(_onvifPtz__GetPresets *onvifPtz__GetPresets, _onvifPtz__GetPresetsResponse &onvifPtz__GetPresetsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetPreset' (returns SOAP_OK or error code)
        virtual int SetPreset(_onvifPtz__SetPreset *onvifPtz__SetPreset, _onvifPtz__SetPresetResponse &onvifPtz__SetPresetResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'RemovePreset' (returns SOAP_OK or error code)
        virtual int RemovePreset(_onvifPtz__RemovePreset *onvifPtz__RemovePreset, _onvifPtz__RemovePresetResponse &onvifPtz__RemovePresetResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GotoPreset' (returns SOAP_OK or error code)
        virtual int GotoPreset(_onvifPtz__GotoPreset *onvifPtz__GotoPreset, _onvifPtz__GotoPresetResponse &onvifPtz__GotoPresetResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetStatus' (returns SOAP_OK or error code)
        virtual int GetStatus(_onvifPtz__GetStatus *onvifPtz__GetStatus, _onvifPtz__GetStatusResponse &onvifPtz__GetStatusResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetConfiguration' (returns SOAP_OK or error code)
        virtual int GetConfiguration(_onvifPtz__GetConfiguration *onvifPtz__GetConfiguration, _onvifPtz__GetConfigurationResponse &onvifPtz__GetConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetNodes' (returns SOAP_OK or error code)
        virtual int GetNodes(_onvifPtz__GetNodes *onvifPtz__GetNodes, _onvifPtz__GetNodesResponse &onvifPtz__GetNodesResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetNode' (returns SOAP_OK or error code)
        virtual int GetNode(_onvifPtz__GetNode *onvifPtz__GetNode, _onvifPtz__GetNodeResponse &onvifPtz__GetNodeResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetConfiguration' (returns SOAP_OK or error code)
        virtual int SetConfiguration(_onvifPtz__SetConfiguration *onvifPtz__SetConfiguration, _onvifPtz__SetConfigurationResponse &onvifPtz__SetConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetConfigurationOptions(_onvifPtz__GetConfigurationOptions *onvifPtz__GetConfigurationOptions, _onvifPtz__GetConfigurationOptionsResponse &onvifPtz__GetConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GotoHomePosition' (returns SOAP_OK or error code)
        virtual int GotoHomePosition(_onvifPtz__GotoHomePosition *onvifPtz__GotoHomePosition, _onvifPtz__GotoHomePositionResponse &onvifPtz__GotoHomePositionResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetHomePosition' (returns SOAP_OK or error code)
        virtual int SetHomePosition(_onvifPtz__SetHomePosition *onvifPtz__SetHomePosition, _onvifPtz__SetHomePositionResponse &onvifPtz__SetHomePositionResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'ContinuousMove' (returns SOAP_OK or error code)
        virtual int ContinuousMove(_onvifPtz__ContinuousMove *onvifPtz__ContinuousMove, _onvifPtz__ContinuousMoveResponse &onvifPtz__ContinuousMoveResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'RelativeMove' (returns SOAP_OK or error code)
        virtual int RelativeMove(_onvifPtz__RelativeMove *onvifPtz__RelativeMove, _onvifPtz__RelativeMoveResponse &onvifPtz__RelativeMoveResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SendAuxiliaryCommand' (returns SOAP_OK or error code)
        virtual int SendAuxiliaryCommand(_onvifPtz__SendAuxiliaryCommand *onvifPtz__SendAuxiliaryCommand, _onvifPtz__SendAuxiliaryCommandResponse &onvifPtz__SendAuxiliaryCommandResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'AbsoluteMove' (returns SOAP_OK or error code)
        virtual int AbsoluteMove(_onvifPtz__AbsoluteMove *onvifPtz__AbsoluteMove, _onvifPtz__AbsoluteMoveResponse &onvifPtz__AbsoluteMoveResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GeoMove' (returns SOAP_OK or error code)
        virtual int GeoMove(_onvifPtz__GeoMove *onvifPtz__GeoMove, _onvifPtz__GeoMoveResponse &onvifPtz__GeoMoveResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'Stop' (returns SOAP_OK or error code)
        virtual int Stop(_onvifPtz__Stop *onvifPtz__Stop, _onvifPtz__StopResponse &onvifPtz__StopResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetPresetTours' (returns SOAP_OK or error code)
        virtual int GetPresetTours(_onvifPtz__GetPresetTours *onvifPtz__GetPresetTours, _onvifPtz__GetPresetToursResponse &onvifPtz__GetPresetToursResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetPresetTour' (returns SOAP_OK or error code)
        virtual int GetPresetTour(_onvifPtz__GetPresetTour *onvifPtz__GetPresetTour, _onvifPtz__GetPresetTourResponse &onvifPtz__GetPresetTourResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetPresetTourOptions' (returns SOAP_OK or error code)
        virtual int GetPresetTourOptions(_onvifPtz__GetPresetTourOptions *onvifPtz__GetPresetTourOptions, _onvifPtz__GetPresetTourOptionsResponse &onvifPtz__GetPresetTourOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'CreatePresetTour' (returns SOAP_OK or error code)
        virtual int CreatePresetTour(_onvifPtz__CreatePresetTour *onvifPtz__CreatePresetTour, _onvifPtz__CreatePresetTourResponse &onvifPtz__CreatePresetTourResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'ModifyPresetTour' (returns SOAP_OK or error code)
        virtual int ModifyPresetTour(_onvifPtz__ModifyPresetTour *onvifPtz__ModifyPresetTour, _onvifPtz__ModifyPresetTourResponse &onvifPtz__ModifyPresetTourResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'OperatePresetTour' (returns SOAP_OK or error code)
        virtual int OperatePresetTour(_onvifPtz__OperatePresetTour *onvifPtz__OperatePresetTour, _onvifPtz__OperatePresetTourResponse &onvifPtz__OperatePresetTourResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'RemovePresetTour' (returns SOAP_OK or error code)
        virtual int RemovePresetTour(_onvifPtz__RemovePresetTour *onvifPtz__RemovePresetTour, _onvifPtz__RemovePresetTourResponse &onvifPtz__RemovePresetTourResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetCompatibleConfigurations' (returns SOAP_OK or error code)
        virtual int GetCompatibleConfigurations(_onvifPtz__GetCompatibleConfigurations *onvifPtz__GetCompatibleConfigurations, _onvifPtz__GetCompatibleConfigurationsResponse &onvifPtz__GetCompatibleConfigurationsResponse) SOAP_PURE_VIRTUAL;
    };
#endif
