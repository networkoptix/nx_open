#ifndef QN_VERSION_REST_HANDLER_H
#define QN_VERSION_REST_HANDLER_H

#include "rest/server/request_handler.h"
#include <nx/vms/api/data/software_version.h>

class QnAppInfoRestHandler: public QnRestRequestHandler
{
    Q_OBJECT
public:
    QnAppInfoRestHandler(const nx::vms::api::SoftwareVersion& engineVersion);

    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor*) override;

    virtual int executePost(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& result,
        QByteArray& contentType, const QnRestConnectionProcessor*) override;

private:
    nx::vms::api::SoftwareVersion m_engineVersion;
};

#endif // QN_VERSION_REST_HANDLER_H
