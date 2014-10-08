#ifndef QN_REQUEST_PARAM_H
#define QN_REQUEST_PARAM_H

#include <algorithm> /* For std::find_if. */

#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QMetaType>
#include <utils/common/uuid.h>

#include <QNetworkReply>


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

    QnHTTPRawResponse( QNetworkReply::NetworkError status, const QnReplyHeaderList &headers, const QByteArray &data, const QByteArray &errorString )
    :
        status(status),
        headers(headers),
        data(data),
        errorString(errorString)
    {
    }

    QNetworkReply::NetworkError status;
    QnReplyHeaderList headers;
    QByteArray data;
    QByteArray errorString;
};

Q_DECLARE_METATYPE(QnRequestParamList); /* Also works for QnRequestHeaderList. */
Q_DECLARE_METATYPE(QnReplyHeaderList);
Q_DECLARE_METATYPE(QnHTTPRawResponse);

#endif // QN_REQUEST_PARAM_H
