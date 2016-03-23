
#include "request_param.h"


QnHTTPRawResponse::QnHTTPRawResponse()
:
    sysErrorCode(SystemError::noError),
    status(QNetworkReply::NoError)
{
}

QnHTTPRawResponse::QnHTTPRawResponse(
    SystemError::ErrorCode _sysErrorCode,
    nx_http::Response _response,
    QByteArray _msgBody)
:
    status(QNetworkReply::NoError),
    sysErrorCode(_sysErrorCode),
    response(std::move(_response)),
    msgBody(std::move(_msgBody))
{
    if (sysErrorCode != SystemError::noError)
    {
        status = sysErrorCodeToNetworkError(sysErrorCode);
        errorString = SystemError::toString(sysErrorCode).toLatin1();
    }
    else
    {
        status = httpStatusCodeToNetworkError(
            static_cast<nx_http::StatusCode::Value>(response.statusLine.statusCode));
        errorString = response.statusLine.reasonPhrase;
    }
}

QNetworkReply::NetworkError QnHTTPRawResponse::sysErrorCodeToNetworkError(
    SystemError::ErrorCode errorCode)
{
    switch (errorCode)
    {
        case SystemError::noError:
            return QNetworkReply::NoError;
        case SystemError::connectionRefused:
            return QNetworkReply::ConnectionRefusedError;
        case SystemError::connectionReset:
            return QNetworkReply::RemoteHostClosedError;
        case SystemError::hostNotFound:
            return QNetworkReply::HostNotFoundError;
        case SystemError::timedOut:
            return QNetworkReply::TimeoutError;
        default:
            return QNetworkReply::UnknownNetworkError;
    }
}

QNetworkReply::NetworkError QnHTTPRawResponse::httpStatusCodeToNetworkError(
    nx_http::StatusCode::Value statusCode)
{
    if ((statusCode / 100) == (nx_http::StatusCode::ok / 100))
        return QNetworkReply::NoError;

    switch (statusCode)
    {
        case nx_http::StatusCode::unauthorized:
            return QNetworkReply::ContentAccessDenied;
        case nx_http::StatusCode::notFound:
            return QNetworkReply::ContentNotFoundError;
        case nx_http::StatusCode::internalServerError:
            return QNetworkReply::InternalServerError;
        case nx_http::StatusCode::serviceUnavailable:
            return QNetworkReply::ServiceUnavailableError;
        default:
            return QNetworkReply::UnknownServerError;
    }
}
