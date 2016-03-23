#ifndef QN_REQUEST_PARAM_H
#define QN_REQUEST_PARAM_H

#include <algorithm> /* For std::find_if. */

#include <QtNetwork/QNetworkReply>
#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QMetaType>

#include <nx/network/http/httptypes.h>
#include <nx/utils/uuid.h>

#include <utils/common/systemerror.h>


class QnRequestParam: public QPair<QString, QString> {
    typedef QPair<QString, QString> base_type;
public:
    QnRequestParam(const QPair<QString, QString> &other): base_type(other) {}
    QnRequestParam(const QString &first, const QString &second): base_type(first, second) {}
    QnRequestParam(const char *first, const char *second): base_type(QLatin1String(first), QLatin1String(second)) {}
    QnRequestParam(const char *first, const QString &second): base_type(QLatin1String(first), second) {}
    QnRequestParam(const char *first, const QnUuid &second): base_type(QLatin1String(first), second.toString()) {}
    QnRequestParam(const QString &first, const char *second): base_type(first, QLatin1String(second)) {}
    QnRequestParam(const char *first, qint64 second): base_type(QLatin1String(first), QString::number(second)) {}
};


// TODO: #Elric remove, use QHash?
template<class Key, class Value>
class QnListMap: public QList<QPair<Key, Value> > {
    typedef QList<QPair<Key, Value> > base_type;
public:
    typedef typename base_type::iterator iterator;
    typedef typename base_type::const_iterator const_iterator;
    typedef typename base_type::value_type value_type;

    QnListMap() {}

    QnListMap(const base_type &other): base_type(other) {}

    Value value(const Key &key, const Value &defaultValue = Value()) const {
        for(const QPair<Key, Value> &pair: *this)
            if(pair.first == key)
                return pair.second;
        return defaultValue;
    }

    QList<Value> allValues(const Key &key) const {
        QList<Value> result;
        for(const QPair<Key, Value> &pair: *this)
            if(pair.first == key)
                result << pair.second;
        return result;
    }

    bool contains(const Key &key) const {
        for(const QPair<Key, Value> &pair: *this)
            if(pair.first == key)
                return true;
        return false;
    }

    iterator find(const Key &key) {
        return std::find_if(base_type::begin(), base_type::end(), [&](const value_type &element) { return element.first == key; });
    }

    const_iterator find(const Key &key) const {
        return std::find_if(base_type::begin(), base_type::end(), [&](const value_type &element) { return element.first == key; });
    }

    using base_type::insert;

    void insert(const Key &key, const Value &value) {
        base_type::push_back(qMakePair(key, value));
    }
};

typedef QPair<QString, QString> QnRequestHeader;

typedef QnListMap<QString, QString> QnRequestHeaderList;
typedef QnListMap<QString, QString> QnRequestParamList;
typedef QnListMap<QByteArray, QByteArray> QnReplyHeaderList;

typedef QHash<QString, QString> QnRequestParams;


struct QnHTTPRawResponse
{
    QnHTTPRawResponse()
    :
        status( QNetworkReply::NoError )
    {
    }

    QnHTTPRawResponse(
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

    SystemError::ErrorCode sysErrorCode;
    QNetworkReply::NetworkError status;
    nx_http::Response response;
    QByteArray msgBody;
    QByteArray errorString;

private:
    QNetworkReply::NetworkError sysErrorCodeToNetworkError(
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

    QNetworkReply::NetworkError httpStatusCodeToNetworkError(
        nx_http::StatusCode::Value statusCode)
    {
        if ((statusCode / 200) == (nx_http::StatusCode::ok / 200))
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
};

Q_DECLARE_METATYPE(QnRequestParamList); /* Also works for QnRequestHeaderList. */
Q_DECLARE_METATYPE(QnReplyHeaderList);
Q_DECLARE_METATYPE(QnHTTPRawResponse);

#endif // QN_REQUEST_PARAM_H
