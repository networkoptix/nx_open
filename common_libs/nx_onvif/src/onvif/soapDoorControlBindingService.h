/* soapDoorControlBindingService.h
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

#ifndef soapDoorControlBindingService_H
#define soapDoorControlBindingService_H
#include "soapH.h"

    class SOAP_CMAC DoorControlBindingService {
      public:
        /// Context to manage service IO and data
        struct soap *soap;
        bool soap_own;  ///< flag indicating that this context is owned by this service when context is shared
        /// Variables globally declared in ../result/interim/onvif.h_, if any
        /// Construct a service with new managing context
        DoorControlBindingService();
        /// Copy constructor
        DoorControlBindingService(const DoorControlBindingService&);
        /// Construct service given a shared managing context
        DoorControlBindingService(struct soap*);
        /// Constructor taking input+output mode flags for the new managing context
        DoorControlBindingService(soap_mode iomode);
        /// Constructor taking input and output mode flags for the new managing context
        DoorControlBindingService(soap_mode imode, soap_mode omode);
        /// Destructor deletes non-shared managing context only (use destroy() to delete deserialized data)
        virtual ~DoorControlBindingService();
        /// Delete all deserialized data (with soap_destroy() and soap_end())
        virtual void destroy();
        /// Delete all deserialized data and reset to defaults
        virtual void reset();
        /// Initializer used by constructors
        virtual void DoorControlBindingService_init(soap_mode imode, soap_mode omode);
        /// Return a copy that has a new managing context with the same engine state
        virtual DoorControlBindingService *copy() SOAP_PURE_VIRTUAL_COPY;
        /// Copy assignment
        DoorControlBindingService& operator=(const DoorControlBindingService&);
        /// Close connection (normally automatic)
        virtual int soap_close_socket();
        /// Force close connection (can kill a thread blocked on IO)
        virtual int soap_force_close_socket();
        /// Return sender-related fault to sender
        virtual int soap_senderfault(const char *string, const char *detailXML);
        /// Return sender-related fault with SOAP 1.2 subcode to sender
        virtual int soap_senderfault(const char *subcodeQName, const char *string, const char *detailXML);
        /// Return receiver-related fault to sender
        virtual int soap_receiverfault(const char *string, const char *detailXML);
        /// Return receiver-related fault with SOAP 1.2 subcode to sender
        virtual int soap_receiverfault(const char *subcodeQName, const char *string, const char *detailXML);
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
        /// Disables and removes SOAP Header from message by setting soap->header = NULL
        virtual void soap_noheader();
        /// Add SOAP Header to message
        virtual void soap_header(char *wsa5__MessageID, struct wsa5__RelatesToType *wsa5__RelatesTo, struct wsa5__EndpointReferenceType *wsa5__From, struct wsa5__EndpointReferenceType *wsa5__ReplyTo, struct wsa5__EndpointReferenceType *wsa5__FaultTo, char *wsa5__To, char *wsa5__Action, struct chan__ChannelInstanceType *chan__ChannelInstance, struct wsdd__AppSequenceType *wsdd__AppSequence, struct _wsse__Security *wsse__Security, char *subscriptionID);
        /// Get SOAP Header structure (i.e. soap->header, which is NULL when absent)
        virtual ::SOAP_ENV__Header *soap_header();
    #ifndef WITH_NOIO
        /// Run simple single-thread (iterative, non-SSL) service on port until a connection error occurs (returns SOAP_OK or error code), use this->bind_flag = SO_REUSEADDR to rebind for immediate rerun
        virtual int run(int port);
    #if defined(WITH_OPENSSL) || defined(WITH_GNUTLS)
        /// Run simple single-thread SSL service on port until a connection error occurs (returns SOAP_OK or error code), use this->bind_flag = SO_REUSEADDR to rebind for immediate rerun
        virtual int ssl_run(int port);
    #endif
        /// Bind service to port (returns master socket or SOAP_INVALID_SOCKET)
        virtual SOAP_SOCKET bind(const char *host, int port, int backlog);
        /// Accept next request (returns socket or SOAP_INVALID_SOCKET)
        virtual SOAP_SOCKET accept();
    #if defined(WITH_OPENSSL) || defined(WITH_GNUTLS)
        /// When SSL is used, after accept() should perform and accept SSL handshake
        virtual int ssl_accept();
    #endif
    #endif
        /// After accept() serve this request (returns SOAP_OK or error code)
        virtual int serve();
        /// Used by serve() to dispatch a request (returns SOAP_OK or error code)
        virtual int dispatch();
        virtual int dispatch(struct soap *soap);
        ///
        /// Service operations are listed below (you should define these)
        /// Note: compile with -DWITH_PURE_VIRTUAL for pure virtual methods
        ///
        /// Web service operation 'GetServiceCapabilities' (returns SOAP_OK or error code)
        virtual int GetServiceCapabilities(_onvifDoorControl__GetServiceCapabilities *onvifDoorControl__GetServiceCapabilities, _onvifDoorControl__GetServiceCapabilitiesResponse &onvifDoorControl__GetServiceCapabilitiesResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetDoorInfoList' (returns SOAP_OK or error code)
        virtual int GetDoorInfoList(_onvifDoorControl__GetDoorInfoList *onvifDoorControl__GetDoorInfoList, _onvifDoorControl__GetDoorInfoListResponse &onvifDoorControl__GetDoorInfoListResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetDoorInfo' (returns SOAP_OK or error code)
        virtual int GetDoorInfo(_onvifDoorControl__GetDoorInfo *onvifDoorControl__GetDoorInfo, _onvifDoorControl__GetDoorInfoResponse &onvifDoorControl__GetDoorInfoResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetDoorList' (returns SOAP_OK or error code)
        virtual int GetDoorList(_onvifDoorControl__GetDoorList *onvifDoorControl__GetDoorList, _onvifDoorControl__GetDoorListResponse &onvifDoorControl__GetDoorListResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetDoors' (returns SOAP_OK or error code)
        virtual int GetDoors(_onvifDoorControl__GetDoors *onvifDoorControl__GetDoors, _onvifDoorControl__GetDoorsResponse &onvifDoorControl__GetDoorsResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'CreateDoor' (returns SOAP_OK or error code)
        virtual int CreateDoor(_onvifDoorControl__CreateDoor *onvifDoorControl__CreateDoor, _onvifDoorControl__CreateDoorResponse &onvifDoorControl__CreateDoorResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'SetDoor' (returns SOAP_OK or error code)
        virtual int SetDoor(_onvifDoorControl__SetDoor *onvifDoorControl__SetDoor, _onvifDoorControl__SetDoorResponse &onvifDoorControl__SetDoorResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'ModifyDoor' (returns SOAP_OK or error code)
        virtual int ModifyDoor(_onvifDoorControl__ModifyDoor *onvifDoorControl__ModifyDoor, _onvifDoorControl__ModifyDoorResponse &onvifDoorControl__ModifyDoorResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'DeleteDoor' (returns SOAP_OK or error code)
        virtual int DeleteDoor(_onvifDoorControl__DeleteDoor *onvifDoorControl__DeleteDoor, _onvifDoorControl__DeleteDoorResponse &onvifDoorControl__DeleteDoorResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'GetDoorState' (returns SOAP_OK or error code)
        virtual int GetDoorState(_onvifDoorControl__GetDoorState *onvifDoorControl__GetDoorState, _onvifDoorControl__GetDoorStateResponse &onvifDoorControl__GetDoorStateResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'AccessDoor' (returns SOAP_OK or error code)
        virtual int AccessDoor(_onvifDoorControl__AccessDoor *onvifDoorControl__AccessDoor, _onvifDoorControl__AccessDoorResponse &onvifDoorControl__AccessDoorResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'LockDoor' (returns SOAP_OK or error code)
        virtual int LockDoor(_onvifDoorControl__LockDoor *onvifDoorControl__LockDoor, _onvifDoorControl__LockDoorResponse &onvifDoorControl__LockDoorResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'UnlockDoor' (returns SOAP_OK or error code)
        virtual int UnlockDoor(_onvifDoorControl__UnlockDoor *onvifDoorControl__UnlockDoor, _onvifDoorControl__UnlockDoorResponse &onvifDoorControl__UnlockDoorResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'BlockDoor' (returns SOAP_OK or error code)
        virtual int BlockDoor(_onvifDoorControl__BlockDoor *onvifDoorControl__BlockDoor, _onvifDoorControl__BlockDoorResponse &onvifDoorControl__BlockDoorResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'LockDownDoor' (returns SOAP_OK or error code)
        virtual int LockDownDoor(_onvifDoorControl__LockDownDoor *onvifDoorControl__LockDownDoor, _onvifDoorControl__LockDownDoorResponse &onvifDoorControl__LockDownDoorResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'LockDownReleaseDoor' (returns SOAP_OK or error code)
        virtual int LockDownReleaseDoor(_onvifDoorControl__LockDownReleaseDoor *onvifDoorControl__LockDownReleaseDoor, _onvifDoorControl__LockDownReleaseDoorResponse &onvifDoorControl__LockDownReleaseDoorResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'LockOpenDoor' (returns SOAP_OK or error code)
        virtual int LockOpenDoor(_onvifDoorControl__LockOpenDoor *onvifDoorControl__LockOpenDoor, _onvifDoorControl__LockOpenDoorResponse &onvifDoorControl__LockOpenDoorResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'LockOpenReleaseDoor' (returns SOAP_OK or error code)
        virtual int LockOpenReleaseDoor(_onvifDoorControl__LockOpenReleaseDoor *onvifDoorControl__LockOpenReleaseDoor, _onvifDoorControl__LockOpenReleaseDoorResponse &onvifDoorControl__LockOpenReleaseDoorResponse) SOAP_PURE_VIRTUAL;
        /// Web service operation 'DoubleLockDoor' (returns SOAP_OK or error code)
        virtual int DoubleLockDoor(_onvifDoorControl__DoubleLockDoor *onvifDoorControl__DoubleLockDoor, _onvifDoorControl__DoubleLockDoorResponse &onvifDoorControl__DoubleLockDoorResponse) SOAP_PURE_VIRTUAL;
    };
#endif
