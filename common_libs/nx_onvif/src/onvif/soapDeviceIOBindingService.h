/* soapDeviceIOBindingService.h
   Generated by gSOAP 2.8.8 from onvif.h

Copyright(C) 2000-2012, Robert van Engelen, Genivia Inc. All Rights Reserved.
The generated code is released under one of the following licenses:
1) GPL or 2) Genivia's license for commercial use.
This program is released under the GPL with the additional exemption that
compiling, linking, and/or using OpenSSL is allowed.
*/

#ifndef soapDeviceIOBindingService_H
#define soapDeviceIOBindingService_H
#include "soapH.h"
class SOAP_CMAC DeviceIOBindingService
{ public:
	struct soap *soap;
	bool own;
	/// Constructor
	DeviceIOBindingService();
	/// Constructor to use/share an engine state
	DeviceIOBindingService(struct soap*);
	/// Constructor with engine input+output mode control
	DeviceIOBindingService(soap_mode iomode);
	/// Constructor with engine input and output mode control
	DeviceIOBindingService(soap_mode imode, soap_mode omode);
	/// Destructor, also frees all deserialized data
	virtual ~DeviceIOBindingService();
	/// Delete all deserialized data (uses soap_destroy and soap_end)
	virtual	void destroy();
	/// Delete all deserialized data and reset to defaults
	virtual	void reset();
	/// Initializer used by constructor
	virtual	void DeviceIOBindingService_init(soap_mode imode, soap_mode omode);
	/// Create a copy
	virtual	DeviceIOBindingService *copy() SOAP_PURE_VIRTUAL;
	/// Close connection (normally automatic)
	virtual	int soap_close_socket();
	/// Force close connection (can kill a thread blocked on IO)
	virtual	int soap_force_close_socket();
	/// Return sender-related fault to sender
	virtual	int soap_senderfault(const char *string, const char *detailXML);
	/// Return sender-related fault with SOAP 1.2 subcode to sender
	virtual	int soap_senderfault(const char *subcodeQName, const char *string, const char *detailXML);
	/// Return receiver-related fault to sender
	virtual	int soap_receiverfault(const char *string, const char *detailXML);
	/// Return receiver-related fault with SOAP 1.2 subcode to sender
	virtual	int soap_receiverfault(const char *subcodeQName, const char *string, const char *detailXML);
	/// Print fault
	virtual	void soap_print_fault(FILE*);
#ifndef WITH_LEAN
	/// Print fault to stream
#ifndef WITH_COMPAT
	virtual	void soap_stream_fault(std::ostream&);
#endif
	/// Put fault into buffer
	virtual	char *soap_sprint_fault(char *buf, size_t len);
#endif
	/// Disables and removes SOAP Header from message
	virtual	void soap_noheader();
	/// Put SOAP Header in message
	virtual	void soap_header(struct _wsse__Security *wsse__Security, char *wsa__MessageID, struct wsa__Relationship *wsa__RelatesTo, struct wsa__EndpointReferenceType *wsa__From, struct wsa__EndpointReferenceType *wsa__ReplyTo, struct wsa__EndpointReferenceType *wsa__FaultTo, char *wsa__To, char *wsa__Action, struct wsdd__AppSequenceType *wsdd__AppSequence);
	/// Get SOAP Header structure (NULL when absent)
	virtual	const SOAP_ENV__Header *soap_header();
	/// Run simple single-thread iterative service on port until a connection error occurs (returns error code or SOAP_OK), use this->bind_flag = SO_REUSEADDR to rebind for a rerun
	virtual	int run(int port);
	/// Bind service to port (returns master socket or SOAP_INVALID_SOCKET)
	virtual	SOAP_SOCKET bind(const char *host, int port, int backlog);
	/// Accept next request (returns socket or SOAP_INVALID_SOCKET)
	virtual	SOAP_SOCKET accept();
	/// Serve this request (returns error code or SOAP_OK)
	virtual	int serve();
	/// Used by serve() to dispatch a request (returns error code or SOAP_OK)
	virtual	int dispatch();

	///
	/// Service operations (you should define these):
	/// Note: compile with -DWITH_PURE_VIRTUAL for pure virtual methods
	///

	/// Web service operation 'GetServiceCapabilities' (returns error code or SOAP_OK)
	virtual	int GetServiceCapabilities(_onvifDeviceIO__GetServiceCapabilities *onvifDeviceIO__GetServiceCapabilities, _onvifDeviceIO__GetServiceCapabilitiesResponse *onvifDeviceIO__GetServiceCapabilitiesResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'GetRelayOutputOptions' (returns error code or SOAP_OK)
	virtual	int GetRelayOutputOptions(_onvifDeviceIO__GetRelayOutputOptions *onvifDeviceIO__GetRelayOutputOptions, _onvifDeviceIO__GetRelayOutputOptionsResponse *onvifDeviceIO__GetRelayOutputOptionsResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'GetAudioSources' (returns error code or SOAP_OK)
	virtual	int GetAudioSources(_onvifMedia__GetAudioSources *onvifMedia__GetAudioSources, _onvifMedia__GetAudioSourcesResponse *onvifMedia__GetAudioSourcesResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'GetAudioOutputs' (returns error code or SOAP_OK)
	virtual	int GetAudioOutputs(_onvifMedia__GetAudioOutputs *onvifMedia__GetAudioOutputs, _onvifMedia__GetAudioOutputsResponse *onvifMedia__GetAudioOutputsResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'GetVideoSources' (returns error code or SOAP_OK)
	virtual	int GetVideoSources(_onvifMedia__GetVideoSources *onvifMedia__GetVideoSources, _onvifMedia__GetVideoSourcesResponse *onvifMedia__GetVideoSourcesResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'GetVideoOutputs' (returns error code or SOAP_OK)
	virtual	int GetVideoOutputs(_onvifDeviceIO__GetVideoOutputs *onvifDeviceIO__GetVideoOutputs, _onvifDeviceIO__GetVideoOutputsResponse *onvifDeviceIO__GetVideoOutputsResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'GetVideoSourceConfiguration' (returns error code or SOAP_OK)
	virtual	int GetVideoSourceConfiguration(_onvifDeviceIO__GetVideoSourceConfiguration *onvifDeviceIO__GetVideoSourceConfiguration, _onvifDeviceIO__GetVideoSourceConfigurationResponse *onvifDeviceIO__GetVideoSourceConfigurationResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'GetVideoOutputConfiguration' (returns error code or SOAP_OK)
	virtual	int GetVideoOutputConfiguration(_onvifDeviceIO__GetVideoOutputConfiguration *onvifDeviceIO__GetVideoOutputConfiguration, _onvifDeviceIO__GetVideoOutputConfigurationResponse *onvifDeviceIO__GetVideoOutputConfigurationResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'GetAudioSourceConfiguration' (returns error code or SOAP_OK)
	virtual	int GetAudioSourceConfiguration(_onvifDeviceIO__GetAudioSourceConfiguration *onvifDeviceIO__GetAudioSourceConfiguration, _onvifDeviceIO__GetAudioSourceConfigurationResponse *onvifDeviceIO__GetAudioSourceConfigurationResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'GetAudioOutputConfiguration' (returns error code or SOAP_OK)
	virtual	int GetAudioOutputConfiguration(_onvifDeviceIO__GetAudioOutputConfiguration *onvifDeviceIO__GetAudioOutputConfiguration, _onvifDeviceIO__GetAudioOutputConfigurationResponse *onvifDeviceIO__GetAudioOutputConfigurationResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'SetVideoSourceConfiguration' (returns error code or SOAP_OK)
	virtual	int SetVideoSourceConfiguration(_onvifDeviceIO__SetVideoSourceConfiguration *onvifDeviceIO__SetVideoSourceConfiguration, _onvifDeviceIO__SetVideoSourceConfigurationResponse *onvifDeviceIO__SetVideoSourceConfigurationResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'SetVideoOutputConfiguration' (returns error code or SOAP_OK)
	virtual	int SetVideoOutputConfiguration(_onvifDeviceIO__SetVideoOutputConfiguration *onvifDeviceIO__SetVideoOutputConfiguration, _onvifDeviceIO__SetVideoOutputConfigurationResponse *onvifDeviceIO__SetVideoOutputConfigurationResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'SetAudioSourceConfiguration' (returns error code or SOAP_OK)
	virtual	int SetAudioSourceConfiguration(_onvifDeviceIO__SetAudioSourceConfiguration *onvifDeviceIO__SetAudioSourceConfiguration, _onvifDeviceIO__SetAudioSourceConfigurationResponse *onvifDeviceIO__SetAudioSourceConfigurationResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'SetAudioOutputConfiguration' (returns error code or SOAP_OK)
	virtual	int SetAudioOutputConfiguration(_onvifDeviceIO__SetAudioOutputConfiguration *onvifDeviceIO__SetAudioOutputConfiguration, _onvifDeviceIO__SetAudioOutputConfigurationResponse *onvifDeviceIO__SetAudioOutputConfigurationResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'GetVideoSourceConfigurationOptions' (returns error code or SOAP_OK)
	virtual	int GetVideoSourceConfigurationOptions(_onvifDeviceIO__GetVideoSourceConfigurationOptions *onvifDeviceIO__GetVideoSourceConfigurationOptions, _onvifDeviceIO__GetVideoSourceConfigurationOptionsResponse *onvifDeviceIO__GetVideoSourceConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'GetVideoOutputConfigurationOptions' (returns error code or SOAP_OK)
	virtual	int GetVideoOutputConfigurationOptions(_onvifDeviceIO__GetVideoOutputConfigurationOptions *onvifDeviceIO__GetVideoOutputConfigurationOptions, _onvifDeviceIO__GetVideoOutputConfigurationOptionsResponse *onvifDeviceIO__GetVideoOutputConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'GetAudioSourceConfigurationOptions' (returns error code or SOAP_OK)
	virtual	int GetAudioSourceConfigurationOptions(_onvifDeviceIO__GetAudioSourceConfigurationOptions *onvifDeviceIO__GetAudioSourceConfigurationOptions, _onvifDeviceIO__GetAudioSourceConfigurationOptionsResponse *onvifDeviceIO__GetAudioSourceConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'GetAudioOutputConfigurationOptions' (returns error code or SOAP_OK)
	virtual	int GetAudioOutputConfigurationOptions(_onvifDeviceIO__GetAudioOutputConfigurationOptions *onvifDeviceIO__GetAudioOutputConfigurationOptions, _onvifDeviceIO__GetAudioOutputConfigurationOptionsResponse *onvifDeviceIO__GetAudioOutputConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'GetRelayOutputs' (returns error code or SOAP_OK)
	virtual	int GetRelayOutputs(_onvifDevice__GetRelayOutputs *onvifDevice__GetRelayOutputs, _onvifDevice__GetRelayOutputsResponse *onvifDevice__GetRelayOutputsResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'SetRelayOutputSettings' (returns error code or SOAP_OK)
	virtual	int SetRelayOutputSettings(_onvifDeviceIO__SetRelayOutputSettings *onvifDeviceIO__SetRelayOutputSettings, _onvifDeviceIO__SetRelayOutputSettingsResponse *onvifDeviceIO__SetRelayOutputSettingsResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'SetRelayOutputState' (returns error code or SOAP_OK)
	virtual	int SetRelayOutputState(_onvifDevice__SetRelayOutputState *onvifDevice__SetRelayOutputState, _onvifDevice__SetRelayOutputStateResponse *onvifDevice__SetRelayOutputStateResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'GetDigitalInputs' (returns error code or SOAP_OK)
	virtual	int GetDigitalInputs(_onvifDeviceIO__GetDigitalInputs *onvifDeviceIO__GetDigitalInputs, _onvifDeviceIO__GetDigitalInputsResponse *onvifDeviceIO__GetDigitalInputsResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'GetSerialPorts' (returns error code or SOAP_OK)
	virtual	int GetSerialPorts(_onvifDeviceIO__GetSerialPorts *onvifDeviceIO__GetSerialPorts, _onvifDeviceIO__GetSerialPortsResponse *onvifDeviceIO__GetSerialPortsResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'GetSerialPortConfiguration' (returns error code or SOAP_OK)
	virtual	int GetSerialPortConfiguration(_onvifDeviceIO__GetSerialPortConfiguration *onvifDeviceIO__GetSerialPortConfiguration, _onvifDeviceIO__GetSerialPortConfigurationResponse *onvifDeviceIO__GetSerialPortConfigurationResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'SetSerialPortConfiguration' (returns error code or SOAP_OK)
	virtual	int SetSerialPortConfiguration(_onvifDeviceIO__SetSerialPortConfiguration *onvifDeviceIO__SetSerialPortConfiguration, _onvifDeviceIO__SetSerialPortConfigurationResponse *onvifDeviceIO__SetSerialPortConfigurationResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'GetSerialPortConfigurationOptions' (returns error code or SOAP_OK)
	virtual	int GetSerialPortConfigurationOptions(_onvifDeviceIO__GetSerialPortConfigurationOptions *onvifDeviceIO__GetSerialPortConfigurationOptions, _onvifDeviceIO__GetSerialPortConfigurationOptionsResponse *onvifDeviceIO__GetSerialPortConfigurationOptionsResponse) SOAP_PURE_VIRTUAL;

	/// Web service operation 'SendReceiveSerialCommand' (returns error code or SOAP_OK)
	virtual	int SendReceiveSerialCommand(_onvifDeviceIO__SendReceiveSerialCommand *onvifDeviceIO__SendReceiveSerialCommand, _onvifDeviceIO__SendReceiveSerialCommandResponse *onvifDeviceIO__SendReceiveSerialCommandResponse) SOAP_PURE_VIRTUAL;
};
#endif
