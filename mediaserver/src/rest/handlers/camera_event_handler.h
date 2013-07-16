/**********************************************************
* 15 jul 2013
* a.kolesnikov
***********************************************************/

#ifndef CAMERA_EVENT_HANDLER_H
#define CAMERA_EVENT_HANDLER_H

#include "rest/server/request_handler.h"


//!Receives events from cameras (Aacti at the moment) and dispatches it to corresponding resource
class QnCameraEventHandler
:
    public QnRestRequestHandler
{
public:
    //!Implementation of QnRestRequestHandler::executeGet
    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& responseMessageBody,
        QByteArray& contentType );
    //!Implementation of QnRestRequestHandler::executePost
    virtual int executePost(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& requestBody,
        QByteArray& responseMessageBody,
        QByteArray& contentType );
};

#endif  //CAMERA_EVENT_HANDLER_H
