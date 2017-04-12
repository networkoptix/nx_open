#pragma once

#include <memory>
#include "core/resource/storage_resource.h"

class QnResourcePool;
class QnCommonModule;
class QnPlatformAbstraction;

namespace nx {
namespace ut {
namespace utils {

class FileStorageTestHelper
{
public:
    FileStorageTestHelper();
    QnStorageResourcePtr createStorage(
        QnCommonModule* commonModule,
        const QString& url,
        qint64 spaceLimit);

private:
    std::unique_ptr<QnCommonModule> m_commonModule;
    std::unique_ptr<QnPlatformAbstraction> m_platformAbstraction;
};

}}}
