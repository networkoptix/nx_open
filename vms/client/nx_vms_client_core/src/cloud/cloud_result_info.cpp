#include "cloud_result_info.h"

#include <nx/network/app_info.h>

using ResultCode = nx::cloud::db::api::ResultCode;

QnCloudResultInfo::QnCloudResultInfo(ResultCode code):
    m_text(toString(code))
{
}

QnCloudResultInfo::operator QString() const
{
    return m_text;
}

QString QnCloudResultInfo::toString(ResultCode code)
{
    switch (code)
    {
        case ResultCode::ok:
        case ResultCode::partialContent:
            return tr("Successful.");

        case ResultCode::notAuthorized:
            return tr("Invalid login or password.");

        case ResultCode::forbidden:
            return tr("Requested operation is not allowed with provided credentials.");

        case ResultCode::accountNotActivated:
            return tr("This account is not activated. Please check your email.");

        case ResultCode::accountBlocked:
            return tr("Too many attempts. Try again in a minute.");

        case ResultCode::dbError:
        case ResultCode::notFound:
        case ResultCode::alreadyExists:
        case ResultCode::notImplemented:
        case ResultCode::unknownRealm:
        case ResultCode::badRequest:
        case ResultCode::invalidNonce:
        case ResultCode::invalidFormat:
        case ResultCode::unknownError:
            return tr("Internal %1 error. Please contact support team.",
                "%1 is the cloud name (like Nx Cloud)")
                .arg(nx::network::AppInfo::cloudName());

        case ResultCode::networkError:
            return tr("Unexpected network error. Please check your Internet connection and try again.");

        case ResultCode::badUsername:
            return tr("Invalid login.");

        case ResultCode::serviceUnavailable:
        case ResultCode::retryLater:
            return tr("%1 is temporary unavailable. Please try again later.",
                "%1 is the cloud name (like Nx Cloud)").arg(nx::network::AppInfo::cloudName());

        case ResultCode::credentialsRemovedPermanently:
            return tr("Credentials are no longer valid.");

        default:
            return QString();
    }
}

