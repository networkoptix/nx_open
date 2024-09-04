/*
 * Copyright (C) 2011, 2012, 2013 Citrix Systems
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __TLS_LISTENER__
#define __TLS_LISTENER__

#include <event2/event.h>

#include "ns_turn_utils.h"

#include "ns_ioalib_impl.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////

struct tls_listener_relay_server_info;
typedef struct tls_listener_relay_server_info tls_listener_relay_server_type;

///////////////////////////////////////////

tls_listener_relay_server_type *create_tls_listener_server(const char *ifname, const char *local_address, int port,
                                                           int verbose, ioa_engine_handle e,
                                                           ioa_engine_new_connection_event_handler send_socket,
                                                           struct relay_server *relay_server);

///////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif //__TLS_LISTENER__
