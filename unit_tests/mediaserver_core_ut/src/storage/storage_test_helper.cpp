#include "storage_test_helper.h"

#include "platform/platform_abstraction.h"
#include "utils/common/long_runnable.h"
#include "core/resource_management/resource_pool.h"
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <common/common_module.h>


namespace nx {
namespace ut {
namespace utils {

FileStorageTestHelper::FileStorageTestHelper() :
    m_resourcePool(new QnResourcePool),
    m_commonModule(new QnCommonModule),
    m_platformAbstraction(new QnPlatformAbstraction(0))
{
    m_commonModule->setModuleGUID(QnUuid::createUuid());
}

QnStorageResourcePtr FileStorageTestHelper::createStorage(const QString& url, qint64 spaceLimit)
{
    QnStorageResourcePtr result(new QnFileStorageResource);
    result->setUrl(url);
    result->setSpaceLimit(spaceLimit);
    result->setId(QnUuid::createUuid());

    return result;
}

}}}
