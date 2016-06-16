#include "db_storage_resource.h"
#include <api/app_server_connection.h>

QnDbStorageResource::QnDbStorageResource() :
    QnStorageResource(),
    m_state(QnDbStorageResourceState::Waiting),
    m_capabilities(cap::ReadFile)
{
}

QnDbStorageResource::~QnDbStorageResource()
{

}

QnStorageResource* QnDbStorageResource::instance(const QString& )
{
    return new QnDbStorageResource();
}

QIODevice *QnDbStorageResource::open(const QString &fileName, QIODevice::OpenMode openMode)
{
    m_filePath = removeProtocolPrefix(fileName);

    QnAppServerConnectionFactory::getConnection2()
        ->getStoredFileManager(Qn::kDefaultUserAccess)
        ->getStoredFileSync(m_filePath, &m_fileData);

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

