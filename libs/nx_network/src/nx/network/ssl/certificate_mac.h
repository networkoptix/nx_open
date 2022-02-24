// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once
#if defined(ENABLE_SSL)

#include <string>

#include <openssl/ssl.h>

namespace nx::network::ssl::detail {

bool verifyBySystemCertificatesMac(
    STACK_OF(X509)* chain, const std::string& hostName, std::string* outErrorMessage);

} // namespace nx::network::ssl::detail

#endif // defined(ENABLE_SSL)
