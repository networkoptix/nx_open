/* soapReceiverBindingService.cpp
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

#include "soapReceiverBindingService.h"

ReceiverBindingService::ReceiverBindingService()
{
    this->soap = soap_new();
    this->soap_own = true;
    ReceiverBindingService_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
}

ReceiverBindingService::ReceiverBindingService(const ReceiverBindingService& rhs)
{
    this->soap = rhs.soap;
    this->soap_own = false;
}

ReceiverBindingService::ReceiverBindingService(struct soap *_soap)
{
    this->soap = _soap;
    this->soap_own = false;
    ReceiverBindingService_init(_soap->imode, _soap->omode);
}

ReceiverBindingService::ReceiverBindingService(soap_mode iomode)
{
    this->soap = soap_new();
    this->soap_own = true;
    ReceiverBindingService_init(iomode, iomode);
}

ReceiverBindingService::ReceiverBindingService(soap_mode imode, soap_mode omode)
{
    this->soap = soap_new();
    this->soap_own = true;
    ReceiverBindingService_init(imode, omode);
}

ReceiverBindingService::~ReceiverBindingService()
{
    if (this->soap_own)
        soap_free(this->soap);
}

void ReceiverBindingService::ReceiverBindingService_init(soap_mode imode, soap_mode omode)
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

void ReceiverBindingService::destroy()
{
    soap_destroy(this->soap);
    soap_end(this->soap);
}

void ReceiverBindingService::reset()
{
    this->destroy();
    soap_done(this->soap);
    soap_initialize(this->soap);
    ReceiverBindingService_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
}

#ifndef WITH_PURE_VIRTUAL
ReceiverBindingService *ReceiverBindingService::copy()
{
    ReceiverBindingService *dup = SOAP_NEW_UNMANAGED(ReceiverBindingService);
    if (dup)
    {
        soap_done(dup->soap);
        soap_copy_context(dup->soap, this->soap);
    }
    return dup;
}
#endif

ReceiverBindingService& ReceiverBindingService::operator=(const ReceiverBindingService& rhs)
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

int ReceiverBindingService::soap_close_socket()
{
    return soap_closesock(this->soap);
}

int ReceiverBindingService::soap_force_close_socket()
{
    return soap_force_closesock(this->soap);
}

int ReceiverBindingService::soap_senderfault(const char *string, const char *detailXML)
{
    return ::soap_sender_fault(this->soap, string, detailXML);
}

int ReceiverBindingService::soap_senderfault(const char *subcodeQName, const char *string, const char *detailXML)
{
    return ::soap_sender_fault_subcode(this->soap, subcodeQName, string, detailXML);
}

int ReceiverBindingService::soap_receiverfault(const char *string, const char *detailXML)
{
    return ::soap_receiver_fault(this->soap, string, detailXML);
}

int ReceiverBindingService::soap_receiverfault(const char *subcodeQName, const char *string, const char *detailXML)
{
    return ::soap_receiver_fault_subcode(this->soap, subcodeQName, string, detailXML);
}

void ReceiverBindingService::soap_print_fault(FILE *fd)
{
    ::soap_print_fault(this->soap, fd);
}

#ifndef WITH_LEAN
#ifndef WITH_COMPAT
void ReceiverBindingService::soap_stream_fault(std::ostream& os)
{
    ::soap_stream_fault(this->soap, os);
}
#endif

char *ReceiverBindingService::soap_sprint_fault(char *buf, size_t len)
{
    return ::soap_sprint_fault(this->soap, buf, len);
}
#endif

void ReceiverBindingService::soap_noheader()
{
    this->soap->header = NULL;
}

void ReceiverBindingService::soap_header(char *wsa5__MessageID, struct wsa5__RelatesToType *wsa5__RelatesTo, struct wsa5__EndpointReferenceType *wsa5__From, struct wsa5__EndpointReferenceType *wsa5__ReplyTo, struct wsa5__EndpointReferenceType *wsa5__FaultTo, char *wsa5__To, char *wsa5__Action, struct chan__ChannelInstanceType *chan__ChannelInstance, struct wsdd__AppSequenceType *wsdd__AppSequence, struct _wsse__Security *wsse__Security, char *SubscriptionId)
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

::SOAP_ENV__Header *ReceiverBindingService::soap_header()
{
    return this->soap->header;
}

#ifndef WITH_NOIO
int ReceiverBindingService::run(int port)
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
int ReceiverBindingService::ssl_run(int port)
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

SOAP_SOCKET ReceiverBindingService::bind(const char *host, int port, int backlog)
{
    return soap_bind(this->soap, host, port, backlog);
}

SOAP_SOCKET ReceiverBindingService::accept()
{
    return soap_accept(this->soap);
}

#if defined(WITH_OPENSSL) || defined(WITH_GNUTLS)
int ReceiverBindingService::ssl_accept()
{
    return soap_ssl_accept(this->soap);
}
#endif
#endif

int ReceiverBindingService::serve()
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

static int serve___onvifReceiver__GetServiceCapabilities(struct soap*, ReceiverBindingService*);
static int serve___onvifReceiver__GetReceivers(struct soap*, ReceiverBindingService*);
static int serve___onvifReceiver__GetReceiver(struct soap*, ReceiverBindingService*);
static int serve___onvifReceiver__CreateReceiver(struct soap*, ReceiverBindingService*);
static int serve___onvifReceiver__DeleteReceiver(struct soap*, ReceiverBindingService*);
static int serve___onvifReceiver__ConfigureReceiver(struct soap*, ReceiverBindingService*);
static int serve___onvifReceiver__SetReceiverMode(struct soap*, ReceiverBindingService*);
static int serve___onvifReceiver__GetReceiverState(struct soap*, ReceiverBindingService*);

int ReceiverBindingService::dispatch()
{
    return dispatch(this->soap);
}

int ReceiverBindingService::dispatch(struct soap* soap)
{
    ReceiverBindingService_init(soap->imode, soap->omode);

    soap_peek_element(soap);
    if (!soap_match_tag(soap, soap->tag, "onvifReceiver:GetServiceCapabilities"))
        return serve___onvifReceiver__GetServiceCapabilities(soap, this);
    if (!soap_match_tag(soap, soap->tag, "onvifReceiver:GetReceivers"))
        return serve___onvifReceiver__GetReceivers(soap, this);
    if (!soap_match_tag(soap, soap->tag, "onvifReceiver:GetReceiver"))
        return serve___onvifReceiver__GetReceiver(soap, this);
    if (!soap_match_tag(soap, soap->tag, "onvifReceiver:CreateReceiver"))
        return serve___onvifReceiver__CreateReceiver(soap, this);
    if (!soap_match_tag(soap, soap->tag, "onvifReceiver:DeleteReceiver"))
        return serve___onvifReceiver__DeleteReceiver(soap, this);
    if (!soap_match_tag(soap, soap->tag, "onvifReceiver:ConfigureReceiver"))
        return serve___onvifReceiver__ConfigureReceiver(soap, this);
    if (!soap_match_tag(soap, soap->tag, "onvifReceiver:SetReceiverMode"))
        return serve___onvifReceiver__SetReceiverMode(soap, this);
    if (!soap_match_tag(soap, soap->tag, "onvifReceiver:GetReceiverState"))
        return serve___onvifReceiver__GetReceiverState(soap, this);
    return soap->error = SOAP_NO_METHOD;
}

static int serve___onvifReceiver__GetServiceCapabilities(struct soap *soap, ReceiverBindingService *service)
{
    struct __onvifReceiver__GetServiceCapabilities soap_tmp___onvifReceiver__GetServiceCapabilities;
    _onvifReceiver__GetServiceCapabilitiesResponse onvifReceiver__GetServiceCapabilitiesResponse;
    onvifReceiver__GetServiceCapabilitiesResponse.soap_default(soap);
    soap_default___onvifReceiver__GetServiceCapabilities(soap, &soap_tmp___onvifReceiver__GetServiceCapabilities);
    if (!soap_get___onvifReceiver__GetServiceCapabilities(soap, &soap_tmp___onvifReceiver__GetServiceCapabilities, "-onvifReceiver:GetServiceCapabilities", NULL))
        return soap->error;
    if (soap_body_end_in(soap)
     || soap_envelope_end_in(soap)
     || soap_end_recv(soap))
        return soap->error;
    soap->error = service->GetServiceCapabilities(soap_tmp___onvifReceiver__GetServiceCapabilities.onvifReceiver__GetServiceCapabilities, onvifReceiver__GetServiceCapabilitiesResponse);
    if (soap->error)
        return soap->error;
    soap->encodingStyle = NULL;
    soap_serializeheader(soap);
    onvifReceiver__GetServiceCapabilitiesResponse.soap_serialize(soap);
    if (soap_begin_count(soap))
        return soap->error;
    if (soap->mode & SOAP_IO_LENGTH)
    {
        if (soap_envelope_begin_out(soap)
         || soap_putheader(soap)
         || soap_body_begin_out(soap)
         || onvifReceiver__GetServiceCapabilitiesResponse.soap_put(soap, "onvifReceiver:GetServiceCapabilitiesResponse", "")
         || soap_body_end_out(soap)
         || soap_envelope_end_out(soap))
             return soap->error;
    };
    if (soap_end_count(soap)
     || soap_response(soap, SOAP_OK)
     || soap_envelope_begin_out(soap)
     || soap_putheader(soap)
     || soap_body_begin_out(soap)
     || onvifReceiver__GetServiceCapabilitiesResponse.soap_put(soap, "onvifReceiver:GetServiceCapabilitiesResponse", "")
     || soap_body_end_out(soap)
     || soap_envelope_end_out(soap)
     || soap_end_send(soap))
        return soap->error;
    return soap_closesock(soap);
}

static int serve___onvifReceiver__GetReceivers(struct soap *soap, ReceiverBindingService *service)
{
    struct __onvifReceiver__GetReceivers soap_tmp___onvifReceiver__GetReceivers;
    _onvifReceiver__GetReceiversResponse onvifReceiver__GetReceiversResponse;
    onvifReceiver__GetReceiversResponse.soap_default(soap);
    soap_default___onvifReceiver__GetReceivers(soap, &soap_tmp___onvifReceiver__GetReceivers);
    if (!soap_get___onvifReceiver__GetReceivers(soap, &soap_tmp___onvifReceiver__GetReceivers, "-onvifReceiver:GetReceivers", NULL))
        return soap->error;
    if (soap_body_end_in(soap)
     || soap_envelope_end_in(soap)
     || soap_end_recv(soap))
        return soap->error;
    soap->error = service->GetReceivers(soap_tmp___onvifReceiver__GetReceivers.onvifReceiver__GetReceivers, onvifReceiver__GetReceiversResponse);
    if (soap->error)
        return soap->error;
    soap->encodingStyle = NULL;
    soap_serializeheader(soap);
    onvifReceiver__GetReceiversResponse.soap_serialize(soap);
    if (soap_begin_count(soap))
        return soap->error;
    if (soap->mode & SOAP_IO_LENGTH)
    {
        if (soap_envelope_begin_out(soap)
         || soap_putheader(soap)
         || soap_body_begin_out(soap)
         || onvifReceiver__GetReceiversResponse.soap_put(soap, "onvifReceiver:GetReceiversResponse", "")
         || soap_body_end_out(soap)
         || soap_envelope_end_out(soap))
             return soap->error;
    };
    if (soap_end_count(soap)
     || soap_response(soap, SOAP_OK)
     || soap_envelope_begin_out(soap)
     || soap_putheader(soap)
     || soap_body_begin_out(soap)
     || onvifReceiver__GetReceiversResponse.soap_put(soap, "onvifReceiver:GetReceiversResponse", "")
     || soap_body_end_out(soap)
     || soap_envelope_end_out(soap)
     || soap_end_send(soap))
        return soap->error;
    return soap_closesock(soap);
}

static int serve___onvifReceiver__GetReceiver(struct soap *soap, ReceiverBindingService *service)
{
    struct __onvifReceiver__GetReceiver soap_tmp___onvifReceiver__GetReceiver;
    _onvifReceiver__GetReceiverResponse onvifReceiver__GetReceiverResponse;
    onvifReceiver__GetReceiverResponse.soap_default(soap);
    soap_default___onvifReceiver__GetReceiver(soap, &soap_tmp___onvifReceiver__GetReceiver);
    if (!soap_get___onvifReceiver__GetReceiver(soap, &soap_tmp___onvifReceiver__GetReceiver, "-onvifReceiver:GetReceiver", NULL))
        return soap->error;
    if (soap_body_end_in(soap)
     || soap_envelope_end_in(soap)
     || soap_end_recv(soap))
        return soap->error;
    soap->error = service->GetReceiver(soap_tmp___onvifReceiver__GetReceiver.onvifReceiver__GetReceiver, onvifReceiver__GetReceiverResponse);
    if (soap->error)
        return soap->error;
    soap->encodingStyle = NULL;
    soap_serializeheader(soap);
    onvifReceiver__GetReceiverResponse.soap_serialize(soap);
    if (soap_begin_count(soap))
        return soap->error;
    if (soap->mode & SOAP_IO_LENGTH)
    {
        if (soap_envelope_begin_out(soap)
         || soap_putheader(soap)
         || soap_body_begin_out(soap)
         || onvifReceiver__GetReceiverResponse.soap_put(soap, "onvifReceiver:GetReceiverResponse", "")
         || soap_body_end_out(soap)
         || soap_envelope_end_out(soap))
             return soap->error;
    };
    if (soap_end_count(soap)
     || soap_response(soap, SOAP_OK)
     || soap_envelope_begin_out(soap)
     || soap_putheader(soap)
     || soap_body_begin_out(soap)
     || onvifReceiver__GetReceiverResponse.soap_put(soap, "onvifReceiver:GetReceiverResponse", "")
     || soap_body_end_out(soap)
     || soap_envelope_end_out(soap)
     || soap_end_send(soap))
        return soap->error;
    return soap_closesock(soap);
}

static int serve___onvifReceiver__CreateReceiver(struct soap *soap, ReceiverBindingService *service)
{
    struct __onvifReceiver__CreateReceiver soap_tmp___onvifReceiver__CreateReceiver;
    _onvifReceiver__CreateReceiverResponse onvifReceiver__CreateReceiverResponse;
    onvifReceiver__CreateReceiverResponse.soap_default(soap);
    soap_default___onvifReceiver__CreateReceiver(soap, &soap_tmp___onvifReceiver__CreateReceiver);
    if (!soap_get___onvifReceiver__CreateReceiver(soap, &soap_tmp___onvifReceiver__CreateReceiver, "-onvifReceiver:CreateReceiver", NULL))
        return soap->error;
    if (soap_body_end_in(soap)
     || soap_envelope_end_in(soap)
     || soap_end_recv(soap))
        return soap->error;
    soap->error = service->CreateReceiver(soap_tmp___onvifReceiver__CreateReceiver.onvifReceiver__CreateReceiver, onvifReceiver__CreateReceiverResponse);
    if (soap->error)
        return soap->error;
    soap->encodingStyle = NULL;
    soap_serializeheader(soap);
    onvifReceiver__CreateReceiverResponse.soap_serialize(soap);
    if (soap_begin_count(soap))
        return soap->error;
    if (soap->mode & SOAP_IO_LENGTH)
    {
        if (soap_envelope_begin_out(soap)
         || soap_putheader(soap)
         || soap_body_begin_out(soap)
         || onvifReceiver__CreateReceiverResponse.soap_put(soap, "onvifReceiver:CreateReceiverResponse", "")
         || soap_body_end_out(soap)
         || soap_envelope_end_out(soap))
             return soap->error;
    };
    if (soap_end_count(soap)
     || soap_response(soap, SOAP_OK)
     || soap_envelope_begin_out(soap)
     || soap_putheader(soap)
     || soap_body_begin_out(soap)
     || onvifReceiver__CreateReceiverResponse.soap_put(soap, "onvifReceiver:CreateReceiverResponse", "")
     || soap_body_end_out(soap)
     || soap_envelope_end_out(soap)
     || soap_end_send(soap))
        return soap->error;
    return soap_closesock(soap);
}

static int serve___onvifReceiver__DeleteReceiver(struct soap *soap, ReceiverBindingService *service)
{
    struct __onvifReceiver__DeleteReceiver soap_tmp___onvifReceiver__DeleteReceiver;
    _onvifReceiver__DeleteReceiverResponse onvifReceiver__DeleteReceiverResponse;
    onvifReceiver__DeleteReceiverResponse.soap_default(soap);
    soap_default___onvifReceiver__DeleteReceiver(soap, &soap_tmp___onvifReceiver__DeleteReceiver);
    if (!soap_get___onvifReceiver__DeleteReceiver(soap, &soap_tmp___onvifReceiver__DeleteReceiver, "-onvifReceiver:DeleteReceiver", NULL))
        return soap->error;
    if (soap_body_end_in(soap)
     || soap_envelope_end_in(soap)
     || soap_end_recv(soap))
        return soap->error;
    soap->error = service->DeleteReceiver(soap_tmp___onvifReceiver__DeleteReceiver.onvifReceiver__DeleteReceiver, onvifReceiver__DeleteReceiverResponse);
    if (soap->error)
        return soap->error;
    soap->encodingStyle = NULL;
    soap_serializeheader(soap);
    onvifReceiver__DeleteReceiverResponse.soap_serialize(soap);
    if (soap_begin_count(soap))
        return soap->error;
    if (soap->mode & SOAP_IO_LENGTH)
    {
        if (soap_envelope_begin_out(soap)
         || soap_putheader(soap)
         || soap_body_begin_out(soap)
         || onvifReceiver__DeleteReceiverResponse.soap_put(soap, "onvifReceiver:DeleteReceiverResponse", "")
         || soap_body_end_out(soap)
         || soap_envelope_end_out(soap))
             return soap->error;
    };
    if (soap_end_count(soap)
     || soap_response(soap, SOAP_OK)
     || soap_envelope_begin_out(soap)
     || soap_putheader(soap)
     || soap_body_begin_out(soap)
     || onvifReceiver__DeleteReceiverResponse.soap_put(soap, "onvifReceiver:DeleteReceiverResponse", "")
     || soap_body_end_out(soap)
     || soap_envelope_end_out(soap)
     || soap_end_send(soap))
        return soap->error;
    return soap_closesock(soap);
}

static int serve___onvifReceiver__ConfigureReceiver(struct soap *soap, ReceiverBindingService *service)
{
    struct __onvifReceiver__ConfigureReceiver soap_tmp___onvifReceiver__ConfigureReceiver;
    _onvifReceiver__ConfigureReceiverResponse onvifReceiver__ConfigureReceiverResponse;
    onvifReceiver__ConfigureReceiverResponse.soap_default(soap);
    soap_default___onvifReceiver__ConfigureReceiver(soap, &soap_tmp___onvifReceiver__ConfigureReceiver);
    if (!soap_get___onvifReceiver__ConfigureReceiver(soap, &soap_tmp___onvifReceiver__ConfigureReceiver, "-onvifReceiver:ConfigureReceiver", NULL))
        return soap->error;
    if (soap_body_end_in(soap)
     || soap_envelope_end_in(soap)
     || soap_end_recv(soap))
        return soap->error;
    soap->error = service->ConfigureReceiver(soap_tmp___onvifReceiver__ConfigureReceiver.onvifReceiver__ConfigureReceiver, onvifReceiver__ConfigureReceiverResponse);
    if (soap->error)
        return soap->error;
    soap->encodingStyle = NULL;
    soap_serializeheader(soap);
    onvifReceiver__ConfigureReceiverResponse.soap_serialize(soap);
    if (soap_begin_count(soap))
        return soap->error;
    if (soap->mode & SOAP_IO_LENGTH)
    {
        if (soap_envelope_begin_out(soap)
         || soap_putheader(soap)
         || soap_body_begin_out(soap)
         || onvifReceiver__ConfigureReceiverResponse.soap_put(soap, "onvifReceiver:ConfigureReceiverResponse", "")
         || soap_body_end_out(soap)
         || soap_envelope_end_out(soap))
             return soap->error;
    };
    if (soap_end_count(soap)
     || soap_response(soap, SOAP_OK)
     || soap_envelope_begin_out(soap)
     || soap_putheader(soap)
     || soap_body_begin_out(soap)
     || onvifReceiver__ConfigureReceiverResponse.soap_put(soap, "onvifReceiver:ConfigureReceiverResponse", "")
     || soap_body_end_out(soap)
     || soap_envelope_end_out(soap)
     || soap_end_send(soap))
        return soap->error;
    return soap_closesock(soap);
}

static int serve___onvifReceiver__SetReceiverMode(struct soap *soap, ReceiverBindingService *service)
{
    struct __onvifReceiver__SetReceiverMode soap_tmp___onvifReceiver__SetReceiverMode;
    _onvifReceiver__SetReceiverModeResponse onvifReceiver__SetReceiverModeResponse;
    onvifReceiver__SetReceiverModeResponse.soap_default(soap);
    soap_default___onvifReceiver__SetReceiverMode(soap, &soap_tmp___onvifReceiver__SetReceiverMode);
    if (!soap_get___onvifReceiver__SetReceiverMode(soap, &soap_tmp___onvifReceiver__SetReceiverMode, "-onvifReceiver:SetReceiverMode", NULL))
        return soap->error;
    if (soap_body_end_in(soap)
     || soap_envelope_end_in(soap)
     || soap_end_recv(soap))
        return soap->error;
    soap->error = service->SetReceiverMode(soap_tmp___onvifReceiver__SetReceiverMode.onvifReceiver__SetReceiverMode, onvifReceiver__SetReceiverModeResponse);
    if (soap->error)
        return soap->error;
    soap->encodingStyle = NULL;
    soap_serializeheader(soap);
    onvifReceiver__SetReceiverModeResponse.soap_serialize(soap);
    if (soap_begin_count(soap))
        return soap->error;
    if (soap->mode & SOAP_IO_LENGTH)
    {
        if (soap_envelope_begin_out(soap)
         || soap_putheader(soap)
         || soap_body_begin_out(soap)
         || onvifReceiver__SetReceiverModeResponse.soap_put(soap, "onvifReceiver:SetReceiverModeResponse", "")
         || soap_body_end_out(soap)
         || soap_envelope_end_out(soap))
             return soap->error;
    };
    if (soap_end_count(soap)
     || soap_response(soap, SOAP_OK)
     || soap_envelope_begin_out(soap)
     || soap_putheader(soap)
     || soap_body_begin_out(soap)
     || onvifReceiver__SetReceiverModeResponse.soap_put(soap, "onvifReceiver:SetReceiverModeResponse", "")
     || soap_body_end_out(soap)
     || soap_envelope_end_out(soap)
     || soap_end_send(soap))
        return soap->error;
    return soap_closesock(soap);
}

static int serve___onvifReceiver__GetReceiverState(struct soap *soap, ReceiverBindingService *service)
{
    struct __onvifReceiver__GetReceiverState soap_tmp___onvifReceiver__GetReceiverState;
    _onvifReceiver__GetReceiverStateResponse onvifReceiver__GetReceiverStateResponse;
    onvifReceiver__GetReceiverStateResponse.soap_default(soap);
    soap_default___onvifReceiver__GetReceiverState(soap, &soap_tmp___onvifReceiver__GetReceiverState);
    if (!soap_get___onvifReceiver__GetReceiverState(soap, &soap_tmp___onvifReceiver__GetReceiverState, "-onvifReceiver:GetReceiverState", NULL))
        return soap->error;
    if (soap_body_end_in(soap)
     || soap_envelope_end_in(soap)
     || soap_end_recv(soap))
        return soap->error;
    soap->error = service->GetReceiverState(soap_tmp___onvifReceiver__GetReceiverState.onvifReceiver__GetReceiverState, onvifReceiver__GetReceiverStateResponse);
    if (soap->error)
        return soap->error;
    soap->encodingStyle = NULL;
    soap_serializeheader(soap);
    onvifReceiver__GetReceiverStateResponse.soap_serialize(soap);
    if (soap_begin_count(soap))
        return soap->error;
    if (soap->mode & SOAP_IO_LENGTH)
    {
        if (soap_envelope_begin_out(soap)
         || soap_putheader(soap)
         || soap_body_begin_out(soap)
         || onvifReceiver__GetReceiverStateResponse.soap_put(soap, "onvifReceiver:GetReceiverStateResponse", "")
         || soap_body_end_out(soap)
         || soap_envelope_end_out(soap))
             return soap->error;
    };
    if (soap_end_count(soap)
     || soap_response(soap, SOAP_OK)
     || soap_envelope_begin_out(soap)
     || soap_putheader(soap)
     || soap_body_begin_out(soap)
     || onvifReceiver__GetReceiverStateResponse.soap_put(soap, "onvifReceiver:GetReceiverStateResponse", "")
     || soap_body_end_out(soap)
     || soap_envelope_end_out(soap)
     || soap_end_send(soap))
        return soap->error;
    return soap_closesock(soap);
}
/* End of server object code */
