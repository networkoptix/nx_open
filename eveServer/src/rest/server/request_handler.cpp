#include <QDebug>
#include "request_handler.h"

struct QnRestGUIRequestHandler::QnRestGUIRequestHandlerPrivate
{
    QnRestGUIRequestHandlerPrivate(): result(0), body(0), code(0) {}

    QnRequestParamList params;
    QByteArray* result;
    const QByteArray* body;
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

int QnRestGUIRequestHandler::executeGet(const QnRequestParamList& params, QByteArray& result)
{
    Q_D(QnRestGUIRequestHandler);
    d->params = params;
    d->result = &result;
    d->method = "GET";
    QMetaObject::invokeMethod(this, "methodExecutor", Qt::BlockingQueuedConnection);
    return d->code;
}

int QnRestGUIRequestHandler::executePost(const QnRequestParamList& params, const QByteArray& body, QByteArray& result)
{
    Q_D(QnRestGUIRequestHandler);
    d->params = params;
    d->result = &result;
    d->body = &body;
    d->method = "POST";
    QMetaObject::invokeMethod(this, "methodExecutor", Qt::BlockingQueuedConnection);
    return d->code;
}

void QnRestGUIRequestHandler::methodExecutor()
{
    Q_D(QnRestGUIRequestHandler);
    if (d->method == "GET")
        d->code = executeGetGUI(d->params, *d->result);
    else if (d->method == "POST")
        d->code = executePostGUI(d->params, *d->body, *d->result);
    else 
        qWarning() << "Unknown execute method " << d->method;
}
