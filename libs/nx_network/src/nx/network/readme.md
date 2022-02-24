# nx_network library {#nx_network}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

## Functionality
Framework for creating a wide range of asynchronous network applications such as:
- Secure HTTP REST API server
- HTTP client applications
- lower-level applications that use TCP/UDP sockets directly with optional SSL support

## Implemented network protocols
- HTTP/1.1 (client/server) (See @ref nx_network_http)
- Websockets
- STUN (rfc5389)
- UPnP
- TIME protocol (rfc868)

## Other functionality
- Cloud connectivity (See @ref nx_network_cloud_connect)
- Asynchronous I/O framework (@ref nx_network_aio)

@subpage nx_network_aio
@subpage nx_network_http
@subpage nx_network_cloud_connect
