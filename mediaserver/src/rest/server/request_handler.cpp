#include <QDebug>
#include <QDateTime>
#include <QStringList>
#include "request_handler.h"
#include "utils/common/util.h"

qint64 QnRestRequestHandler::parseDateTime(const QString& dateTime)
{
    if (dateTime.toLower().trimmed() == "now")
    {
        return DATETIME_NOW;
    }
    else if (dateTime.contains('T') || dateTime.contains('-'))
    {
        QStringList dateTimeParts = dateTime.split('.');
        QDateTime tmpDateTime = QDateTime::fromString(dateTimeParts[0], Qt::ISODate);
        if (dateTimeParts.size() > 1)
            tmpDateTime = tmpDateTime.addMSecs(dateTimeParts[1].toInt()/1000);
        return tmpDateTime.toMSecsSinceEpoch() * 1000;
    }
    else
        return dateTime.toLongLong();
}

class QnRestGUIRequestHandler::QnRestGUIRequestHandlerPrivate {
public:
    QnRestGUIRequestHandlerPrivate(): result(0), body(0), code(0) {}

    QnRequestParamList params;
    QByteArray* result;
    const QByteArray* body;
    QString path;
    int code;
    QString method;
};

QnRestGUIRequestHandler::QnRestGUIRequestHandler(): d_ptr(new QnRestGUIRequestHandlerPrivate)
{

}

QnRestGUIRequestHandler::~QnRestGUIRequestHandler()
{
    delete d_ptr;
}

int QnRestGUIRequestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(path)
    Q_UNUSED(contentType)
    Q_D(QnRestGUIRequestHandler);
    d->params = params;
    d->result = &result;
    d->method = "GET";
    QMetaObject::invokeMethod(this, "methodExecutor", Qt::BlockingQueuedConnection);
    return d->code;
}

int QnRestGUIRequestHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_D(QnRestGUIRequestHandler);
    d->params = params;
    d->result = &result;
    d->body = &body;
    d->path = path;
    d->method = "POST";
    QMetaObject::invokeMethod(this, "methodExecutor", Qt::BlockingQueuedConnection);
    return d->code;
}

void QnRestGUIRequestHandler::methodExecutor()
{
    Q_D(QnRestGUIRequestHandler);
    if (d->method == "GET")
        d->code = executeGetGUI(d->path, d->params, *d->result);
    else if (d->method == "POST")
        d->code = executePostGUI(d->path, d->params, *d->body, *d->result);
    else 
        qWarning() << "Unknown execute method " << d->method;
}
