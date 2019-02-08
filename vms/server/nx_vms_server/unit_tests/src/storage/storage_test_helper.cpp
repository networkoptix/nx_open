#include "storage_test_helper.h"

#include "platform/platform_abstraction.h"
#include "nx/utils/thread/long_runnable.h"
#include <nx/core/access/access_types.h>
#include "core/resource_management/resource_pool.h"
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <common/common_module.h>

namespace nx {
namespace ut {
namespace utils {

FileStorageTestHelper::FileStorageTestHelper():
    m_serverModule(new QnMediaServerModule()),
    m_platformAbstraction(new QnPlatformAbstraction())
{
    m_serverModule->commonModule()->setModuleGUID(QnUuid::createUuid());
}

QnStorageResourcePtr FileStorageTestHelper::createStorage(
    const QString& url,
    qint64 spaceLimit)
{
    return createStorage(m_serverModule.get(), url, spaceLimit);
}

QnStorageResourcePtr FileStorageTestHelper::createStorage(
    QnMediaServerModule* serverModule,
    const QString& url,
    qint64 spaceLimit)
{
    QnStorageResourcePtr result(new QnFileStorageResource(serverModule));
    result->setUrl(url);
    result->setSpaceLimit(spaceLimit);
    result->setId(QnUuid::createUuid());

    return result;
}

} // namespace utils
} // namespace ut
} // namespace nx
