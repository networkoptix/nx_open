/* soapwsddService.cpp
   Generated by gSOAP 2.8.8 from onvif.h

Copyright(C) 2000-2012, Robert van Engelen, Genivia Inc. All Rights Reserved.
The generated code is released under one of the following licenses:
1) GPL or 2) Genivia's license for commercial use.
This program is released under the GPL with the additional exemption that
compiling, linking, and/or using OpenSSL is allowed.
*/

#include "soapwsddService.h"

wsddService::wsddService()
{	this->soap = soap_new();
	this->own = true;
	wsddService_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
}

wsddService::wsddService(struct soap *_soap)
{	this->soap = _soap;
	this->own = false;
	wsddService_init(_soap->imode, _soap->omode);
}

wsddService::wsddService(soap_mode iomode)
{	this->soap = soap_new();
	this->own = true;
	wsddService_init(iomode, iomode);
}

wsddService::wsddService(soap_mode imode, soap_mode omode)
{	this->soap = soap_new();
	this->own = true;
	wsddService_init(imode, omode);
}

wsddService::~wsddService()
{	if (this->own)
		soap_free(this->soap);
}

void wsddService::wsddService_init(soap_mode imode, soap_mode omode)
{	soap_imode(this->soap, imode);
	soap_omode(this->soap, omode);
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
};

void wsddService::destroy()
{	soap_destroy(this->soap);
	soap_end(this->soap);
}

void wsddService::reset()
{	destroy();
	soap_done(this->soap);
	soap_init(this->soap);
	wsddService_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
}

#ifndef WITH_PURE_VIRTUAL
wsddService *wsddService::copy()
{	wsddService *dup = SOAP_NEW_COPY(wsddService);
	if (dup)
		soap_copy_context(dup->soap, this->soap);
	return dup;
}
#endif

int wsddService::soap_close_socket()
{	return soap_closesock(this->soap);
}

int wsddService::soap_force_close_socket()
{	return soap_force_closesock(this->soap);
}

int wsddService::soap_senderfault(const char *string, const char *detailXML)
{	return ::soap_sender_fault(this->soap, string, detailXML);
}

int wsddService::soap_senderfault(const char *subcodeQName, const char *string, const char *detailXML)
{	return ::soap_sender_fault_subcode(this->soap, subcodeQName, string, detailXML);
}

int wsddService::soap_receiverfault(const char *string, const char *detailXML)
{	return ::soap_receiver_fault(this->soap, string, detailXML);
}

int wsddService::soap_receiverfault(const char *subcodeQName, const char *string, const char *detailXML)
{	return ::soap_receiver_fault_subcode(this->soap, subcodeQName, string, detailXML);
}

void wsddService::soap_print_fault(FILE *fd)
{	::soap_print_fault(this->soap, fd);
}

#ifndef WITH_LEAN
#ifndef WITH_COMPAT
void wsddService::soap_stream_fault(std::ostream& os)
{	::soap_stream_fault(this->soap, os);
}
#endif

char *wsddService::soap_sprint_fault(char *buf, size_t len)
{	return ::soap_sprint_fault(this->soap, buf, len);
}
#endif

void wsddService::soap_noheader()
{	this->soap->header = NULL;
}

void wsddService::soap_header(struct _wsse__Security *wsse__Security, char *wsa__MessageID, struct wsa__Relationship *wsa__RelatesTo, struct wsa__EndpointReferenceType *wsa__From, struct wsa__EndpointReferenceType *wsa__ReplyTo, struct wsa__EndpointReferenceType *wsa__FaultTo, char *wsa__To, char *wsa__Action, struct wsdd__AppSequenceType *wsdd__AppSequence)
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

const SOAP_ENV__Header *wsddService::soap_header()
{	return this->soap->header;
}

int wsddService::run(int port)
{	if (soap_valid_socket(bind(NULL, port, 100)))
	{	for (;;)
		{	if (!soap_valid_socket(accept()) || serve())
				return this->soap->error;
			soap_destroy(this->soap);
			soap_end(this->soap);
		}
	}
	else
		return this->soap->error;
	return SOAP_OK;
}

SOAP_SOCKET wsddService::bind(const char *host, int port, int backlog)
{	return soap_bind(this->soap, host, port, backlog);
}

SOAP_SOCKET wsddService::accept()
{	return soap_accept(this->soap);
}

int wsddService::serve()
{
#ifndef WITH_FASTCGI
	unsigned int k = this->soap->max_keep_alive;
#endif
	do
	{

#ifndef WITH_FASTCGI
		if (this->soap->max_keep_alive > 0 && !--k)
			this->soap->keep_alive = 0;
#endif

		if (soap_begin_serve(this->soap))
		{	if (this->soap->error >= SOAP_STOP)
				continue;
			return this->soap->error;
		}
		if (dispatch() || (this->soap->fserveloop && this->soap->fserveloop(this->soap)))
		{
#ifdef WITH_FASTCGI
			soap_send_fault(this->soap);
#else
			return soap_send_fault(this->soap);
#endif
		}

#ifdef WITH_FASTCGI
		soap_destroy(this->soap);
		soap_end(this->soap);
	} while (1);
#else
	} while (this->soap->keep_alive);
#endif
	return SOAP_OK;
}

static int serve___wsdd__Hello(wsddService*);
static int serve___wsdd__Bye(wsddService*);
static int serve___wsdd__Probe(wsddService*);
static int serve___wsdd__ProbeMatches(wsddService*);
static int serve___wsdd__Resolve(wsddService*);
static int serve___wsdd__ResolveMatches(wsddService*);

int wsddService::dispatch()
{	wsddService_init(this->soap->imode, this->soap->omode);
	soap_peek_element(this->soap);
	if (!soap_match_tag(this->soap, this->soap->tag, "wsdd:Hello"))
		return serve___wsdd__Hello(this);
	if (!soap_match_tag(this->soap, this->soap->tag, "wsdd:Bye"))
		return serve___wsdd__Bye(this);
	if (!soap_match_tag(this->soap, this->soap->tag, "wsdd:Probe"))
		return serve___wsdd__Probe(this);
	if (!soap_match_tag(this->soap, this->soap->tag, "wsdd:ProbeMatches"))
		return serve___wsdd__ProbeMatches(this);
	if (!soap_match_tag(this->soap, this->soap->tag, "wsdd:Resolve"))
		return serve___wsdd__Resolve(this);
	if (!soap_match_tag(this->soap, this->soap->tag, "wsdd:ResolveMatches"))
		return serve___wsdd__ResolveMatches(this);
	return this->soap->error = SOAP_NO_METHOD;
}

static int serve___wsdd__Hello(wsddService *service)
{	struct soap *soap = service->soap;
	struct __wsdd__Hello soap_tmp___wsdd__Hello;
	soap_default___wsdd__Hello(soap, &soap_tmp___wsdd__Hello);
	soap->encodingStyle = NULL;
	if (!soap_get___wsdd__Hello(soap, &soap_tmp___wsdd__Hello, "-wsdd:Hello", NULL))
		return soap->error;
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap->error;
	soap->error = service->Hello(soap_tmp___wsdd__Hello.wsdd__Hello);
	if (soap->error)
		return soap->error;
	return soap_closesock(soap);
}

static int serve___wsdd__Bye(wsddService *service)
{	struct soap *soap = service->soap;
	struct __wsdd__Bye soap_tmp___wsdd__Bye;
	soap_default___wsdd__Bye(soap, &soap_tmp___wsdd__Bye);
	soap->encodingStyle = NULL;
	if (!soap_get___wsdd__Bye(soap, &soap_tmp___wsdd__Bye, "-wsdd:Bye", NULL))
		return soap->error;
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap->error;
	soap->error = service->Bye(soap_tmp___wsdd__Bye.wsdd__Bye);
	if (soap->error)
		return soap->error;
	return soap_closesock(soap);
}

static int serve___wsdd__Probe(wsddService *service)
{	struct soap *soap = service->soap;
	struct __wsdd__Probe soap_tmp___wsdd__Probe;
	soap_default___wsdd__Probe(soap, &soap_tmp___wsdd__Probe);
	soap->encodingStyle = NULL;
	if (!soap_get___wsdd__Probe(soap, &soap_tmp___wsdd__Probe, "-wsdd:Probe", NULL))
		return soap->error;
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap->error;
	soap->error = service->Probe(soap_tmp___wsdd__Probe.wsdd__Probe);
	if (soap->error)
		return soap->error;
	return soap_closesock(soap);
}

static int serve___wsdd__ProbeMatches(wsddService *service)
{	struct soap *soap = service->soap;
	struct __wsdd__ProbeMatches soap_tmp___wsdd__ProbeMatches;
	soap_default___wsdd__ProbeMatches(soap, &soap_tmp___wsdd__ProbeMatches);
	soap->encodingStyle = NULL;
	if (!soap_get___wsdd__ProbeMatches(soap, &soap_tmp___wsdd__ProbeMatches, "-wsdd:ProbeMatches", NULL))
		return soap->error;
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap->error;
	soap->error = service->ProbeMatches(soap_tmp___wsdd__ProbeMatches.wsdd__ProbeMatches);
	if (soap->error)
		return soap->error;
	return soap_closesock(soap);
}

static int serve___wsdd__Resolve(wsddService *service)
{	struct soap *soap = service->soap;
	struct __wsdd__Resolve soap_tmp___wsdd__Resolve;
	soap_default___wsdd__Resolve(soap, &soap_tmp___wsdd__Resolve);
	soap->encodingStyle = NULL;
	if (!soap_get___wsdd__Resolve(soap, &soap_tmp___wsdd__Resolve, "-wsdd:Resolve", NULL))
		return soap->error;
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap->error;
	soap->error = service->Resolve(soap_tmp___wsdd__Resolve.wsdd__Resolve);
	if (soap->error)
		return soap->error;
	return soap_closesock(soap);
}

static int serve___wsdd__ResolveMatches(wsddService *service)
{	struct soap *soap = service->soap;
	struct __wsdd__ResolveMatches soap_tmp___wsdd__ResolveMatches;
	soap_default___wsdd__ResolveMatches(soap, &soap_tmp___wsdd__ResolveMatches);
	soap->encodingStyle = NULL;
	if (!soap_get___wsdd__ResolveMatches(soap, &soap_tmp___wsdd__ResolveMatches, "-wsdd:ResolveMatches", NULL))
		return soap->error;
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap->error;
	soap->error = service->ResolveMatches(soap_tmp___wsdd__ResolveMatches.wsdd__ResolveMatches);
	if (soap->error)
		return soap->error;
	return soap_closesock(soap);
}
/* End of server object code */
