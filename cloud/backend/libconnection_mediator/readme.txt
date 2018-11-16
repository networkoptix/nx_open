Network server used by two unbound hosts each behind its own NAT to establish connection using UDP hole punching.
UDP hole punching is described at http://en.wikipedia.org/wiki/UDP_hole_punching. In terms of wiki article, this is S host.

Listens public UDP port

Implemented using UDT (http://udt.sourceforge.net/)


Overall operation description:

=======================================================================================================================================================
server:                                                          |   client:
=======================================================================================================================================================
                                                                 |
creates server_sock1                                             |
                                                                 |
binds to local port                                              | 
                                                                 |
connects to nameserver                                           | 
                                                                 |
reveals its name to the nameserver                               | 
                                                                 |   creates client_sock1
                                                                 |   
                                                                 |   connects to the nameserver
                                                                 |
                                                                 |   tells it wants to connect to the server
                                                                 |
                                          (all operations above are totally independent)
                                                                 |
nameserver notifies that client wishes to                        |   nameserver responses with server's ip:port (external address of server_sock1)
establish connection and reports client's ip:port                |   
(external address of client_sock1)                               |   
                                                                 |
creates socket server_sock2 bound to the address of server_sock1 |   creates socket client_sock2 bound to the address of client_sock1
                                                                 |
initiates rendezvous connection of server_sock2 with client      |   initiates rendezvous connection of client_sock2 with server
=======================================================================================================================================================





Let H1 be host behind NAT1 which wishes accepts incoming connections
H2 behind NAT2 is a host which wants to connect to H1
S is this module

# 1. H1 registeres with S
(H1 establishes UDT connection to S)

1.1
H1->S:
GET /bind?host_id=host_sweet_host HTTP/1.1

S->H1:
HTTP/1.1 200 OK


# H1 <-> S connection is kept opened


# 2. H2 connects to H1
(H2 establishes UDT connection to S)

2.1
H2->S:
GET /connect?host_id=host_sweet_host HTTP/1.1

S->H2:
HTTP/1.1 200 OK
Content-Type: multipart/x-mixed-replace;boundary=--hx_hole_punching
Transfer-Encoding: chunked

--hx_hole_punching
host_sweet_host: h1_ip:h1_port

(end of message body)

#ip and port are known from connection "H1 <-> S"


2.3
S->H2: # connection was not closed!
--hx_hole_punching
: h2_ip:h2_port

# ip and port are known from connection "H2 <-> S". Name is empty, since H2 did not specify its name
