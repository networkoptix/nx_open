#pragma once

#include <rest/server/json_rest_handler.h>
#include <nx/mediaserver/server_module_aware.h>
#include <core/resource/resource_fwd.h>

class QnStorageStatusRestHandler:
    public QnJsonRestHandler,
    public nx::mediaserver::ServerModuleAware
{
    Q_OBJECT
public:
    QnStorageStatusRestHandler(QnMediaServerModule* serverModule);

    virtual int executeGet(
        const QString &path,
        const QnRequestParams &params,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor*) override;
private:
    QnStorageResourcePtr getOrCreateStorage(const QString& storageUrl) const;
};
