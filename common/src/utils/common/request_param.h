#ifndef QN_REQUEST_PARAM_H
#define QN_REQUEST_PARAM_H

#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QMetaType>

class QnRequestParam: public QPair<QString, QString> {
    typedef QPair<QString, QString> base_type;
public:
    QnRequestParam(const QString &first, const QString &second): base_type(first, second) {}
    QnRequestParam(const char *first, const char *second): base_type(QLatin1String(first), QLatin1String(second)) {}
    QnRequestParam(const char *first, const QString &second): base_type(QLatin1String(first), second) {}
    QnRequestParam(const QString &first, const char *second): base_type(first, QLatin1String(second)) {}
};

typedef QnRequestParam QnRequestHeader;

typedef QList<QPair<QString, QString> > QnRequestParamList;
typedef QList<QPair<QString, QString> > QnRequestHeaderList;

Q_DECLARE_METATYPE(QnRequestParamList);

#endif // QN_REQUEST_PARAM_H
