// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ping_utils.h"

#include <QtCore/QRegularExpression>

namespace nx::vms::client::desktop {

bool isPingableHost(const QString& host)
{
    static const QRegularExpression kRegExp(QRegularExpression::anchoredPattern(
        "[A-Za-z0-9][A-Za-z0-9._-]*" //< IPv4 address or DNS name.
        "|[0-9A-Fa-f]*:[0-9A-Fa-f:.]*(%[A-Za-z0-9._-]+)?")); //< IPv6 address with optional zone.

    return kRegExp.match(host).hasMatch();
}

} // namespace nx::vms::client::desktop
