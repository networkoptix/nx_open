#pragma once

#include "rest/server/request_handler.h"

class AbstractStreamSocket;
class QnCommonModule;

namespace ec2 {

class Ec2DirectConnectionFactory;
class TimeSynchronizationManager;

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

    static nx::network::http::StatusCode::Value processRequest(
        const nx::network::http::Request& request,
        TimeSynchronizationManager* timeSynchronizationManager,
        AbstractStreamSocket* connection);

    static void prepareResponse(
        const TimeSynchronizationManager& timeSynchronizationManager,
        const QnCommonModule& commonModule,
        nx::network::http::Response* response);

private:
    Ec2DirectConnectionFactory* m_appServerConnection;
};

} // namespace ec2
