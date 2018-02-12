#pragma once

#include "rest/server/request_handler.h"

namespace ec2 {

class Ec2DirectConnectionFactory;

class QnTimeSyncRestHandler:
    public QnRestRequestHandler
{
public:
    static const QString PATH;
    /** Contains peer's time synchronization information. */
    static const QByteArray TIME_SYNC_HEADER_NAME;

    QnTimeSyncRestHandler(Ec2DirectConnectionFactory* connection);

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
        QByteArray& resultContentType,
        const QnRestConnectionProcessor*) override;

private:
    Ec2DirectConnectionFactory* m_appServerConnection;
};

} // namespace ec2
