#include "test_file_storage.h"

#include <nx/vms/server/root_fs.h>

namespace nx::vms::server::test_support {

StorageResourcePtr TestFileStorage::create(
    QnMediaServerModule* serverModule,
    const QString& path,
    bool isBackup)
{
    StorageResourcePtr storage(new TestFileStorage(serverModule));

    storage->setName("Test storage");
    storage->setParentId(serverModule->commonModule()->moduleGUID());
    storage->setUrl(path);
    storage->setSpaceLimit(0);
    storage->setStorageType("local");
    storage->setUsedForWriting(
        storage->initOrUpdate() == Qn::StorageInit_Ok && storage->isWritable());
    storage->setIdUnsafe(QnUuid::createUuid());
    storage->setBackup(isBackup);

    NX_ASSERT(storage->isUsedForWriting());

    QnResourceTypePtr resType = qnResTypePool->getResourceTypeByName("Storage");
    if (resType)
        storage->setTypeId(resType->getId());

    return storage;
}

Qn::StorageInitResult TestFileStorage::initOrUpdate()
{
    const auto url = getUrl();
    NX_CRITICAL(!url.isNull());
    const auto onExit = nx::utils::makeScopeGuard(
        [this]() { m_cachedTotalSpace = rootTool()->totalSpace(getFsPath()); });

    if (QDir(url).exists())
        return m_state = Qn::StorageInit_Ok;

    m_state = QDir().mkpath(url) ? Qn::StorageInit_Ok : Qn::StorageInit_WrongPath;
    return m_state;
}

} // namespace nx::vms::server::test_support