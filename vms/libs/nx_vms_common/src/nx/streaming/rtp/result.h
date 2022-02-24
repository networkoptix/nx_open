// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QString>

namespace nx::streaming::rtp {

struct Result
{
    Result() = default;
    Result(bool success) : success(success) {}
    Result(bool success, QString message) : success(success), message(message) {}

    operator bool() { return success; }

    bool success = false;
    QString message;
};

} // namespace nx::streaming::rtp
