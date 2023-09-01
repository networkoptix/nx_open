// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>

namespace nx::network {

struct NX_NETWORK_API Ini: nx::kit::IniConfig
{
    Ini(): IniConfig("nx_network.ini") { reload(); }

    NX_INI_INT(0, aioThreadCount, "Number of AIO threads, 0 means the number of CPU cores.");

    NX_INI_FLAG(true, verifySslCertificates, "Enables SSL certificate validation in general.");

    // VMS-20300
    NX_INI_FLAG(true, verifyVmsSslCertificates,
        "[Feature] Verify vms server's self-signed SSL certificates. This option does not work if "
        "verifySslCertificates is disabled.");

    NX_INI_FLAG(true, useDefaultSslCertificateVerificationByOs,
        "Verify SSL certificates using OS CA by default if there is no specific certificate\n"
        "verification assumed (i.e. client-server, server-server or server-cloud connections)");

    NX_INI_FLAG(false, httpClientTraffic, "Trace HTTP traffic for nx::network::http::AsyncHttpClient");
    NX_INI_STRING("", disableHosts, "Comma-separated list of forbidden IPs and domains");

    NX_INI_INT(10'000'000, minSocketSendDurationUs,
        "Minimum duration of socket send() to log as WARNING, in microseconds. Lesser durations\n"
        "are logged as VERBOSE.");

    NX_INI_FLAG(false, traceIoObjectsLifetime,
        "Enables reporting creation stack traces of dangling HTTP clients during server shutdown");
};

NX_NETWORK_API Ini& ini();

} // namespace nx::network
