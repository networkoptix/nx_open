/* soapDeviceIOBindingService.h
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

#ifndef soapDeviceIOBindingService_H
#define soapDeviceIOBindingService_H
#include "soapH.h"

    class SOAP_CMAC DeviceIOBindingService {
      public:
        /// Context to manage service IO and data
        struct soap *soap;
        bool soap_own;  ///< flag indicating that this context is owned by this service when context is shared
        /// Variables globally declared in ../result/interim/onvif.h_, if any
        /// Construct a service with new managing context
        DeviceIOBindingService();
        /// Copy constructor
        DeviceIOBindingService(const DeviceIOBindingService&);
        /// Construct service given a shared managing context
        DeviceIOBindingService(struct soap*);
        /// Constructor taking input+output mode flags for the new managing context
        DeviceIOBindingService(soap_mode iomode);
        /// Constructor taking input and output mode flags for the new managing context
        DeviceIOBindingService(soap_mode imode, soap_mode omode);
        /// Destructor deletes non-shared managing context only (use destroy() to delete deserialized data)
        virtual ~DeviceIOBindingService();
        /// Delete all deserialized data (with soap_destroy() and soap_end())
        virtual void destroy();
        /// Delete all deserialized data and reset to defaults
        virtual void reset();
        /// Initializer used by constructors
        virtual void DeviceIOBindingService_init(soap_mode imode, soap_mode omode);
        /// Return a copy that has a new managing context with the same engine state
        virtual DeviceIOBindingService *copy() SOAP_PURE_VIRTUAL_COPY;
        /// Copy assignment
        DeviceIOBindingService& operator=(const DeviceIOBindingService&);
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
        virtual int GetServiceCapabilities(_onvifDeviceIO__GetServiceCapabilities *onvifDeviceIO__GetServiceCapabilities, _onvifDeviceIO__GetServiceCapabilitiesResponse &onvifDeviceIO__GetServiceCapabilitiesResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetRelayOutputOptions' (returns SOAP_OK or error code)
        virtual int GetRelayOutputOptions(_onvifDeviceIO__GetRelayOutputOptions *onvifDeviceIO__GetRelayOutputOptions, _onvifDeviceIO__GetRelayOutputOptionsResponse &onvifDeviceIO__GetRelayOutputOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioSources' (returns SOAP_OK or error code)
        virtual int GetAudioSources(onvifDeviceIO__Get *onvifDeviceIO__GetAudioSources, onvifDeviceIO__GetResponse &onvifDeviceIO__GetAudioSourcesResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioOutputs' (returns SOAP_OK or error code)
        virtual int GetAudioOutputs(onvifDeviceIO__Get *onvifDeviceIO__GetAudioOutputs, onvifDeviceIO__GetResponse &onvifDeviceIO__GetAudioOutputsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoSources' (returns SOAP_OK or error code)
        virtual int GetVideoSources(onvifDeviceIO__Get *onvifDeviceIO__GetVideoSources, onvifDeviceIO__GetResponse &onvifDeviceIO__GetVideoSourcesResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoOutputs' (returns SOAP_OK or error code)
        virtual int GetVideoOutputs(_onvifDeviceIO__GetVideoOutputs *onvifDeviceIO__GetVideoOutputs, _onvifDeviceIO__GetVideoOutputsResponse &onvifDeviceIO__GetVideoOutputsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoSourceConfiguration' (returns SOAP_OK or error code)
        virtual int GetVideoSourceConfiguration(_onvifDeviceIO__GetVideoSourceConfiguration *onvifDeviceIO__GetVideoSourceConfiguration, _onvifDeviceIO__GetVideoSourceConfigurationResponse &onvifDeviceIO__GetVideoSourceConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoOutputConfiguration' (returns SOAP_OK or error code)
        virtual int GetVideoOutputConfiguration(_onvifDeviceIO__GetVideoOutputConfiguration *onvifDeviceIO__GetVideoOutputConfiguration, _onvifDeviceIO__GetVideoOutputConfigurationResponse &onvifDeviceIO__GetVideoOutputConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioSourceConfiguration' (returns SOAP_OK or error code)
        virtual int GetAudioSourceConfiguration(_onvifDeviceIO__GetAudioSourceConfiguration *onvifDeviceIO__GetAudioSourceConfiguration, _onvifDeviceIO__GetAudioSourceConfigurationResponse &onvifDeviceIO__GetAudioSourceConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioOutputConfiguration' (returns SOAP_OK or error code)
        virtual int GetAudioOutputConfiguration(_onvifDeviceIO__GetAudioOutputConfiguration *onvifDeviceIO__GetAudioOutputConfiguration, _onvifDeviceIO__GetAudioOutputConfigurationResponse &onvifDeviceIO__GetAudioOutputConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetVideoSourceConfiguration' (returns SOAP_OK or error code)
        virtual int SetVideoSourceConfiguration(_onvifDeviceIO__SetVideoSourceConfiguration *onvifDeviceIO__SetVideoSourceConfiguration, _onvifDeviceIO__SetVideoSourceConfigurationResponse &onvifDeviceIO__SetVideoSourceConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetVideoOutputConfiguration' (returns SOAP_OK or error code)
        virtual int SetVideoOutputConfiguration(_onvifDeviceIO__SetVideoOutputConfiguration *onvifDeviceIO__SetVideoOutputConfiguration, _onvifDeviceIO__SetVideoOutputConfigurationResponse &onvifDeviceIO__SetVideoOutputConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetAudioSourceConfiguration' (returns SOAP_OK or error code)
        virtual int SetAudioSourceConfiguration(_onvifDeviceIO__SetAudioSourceConfiguration *onvifDeviceIO__SetAudioSourceConfiguration, _onvifDeviceIO__SetAudioSourceConfigurationResponse &onvifDeviceIO__SetAudioSourceConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetAudioOutputConfiguration' (returns SOAP_OK or error code)
        virtual int SetAudioOutputConfiguration(_onvifDeviceIO__SetAudioOutputConfiguration *onvifDeviceIO__SetAudioOutputConfiguration, _onvifDeviceIO__SetAudioOutputConfigurationResponse &onvifDeviceIO__SetAudioOutputConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoSourceConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetVideoSourceConfigurationOptions(_onvifDeviceIO__GetVideoSourceConfigurationOptions *onvifDeviceIO__GetVideoSourceConfigurationOptions, _onvifDeviceIO__GetVideoSourceConfigurationOptionsResponse &onvifDeviceIO__GetVideoSourceConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoOutputConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetVideoOutputConfigurationOptions(_onvifDeviceIO__GetVideoOutputConfigurationOptions *onvifDeviceIO__GetVideoOutputConfigurationOptions, _onvifDeviceIO__GetVideoOutputConfigurationOptionsResponse &onvifDeviceIO__GetVideoOutputConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioSourceConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetAudioSourceConfigurationOptions(_onvifDeviceIO__GetAudioSourceConfigurationOptions *onvifDeviceIO__GetAudioSourceConfigurationOptions, _onvifDeviceIO__GetAudioSourceConfigurationOptionsResponse &onvifDeviceIO__GetAudioSourceConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioOutputConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetAudioOutputConfigurationOptions(_onvifDeviceIO__GetAudioOutputConfigurationOptions *onvifDeviceIO__GetAudioOutputConfigurationOptions, _onvifDeviceIO__GetAudioOutputConfigurationOptionsResponse &onvifDeviceIO__GetAudioOutputConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetRelayOutputs' (returns SOAP_OK or error code)
        virtual int GetRelayOutputs(_onvifDevice__GetRelayOutputs *onvifDevice__GetRelayOutputs, _onvifDevice__GetRelayOutputsResponse &onvifDevice__GetRelayOutputsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetRelayOutputSettings' (returns SOAP_OK or error code)
        virtual int SetRelayOutputSettings(_onvifDeviceIO__SetRelayOutputSettings *onvifDeviceIO__SetRelayOutputSettings, _onvifDeviceIO__SetRelayOutputSettingsResponse &onvifDeviceIO__SetRelayOutputSettingsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetRelayOutputState' (returns SOAP_OK or error code)
        virtual int SetRelayOutputState(_onvifDevice__SetRelayOutputState *onvifDevice__SetRelayOutputState, _onvifDevice__SetRelayOutputStateResponse &onvifDevice__SetRelayOutputStateResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetDigitalInputs' (returns SOAP_OK or error code)
        virtual int GetDigitalInputs(_onvifDeviceIO__GetDigitalInputs *onvifDeviceIO__GetDigitalInputs, _onvifDeviceIO__GetDigitalInputsResponse &onvifDeviceIO__GetDigitalInputsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetDigitalInputConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetDigitalInputConfigurationOptions(_onvifDeviceIO__GetDigitalInputConfigurationOptions *onvifDeviceIO__GetDigitalInputConfigurationOptions, _onvifDeviceIO__GetDigitalInputConfigurationOptionsResponse &onvifDeviceIO__GetDigitalInputConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetDigitalInputConfigurations' (returns SOAP_OK or error code)
        virtual int SetDigitalInputConfigurations(_onvifDeviceIO__SetDigitalInputConfigurations *onvifDeviceIO__SetDigitalInputConfigurations, _onvifDeviceIO__SetDigitalInputConfigurationsResponse &onvifDeviceIO__SetDigitalInputConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetSerialPorts' (returns SOAP_OK or error code)
        virtual int GetSerialPorts(_onvifDeviceIO__GetSerialPorts *onvifDeviceIO__GetSerialPorts, _onvifDeviceIO__GetSerialPortsResponse &onvifDeviceIO__GetSerialPortsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetSerialPortConfiguration' (returns SOAP_OK or error code)
        virtual int GetSerialPortConfiguration(_onvifDeviceIO__GetSerialPortConfiguration *onvifDeviceIO__GetSerialPortConfiguration, _onvifDeviceIO__GetSerialPortConfigurationResponse &onvifDeviceIO__GetSerialPortConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetSerialPortConfiguration' (returns SOAP_OK or error code)
        virtual int SetSerialPortConfiguration(_onvifDeviceIO__SetSerialPortConfiguration *onvifDeviceIO__SetSerialPortConfiguration, _onvifDeviceIO__SetSerialPortConfigurationResponse &onvifDeviceIO__SetSerialPortConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetSerialPortConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetSerialPortConfigurationOptions(_onvifDeviceIO__GetSerialPortConfigurationOptions *onvifDeviceIO__GetSerialPortConfigurationOptions, _onvifDeviceIO__GetSerialPortConfigurationOptionsResponse &onvifDeviceIO__GetSerialPortConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SendReceiveSerialCommand' (returns SOAP_OK or error code)
        virtual int SendReceiveSerialCommand(_onvifDeviceIO__SendReceiveSerialCommand *onvifDeviceIO__SendReceiveSerialCommand, _onvifDeviceIO__SendReceiveSerialCommandResponse &onvifDeviceIO__SendReceiveSerialCommandResponse) SOAP_PURE_VIRTUAL;
    };
#endif
