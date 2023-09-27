// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QString>

namespace ec2 {

const int INVALID_REQ_ID = -1;

enum class ErrorCode
{
    ok,
    failure,
    ioError,
    serverError,
    asyncRaceError,
    unsupported,
    unauthorized,

    //!Requested operation is currently forbidden (e.g., read-only mode is enabled)
    forbidden,
    //!Response parse error
    badResponse,
    //!Error executing DB request
    dbError,
    containsBecauseTimestamp, // transaction already in database
    containsBecauseSequence,  // transaction already in database
    //!Method is not implemented yet
    notImplemented,

    badRequest,
    notFound,
};

NX_VMS_COMMON_API QString toString(ErrorCode errorCode);

struct NX_VMS_COMMON_API Result
{
    ErrorCode error;
    QString message;

    Result(ErrorCode error = ErrorCode::ok, QString message = QString()):
        error(error), message(message)
    {
    }

    operator ErrorCode() const;
    operator bool() const;

    QString toString() const;
};

enum class NotificationSource
{
    Local,
    Remote
};

NX_VMS_COMMON_API QString toString(NotificationSource source);

template <typename... Args>
using Handler = std::function<void(int reqId, Result, const Args&...)>;

} // namespace ec2
