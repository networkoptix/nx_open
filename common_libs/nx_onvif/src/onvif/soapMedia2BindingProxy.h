/* soapMedia2BindingProxy.h
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

#ifndef soapMedia2BindingProxy_H
#define soapMedia2BindingProxy_H
#include "soapH.h"

    class SOAP_CMAC Media2BindingProxy {
      public:
        /// Context to manage proxy IO and data
        struct soap *soap;
        bool soap_own; ///< flag indicating that this context is owned by this proxy when context is shared
        /// Endpoint URL of service 'Media2BindingProxy' (change as needed)
        const char *soap_endpoint;
        /// Variables globally declared in ../result/interim/onvif.h_, if any
        /// Construct a proxy with new managing context
        Media2BindingProxy();
        /// Copy constructor
        Media2BindingProxy(const Media2BindingProxy& rhs);
        /// Construct proxy given a shared managing context
        Media2BindingProxy(struct soap*);
        /// Constructor taking an endpoint URL
        Media2BindingProxy(const char *endpoint);
        /// Constructor taking input and output mode flags for the new managing context
        Media2BindingProxy(soap_mode iomode);
        /// Constructor taking endpoint URL and input and output mode flags for the new managing context
        Media2BindingProxy(const char *endpoint, soap_mode iomode);
        /// Constructor taking input and output mode flags for the new managing context
        Media2BindingProxy(soap_mode imode, soap_mode omode);
        /// Destructor deletes non-shared managing context only (use destroy() to delete deserialized data)
        virtual ~Media2BindingProxy();
        /// Initializer used by constructors
        virtual void Media2BindingProxy_init(soap_mode imode, soap_mode omode);
        /// Return a copy that has a new managing context with the same engine state
        virtual Media2BindingProxy *copy();
        /// Copy assignment
        Media2BindingProxy& operator=(const Media2BindingProxy&);
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
        virtual int GetServiceCapabilities(_onvifMedia2__GetServiceCapabilities *onvifMedia2__GetServiceCapabilities, _onvifMedia2__GetServiceCapabilitiesResponse &onvifMedia2__GetServiceCapabilitiesResponse)
        { return this->GetServiceCapabilities(NULL, NULL, onvifMedia2__GetServiceCapabilities, onvifMedia2__GetServiceCapabilitiesResponse); }
        virtual int GetServiceCapabilities(const char *soap_endpoint, const char *soap_action, _onvifMedia2__GetServiceCapabilities *onvifMedia2__GetServiceCapabilities, _onvifMedia2__GetServiceCapabilitiesResponse &onvifMedia2__GetServiceCapabilitiesResponse);
        /// Web service operation 'CreateProfile' (returns SOAP_OK or error code)
        virtual int CreateProfile(_onvifMedia2__CreateProfile *onvifMedia2__CreateProfile, _onvifMedia2__CreateProfileResponse &onvifMedia2__CreateProfileResponse)
        { return this->CreateProfile(NULL, NULL, onvifMedia2__CreateProfile, onvifMedia2__CreateProfileResponse); }
        virtual int CreateProfile(const char *soap_endpoint, const char *soap_action, _onvifMedia2__CreateProfile *onvifMedia2__CreateProfile, _onvifMedia2__CreateProfileResponse &onvifMedia2__CreateProfileResponse);
        /// Web service operation 'GetProfiles' (returns SOAP_OK or error code)
        virtual int GetProfiles(_onvifMedia2__GetProfiles *onvifMedia2__GetProfiles, _onvifMedia2__GetProfilesResponse &onvifMedia2__GetProfilesResponse)
        { return this->GetProfiles(NULL, NULL, onvifMedia2__GetProfiles, onvifMedia2__GetProfilesResponse); }
        virtual int GetProfiles(const char *soap_endpoint, const char *soap_action, _onvifMedia2__GetProfiles *onvifMedia2__GetProfiles, _onvifMedia2__GetProfilesResponse &onvifMedia2__GetProfilesResponse);
        /// Web service operation 'AddConfiguration' (returns SOAP_OK or error code)
        virtual int AddConfiguration(_onvifMedia2__AddConfiguration *onvifMedia2__AddConfiguration, _onvifMedia2__AddConfigurationResponse &onvifMedia2__AddConfigurationResponse)
        { return this->AddConfiguration(NULL, NULL, onvifMedia2__AddConfiguration, onvifMedia2__AddConfigurationResponse); }
        virtual int AddConfiguration(const char *soap_endpoint, const char *soap_action, _onvifMedia2__AddConfiguration *onvifMedia2__AddConfiguration, _onvifMedia2__AddConfigurationResponse &onvifMedia2__AddConfigurationResponse);
        /// Web service operation 'RemoveConfiguration' (returns SOAP_OK or error code)
        virtual int RemoveConfiguration(_onvifMedia2__RemoveConfiguration *onvifMedia2__RemoveConfiguration, _onvifMedia2__RemoveConfigurationResponse &onvifMedia2__RemoveConfigurationResponse)
        { return this->RemoveConfiguration(NULL, NULL, onvifMedia2__RemoveConfiguration, onvifMedia2__RemoveConfigurationResponse); }
        virtual int RemoveConfiguration(const char *soap_endpoint, const char *soap_action, _onvifMedia2__RemoveConfiguration *onvifMedia2__RemoveConfiguration, _onvifMedia2__RemoveConfigurationResponse &onvifMedia2__RemoveConfigurationResponse);
        /// Web service operation 'DeleteProfile' (returns SOAP_OK or error code)
        virtual int DeleteProfile(_onvifMedia2__DeleteProfile *onvifMedia2__DeleteProfile, _onvifMedia2__DeleteProfileResponse &onvifMedia2__DeleteProfileResponse)
        { return this->DeleteProfile(NULL, NULL, onvifMedia2__DeleteProfile, onvifMedia2__DeleteProfileResponse); }
        virtual int DeleteProfile(const char *soap_endpoint, const char *soap_action, _onvifMedia2__DeleteProfile *onvifMedia2__DeleteProfile, _onvifMedia2__DeleteProfileResponse &onvifMedia2__DeleteProfileResponse);
        /// Web service operation 'GetVideoSourceConfigurations' (returns SOAP_OK or error code)
        virtual int GetVideoSourceConfigurations(onvifMedia2__GetConfiguration *onvifMedia2__GetVideoSourceConfigurations, _onvifMedia2__GetVideoSourceConfigurationsResponse &onvifMedia2__GetVideoSourceConfigurationsResponse)
        { return this->GetVideoSourceConfigurations(NULL, NULL, onvifMedia2__GetVideoSourceConfigurations, onvifMedia2__GetVideoSourceConfigurationsResponse); }
        virtual int GetVideoSourceConfigurations(const char *soap_endpoint, const char *soap_action, onvifMedia2__GetConfiguration *onvifMedia2__GetVideoSourceConfigurations, _onvifMedia2__GetVideoSourceConfigurationsResponse &onvifMedia2__GetVideoSourceConfigurationsResponse);
        /// Web service operation 'GetVideoEncoderConfigurations' (returns SOAP_OK or error code)
        virtual int GetVideoEncoderConfigurations(onvifMedia2__GetConfiguration *onvifMedia2__GetVideoEncoderConfigurations, _onvifMedia2__GetVideoEncoderConfigurationsResponse &onvifMedia2__GetVideoEncoderConfigurationsResponse)
        { return this->GetVideoEncoderConfigurations(NULL, NULL, onvifMedia2__GetVideoEncoderConfigurations, onvifMedia2__GetVideoEncoderConfigurationsResponse); }
        virtual int GetVideoEncoderConfigurations(const char *soap_endpoint, const char *soap_action, onvifMedia2__GetConfiguration *onvifMedia2__GetVideoEncoderConfigurations, _onvifMedia2__GetVideoEncoderConfigurationsResponse &onvifMedia2__GetVideoEncoderConfigurationsResponse);
        /// Web service operation 'GetAudioSourceConfigurations' (returns SOAP_OK or error code)
        virtual int GetAudioSourceConfigurations(onvifMedia2__GetConfiguration *onvifMedia2__GetAudioSourceConfigurations, _onvifMedia2__GetAudioSourceConfigurationsResponse &onvifMedia2__GetAudioSourceConfigurationsResponse)
        { return this->GetAudioSourceConfigurations(NULL, NULL, onvifMedia2__GetAudioSourceConfigurations, onvifMedia2__GetAudioSourceConfigurationsResponse); }
        virtual int GetAudioSourceConfigurations(const char *soap_endpoint, const char *soap_action, onvifMedia2__GetConfiguration *onvifMedia2__GetAudioSourceConfigurations, _onvifMedia2__GetAudioSourceConfigurationsResponse &onvifMedia2__GetAudioSourceConfigurationsResponse);
        /// Web service operation 'GetAudioEncoderConfigurations' (returns SOAP_OK or error code)
        virtual int GetAudioEncoderConfigurations(onvifMedia2__GetConfiguration *onvifMedia2__GetAudioEncoderConfigurations, _onvifMedia2__GetAudioEncoderConfigurationsResponse &onvifMedia2__GetAudioEncoderConfigurationsResponse)
        { return this->GetAudioEncoderConfigurations(NULL, NULL, onvifMedia2__GetAudioEncoderConfigurations, onvifMedia2__GetAudioEncoderConfigurationsResponse); }
        virtual int GetAudioEncoderConfigurations(const char *soap_endpoint, const char *soap_action, onvifMedia2__GetConfiguration *onvifMedia2__GetAudioEncoderConfigurations, _onvifMedia2__GetAudioEncoderConfigurationsResponse &onvifMedia2__GetAudioEncoderConfigurationsResponse);
        /// Web service operation 'GetAnalyticsConfigurations' (returns SOAP_OK or error code)
        virtual int GetAnalyticsConfigurations(onvifMedia2__GetConfiguration *onvifMedia2__GetAnalyticsConfigurations, _onvifMedia2__GetAnalyticsConfigurationsResponse &onvifMedia2__GetAnalyticsConfigurationsResponse)
        { return this->GetAnalyticsConfigurations(NULL, NULL, onvifMedia2__GetAnalyticsConfigurations, onvifMedia2__GetAnalyticsConfigurationsResponse); }
        virtual int GetAnalyticsConfigurations(const char *soap_endpoint, const char *soap_action, onvifMedia2__GetConfiguration *onvifMedia2__GetAnalyticsConfigurations, _onvifMedia2__GetAnalyticsConfigurationsResponse &onvifMedia2__GetAnalyticsConfigurationsResponse);
        /// Web service operation 'GetMetadataConfigurations' (returns SOAP_OK or error code)
        virtual int GetMetadataConfigurations(onvifMedia2__GetConfiguration *onvifMedia2__GetMetadataConfigurations, _onvifMedia2__GetMetadataConfigurationsResponse &onvifMedia2__GetMetadataConfigurationsResponse)
        { return this->GetMetadataConfigurations(NULL, NULL, onvifMedia2__GetMetadataConfigurations, onvifMedia2__GetMetadataConfigurationsResponse); }
        virtual int GetMetadataConfigurations(const char *soap_endpoint, const char *soap_action, onvifMedia2__GetConfiguration *onvifMedia2__GetMetadataConfigurations, _onvifMedia2__GetMetadataConfigurationsResponse &onvifMedia2__GetMetadataConfigurationsResponse);
        /// Web service operation 'GetAudioOutputConfigurations' (returns SOAP_OK or error code)
        virtual int GetAudioOutputConfigurations(onvifMedia2__GetConfiguration *onvifMedia2__GetAudioOutputConfigurations, _onvifMedia2__GetAudioOutputConfigurationsResponse &onvifMedia2__GetAudioOutputConfigurationsResponse)
        { return this->GetAudioOutputConfigurations(NULL, NULL, onvifMedia2__GetAudioOutputConfigurations, onvifMedia2__GetAudioOutputConfigurationsResponse); }
        virtual int GetAudioOutputConfigurations(const char *soap_endpoint, const char *soap_action, onvifMedia2__GetConfiguration *onvifMedia2__GetAudioOutputConfigurations, _onvifMedia2__GetAudioOutputConfigurationsResponse &onvifMedia2__GetAudioOutputConfigurationsResponse);
        /// Web service operation 'GetAudioDecoderConfigurations' (returns SOAP_OK or error code)
        virtual int GetAudioDecoderConfigurations(onvifMedia2__GetConfiguration *onvifMedia2__GetAudioDecoderConfigurations, _onvifMedia2__GetAudioDecoderConfigurationsResponse &onvifMedia2__GetAudioDecoderConfigurationsResponse)
        { return this->GetAudioDecoderConfigurations(NULL, NULL, onvifMedia2__GetAudioDecoderConfigurations, onvifMedia2__GetAudioDecoderConfigurationsResponse); }
        virtual int GetAudioDecoderConfigurations(const char *soap_endpoint, const char *soap_action, onvifMedia2__GetConfiguration *onvifMedia2__GetAudioDecoderConfigurations, _onvifMedia2__GetAudioDecoderConfigurationsResponse &onvifMedia2__GetAudioDecoderConfigurationsResponse);
        /// Web service operation 'SetVideoSourceConfiguration' (returns SOAP_OK or error code)
        virtual int SetVideoSourceConfiguration(_onvifMedia2__SetVideoSourceConfiguration *onvifMedia2__SetVideoSourceConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetVideoSourceConfigurationResponse)
        { return this->SetVideoSourceConfiguration(NULL, NULL, onvifMedia2__SetVideoSourceConfiguration, onvifMedia2__SetVideoSourceConfigurationResponse); }
        virtual int SetVideoSourceConfiguration(const char *soap_endpoint, const char *soap_action, _onvifMedia2__SetVideoSourceConfiguration *onvifMedia2__SetVideoSourceConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetVideoSourceConfigurationResponse);
        /// Web service operation 'SetVideoEncoderConfiguration' (returns SOAP_OK or error code)
        virtual int SetVideoEncoderConfiguration(_onvifMedia2__SetVideoEncoderConfiguration *onvifMedia2__SetVideoEncoderConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetVideoEncoderConfigurationResponse)
        { return this->SetVideoEncoderConfiguration(NULL, NULL, onvifMedia2__SetVideoEncoderConfiguration, onvifMedia2__SetVideoEncoderConfigurationResponse); }
        virtual int SetVideoEncoderConfiguration(const char *soap_endpoint, const char *soap_action, _onvifMedia2__SetVideoEncoderConfiguration *onvifMedia2__SetVideoEncoderConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetVideoEncoderConfigurationResponse);
        /// Web service operation 'SetAudioSourceConfiguration' (returns SOAP_OK or error code)
        virtual int SetAudioSourceConfiguration(_onvifMedia2__SetAudioSourceConfiguration *onvifMedia2__SetAudioSourceConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetAudioSourceConfigurationResponse)
        { return this->SetAudioSourceConfiguration(NULL, NULL, onvifMedia2__SetAudioSourceConfiguration, onvifMedia2__SetAudioSourceConfigurationResponse); }
        virtual int SetAudioSourceConfiguration(const char *soap_endpoint, const char *soap_action, _onvifMedia2__SetAudioSourceConfiguration *onvifMedia2__SetAudioSourceConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetAudioSourceConfigurationResponse);
        /// Web service operation 'SetAudioEncoderConfiguration' (returns SOAP_OK or error code)
        virtual int SetAudioEncoderConfiguration(_onvifMedia2__SetAudioEncoderConfiguration *onvifMedia2__SetAudioEncoderConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetAudioEncoderConfigurationResponse)
        { return this->SetAudioEncoderConfiguration(NULL, NULL, onvifMedia2__SetAudioEncoderConfiguration, onvifMedia2__SetAudioEncoderConfigurationResponse); }
        virtual int SetAudioEncoderConfiguration(const char *soap_endpoint, const char *soap_action, _onvifMedia2__SetAudioEncoderConfiguration *onvifMedia2__SetAudioEncoderConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetAudioEncoderConfigurationResponse);
        /// Web service operation 'SetMetadataConfiguration' (returns SOAP_OK or error code)
        virtual int SetMetadataConfiguration(_onvifMedia2__SetMetadataConfiguration *onvifMedia2__SetMetadataConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetMetadataConfigurationResponse)
        { return this->SetMetadataConfiguration(NULL, NULL, onvifMedia2__SetMetadataConfiguration, onvifMedia2__SetMetadataConfigurationResponse); }
        virtual int SetMetadataConfiguration(const char *soap_endpoint, const char *soap_action, _onvifMedia2__SetMetadataConfiguration *onvifMedia2__SetMetadataConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetMetadataConfigurationResponse);
        /// Web service operation 'SetAudioOutputConfiguration' (returns SOAP_OK or error code)
        virtual int SetAudioOutputConfiguration(_onvifMedia2__SetAudioOutputConfiguration *onvifMedia2__SetAudioOutputConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetAudioOutputConfigurationResponse)
        { return this->SetAudioOutputConfiguration(NULL, NULL, onvifMedia2__SetAudioOutputConfiguration, onvifMedia2__SetAudioOutputConfigurationResponse); }
        virtual int SetAudioOutputConfiguration(const char *soap_endpoint, const char *soap_action, _onvifMedia2__SetAudioOutputConfiguration *onvifMedia2__SetAudioOutputConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetAudioOutputConfigurationResponse);
        /// Web service operation 'SetAudioDecoderConfiguration' (returns SOAP_OK or error code)
        virtual int SetAudioDecoderConfiguration(_onvifMedia2__SetAudioDecoderConfiguration *onvifMedia2__SetAudioDecoderConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetAudioDecoderConfigurationResponse)
        { return this->SetAudioDecoderConfiguration(NULL, NULL, onvifMedia2__SetAudioDecoderConfiguration, onvifMedia2__SetAudioDecoderConfigurationResponse); }
        virtual int SetAudioDecoderConfiguration(const char *soap_endpoint, const char *soap_action, _onvifMedia2__SetAudioDecoderConfiguration *onvifMedia2__SetAudioDecoderConfiguration, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetAudioDecoderConfigurationResponse);
        /// Web service operation 'GetVideoSourceConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetVideoSourceConfigurationOptions(onvifMedia2__GetConfiguration *onvifMedia2__GetVideoSourceConfigurationOptions, _onvifMedia2__GetVideoSourceConfigurationOptionsResponse &onvifMedia2__GetVideoSourceConfigurationOptionsResponse)
        { return this->GetVideoSourceConfigurationOptions(NULL, NULL, onvifMedia2__GetVideoSourceConfigurationOptions, onvifMedia2__GetVideoSourceConfigurationOptionsResponse); }
        virtual int GetVideoSourceConfigurationOptions(const char *soap_endpoint, const char *soap_action, onvifMedia2__GetConfiguration *onvifMedia2__GetVideoSourceConfigurationOptions, _onvifMedia2__GetVideoSourceConfigurationOptionsResponse &onvifMedia2__GetVideoSourceConfigurationOptionsResponse);
        /// Web service operation 'GetVideoEncoderConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetVideoEncoderConfigurationOptions(onvifMedia2__GetConfiguration *onvifMedia2__GetVideoEncoderConfigurationOptions, _onvifMedia2__GetVideoEncoderConfigurationOptionsResponse &onvifMedia2__GetVideoEncoderConfigurationOptionsResponse)
        { return this->GetVideoEncoderConfigurationOptions(NULL, NULL, onvifMedia2__GetVideoEncoderConfigurationOptions, onvifMedia2__GetVideoEncoderConfigurationOptionsResponse); }
        virtual int GetVideoEncoderConfigurationOptions(const char *soap_endpoint, const char *soap_action, onvifMedia2__GetConfiguration *onvifMedia2__GetVideoEncoderConfigurationOptions, _onvifMedia2__GetVideoEncoderConfigurationOptionsResponse &onvifMedia2__GetVideoEncoderConfigurationOptionsResponse);
        /// Web service operation 'GetAudioSourceConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetAudioSourceConfigurationOptions(onvifMedia2__GetConfiguration *onvifMedia2__GetAudioSourceConfigurationOptions, _onvifMedia2__GetAudioSourceConfigurationOptionsResponse &onvifMedia2__GetAudioSourceConfigurationOptionsResponse)
        { return this->GetAudioSourceConfigurationOptions(NULL, NULL, onvifMedia2__GetAudioSourceConfigurationOptions, onvifMedia2__GetAudioSourceConfigurationOptionsResponse); }
        virtual int GetAudioSourceConfigurationOptions(const char *soap_endpoint, const char *soap_action, onvifMedia2__GetConfiguration *onvifMedia2__GetAudioSourceConfigurationOptions, _onvifMedia2__GetAudioSourceConfigurationOptionsResponse &onvifMedia2__GetAudioSourceConfigurationOptionsResponse);
        /// Web service operation 'GetAudioEncoderConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetAudioEncoderConfigurationOptions(onvifMedia2__GetConfiguration *onvifMedia2__GetAudioEncoderConfigurationOptions, _onvifMedia2__GetAudioEncoderConfigurationOptionsResponse &onvifMedia2__GetAudioEncoderConfigurationOptionsResponse)
        { return this->GetAudioEncoderConfigurationOptions(NULL, NULL, onvifMedia2__GetAudioEncoderConfigurationOptions, onvifMedia2__GetAudioEncoderConfigurationOptionsResponse); }
        virtual int GetAudioEncoderConfigurationOptions(const char *soap_endpoint, const char *soap_action, onvifMedia2__GetConfiguration *onvifMedia2__GetAudioEncoderConfigurationOptions, _onvifMedia2__GetAudioEncoderConfigurationOptionsResponse &onvifMedia2__GetAudioEncoderConfigurationOptionsResponse);
        /// Web service operation 'GetMetadataConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetMetadataConfigurationOptions(onvifMedia2__GetConfiguration *onvifMedia2__GetMetadataConfigurationOptions, _onvifMedia2__GetMetadataConfigurationOptionsResponse &onvifMedia2__GetMetadataConfigurationOptionsResponse)
        { return this->GetMetadataConfigurationOptions(NULL, NULL, onvifMedia2__GetMetadataConfigurationOptions, onvifMedia2__GetMetadataConfigurationOptionsResponse); }
        virtual int GetMetadataConfigurationOptions(const char *soap_endpoint, const char *soap_action, onvifMedia2__GetConfiguration *onvifMedia2__GetMetadataConfigurationOptions, _onvifMedia2__GetMetadataConfigurationOptionsResponse &onvifMedia2__GetMetadataConfigurationOptionsResponse);
        /// Web service operation 'GetAudioOutputConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetAudioOutputConfigurationOptions(onvifMedia2__GetConfiguration *onvifMedia2__GetAudioOutputConfigurationOptions, _onvifMedia2__GetAudioOutputConfigurationOptionsResponse &onvifMedia2__GetAudioOutputConfigurationOptionsResponse)
        { return this->GetAudioOutputConfigurationOptions(NULL, NULL, onvifMedia2__GetAudioOutputConfigurationOptions, onvifMedia2__GetAudioOutputConfigurationOptionsResponse); }
        virtual int GetAudioOutputConfigurationOptions(const char *soap_endpoint, const char *soap_action, onvifMedia2__GetConfiguration *onvifMedia2__GetAudioOutputConfigurationOptions, _onvifMedia2__GetAudioOutputConfigurationOptionsResponse &onvifMedia2__GetAudioOutputConfigurationOptionsResponse);
        /// Web service operation 'GetAudioDecoderConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetAudioDecoderConfigurationOptions(onvifMedia2__GetConfiguration *onvifMedia2__GetAudioDecoderConfigurationOptions, _onvifMedia2__GetAudioDecoderConfigurationOptionsResponse &onvifMedia2__GetAudioDecoderConfigurationOptionsResponse)
        { return this->GetAudioDecoderConfigurationOptions(NULL, NULL, onvifMedia2__GetAudioDecoderConfigurationOptions, onvifMedia2__GetAudioDecoderConfigurationOptionsResponse); }
        virtual int GetAudioDecoderConfigurationOptions(const char *soap_endpoint, const char *soap_action, onvifMedia2__GetConfiguration *onvifMedia2__GetAudioDecoderConfigurationOptions, _onvifMedia2__GetAudioDecoderConfigurationOptionsResponse &onvifMedia2__GetAudioDecoderConfigurationOptionsResponse);
        /// Web service operation 'GetVideoEncoderInstances' (returns SOAP_OK or error code)
        virtual int GetVideoEncoderInstances(_onvifMedia2__GetVideoEncoderInstances *onvifMedia2__GetVideoEncoderInstances, _onvifMedia2__GetVideoEncoderInstancesResponse &onvifMedia2__GetVideoEncoderInstancesResponse)
        { return this->GetVideoEncoderInstances(NULL, NULL, onvifMedia2__GetVideoEncoderInstances, onvifMedia2__GetVideoEncoderInstancesResponse); }
        virtual int GetVideoEncoderInstances(const char *soap_endpoint, const char *soap_action, _onvifMedia2__GetVideoEncoderInstances *onvifMedia2__GetVideoEncoderInstances, _onvifMedia2__GetVideoEncoderInstancesResponse &onvifMedia2__GetVideoEncoderInstancesResponse);
        /// Web service operation 'GetStreamUri' (returns SOAP_OK or error code)
        virtual int GetStreamUri(_onvifMedia2__GetStreamUri *onvifMedia2__GetStreamUri, _onvifMedia2__GetStreamUriResponse &onvifMedia2__GetStreamUriResponse)
        { return this->GetStreamUri(NULL, NULL, onvifMedia2__GetStreamUri, onvifMedia2__GetStreamUriResponse); }
        virtual int GetStreamUri(const char *soap_endpoint, const char *soap_action, _onvifMedia2__GetStreamUri *onvifMedia2__GetStreamUri, _onvifMedia2__GetStreamUriResponse &onvifMedia2__GetStreamUriResponse);
        /// Web service operation 'StartMulticastStreaming' (returns SOAP_OK or error code)
        virtual int StartMulticastStreaming(onvifMedia2__StartStopMulticastStreaming *onvifMedia2__StartMulticastStreaming, onvifMedia2__SetConfigurationResponse &onvifMedia2__StartMulticastStreamingResponse)
        { return this->StartMulticastStreaming(NULL, NULL, onvifMedia2__StartMulticastStreaming, onvifMedia2__StartMulticastStreamingResponse); }
        virtual int StartMulticastStreaming(const char *soap_endpoint, const char *soap_action, onvifMedia2__StartStopMulticastStreaming *onvifMedia2__StartMulticastStreaming, onvifMedia2__SetConfigurationResponse &onvifMedia2__StartMulticastStreamingResponse);
        /// Web service operation 'StopMulticastStreaming' (returns SOAP_OK or error code)
        virtual int StopMulticastStreaming(onvifMedia2__StartStopMulticastStreaming *onvifMedia2__StopMulticastStreaming, onvifMedia2__SetConfigurationResponse &onvifMedia2__StopMulticastStreamingResponse)
        { return this->StopMulticastStreaming(NULL, NULL, onvifMedia2__StopMulticastStreaming, onvifMedia2__StopMulticastStreamingResponse); }
        virtual int StopMulticastStreaming(const char *soap_endpoint, const char *soap_action, onvifMedia2__StartStopMulticastStreaming *onvifMedia2__StopMulticastStreaming, onvifMedia2__SetConfigurationResponse &onvifMedia2__StopMulticastStreamingResponse);
        /// Web service operation 'SetSynchronizationPoint' (returns SOAP_OK or error code)
        virtual int SetSynchronizationPoint(_onvifMedia2__SetSynchronizationPoint *onvifMedia2__SetSynchronizationPoint, _onvifMedia2__SetSynchronizationPointResponse &onvifMedia2__SetSynchronizationPointResponse)
        { return this->SetSynchronizationPoint(NULL, NULL, onvifMedia2__SetSynchronizationPoint, onvifMedia2__SetSynchronizationPointResponse); }
        virtual int SetSynchronizationPoint(const char *soap_endpoint, const char *soap_action, _onvifMedia2__SetSynchronizationPoint *onvifMedia2__SetSynchronizationPoint, _onvifMedia2__SetSynchronizationPointResponse &onvifMedia2__SetSynchronizationPointResponse);
        /// Web service operation 'GetSnapshotUri' (returns SOAP_OK or error code)
        virtual int GetSnapshotUri(_onvifMedia2__GetSnapshotUri *onvifMedia2__GetSnapshotUri, _onvifMedia2__GetSnapshotUriResponse &onvifMedia2__GetSnapshotUriResponse)
        { return this->GetSnapshotUri(NULL, NULL, onvifMedia2__GetSnapshotUri, onvifMedia2__GetSnapshotUriResponse); }
        virtual int GetSnapshotUri(const char *soap_endpoint, const char *soap_action, _onvifMedia2__GetSnapshotUri *onvifMedia2__GetSnapshotUri, _onvifMedia2__GetSnapshotUriResponse &onvifMedia2__GetSnapshotUriResponse);
        /// Web service operation 'GetVideoSourceModes' (returns SOAP_OK or error code)
        virtual int GetVideoSourceModes(_onvifMedia2__GetVideoSourceModes *onvifMedia2__GetVideoSourceModes, _onvifMedia2__GetVideoSourceModesResponse &onvifMedia2__GetVideoSourceModesResponse)
        { return this->GetVideoSourceModes(NULL, NULL, onvifMedia2__GetVideoSourceModes, onvifMedia2__GetVideoSourceModesResponse); }
        virtual int GetVideoSourceModes(const char *soap_endpoint, const char *soap_action, _onvifMedia2__GetVideoSourceModes *onvifMedia2__GetVideoSourceModes, _onvifMedia2__GetVideoSourceModesResponse &onvifMedia2__GetVideoSourceModesResponse);
        /// Web service operation 'SetVideoSourceMode' (returns SOAP_OK or error code)
        virtual int SetVideoSourceMode(_onvifMedia2__SetVideoSourceMode *onvifMedia2__SetVideoSourceMode, _onvifMedia2__SetVideoSourceModeResponse &onvifMedia2__SetVideoSourceModeResponse)
        { return this->SetVideoSourceMode(NULL, NULL, onvifMedia2__SetVideoSourceMode, onvifMedia2__SetVideoSourceModeResponse); }
        virtual int SetVideoSourceMode(const char *soap_endpoint, const char *soap_action, _onvifMedia2__SetVideoSourceMode *onvifMedia2__SetVideoSourceMode, _onvifMedia2__SetVideoSourceModeResponse &onvifMedia2__SetVideoSourceModeResponse);
        /// Web service operation 'GetOSDs' (returns SOAP_OK or error code)
        virtual int GetOSDs(_onvifMedia2__GetOSDs *onvifMedia2__GetOSDs, _onvifMedia2__GetOSDsResponse &onvifMedia2__GetOSDsResponse)
        { return this->GetOSDs(NULL, NULL, onvifMedia2__GetOSDs, onvifMedia2__GetOSDsResponse); }
        virtual int GetOSDs(const char *soap_endpoint, const char *soap_action, _onvifMedia2__GetOSDs *onvifMedia2__GetOSDs, _onvifMedia2__GetOSDsResponse &onvifMedia2__GetOSDsResponse);
        /// Web service operation 'GetOSDOptions' (returns SOAP_OK or error code)
        virtual int GetOSDOptions(_onvifMedia2__GetOSDOptions *onvifMedia2__GetOSDOptions, _onvifMedia2__GetOSDOptionsResponse &onvifMedia2__GetOSDOptionsResponse)
        { return this->GetOSDOptions(NULL, NULL, onvifMedia2__GetOSDOptions, onvifMedia2__GetOSDOptionsResponse); }
        virtual int GetOSDOptions(const char *soap_endpoint, const char *soap_action, _onvifMedia2__GetOSDOptions *onvifMedia2__GetOSDOptions, _onvifMedia2__GetOSDOptionsResponse &onvifMedia2__GetOSDOptionsResponse);
        /// Web service operation 'SetOSD' (returns SOAP_OK or error code)
        virtual int SetOSD(_onvifMedia2__SetOSD *onvifMedia2__SetOSD, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetOSDResponse)
        { return this->SetOSD(NULL, NULL, onvifMedia2__SetOSD, onvifMedia2__SetOSDResponse); }
        virtual int SetOSD(const char *soap_endpoint, const char *soap_action, _onvifMedia2__SetOSD *onvifMedia2__SetOSD, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetOSDResponse);
        /// Web service operation 'CreateOSD' (returns SOAP_OK or error code)
        virtual int CreateOSD(_onvifMedia2__CreateOSD *onvifMedia2__CreateOSD, _onvifMedia2__CreateOSDResponse &onvifMedia2__CreateOSDResponse)
        { return this->CreateOSD(NULL, NULL, onvifMedia2__CreateOSD, onvifMedia2__CreateOSDResponse); }
        virtual int CreateOSD(const char *soap_endpoint, const char *soap_action, _onvifMedia2__CreateOSD *onvifMedia2__CreateOSD, _onvifMedia2__CreateOSDResponse &onvifMedia2__CreateOSDResponse);
        /// Web service operation 'DeleteOSD' (returns SOAP_OK or error code)
        virtual int DeleteOSD(_onvifMedia2__DeleteOSD *onvifMedia2__DeleteOSD, onvifMedia2__SetConfigurationResponse &onvifMedia2__DeleteOSDResponse)
        { return this->DeleteOSD(NULL, NULL, onvifMedia2__DeleteOSD, onvifMedia2__DeleteOSDResponse); }
        virtual int DeleteOSD(const char *soap_endpoint, const char *soap_action, _onvifMedia2__DeleteOSD *onvifMedia2__DeleteOSD, onvifMedia2__SetConfigurationResponse &onvifMedia2__DeleteOSDResponse);
        /// Web service operation 'GetMasks' (returns SOAP_OK or error code)
        virtual int GetMasks(_onvifMedia2__GetMasks *onvifMedia2__GetMasks, _onvifMedia2__GetMasksResponse &onvifMedia2__GetMasksResponse)
        { return this->GetMasks(NULL, NULL, onvifMedia2__GetMasks, onvifMedia2__GetMasksResponse); }
        virtual int GetMasks(const char *soap_endpoint, const char *soap_action, _onvifMedia2__GetMasks *onvifMedia2__GetMasks, _onvifMedia2__GetMasksResponse &onvifMedia2__GetMasksResponse);
        /// Web service operation 'GetMaskOptions' (returns SOAP_OK or error code)
        virtual int GetMaskOptions(_onvifMedia2__GetMaskOptions *onvifMedia2__GetMaskOptions, _onvifMedia2__GetMaskOptionsResponse &onvifMedia2__GetMaskOptionsResponse)
        { return this->GetMaskOptions(NULL, NULL, onvifMedia2__GetMaskOptions, onvifMedia2__GetMaskOptionsResponse); }
        virtual int GetMaskOptions(const char *soap_endpoint, const char *soap_action, _onvifMedia2__GetMaskOptions *onvifMedia2__GetMaskOptions, _onvifMedia2__GetMaskOptionsResponse &onvifMedia2__GetMaskOptionsResponse);
        /// Web service operation 'SetMask' (returns SOAP_OK or error code)
        virtual int SetMask(_onvifMedia2__SetMask *onvifMedia2__SetMask, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetMaskResponse)
        { return this->SetMask(NULL, NULL, onvifMedia2__SetMask, onvifMedia2__SetMaskResponse); }
        virtual int SetMask(const char *soap_endpoint, const char *soap_action, _onvifMedia2__SetMask *onvifMedia2__SetMask, onvifMedia2__SetConfigurationResponse &onvifMedia2__SetMaskResponse);
        /// Web service operation 'CreateMask' (returns SOAP_OK or error code)
        virtual int CreateMask(_onvifMedia2__CreateMask *onvifMedia2__CreateMask, _onvifMedia2__CreateMaskResponse &onvifMedia2__CreateMaskResponse)
        { return this->CreateMask(NULL, NULL, onvifMedia2__CreateMask, onvifMedia2__CreateMaskResponse); }
        virtual int CreateMask(const char *soap_endpoint, const char *soap_action, _onvifMedia2__CreateMask *onvifMedia2__CreateMask, _onvifMedia2__CreateMaskResponse &onvifMedia2__CreateMaskResponse);
        /// Web service operation 'DeleteMask' (returns SOAP_OK or error code)
        virtual int DeleteMask(_onvifMedia2__DeleteMask *onvifMedia2__DeleteMask, onvifMedia2__SetConfigurationResponse &onvifMedia2__DeleteMaskResponse)
        { return this->DeleteMask(NULL, NULL, onvifMedia2__DeleteMask, onvifMedia2__DeleteMaskResponse); }
        virtual int DeleteMask(const char *soap_endpoint, const char *soap_action, _onvifMedia2__DeleteMask *onvifMedia2__DeleteMask, onvifMedia2__SetConfigurationResponse &onvifMedia2__DeleteMaskResponse);
    };
#endif
