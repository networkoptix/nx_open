/* soapImagingBindingProxy.h
   Generated by gSOAP 2.8.8 from onvif.h

Copyright(C) 2000-2012, Robert van Engelen, Genivia Inc. All Rights Reserved.
The generated code is released under one of the following licenses:
1) GPL or 2) Genivia's license for commercial use.
This program is released under the GPL with the additional exemption that
compiling, linking, and/or using OpenSSL is allowed.
*/

#ifndef soapImagingBindingProxy_H
#define soapImagingBindingProxy_H
#include "soapH.h"

class SOAP_CMAC ImagingBindingProxy
{ public:
	struct soap *soap;
	bool own;
	/// Endpoint URL of service 'ImagingBindingProxy' (change as needed)
	const char *soap_endpoint;
	/// Constructor
	ImagingBindingProxy();
	/// Constructor to use/share an engine state
	ImagingBindingProxy(struct soap*);
	/// Constructor with endpoint URL
	ImagingBindingProxy(const char *url);
	/// Constructor with engine input+output mode control
	ImagingBindingProxy(soap_mode iomode);
	/// Constructor with URL and input+output mode control
	ImagingBindingProxy(const char *url, soap_mode iomode);
	/// Constructor with engine input and output mode control
	ImagingBindingProxy(soap_mode imode, soap_mode omode);
	/// Destructor frees deserialized data
	virtual	~ImagingBindingProxy();
	/// Initializer used by constructors
	virtual	void ImagingBindingProxy_init(soap_mode imode, soap_mode omode);
	/// Delete all deserialized data (uses soap_destroy and soap_end)
	virtual	void destroy();
	/// Delete all deserialized data and reset to default
	virtual	void reset();
	/// Disables and removes SOAP Header from message
	virtual	void soap_noheader();
	/// Put SOAP Header in message
	virtual	void soap_header(struct _wsse__Security *wsse__Security, char *wsa__MessageID, struct wsa__Relationship *wsa__RelatesTo, struct wsa__EndpointReferenceType *wsa__From, struct wsa__EndpointReferenceType *wsa__ReplyTo, struct wsa__EndpointReferenceType *wsa__FaultTo, char *wsa__To, char *wsa__Action, struct wsdd__AppSequenceType *wsdd__AppSequence);
	/// Get SOAP Header structure (NULL when absent)
	virtual	const SOAP_ENV__Header *soap_header();
	/// Get SOAP Fault structure (NULL when absent)
	virtual	const SOAP_ENV__Fault *soap_fault();
	/// Get SOAP Fault string (NULL when absent)
	virtual	const char *soap_fault_string();
	/// Get SOAP Fault detail as string (NULL when absent)
	virtual	const char *soap_fault_detail();
	/// Close connection (normally automatic, except for send_X ops)
	virtual	int soap_close_socket();
	/// Force close connection (can kill a thread blocked on IO)
	virtual	int soap_force_close_socket();
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

	/// Web service operation 'GetServiceCapabilities' (returns error code or SOAP_OK)
	virtual	int GetServiceCapabilities(_onvifImg__GetServiceCapabilities *onvifImg__GetServiceCapabilities, _onvifImg__GetServiceCapabilitiesResponse *onvifImg__GetServiceCapabilitiesResponse) { return GetServiceCapabilities(NULL, NULL, onvifImg__GetServiceCapabilities, onvifImg__GetServiceCapabilitiesResponse); }
	virtual	int GetServiceCapabilities(const char *endpoint, const char *soap_action, _onvifImg__GetServiceCapabilities *onvifImg__GetServiceCapabilities, _onvifImg__GetServiceCapabilitiesResponse *onvifImg__GetServiceCapabilitiesResponse);

	/// Web service operation 'GetImagingSettings' (returns error code or SOAP_OK)
	virtual	int GetImagingSettings(_onvifImg__GetImagingSettings *onvifImg__GetImagingSettings, _onvifImg__GetImagingSettingsResponse *onvifImg__GetImagingSettingsResponse) { return GetImagingSettings(NULL, NULL, onvifImg__GetImagingSettings, onvifImg__GetImagingSettingsResponse); }
	virtual	int GetImagingSettings(const char *endpoint, const char *soap_action, _onvifImg__GetImagingSettings *onvifImg__GetImagingSettings, _onvifImg__GetImagingSettingsResponse *onvifImg__GetImagingSettingsResponse);

	/// Web service operation 'SetImagingSettings' (returns error code or SOAP_OK)
	virtual	int SetImagingSettings(_onvifImg__SetImagingSettings *onvifImg__SetImagingSettings, _onvifImg__SetImagingSettingsResponse *onvifImg__SetImagingSettingsResponse) { return SetImagingSettings(NULL, NULL, onvifImg__SetImagingSettings, onvifImg__SetImagingSettingsResponse); }
	virtual	int SetImagingSettings(const char *endpoint, const char *soap_action, _onvifImg__SetImagingSettings *onvifImg__SetImagingSettings, _onvifImg__SetImagingSettingsResponse *onvifImg__SetImagingSettingsResponse);

	/// Web service operation 'GetOptions' (returns error code or SOAP_OK)
	virtual	int GetOptions(_onvifImg__GetOptions *onvifImg__GetOptions, _onvifImg__GetOptionsResponse *onvifImg__GetOptionsResponse) { return GetOptions(NULL, NULL, onvifImg__GetOptions, onvifImg__GetOptionsResponse); }
	virtual	int GetOptions(const char *endpoint, const char *soap_action, _onvifImg__GetOptions *onvifImg__GetOptions, _onvifImg__GetOptionsResponse *onvifImg__GetOptionsResponse);

	/// Web service operation 'Move' (returns error code or SOAP_OK)
	virtual	int Move(_onvifImg__Move *onvifImg__Move, _onvifImg__MoveResponse *onvifImg__MoveResponse) { return Move(NULL, NULL, onvifImg__Move, onvifImg__MoveResponse); }
	virtual	int Move(const char *endpoint, const char *soap_action, _onvifImg__Move *onvifImg__Move, _onvifImg__MoveResponse *onvifImg__MoveResponse);

	/// Web service operation 'Stop' (returns error code or SOAP_OK)
	virtual	int Stop(_onvifImg__Stop *onvifImg__Stop, _onvifImg__StopResponse *onvifImg__StopResponse) { return Stop(NULL, NULL, onvifImg__Stop, onvifImg__StopResponse); }
	virtual	int Stop(const char *endpoint, const char *soap_action, _onvifImg__Stop *onvifImg__Stop, _onvifImg__StopResponse *onvifImg__StopResponse);

	/// Web service operation 'GetStatus' (returns error code or SOAP_OK)
	virtual	int GetStatus(_onvifImg__GetStatus *onvifImg__GetStatus, _onvifImg__GetStatusResponse *onvifImg__GetStatusResponse) { return GetStatus(NULL, NULL, onvifImg__GetStatus, onvifImg__GetStatusResponse); }
	virtual	int GetStatus(const char *endpoint, const char *soap_action, _onvifImg__GetStatus *onvifImg__GetStatus, _onvifImg__GetStatusResponse *onvifImg__GetStatusResponse);

	/// Web service operation 'GetMoveOptions' (returns error code or SOAP_OK)
	virtual	int GetMoveOptions(_onvifImg__GetMoveOptions *onvifImg__GetMoveOptions, _onvifImg__GetMoveOptionsResponse *onvifImg__GetMoveOptionsResponse) { return GetMoveOptions(NULL, NULL, onvifImg__GetMoveOptions, onvifImg__GetMoveOptionsResponse); }
	virtual	int GetMoveOptions(const char *endpoint, const char *soap_action, _onvifImg__GetMoveOptions *onvifImg__GetMoveOptions, _onvifImg__GetMoveOptionsResponse *onvifImg__GetMoveOptionsResponse);
};
#endif
