// Needed for boost 1.60.0 seed_rng.hpp
#undef NULL
#define NULL (0)

#include "utils.h"
#include "mediaserver_launcher.h"

#include <core/resource/storage_plugin_factory.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <recorder/storage_manager.h>

#include <cassert>
#include <stdio.h>

#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>

static nx::ut::cfg::Config config;

namespace nx {
namespace ut {
namespace cfg {

nx::ut::cfg::Config& configInstance()
{
    return config;
}

} // namespace cfg

namespace utils {

boost::optional<QString> createRandomDir()
{
#ifdef _DEBUG
    // C++ asserts are required for unit tests to work correctly. Do not replace by NX_ASSERT.
    assert(!config.tmpDir.isEmpty()); //< Safe under define.
#endif

    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    QString dirName = QString::fromStdString(boost::uuids::to_string(uuid));;
    QString fullPath(QDir(config.tmpDir).absoluteFilePath(dirName));

#ifdef _DEBUG
    // C++ asserts are required for unit tests to work correctly. Do not replace by NX_ASSERT.
    assert(!QDir().exists(fullPath)); //< Safe under define.
#endif

    if (QDir().exists(fullPath))
        return boost::none;
    if (!QDir().mkpath(fullPath))
        return boost::none;
    return fullPath;
}

WorkDirResource::WorkDirResource(const QString& path):
    m_workDir(path.isEmpty() ? createRandomDir() : path)
{
}

WorkDirResource::~WorkDirResource()
{
    if (m_workDir)
    {
        if (!QDir(*m_workDir).removeRecursively())
            fprintf(stderr, "Recursive cleaning up %s failed\n", (*m_workDir).toLatin1().constData());
    }
}

boost::optional<QString> WorkDirResource::getDirName() const
{
    return m_workDir;
}

bool validateAndOrCreatePath(const QString &path)
{
    if (path.isNull() || path.isEmpty())
        return false;
    QDir dir(path);
    if (dir.exists() || dir.mkpath(path))
        return true;
    return false;
}

static void waitForStorageToAppear(MediaServerLauncher* server, const QString& storagePath)
{
    while (true)
    {
        const auto allStorages = server->serverModule()->normalStorageManager()->getStorages();

        const auto testStorageIt = std::find_if(
            allStorages.cbegin(), allStorages.cend(),
            [&storagePath](const auto& storage) { return storage->getUrl() == storagePath; });

        if (testStorageIt == allStorages.cend())
        {
            qDebug() << "WAITING FOR STORAGE";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        while (true) //< Waiting for initialization
        {
            const auto storage = *testStorageIt;
            const auto fileStorage = storage.dynamicCast<QnFileStorageResource>();
            NX_ASSERT(fileStorage);
            fileStorage->setMounted(true);

            if (!(storage->isWritable() && storage->isOnline() && storage->isUsedForWriting()))
                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            else
                break;
        }

        break;
    }
}

void addTestStorage(MediaServerLauncher* server, const QString& storagePath)
{
    NX_ASSERT(!storagePath.isEmpty());

    QnStorageResourcePtr storage(server->serverModule()->storagePluginFactory()->createStorage(
            server->commonModule(), "ufile"));

    storage->setName("Test storage");
    storage->setParentId(server->commonModule()->moduleGUID());
    storage->setUrl(storagePath);
    storage->setSpaceLimit(0);
    storage->setStorageType("local");
    storage->setUsedForWriting(storage->initOrUpdate() == Qn::StorageInit_Ok && storage->isWritable());

    NX_ASSERT(storage->isUsedForWriting());

    QnResourceTypePtr resType = qnResTypePool->getResourceTypeByName("Storage");
    if (resType)
        storage->setTypeId(resType->getId());

    QList<QnStorageResourcePtr> storages{ storage };
    nx::vms::api::StorageDataList apiStorages;
    ec2::fromResourceListToApi(QList<QnStorageResourcePtr>{storage}, apiStorages);

    const auto ec2Connection = server->serverModule()->ec2Connection();
    const auto saveResult =
        ec2Connection->getMediaServerManager(Qn::kSystemAccess)->saveStoragesSync(apiStorages);

    NX_ASSERT(ec2::ErrorCode::ok == saveResult);

    for (const auto &storage : storages)
    {
        server->serverModule()->commonModule()->messageProcessor()->updateResource(
            storage, ec2::NotificationSource::Local);
    }

    server->serverModule()->normalStorageManager()->initDone();
    waitForStorageToAppear(server, storagePath);
}

} // namespace utils
} // namespace ut
} // namespace nx
