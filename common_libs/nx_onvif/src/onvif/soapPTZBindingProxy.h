/* soapPTZBindingProxy.h
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

#ifndef soapPTZBindingProxy_H
#define soapPTZBindingProxy_H
#include "soapH.h"

    class SOAP_CMAC PTZBindingProxy {
      public:
        /// Context to manage proxy IO and data
        struct soap *soap;
        bool soap_own; ///< flag indicating that this context is owned by this proxy when context is shared
        /// Endpoint URL of service 'PTZBindingProxy' (change as needed)
        const char *soap_endpoint;
        /// Variables globally declared in ../result/interim/onvif.h_, if any
        /// Construct a proxy with new managing context
        PTZBindingProxy();
        /// Copy constructor
        PTZBindingProxy(const PTZBindingProxy& rhs);
        /// Construct proxy given a shared managing context
        PTZBindingProxy(struct soap*);
        /// Constructor taking an endpoint URL
        PTZBindingProxy(const char *endpoint);
        /// Constructor taking input and output mode flags for the new managing context
        PTZBindingProxy(soap_mode iomode);
        /// Constructor taking endpoint URL and input and output mode flags for the new managing context
        PTZBindingProxy(const char *endpoint, soap_mode iomode);
        /// Constructor taking input and output mode flags for the new managing context
        PTZBindingProxy(soap_mode imode, soap_mode omode);
        /// Destructor deletes non-shared managing context only (use destroy() to delete deserialized data)
        virtual ~PTZBindingProxy();
        /// Initializer used by constructors
        virtual void PTZBindingProxy_init(soap_mode imode, soap_mode omode);
        /// Return a copy that has a new managing context with the same engine state
        virtual PTZBindingProxy *copy();
        /// Copy assignment
        PTZBindingProxy& operator=(const PTZBindingProxy&);
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
        virtual int GetServiceCapabilities(_onvifPtz__GetServiceCapabilities *onvifPtz__GetServiceCapabilities, _onvifPtz__GetServiceCapabilitiesResponse &onvifPtz__GetServiceCapabilitiesResponse)
        { return this->GetServiceCapabilities(NULL, NULL, onvifPtz__GetServiceCapabilities, onvifPtz__GetServiceCapabilitiesResponse); }
        virtual int GetServiceCapabilities(const char *soap_endpoint, const char *soap_action, _onvifPtz__GetServiceCapabilities *onvifPtz__GetServiceCapabilities, _onvifPtz__GetServiceCapabilitiesResponse &onvifPtz__GetServiceCapabilitiesResponse);
        /// Web service operation 'GetConfigurations' (returns SOAP_OK or error code)
        virtual int GetConfigurations(_onvifPtz__GetConfigurations *onvifPtz__GetConfigurations, _onvifPtz__GetConfigurationsResponse &onvifPtz__GetConfigurationsResponse)
        { return this->GetConfigurations(NULL, NULL, onvifPtz__GetConfigurations, onvifPtz__GetConfigurationsResponse); }
        virtual int GetConfigurations(const char *soap_endpoint, const char *soap_action, _onvifPtz__GetConfigurations *onvifPtz__GetConfigurations, _onvifPtz__GetConfigurationsResponse &onvifPtz__GetConfigurationsResponse);
        /// Web service operation 'GetPresets' (returns SOAP_OK or error code)
        virtual int GetPresets(_onvifPtz__GetPresets *onvifPtz__GetPresets, _onvifPtz__GetPresetsResponse &onvifPtz__GetPresetsResponse)
        { return this->GetPresets(NULL, NULL, onvifPtz__GetPresets, onvifPtz__GetPresetsResponse); }
        virtual int GetPresets(const char *soap_endpoint, const char *soap_action, _onvifPtz__GetPresets *onvifPtz__GetPresets, _onvifPtz__GetPresetsResponse &onvifPtz__GetPresetsResponse);
        /// Web service operation 'SetPreset' (returns SOAP_OK or error code)
        virtual int SetPreset(_onvifPtz__SetPreset *onvifPtz__SetPreset, _onvifPtz__SetPresetResponse &onvifPtz__SetPresetResponse)
        { return this->SetPreset(NULL, NULL, onvifPtz__SetPreset, onvifPtz__SetPresetResponse); }
        virtual int SetPreset(const char *soap_endpoint, const char *soap_action, _onvifPtz__SetPreset *onvifPtz__SetPreset, _onvifPtz__SetPresetResponse &onvifPtz__SetPresetResponse);
        /// Web service operation 'RemovePreset' (returns SOAP_OK or error code)
        virtual int RemovePreset(_onvifPtz__RemovePreset *onvifPtz__RemovePreset, _onvifPtz__RemovePresetResponse &onvifPtz__RemovePresetResponse)
        { return this->RemovePreset(NULL, NULL, onvifPtz__RemovePreset, onvifPtz__RemovePresetResponse); }
        virtual int RemovePreset(const char *soap_endpoint, const char *soap_action, _onvifPtz__RemovePreset *onvifPtz__RemovePreset, _onvifPtz__RemovePresetResponse &onvifPtz__RemovePresetResponse);
        /// Web service operation 'GotoPreset' (returns SOAP_OK or error code)
        virtual int GotoPreset(_onvifPtz__GotoPreset *onvifPtz__GotoPreset, _onvifPtz__GotoPresetResponse &onvifPtz__GotoPresetResponse)
        { return this->GotoPreset(NULL, NULL, onvifPtz__GotoPreset, onvifPtz__GotoPresetResponse); }
        virtual int GotoPreset(const char *soap_endpoint, const char *soap_action, _onvifPtz__GotoPreset *onvifPtz__GotoPreset, _onvifPtz__GotoPresetResponse &onvifPtz__GotoPresetResponse);
        /// Web service operation 'GetStatus' (returns SOAP_OK or error code)
        virtual int GetStatus(_onvifPtz__GetStatus *onvifPtz__GetStatus, _onvifPtz__GetStatusResponse &onvifPtz__GetStatusResponse)
        { return this->GetStatus(NULL, NULL, onvifPtz__GetStatus, onvifPtz__GetStatusResponse); }
        virtual int GetStatus(const char *soap_endpoint, const char *soap_action, _onvifPtz__GetStatus *onvifPtz__GetStatus, _onvifPtz__GetStatusResponse &onvifPtz__GetStatusResponse);
        /// Web service operation 'GetConfiguration' (returns SOAP_OK or error code)
        virtual int GetConfiguration(_onvifPtz__GetConfiguration *onvifPtz__GetConfiguration, _onvifPtz__GetConfigurationResponse &onvifPtz__GetConfigurationResponse)
        { return this->GetConfiguration(NULL, NULL, onvifPtz__GetConfiguration, onvifPtz__GetConfigurationResponse); }
        virtual int GetConfiguration(const char *soap_endpoint, const char *soap_action, _onvifPtz__GetConfiguration *onvifPtz__GetConfiguration, _onvifPtz__GetConfigurationResponse &onvifPtz__GetConfigurationResponse);
        /// Web service operation 'GetNodes' (returns SOAP_OK or error code)
        virtual int GetNodes(_onvifPtz__GetNodes *onvifPtz__GetNodes, _onvifPtz__GetNodesResponse &onvifPtz__GetNodesResponse)
        { return this->GetNodes(NULL, NULL, onvifPtz__GetNodes, onvifPtz__GetNodesResponse); }
        virtual int GetNodes(const char *soap_endpoint, const char *soap_action, _onvifPtz__GetNodes *onvifPtz__GetNodes, _onvifPtz__GetNodesResponse &onvifPtz__GetNodesResponse);
        /// Web service operation 'GetNode' (returns SOAP_OK or error code)
        virtual int GetNode(_onvifPtz__GetNode *onvifPtz__GetNode, _onvifPtz__GetNodeResponse &onvifPtz__GetNodeResponse)
        { return this->GetNode(NULL, NULL, onvifPtz__GetNode, onvifPtz__GetNodeResponse); }
        virtual int GetNode(const char *soap_endpoint, const char *soap_action, _onvifPtz__GetNode *onvifPtz__GetNode, _onvifPtz__GetNodeResponse &onvifPtz__GetNodeResponse);
        /// Web service operation 'SetConfiguration' (returns SOAP_OK or error code)
        virtual int SetConfiguration(_onvifPtz__SetConfiguration *onvifPtz__SetConfiguration, _onvifPtz__SetConfigurationResponse &onvifPtz__SetConfigurationResponse)
        { return this->SetConfiguration(NULL, NULL, onvifPtz__SetConfiguration, onvifPtz__SetConfigurationResponse); }
        virtual int SetConfiguration(const char *soap_endpoint, const char *soap_action, _onvifPtz__SetConfiguration *onvifPtz__SetConfiguration, _onvifPtz__SetConfigurationResponse &onvifPtz__SetConfigurationResponse);
        /// Web service operation 'GetConfigurationOptions' (returns SOAP_OK or error code)
        virtual int GetConfigurationOptions(_onvifPtz__GetConfigurationOptions *onvifPtz__GetConfigurationOptions, _onvifPtz__GetConfigurationOptionsResponse &onvifPtz__GetConfigurationOptionsResponse)
        { return this->GetConfigurationOptions(NULL, NULL, onvifPtz__GetConfigurationOptions, onvifPtz__GetConfigurationOptionsResponse); }
        virtual int GetConfigurationOptions(const char *soap_endpoint, const char *soap_action, _onvifPtz__GetConfigurationOptions *onvifPtz__GetConfigurationOptions, _onvifPtz__GetConfigurationOptionsResponse &onvifPtz__GetConfigurationOptionsResponse);
        /// Web service operation 'GotoHomePosition' (returns SOAP_OK or error code)
        virtual int GotoHomePosition(_onvifPtz__GotoHomePosition *onvifPtz__GotoHomePosition, _onvifPtz__GotoHomePositionResponse &onvifPtz__GotoHomePositionResponse)
        { return this->GotoHomePosition(NULL, NULL, onvifPtz__GotoHomePosition, onvifPtz__GotoHomePositionResponse); }
        virtual int GotoHomePosition(const char *soap_endpoint, const char *soap_action, _onvifPtz__GotoHomePosition *onvifPtz__GotoHomePosition, _onvifPtz__GotoHomePositionResponse &onvifPtz__GotoHomePositionResponse);
        /// Web service operation 'SetHomePosition' (returns SOAP_OK or error code)
        virtual int SetHomePosition(_onvifPtz__SetHomePosition *onvifPtz__SetHomePosition, _onvifPtz__SetHomePositionResponse &onvifPtz__SetHomePositionResponse)
        { return this->SetHomePosition(NULL, NULL, onvifPtz__SetHomePosition, onvifPtz__SetHomePositionResponse); }
        virtual int SetHomePosition(const char *soap_endpoint, const char *soap_action, _onvifPtz__SetHomePosition *onvifPtz__SetHomePosition, _onvifPtz__SetHomePositionResponse &onvifPtz__SetHomePositionResponse);
        /// Web service operation 'ContinuousMove' (returns SOAP_OK or error code)
        virtual int ContinuousMove(_onvifPtz__ContinuousMove *onvifPtz__ContinuousMove, _onvifPtz__ContinuousMoveResponse &onvifPtz__ContinuousMoveResponse)
        { return this->ContinuousMove(NULL, NULL, onvifPtz__ContinuousMove, onvifPtz__ContinuousMoveResponse); }
        virtual int ContinuousMove(const char *soap_endpoint, const char *soap_action, _onvifPtz__ContinuousMove *onvifPtz__ContinuousMove, _onvifPtz__ContinuousMoveResponse &onvifPtz__ContinuousMoveResponse);
        /// Web service operation 'RelativeMove' (returns SOAP_OK or error code)
        virtual int RelativeMove(_onvifPtz__RelativeMove *onvifPtz__RelativeMove, _onvifPtz__RelativeMoveResponse &onvifPtz__RelativeMoveResponse)
        { return this->RelativeMove(NULL, NULL, onvifPtz__RelativeMove, onvifPtz__RelativeMoveResponse); }
        virtual int RelativeMove(const char *soap_endpoint, const char *soap_action, _onvifPtz__RelativeMove *onvifPtz__RelativeMove, _onvifPtz__RelativeMoveResponse &onvifPtz__RelativeMoveResponse);
        /// Web service operation 'SendAuxiliaryCommand' (returns SOAP_OK or error code)
        virtual int SendAuxiliaryCommand(_onvifPtz__SendAuxiliaryCommand *onvifPtz__SendAuxiliaryCommand, _onvifPtz__SendAuxiliaryCommandResponse &onvifPtz__SendAuxiliaryCommandResponse)
        { return this->SendAuxiliaryCommand(NULL, NULL, onvifPtz__SendAuxiliaryCommand, onvifPtz__SendAuxiliaryCommandResponse); }
        virtual int SendAuxiliaryCommand(const char *soap_endpoint, const char *soap_action, _onvifPtz__SendAuxiliaryCommand *onvifPtz__SendAuxiliaryCommand, _onvifPtz__SendAuxiliaryCommandResponse &onvifPtz__SendAuxiliaryCommandResponse);
        /// Web service operation 'AbsoluteMove' (returns SOAP_OK or error code)
        virtual int AbsoluteMove(_onvifPtz__AbsoluteMove *onvifPtz__AbsoluteMove, _onvifPtz__AbsoluteMoveResponse &onvifPtz__AbsoluteMoveResponse)
        { return this->AbsoluteMove(NULL, NULL, onvifPtz__AbsoluteMove, onvifPtz__AbsoluteMoveResponse); }
        virtual int AbsoluteMove(const char *soap_endpoint, const char *soap_action, _onvifPtz__AbsoluteMove *onvifPtz__AbsoluteMove, _onvifPtz__AbsoluteMoveResponse &onvifPtz__AbsoluteMoveResponse);
        /// Web service operation 'GeoMove' (returns SOAP_OK or error code)
        virtual int GeoMove(_onvifPtz__GeoMove *onvifPtz__GeoMove, _onvifPtz__GeoMoveResponse &onvifPtz__GeoMoveResponse)
        { return this->GeoMove(NULL, NULL, onvifPtz__GeoMove, onvifPtz__GeoMoveResponse); }
        virtual int GeoMove(const char *soap_endpoint, const char *soap_action, _onvifPtz__GeoMove *onvifPtz__GeoMove, _onvifPtz__GeoMoveResponse &onvifPtz__GeoMoveResponse);
        /// Web service operation 'Stop' (returns SOAP_OK or error code)
        virtual int Stop(_onvifPtz__Stop *onvifPtz__Stop, _onvifPtz__StopResponse &onvifPtz__StopResponse)
        { return this->Stop(NULL, NULL, onvifPtz__Stop, onvifPtz__StopResponse); }
        virtual int Stop(const char *soap_endpoint, const char *soap_action, _onvifPtz__Stop *onvifPtz__Stop, _onvifPtz__StopResponse &onvifPtz__StopResponse);
        /// Web service operation 'GetPresetTours' (returns SOAP_OK or error code)
        virtual int GetPresetTours(_onvifPtz__GetPresetTours *onvifPtz__GetPresetTours, _onvifPtz__GetPresetToursResponse &onvifPtz__GetPresetToursResponse)
        { return this->GetPresetTours(NULL, NULL, onvifPtz__GetPresetTours, onvifPtz__GetPresetToursResponse); }
        virtual int GetPresetTours(const char *soap_endpoint, const char *soap_action, _onvifPtz__GetPresetTours *onvifPtz__GetPresetTours, _onvifPtz__GetPresetToursResponse &onvifPtz__GetPresetToursResponse);
        /// Web service operation 'GetPresetTour' (returns SOAP_OK or error code)
        virtual int GetPresetTour(_onvifPtz__GetPresetTour *onvifPtz__GetPresetTour, _onvifPtz__GetPresetTourResponse &onvifPtz__GetPresetTourResponse)
        { return this->GetPresetTour(NULL, NULL, onvifPtz__GetPresetTour, onvifPtz__GetPresetTourResponse); }
        virtual int GetPresetTour(const char *soap_endpoint, const char *soap_action, _onvifPtz__GetPresetTour *onvifPtz__GetPresetTour, _onvifPtz__GetPresetTourResponse &onvifPtz__GetPresetTourResponse);
        /// Web service operation 'GetPresetTourOptions' (returns SOAP_OK or error code)
        virtual int GetPresetTourOptions(_onvifPtz__GetPresetTourOptions *onvifPtz__GetPresetTourOptions, _onvifPtz__GetPresetTourOptionsResponse &onvifPtz__GetPresetTourOptionsResponse)
        { return this->GetPresetTourOptions(NULL, NULL, onvifPtz__GetPresetTourOptions, onvifPtz__GetPresetTourOptionsResponse); }
        virtual int GetPresetTourOptions(const char *soap_endpoint, const char *soap_action, _onvifPtz__GetPresetTourOptions *onvifPtz__GetPresetTourOptions, _onvifPtz__GetPresetTourOptionsResponse &onvifPtz__GetPresetTourOptionsResponse);
        /// Web service operation 'CreatePresetTour' (returns SOAP_OK or error code)
        virtual int CreatePresetTour(_onvifPtz__CreatePresetTour *onvifPtz__CreatePresetTour, _onvifPtz__CreatePresetTourResponse &onvifPtz__CreatePresetTourResponse)
        { return this->CreatePresetTour(NULL, NULL, onvifPtz__CreatePresetTour, onvifPtz__CreatePresetTourResponse); }
        virtual int CreatePresetTour(const char *soap_endpoint, const char *soap_action, _onvifPtz__CreatePresetTour *onvifPtz__CreatePresetTour, _onvifPtz__CreatePresetTourResponse &onvifPtz__CreatePresetTourResponse);
        /// Web service operation 'ModifyPresetTour' (returns SOAP_OK or error code)
        virtual int ModifyPresetTour(_onvifPtz__ModifyPresetTour *onvifPtz__ModifyPresetTour, _onvifPtz__ModifyPresetTourResponse &onvifPtz__ModifyPresetTourResponse)
        { return this->ModifyPresetTour(NULL, NULL, onvifPtz__ModifyPresetTour, onvifPtz__ModifyPresetTourResponse); }
        virtual int ModifyPresetTour(const char *soap_endpoint, const char *soap_action, _onvifPtz__ModifyPresetTour *onvifPtz__ModifyPresetTour, _onvifPtz__ModifyPresetTourResponse &onvifPtz__ModifyPresetTourResponse);
        /// Web service operation 'OperatePresetTour' (returns SOAP_OK or error code)
        virtual int OperatePresetTour(_onvifPtz__OperatePresetTour *onvifPtz__OperatePresetTour, _onvifPtz__OperatePresetTourResponse &onvifPtz__OperatePresetTourResponse)
        { return this->OperatePresetTour(NULL, NULL, onvifPtz__OperatePresetTour, onvifPtz__OperatePresetTourResponse); }
        virtual int OperatePresetTour(const char *soap_endpoint, const char *soap_action, _onvifPtz__OperatePresetTour *onvifPtz__OperatePresetTour, _onvifPtz__OperatePresetTourResponse &onvifPtz__OperatePresetTourResponse);
        /// Web service operation 'RemovePresetTour' (returns SOAP_OK or error code)
        virtual int RemovePresetTour(_onvifPtz__RemovePresetTour *onvifPtz__RemovePresetTour, _onvifPtz__RemovePresetTourResponse &onvifPtz__RemovePresetTourResponse)
        { return this->RemovePresetTour(NULL, NULL, onvifPtz__RemovePresetTour, onvifPtz__RemovePresetTourResponse); }
        virtual int RemovePresetTour(const char *soap_endpoint, const char *soap_action, _onvifPtz__RemovePresetTour *onvifPtz__RemovePresetTour, _onvifPtz__RemovePresetTourResponse &onvifPtz__RemovePresetTourResponse);
        /// Web service operation 'GetCompatibleConfigurations' (returns SOAP_OK or error code)
        virtual int GetCompatibleConfigurations(_onvifPtz__GetCompatibleConfigurations *onvifPtz__GetCompatibleConfigurations, _onvifPtz__GetCompatibleConfigurationsResponse &onvifPtz__GetCompatibleConfigurationsResponse)
        { return this->GetCompatibleConfigurations(NULL, NULL, onvifPtz__GetCompatibleConfigurations, onvifPtz__GetCompatibleConfigurationsResponse); }
        virtual int GetCompatibleConfigurations(const char *soap_endpoint, const char *soap_action, _onvifPtz__GetCompatibleConfigurations *onvifPtz__GetCompatibleConfigurations, _onvifPtz__GetCompatibleConfigurationsResponse &onvifPtz__GetCompatibleConfigurationsResponse);
    };
#endif
