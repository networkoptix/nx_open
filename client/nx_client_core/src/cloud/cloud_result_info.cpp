#include "cloud_result_info.h"

#include <nx/network/app_info.h>

#include <utils/common/app_info.h>

using namespace nx::cdb::api;

QnCloudResultInfo::QnCloudResultInfo(ResultCode code) : m_text(toString(code))
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
        /* Public result codes: */

        case ResultCode::ok:
            return tr("Successful.");

        case ResultCode::notAuthorized:
            return tr("Invalid login or password.");

        case ResultCode::forbidden:
            return tr("Requested operation is not allowed with provided credentials.");

        case ResultCode::accountNotActivated:
            return tr("This account is not activated. Please check your email.");

        case ResultCode::accountBlocked:
            return tr("This account is blocked.");

        case ResultCode::dbError:
            return tr("Internal %1 error. Please contact support team.",
                "%1 is the cloud name (like 'Nx Cloud')")
                .arg(nx::network::AppInfo::cloudName());

        case ResultCode::networkError:
            return tr("Unexpected network error. Please check your Internet connection and try again.");

        case ResultCode::badUsername:
            return tr("Invalid login.");

        case ResultCode::serviceUnavailable:
            return tr("Sorry, %1 Service is temporary unavailable. We are doing our best to restore it. Please try again later.",
                "%1 is the cloud name (like 'Nx Cloud')")
                .arg(nx::network::AppInfo::cloudName());

        case ResultCode::credentialsRemovedPermanently:
            return tr("Credentials are no longer valid.");

        case ResultCode::retryLater:
            return tr("Sorry, %1 Service could not process your request. Please try again in a few moments.",
                "%1 is the cloud name (like 'Nx Cloud')")
                .arg(nx::network::AppInfo::cloudName());

        /* Internal result codes: */

        case ResultCode::partialContent:
            return tr("Successful.");

        case ResultCode::notFound:
            return tr("Requested object is not found.");

        case ResultCode::alreadyExists:
            return tr("Object already exists.");

        case ResultCode::notImplemented:
            return tr("Requested feature is not implemented.");

        case ResultCode::unknownRealm:
            return tr("Unknown realm.");

        case ResultCode::badRequest:
            return tr("Bad request.");

        case ResultCode::invalidNonce:
            return tr("Invalid nonce.");

        case ResultCode::invalidFormat:
            return tr("Invalid data received.");

        case ResultCode::unknownError:
            return tr("Unknown error.");

        default:
            return QString();
    }
}

