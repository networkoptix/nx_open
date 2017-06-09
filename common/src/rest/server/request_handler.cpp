#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtCore/QStringList>
#include "request_handler.h"
#include "utils/common/util.h"

QnRestRequestHandler::QnRestRequestHandler():
    m_permissions(Qn::NoGlobalPermissions)
{

}

int QnRestRequestHandler::executeDelete(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor*)
{
    return nx_http::StatusCode::notImplemented;
}

int QnRestRequestHandler::executePut(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& srcBodyContentType,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* processor)
{
    return executePost(
        path, params, body, srcBodyContentType, result, resultContentType, processor);
}

QString QnRestRequestHandler::extractAction(const QString &path) const {
    QString localPath = path;
    while(localPath.endsWith(L'/'))
        localPath.chop(1);
    return localPath.mid(localPath.lastIndexOf(L'/') + 1);
}

class QnRestGUIRequestHandlerPrivate {
public:
    QnRestGUIRequestHandlerPrivate(): result(0), body(0), code(0) {}

    QnRequestParamList params;
    QByteArray* result;
    const QByteArray* body;
    QString path;
    int code;
    QString method;
    QByteArray contentType;
};

QnRestGUIRequestHandler::QnRestGUIRequestHandler(): d_ptr(new QnRestGUIRequestHandlerPrivate)
{

}

QnRestGUIRequestHandler::~QnRestGUIRequestHandler()
{
    delete d_ptr;
}

int QnRestGUIRequestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const  QnRestConnectionProcessor*)
{
    Q_D(QnRestGUIRequestHandler);
    d->path = path;
    d->params = params;
    d->result = &result;
    d->method = QLatin1String("GET");
    d->contentType = contentType;
    QMetaObject::invokeMethod(this, "methodExecutor", Qt::BlockingQueuedConnection);
    return d->code;
}

int QnRestGUIRequestHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, const QByteArray& /*srcBodyContentType*/, QByteArray& result, 
                                         QByteArray& contentType, const  QnRestConnectionProcessor*)
{
    Q_D(QnRestGUIRequestHandler);
    d->params = params;
    d->result = &result;
    d->body = &body;
    d->path = path;
    d->method = QLatin1String("POST");
    d->contentType = contentType;
    QMetaObject::invokeMethod(this, "methodExecutor", Qt::BlockingQueuedConnection);
    return d->code;
}

void QnRestGUIRequestHandler::methodExecutor()
{
    Q_D(QnRestGUIRequestHandler);
    if (d->method == QLatin1String("GET"))
        d->code = executeGetGUI(d->path, d->params, *d->result);
    else if (d->method == QLatin1String("POST"))
        d->code = executePostGUI(d->path, d->params, *d->body, *d->result);
    else 
        qWarning() << "Unknown execute method " << d->method;
}
