#ifndef QN_REQUEST_PARAM_H
#define QN_REQUEST_PARAM_H

#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QMetaType>
#include <QtNetwork/QNetworkReply>
#include "id.h"

class QnRequestParam: public QPair<QString, QString> {
    typedef QPair<QString, QString> base_type;
public:
    QnRequestParam(const QPair<QString, QString> &other): base_type(other) {}
    QnRequestParam(const QString &first, const QString &second): base_type(first, second) {}
    QnRequestParam(const char *first, const char *second): base_type(QLatin1String(first), QLatin1String(second)) {}
    QnRequestParam(const char *first, const QString &second): base_type(QLatin1String(first), second) {}
    QnRequestParam(const char *first, const QnId &second): base_type(QLatin1String(first), second.toString()) {}
    QnRequestParam(const QString &first, const char *second): base_type(first, QLatin1String(second)) {}
    QnRequestParam(const char *first, qint64 second): base_type(QLatin1String(first), QString::number(second)) {}
};

typedef QPair<QString, QString> QnRequestHeader;

typedef QList<QPair<QString, QString> > QnRequestParamList;
typedef QList<QPair<QString, QString> > QnRequestHeaderList;
typedef QList<QNetworkReply::RawHeaderPair> QnReplyHeaderList;
typedef QHash<QString, QString> QnRequestParams;

struct QnHTTPRawResponse
{
    QnHTTPRawResponse()
    {
    }

    QnHTTPRawResponse(int status, const QnReplyHeaderList &headers, const QByteArray &data, const QByteArray &errorString)
        : status(status),
          headers(headers),
          data(data),
          errorString(errorString)
    {
    }

    int status;
    QnReplyHeaderList headers;
    QByteArray data;
    QByteArray errorString;
};

Q_DECLARE_METATYPE(QnRequestParamList); /* Also works for QnRequestHeaderList. */
Q_DECLARE_METATYPE(QnReplyHeaderList);
Q_DECLARE_METATYPE(QnHTTPRawResponse);

#endif // QN_REQUEST_PARAM_H
