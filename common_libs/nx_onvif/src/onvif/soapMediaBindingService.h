/* soapMediaBindingService.h
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

#ifndef soapMediaBindingService_H
#define soapMediaBindingService_H
#include "soapH.h"

    class SOAP_CMAC MediaBindingService {
      public:
        /// Context to manage service IO and data
        struct soap *soap;
        bool soap_own;  ///< flag indicating that this context is owned by this service when context is shared
        /// Variables globally declared in ../result/interim/onvif.h_, if any
        /// Construct a service with new managing context
        MediaBindingService();
        /// Copy constructor
        MediaBindingService(const MediaBindingService&);
        /// Construct service given a shared managing context
        MediaBindingService(struct soap*);
        /// Constructor taking input+output mode flags for the new managing context
        MediaBindingService(soap_mode iomode);
        /// Constructor taking input and output mode flags for the new managing context
        MediaBindingService(soap_mode imode, soap_mode omode);
        /// Destructor deletes non-shared managing context only (use destroy() to delete deserialized data)
        virtual ~MediaBindingService();
        /// Delete all deserialized data (with soap_destroy() and soap_end())
        virtual void destroy();
        /// Delete all deserialized data and reset to defaults
        virtual void reset();
        /// Initializer used by constructors
        virtual void MediaBindingService_init(soap_mode imode, soap_mode omode);
        /// Return a copy that has a new managing context with the same engine state
        virtual MediaBindingService *copy() SOAP_PURE_VIRTUAL_COPY;
        /// Copy assignment
        MediaBindingService& operator=(const MediaBindingService&);
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
        virtual int GetServiceCapabilities(_onvifMedia__GetServiceCapabilities *onvifMedia__GetServiceCapabilities, _onvifMedia__GetServiceCapabilitiesResponse &onvifMedia__GetServiceCapabilitiesResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoSources' (returns SOAP_OK or error code)
        virtual int GetVideoSources(_onvifMedia__GetVideoSources *onvifMedia__GetVideoSources, _onvifMedia__GetVideoSourcesResponse &onvifMedia__GetVideoSourcesResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioSources' (returns SOAP_OK or error code)
        virtual int GetAudioSources(_onvifMedia__GetAudioSources *onvifMedia__GetAudioSources, _onvifMedia__GetAudioSourcesResponse &onvifMedia__GetAudioSourcesResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioOutputs' (returns SOAP_OK or error code)
        virtual int GetAudioOutputs(_onvifMedia__GetAudioOutputs *onvifMedia__GetAudioOutputs, _onvifMedia__GetAudioOutputsResponse &onvifMedia__GetAudioOutputsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'CreateProfile' (returns SOAP_OK or error code)
        virtual int CreateProfile(_onvifMedia__CreateProfile *onvifMedia__CreateProfile, _onvifMedia__CreateProfileResponse &onvifMedia__CreateProfileResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetProfile' (returns SOAP_OK or error code)
        virtual int GetProfile(_onvifMedia__GetProfile *onvifMedia__GetProfile, _onvifMedia__GetProfileResponse &onvifMedia__GetProfileResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetProfiles' (returns SOAP_OK or error code)
        virtual int GetProfiles(_onvifMedia__GetProfiles *onvifMedia__GetProfiles, _onvifMedia__GetProfilesResponse &onvifMedia__GetProfilesResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'AddVideoEncoderConfiguration' (returns SOAP_OK or error code)
        virtual int AddVideoEncoderConfiguration(_onvifMedia__AddVideoEncoderConfiguration *onvifMedia__AddVideoEncoderConfiguration, _onvifMedia__AddVideoEncoderConfigurationResponse &onvifMedia__AddVideoEncoderConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'AddVideoSourceConfiguration' (returns SOAP_OK or error code)
        virtual int AddVideoSourceConfiguration(_onvifMedia__AddVideoSourceConfiguration *onvifMedia__AddVideoSourceConfiguration, _onvifMedia__AddVideoSourceConfigurationResponse &onvifMedia__AddVideoSourceConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'AddAudioEncoderConfiguration' (returns SOAP_OK or error code)
        virtual int AddAudioEncoderConfiguration(_onvifMedia__AddAudioEncoderConfiguration *onvifMedia__AddAudioEncoderConfiguration, _onvifMedia__AddAudioEncoderConfigurationResponse &onvifMedia__AddAudioEncoderConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'AddAudioSourceConfiguration' (returns SOAP_OK or error code)
        virtual int AddAudioSourceConfiguration(_onvifMedia__AddAudioSourceConfiguration *onvifMedia__AddAudioSourceConfiguration, _onvifMedia__AddAudioSourceConfigurationResponse &onvifMedia__AddAudioSourceConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'AddPTZConfiguration' (returns SOAP_OK or error code)
        virtual int AddPTZConfiguration(_onvifMedia__AddPTZConfiguration *onvifMedia__AddPTZConfiguration, _onvifMedia__AddPTZConfigurationResponse &onvifMedia__AddPTZConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'AddVideoAnalyticsConfiguration' (returns SOAP_OK or error code)
        virtual int AddVideoAnalyticsConfiguration(_onvifMedia__AddVideoAnalyticsConfiguration *onvifMedia__AddVideoAnalyticsConfiguration, _onvifMedia__AddVideoAnalyticsConfigurationResponse &onvifMedia__AddVideoAnalyticsConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'AddMetadataConfiguration' (returns SOAP_OK or error code)
        virtual int AddMetadataConfiguration(_onvifMedia__AddMetadataConfiguration *onvifMedia__AddMetadataConfiguration, _onvifMedia__AddMetadataConfigurationResponse &onvifMedia__AddMetadataConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'AddAudioOutputConfiguration' (returns SOAP_OK or error code)
        virtual int AddAudioOutputConfiguration(_onvifMedia__AddAudioOutputConfiguration *onvifMedia__AddAudioOutputConfiguration, _onvifMedia__AddAudioOutputConfigurationResponse &onvifMedia__AddAudioOutputConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'AddAudioDecoderConfiguration' (returns SOAP_OK or error code)
        virtual int AddAudioDecoderConfiguration(_onvifMedia__AddAudioDecoderConfiguration *onvifMedia__AddAudioDecoderConfiguration, _onvifMedia__AddAudioDecoderConfigurationResponse &onvifMedia__AddAudioDecoderConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'RemoveVideoEncoderConfiguration' (returns SOAP_OK or error code)
        virtual int RemoveVideoEncoderConfiguration(_onvifMedia__RemoveVideoEncoderConfiguration *onvifMedia__RemoveVideoEncoderConfiguration, _onvifMedia__RemoveVideoEncoderConfigurationResponse &onvifMedia__RemoveVideoEncoderConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'RemoveVideoSourceConfiguration' (returns SOAP_OK or error code)
        virtual int RemoveVideoSourceConfiguration(_onvifMedia__RemoveVideoSourceConfiguration *onvifMedia__RemoveVideoSourceConfiguration, _onvifMedia__RemoveVideoSourceConfigurationResponse &onvifMedia__RemoveVideoSourceConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'RemoveAudioEncoderConfiguration' (returns SOAP_OK or error code)
        virtual int RemoveAudioEncoderConfiguration(_onvifMedia__RemoveAudioEncoderConfiguration *onvifMedia__RemoveAudioEncoderConfiguration, _onvifMedia__RemoveAudioEncoderConfigurationResponse &onvifMedia__RemoveAudioEncoderConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'RemoveAudioSourceConfiguration' (returns SOAP_OK or error code)
        virtual int RemoveAudioSourceConfiguration(_onvifMedia__RemoveAudioSourceConfiguration *onvifMedia__RemoveAudioSourceConfiguration, _onvifMedia__RemoveAudioSourceConfigurationResponse &onvifMedia__RemoveAudioSourceConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'RemovePTZConfiguration' (returns SOAP_OK or error code)
        virtual int RemovePTZConfiguration(_onvifMedia__RemovePTZConfiguration *onvifMedia__RemovePTZConfiguration, _onvifMedia__RemovePTZConfigurationResponse &onvifMedia__RemovePTZConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'RemoveVideoAnalyticsConfiguration' (returns SOAP_OK or error code)
        virtual int RemoveVideoAnalyticsConfiguration(_onvifMedia__RemoveVideoAnalyticsConfiguration *onvifMedia__RemoveVideoAnalyticsConfiguration, _onvifMedia__RemoveVideoAnalyticsConfigurationResponse &onvifMedia__RemoveVideoAnalyticsConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'RemoveMetadataConfiguration' (returns SOAP_OK or error code)
        virtual int RemoveMetadataConfiguration(_onvifMedia__RemoveMetadataConfiguration *onvifMedia__RemoveMetadataConfiguration, _onvifMedia__RemoveMetadataConfigurationResponse &onvifMedia__RemoveMetadataConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'RemoveAudioOutputConfiguration' (returns SOAP_OK or error code)
        virtual int RemoveAudioOutputConfiguration(_onvifMedia__RemoveAudioOutputConfiguration *onvifMedia__RemoveAudioOutputConfiguration, _onvifMedia__RemoveAudioOutputConfigurationResponse &onvifMedia__RemoveAudioOutputConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'RemoveAudioDecoderConfiguration' (returns SOAP_OK or error code)
        virtual int RemoveAudioDecoderConfiguration(_onvifMedia__RemoveAudioDecoderConfiguration *onvifMedia__RemoveAudioDecoderConfiguration, _onvifMedia__RemoveAudioDecoderConfigurationResponse &onvifMedia__RemoveAudioDecoderConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'DeleteProfile' (returns SOAP_OK or error code)
        virtual int DeleteProfile(_onvifMedia__DeleteProfile *onvifMedia__DeleteProfile, _onvifMedia__DeleteProfileResponse &onvifMedia__DeleteProfileResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoSourceConfigurations' (returns SOAP_OK or error code)
        virtual int GetVideoSourceConfigurations(_onvifMedia__GetVideoSourceConfigurations *onvifMedia__GetVideoSourceConfigurations, _onvifMedia__GetVideoSourceConfigurationsResponse &onvifMedia__GetVideoSourceConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoEncoderConfigurations' (returns SOAP_OK or error code)
        virtual int GetVideoEncoderConfigurations(_onvifMedia__GetVideoEncoderConfigurations *onvifMedia__GetVideoEncoderConfigurations, _onvifMedia__GetVideoEncoderConfigurationsResponse &onvifMedia__GetVideoEncoderConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioSourceConfigurations' (returns SOAP_OK or error code)
        virtual int GetAudioSourceConfigurations(_onvifMedia__GetAudioSourceConfigurations *onvifMedia__GetAudioSourceConfigurations, _onvifMedia__GetAudioSourceConfigurationsResponse &onvifMedia__GetAudioSourceConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioEncoderConfigurations' (returns SOAP_OK or error code)
        virtual int GetAudioEncoderConfigurations(_onvifMedia__GetAudioEncoderConfigurations *onvifMedia__GetAudioEncoderConfigurations, _onvifMedia__GetAudioEncoderConfigurationsResponse &onvifMedia__GetAudioEncoderConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoAnalyticsConfigurations' (returns SOAP_OK or error code)
        virtual int GetVideoAnalyticsConfigurations(_onvifMedia__GetVideoAnalyticsConfigurations *onvifMedia__GetVideoAnalyticsConfigurations, _onvifMedia__GetVideoAnalyticsConfigurationsResponse &onvifMedia__GetVideoAnalyticsConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetMetadataConfigurations' (returns SOAP_OK or error code)
        virtual int GetMetadataConfigurations(_onvifMedia__GetMetadataConfigurations *onvifMedia__GetMetadataConfigurations, _onvifMedia__GetMetadataConfigurationsResponse &onvifMedia__GetMetadataConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioOutputConfigurations' (returns SOAP_OK or error code)
        virtual int GetAudioOutputConfigurations(_onvifMedia__GetAudioOutputConfigurations *onvifMedia__GetAudioOutputConfigurations, _onvifMedia__GetAudioOutputConfigurationsResponse &onvifMedia__GetAudioOutputConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioDecoderConfigurations' (returns SOAP_OK or error code)
        virtual int GetAudioDecoderConfigurations(_onvifMedia__GetAudioDecoderConfigurations *onvifMedia__GetAudioDecoderConfigurations, _onvifMedia__GetAudioDecoderConfigurationsResponse &onvifMedia__GetAudioDecoderConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoSourceConfiguration' (returns SOAP_OK or error code)
        virtual int GetVideoSourceConfiguration(_onvifMedia__GetVideoSourceConfiguration *onvifMedia__GetVideoSourceConfiguration, _onvifMedia__GetVideoSourceConfigurationResponse &onvifMedia__GetVideoSourceConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoEncoderConfiguration' (returns SOAP_OK or error code)
        virtual int GetVideoEncoderConfiguration(_onvifMedia__GetVideoEncoderConfiguration *onvifMedia__GetVideoEncoderConfiguration, _onvifMedia__GetVideoEncoderConfigurationResponse &onvifMedia__GetVideoEncoderConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioSourceConfiguration' (returns SOAP_OK or error code)
        virtual int GetAudioSourceConfiguration(_onvifMedia__GetAudioSourceConfiguration *onvifMedia__GetAudioSourceConfiguration, _onvifMedia__GetAudioSourceConfigurationResponse &onvifMedia__GetAudioSourceConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioEncoderConfiguration' (returns SOAP_OK or error code)
        virtual int GetAudioEncoderConfiguration(_onvifMedia__GetAudioEncoderConfiguration *onvifMedia__GetAudioEncoderConfiguration, _onvifMedia__GetAudioEncoderConfigurationResponse &onvifMedia__GetAudioEncoderConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoAnalyticsConfiguration' (returns SOAP_OK or error code)
        virtual int GetVideoAnalyticsConfiguration(_onvifMedia__GetVideoAnalyticsConfiguration *onvifMedia__GetVideoAnalyticsConfiguration, _onvifMedia__GetVideoAnalyticsConfigurationResponse &onvifMedia__GetVideoAnalyticsConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetMetadataConfiguration' (returns SOAP_OK or error code)
        virtual int GetMetadataConfiguration(_onvifMedia__GetMetadataConfiguration *onvifMedia__GetMetadataConfiguration, _onvifMedia__GetMetadataConfigurationResponse &onvifMedia__GetMetadataConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioOutputConfiguration' (returns SOAP_OK or error code)
        virtual int GetAudioOutputConfiguration(_onvifMedia__GetAudioOutputConfiguration *onvifMedia__GetAudioOutputConfiguration, _onvifMedia__GetAudioOutputConfigurationResponse &onvifMedia__GetAudioOutputConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioDecoderConfiguration' (returns SOAP_OK or error code)
        virtual int GetAudioDecoderConfiguration(_onvifMedia__GetAudioDecoderConfiguration *onvifMedia__GetAudioDecoderConfiguration, _onvifMedia__GetAudioDecoderConfigurationResponse &onvifMedia__GetAudioDecoderConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetCompatibleVideoEncoderConfigurations' (returns SOAP_OK or error code)
        virtual int GetCompatibleVideoEncoderConfigurations(_onvifMedia__GetCompatibleVideoEncoderConfigurations *onvifMedia__GetCompatibleVideoEncoderConfigurations, _onvifMedia__GetCompatibleVideoEncoderConfigurationsResponse &onvifMedia__GetCompatibleVideoEncoderConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetCompatibleVideoSourceConfigurations' (returns SOAP_OK or error code)
        virtual int GetCompatibleVideoSourceConfigurations(_onvifMedia__GetCompatibleVideoSourceConfigurations *onvifMedia__GetCompatibleVideoSourceConfigurations, _onvifMedia__GetCompatibleVideoSourceConfigurationsResponse &onvifMedia__GetCompatibleVideoSourceConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetCompatibleAudioEncoderConfigurations' (returns SOAP_OK or error code)
        virtual int GetCompatibleAudioEncoderConfigurations(_onvifMedia__GetCompatibleAudioEncoderConfigurations *onvifMedia__GetCompatibleAudioEncoderConfigurations, _onvifMedia__GetCompatibleAudioEncoderConfigurationsResponse &onvifMedia__GetCompatibleAudioEncoderConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetCompatibleAudioSourceConfigurations' (returns SOAP_OK or error code)
        virtual int GetCompatibleAudioSourceConfigurations(_onvifMedia__GetCompatibleAudioSourceConfigurations *onvifMedia__GetCompatibleAudioSourceConfigurations, _onvifMedia__GetCompatibleAudioSourceConfigurationsResponse &onvifMedia__GetCompatibleAudioSourceConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetCompatibleVideoAnalyticsConfigurations' (returns SOAP_OK or error code)
        virtual int GetCompatibleVideoAnalyticsConfigurations(_onvifMedia__GetCompatibleVideoAnalyticsConfigurations *onvifMedia__GetCompatibleVideoAnalyticsConfigurations, _onvifMedia__GetCompatibleVideoAnalyticsConfigurationsResponse &onvifMedia__GetCompatibleVideoAnalyticsConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetCompatibleMetadataConfigurations' (returns SOAP_OK or error code)
        virtual int GetCompatibleMetadataConfigurations(_onvifMedia__GetCompatibleMetadataConfigurations *onvifMedia__GetCompatibleMetadataConfigurations, _onvifMedia__GetCompatibleMetadataConfigurationsResponse &onvifMedia__GetCompatibleMetadataConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetCompatibleAudioOutputConfigurations' (returns SOAP_OK or error code)
        virtual int GetCompatibleAudioOutputConfigurations(_onvifMedia__GetCompatibleAudioOutputConfigurations *onvifMedia__GetCompatibleAudioOutputConfigurations, _onvifMedia__GetCompatibleAudioOutputConfigurationsResponse &onvifMedia__GetCompatibleAudioOutputConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetCompatibleAudioDecoderConfigurations' (returns SOAP_OK or error code)
        virtual int GetCompatibleAudioDecoderConfigurations(_onvifMedia__GetCompatibleAudioDecoderConfigurations *onvifMedia__GetCompatibleAudioDecoderConfigurations, _onvifMedia__GetCompatibleAudioDecoderConfigurationsResponse &onvifMedia__GetCompatibleAudioDecoderConfigurationsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetVideoSourceConfiguration' (returns SOAP_OK or error code)
        virtual int SetVideoSourceConfiguration(_onvifMedia__SetVideoSourceConfiguration *onvifMedia__SetVideoSourceConfiguration, _onvifMedia__SetVideoSourceConfigurationResponse &onvifMedia__SetVideoSourceConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetVideoEncoderConfiguration' (returns SOAP_OK or error code)
        virtual int SetVideoEncoderConfiguration(_onvifMedia__SetVideoEncoderConfiguration *onvifMedia__SetVideoEncoderConfiguration, _onvifMedia__SetVideoEncoderConfigurationResponse &onvifMedia__SetVideoEncoderConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetAudioSourceConfiguration' (returns SOAP_OK or error code)
        virtual int SetAudioSourceConfiguration(_onvifMedia__SetAudioSourceConfiguration *onvifMedia__SetAudioSourceConfiguration, _onvifMedia__SetAudioSourceConfigurationResponse &onvifMedia__SetAudioSourceConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetAudioEncoderConfiguration' (returns SOAP_OK or error code)
        virtual int SetAudioEncoderConfiguration(_onvifMedia__SetAudioEncoderConfiguration *onvifMedia__SetAudioEncoderConfiguration, _onvifMedia__SetAudioEncoderConfigurationResponse &onvifMedia__SetAudioEncoderConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetVideoAnalyticsConfiguration' (returns SOAP_OK or error code)
        virtual int SetVideoAnalyticsConfiguration(_onvifMedia__SetVideoAnalyticsConfiguration *onvifMedia__SetVideoAnalyticsConfiguration, _onvifMedia__SetVideoAnalyticsConfigurationResponse &onvifMedia__SetVideoAnalyticsConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetMetadataConfiguration' (returns SOAP_OK or error code)
        virtual int SetMetadataConfiguration(_onvifMedia__SetMetadataConfiguration *onvifMedia__SetMetadataConfiguration, _onvifMedia__SetMetadataConfigurationResponse &onvifMedia__SetMetadataConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetAudioOutputConfiguration' (returns SOAP_OK or error code)
        virtual int SetAudioOutputConfiguration(_onvifMedia__SetAudioOutputConfiguration *onvifMedia__SetAudioOutputConfiguration, _onvifMedia__SetAudioOutputConfigurationResponse &onvifMedia__SetAudioOutputConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetAudioDecoderConfiguration' (returns SOAP_OK or error code)
        virtual int SetAudioDecoderConfiguration(_onvifMedia__SetAudioDecoderConfiguration *onvifMedia__SetAudioDecoderConfiguration, _onvifMedia__SetAudioDecoderConfigurationResponse &onvifMedia__SetAudioDecoderConfigurationResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoSourceConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetVideoSourceConfigurationOptions(_onvifMedia__GetVideoSourceConfigurationOptions *onvifMedia__GetVideoSourceConfigurationOptions, _onvifMedia__GetVideoSourceConfigurationOptionsResponse &onvifMedia__GetVideoSourceConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoEncoderConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetVideoEncoderConfigurationOptions(_onvifMedia__GetVideoEncoderConfigurationOptions *onvifMedia__GetVideoEncoderConfigurationOptions, _onvifMedia__GetVideoEncoderConfigurationOptionsResponse &onvifMedia__GetVideoEncoderConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioSourceConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetAudioSourceConfigurationOptions(_onvifMedia__GetAudioSourceConfigurationOptions *onvifMedia__GetAudioSourceConfigurationOptions, _onvifMedia__GetAudioSourceConfigurationOptionsResponse &onvifMedia__GetAudioSourceConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioEncoderConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetAudioEncoderConfigurationOptions(_onvifMedia__GetAudioEncoderConfigurationOptions *onvifMedia__GetAudioEncoderConfigurationOptions, _onvifMedia__GetAudioEncoderConfigurationOptionsResponse &onvifMedia__GetAudioEncoderConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetMetadataConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetMetadataConfigurationOptions(_onvifMedia__GetMetadataConfigurationOptions *onvifMedia__GetMetadataConfigurationOptions, _onvifMedia__GetMetadataConfigurationOptionsResponse &onvifMedia__GetMetadataConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioOutputConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetAudioOutputConfigurationOptions(_onvifMedia__GetAudioOutputConfigurationOptions *onvifMedia__GetAudioOutputConfigurationOptions, _onvifMedia__GetAudioOutputConfigurationOptionsResponse &onvifMedia__GetAudioOutputConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetAudioDecoderConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetAudioDecoderConfigurationOptions(_onvifMedia__GetAudioDecoderConfigurationOptions *onvifMedia__GetAudioDecoderConfigurationOptions, _onvifMedia__GetAudioDecoderConfigurationOptionsResponse &onvifMedia__GetAudioDecoderConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetGuaranteedNumberOfVideoEncoderInstances' (returns SOAP_OK or error code)
        virtual int GetGuaranteedNumberOfVideoEncoderInstances(_onvifMedia__GetGuaranteedNumberOfVideoEncoderInstances *onvifMedia__GetGuaranteedNumberOfVideoEncoderInstances, _onvifMedia__GetGuaranteedNumberOfVideoEncoderInstancesResponse &onvifMedia__GetGuaranteedNumberOfVideoEncoderInstancesResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetStreamUri' (returns SOAP_OK or error code)
        virtual int GetStreamUri(_onvifMedia__GetStreamUri *onvifMedia__GetStreamUri, _onvifMedia__GetStreamUriResponse &onvifMedia__GetStreamUriResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'StartMulticastStreaming' (returns SOAP_OK or error code)
        virtual int StartMulticastStreaming(_onvifMedia__StartMulticastStreaming *onvifMedia__StartMulticastStreaming, _onvifMedia__StartMulticastStreamingResponse &onvifMedia__StartMulticastStreamingResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'StopMulticastStreaming' (returns SOAP_OK or error code)
        virtual int StopMulticastStreaming(_onvifMedia__StopMulticastStreaming *onvifMedia__StopMulticastStreaming, _onvifMedia__StopMulticastStreamingResponse &onvifMedia__StopMulticastStreamingResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetSynchronizationPoint' (returns SOAP_OK or error code)
        virtual int SetSynchronizationPoint(_onvifMedia__SetSynchronizationPoint *onvifMedia__SetSynchronizationPoint, _onvifMedia__SetSynchronizationPointResponse &onvifMedia__SetSynchronizationPointResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetSnapshotUri' (returns SOAP_OK or error code)
        virtual int GetSnapshotUri(_onvifMedia__GetSnapshotUri *onvifMedia__GetSnapshotUri, _onvifMedia__GetSnapshotUriResponse &onvifMedia__GetSnapshotUriResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetVideoSourceModes' (returns SOAP_OK or error code)
        virtual int GetVideoSourceModes(_onvifMedia__GetVideoSourceModes *onvifMedia__GetVideoSourceModes, _onvifMedia__GetVideoSourceModesResponse &onvifMedia__GetVideoSourceModesResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetVideoSourceMode' (returns SOAP_OK or error code)
        virtual int SetVideoSourceMode(_onvifMedia__SetVideoSourceMode *onvifMedia__SetVideoSourceMode, _onvifMedia__SetVideoSourceModeResponse &onvifMedia__SetVideoSourceModeResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetOSDs' (returns SOAP_OK or error code)
        virtual int GetOSDs(_onvifMedia__GetOSDs *onvifMedia__GetOSDs, _onvifMedia__GetOSDsResponse &onvifMedia__GetOSDsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetOSD' (returns SOAP_OK or error code)
        virtual int GetOSD(_onvifMedia__GetOSD *onvifMedia__GetOSD, _onvifMedia__GetOSDResponse &onvifMedia__GetOSDResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetOSDOptions' (returns SOAP_OK or error code)
        virtual int GetOSDOptions(_onvifMedia__GetOSDOptions *onvifMedia__GetOSDOptions, _onvifMedia__GetOSDOptionsResponse &onvifMedia__GetOSDOptionsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetOSD' (returns SOAP_OK or error code)
        virtual int SetOSD(_onvifMedia__SetOSD *onvifMedia__SetOSD, _onvifMedia__SetOSDResponse &onvifMedia__SetOSDResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'CreateOSD' (returns SOAP_OK or error code)
        virtual int CreateOSD(_onvifMedia__CreateOSD *onvifMedia__CreateOSD, _onvifMedia__CreateOSDResponse &onvifMedia__CreateOSDResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'DeleteOSD' (returns SOAP_OK or error code)
        virtual int DeleteOSD(_onvifMedia__DeleteOSD *onvifMedia__DeleteOSD, _onvifMedia__DeleteOSDResponse &onvifMedia__DeleteOSDResponse) SOAP_PURE_VIRTUAL;
    };
#endif
