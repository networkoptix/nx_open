/* soapDoorControlBindingProxy.h
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

#ifndef soapDoorControlBindingProxy_H
#define soapDoorControlBindingProxy_H
#include "soapH.h"

    class SOAP_CMAC DoorControlBindingProxy {
      public:
        /// Context to manage proxy IO and data
        struct soap *soap;
        bool soap_own; ///< flag indicating that this context is owned by this proxy when context is shared
        /// Endpoint URL of service 'DoorControlBindingProxy' (change as needed)
        const char *soap_endpoint;
        /// Variables globally declared in ../result/interim/onvif.h_, if any
        /// Construct a proxy with new managing context
        DoorControlBindingProxy();
        /// Copy constructor
        DoorControlBindingProxy(const DoorControlBindingProxy& rhs);
        /// Construct proxy given a shared managing context
        DoorControlBindingProxy(struct soap*);
        /// Constructor taking an endpoint URL
        DoorControlBindingProxy(const char *endpoint);
        /// Constructor taking input and output mode flags for the new managing context
        DoorControlBindingProxy(soap_mode iomode);
        /// Constructor taking endpoint URL and input and output mode flags for the new managing context
        DoorControlBindingProxy(const char *endpoint, soap_mode iomode);
        /// Constructor taking input and output mode flags for the new managing context
        DoorControlBindingProxy(soap_mode imode, soap_mode omode);
        /// Destructor deletes non-shared managing context only (use destroy() to delete deserialized data)
        virtual ~DoorControlBindingProxy();
        /// Initializer used by constructors
        virtual void DoorControlBindingProxy_init(soap_mode imode, soap_mode omode);
        /// Return a copy that has a new managing context with the same engine state
        virtual DoorControlBindingProxy *copy();
        /// Copy assignment
        DoorControlBindingProxy& operator=(const DoorControlBindingProxy&);
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
        virtual int GetServiceCapabilities(_onvifDoorControl__GetServiceCapabilities *onvifDoorControl__GetServiceCapabilities, _onvifDoorControl__GetServiceCapabilitiesResponse &onvifDoorControl__GetServiceCapabilitiesResponse)
        { return this->GetServiceCapabilities(NULL, NULL, onvifDoorControl__GetServiceCapabilities, onvifDoorControl__GetServiceCapabilitiesResponse); }
        virtual int GetServiceCapabilities(const char *soap_endpoint, const char *soap_action, _onvifDoorControl__GetServiceCapabilities *onvifDoorControl__GetServiceCapabilities, _onvifDoorControl__GetServiceCapabilitiesResponse &onvifDoorControl__GetServiceCapabilitiesResponse);
        /// Web service operation 'GetDoorInfoList' (returns SOAP_OK or error code)
        virtual int GetDoorInfoList(_onvifDoorControl__GetDoorInfoList *onvifDoorControl__GetDoorInfoList, _onvifDoorControl__GetDoorInfoListResponse &onvifDoorControl__GetDoorInfoListResponse)
        { return this->GetDoorInfoList(NULL, NULL, onvifDoorControl__GetDoorInfoList, onvifDoorControl__GetDoorInfoListResponse); }
        virtual int GetDoorInfoList(const char *soap_endpoint, const char *soap_action, _onvifDoorControl__GetDoorInfoList *onvifDoorControl__GetDoorInfoList, _onvifDoorControl__GetDoorInfoListResponse &onvifDoorControl__GetDoorInfoListResponse);
        /// Web service operation 'GetDoorInfo' (returns SOAP_OK or error code)
        virtual int GetDoorInfo(_onvifDoorControl__GetDoorInfo *onvifDoorControl__GetDoorInfo, _onvifDoorControl__GetDoorInfoResponse &onvifDoorControl__GetDoorInfoResponse)
        { return this->GetDoorInfo(NULL, NULL, onvifDoorControl__GetDoorInfo, onvifDoorControl__GetDoorInfoResponse); }
        virtual int GetDoorInfo(const char *soap_endpoint, const char *soap_action, _onvifDoorControl__GetDoorInfo *onvifDoorControl__GetDoorInfo, _onvifDoorControl__GetDoorInfoResponse &onvifDoorControl__GetDoorInfoResponse);
        /// Web service operation 'GetDoorList' (returns SOAP_OK or error code)
        virtual int GetDoorList(_onvifDoorControl__GetDoorList *onvifDoorControl__GetDoorList, _onvifDoorControl__GetDoorListResponse &onvifDoorControl__GetDoorListResponse)
        { return this->GetDoorList(NULL, NULL, onvifDoorControl__GetDoorList, onvifDoorControl__GetDoorListResponse); }
        virtual int GetDoorList(const char *soap_endpoint, const char *soap_action, _onvifDoorControl__GetDoorList *onvifDoorControl__GetDoorList, _onvifDoorControl__GetDoorListResponse &onvifDoorControl__GetDoorListResponse);
        /// Web service operation 'GetDoors' (returns SOAP_OK or error code)
        virtual int GetDoors(_onvifDoorControl__GetDoors *onvifDoorControl__GetDoors, _onvifDoorControl__GetDoorsResponse &onvifDoorControl__GetDoorsResponse)
        { return this->GetDoors(NULL, NULL, onvifDoorControl__GetDoors, onvifDoorControl__GetDoorsResponse); }
        virtual int GetDoors(const char *soap_endpoint, const char *soap_action, _onvifDoorControl__GetDoors *onvifDoorControl__GetDoors, _onvifDoorControl__GetDoorsResponse &onvifDoorControl__GetDoorsResponse);
        /// Web service operation 'CreateDoor' (returns SOAP_OK or error code)
        virtual int CreateDoor(_onvifDoorControl__CreateDoor *onvifDoorControl__CreateDoor, _onvifDoorControl__CreateDoorResponse &onvifDoorControl__CreateDoorResponse)
        { return this->CreateDoor(NULL, NULL, onvifDoorControl__CreateDoor, onvifDoorControl__CreateDoorResponse); }
        virtual int CreateDoor(const char *soap_endpoint, const char *soap_action, _onvifDoorControl__CreateDoor *onvifDoorControl__CreateDoor, _onvifDoorControl__CreateDoorResponse &onvifDoorControl__CreateDoorResponse);
        /// Web service operation 'SetDoor' (returns SOAP_OK or error code)
        virtual int SetDoor(_onvifDoorControl__SetDoor *onvifDoorControl__SetDoor, _onvifDoorControl__SetDoorResponse &onvifDoorControl__SetDoorResponse)
        { return this->SetDoor(NULL, NULL, onvifDoorControl__SetDoor, onvifDoorControl__SetDoorResponse); }
        virtual int SetDoor(const char *soap_endpoint, const char *soap_action, _onvifDoorControl__SetDoor *onvifDoorControl__SetDoor, _onvifDoorControl__SetDoorResponse &onvifDoorControl__SetDoorResponse);
        /// Web service operation 'ModifyDoor' (returns SOAP_OK or error code)
        virtual int ModifyDoor(_onvifDoorControl__ModifyDoor *onvifDoorControl__ModifyDoor, _onvifDoorControl__ModifyDoorResponse &onvifDoorControl__ModifyDoorResponse)
        { return this->ModifyDoor(NULL, NULL, onvifDoorControl__ModifyDoor, onvifDoorControl__ModifyDoorResponse); }
        virtual int ModifyDoor(const char *soap_endpoint, const char *soap_action, _onvifDoorControl__ModifyDoor *onvifDoorControl__ModifyDoor, _onvifDoorControl__ModifyDoorResponse &onvifDoorControl__ModifyDoorResponse);
        /// Web service operation 'DeleteDoor' (returns SOAP_OK or error code)
        virtual int DeleteDoor(_onvifDoorControl__DeleteDoor *onvifDoorControl__DeleteDoor, _onvifDoorControl__DeleteDoorResponse &onvifDoorControl__DeleteDoorResponse)
        { return this->DeleteDoor(NULL, NULL, onvifDoorControl__DeleteDoor, onvifDoorControl__DeleteDoorResponse); }
        virtual int DeleteDoor(const char *soap_endpoint, const char *soap_action, _onvifDoorControl__DeleteDoor *onvifDoorControl__DeleteDoor, _onvifDoorControl__DeleteDoorResponse &onvifDoorControl__DeleteDoorResponse);
        /// Web service operation 'GetDoorState' (returns SOAP_OK or error code)
        virtual int GetDoorState(_onvifDoorControl__GetDoorState *onvifDoorControl__GetDoorState, _onvifDoorControl__GetDoorStateResponse &onvifDoorControl__GetDoorStateResponse)
        { return this->GetDoorState(NULL, NULL, onvifDoorControl__GetDoorState, onvifDoorControl__GetDoorStateResponse); }
        virtual int GetDoorState(const char *soap_endpoint, const char *soap_action, _onvifDoorControl__GetDoorState *onvifDoorControl__GetDoorState, _onvifDoorControl__GetDoorStateResponse &onvifDoorControl__GetDoorStateResponse);
        /// Web service operation 'AccessDoor' (returns SOAP_OK or error code)
        virtual int AccessDoor(_onvifDoorControl__AccessDoor *onvifDoorControl__AccessDoor, _onvifDoorControl__AccessDoorResponse &onvifDoorControl__AccessDoorResponse)
        { return this->AccessDoor(NULL, NULL, onvifDoorControl__AccessDoor, onvifDoorControl__AccessDoorResponse); }
        virtual int AccessDoor(const char *soap_endpoint, const char *soap_action, _onvifDoorControl__AccessDoor *onvifDoorControl__AccessDoor, _onvifDoorControl__AccessDoorResponse &onvifDoorControl__AccessDoorResponse);
        /// Web service operation 'LockDoor' (returns SOAP_OK or error code)
        virtual int LockDoor(_onvifDoorControl__LockDoor *onvifDoorControl__LockDoor, _onvifDoorControl__LockDoorResponse &onvifDoorControl__LockDoorResponse)
        { return this->LockDoor(NULL, NULL, onvifDoorControl__LockDoor, onvifDoorControl__LockDoorResponse); }
        virtual int LockDoor(const char *soap_endpoint, const char *soap_action, _onvifDoorControl__LockDoor *onvifDoorControl__LockDoor, _onvifDoorControl__LockDoorResponse &onvifDoorControl__LockDoorResponse);
        /// Web service operation 'UnlockDoor' (returns SOAP_OK or error code)
        virtual int UnlockDoor(_onvifDoorControl__UnlockDoor *onvifDoorControl__UnlockDoor, _onvifDoorControl__UnlockDoorResponse &onvifDoorControl__UnlockDoorResponse)
        { return this->UnlockDoor(NULL, NULL, onvifDoorControl__UnlockDoor, onvifDoorControl__UnlockDoorResponse); }
        virtual int UnlockDoor(const char *soap_endpoint, const char *soap_action, _onvifDoorControl__UnlockDoor *onvifDoorControl__UnlockDoor, _onvifDoorControl__UnlockDoorResponse &onvifDoorControl__UnlockDoorResponse);
        /// Web service operation 'BlockDoor' (returns SOAP_OK or error code)
        virtual int BlockDoor(_onvifDoorControl__BlockDoor *onvifDoorControl__BlockDoor, _onvifDoorControl__BlockDoorResponse &onvifDoorControl__BlockDoorResponse)
        { return this->BlockDoor(NULL, NULL, onvifDoorControl__BlockDoor, onvifDoorControl__BlockDoorResponse); }
        virtual int BlockDoor(const char *soap_endpoint, const char *soap_action, _onvifDoorControl__BlockDoor *onvifDoorControl__BlockDoor, _onvifDoorControl__BlockDoorResponse &onvifDoorControl__BlockDoorResponse);
        /// Web service operation 'LockDownDoor' (returns SOAP_OK or error code)
        virtual int LockDownDoor(_onvifDoorControl__LockDownDoor *onvifDoorControl__LockDownDoor, _onvifDoorControl__LockDownDoorResponse &onvifDoorControl__LockDownDoorResponse)
        { return this->LockDownDoor(NULL, NULL, onvifDoorControl__LockDownDoor, onvifDoorControl__LockDownDoorResponse); }
        virtual int LockDownDoor(const char *soap_endpoint, const char *soap_action, _onvifDoorControl__LockDownDoor *onvifDoorControl__LockDownDoor, _onvifDoorControl__LockDownDoorResponse &onvifDoorControl__LockDownDoorResponse);
        /// Web service operation 'LockDownReleaseDoor' (returns SOAP_OK or error code)
        virtual int LockDownReleaseDoor(_onvifDoorControl__LockDownReleaseDoor *onvifDoorControl__LockDownReleaseDoor, _onvifDoorControl__LockDownReleaseDoorResponse &onvifDoorControl__LockDownReleaseDoorResponse)
        { return this->LockDownReleaseDoor(NULL, NULL, onvifDoorControl__LockDownReleaseDoor, onvifDoorControl__LockDownReleaseDoorResponse); }
        virtual int LockDownReleaseDoor(const char *soap_endpoint, const char *soap_action, _onvifDoorControl__LockDownReleaseDoor *onvifDoorControl__LockDownReleaseDoor, _onvifDoorControl__LockDownReleaseDoorResponse &onvifDoorControl__LockDownReleaseDoorResponse);
        /// Web service operation 'LockOpenDoor' (returns SOAP_OK or error code)
        virtual int LockOpenDoor(_onvifDoorControl__LockOpenDoor *onvifDoorControl__LockOpenDoor, _onvifDoorControl__LockOpenDoorResponse &onvifDoorControl__LockOpenDoorResponse)
        { return this->LockOpenDoor(NULL, NULL, onvifDoorControl__LockOpenDoor, onvifDoorControl__LockOpenDoorResponse); }
        virtual int LockOpenDoor(const char *soap_endpoint, const char *soap_action, _onvifDoorControl__LockOpenDoor *onvifDoorControl__LockOpenDoor, _onvifDoorControl__LockOpenDoorResponse &onvifDoorControl__LockOpenDoorResponse);
        /// Web service operation 'LockOpenReleaseDoor' (returns SOAP_OK or error code)
        virtual int LockOpenReleaseDoor(_onvifDoorControl__LockOpenReleaseDoor *onvifDoorControl__LockOpenReleaseDoor, _onvifDoorControl__LockOpenReleaseDoorResponse &onvifDoorControl__LockOpenReleaseDoorResponse)
        { return this->LockOpenReleaseDoor(NULL, NULL, onvifDoorControl__LockOpenReleaseDoor, onvifDoorControl__LockOpenReleaseDoorResponse); }
        virtual int LockOpenReleaseDoor(const char *soap_endpoint, const char *soap_action, _onvifDoorControl__LockOpenReleaseDoor *onvifDoorControl__LockOpenReleaseDoor, _onvifDoorControl__LockOpenReleaseDoorResponse &onvifDoorControl__LockOpenReleaseDoorResponse);
        /// Web service operation 'DoubleLockDoor' (returns SOAP_OK or error code)
        virtual int DoubleLockDoor(_onvifDoorControl__DoubleLockDoor *onvifDoorControl__DoubleLockDoor, _onvifDoorControl__DoubleLockDoorResponse &onvifDoorControl__DoubleLockDoorResponse)
        { return this->DoubleLockDoor(NULL, NULL, onvifDoorControl__DoubleLockDoor, onvifDoorControl__DoubleLockDoorResponse); }
        virtual int DoubleLockDoor(const char *soap_endpoint, const char *soap_action, _onvifDoorControl__DoubleLockDoor *onvifDoorControl__DoubleLockDoor, _onvifDoorControl__DoubleLockDoorResponse &onvifDoorControl__DoubleLockDoorResponse);
    };
#endif
