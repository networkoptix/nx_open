#ifndef QN_EXTERNAL_BUSINESS_EVENT_HANDLER_H
#define QN_EXTERNAL_BUSINESS_EVENT_HANDLER_H

#include "rest/server/request_handler.h"
#include <business/events/reasoned_business_event.h>
#include "core/resource/resource.h"

class QnExternalBusinessEventHandler: public QnRestRequestHandler
{
    Q_OBJECT
public:
    QnExternalBusinessEventHandler();
signals:
    void mserverFailure(QnResourcePtr resource, qint64 time, QnBusiness::EventReason reason);

protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description() const override;
};

#endif // QN_FILE_SYSTEM_HANDLER_H
