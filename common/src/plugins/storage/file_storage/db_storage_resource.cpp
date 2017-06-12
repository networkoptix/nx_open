#include "db_storage_resource.h"
#include <api/app_server_connection.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <core/resource_access/user_access_data.h>
#include <nx_ec//ec_api.h>

QnDbStorageResource::QnDbStorageResource(QnCommonModule* commonModule):
    base_type(commonModule),
    m_state(QnDbStorageResourceState::Waiting),
    m_capabilities(cap::ReadFile)
{
}

QnDbStorageResource::~QnDbStorageResource()
{

}

QnStorageResource* QnDbStorageResource::instance(QnCommonModule* commonModule, const QString& )
{
    return new QnDbStorageResource(commonModule);
}

QIODevice* QnDbStorageResource::open(const QString &fileName, QIODevice::OpenMode openMode)
{
    m_filePath = removeProtocolPrefix(fileName);

    auto resCommonModule = commonModule();
    NX_ASSERT(resCommonModule, "Common module should be set");

    auto connection = resCommonModule->ec2Connection();

    if (!connection)
        return nullptr;

    auto fileManager = connection->getStoredFileManager(Qn::kSystemAccess);

    if (!fileManager)
        return nullptr;

    fileManager->getStoredFileSync(m_filePath, &m_fileData);

    QBuffer* buffer = new QBuffer();
    buffer->setBuffer(&m_fileData);
    buffer->open(openMode);

    return buffer;
}

int QnDbStorageResource::getCapabilities() const
{
    return m_capabilities;
}

QString QnDbStorageResource::removeProtocolPrefix(const QString &url) const
{
    auto prefix = url.indexOf(QLatin1String("://"));
    return prefix == -1 ? url : url.mid(prefix + 3);
}

