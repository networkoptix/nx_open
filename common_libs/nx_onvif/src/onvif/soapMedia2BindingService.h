/* soapMedia2BindingService.h
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

#ifndef soapMedia2BindingService_H
#define soapMedia2BindingService_H
#include "soapH.h"

    class SOAP_CMAC Media2BindingService {
      public:
        /// Context to manage service IO and data
        struct soap *soap;
        bool soap_own;  ///< flag indicating that this context is owned by this service when context is shared
        /// Variables globally declared in ../result/interim/onvif.h_, if any
        /// Construct a service with new managing context
        Media2BindingService();
        /// Copy constructor
        Media2BindingService(const Media2BindingService&);
        /// Construct service given a shared managing context
        Media2BindingService(struct soap*);
        /// Constructor taking input+output mode flags for the new managing context
        Media2BindingService(soap_mode iomode);
        /// Constructor taking input and output mode flags for the new managing context
        Media2BindingService(soap_mode imode, soap_mode omode);
        /// Destructor deletes non-shared managing context only (use destroy() to delete deserialized data)
        virtual ~Media2BindingService();
        /// Delete all deserialized data (with soap_destroy() and soap_end())
        virtual void destroy();
        /// Delete all deserialized data and reset to defaults
        virtual void reset();
        /// Initializer used by constructors
        virtual void Media2BindingService_init(soap_mode imode, soap_mode omode);
        /// Return a copy that has a new managing context with the same engine state
        virtual Media2BindingService *copy() SOAP_PURE_VIRTUAL_COPY;
        /// Copy assignment
        Media2BindingService& operator=(const Media2BindingService&);
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
        virtual int GetServiceCapabilities(_onvifMedia2__GetServiceCapabilities *onvifMedia2__GetServiceCapabilities, _onvifMedia2__GetServiceCapabilitiesResponse &onvifMedia2__GetServiceCapabilitiesResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'CreateProfile' (returns SOAP_OK or error code)
        virtual int CreateProfile(_onvifMedia2__CreateProfile *onvifMedia2__CreateProfile, _onvifMedia2__CreateProfileResponse &onvifMedia2__CreateProfileResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetProfiles' (returns SOAP_OK or error code)
        virtual int GetProfiles(_onvifMedia2__GetProfiles *onvifMedia2__GetProfiles, _onvifMedia2__GetProfilesResponse &onvifMedia2__GetProfilesResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'AddConfiguration' (returns SOAP_OK or error code)
        virtual int AddConfiguration(_onvifMedia2__AddConfiguration *onvifMedia2__AddConfiguration, _onvifMedia2__AddConfigurationResponse &onvifMedia2__AddConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'RemoveConfiguration' (returns SOAP_OK or error code)
        virtual int RemoveConfiguration(_onvifMedia2__RemoveConfiguration *onvifMedia2__RemoveConfiguration, _onvifMedia2__RemoveConfigurationResponse &onvifMedia2__RemoveConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'DeleteProfile' (returns SOAP_OK or error code)
        virtual int DeleteProfile(_onvifMedia2__DeleteProfile *onvifMedia2__DeleteProfile, _onvifMedia2__DeleteProfileResponse &onvifMedia2__DeleteProfileResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoSourceConfigurations' (returns SOAP_OK or error code)
        virtual int GetVideoSourceConfigurations(onvifMedia2__GetConfiguration *onvifMedia2__GetVideoSourceConfigurations, _onvifMedia2__GetVideoSourceConfigurationsResponse &onvifMedia2__GetVideoSourceConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoEncoderConfigurations' (returns SOAP_OK or error code)
        virtual int GetVideoEncoderConfigurations(onvifMedia2__GetConfiguration *onvifMedia2__GetVideoEncoderConfigurations, _onvifMedia2__GetVideoEncoderConfigurationsResponse &onvifMedia2__GetVideoEncoderConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioSourceConfigurations' (returns SOAP_OK or error code)
        virtual int GetAudioSourceConfigurations(onvifMedia2__GetConfiguration *onvifMedia2__GetAudioSourceConfigurations, _onvifMedia2__GetAudioSourceConfigurationsResponse &onvifMedia2__GetAudioSourceConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioEncoderConfigurations' (returns SOAP_OK or error code)
        virtual int GetAudioEncoderConfigurations(onvifMedia2__GetConfiguration *onvifMedia2__GetAudioEncoderConfigurations, _onvifMedia2__GetAudioEncoderConfigurationsResponse &onvifMedia2__GetAudioEncoderConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAnalyticsConfigurations' (returns SOAP_OK or error code)
        virtual int GetAnalyticsConfigurations(onvifMedia2__GetConfiguration *onvifMedia2__GetAnalyticsConfigurations, _onvifMedia2__GetAnalyticsConfigurationsResponse &onvifMedia2__GetAnalyticsConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetMetadataConfigurations' (returns SOAP_OK or error code)
        virtual int GetMetadataConfigurations(onvifMedia2__GetConfiguration *onvifMedia2__GetMetadataConfigurations, _onvifMedia2__GetMetadataConfigurationsResponse &onvifMedia2__GetMetadataConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioOutputConfigurations' (returns SOAP_OK or error code)
        virtual int GetAudioOutputConfigurations(onvifMedia2__GetConfiguration *onvifMedia2__GetAudioOutputConfigurations, _onvifMedia2__GetAudioOutputConfigurationsResponse &onvifMedia2__GetAudioOutputConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioDecoderConfigurations' (returns SOAP_OK or error code)
        virtual int GetAudioDecoderConfigurations(onvifMedia2__GetConfiguration *onvifMedia2__GetAudioDecoderConfigurations, _onvifMedia2__GetAudioDecoderConfigurationsResponse &onvifMedia2__GetAudioDecoderConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetVideoSourceConfiguration' (returns SOAP_OK or error code)
        virtual int SetVideoSourceConfiguration(_onvifMedia2__SetVideoSourceConfiguration *onvifMedia2__SetVideoSourceConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetVideoSourceConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetVideoEncoderConfiguration' (returns SOAP_OK or error code)
        virtual int SetVideoEncoderConfiguration(_onvifMedia2__SetVideoEncoderConfiguration *onvifMedia2__SetVideoEncoderConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetVideoEncoderConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetAudioSourceConfiguration' (returns SOAP_OK or error code)
        virtual int SetAudioSourceConfiguration(_onvifMedia2__SetAudioSourceConfiguration *onvifMedia2__SetAudioSourceConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetAudioSourceConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetAudioEncoderConfiguration' (returns SOAP_OK or error code)
        virtual int SetAudioEncoderConfiguration(_onvifMedia2__SetAudioEncoderConfiguration *onvifMedia2__SetAudioEncoderConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetAudioEncoderConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetMetadataConfiguration' (returns SOAP_OK or error code)
        virtual int SetMetadataConfiguration(_onvifMedia2__SetMetadataConfiguration *onvifMedia2__SetMetadataConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetMetadataConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetAudioOutputConfiguration' (returns SOAP_OK or error code)
        virtual int SetAudioOutputConfiguration(_onvifMedia2__SetAudioOutputConfiguration *onvifMedia2__SetAudioOutputConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetAudioOutputConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetAudioDecoderConfiguration' (returns SOAP_OK or error code)
        virtual int SetAudioDecoderConfiguration(_onvifMedia2__SetAudioDecoderConfiguration *onvifMedia2__SetAudioDecoderConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetAudioDecoderConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoSourceConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetVideoSourceConfigurationOptions(onvifMedia2__GetConfiguration *onvifMedia2__GetVideoSourceConfigurationOptions, _onvifMedia2__GetVideoSourceConfigurationOptionsResponse &onvifMedia2__GetVideoSourceConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoEncoderConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetVideoEncoderConfigurationOptions(onvifMedia2__GetConfiguration *onvifMedia2__GetVideoEncoderConfigurationOptions, _onvifMedia2__GetVideoEncoderConfigurationOptionsResponse &onvifMedia2__GetVideoEncoderConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioSourceConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetAudioSourceConfigurationOptions(onvifMedia2__GetConfiguration *onvifMedia2__GetAudioSourceConfigurationOptions, _onvifMedia2__GetAudioSourceConfigurationOptionsResponse &onvifMedia2__GetAudioSourceConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioEncoderConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetAudioEncoderConfigurationOptions(onvifMedia2__GetConfiguration *onvifMedia2__GetAudioEncoderConfigurationOptions, _onvifMedia2__GetAudioEncoderConfigurationOptionsResponse &onvifMedia2__GetAudioEncoderConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetMetadataConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetMetadataConfigurationOptions(onvifMedia2__GetConfiguration *onvifMedia2__GetMetadataConfigurationOptions, _onvifMedia2__GetMetadataConfigurationOptionsResponse &onvifMedia2__GetMetadataConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioOutputConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetAudioOutputConfigurationOptions(onvifMedia2__GetConfiguration *onvifMedia2__GetAudioOutputConfigurationOptions, _onvifMedia2__GetAudioOutputConfigurationOptionsResponse &onvifMedia2__GetAudioOutputConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioDecoderConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetAudioDecoderConfigurationOptions(onvifMedia2__GetConfiguration *onvifMedia2__GetAudioDecoderConfigurationOptions, _onvifMedia2__GetAudioDecoderConfigurationOptionsResponse &onvifMedia2__GetAudioDecoderConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoEncoderInstances' (returns SOAP_OK or error code)
        virtual int GetVideoEncoderInstances(_onvifMedia2__GetVideoEncoderInstances *onvifMedia2__GetVideoEncoderInstances, _onvifMedia2__GetVideoEncoderInstancesResponse &onvifMedia2__GetVideoEncoderInstancesResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetStreamUri' (returns SOAP_OK or error code)
        virtual int GetStreamUri(_onvifMedia2__GetStreamUri *onvifMedia2__GetStreamUri, _onvifMedia2__GetStreamUriResponse &onvifMedia2__GetStreamUriResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'StartMulticastStreaming' (returns SOAP_OK or error code)
        virtual int StartMulticastStreaming(onvifMedia2__StartStopMulticastStreaming *onvifMedia2__StartMulticastStreaming, onvifMedia2__SetConfigurationResponse &onvifMedia2__StartMulticastStreamingResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'StopMulticastStreaming' (returns SOAP_OK or error code)
        virtual int StopMulticastStreaming(onvifMedia2__StartStopMulticastStreaming *onvifMedia2__StopMulticastStreaming, onvifMedia2__SetConfigurationResponse &onvifMedia2__StopMulticastStreamingResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetSynchronizationPoint' (returns SOAP_OK or error code)
        virtual int SetSynchronizationPoint(_onvifMedia2__SetSynchronizationPoint *onvifMedia2__SetSynchronizationPoint, _onvifMedia2__SetSynchronizationPointResponse &onvifMedia2__SetSynchronizationPointResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetSnapshotUri' (returns SOAP_OK or error code)
        virtual int GetSnapshotUri(_onvifMedia2__GetSnapshotUri *onvifMedia2__GetSnapshotUri, _onvifMedia2__GetSnapshotUriResponse &onvifMedia2__GetSnapshotUriResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoSourceModes' (returns SOAP_OK or error code)
        virtual int GetVideoSourceModes(_onvifMedia2__GetVideoSourceModes *onvifMedia2__GetVideoSourceModes, _onvifMedia2__GetVideoSourceModesResponse &onvifMedia2__GetVideoSourceModesResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetVideoSourceMode' (returns SOAP_OK or error code)
        virtual int SetVideoSourceMode(_onvifMedia2__SetVideoSourceMode *onvifMedia2__SetVideoSourceMode, _onvifMedia2__SetVideoSourceModeResponse &onvifMedia2__SetVideoSourceModeResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetOSDs' (returns SOAP_OK or error code)
        virtual int GetOSDs(_onvifMedia2__GetOSDs *onvifMedia2__GetOSDs, _onvifMedia2__GetOSDsResponse &onvifMedia2__GetOSDsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetOSDOptions' (returns SOAP_OK or error code)
        virtual int GetOSDOptions(_onvifMedia2__GetOSDOptions *onvifMedia2__GetOSDOptions, _onvifMedia2__GetOSDOptionsResponse &onvifMedia2__GetOSDOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetOSD' (returns SOAP_OK or error code)
        virtual int SetOSD(_onvifMedia2__SetOSD *onvifMedia2__SetOSD, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetOSDResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'CreateOSD' (returns SOAP_OK or error code)
        virtual int CreateOSD(_onvifMedia2__CreateOSD *onvifMedia2__CreateOSD, _onvifMedia2__CreateOSDResponse &onvifMedia2__CreateOSDResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'DeleteOSD' (returns SOAP_OK or error code)
        virtual int DeleteOSD(_onvifMedia2__DeleteOSD *onvifMedia2__DeleteOSD, onvifMedia2__SetConfigurationResponse &onvifMedia2__DeleteOSDResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetMasks' (returns SOAP_OK or error code)
        virtual int GetMasks(_onvifMedia2__GetMasks *onvifMedia2__GetMasks, _onvifMedia2__GetMasksResponse &onvifMedia2__GetMasksResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetMaskOptions' (returns SOAP_OK or error code)
        virtual int GetMaskOptions(_onvifMedia2__GetMaskOptions *onvifMedia2__GetMaskOptions, _onvifMedia2__GetMaskOptionsResponse &onvifMedia2__GetMaskOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetMask' (returns SOAP_OK or error code)
        virtual int SetMask(_onvifMedia2__SetMask *onvifMedia2__SetMask, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetMaskResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'CreateMask' (returns SOAP_OK or error code)
        virtual int CreateMask(_onvifMedia2__CreateMask *onvifMedia2__CreateMask, _onvifMedia2__CreateMaskResponse &onvifMedia2__CreateMaskResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'DeleteMask' (returns SOAP_OK or error code)
        virtual int DeleteMask(_onvifMedia2__DeleteMask *onvifMedia2__DeleteMask, onvifMedia2__SetConfigurationResponse &onvifMedia2__DeleteMaskResponse) SOAP_PURE_VIRTUAL;
    };
#endif
