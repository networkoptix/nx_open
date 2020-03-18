#include "ec_api_impl.h"

#include <QtCore/QMetaType>

namespace ec2 {

QString toString(ErrorCode errorCode)
{
    switch (errorCode)
    {
        case ErrorCode::ok:
            return lit("ok");
        case ErrorCode::failure:
            return lit("failure");
        case ErrorCode::ioError:
            return lit("IO error");
        case ErrorCode::serverError:
            return lit("server error");
        case ErrorCode::unsupported:
            return lit("unsupported");
        case ErrorCode::unauthorized:
            return lit("unauthorized");
        case ErrorCode::ldap_temporary_unauthorized:
            return lit("ldap temporary unauthorized");
        case ErrorCode::forbidden:
            return lit("forbidden");
        case ErrorCode::cloud_temporary_unauthorized:
            return lit("cloud temporary unauthorized");
        case ErrorCode::badResponse:
            return lit("badResponse");
        case ErrorCode::dbError:
            return lit("dbError");
        case ErrorCode::containsBecauseTimestamp:
            return lit("containsBecauseTimestamp");
        case ErrorCode::containsBecauseSequence:
            return lit("containsBecauseSequence");
        case ErrorCode::notImplemented:
            return lit("notImplemented");
        case ErrorCode::incompatiblePeer:
            return lit("incompatiblePeer");
        case ErrorCode::disabled_user_unauthorized:
            return lit("disabledUser");
        case ErrorCode::userLockedOut:
            return lit("userLockedOut");
        default:
            return lit("unknown error");
    }
}

QString toString(NotificationSource source)
{
    switch (source)
    {
        case NotificationSource::Local: return "local";
        case NotificationSource::Remote: return "remote";
    }

    const QString error = nx::utils::log::makeMessage("unexpected(%1)", static_cast<int>(source));
    NX_ASSERT(false, error);
    return error;
}

} // namespace ec2
