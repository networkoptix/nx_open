// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stun_types.h"

namespace nx::network::stun {

std::string_view urlScheme(bool isSecure)
{
    return isSecure ? kSecureUrlSchemeName : kUrlSchemeName;
}

bool isUrlScheme(const std::string_view& scheme)
{
    return scheme == kUrlSchemeName || scheme == kSecureUrlSchemeName;
}

bool isUrlScheme(const QString& scheme)
{
    return isUrlScheme(scheme.toStdString());
}

bool isUrlScheme(const char* scheme)
{
    return isUrlScheme(std::string_view(scheme));
}

} // namespace nx::network::stun
