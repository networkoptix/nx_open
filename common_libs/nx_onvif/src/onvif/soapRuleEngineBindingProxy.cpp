/* soapRuleEngineBindingProxy.cpp
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

#include "soapRuleEngineBindingProxy.h"

RuleEngineBindingProxy::RuleEngineBindingProxy()
{	this->soap = soap_new();
	this->soap_own = true;
	RuleEngineBindingProxy_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
}

RuleEngineBindingProxy::RuleEngineBindingProxy(const RuleEngineBindingProxy& rhs)
{	this->soap = rhs.soap;
	this->soap_own = false;
	this->soap_endpoint = rhs.soap_endpoint;
}

RuleEngineBindingProxy::RuleEngineBindingProxy(struct soap *_soap)
{	this->soap = _soap;
	this->soap_own = false;
	RuleEngineBindingProxy_init(_soap->imode, _soap->omode);
}

RuleEngineBindingProxy::RuleEngineBindingProxy(const char *endpoint)
{	this->soap = soap_new();
	this->soap_own = true;
	RuleEngineBindingProxy_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
	soap_endpoint = endpoint;
}

RuleEngineBindingProxy::RuleEngineBindingProxy(soap_mode iomode)
{	this->soap = soap_new();
	this->soap_own = true;
	RuleEngineBindingProxy_init(iomode, iomode);
}

RuleEngineBindingProxy::RuleEngineBindingProxy(const char *endpoint, soap_mode iomode)
{	this->soap = soap_new();
	this->soap_own = true;
	RuleEngineBindingProxy_init(iomode, iomode);
	soap_endpoint = endpoint;
}

RuleEngineBindingProxy::RuleEngineBindingProxy(soap_mode imode, soap_mode omode)
{	this->soap = soap_new();
	this->soap_own = true;
	RuleEngineBindingProxy_init(imode, omode);
}

RuleEngineBindingProxy::~RuleEngineBindingProxy()
{	if (this->soap_own)
		soap_free(this->soap);
}

void RuleEngineBindingProxy::RuleEngineBindingProxy_init(soap_mode imode, soap_mode omode)
{	soap_imode(this->soap, imode);
	soap_omode(this->soap, omode);
	soap_endpoint = NULL;
	static const struct Namespace namespaces[] = {
        {"SOAP-ENV", "http://www.w3.org/2003/05/soap-envelope", "http://schemas.xmlsoap.org/soap/envelope/", NULL},
        {"SOAP-ENC", "http://www.w3.org/2003/05/soap-encoding", "http://schemas.xmlsoap.org/soap/encoding/", NULL},
        {"xsi", "http://www.w3.org/2001/XMLSchema-instance", "http://www.w3.org/*/XMLSchema-instance", NULL},
        {"xsd", "http://www.w3.org/2001/XMLSchema", "http://www.w3.org/*/XMLSchema", NULL},
        {"chan", "http://schemas.microsoft.com/ws/2005/02/duplex", NULL, NULL},
        {"wsa5", "http://www.w3.org/2005/08/addressing", "http://schemas.xmlsoap.org/ws/2004/08/addressing", NULL},
        {"wsdd", "http://schemas.xmlsoap.org/ws/2005/04/discovery", NULL, NULL},
        {"c14n", "http://www.w3.org/2001/10/xml-exc-c14n#", NULL, NULL},
        {"ds", "http://www.w3.org/2000/09/xmldsig#", NULL, NULL},
        {"saml1", "urn:oasis:names:tc:SAML:1.0:assertion", NULL, NULL},
        {"saml2", "urn:oasis:names:tc:SAML:2.0:assertion", NULL, NULL},
        {"wsu", "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd", NULL, NULL},
        {"xenc", "http://www.w3.org/2001/04/xmlenc#", NULL, NULL},
        {"wsc", "http://docs.oasis-open.org/ws-sx/ws-secureconversation/200512", NULL, NULL},
        {"wsse", "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd", "http://docs.oasis-open.org/wss/oasis-wss-wssecurity-secext-1.1.xsd", NULL},
        {"onvifPacs", "http://www.onvif.org/ver10/pacs", NULL, NULL},
        {"xmime", "http://tempuri.org/xmime.xsd", NULL, NULL},
        {"xop", "http://www.w3.org/2004/08/xop/include", NULL, NULL},
        {"onvifXsd", "http://www.onvif.org/ver10/schema", NULL, NULL},
        {"oasisWsrf", "http://docs.oasis-open.org/wsrf/bf-2", NULL, NULL},
        {"oasisWsnT1", "http://docs.oasis-open.org/wsn/t-1", NULL, NULL},
        {"oasisWsrfR2", "http://docs.oasis-open.org/wsrf/r-2", NULL, NULL},
        {"onvifAccessControl", "http://www.onvif.org/ver10/accesscontrol/wsdl", NULL, NULL},
        {"onvifAccessRules", "http://www.onvif.org/ver10/accessrules/wsdl", NULL, NULL},
        {"onvifActionEngine", "http://www.onvif.org/ver10/actionengine/wsdl", NULL, NULL},
        {"onvifAdvancedSecurity-assb", "http://www.onvif.org/ver10/advancedsecurity/wsdl/AdvancedSecurityServiceBinding", NULL, NULL},
        {"onvifAdvancedSecurity-db", "http://www.onvif.org/ver10/advancedsecurity/wsdl/Dot1XBinding", NULL, NULL},
        {"onvifAdvancedSecurity-kb", "http://www.onvif.org/ver10/advancedsecurity/wsdl/KeystoreBinding", NULL, NULL},
        {"onvifAdvancedSecurity-tsb", "http://www.onvif.org/ver10/advancedsecurity/wsdl/TLSServerBinding", NULL, NULL},
        {"onvifAdvancedSecurity", "http://www.onvif.org/ver10/advancedsecurity/wsdl", NULL, NULL},
        {"onvifAnalytics-aeb", "http://www.onvif.org/ver20/analytics/wsdl/AnalyticsEngineBinding", NULL, NULL},
        {"onvifAnalytics-reb", "http://www.onvif.org/ver20/analytics/wsdl/RuleEngineBinding", NULL, NULL},
        {"onvifAnalytics", "http://www.onvif.org/ver20/analytics/wsdl", NULL, NULL},
        {"onvifAnalyticsDevice", "http://www.onvif.org/ver10/analyticsdevice/wsdl", NULL, NULL},
        {"onvifCredential", "http://www.onvif.org/ver10/credential/wsdl", NULL, NULL},
        {"onvifDevice", "http://www.onvif.org/ver10/device/wsdl", NULL, NULL},
        {"onvifDeviceIO", "http://www.onvif.org/ver10/deviceIO/wsdl", NULL, NULL},
        {"onvifDisplay", "http://www.onvif.org/ver10/display/wsdl", NULL, NULL},
        {"onvifDoorControl", "http://www.onvif.org/ver10/doorcontrol/wsdl", NULL, NULL},
        {"onvifEvents-cpb", "http://www.onvif.org/ver10/events/wsdl/CreatePullPointBinding", NULL, NULL},
        {"onvifEvents-eb", "http://www.onvif.org/ver10/events/wsdl/EventBinding", NULL, NULL},
        {"onvifEvents-ncb", "http://www.onvif.org/ver10/events/wsdl/NotificationConsumerBinding", NULL, NULL},
        {"onvifEvents-npb", "http://www.onvif.org/ver10/events/wsdl/NotificationProducerBinding", NULL, NULL},
        {"onvifEvents-ppb", "http://www.onvif.org/ver10/events/wsdl/PullPointBinding", NULL, NULL},
        {"onvifEvents", "http://www.onvif.org/ver10/events/wsdl", NULL, NULL},
        {"onvifEvents-pps", "http://www.onvif.org/ver10/events/wsdl/PullPointSubscriptionBinding", NULL, NULL},
        {"onvifEvents-psmb", "http://www.onvif.org/ver10/events/wsdl/PausableSubscriptionManagerBinding", NULL, NULL},
        {"oasisWsnB2", "http://docs.oasis-open.org/wsn/b-2", NULL, NULL},
        {"onvifEvents-smb", "http://www.onvif.org/ver10/events/wsdl/SubscriptionManagerBinding", NULL, NULL},
        {"onvifImg", "http://www.onvif.org/ver20/imaging/wsdl", NULL, NULL},
        {"onvifMedia", "http://www.onvif.org/ver10/media/wsdl", NULL, NULL},
        {"onvifMedia2", "http://www.onvif.org/ver20/media/wsdl", NULL, NULL},
        {"onvifNetwork-dlb", "http://www.onvif.org/ver10/network/wsdl/DiscoveryLookupBinding", NULL, NULL},
        {"onvifNetwork-rdb", "http://www.onvif.org/ver10/network/wsdl/RemoteDiscoveryBinding", NULL, NULL},
        {"onvifNetwork", "http://www.onvif.org/ver10/network/wsdl", NULL, NULL},
        {"onvifProvisioning", "http://www.onvif.org/ver10/provisioning/wsdl", NULL, NULL},
        {"onvifPtz", "http://www.onvif.org/ver20/ptz/wsdl", NULL, NULL},
        {"onvifReceiver", "http://www.onvif.org/ver10/receiver/wsdl", NULL, NULL},
        {"onvifRecording", "http://www.onvif.org/ver10/recording/wsdl", NULL, NULL},
        {"onvifReplay", "http://www.onvif.org/ver10/replay/wsdl", NULL, NULL},
        {"onvifScedule", "http://www.onvif.org/ver10/schedule/wsdl", NULL, NULL},
        {"onvifSearch", "http://www.onvif.org/ver10/search/wsdl", NULL, NULL},
        {"onvifThermal", "http://www.onvif.org/ver10/thermal/wsdl", NULL, NULL},
        {NULL, NULL, NULL, NULL}
    };
	soap_set_namespaces(this->soap, namespaces);
}

RuleEngineBindingProxy *RuleEngineBindingProxy::copy()
{	RuleEngineBindingProxy *dup = SOAP_NEW_UNMANAGED(RuleEngineBindingProxy);
	if (dup)
	{	soap_done(dup->soap);
		soap_copy_context(dup->soap, this->soap);
	}
	return dup;
}

RuleEngineBindingProxy& RuleEngineBindingProxy::operator=(const RuleEngineBindingProxy& rhs)
{	if (this->soap != rhs.soap)
	{	if (this->soap_own)
			soap_free(this->soap);
		this->soap = rhs.soap;
		this->soap_own = false;
		this->soap_endpoint = rhs.soap_endpoint;
	}
	return *this;
}

void RuleEngineBindingProxy::destroy()
{	soap_destroy(this->soap);
	soap_end(this->soap);
}

void RuleEngineBindingProxy::reset()
{	this->destroy();
	soap_done(this->soap);
	soap_initialize(this->soap);
	RuleEngineBindingProxy_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
}

void RuleEngineBindingProxy::soap_noheader()
{	this->soap->header = NULL;
}

void RuleEngineBindingProxy::soap_header(char *wsa5__MessageID, struct wsa5__RelatesToType *wsa5__RelatesTo, struct wsa5__EndpointReferenceType *wsa5__From, struct wsa5__EndpointReferenceType *wsa5__ReplyTo, struct wsa5__EndpointReferenceType *wsa5__FaultTo, char *wsa5__To, char *wsa5__Action, struct chan__ChannelInstanceType *chan__ChannelInstance, struct wsdd__AppSequenceType *wsdd__AppSequence, struct _wsse__Security *wsse__Security, char *subscriptionID)
{	::soap_header(this->soap);
	this->soap->header->wsa5__MessageID = wsa5__MessageID;
	this->soap->header->wsa5__RelatesTo = wsa5__RelatesTo;
	this->soap->header->wsa5__From = wsa5__From;
	this->soap->header->wsa5__ReplyTo = wsa5__ReplyTo;
	this->soap->header->wsa5__FaultTo = wsa5__FaultTo;
	this->soap->header->wsa5__To = wsa5__To;
	this->soap->header->wsa5__Action = wsa5__Action;
	this->soap->header->chan__ChannelInstance = chan__ChannelInstance;
	this->soap->header->wsdd__AppSequence = wsdd__AppSequence;
	this->soap->header->wsse__Security = wsse__Security;
	this->soap->header->subscriptionID = subscriptionID;
}

::SOAP_ENV__Header *RuleEngineBindingProxy::soap_header()
{	return this->soap->header;
}

::SOAP_ENV__Fault *RuleEngineBindingProxy::soap_fault()
{	return this->soap->fault;
}

const char *RuleEngineBindingProxy::soap_fault_string()
{	return *soap_faultstring(this->soap);
}

const char *RuleEngineBindingProxy::soap_fault_detail()
{	return *soap_faultdetail(this->soap);
}

int RuleEngineBindingProxy::soap_close_socket()
{	return soap_closesock(this->soap);
}

int RuleEngineBindingProxy::soap_force_close_socket()
{	return soap_force_closesock(this->soap);
}

void RuleEngineBindingProxy::soap_print_fault(FILE *fd)
{	::soap_print_fault(this->soap, fd);
}

#ifndef WITH_LEAN
#ifndef WITH_COMPAT
void RuleEngineBindingProxy::soap_stream_fault(std::ostream& os)
{	::soap_stream_fault(this->soap, os);
}
#endif

char *RuleEngineBindingProxy::soap_sprint_fault(char *buf, size_t len)
{	return ::soap_sprint_fault(this->soap, buf, len);
}
#endif

int RuleEngineBindingProxy::GetSupportedRules(const char *endpoint, const char *soap_action, _onvifAnalytics__GetSupportedRules *onvifAnalytics__GetSupportedRules, _onvifAnalytics__GetSupportedRulesResponse &onvifAnalytics__GetSupportedRulesResponse)
{
	struct __onvifAnalytics_reb__GetSupportedRules soap_tmp___onvifAnalytics_reb__GetSupportedRules;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.onvif.org/ver20/analytics/wsdl/GetSupportedRules";
	soap_tmp___onvifAnalytics_reb__GetSupportedRules.onvifAnalytics__GetSupportedRules = onvifAnalytics__GetSupportedRules;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___onvifAnalytics_reb__GetSupportedRules(soap, &soap_tmp___onvifAnalytics_reb__GetSupportedRules);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___onvifAnalytics_reb__GetSupportedRules(soap, &soap_tmp___onvifAnalytics_reb__GetSupportedRules, "-onvifAnalytics-reb:GetSupportedRules", "")
		 || soap_body_end_out(soap)
		 || soap_envelope_end_out(soap))
			 return soap->error;
	}
	if (soap_end_count(soap))
		return soap->error;
	if (soap_connect(soap, soap_endpoint, soap_action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put___onvifAnalytics_reb__GetSupportedRules(soap, &soap_tmp___onvifAnalytics_reb__GetSupportedRules, "-onvifAnalytics-reb:GetSupportedRules", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_onvifAnalytics__GetSupportedRulesResponse*>(&onvifAnalytics__GetSupportedRulesResponse)) // NULL ref?
		return soap_closesock(soap);
	onvifAnalytics__GetSupportedRulesResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	onvifAnalytics__GetSupportedRulesResponse.soap_get(soap, "onvifAnalytics:GetSupportedRulesResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int RuleEngineBindingProxy::CreateRules(const char *endpoint, const char *soap_action, _onvifAnalytics__CreateRules *onvifAnalytics__CreateRules, _onvifAnalytics__CreateRulesResponse &onvifAnalytics__CreateRulesResponse)
{
	struct __onvifAnalytics_reb__CreateRules soap_tmp___onvifAnalytics_reb__CreateRules;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.onvif.org/ver20/analytics/wsdl/CreateRules";
	soap_tmp___onvifAnalytics_reb__CreateRules.onvifAnalytics__CreateRules = onvifAnalytics__CreateRules;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___onvifAnalytics_reb__CreateRules(soap, &soap_tmp___onvifAnalytics_reb__CreateRules);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___onvifAnalytics_reb__CreateRules(soap, &soap_tmp___onvifAnalytics_reb__CreateRules, "-onvifAnalytics-reb:CreateRules", "")
		 || soap_body_end_out(soap)
		 || soap_envelope_end_out(soap))
			 return soap->error;
	}
	if (soap_end_count(soap))
		return soap->error;
	if (soap_connect(soap, soap_endpoint, soap_action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put___onvifAnalytics_reb__CreateRules(soap, &soap_tmp___onvifAnalytics_reb__CreateRules, "-onvifAnalytics-reb:CreateRules", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_onvifAnalytics__CreateRulesResponse*>(&onvifAnalytics__CreateRulesResponse)) // NULL ref?
		return soap_closesock(soap);
	onvifAnalytics__CreateRulesResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	onvifAnalytics__CreateRulesResponse.soap_get(soap, "onvifAnalytics:CreateRulesResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int RuleEngineBindingProxy::DeleteRules(const char *endpoint, const char *soap_action, _onvifAnalytics__DeleteRules *onvifAnalytics__DeleteRules, _onvifAnalytics__DeleteRulesResponse &onvifAnalytics__DeleteRulesResponse)
{
	struct __onvifAnalytics_reb__DeleteRules soap_tmp___onvifAnalytics_reb__DeleteRules;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.onvif.org/ver20/analytics/wsdl/DeleteRules";
	soap_tmp___onvifAnalytics_reb__DeleteRules.onvifAnalytics__DeleteRules = onvifAnalytics__DeleteRules;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___onvifAnalytics_reb__DeleteRules(soap, &soap_tmp___onvifAnalytics_reb__DeleteRules);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___onvifAnalytics_reb__DeleteRules(soap, &soap_tmp___onvifAnalytics_reb__DeleteRules, "-onvifAnalytics-reb:DeleteRules", "")
		 || soap_body_end_out(soap)
		 || soap_envelope_end_out(soap))
			 return soap->error;
	}
	if (soap_end_count(soap))
		return soap->error;
	if (soap_connect(soap, soap_endpoint, soap_action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put___onvifAnalytics_reb__DeleteRules(soap, &soap_tmp___onvifAnalytics_reb__DeleteRules, "-onvifAnalytics-reb:DeleteRules", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_onvifAnalytics__DeleteRulesResponse*>(&onvifAnalytics__DeleteRulesResponse)) // NULL ref?
		return soap_closesock(soap);
	onvifAnalytics__DeleteRulesResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	onvifAnalytics__DeleteRulesResponse.soap_get(soap, "onvifAnalytics:DeleteRulesResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int RuleEngineBindingProxy::GetRules(const char *endpoint, const char *soap_action, _onvifAnalytics__GetRules *onvifAnalytics__GetRules, _onvifAnalytics__GetRulesResponse &onvifAnalytics__GetRulesResponse)
{
	struct __onvifAnalytics_reb__GetRules soap_tmp___onvifAnalytics_reb__GetRules;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.onvif.org/ver20/analytics/wsdl/GetRules";
	soap_tmp___onvifAnalytics_reb__GetRules.onvifAnalytics__GetRules = onvifAnalytics__GetRules;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___onvifAnalytics_reb__GetRules(soap, &soap_tmp___onvifAnalytics_reb__GetRules);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___onvifAnalytics_reb__GetRules(soap, &soap_tmp___onvifAnalytics_reb__GetRules, "-onvifAnalytics-reb:GetRules", "")
		 || soap_body_end_out(soap)
		 || soap_envelope_end_out(soap))
			 return soap->error;
	}
	if (soap_end_count(soap))
		return soap->error;
	if (soap_connect(soap, soap_endpoint, soap_action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put___onvifAnalytics_reb__GetRules(soap, &soap_tmp___onvifAnalytics_reb__GetRules, "-onvifAnalytics-reb:GetRules", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_onvifAnalytics__GetRulesResponse*>(&onvifAnalytics__GetRulesResponse)) // NULL ref?
		return soap_closesock(soap);
	onvifAnalytics__GetRulesResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	onvifAnalytics__GetRulesResponse.soap_get(soap, "onvifAnalytics:GetRulesResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int RuleEngineBindingProxy::GetRuleOptions(const char *endpoint, const char *soap_action, _onvifAnalytics__GetRuleOptions *onvifAnalytics__GetRuleOptions, _onvifAnalytics__GetRuleOptionsResponse &onvifAnalytics__GetRuleOptionsResponse)
{
	struct __onvifAnalytics_reb__GetRuleOptions soap_tmp___onvifAnalytics_reb__GetRuleOptions;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.onvif.org/ver20/analytics/wsdl/GetRuleOptions";
	soap_tmp___onvifAnalytics_reb__GetRuleOptions.onvifAnalytics__GetRuleOptions = onvifAnalytics__GetRuleOptions;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___onvifAnalytics_reb__GetRuleOptions(soap, &soap_tmp___onvifAnalytics_reb__GetRuleOptions);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___onvifAnalytics_reb__GetRuleOptions(soap, &soap_tmp___onvifAnalytics_reb__GetRuleOptions, "-onvifAnalytics-reb:GetRuleOptions", "")
		 || soap_body_end_out(soap)
		 || soap_envelope_end_out(soap))
			 return soap->error;
	}
	if (soap_end_count(soap))
		return soap->error;
	if (soap_connect(soap, soap_endpoint, soap_action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put___onvifAnalytics_reb__GetRuleOptions(soap, &soap_tmp___onvifAnalytics_reb__GetRuleOptions, "-onvifAnalytics-reb:GetRuleOptions", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_onvifAnalytics__GetRuleOptionsResponse*>(&onvifAnalytics__GetRuleOptionsResponse)) // NULL ref?
		return soap_closesock(soap);
	onvifAnalytics__GetRuleOptionsResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	onvifAnalytics__GetRuleOptionsResponse.soap_get(soap, "onvifAnalytics:GetRuleOptionsResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int RuleEngineBindingProxy::ModifyRules(const char *endpoint, const char *soap_action, _onvifAnalytics__ModifyRules *onvifAnalytics__ModifyRules, _onvifAnalytics__ModifyRulesResponse &onvifAnalytics__ModifyRulesResponse)
{
	struct __onvifAnalytics_reb__ModifyRules soap_tmp___onvifAnalytics_reb__ModifyRules;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.onvif.org/ver20/analytics/wsdl/ModifyRules";
	soap_tmp___onvifAnalytics_reb__ModifyRules.onvifAnalytics__ModifyRules = onvifAnalytics__ModifyRules;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___onvifAnalytics_reb__ModifyRules(soap, &soap_tmp___onvifAnalytics_reb__ModifyRules);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___onvifAnalytics_reb__ModifyRules(soap, &soap_tmp___onvifAnalytics_reb__ModifyRules, "-onvifAnalytics-reb:ModifyRules", "")
		 || soap_body_end_out(soap)
		 || soap_envelope_end_out(soap))
			 return soap->error;
	}
	if (soap_end_count(soap))
		return soap->error;
	if (soap_connect(soap, soap_endpoint, soap_action)
	 || soap_envelope_begin_out(soap)
	 || soap_putheader(soap)
	 || soap_body_begin_out(soap)
	 || soap_put___onvifAnalytics_reb__ModifyRules(soap, &soap_tmp___onvifAnalytics_reb__ModifyRules, "-onvifAnalytics-reb:ModifyRules", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_onvifAnalytics__ModifyRulesResponse*>(&onvifAnalytics__ModifyRulesResponse)) // NULL ref?
		return soap_closesock(soap);
	onvifAnalytics__ModifyRulesResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	onvifAnalytics__ModifyRulesResponse.soap_get(soap, "onvifAnalytics:ModifyRulesResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}
/* End of client proxy code */
