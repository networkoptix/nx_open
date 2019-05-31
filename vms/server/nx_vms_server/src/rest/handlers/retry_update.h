#pragma once

#include <rest/server/request_handler.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server::rest::handlers {

class RetryUpdate: public QnRestRequestHandler, public ServerModuleAware
{
public:
    RetryUpdate(QnMediaServerModule* serverModule);

    virtual int executePost(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& result,
        QByteArray& resultContentType,
        const QnRestConnectionProcessor* processor) override;
};

} // namespace nx::vms::server::rest::handlers
