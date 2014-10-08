#ifndef QN_EXTERNAL_BUSINESS_EVENT_REST_HANDLER_H
#define QN_EXTERNAL_BUSINESS_EVENT_REST_HANDLER_H

#include <core/resource/resource_fwd.h>
#include "rest/server/request_handler.h"
#include <business/events/reasoned_business_event.h>

class QnExternalBusinessEventRestHandler: public QnRestRequestHandler
{
    Q_OBJECT
public:
    QnExternalBusinessEventRestHandler();

    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*) override;
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, const QByteArray& srcBodyContentType, 
                            QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*) override;

signals:
    void mserverFailure(QnResourcePtr resource, qint64 time, QnBusiness::EventReason reason, const QString& reasonText);
};

#endif // QN_FILE_SYSTEM_HANDLER_H
