/* soapEventBindingProxy.cpp
   Generated by gSOAP 2.8.55 for generated_with_wsdl2h/axis_soap_event_action

gSOAP XML Web services tools
Copyright (C) 2000-2017, Robert van Engelen, Genivia Inc. All Rights Reserved.
The soapcpp2 tool and its generated software are released under the GPL.
This program is released under the GPL with the additional exemption that
compiling, linking, and/or using OpenSSL is allowed.
--------------------------------------------------------------------------------
A commercial use license is available from Genivia Inc., contact@genivia.com
--------------------------------------------------------------------------------
*/

#include "soapEventBindingProxy.h"

EventBindingProxy::EventBindingProxy()
{	this->soap = soap_new();
	this->soap_own = true;
	EventBindingProxy_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
}

EventBindingProxy::EventBindingProxy(const EventBindingProxy& rhs)
{	this->soap = rhs.soap;
	this->soap_own = false;
	this->soap_endpoint = rhs.soap_endpoint;
}

EventBindingProxy::EventBindingProxy(struct soap *_soap)
{	this->soap = _soap;
	this->soap_own = false;
	EventBindingProxy_init(_soap->imode, _soap->omode);
}

EventBindingProxy::EventBindingProxy(const char *endpoint)
{	this->soap = soap_new();
	this->soap_own = true;
	EventBindingProxy_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
	soap_endpoint = endpoint;
}

EventBindingProxy::EventBindingProxy(soap_mode iomode)
{	this->soap = soap_new();
	this->soap_own = true;
	EventBindingProxy_init(iomode, iomode);
}

EventBindingProxy::EventBindingProxy(const char *endpoint, soap_mode iomode)
{	this->soap = soap_new();
	this->soap_own = true;
	EventBindingProxy_init(iomode, iomode);
	soap_endpoint = endpoint;
}

EventBindingProxy::EventBindingProxy(soap_mode imode, soap_mode omode)
{	this->soap = soap_new();
	this->soap_own = true;
	EventBindingProxy_init(imode, omode);
}

EventBindingProxy::~EventBindingProxy()
{	if (this->soap_own)
		soap_free(this->soap);
}

void EventBindingProxy::EventBindingProxy_init(soap_mode imode, soap_mode omode)
{	soap_imode(this->soap, imode);
	soap_omode(this->soap, omode);
	soap_endpoint = NULL;
	static const struct Namespace namespaces[] = {
        {"SOAP-ENV", "http://www.w3.org/2003/05/soap-envelope", "http://schemas.xmlsoap.org/soap/envelope/", NULL},
        {"SOAP-ENC", "http://www.w3.org/2003/05/soap-encoding", "http://schemas.xmlsoap.org/soap/encoding/", NULL},
        {"xsi", "http://www.w3.org/2001/XMLSchema-instance", "http://www.w3.org/*/XMLSchema-instance", NULL},
        {"xsd", "http://www.w3.org/2001/XMLSchema", "http://www.w3.org/*/XMLSchema", NULL},
        {"ns6", "http://www.w3.org/2005/08/addressing", NULL, NULL},
        {"ns4", "http://docs.oasis-open.org/wsrf/bf-2", NULL, NULL},
        {"ns2", "http://docs.oasis-open.org/wsn/t-1", NULL, NULL},
        {"ns3", "http://docs.oasis-open.org/wsn/b-2", NULL, NULL},
        {"ns1", "http://www.axis.com/vapix/ws/event1", NULL, NULL},
        {"ns5", "http://www.axis.com/vapix/ws/action1", NULL, NULL},
        {NULL, NULL, NULL, NULL}
    };
	soap_set_namespaces(this->soap, namespaces);
}

EventBindingProxy *EventBindingProxy::copy()
{	EventBindingProxy *dup = SOAP_NEW_COPY(EventBindingProxy);
	if (dup)
		soap_copy_context(dup->soap, this->soap);
	return dup;
}

EventBindingProxy& EventBindingProxy::operator=(const EventBindingProxy& rhs)
{	if (this->soap != rhs.soap)
	{	if (this->soap_own)
			soap_free(this->soap);
		this->soap = rhs.soap;
		this->soap_own = false;
		this->soap_endpoint = rhs.soap_endpoint;
	}
	return *this;
}

void EventBindingProxy::destroy()
{	soap_destroy(this->soap);
	soap_end(this->soap);
}

void EventBindingProxy::reset()
{	this->destroy();
	soap_done(this->soap);
	soap_initialize(this->soap);
	EventBindingProxy_init(SOAP_IO_DEFAULT, SOAP_IO_DEFAULT);
}

void EventBindingProxy::soap_noheader()
{	this->soap->header = NULL;
}

::SOAP_ENV__Header *EventBindingProxy::soap_header()
{	return this->soap->header;
}

::SOAP_ENV__Fault *EventBindingProxy::soap_fault()
{	return this->soap->fault;
}

const char *EventBindingProxy::soap_fault_string()
{	return *soap_faultstring(this->soap);
}

const char *EventBindingProxy::soap_fault_detail()
{	return *soap_faultdetail(this->soap);
}

int EventBindingProxy::soap_close_socket()
{	return soap_closesock(this->soap);
}

int EventBindingProxy::soap_force_close_socket()
{	return soap_force_closesock(this->soap);
}

void EventBindingProxy::soap_print_fault(FILE *fd)
{	::soap_print_fault(this->soap, fd);
}

#ifndef WITH_LEAN
#ifndef WITH_COMPAT
void EventBindingProxy::soap_stream_fault(std::ostream& os)
{	::soap_stream_fault(this->soap, os);
}
#endif

char *EventBindingProxy::soap_sprint_fault(char *buf, size_t len)
{	return ::soap_sprint_fault(this->soap, buf, len);
}
#endif

int EventBindingProxy::GetEventInstances(const char *endpoint, const char *soap_action, _ns1__GetEventInstances *ns1__GetEventInstances, _ns1__GetEventInstancesResponse &ns1__GetEventInstancesResponse)
{	struct soap *soap = this->soap;
	struct __ns1__GetEventInstances soap_tmp___ns1__GetEventInstances;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.axis.com/vapix/ws/event1/GetEventInstances";
	soap_tmp___ns1__GetEventInstances.ns1__GetEventInstances = ns1__GetEventInstances;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___ns1__GetEventInstances(soap, &soap_tmp___ns1__GetEventInstances);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___ns1__GetEventInstances(soap, &soap_tmp___ns1__GetEventInstances, "-ns1:GetEventInstances", "")
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
	 || soap_put___ns1__GetEventInstances(soap, &soap_tmp___ns1__GetEventInstances, "-ns1:GetEventInstances", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_ns1__GetEventInstancesResponse*>(&ns1__GetEventInstancesResponse)) // NULL ref?
		return soap_closesock(soap);
	ns1__GetEventInstancesResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	ns1__GetEventInstancesResponse.soap_get(soap, "ns1:GetEventInstancesResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int EventBindingProxy::AddScheduledEvent(const char *endpoint, const char *soap_action, _ns1__AddScheduledEvent *ns1__AddScheduledEvent, _ns1__AddScheduledEventResponse &ns1__AddScheduledEventResponse)
{	struct soap *soap = this->soap;
	struct __ns1__AddScheduledEvent soap_tmp___ns1__AddScheduledEvent;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.axis.com/vapix/ws/event1/AddScheduledEvent";
	soap_tmp___ns1__AddScheduledEvent.ns1__AddScheduledEvent = ns1__AddScheduledEvent;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___ns1__AddScheduledEvent(soap, &soap_tmp___ns1__AddScheduledEvent);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___ns1__AddScheduledEvent(soap, &soap_tmp___ns1__AddScheduledEvent, "-ns1:AddScheduledEvent", "")
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
	 || soap_put___ns1__AddScheduledEvent(soap, &soap_tmp___ns1__AddScheduledEvent, "-ns1:AddScheduledEvent", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_ns1__AddScheduledEventResponse*>(&ns1__AddScheduledEventResponse)) // NULL ref?
		return soap_closesock(soap);
	ns1__AddScheduledEventResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	ns1__AddScheduledEventResponse.soap_get(soap, "ns1:AddScheduledEventResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int EventBindingProxy::RemoveScheduledEvent(const char *endpoint, const char *soap_action, _ns1__RemoveScheduledEvent *ns1__RemoveScheduledEvent, _ns1__RemoveScheduledEventResponse &ns1__RemoveScheduledEventResponse)
{	struct soap *soap = this->soap;
	struct __ns1__RemoveScheduledEvent soap_tmp___ns1__RemoveScheduledEvent;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.axis.com/vapix/ws/event1/RemoveScheduledEvent";
	soap_tmp___ns1__RemoveScheduledEvent.ns1__RemoveScheduledEvent = ns1__RemoveScheduledEvent;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___ns1__RemoveScheduledEvent(soap, &soap_tmp___ns1__RemoveScheduledEvent);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___ns1__RemoveScheduledEvent(soap, &soap_tmp___ns1__RemoveScheduledEvent, "-ns1:RemoveScheduledEvent", "")
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
	 || soap_put___ns1__RemoveScheduledEvent(soap, &soap_tmp___ns1__RemoveScheduledEvent, "-ns1:RemoveScheduledEvent", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_ns1__RemoveScheduledEventResponse*>(&ns1__RemoveScheduledEventResponse)) // NULL ref?
		return soap_closesock(soap);
	ns1__RemoveScheduledEventResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	ns1__RemoveScheduledEventResponse.soap_get(soap, "ns1:RemoveScheduledEventResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int EventBindingProxy::GetScheduledEvents(const char *endpoint, const char *soap_action, _ns1__GetScheduledEvents *ns1__GetScheduledEvents, _ns1__GetScheduledEventsResponse &ns1__GetScheduledEventsResponse)
{	struct soap *soap = this->soap;
	struct __ns1__GetScheduledEvents soap_tmp___ns1__GetScheduledEvents;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.axis.com/vapix/ws/event1/GetScheduledEvents";
	soap_tmp___ns1__GetScheduledEvents.ns1__GetScheduledEvents = ns1__GetScheduledEvents;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___ns1__GetScheduledEvents(soap, &soap_tmp___ns1__GetScheduledEvents);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___ns1__GetScheduledEvents(soap, &soap_tmp___ns1__GetScheduledEvents, "-ns1:GetScheduledEvents", "")
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
	 || soap_put___ns1__GetScheduledEvents(soap, &soap_tmp___ns1__GetScheduledEvents, "-ns1:GetScheduledEvents", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_ns1__GetScheduledEventsResponse*>(&ns1__GetScheduledEventsResponse)) // NULL ref?
		return soap_closesock(soap);
	ns1__GetScheduledEventsResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	ns1__GetScheduledEventsResponse.soap_get(soap, "ns1:GetScheduledEventsResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}

int EventBindingProxy::ChangeVirtualInputState(const char *endpoint, const char *soap_action, _ns1__ChangeVirtualInputState *ns1__ChangeVirtualInputState, _ns1__ChangeVirtualInputStateResponse &ns1__ChangeVirtualInputStateResponse)
{	struct soap *soap = this->soap;
	struct __ns1__ChangeVirtualInputState soap_tmp___ns1__ChangeVirtualInputState;
	if (endpoint)
		soap_endpoint = endpoint;
	if (soap_action == NULL)
		soap_action = "http://www.axis.com/vapix/ws/event1/ChangeVirtualInputState";
	soap_tmp___ns1__ChangeVirtualInputState.ns1__ChangeVirtualInputState = ns1__ChangeVirtualInputState;
	soap_begin(soap);
	soap->encodingStyle = NULL;
	soap_serializeheader(soap);
	soap_serialize___ns1__ChangeVirtualInputState(soap, &soap_tmp___ns1__ChangeVirtualInputState);
	if (soap_begin_count(soap))
		return soap->error;
	if (soap->mode & SOAP_IO_LENGTH)
	{	if (soap_envelope_begin_out(soap)
		 || soap_putheader(soap)
		 || soap_body_begin_out(soap)
		 || soap_put___ns1__ChangeVirtualInputState(soap, &soap_tmp___ns1__ChangeVirtualInputState, "-ns1:ChangeVirtualInputState", "")
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
	 || soap_put___ns1__ChangeVirtualInputState(soap, &soap_tmp___ns1__ChangeVirtualInputState, "-ns1:ChangeVirtualInputState", "")
	 || soap_body_end_out(soap)
	 || soap_envelope_end_out(soap)
	 || soap_end_send(soap))
		return soap_closesock(soap);
	if (!static_cast<_ns1__ChangeVirtualInputStateResponse*>(&ns1__ChangeVirtualInputStateResponse)) // NULL ref?
		return soap_closesock(soap);
	ns1__ChangeVirtualInputStateResponse.soap_default(soap);
	if (soap_begin_recv(soap)
	 || soap_envelope_begin_in(soap)
	 || soap_recv_header(soap)
	 || soap_body_begin_in(soap))
		return soap_closesock(soap);
	ns1__ChangeVirtualInputStateResponse.soap_get(soap, "ns1:ChangeVirtualInputStateResponse", NULL);
	if (soap->error)
		return soap_recv_fault(soap, 0);
	if (soap_body_end_in(soap)
	 || soap_envelope_end_in(soap)
	 || soap_end_recv(soap))
		return soap_closesock(soap);
	return soap_closesock(soap);
}
/* End of client proxy code */
