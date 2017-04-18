#pragma once

#include <api/model/api_model_fwd.h>
#include <rest/server/json_rest_handler.h>

class QnPlatformMonitor;
class QnCommonModule;

class QnStorageSpaceRestHandler: public QnJsonRestHandler {
    Q_OBJECT
public:
    QnStorageSpaceRestHandler();

    virtual int executeGet(
        const QString                   &path,
        const QnRequestParams           &params,
        QnJsonRestResult                &result,
        const QnRestConnectionProcessor *
    ) override;

private:
    QList<QString> getStorageProtocols() const;
    QList<QString> getStoragePaths() const;

    /**
    *   Get list of storages that do not exist in the resource pool, but
    *   can be created on the local (or mounted) partitions.
    */
    QnStorageSpaceDataList getOptionalStorages(QnCommonModule* commonModule) const;
};
