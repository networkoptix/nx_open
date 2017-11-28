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
#include <nx/utils/uuid.h>

#include <nx/utils/system_error.h>


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

    QnListMap(std::initializer_list<QPair<Key, Value>> args): base_type(args) {}

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

    void insert(const QnListMap &other) {
        for (const auto &pair: other)
            insert(pair.first, pair.second);
    }

    QHash<Key, Value> toHash() const {
        QHash<Key, Value> result;
        for(const QPair<Key, Value> &pair: *this)
            result.insert(pair.first, pair.second);
        return result;
    }
};

typedef QPair<QString, QString> QnRequestHeader;

typedef QnListMap<QString, QString> QnRequestHeaderList;
typedef QnListMap<QString, QString> QnRequestParamList;
typedef QnListMap<QByteArray, QByteArray> QnReplyHeaderList;

typedef QHash<QString, QString> QnRequestParams;

/** NOTE: If identical param names exist in url, the first one is taken. */
QnRequestParams requestParamsFromUrl(const nx::utils::Url& url);

struct QnHTTPRawResponse
{
    QnHTTPRawResponse();
    QnHTTPRawResponse(
        SystemError::ErrorCode sysErrorCode,
        const nx_http::StatusLine& statusLine,
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
        nx_http::StatusCode::Value statusCode);
};

Q_DECLARE_METATYPE(QnRequestParamList); /* Also works for QnRequestHeaderList. */
Q_DECLARE_METATYPE(QnReplyHeaderList);
Q_DECLARE_METATYPE(QnHTTPRawResponse);

#endif // QN_REQUEST_PARAM_H
