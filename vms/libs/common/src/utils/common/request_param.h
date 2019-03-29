#ifndef QN_REQUEST_PARAM_H
#define QN_REQUEST_PARAM_H

#include <algorithm> /* For std::find_if. */

#include <QtNetwork/QNetworkReply>
#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QMetaType>
#include <QtCore/QUrlQuery>

#include <nx/network/http/http_types.h>
#include <nx/utils/log/to_string.h>
#include <nx/utils/system_error.h>
#include <nx/utils/uuid.h>

class RequestParams: public QMultiMap<QString, QString>
{
public:
    using Base = QMultiMap<QString, QString>;
    using Base::Base;

    RequestParams(const QMultiMap<QString, QString>& map);
    RequestParams(const QHash<QString, QString>& hash);

    template<typename T>
    void insert(const QString& key, const T& value) { Base::insert(key, ::toString(value)); }

    inline QString operator[](const QString& key) const { return value(key); }

    static RequestParams fromUrlQuery(const QUrlQuery& query);
    static RequestParams fromList(const QList<QPair<QString, QString>>& list);
    static RequestParams fromJson(const QJsonObject& value);

    QUrlQuery toUrlQuery() const;
    QList<QPair<QString, QString>> toList() const;
    QJsonObject toJson() const;
};

Q_DECLARE_METATYPE(RequestParams);

// TODO: Get rid of these and replace in code!
typedef RequestParams QnRequestParamList;
typedef RequestParams QnRequestParams;

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
