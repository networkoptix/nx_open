#include "storage_test_helper.h"

#include "platform/platform_abstraction.h"
#include "nx/utils/thread/long_runnable.h"
#include "core/resource_management/resource_pool.h"
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <common/common_module.h>


namespace nx {
namespace ut {
namespace utils {

FileStorageTestHelper::FileStorageTestHelper() :
    m_commonModule(new QnCommonModule(/*isClient*/false,
        nx::core::access::Mode::direct)),
    m_platformAbstraction(new QnPlatformAbstraction())
{
    m_commonModule->setModuleGUID(QnUuid::createUuid());
}

QnStorageResourcePtr FileStorageTestHelper::createStorage(
    QnCommonModule* commonModule,
    const QString& url,
    qint64 spaceLimit)
{
    QnStorageResourcePtr result(new QnFileStorageResource(commonModule));
    result->setUrl(url);
    result->setSpaceLimit(spaceLimit);
    result->setId(QnUuid::createUuid());

    return result;
}

}}}
