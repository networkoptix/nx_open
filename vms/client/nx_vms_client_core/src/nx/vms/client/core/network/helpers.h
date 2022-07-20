// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>
#include <string>

#include <QByteArray>
#include <QString>

#include <nx/network/ssl/certificate.h>

namespace nx::vms::client::core {

inline std::optional<std::string> certificateName(const nx::network::ssl::X509Name& subject)
{
    const auto& attrs = subject.attrs;

    if (const auto it = attrs.find(SN_commonName); it != attrs.end())
        return it->second;

    if (const auto it = attrs.find(SN_organizationName); it != attrs.end())
        return it->second;

    return {};
}

inline QString formattedFingerprint(const std::vector<unsigned char>& hash)
{
    return QString(QByteArray::fromRawData((char *)hash.data(), hash.size()).toHex(' ')).toUpper();
}

} // namespace nx::vms::client::core
