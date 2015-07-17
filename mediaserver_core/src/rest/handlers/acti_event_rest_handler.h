/**********************************************************
* 15 jul 2013
* a.kolesnikov
***********************************************************/

#ifndef CAMERA_EVENT_HANDLER_H
#define CAMERA_EVENT_HANDLER_H

#ifdef ENABLE_ACTI

#include "rest/server/request_handler.h"

//!Receives events from cameras (Acti at the moment) and dispatches it to corresponding resource
class QnActiEventRestHandler
:
    public QnRestRequestHandler
{
    Q_OBJECT
public:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& responseMessageBody, QByteArray& contentType, const QnRestConnectionProcessor*) override;
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& requestBody, const QByteArray& srcBodyContentType, QByteArray& responseMessageBody, 
                            QByteArray& contentType, const QnRestConnectionProcessor*) override;
};

#endif // ENABLE_ACTI
#endif // CAMERA_EVENT_HANDLER_H
