/* soapDiscoveryLookupBindingService.cpp
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

#include "soapDiscoveryLookupBindingService.h"

DiscoveryLookupBindingService::DiscoveryLookupBindingService()
{
    this->soap = soap_new();
    this->soap_own = true;
    DiscoveryLookupBindingService_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
}

DiscoveryLookupBindingService::DiscoveryLookupBindingService(const DiscoveryLookupBindingService& rhs)
{
    this->soap = rhs.soap;
    this->soap_own = false;
}

DiscoveryLookupBindingService::DiscoveryLookupBindingService(struct soap *_soap)
{
    this->soap = _soap;
    this->soap_own = false;
    DiscoveryLookupBindingService_init(_soap->imode, _soap->omode);
}

DiscoveryLookupBindingService::DiscoveryLookupBindingService(soap_mode iomode)
{
    this->soap = soap_new();
    this->soap_own = true;
    DiscoveryLookupBindingService_init(iomode, iomode);
}

DiscoveryLookupBindingService::DiscoveryLookupBindingService(soap_mode imode, soap_mode omode)
{
    this->soap = soap_new();
    this->soap_own = true;
    DiscoveryLookupBindingService_init(imode, omode);
}

DiscoveryLookupBindingService::~DiscoveryLookupBindingService()
{
    if (this->soap_own)
        soap_free(this->soap);
}

void DiscoveryLookupBindingService::DiscoveryLookupBindingService_init(soap_mode imode, soap_mode omode)
{
    soap_imode(this->soap, imode);
    soap_omode(this->soap, omode);
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

void DiscoveryLookupBindingService::destroy()
{
    soap_destroy(this->soap);
    soap_end(this->soap);
}

void DiscoveryLookupBindingService::reset()
{
    this->destroy();
    soap_done(this->soap);
    soap_initialize(this->soap);
    DiscoveryLookupBindingService_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
}

#ifndef WITH_PURE_VIRTUAL
DiscoveryLookupBindingService *DiscoveryLookupBindingService::copy()
{
    DiscoveryLookupBindingService *dup = SOAP_NEW_UNMANAGED(DiscoveryLookupBindingService);
    if (dup)
    {
        soap_done(dup->soap);
        soap_copy_context(dup->soap, this->soap);
    }
    return dup;
}
#endif

DiscoveryLookupBindingService& DiscoveryLookupBindingService::operator=(const DiscoveryLookupBindingService& rhs)
{
    if (this->soap != rhs.soap)
    {
        if (this->soap_own)
            soap_free(this->soap);
        this->soap = rhs.soap;
        this->soap_own = false;
    }
    return *this;
}

int DiscoveryLookupBindingService::soap_close_socket()
{
    return soap_closesock(this->soap);
}

int DiscoveryLookupBindingService::soap_force_close_socket()
{
    return soap_force_closesock(this->soap);
}

int DiscoveryLookupBindingService::soap_senderfault(const char *string, const char *detailXML)
{
    return ::soap_sender_fault(this->soap, string, detailXML);
}

int DiscoveryLookupBindingService::soap_senderfault(const char *subcodeQName, const char *string, const char *detailXML)
{
    return ::soap_sender_fault_subcode(this->soap, subcodeQName, string, detailXML);
}

int DiscoveryLookupBindingService::soap_receiverfault(const char *string, const char *detailXML)
{
    return ::soap_receiver_fault(this->soap, string, detailXML);
}

int DiscoveryLookupBindingService::soap_receiverfault(const char *subcodeQName, const char *string, const char *detailXML)
{
    return ::soap_receiver_fault_subcode(this->soap, subcodeQName, string, detailXML);
}

void DiscoveryLookupBindingService::soap_print_fault(FILE *fd)
{
    ::soap_print_fault(this->soap, fd);
}

#ifndef WITH_LEAN
#ifndef WITH_COMPAT
void DiscoveryLookupBindingService::soap_stream_fault(std::ostream& os)
{
    ::soap_stream_fault(this->soap, os);
}
#endif

char *DiscoveryLookupBindingService::soap_sprint_fault(char *buf, size_t len)
{
    return ::soap_sprint_fault(this->soap, buf, len);
}
#endif

void DiscoveryLookupBindingService::soap_noheader()
{
    this->soap->header = NULL;
}

void DiscoveryLookupBindingService::soap_header(char *wsa5__MessageID, struct wsa5__RelatesToType *wsa5__RelatesTo, struct wsa5__EndpointReferenceType *wsa5__From, struct wsa5__EndpointReferenceType *wsa5__ReplyTo, struct wsa5__EndpointReferenceType *wsa5__FaultTo, char *wsa5__To, char *wsa5__Action, struct chan__ChannelInstanceType *chan__ChannelInstance, struct wsdd__AppSequenceType *wsdd__AppSequence, struct _wsse__Security *wsse__Security, char *SubscriptionId)
{
    ::soap_header(this->soap);
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
    this->soap->header->SubscriptionId = SubscriptionId;
}

::SOAP_ENV__Header *DiscoveryLookupBindingService::soap_header()
{
    return this->soap->header;
}

#ifndef WITH_NOIO
int DiscoveryLookupBindingService::run(int port)
{
    if (!soap_valid_socket(this->soap->master) && !soap_valid_socket(this->bind(NULL, port, 100)))
        return this->soap->error;
    for (;;)
    {
        if (!soap_valid_socket(this->accept()))
        {
            if (this->soap->errnum == 0) // timeout?
                this->soap->error = SOAP_OK;
            break;
        }
        if (this->serve())
            break;
        this->destroy();
    }
    return this->soap->error;
}

#if defined(WITH_OPENSSL) || defined(WITH_GNUTLS)
int DiscoveryLookupBindingService::ssl_run(int port)
{
    if (!soap_valid_socket(this->soap->master) && !soap_valid_socket(this->bind(NULL, port, 100)))
        return this->soap->error;
    for (;;)
    {
        if (!soap_valid_socket(this->accept()))
        {
            if (this->soap->errnum == 0) // timeout?
                this->soap->error = SOAP_OK;
            break;
        }
        if (this->ssl_accept() || this->serve())
            break;
        this->destroy();
    }
    return this->soap->error;
}
#endif

SOAP_SOCKET DiscoveryLookupBindingService::bind(const char *host, int port, int backlog)
{
    return soap_bind(this->soap, host, port, backlog);
}

SOAP_SOCKET DiscoveryLookupBindingService::accept()
{
    return soap_accept(this->soap);
}

#if defined(WITH_OPENSSL) || defined(WITH_GNUTLS)
int DiscoveryLookupBindingService::ssl_accept()
{
    return soap_ssl_accept(this->soap);
}
#endif
#endif

int DiscoveryLookupBindingService::serve()
{
#ifndef WITH_FASTCGI
    this->soap->keep_alive = this->soap->max_keep_alive + 1;
#endif
    do
    {
#ifndef WITH_FASTCGI
        if (this->soap->keep_alive > 0 && this->soap->max_keep_alive > 0)
            this->soap->keep_alive--;
#endif
        if (soap_begin_serve(this->soap))
        {
            if (this->soap->error >= SOAP_STOP)
                continue;
            return this->soap->error;
        }
        if ((dispatch() || (this->soap->fserveloop && this->soap->fserveloop(this->soap))) && this->soap->error && this->soap->error < SOAP_STOP)
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

static int serve___onvifNetwork_dlb__Probe(struct soap*, DiscoveryLookupBindingService*);

int DiscoveryLookupBindingService::dispatch()
{
    return dispatch(this->soap);
}

int DiscoveryLookupBindingService::dispatch(struct soap* soap)
{
    DiscoveryLookupBindingService_init(soap->imode, soap->omode);

    soap_peek_element(soap);
    if (!soap_match_tag(soap, soap->tag, "onvifNetwork:Probe"))
        return serve___onvifNetwork_dlb__Probe(soap, this);
    return soap->error = SOAP_NO_METHOD;
}

static int serve___onvifNetwork_dlb__Probe(struct soap *soap, DiscoveryLookupBindingService *service)
{
    struct __onvifNetwork_dlb__Probe soap_tmp___onvifNetwork_dlb__Probe;
    struct wsdd__ProbeMatchesType onvifNetwork__ProbeResponse;
    soap_default_wsdd__ProbeMatchesType(soap, &onvifNetwork__ProbeResponse);
    soap_default___onvifNetwork_dlb__Probe(soap, &soap_tmp___onvifNetwork_dlb__Probe);
    if (!soap_get___onvifNetwork_dlb__Probe(soap, &soap_tmp___onvifNetwork_dlb__Probe, "-onvifNetwork-dlb:Probe", NULL))
        return soap->error;
    if (soap_body_end_in(soap)
     || soap_envelope_end_in(soap)
     || soap_end_recv(soap))
        return soap->error;
    soap->error = service->Probe(soap_tmp___onvifNetwork_dlb__Probe.onvifNetwork__Probe, onvifNetwork__ProbeResponse);
    if (soap->error)
        return soap->error;
    soap->encodingStyle = NULL;
    soap_serializeheader(soap);
    soap_serialize_wsdd__ProbeMatchesType(soap, &onvifNetwork__ProbeResponse);
    if (soap_begin_count(soap))
        return soap->error;
    if (soap->mode & SOAP_IO_LENGTH)
    {
        if (soap_envelope_begin_out(soap)
         || soap_putheader(soap)
         || soap_body_begin_out(soap)
         || soap_put_wsdd__ProbeMatchesType(soap, &onvifNetwork__ProbeResponse, "onvifNetwork:ProbeResponse", "")
         || soap_body_end_out(soap)
         || soap_envelope_end_out(soap))
             return soap->error;
    };
    if (soap_end_count(soap)
     || soap_response(soap, SOAP_OK)
     || soap_envelope_begin_out(soap)
     || soap_putheader(soap)
     || soap_body_begin_out(soap)
     || soap_put_wsdd__ProbeMatchesType(soap, &onvifNetwork__ProbeResponse, "onvifNetwork:ProbeResponse", "")
     || soap_body_end_out(soap)
     || soap_envelope_end_out(soap)
     || soap_end_send(soap))
        return soap->error;
    return soap_closesock(soap);
}
/* End of server object code */
