/* soapPullPointBindingProxy.cpp
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

#include "soapPullPointBindingProxy.h"

PullPointBindingProxy::PullPointBindingProxy()
{	this->soap = soap_new();
	this->soap_own = true;
	PullPointBindingProxy_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
}

PullPointBindingProxy::PullPointBindingProxy(const PullPointBindingProxy& rhs)
{	this->soap = rhs.soap;
	this->soap_own = false;
	this->soap_endpoint = rhs.soap_endpoint;
}

PullPointBindingProxy::PullPointBindingProxy(struct soap *_soap)
{	this->soap = _soap;
	this->soap_own = false;
	PullPointBindingProxy_init(_soap->imode, _soap->omode);
}

PullPointBindingProxy::PullPointBindingProxy(const char *endpoint)
{	this->soap = soap_new();
	this->soap_own = true;
	PullPointBindingProxy_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
	soap_endpoint = endpoint;
}

PullPointBindingProxy::PullPointBindingProxy(soap_mode iomode)
{	this->soap = soap_new();
	this->soap_own = true;
	PullPointBindingProxy_init(iomode, iomode);
}

PullPointBindingProxy::PullPointBindingProxy(const char *endpoint, soap_mode iomode)
{	this->soap = soap_new();
	this->soap_own = true;
	PullPointBindingProxy_init(iomode, iomode);
	soap_endpoint = endpoint;
}

PullPointBindingProxy::PullPointBindingProxy(soap_mode imode, soap_mode omode)
{	this->soap = soap_new();
	this->soap_own = true;
	PullPointBindingProxy_init(imode, omode);
}

PullPointBindingProxy::~PullPointBindingProxy()
{	if (this->soap_own)
		soap_free(this->soap);
}

void PullPointBindingProxy::PullPointBindingProxy_init(soap_mode imode, soap_mode omode)
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

PullPointBindingProxy *PullPointBindingProxy::copy()
{	PullPointBindingProxy *dup = SOAP_NEW_UNMANAGED(PullPointBindingProxy);
	if (dup)
	{	soap_done(dup->soap);
		soap_copy_context(dup->soap, this->soap);
	}
	return dup;
}

PullPointBindingProxy& PullPointBindingProxy::operator=(const PullPointBindingProxy& rhs)
{	if (this->soap != rhs.soap)
	{	if (this->soap_own)
			soap_free(this->soap);
		this->soap = rhs.soap;
		this->soap_own = false;
		this->soap_endpoint = rhs.soap_endpoint;
	}
	return *this;
}

void PullPointBindingProxy::destroy()
{	soap_destroy(this->soap);
	soap_end(this->soap);
}

void PullPointBindingProxy::reset()
{	this->destroy();
	soap_done(this->soap);
	soap_initialize(this->soap);
	PullPointBindingProxy_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
}

void PullPointBindingProxy::soap_noheader()
{	this->soap->header = NULL;
}

void PullPointBindingProxy::soap_header(char *wsa5__MessageID, struct wsa5__RelatesToType *wsa5__RelatesTo, struct wsa5__EndpointReferenceType *wsa5__From, struct wsa5__EndpointReferenceType *wsa5__ReplyTo, struct wsa5__EndpointReferenceType *wsa5__FaultTo, char *wsa5__To, char *wsa5__Action, struct chan__ChannelInstanceType *chan__ChannelInstance, struct wsdd__AppSequenceType *wsdd__AppSequence, struct _wsse__Security *wsse__Security, char *subscriptionID)
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

::SOAP_ENV__Header *PullPointBindingProxy::soap_header()
{	return this->soap->header;
}

::SOAP_ENV__Fault *PullPointBindingProxy::soap_fault()
{	return this->soap->fault;
}

const char *PullPointBindingProxy::soap_fault_string()
{	return *soap_faultstring(this->soap);
}

const char *PullPointBindingProxy::soap_fault_detail()
{	return *soap_faultdetail(this->soap);
}

int PullPointBindingProxy::soap_close_socket()
{	return soap_closesock(this->soap);
}

int PullPointBindingProxy::soap_force_close_socket()
{	return soap_force_closesock(this->soap);
}

void PullPointBindingProxy::soap_print_fault(FILE *fd)
{	::soap_print_fault(this->soap, fd);
}

#ifndef WITH_LEAN
#ifndef WITH_COMPAT
void PullPointBindingProxy::soap_stream_fault(std::ostream& os)
{	::soap_stream_fault(this->soap, os);
}
#endif

char *PullPointBindingProxy::soap_sprint_fault(char *buf, size_t len)
{	return ::soap_sprint_fault(this->soap, buf, len);
}
#endif

int PullPointBindingProxy::GetMessages(const char *endpoint, const char *soap_action, _oasisWsnB2__GetMessages *oasisWsnB2__GetMessages, _oasisWsnB2__GetMessagesResponse &oasisWsnB2__GetMessagesResponse)
{
	struct __onvifEvents_ppb__GetMessages soap_tmp___onvifEvents_ppb__GetMessages;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://docs.oasis-open.org/wsn/bw-2/PullPoint/GetMessagesRequest";
	soap_tmp___onvifEvents_ppb__GetMessages.oasisWsnB2__GetMessages = oasisWsnB2__GetMessages;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___onvifEvents_ppb__GetMessages(soap, &soap_tmp___onvifEvents_ppb__GetMessages);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___onvifEvents_ppb__GetMessages(soap, &soap_tmp___onvifEvents_ppb__GetMessages, "-onvifEvents-ppb:GetMessages", "")
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
	 || soap_put___onvifEvents_ppb__GetMessages(soap, &soap_tmp___onvifEvents_ppb__GetMessages, "-onvifEvents-ppb:GetMessages", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_oasisWsnB2__GetMessagesResponse*>(&oasisWsnB2__GetMessagesResponse)) // NULL ref?
		return soap_closesock(soap);
	oasisWsnB2__GetMessagesResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	oasisWsnB2__GetMessagesResponse.soap_get(soap, "oasisWsnB2:GetMessagesResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int PullPointBindingProxy::DestroyPullPoint(const char *endpoint, const char *soap_action, _oasisWsnB2__DestroyPullPoint *oasisWsnB2__DestroyPullPoint, _oasisWsnB2__DestroyPullPointResponse &oasisWsnB2__DestroyPullPointResponse)
{
	struct __onvifEvents_ppb__DestroyPullPoint soap_tmp___onvifEvents_ppb__DestroyPullPoint;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://docs.oasis-open.org/wsn/bw-2/PullPoint/DestroyPullPointRequest";
	soap_tmp___onvifEvents_ppb__DestroyPullPoint.oasisWsnB2__DestroyPullPoint = oasisWsnB2__DestroyPullPoint;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___onvifEvents_ppb__DestroyPullPoint(soap, &soap_tmp___onvifEvents_ppb__DestroyPullPoint);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___onvifEvents_ppb__DestroyPullPoint(soap, &soap_tmp___onvifEvents_ppb__DestroyPullPoint, "-onvifEvents-ppb:DestroyPullPoint", "")
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
	 || soap_put___onvifEvents_ppb__DestroyPullPoint(soap, &soap_tmp___onvifEvents_ppb__DestroyPullPoint, "-onvifEvents-ppb:DestroyPullPoint", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_oasisWsnB2__DestroyPullPointResponse*>(&oasisWsnB2__DestroyPullPointResponse)) // NULL ref?
		return soap_closesock(soap);
	oasisWsnB2__DestroyPullPointResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	oasisWsnB2__DestroyPullPointResponse.soap_get(soap, "oasisWsnB2:DestroyPullPointResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int PullPointBindingProxy::send_Notify(const char *endpoint, const char *soap_action, _oasisWsnB2__Notify *oasisWsnB2__Notify)
{
	struct __onvifEvents_ppb__Notify soap_tmp___onvifEvents_ppb__Notify;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://docs.oasis-open.org/wsn/bw-2/PullPoint/Notify";
	soap_tmp___onvifEvents_ppb__Notify.oasisWsnB2__Notify = oasisWsnB2__Notify;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___onvifEvents_ppb__Notify(soap, &soap_tmp___onvifEvents_ppb__Notify);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___onvifEvents_ppb__Notify(soap, &soap_tmp___onvifEvents_ppb__Notify, "-onvifEvents-ppb:Notify", "")
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
	 || soap_put___onvifEvents_ppb__Notify(soap, &soap_tmp___onvifEvents_ppb__Notify, "-onvifEvents-ppb:Notify", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	return SOAP_OK;
}

int PullPointBindingProxy::recv_Notify(struct __onvifEvents_ppb__Notify& tmp)
{

	struct __onvifEvents_ppb__Notify *_param_1 = &tmp;
	soap_default___onvifEvents_ppb__Notify(soap, _param_1);
	soap_begin(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	soap_get___onvifEvents_ppb__Notify(soap, _param_1, "-onvifEvents-ppb:Notify", NULL);
	if (soap->error == SOAP_TAG_MISMATCH && soap->level == 2)
		soap->error = SOAP_OK;
	if (soap->error
	 || soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}
/* End of client proxy code */
