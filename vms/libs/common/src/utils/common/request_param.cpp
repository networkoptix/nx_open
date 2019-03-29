#include "request_param.h"

#include <nx/utils/log/log.h>

RequestParams::RequestParams(const QMultiMap<QString, QString>& map):
    QMultiMap<QString, QString>(map)
{
}

RequestParams::RequestParams(const QHash<QString, QString>& hash)
{
    for (auto it = hash.begin(); it != hash.end(); ++it)
        insert(it.key(), it.value());
}

RequestParams RequestParams::fromUrlQuery(const QUrlQuery& query)
{
    return fromList(query.queryItems(QUrl::FullyDecoded));
}

RequestParams RequestParams::fromList(const QList<QPair<QString, QString>>& list)
{
    RequestParams params;
    for (const auto& item: list)
        params.insert(item.first, item.second);
    return params;
}

RequestParams RequestParams::fromJson(const QJsonObject& value)
{
    const auto jsonValue =
        [](const QJsonValue& value) -> QString
    {
        if (value.type() == QJsonValue::Null)
            return QString();

        if (value.type() == QJsonValue::Bool)
            return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");

        if (value.type() == QJsonValue::Double)
            return QString::number(value.toDouble());

        // TODO: Add some format for Array and Object.
        return value.toString();
    };

    RequestParams params;
    for (auto it = value.begin(); it != value.end(); ++it)
        params.insert(it.key(), jsonValue(it.value()));
    return params;
}

QUrlQuery RequestParams::toUrlQuery() const
{
    QUrlQuery query;
    query.setQueryItems(toList());
    return query;
}

QList<QPair<QString, QString>> RequestParams::toList() const
{
    QList<QPair<QString, QString>> list;
    for (auto it = begin(); it != end(); ++it)
        list.append({it.key(), it.value()});
    return list;
}

QJsonObject RequestParams::toJson() const
{
    const auto jsonValue =
        [](const QString& value) -> QJsonValue
    {
        if (value.isEmpty())
            return QJsonValue(QJsonValue::Null);

        if (value == QStringLiteral("true"))
            return QJsonValue(true);

        if (value == QStringLiteral("false"))
            return QJsonValue(false);

        bool isOk = false;
        if (const auto number = value.toDouble(&isOk); isOk)
            return QJsonValue(number);

        // TODO: Add some format for Array and Object.
        return QJsonValue(value);
    };

    QJsonObject object;
    for (auto it = begin(); it != end(); ++it)
        object.insert(it.key(), jsonValue(it.value()));
    return object;
}

QnHTTPRawResponse::QnHTTPRawResponse():
    sysErrorCode(SystemError::noError),
    status(QNetworkReply::NoError)
{
}

QnHTTPRawResponse::QnHTTPRawResponse(
    SystemError::ErrorCode sysErrorCode,
    const nx::network::http::StatusLine&  statusLine,
    const QByteArray& contentType,
    const QByteArray& msgBody)
    :
    sysErrorCode(sysErrorCode),
    status(QNetworkReply::NoError),
    contentType(contentType),
    msgBody(msgBody)
{
    if (sysErrorCode != SystemError::noError)
    {
        status = sysErrorCodeToNetworkError(sysErrorCode);
        errorString = SystemError::toString(sysErrorCode);
    }
    else
    {
        status = httpStatusCodeToNetworkError(
            static_cast<nx::network::http::StatusCode::Value>(statusLine.statusCode));
        errorString = QString::fromUtf8(statusLine.reasonPhrase);
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
    nx::network::http::StatusCode::Value statusCode)
{
    if ((statusCode / 100) == (nx::network::http::StatusCode::ok / 100))
        return QNetworkReply::NoError;

    switch (statusCode)
    {
        case nx::network::http::StatusCode::unauthorized:
            return QNetworkReply::ContentAccessDenied;
        case nx::network::http::StatusCode::notFound:
            return QNetworkReply::ContentNotFoundError;
        case nx::network::http::StatusCode::internalServerError:
            return QNetworkReply::InternalServerError;
        case nx::network::http::StatusCode::serviceUnavailable:
            return QNetworkReply::ServiceUnavailableError;
        default:
            return QNetworkReply::UnknownServerError;
    }
}
