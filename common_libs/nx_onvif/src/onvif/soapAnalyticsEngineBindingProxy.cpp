/* soapAnalyticsEngineBindingProxy.cpp
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

#include "soapAnalyticsEngineBindingProxy.h"

AnalyticsEngineBindingProxy::AnalyticsEngineBindingProxy()
{
    this->soap = soap_new();
    this->soap_own = true;
    AnalyticsEngineBindingProxy_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
}

AnalyticsEngineBindingProxy::AnalyticsEngineBindingProxy(const AnalyticsEngineBindingProxy& rhs)
{
    this->soap = rhs.soap;
    this->soap_own = false;
    this->soap_endpoint = rhs.soap_endpoint;
}

AnalyticsEngineBindingProxy::AnalyticsEngineBindingProxy(struct soap *_soap)
{
    this->soap = _soap;
    this->soap_own = false;
    AnalyticsEngineBindingProxy_init(_soap->imode, _soap->omode);
}

AnalyticsEngineBindingProxy::AnalyticsEngineBindingProxy(const char *endpoint)
{
    this->soap = soap_new();
    this->soap_own = true;
    AnalyticsEngineBindingProxy_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
    soap_endpoint = endpoint;
}

AnalyticsEngineBindingProxy::AnalyticsEngineBindingProxy(soap_mode iomode)
{
    this->soap = soap_new();
    this->soap_own = true;
    AnalyticsEngineBindingProxy_init(iomode, iomode);
}

AnalyticsEngineBindingProxy::AnalyticsEngineBindingProxy(const char *endpoint, soap_mode iomode)
{
    this->soap = soap_new();
    this->soap_own = true;
    AnalyticsEngineBindingProxy_init(iomode, iomode);
    soap_endpoint = endpoint;
}

AnalyticsEngineBindingProxy::AnalyticsEngineBindingProxy(soap_mode imode, soap_mode omode)
{
    this->soap = soap_new();
    this->soap_own = true;
    AnalyticsEngineBindingProxy_init(imode, omode);
}

AnalyticsEngineBindingProxy::~AnalyticsEngineBindingProxy()
{
    if (this->soap_own)
        soap_free(this->soap);
}

void AnalyticsEngineBindingProxy::AnalyticsEngineBindingProxy_init(soap_mode imode, soap_mode omode)
{
    soap_imode(this->soap, imode);
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

AnalyticsEngineBindingProxy *AnalyticsEngineBindingProxy::copy()
{
    AnalyticsEngineBindingProxy *dup = SOAP_NEW_UNMANAGED(AnalyticsEngineBindingProxy);
    if (dup)
    {
        soap_done(dup->soap);
        soap_copy_context(dup->soap, this->soap);
    }
    return dup;
}

AnalyticsEngineBindingProxy& AnalyticsEngineBindingProxy::operator=(const AnalyticsEngineBindingProxy& rhs)
{
    if (this->soap != rhs.soap)
    {
        if (this->soap_own)
            soap_free(this->soap);
        this->soap = rhs.soap;
        this->soap_own = false;
        this->soap_endpoint = rhs.soap_endpoint;
    }
    return *this;
}

void AnalyticsEngineBindingProxy::destroy()
{
    soap_destroy(this->soap);
    soap_end(this->soap);
}

void AnalyticsEngineBindingProxy::reset()
{
    this->destroy();
    soap_done(this->soap);
    soap_initialize(this->soap);
    AnalyticsEngineBindingProxy_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
}

void AnalyticsEngineBindingProxy::soap_noheader()
{
    this->soap->header = NULL;
}

void AnalyticsEngineBindingProxy::soap_header(char *wsa5__MessageID, struct wsa5__RelatesToType *wsa5__RelatesTo, struct wsa5__EndpointReferenceType *wsa5__From, struct wsa5__EndpointReferenceType *wsa5__ReplyTo, struct wsa5__EndpointReferenceType *wsa5__FaultTo, char *wsa5__To, char *wsa5__Action, struct chan__ChannelInstanceType *chan__ChannelInstance, struct wsdd__AppSequenceType *wsdd__AppSequence, struct _wsse__Security *wsse__Security, char *SubscriptionId)
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

::SOAP_ENV__Header *AnalyticsEngineBindingProxy::soap_header()
{
    return this->soap->header;
}

::SOAP_ENV__Fault *AnalyticsEngineBindingProxy::soap_fault()
{
    return this->soap->fault;
}

const char *AnalyticsEngineBindingProxy::soap_fault_string()
{
    return *soap_faultstring(this->soap);
}

const char *AnalyticsEngineBindingProxy::soap_fault_detail()
{
    return *soap_faultdetail(this->soap);
}

int AnalyticsEngineBindingProxy::soap_close_socket()
{
    return soap_closesock(this->soap);
}

int AnalyticsEngineBindingProxy::soap_force_close_socket()
{
    return soap_force_closesock(this->soap);
}

void AnalyticsEngineBindingProxy::soap_print_fault(FILE *fd)
{
    ::soap_print_fault(this->soap, fd);
}

#ifndef WITH_LEAN
#ifndef WITH_COMPAT
void AnalyticsEngineBindingProxy::soap_stream_fault(std::ostream& os)
{
    ::soap_stream_fault(this->soap, os);
}
#endif

char *AnalyticsEngineBindingProxy::soap_sprint_fault(char *buf, size_t len)
{
    return ::soap_sprint_fault(this->soap, buf, len);
}
#endif

int AnalyticsEngineBindingProxy::GetServiceCapabilities(const char *endpoint, const char *soap_action, _onvifAnalytics__GetServiceCapabilities *onvifAnalytics__GetServiceCapabilities, _onvifAnalytics__GetServiceCapabilitiesResponse &onvifAnalytics__GetServiceCapabilitiesResponse)
{
    struct __onvifAnalytics_aeb__GetServiceCapabilities soap_tmp___onvifAnalytics_aeb__GetServiceCapabilities;
    if (endpoint)
        soap_endpoint = endpoint;
    if (soap_action == NULL)
        soap_action = "http://www.onvif.org/ver20/analytics/wsdl/GetServiceCapabilities";
    soap_tmp___onvifAnalytics_aeb__GetServiceCapabilities.onvifAnalytics__GetServiceCapabilities = onvifAnalytics__GetServiceCapabilities;
    soap_begin(soap);
    soap->encodingStyle = NULL;
    soap_serializeheader(soap);
    soap_serialize___onvifAnalytics_aeb__GetServiceCapabilities(soap, &soap_tmp___onvifAnalytics_aeb__GetServiceCapabilities);
    if (soap_begin_count(soap))
        return soap->error;
    if (soap->mode & SOAP_IO_LENGTH)
    {
        if (soap_envelope_begin_out(soap)
         || soap_putheader(soap)
         || soap_body_begin_out(soap)
         || soap_put___onvifAnalytics_aeb__GetServiceCapabilities(soap, &soap_tmp___onvifAnalytics_aeb__GetServiceCapabilities, "-onvifAnalytics-aeb:GetServiceCapabilities", "")
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
     || soap_put___onvifAnalytics_aeb__GetServiceCapabilities(soap, &soap_tmp___onvifAnalytics_aeb__GetServiceCapabilities, "-onvifAnalytics-aeb:GetServiceCapabilities", "")
     || soap_body_end_out(soap)
     || soap_envelope_end_out(soap)
     || soap_end_send(soap))
        return soap_closesock(soap);
    if (!static_cast<_onvifAnalytics__GetServiceCapabilitiesResponse*>(&onvifAnalytics__GetServiceCapabilitiesResponse)) // NULL ref?
        return soap_closesock(soap);
    onvifAnalytics__GetServiceCapabilitiesResponse.soap_default(soap);
    if (soap_begin_recv(soap)
     || soap_envelope_begin_in(soap)
     || soap_recv_header(soap)
     || soap_body_begin_in(soap))
        return soap_closesock(soap);
    onvifAnalytics__GetServiceCapabilitiesResponse.soap_get(soap, "onvifAnalytics:GetServiceCapabilitiesResponse", NULL);
    if (soap->error)
        return soap_recv_fault(soap, 0);
    if (soap_body_end_in(soap)
     || soap_envelope_end_in(soap)
     || soap_end_recv(soap))
        return soap_closesock(soap);
    return soap_closesock(soap);
}

int AnalyticsEngineBindingProxy::GetSupportedAnalyticsModules(const char *endpoint, const char *soap_action, _onvifAnalytics__GetSupportedAnalyticsModules *onvifAnalytics__GetSupportedAnalyticsModules, _onvifAnalytics__GetSupportedAnalyticsModulesResponse &onvifAnalytics__GetSupportedAnalyticsModulesResponse)
{
    struct __onvifAnalytics_aeb__GetSupportedAnalyticsModules soap_tmp___onvifAnalytics_aeb__GetSupportedAnalyticsModules;
    if (endpoint)
        soap_endpoint = endpoint;
    if (soap_action == NULL)
        soap_action = "http://www.onvif.org/ver20/analytics/wsdl/GetSupportedAnalyticsModules";
    soap_tmp___onvifAnalytics_aeb__GetSupportedAnalyticsModules.onvifAnalytics__GetSupportedAnalyticsModules = onvifAnalytics__GetSupportedAnalyticsModules;
    soap_begin(soap);
    soap->encodingStyle = NULL;
    soap_serializeheader(soap);
    soap_serialize___onvifAnalytics_aeb__GetSupportedAnalyticsModules(soap, &soap_tmp___onvifAnalytics_aeb__GetSupportedAnalyticsModules);
    if (soap_begin_count(soap))
        return soap->error;
    if (soap->mode & SOAP_IO_LENGTH)
    {
        if (soap_envelope_begin_out(soap)
         || soap_putheader(soap)
         || soap_body_begin_out(soap)
         || soap_put___onvifAnalytics_aeb__GetSupportedAnalyticsModules(soap, &soap_tmp___onvifAnalytics_aeb__GetSupportedAnalyticsModules, "-onvifAnalytics-aeb:GetSupportedAnalyticsModules", "")
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
     || soap_put___onvifAnalytics_aeb__GetSupportedAnalyticsModules(soap, &soap_tmp___onvifAnalytics_aeb__GetSupportedAnalyticsModules, "-onvifAnalytics-aeb:GetSupportedAnalyticsModules", "")
     || soap_body_end_out(soap)
     || soap_envelope_end_out(soap)
     || soap_end_send(soap))
        return soap_closesock(soap);
    if (!static_cast<_onvifAnalytics__GetSupportedAnalyticsModulesResponse*>(&onvifAnalytics__GetSupportedAnalyticsModulesResponse)) // NULL ref?
        return soap_closesock(soap);
    onvifAnalytics__GetSupportedAnalyticsModulesResponse.soap_default(soap);
    if (soap_begin_recv(soap)
     || soap_envelope_begin_in(soap)
     || soap_recv_header(soap)
     || soap_body_begin_in(soap))
        return soap_closesock(soap);
    onvifAnalytics__GetSupportedAnalyticsModulesResponse.soap_get(soap, "onvifAnalytics:GetSupportedAnalyticsModulesResponse", NULL);
    if (soap->error)
        return soap_recv_fault(soap, 0);
    if (soap_body_end_in(soap)
     || soap_envelope_end_in(soap)
     || soap_end_recv(soap))
        return soap_closesock(soap);
    return soap_closesock(soap);
}

int AnalyticsEngineBindingProxy::CreateAnalyticsModules(const char *endpoint, const char *soap_action, _onvifAnalytics__CreateAnalyticsModules *onvifAnalytics__CreateAnalyticsModules, _onvifAnalytics__CreateAnalyticsModulesResponse &onvifAnalytics__CreateAnalyticsModulesResponse)
{
    struct __onvifAnalytics_aeb__CreateAnalyticsModules soap_tmp___onvifAnalytics_aeb__CreateAnalyticsModules;
    if (endpoint)
        soap_endpoint = endpoint;
    if (soap_action == NULL)
        soap_action = "http://www.onvif.org/ver20/analytics/wsdl/CreateAnalyticsModules";
    soap_tmp___onvifAnalytics_aeb__CreateAnalyticsModules.onvifAnalytics__CreateAnalyticsModules = onvifAnalytics__CreateAnalyticsModules;
    soap_begin(soap);
    soap->encodingStyle = NULL;
    soap_serializeheader(soap);
    soap_serialize___onvifAnalytics_aeb__CreateAnalyticsModules(soap, &soap_tmp___onvifAnalytics_aeb__CreateAnalyticsModules);
    if (soap_begin_count(soap))
        return soap->error;
    if (soap->mode & SOAP_IO_LENGTH)
    {
        if (soap_envelope_begin_out(soap)
         || soap_putheader(soap)
         || soap_body_begin_out(soap)
         || soap_put___onvifAnalytics_aeb__CreateAnalyticsModules(soap, &soap_tmp___onvifAnalytics_aeb__CreateAnalyticsModules, "-onvifAnalytics-aeb:CreateAnalyticsModules", "")
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
     || soap_put___onvifAnalytics_aeb__CreateAnalyticsModules(soap, &soap_tmp___onvifAnalytics_aeb__CreateAnalyticsModules, "-onvifAnalytics-aeb:CreateAnalyticsModules", "")
     || soap_body_end_out(soap)
     || soap_envelope_end_out(soap)
     || soap_end_send(soap))
        return soap_closesock(soap);
    if (!static_cast<_onvifAnalytics__CreateAnalyticsModulesResponse*>(&onvifAnalytics__CreateAnalyticsModulesResponse)) // NULL ref?
        return soap_closesock(soap);
    onvifAnalytics__CreateAnalyticsModulesResponse.soap_default(soap);
    if (soap_begin_recv(soap)
     || soap_envelope_begin_in(soap)
     || soap_recv_header(soap)
     || soap_body_begin_in(soap))
        return soap_closesock(soap);
    onvifAnalytics__CreateAnalyticsModulesResponse.soap_get(soap, "onvifAnalytics:CreateAnalyticsModulesResponse", NULL);
    if (soap->error)
        return soap_recv_fault(soap, 0);
    if (soap_body_end_in(soap)
     || soap_envelope_end_in(soap)
     || soap_end_recv(soap))
        return soap_closesock(soap);
    return soap_closesock(soap);
}

int AnalyticsEngineBindingProxy::DeleteAnalyticsModules(const char *endpoint, const char *soap_action, _onvifAnalytics__DeleteAnalyticsModules *onvifAnalytics__DeleteAnalyticsModules, _onvifAnalytics__DeleteAnalyticsModulesResponse &onvifAnalytics__DeleteAnalyticsModulesResponse)
{
    struct __onvifAnalytics_aeb__DeleteAnalyticsModules soap_tmp___onvifAnalytics_aeb__DeleteAnalyticsModules;
    if (endpoint)
        soap_endpoint = endpoint;
    if (soap_action == NULL)
        soap_action = "http://www.onvif.org/ver20/analytics/wsdl/DeleteAnalyticsModules";
    soap_tmp___onvifAnalytics_aeb__DeleteAnalyticsModules.onvifAnalytics__DeleteAnalyticsModules = onvifAnalytics__DeleteAnalyticsModules;
    soap_begin(soap);
    soap->encodingStyle = NULL;
    soap_serializeheader(soap);
    soap_serialize___onvifAnalytics_aeb__DeleteAnalyticsModules(soap, &soap_tmp___onvifAnalytics_aeb__DeleteAnalyticsModules);
    if (soap_begin_count(soap))
        return soap->error;
    if (soap->mode & SOAP_IO_LENGTH)
    {
        if (soap_envelope_begin_out(soap)
         || soap_putheader(soap)
         || soap_body_begin_out(soap)
         || soap_put___onvifAnalytics_aeb__DeleteAnalyticsModules(soap, &soap_tmp___onvifAnalytics_aeb__DeleteAnalyticsModules, "-onvifAnalytics-aeb:DeleteAnalyticsModules", "")
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
     || soap_put___onvifAnalytics_aeb__DeleteAnalyticsModules(soap, &soap_tmp___onvifAnalytics_aeb__DeleteAnalyticsModules, "-onvifAnalytics-aeb:DeleteAnalyticsModules", "")
     || soap_body_end_out(soap)
     || soap_envelope_end_out(soap)
     || soap_end_send(soap))
        return soap_closesock(soap);
    if (!static_cast<_onvifAnalytics__DeleteAnalyticsModulesResponse*>(&onvifAnalytics__DeleteAnalyticsModulesResponse)) // NULL ref?
        return soap_closesock(soap);
    onvifAnalytics__DeleteAnalyticsModulesResponse.soap_default(soap);
    if (soap_begin_recv(soap)
     || soap_envelope_begin_in(soap)
     || soap_recv_header(soap)
     || soap_body_begin_in(soap))
        return soap_closesock(soap);
    onvifAnalytics__DeleteAnalyticsModulesResponse.soap_get(soap, "onvifAnalytics:DeleteAnalyticsModulesResponse", NULL);
    if (soap->error)
        return soap_recv_fault(soap, 0);
    if (soap_body_end_in(soap)
     || soap_envelope_end_in(soap)
     || soap_end_recv(soap))
        return soap_closesock(soap);
    return soap_closesock(soap);
}

int AnalyticsEngineBindingProxy::GetAnalyticsModules(const char *endpoint, const char *soap_action, _onvifAnalytics__GetAnalyticsModules *onvifAnalytics__GetAnalyticsModules, _onvifAnalytics__GetAnalyticsModulesResponse &onvifAnalytics__GetAnalyticsModulesResponse)
{
    struct __onvifAnalytics_aeb__GetAnalyticsModules soap_tmp___onvifAnalytics_aeb__GetAnalyticsModules;
    if (endpoint)
        soap_endpoint = endpoint;
    if (soap_action == NULL)
        soap_action = "http://www.onvif.org/ver20/analytics/wsdl/GetAnalyticsModules";
    soap_tmp___onvifAnalytics_aeb__GetAnalyticsModules.onvifAnalytics__GetAnalyticsModules = onvifAnalytics__GetAnalyticsModules;
    soap_begin(soap);
    soap->encodingStyle = NULL;
    soap_serializeheader(soap);
    soap_serialize___onvifAnalytics_aeb__GetAnalyticsModules(soap, &soap_tmp___onvifAnalytics_aeb__GetAnalyticsModules);
    if (soap_begin_count(soap))
        return soap->error;
    if (soap->mode & SOAP_IO_LENGTH)
    {
        if (soap_envelope_begin_out(soap)
         || soap_putheader(soap)
         || soap_body_begin_out(soap)
         || soap_put___onvifAnalytics_aeb__GetAnalyticsModules(soap, &soap_tmp___onvifAnalytics_aeb__GetAnalyticsModules, "-onvifAnalytics-aeb:GetAnalyticsModules", "")
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
     || soap_put___onvifAnalytics_aeb__GetAnalyticsModules(soap, &soap_tmp___onvifAnalytics_aeb__GetAnalyticsModules, "-onvifAnalytics-aeb:GetAnalyticsModules", "")
     || soap_body_end_out(soap)
     || soap_envelope_end_out(soap)
     || soap_end_send(soap))
        return soap_closesock(soap);
    if (!static_cast<_onvifAnalytics__GetAnalyticsModulesResponse*>(&onvifAnalytics__GetAnalyticsModulesResponse)) // NULL ref?
        return soap_closesock(soap);
    onvifAnalytics__GetAnalyticsModulesResponse.soap_default(soap);
    if (soap_begin_recv(soap)
     || soap_envelope_begin_in(soap)
     || soap_recv_header(soap)
     || soap_body_begin_in(soap))
        return soap_closesock(soap);
    onvifAnalytics__GetAnalyticsModulesResponse.soap_get(soap, "onvifAnalytics:GetAnalyticsModulesResponse", NULL);
    if (soap->error)
        return soap_recv_fault(soap, 0);
    if (soap_body_end_in(soap)
     || soap_envelope_end_in(soap)
     || soap_end_recv(soap))
        return soap_closesock(soap);
    return soap_closesock(soap);
}

int AnalyticsEngineBindingProxy::GetAnalyticsModuleOptions(const char *endpoint, const char *soap_action, _onvifAnalytics__GetAnalyticsModuleOptions *onvifAnalytics__GetAnalyticsModuleOptions, _onvifAnalytics__GetAnalyticsModuleOptionsResponse &onvifAnalytics__GetAnalyticsModuleOptionsResponse)
{
    struct __onvifAnalytics_aeb__GetAnalyticsModuleOptions soap_tmp___onvifAnalytics_aeb__GetAnalyticsModuleOptions;
    if (endpoint)
        soap_endpoint = endpoint;
    if (soap_action == NULL)
        soap_action = "http://www.onvif.org/ver20/analytics/wsdl/GetAnalyticsModuleOptions";
    soap_tmp___onvifAnalytics_aeb__GetAnalyticsModuleOptions.onvifAnalytics__GetAnalyticsModuleOptions = onvifAnalytics__GetAnalyticsModuleOptions;
    soap_begin(soap);
    soap->encodingStyle = NULL;
    soap_serializeheader(soap);
    soap_serialize___onvifAnalytics_aeb__GetAnalyticsModuleOptions(soap, &soap_tmp___onvifAnalytics_aeb__GetAnalyticsModuleOptions);
    if (soap_begin_count(soap))
        return soap->error;
    if (soap->mode & SOAP_IO_LENGTH)
    {
        if (soap_envelope_begin_out(soap)
         || soap_putheader(soap)
         || soap_body_begin_out(soap)
         || soap_put___onvifAnalytics_aeb__GetAnalyticsModuleOptions(soap, &soap_tmp___onvifAnalytics_aeb__GetAnalyticsModuleOptions, "-onvifAnalytics-aeb:GetAnalyticsModuleOptions", "")
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
     || soap_put___onvifAnalytics_aeb__GetAnalyticsModuleOptions(soap, &soap_tmp___onvifAnalytics_aeb__GetAnalyticsModuleOptions, "-onvifAnalytics-aeb:GetAnalyticsModuleOptions", "")
     || soap_body_end_out(soap)
     || soap_envelope_end_out(soap)
     || soap_end_send(soap))
        return soap_closesock(soap);
    if (!static_cast<_onvifAnalytics__GetAnalyticsModuleOptionsResponse*>(&onvifAnalytics__GetAnalyticsModuleOptionsResponse)) // NULL ref?
        return soap_closesock(soap);
    onvifAnalytics__GetAnalyticsModuleOptionsResponse.soap_default(soap);
    if (soap_begin_recv(soap)
     || soap_envelope_begin_in(soap)
     || soap_recv_header(soap)
     || soap_body_begin_in(soap))
        return soap_closesock(soap);
    onvifAnalytics__GetAnalyticsModuleOptionsResponse.soap_get(soap, "onvifAnalytics:GetAnalyticsModuleOptionsResponse", NULL);
    if (soap->error)
        return soap_recv_fault(soap, 0);
    if (soap_body_end_in(soap)
     || soap_envelope_end_in(soap)
     || soap_end_recv(soap))
        return soap_closesock(soap);
    return soap_closesock(soap);
}

int AnalyticsEngineBindingProxy::ModifyAnalyticsModules(const char *endpoint, const char *soap_action, _onvifAnalytics__ModifyAnalyticsModules *onvifAnalytics__ModifyAnalyticsModules, _onvifAnalytics__ModifyAnalyticsModulesResponse &onvifAnalytics__ModifyAnalyticsModulesResponse)
{
    struct __onvifAnalytics_aeb__ModifyAnalyticsModules soap_tmp___onvifAnalytics_aeb__ModifyAnalyticsModules;
    if (endpoint)
        soap_endpoint = endpoint;
    if (soap_action == NULL)
        soap_action = "http://www.onvif.org/ver20/analytics/wsdl/ModifyAnalyticsModules";
    soap_tmp___onvifAnalytics_aeb__ModifyAnalyticsModules.onvifAnalytics__ModifyAnalyticsModules = onvifAnalytics__ModifyAnalyticsModules;
    soap_begin(soap);
    soap->encodingStyle = NULL;
    soap_serializeheader(soap);
    soap_serialize___onvifAnalytics_aeb__ModifyAnalyticsModules(soap, &soap_tmp___onvifAnalytics_aeb__ModifyAnalyticsModules);
    if (soap_begin_count(soap))
        return soap->error;
    if (soap->mode & SOAP_IO_LENGTH)
    {
        if (soap_envelope_begin_out(soap)
         || soap_putheader(soap)
         || soap_body_begin_out(soap)
         || soap_put___onvifAnalytics_aeb__ModifyAnalyticsModules(soap, &soap_tmp___onvifAnalytics_aeb__ModifyAnalyticsModules, "-onvifAnalytics-aeb:ModifyAnalyticsModules", "")
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
     || soap_put___onvifAnalytics_aeb__ModifyAnalyticsModules(soap, &soap_tmp___onvifAnalytics_aeb__ModifyAnalyticsModules, "-onvifAnalytics-aeb:ModifyAnalyticsModules", "")
     || soap_body_end_out(soap)
     || soap_envelope_end_out(soap)
     || soap_end_send(soap))
        return soap_closesock(soap);
    if (!static_cast<_onvifAnalytics__ModifyAnalyticsModulesResponse*>(&onvifAnalytics__ModifyAnalyticsModulesResponse)) // NULL ref?
        return soap_closesock(soap);
    onvifAnalytics__ModifyAnalyticsModulesResponse.soap_default(soap);
    if (soap_begin_recv(soap)
     || soap_envelope_begin_in(soap)
     || soap_recv_header(soap)
     || soap_body_begin_in(soap))
        return soap_closesock(soap);
    onvifAnalytics__ModifyAnalyticsModulesResponse.soap_get(soap, "onvifAnalytics:ModifyAnalyticsModulesResponse", NULL);
    if (soap->error)
        return soap_recv_fault(soap, 0);
    if (soap_body_end_in(soap)
     || soap_envelope_end_in(soap)
     || soap_end_recv(soap))
        return soap_closesock(soap);
    return soap_closesock(soap);
}
/* End of client proxy code */
