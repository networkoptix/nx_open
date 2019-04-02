#ifndef QN_REQUEST_PARAM_H
#define QN_REQUEST_PARAM_H

#include <algorithm> /* For std::find_if. */

#include <QtNetwork/QNetworkReply>

#include <nx/network/http/http_types.h>
#include <nx/network/rest/params.h>
#include <nx/utils/log/to_string.h>
#include <nx/utils/system_error.h>
#include <nx/utils/uuid.h>

// TODO: Get rid of these and replace in code!
typedef nx::network::rest::Params QnRequestParamList;
typedef nx::network::rest::Params QnRequestParams;

struct QnHTTPRawResponse
{
    QnHTTPRawResponse();
    QnHTTPRawResponse(
        SystemError::ErrorCode sysErrorCode,
        const nx::network::http::StatusLine& statusLine,
        const QByteArray& contentType,
        const QByteArray& msgBody);

    SystemError::ErrorCode sysErrorCode;
    QNetworkReply::NetworkError status;
    QByteArray contentType;
    QByteArray msgBody;
    QString errorString;

private:
    QNetworkReply::NetworkError sysErrorCodeToNetworkError(
        SystemError::ErrorCode errorCode);
    QNetworkReply::NetworkError httpStatusCodeToNetworkError(
        nx::network::http::StatusCode::Value statusCode);
};

Q_DECLARE_METATYPE(QnHTTPRawResponse);

#endif // QN_REQUEST_PARAM_H
