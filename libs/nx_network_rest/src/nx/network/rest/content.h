// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonValue>

#include <nx/network/http/http_types.h>

namespace nx::network::rest {

struct NX_NETWORK_REST_API Content
{
    nx::network::http::header::ContentType type;
    QByteArray body;

    std::optional<QJsonValue> parse() const;
    QJsonValue parseOrThrow() const;
};

} // namespace nx::network::rest
