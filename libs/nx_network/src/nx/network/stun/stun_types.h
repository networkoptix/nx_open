// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include "message.h"
#include "stun_attributes.h"

namespace nx::network::stun {

static constexpr char kUrlSchemeName[] = "stun";
static constexpr char kSecureUrlSchemeName[] = "stuns";

NX_NETWORK_API std::string_view urlScheme(bool isSecure);
NX_NETWORK_API bool isUrlScheme(const std::string_view& scheme);
NX_NETWORK_API bool isUrlScheme(const QString& scheme);
NX_NETWORK_API bool isUrlScheme(const char* scheme);

} // namespace nx::network::stun
