#pragma once

#include "rest/server/request_handler.h"

namespace nx {
namespace network {
class AbstractStreamSocket;
} // network
} // nx
class QnCommonModule;

namespace ec2 {

class LocalConnectionFactory;
class TimeSynchronizationManager;

class QnTimeSyncRestHandler:
    public QnRestRequestHandler
{
public:
    /** Contains peer's time synchronization information. */

    QnTimeSyncRestHandler(LocalConnectionFactory* connection);

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
        nx::network::AbstractStreamSocket* connection);

    static void prepareResponse(
        const TimeSynchronizationManager& timeSynchronizationManager,
        const QnCommonModule& commonModule,
        nx::network::http::Response* response);

private:
    LocalConnectionFactory* m_appServerConnection;
};

} // namespace ec2
