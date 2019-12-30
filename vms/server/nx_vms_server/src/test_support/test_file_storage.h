#pragma once

#include <plugins/storage/file_storage/file_storage_resource.h>
#include <media_server/media_server_module.h>

namespace nx::vms::server::test_support {

class TestFileStorage: public QnFileStorageResource
{
public:
    using QnFileStorageResource::QnFileStorageResource;

    static StorageResourcePtr create(
        QnMediaServerModule* serverModule,
        const QString& path,
        bool isBackup = false);

    virtual Qn::StorageInitResult initOrUpdate() override;
};

} // namespace nx::vms::server::test_support