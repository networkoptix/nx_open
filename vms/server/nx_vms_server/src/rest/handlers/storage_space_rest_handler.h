#pragma once

#include <api/model/api_model_fwd.h>
#include <rest/server/json_rest_handler.h>
#include <platform/platform_abstraction.h>
#include <core/resource/storage_resource.h>
#include <nx/vms/server/server_module_aware.h>

class QnCommonModule;
class QnMediaServerModule;

class QnStorageSpaceRestHandler: public QnJsonRestHandler, public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT
public:
    QnStorageSpaceRestHandler(QnMediaServerModule* serverModule);

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
    QnStorageSpaceDataList getOptionalStorages() const;
    QList<QnPlatformMonitor::PartitionSpace> getSuitablePartitions() const;
    QnStorageResourceList storageListFrom(
        const QList<QnPlatformMonitor::PartitionSpace>& partitions) const;
    bool wouldBeWritableInPool(const QnStorageResourcePtr& storage) const;
};
