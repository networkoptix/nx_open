// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ec_api_common.h"

#include <nx/utils/log/format.h>
#include <nx/utils/log/assert.h>

namespace ec2 {

QString toString(ErrorCode errorCode)
{
    switch (errorCode)
    {
        case ErrorCode::ok:
            return "ok";
        case ErrorCode::failure:
            return "failure";
        case ErrorCode::ioError:
            return "IO error";
        case ErrorCode::serverError:
            return "server error";
        case ErrorCode::unsupported:
            return "unsupported";
        case ErrorCode::unauthorized:
            return "unauthorized";
        case ErrorCode::forbidden:
            return "forbidden";
        case ErrorCode::badResponse:
            return "badResponse";
        case ErrorCode::dbError:
            return "dbError";
        case ErrorCode::containsBecauseTimestamp:
            return "containsBecauseTimestamp";
        case ErrorCode::containsBecauseSequence:
            return "containsBecauseSequence";
        case ErrorCode::notImplemented:
            return "notImplemented";
        case ErrorCode::notFound:
            return "notFound";
    }
    return "unknown error";
}

Result::operator ErrorCode() const
{
    return error;
}

Result::operator bool() const
{
        return error == ErrorCode::ok;
}

QString Result::toString() const
{
    return message.isEmpty() ? ec2::toString(error) : (ec2::toString(error) + " - " + message);
}

QString toString(NotificationSource source)
{
    switch (source)
    {
        case NotificationSource::Local:
            return "local";
        case NotificationSource::Remote:
            return "remote";
    }
    QString error = NX_FMT("unexpected(%1)", static_cast<int>(source));
    NX_ASSERT(false, error);
    return error;
}

} // namespace ec2
