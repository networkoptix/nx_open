/* soapNotificationProducerBindingProxy.cpp
   Generated by gSOAP 2.8.8 from onvif.h

Copyright(C) 2000-2012, Robert van Engelen, Genivia Inc. All Rights Reserved.
The generated code is released under one of the following licenses:
1) GPL or 2) Genivia's license for commercial use.
This program is released under the GPL with the additional exemption that
compiling, linking, and/or using OpenSSL is allowed.
*/

#include "soapNotificationProducerBindingProxy.h"

NotificationProducerBindingProxy::NotificationProducerBindingProxy()
{	this->soap = soap_new();
	this->own = true;
	NotificationProducerBindingProxy_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
}

NotificationProducerBindingProxy::NotificationProducerBindingProxy(struct soap *_soap)
{	this->soap = _soap;
	this->own = false;
	NotificationProducerBindingProxy_init(_soap->imode, _soap->omode);
}

NotificationProducerBindingProxy::NotificationProducerBindingProxy(const char *url)
{	this->soap = soap_new();
	this->own = true;
	NotificationProducerBindingProxy_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
	soap_endpoint = url;
}

NotificationProducerBindingProxy::NotificationProducerBindingProxy(soap_mode iomode)
{	this->soap = soap_new();
	this->own = true;
	NotificationProducerBindingProxy_init(iomode, iomode);
}

NotificationProducerBindingProxy::NotificationProducerBindingProxy(const char *url, soap_mode iomode)
{	this->soap = soap_new();
	this->own = true;
	NotificationProducerBindingProxy_init(iomode, iomode);
	soap_endpoint = url;
}

NotificationProducerBindingProxy::NotificationProducerBindingProxy(soap_mode imode, soap_mode omode)
{	this->soap = soap_new();
	this->own = true;
	NotificationProducerBindingProxy_init(imode, omode);
}

NotificationProducerBindingProxy::~NotificationProducerBindingProxy()
{	if (this->own)
		soap_free(this->soap);
}

void NotificationProducerBindingProxy::NotificationProducerBindingProxy_init(soap_mode imode, soap_mode omode)
{	soap_imode(this->soap, imode);
	soap_omode(this->soap, omode);
	soap_endpoint = NULL;
	static const struct Namespace namespaces[] =
{
	{"SOAP-ENV", "http://www.w3.org/2003/05/soap-envelope", "http://schemas.xmlsoap.org/soap/envelope/", NULL},
	{"SOAP-ENC", "http://www.w3.org/2003/05/soap-encoding", "http://schemas.xmlsoap.org/soap/encoding/", NULL},
	{"xsi", "http://www.w3.org/2001/XMLSchema-instance", "http://www.w3.org/*/XMLSchema-instance", NULL},
	{"xsd", "http://www.w3.org/2001/XMLSchema", "http://www.w3.org/*/XMLSchema", NULL},
	{"c14n", "http://www.w3.org/2001/10/xml-exc-c14n#", NULL, NULL},
	{"wsu", "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd", NULL, NULL},
	{"xenc", "http://www.w3.org/2001/04/xmlenc#", NULL, NULL},
	{"ds", "http://www.w3.org/2000/09/xmldsig#", NULL, NULL},
	{"wsse", "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd", "http://docs.oasis-open.org/wss/oasis-wss-wssecurity-secext-1.1.xsd", NULL},
	{"wsa", "http://schemas.xmlsoap.org/ws/2004/08/addressing", NULL, NULL},
	{"wsdd", "http://schemas.xmlsoap.org/ws/2005/04/discovery", NULL, NULL},
	{"ns1", "http://www.w3.org/2005/08/addressing", NULL, NULL},
	{"oasisWsrf", "http://docs.oasis-open.org/wsrf/bf-2", NULL, NULL},
	{"xmime", "http://tempuri.org/xmime.xsd", NULL, NULL},
	{"ns5", "http://www.w3.org/2004/08/xop/include", NULL, NULL},
	{"onvifXsd", "http://www.onvif.org/ver10/schema", NULL, NULL},
	{"oasisWsnT1", "http://docs.oasis-open.org/wsn/t-1", NULL, NULL},
	{"oasisWsrfR2", "http://docs.oasis-open.org/wsrf/r-2", NULL, NULL},
	{"ns2", "http://www.onvif.org/ver10/events/wsdl/PausableSubscriptionManagerBinding", NULL, NULL},
	{"onvifDevice", "http://www.onvif.org/ver10/device/wsdl", NULL, NULL},
	{"onvifDeviceIO", "http://www.onvif.org/ver10/deviceIO/wsdl", NULL, NULL},
	{"onvifImg", "http://www.onvif.org/ver20/imaging/wsdl", NULL, NULL},
	{"onvifMedia", "http://www.onvif.org/ver10/media/wsdl", NULL, NULL},
	{"onvifPtz", "http://www.onvif.org/ver20/ptz/wsdl", NULL, NULL},
	{"tev-cpb", "http://www.onvif.org/ver10/events/wsdl/CreatePullPointBinding", NULL, NULL},
	{"tev-eb", "http://www.onvif.org/ver10/events/wsdl/EventBinding", NULL, NULL},
	{"tev-ncb", "http://www.onvif.org/ver10/events/wsdl/NotificationConsumerBinding", NULL, NULL},
	{"tev-npb", "http://www.onvif.org/ver10/events/wsdl/NotificationProducerBinding", NULL, NULL},
	{"oasisWsnB2", "http://docs.oasis-open.org/wsn/b-2", NULL, NULL},
	{"tev-ppb", "http://www.onvif.org/ver10/events/wsdl/PullPointBinding", NULL, NULL},
	{"tev-pps", "http://www.onvif.org/ver10/events/wsdl/PullPointSubscriptionBinding", NULL, NULL},
	{"onvifEvents", "http://www.onvif.org/ver10/events/wsdl", NULL, NULL},
	{"tev-smb", "http://www.onvif.org/ver10/events/wsdl/SubscriptionManagerBinding", NULL, NULL},
	{NULL, NULL, NULL, NULL}
};
	soap_set_namespaces(this->soap, namespaces);
}

void NotificationProducerBindingProxy::destroy()
{	soap_destroy(this->soap);
	soap_end(this->soap);
}

void NotificationProducerBindingProxy::reset()
{	destroy();
	soap_done(this->soap);
	soap_init(this->soap);
	NotificationProducerBindingProxy_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
}

void NotificationProducerBindingProxy::soap_noheader()
{	this->soap->header = NULL;
}

void NotificationProducerBindingProxy::soap_header(struct _wsse__Security *wsse__Security, char *wsa__MessageID, struct wsa__Relationship *wsa__RelatesTo, struct wsa__EndpointReferenceType *wsa__From, struct wsa__EndpointReferenceType *wsa__ReplyTo, struct wsa__EndpointReferenceType *wsa__FaultTo, char *wsa__To, char *wsa__Action, struct wsdd__AppSequenceType *wsdd__AppSequence)
{	::soap_header(this->soap);
	this->soap->header->wsse__Security = wsse__Security;
	this->soap->header->wsa__MessageID = wsa__MessageID;
	this->soap->header->wsa__RelatesTo = wsa__RelatesTo;
	this->soap->header->wsa__From = wsa__From;
	this->soap->header->wsa__ReplyTo = wsa__ReplyTo;
	this->soap->header->wsa__FaultTo = wsa__FaultTo;
	this->soap->header->wsa__To = wsa__To;
	this->soap->header->wsa__Action = wsa__Action;
	this->soap->header->wsdd__AppSequence = wsdd__AppSequence;
}

const SOAP_ENV__Header *NotificationProducerBindingProxy::soap_header()
{	return this->soap->header;
}

const SOAP_ENV__Fault *NotificationProducerBindingProxy::soap_fault()
{	return this->soap->fault;
}

const char *NotificationProducerBindingProxy::soap_fault_string()
{	return *soap_faultstring(this->soap);
}

const char *NotificationProducerBindingProxy::soap_fault_detail()
{	return *soap_faultdetail(this->soap);
}

int NotificationProducerBindingProxy::soap_close_socket()
{	return soap_closesock(this->soap);
}

int NotificationProducerBindingProxy::soap_force_close_socket()
{	return soap_force_closesock(this->soap);
}

void NotificationProducerBindingProxy::soap_print_fault(FILE *fd)
{	::soap_print_fault(this->soap, fd);
}

#ifndef WITH_LEAN
#ifndef WITH_COMPAT
void NotificationProducerBindingProxy::soap_stream_fault(std::ostream& os)
{	::soap_stream_fault(this->soap, os);
}
#endif

char *NotificationProducerBindingProxy::soap_sprint_fault(char *buf, size_t len)
{	return ::soap_sprint_fault(this->soap, buf, len);
}
#endif

int NotificationProducerBindingProxy::Subscribe(const char *endpoint, const char *soap_action, _oasisWsnB2__Subscribe *oasisWsnB2__Subscribe, _oasisWsnB2__SubscribeResponse *oasisWsnB2__SubscribeResponse)
{	struct soap *soap = this->soap;
	struct __tev_npb__Subscribe soap_tmp___tev_npb__Subscribe;
	if (endpoint)
		soap_endpoint = endpoint;
	if (!soap_action)
		soap_action = "http://docs.oasis-open.org/wsn/bw-2/NotificationProducer/SubscribeRequest";
	soap->encodingStyle = NULL;
	soap_tmp___tev_npb__Subscribe.oasisWsnB2__Subscribe = oasisWsnB2__Subscribe;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize___tev_npb__Subscribe(soap, &soap_tmp___tev_npb__Subscribe);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___tev_npb__Subscribe(soap, &soap_tmp___tev_npb__Subscribe, "-tev-npb:Subscribe", NULL)
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
	 || soap_put___tev_npb__Subscribe(soap, &soap_tmp___tev_npb__Subscribe, "-tev-npb:Subscribe", NULL)
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!oasisWsnB2__SubscribeResponse)
		return soap_closesock(soap);
	oasisWsnB2__SubscribeResponse->soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	oasisWsnB2__SubscribeResponse->soap_get(soap, "oasisWsnB2:SubscribeResponse", "");
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int NotificationProducerBindingProxy::GetCurrentMessage(const char *endpoint, const char *soap_action, _oasisWsnB2__GetCurrentMessage *oasisWsnB2__GetCurrentMessage, _oasisWsnB2__GetCurrentMessageResponse *oasisWsnB2__GetCurrentMessageResponse)
{	struct soap *soap = this->soap;
	struct __tev_npb__GetCurrentMessage soap_tmp___tev_npb__GetCurrentMessage;
	if (endpoint)
		soap_endpoint = endpoint;
	if (!soap_action)
		soap_action = "http://docs.oasis-open.org/wsn/bw-2/NotificationProducer/GetCurrentMessageRequest";
	soap->encodingStyle = NULL;
	soap_tmp___tev_npb__GetCurrentMessage.oasisWsnB2__GetCurrentMessage = oasisWsnB2__GetCurrentMessage;
	soap_begin(soap);
	soap_serializeheader(soap);
	soap_serialize___tev_npb__GetCurrentMessage(soap, &soap_tmp___tev_npb__GetCurrentMessage);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___tev_npb__GetCurrentMessage(soap, &soap_tmp___tev_npb__GetCurrentMessage, "-tev-npb:GetCurrentMessage", NULL)
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
	 || soap_put___tev_npb__GetCurrentMessage(soap, &soap_tmp___tev_npb__GetCurrentMessage, "-tev-npb:GetCurrentMessage", NULL)
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!oasisWsnB2__GetCurrentMessageResponse)
		return soap_closesock(soap);
	oasisWsnB2__GetCurrentMessageResponse->soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	oasisWsnB2__GetCurrentMessageResponse->soap_get(soap, "oasisWsnB2:GetCurrentMessageResponse", "");
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}
/* End of client proxy code */
