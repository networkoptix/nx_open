/* soapSearchBindingProxy.h
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

#ifndef soapSearchBindingProxy_H
#define soapSearchBindingProxy_H
#include "soapH.h"

    class SOAP_CMAC SearchBindingProxy {
      public:
        /// Context to manage proxy IO and data
        struct soap *soap;
        bool soap_own; ///< flag indicating that this context is owned by this proxy when context is shared
        /// Endpoint URL of service 'SearchBindingProxy' (change as needed)
        const char *soap_endpoint;
        /// Variables globally declared in ../result/interim/onvif.h_, if any
        /// Construct a proxy with new managing context
        SearchBindingProxy();
        /// Copy constructor
        SearchBindingProxy(const SearchBindingProxy& rhs);
        /// Construct proxy given a shared managing context
        SearchBindingProxy(struct soap*);
        /// Constructor taking an endpoint URL
        SearchBindingProxy(const char *endpoint);
        /// Constructor taking input and output mode flags for the new managing context
        SearchBindingProxy(soap_mode iomode);
        /// Constructor taking endpoint URL and input and output mode flags for the new managing context
        SearchBindingProxy(const char *endpoint, soap_mode iomode);
        /// Constructor taking input and output mode flags for the new managing context
        SearchBindingProxy(soap_mode imode, soap_mode omode);
        /// Destructor deletes non-shared managing context only (use destroy() to delete deserialized data)
        virtual ~SearchBindingProxy();
        /// Initializer used by constructors
        virtual void SearchBindingProxy_init(soap_mode imode, soap_mode omode);
        /// Return a copy that has a new managing context with the same engine state
        virtual SearchBindingProxy *copy();
        /// Copy assignment
        SearchBindingProxy& operator=(const SearchBindingProxy&);
        /// Delete all deserialized data (uses soap_destroy() and soap_end())
        virtual void destroy();
        /// Delete all deserialized data and reset to default
        virtual void reset();
        /// Disables and removes SOAP Header from message by setting soap->header = NULL
        virtual void soap_noheader();
        /// Add SOAP Header to message
        virtual void soap_header(char *wsa5__MessageID, struct wsa5__RelatesToType *wsa5__RelatesTo, struct wsa5__EndpointReferenceType *wsa5__From, struct wsa5__EndpointReferenceType *wsa5__ReplyTo, struct wsa5__EndpointReferenceType *wsa5__FaultTo, char *wsa5__To, char *wsa5__Action, struct chan__ChannelInstanceType *chan__ChannelInstance, struct wsdd__AppSequenceType *wsdd__AppSequence, struct _wsse__Security *wsse__Security, char *subscriptionID);
        /// Get SOAP Header structure (i.e. soap->header, which is NULL when absent)
        virtual ::SOAP_ENV__Header *soap_header();
        /// Get SOAP Fault structure (i.e. soap->fault, which is NULL when absent)
        virtual ::SOAP_ENV__Fault *soap_fault();
        /// Get SOAP Fault string (NULL when absent)
        virtual const char *soap_fault_string();
        /// Get SOAP Fault detail as string (NULL when absent)
        virtual const char *soap_fault_detail();
        /// Close connection (normally automatic, except for send_X ops)
        virtual int soap_close_socket();
        /// Force close connection (can kill a thread blocked on IO)
        virtual int soap_force_close_socket();
        /// Print fault
        virtual void soap_print_fault(FILE*);
    #ifndef WITH_LEAN
    #ifndef WITH_COMPAT
        /// Print fault to stream
        virtual void soap_stream_fault(std::ostream&);
    #endif
        /// Write fault to buffer
        virtual char *soap_sprint_fault(char *buf, size_t len);
    #endif
        /// Web service operation 'GetServiceCapabilities' (returns SOAP_OK or error code)
        virtual int GetServiceCapabilities(_onvifSearch__GetServiceCapabilities *onvifSearch__GetServiceCapabilities, _onvifSearch__GetServiceCapabilitiesResponse &onvifSearch__GetServiceCapabilitiesResponse)
        { return this->GetServiceCapabilities(NULL, NULL, onvifSearch__GetServiceCapabilities, onvifSearch__GetServiceCapabilitiesResponse); }
        virtual int GetServiceCapabilities(const char *soap_endpoint, const char *soap_action, _onvifSearch__GetServiceCapabilities *onvifSearch__GetServiceCapabilities, _onvifSearch__GetServiceCapabilitiesResponse &onvifSearch__GetServiceCapabilitiesResponse);
        /// Web service operation 'GetRecordingSummary' (returns SOAP_OK or error code)
        virtual int GetRecordingSummary(_onvifSearch__GetRecordingSummary *onvifSearch__GetRecordingSummary, _onvifSearch__GetRecordingSummaryResponse &onvifSearch__GetRecordingSummaryResponse)
        { return this->GetRecordingSummary(NULL, NULL, onvifSearch__GetRecordingSummary, onvifSearch__GetRecordingSummaryResponse); }
        virtual int GetRecordingSummary(const char *soap_endpoint, const char *soap_action, _onvifSearch__GetRecordingSummary *onvifSearch__GetRecordingSummary, _onvifSearch__GetRecordingSummaryResponse &onvifSearch__GetRecordingSummaryResponse);
        /// Web service operation 'GetRecordingInformation' (returns SOAP_OK or error code)
        virtual int GetRecordingInformation(_onvifSearch__GetRecordingInformation *onvifSearch__GetRecordingInformation, _onvifSearch__GetRecordingInformationResponse &onvifSearch__GetRecordingInformationResponse)
        { return this->GetRecordingInformation(NULL, NULL, onvifSearch__GetRecordingInformation, onvifSearch__GetRecordingInformationResponse); }
        virtual int GetRecordingInformation(const char *soap_endpoint, const char *soap_action, _onvifSearch__GetRecordingInformation *onvifSearch__GetRecordingInformation, _onvifSearch__GetRecordingInformationResponse &onvifSearch__GetRecordingInformationResponse);
        /// Web service operation 'GetMediaAttributes' (returns SOAP_OK or error code)
        virtual int GetMediaAttributes(_onvifSearch__GetMediaAttributes *onvifSearch__GetMediaAttributes, _onvifSearch__GetMediaAttributesResponse &onvifSearch__GetMediaAttributesResponse)
        { return this->GetMediaAttributes(NULL, NULL, onvifSearch__GetMediaAttributes, onvifSearch__GetMediaAttributesResponse); }
        virtual int GetMediaAttributes(const char *soap_endpoint, const char *soap_action, _onvifSearch__GetMediaAttributes *onvifSearch__GetMediaAttributes, _onvifSearch__GetMediaAttributesResponse &onvifSearch__GetMediaAttributesResponse);
        /// Web service operation 'FindRecordings' (returns SOAP_OK or error code)
        virtual int FindRecordings(_onvifSearch__FindRecordings *onvifSearch__FindRecordings, _onvifSearch__FindRecordingsResponse &onvifSearch__FindRecordingsResponse)
        { return this->FindRecordings(NULL, NULL, onvifSearch__FindRecordings, onvifSearch__FindRecordingsResponse); }
        virtual int FindRecordings(const char *soap_endpoint, const char *soap_action, _onvifSearch__FindRecordings *onvifSearch__FindRecordings, _onvifSearch__FindRecordingsResponse &onvifSearch__FindRecordingsResponse);
        /// Web service operation 'GetRecordingSearchResults' (returns SOAP_OK or error code)
        virtual int GetRecordingSearchResults(_onvifSearch__GetRecordingSearchResults *onvifSearch__GetRecordingSearchResults, _onvifSearch__GetRecordingSearchResultsResponse &onvifSearch__GetRecordingSearchResultsResponse)
        { return this->GetRecordingSearchResults(NULL, NULL, onvifSearch__GetRecordingSearchResults, onvifSearch__GetRecordingSearchResultsResponse); }
        virtual int GetRecordingSearchResults(const char *soap_endpoint, const char *soap_action, _onvifSearch__GetRecordingSearchResults *onvifSearch__GetRecordingSearchResults, _onvifSearch__GetRecordingSearchResultsResponse &onvifSearch__GetRecordingSearchResultsResponse);
        /// Web service operation 'FindEvents' (returns SOAP_OK or error code)
        virtual int FindEvents(_onvifSearch__FindEvents *onvifSearch__FindEvents, _onvifSearch__FindEventsResponse &onvifSearch__FindEventsResponse)
        { return this->FindEvents(NULL, NULL, onvifSearch__FindEvents, onvifSearch__FindEventsResponse); }
        virtual int FindEvents(const char *soap_endpoint, const char *soap_action, _onvifSearch__FindEvents *onvifSearch__FindEvents, _onvifSearch__FindEventsResponse &onvifSearch__FindEventsResponse);
        /// Web service operation 'GetEventSearchResults' (returns SOAP_OK or error code)
        virtual int GetEventSearchResults(_onvifSearch__GetEventSearchResults *onvifSearch__GetEventSearchResults, _onvifSearch__GetEventSearchResultsResponse &onvifSearch__GetEventSearchResultsResponse)
        { return this->GetEventSearchResults(NULL, NULL, onvifSearch__GetEventSearchResults, onvifSearch__GetEventSearchResultsResponse); }
        virtual int GetEventSearchResults(const char *soap_endpoint, const char *soap_action, _onvifSearch__GetEventSearchResults *onvifSearch__GetEventSearchResults, _onvifSearch__GetEventSearchResultsResponse &onvifSearch__GetEventSearchResultsResponse);
        /// Web service operation 'FindPTZPosition' (returns SOAP_OK or error code)
        virtual int FindPTZPosition(_onvifSearch__FindPTZPosition *onvifSearch__FindPTZPosition, _onvifSearch__FindPTZPositionResponse &onvifSearch__FindPTZPositionResponse)
        { return this->FindPTZPosition(NULL, NULL, onvifSearch__FindPTZPosition, onvifSearch__FindPTZPositionResponse); }
        virtual int FindPTZPosition(const char *soap_endpoint, const char *soap_action, _onvifSearch__FindPTZPosition *onvifSearch__FindPTZPosition, _onvifSearch__FindPTZPositionResponse &onvifSearch__FindPTZPositionResponse);
        /// Web service operation 'GetPTZPositionSearchResults' (returns SOAP_OK or error code)
        virtual int GetPTZPositionSearchResults(_onvifSearch__GetPTZPositionSearchResults *onvifSearch__GetPTZPositionSearchResults, _onvifSearch__GetPTZPositionSearchResultsResponse &onvifSearch__GetPTZPositionSearchResultsResponse)
        { return this->GetPTZPositionSearchResults(NULL, NULL, onvifSearch__GetPTZPositionSearchResults, onvifSearch__GetPTZPositionSearchResultsResponse); }
        virtual int GetPTZPositionSearchResults(const char *soap_endpoint, const char *soap_action, _onvifSearch__GetPTZPositionSearchResults *onvifSearch__GetPTZPositionSearchResults, _onvifSearch__GetPTZPositionSearchResultsResponse &onvifSearch__GetPTZPositionSearchResultsResponse);
        /// Web service operation 'GetSearchState' (returns SOAP_OK or error code)
        virtual int GetSearchState(_onvifSearch__GetSearchState *onvifSearch__GetSearchState, _onvifSearch__GetSearchStateResponse &onvifSearch__GetSearchStateResponse)
        { return this->GetSearchState(NULL, NULL, onvifSearch__GetSearchState, onvifSearch__GetSearchStateResponse); }
        virtual int GetSearchState(const char *soap_endpoint, const char *soap_action, _onvifSearch__GetSearchState *onvifSearch__GetSearchState, _onvifSearch__GetSearchStateResponse &onvifSearch__GetSearchStateResponse);
        /// Web service operation 'EndSearch' (returns SOAP_OK or error code)
        virtual int EndSearch(_onvifSearch__EndSearch *onvifSearch__EndSearch, _onvifSearch__EndSearchResponse &onvifSearch__EndSearchResponse)
        { return this->EndSearch(NULL, NULL, onvifSearch__EndSearch, onvifSearch__EndSearchResponse); }
        virtual int EndSearch(const char *soap_endpoint, const char *soap_action, _onvifSearch__EndSearch *onvifSearch__EndSearch, _onvifSearch__EndSearchResponse &onvifSearch__EndSearchResponse);
        /// Web service operation 'FindMetadata' (returns SOAP_OK or error code)
        virtual int FindMetadata(_onvifSearch__FindMetadata *onvifSearch__FindMetadata, _onvifSearch__FindMetadataResponse &onvifSearch__FindMetadataResponse)
        { return this->FindMetadata(NULL, NULL, onvifSearch__FindMetadata, onvifSearch__FindMetadataResponse); }
        virtual int FindMetadata(const char *soap_endpoint, const char *soap_action, _onvifSearch__FindMetadata *onvifSearch__FindMetadata, _onvifSearch__FindMetadataResponse &onvifSearch__FindMetadataResponse);
        /// Web service operation 'GetMetadataSearchResults' (returns SOAP_OK or error code)
        virtual int GetMetadataSearchResults(_onvifSearch__GetMetadataSearchResults *onvifSearch__GetMetadataSearchResults, _onvifSearch__GetMetadataSearchResultsResponse &onvifSearch__GetMetadataSearchResultsResponse)
        { return this->GetMetadataSearchResults(NULL, NULL, onvifSearch__GetMetadataSearchResults, onvifSearch__GetMetadataSearchResultsResponse); }
        virtual int GetMetadataSearchResults(const char *soap_endpoint, const char *soap_action, _onvifSearch__GetMetadataSearchResults *onvifSearch__GetMetadataSearchResults, _onvifSearch__GetMetadataSearchResultsResponse &onvifSearch__GetMetadataSearchResultsResponse);
    };
#endif
