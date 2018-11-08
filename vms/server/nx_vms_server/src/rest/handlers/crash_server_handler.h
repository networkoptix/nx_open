/**********************************************************
* 20 mar 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CRASH_SERVER_HANDLER_H
#define NX_CRASH_SERVER_HANDLER_H

#include "rest/server/request_handler.h"


class QnCrashServerHandler
:
    public QnRestRequestHandler
{
    Q_OBJECT

public:
    //!Implementation of QnRestRequestHandler::executeGet
    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& responseMessageBody,
        QByteArray& contentType,
        const QnRestConnectionProcessor* ) override;
    //!Implementation of QnRestRequestHandler::executePost
    virtual int executePost(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& requestBody,
        const QByteArray& srcBodyContentType,
        QByteArray& responseMessageBody,
        QByteArray& contentType,
        const QnRestConnectionProcessor* ) override;

private:
};

#endif  //NX_CRASH_SERVER_HANDLER_H
