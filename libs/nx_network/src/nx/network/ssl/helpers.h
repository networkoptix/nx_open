// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <optional>
#include <vector>
#include <string>

#include <nx/network/socket_factory.h>

namespace nx::network::ssl {

class Certificate;

// Certificate chain, ordered from the leaf node to the root certificate.
using CertificateChain = std::vector<Certificate>;
using VerifyCertificateFunc = std::function<bool(const CertificateChain&)>;

NX_NETWORK_API bool verifyBySystemCertificates(
    const CertificateChain& chain,
    const std::string& hostName,
    std::string* outErrorMessage = nullptr);

NX_NETWORK_API CertificateChain completeCertificateChain(
    const CertificateChain& chain,
    bool* ok = nullptr);

NX_NETWORK_API AdapterFunc makeAdapterFunc(
    VerifyCertificateFunc verifyCertificateFunc,
    std::optional<std::string> serverName = std::nullopt);

} // namespace nx::network::ssl
