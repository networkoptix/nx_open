#pragma once

#include <memory>
#include "core/resource/storage_resource.h"
#include <media_server/media_server_module.h>

class QnResourcePool;
class QnPlatformAbstraction;

namespace nx {
namespace ut {
namespace utils {

class FileStorageTestHelper
{
public:
    FileStorageTestHelper();

    QnStorageResourcePtr createStorage(
        const QString& url,
        qint64 spaceLimit);
    static QnStorageResourcePtr createStorage(
        QnMediaServerModule* serverModule,
        const QString& url,
        qint64 spaceLimit);

private:
    std::unique_ptr<QnMediaServerModule> m_serverModule;
    std::unique_ptr<QnPlatformAbstraction> m_platformAbstraction;
};

}}}
