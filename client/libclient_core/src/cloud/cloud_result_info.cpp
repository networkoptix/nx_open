#include "cloud_result_info.h"

using namespace nx::cdb::api;

QnCloudResultInfo::QnCloudResultInfo(ResultCode code) : m_text(toString(code))
{
}

QnCloudResultInfo::operator QString() const
{
    return m_text;
}

//TODO: #placeholder #stub Fill with better human-readable strings
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
            return tr("This account is blocked.");

        case ResultCode::notFound:
            return tr("Not found.");

        case ResultCode::alreadyExists:
            return tr("Already exists.");

        case ResultCode::dbError:
            return tr("Internal server error. Please contact support team.");

        case ResultCode::networkError:
            return tr("Network operation failed.");

        case ResultCode::notImplemented:
            return tr("Requested feature is not implemented.");

        case ResultCode::unknownRealm:
            return tr("Unknown realm.");

        case ResultCode::badUsername:
            return tr("Bad username.");

        case ResultCode::badRequest:
            return tr("Bad request.");

        case ResultCode::invalidNonce:
            return tr("Invalid nonce.");

        case ResultCode::serviceUnavailable:
            return tr("Service is unavailable.");

        case ResultCode::credentialsRemovedPermanently:
            return tr("Credentials are no longer valid.");

        case ResultCode::invalidFormat:
            return tr("Invalid data received.");

        case ResultCode::retryLater:
            return tr("Please retry later.");

        case ResultCode::unknownError:
            return tr("Unknown error.");

        default:
            return QString();
    }
}

