#pragma once

#include <rest/server/json_rest_handler.h>
#include <nx/mediaserver/server_module_aware.h>

class QnSaveUserExRestHandler : public QnJsonRestHandler,
    public nx::mediaserver::ServerModuleAware
{
public:
    QnSaveUserExRestHandler(QnMediaServerModule* serverModule);

    virtual int executePost(const QString& path, const QnRequestParams& params,
        const QByteArray& body, QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;
};
