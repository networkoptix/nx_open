#include "storage_space_helper.h"
#include <media_server/media_server_module.h>
#include <recorder/storage_manager.h>

namespace nx::rest::helpers {

QnStorageSpaceDataList availableStorages(QnMediaServerModule* serverModule)
{
    const auto storages =
        serverModule->normalStorageManager()->getStorages()
        + serverModule->backupStorageManager()->getStorages();

    const auto writable =
        serverModule->normalStorageManager()->getAllWritableStorages()
        + serverModule->backupStorageManager()->getAllWritableStorages();

    QnStorageSpaceDataList result;
    std::transform(
        storages.cbegin(), storages.cend(), std::back_inserter(result),
        [&writable, serverModule](const auto& storage)
        {
            QnStorageSpaceData data(storage, false);
            data.url = QnStorageResource::urlWithoutCredentials(data.url);
            data.isWritable =  writable.contains(storage);
            data.storageStatus = QnStorageManager::storageStatus(serverModule, storage);
            return data;
        });

    return result;
}

} // namespace nx::rest::helpers