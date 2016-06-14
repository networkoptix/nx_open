#include "client_storage_resource.h"

#include <cassert>

#include <core/resource/media_server_resource.h>

QnClientStorageResource::QnClientStorageResource()
    : base_type()
    , m_freeSpace(QnStorageResource::kUnknownSize)
    , m_totalSpace(QnStorageResource::kUnknownSize)
    , m_writable(false)
    , m_active(false)
{

}

QnClientStorageResourcePtr QnClientStorageResource::newStorage( const QnMediaServerResourcePtr &parentServer, const QString &url ) {
    NX_ASSERT(parentServer, Q_FUNC_INFO, "Server must exist here");

    QnClientStorageResourcePtr storage(new QnClientStorageResource());

    auto resTypeId = qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kStorageTypeId);
    if (!resTypeId.isNull())
        storage->setTypeId(resTypeId);

    if (parentServer) {
        storage->setParentId(parentServer->getId());
        storage->setId(fillID(parentServer->getId(), url));
    }

    storage->setName(QnUuid::createUuid().toString());
    storage->setUrl(url);
    return storage;
}


QnClientStorageResource::~QnClientStorageResource()
{}

void QnClientStorageResource::updateInner( const QnResourcePtr &other, QSet<QByteArray>& modifiedFields ) {
    base_type::updateInner(other, modifiedFields);

    QnClientStorageResource* localOther = dynamic_cast<QnClientStorageResource*>(other.data());

    /* Do not overwrite space info data with invalid values. */
    if(localOther && localOther->isSpaceInfoAvailable()) {
        if (m_freeSpace != localOther->m_freeSpace)
            modifiedFields << "freeSpaceChanged";
        m_freeSpace = localOther->m_freeSpace;

        if (m_totalSpace != localOther->m_totalSpace)
            modifiedFields << "totalSpaceChanged";
        m_totalSpace = localOther->m_totalSpace;

        if (m_writable != localOther->m_writable)
            modifiedFields << "isWritableChanged";
        m_writable = localOther->m_writable;
    }
}

bool QnClientStorageResource::isSpaceInfoAvailable() const {
    return m_totalSpace != QnStorageResource::kUnknownSize
        && m_totalSpace != QnStorageResource::kSizeDetectionOmitted;
}

bool QnClientStorageResource::isActive() const
{
    QnMutexLocker lock(&m_mutex);
    return m_active;
}


void QnClientStorageResource::setActive(bool value)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_active == value)
            return;
        m_active = value;
    }
    emit isActiveChanged(::toSharedPointer(this));
}


QIODevice* QnClientStorageResource::open(const QString&, QIODevice::OpenMode, bool /*noBuffering*/)
{
    NX_ASSERT(false);
    return NULL;
}

bool QnClientStorageResource::initOrUpdate() const
{
    NX_ASSERT(false);
    return 0;
}

QnAbstractStorageResource::FileInfoList QnClientStorageResource::getFileList(const QString&)
{
    NX_ASSERT(false);
    return QnAbstractStorageResource::FileInfoList();
}

qint64 QnClientStorageResource::getFileSize(const QString&) const
{
    NX_ASSERT(false);
    return 0;
}

bool QnClientStorageResource::removeFile(const QString&)
{
    NX_ASSERT(false);
    return false;
}

bool QnClientStorageResource::removeDir(const QString&)
{
    NX_ASSERT(false);
    return false;
}

bool QnClientStorageResource::renameFile(const QString&, const QString&)
{
    NX_ASSERT(false);
    return false;
}

bool QnClientStorageResource::isFileExists(const QString&)
{
    NX_ASSERT(false);
    return false;
}

bool QnClientStorageResource::isDirExists(const QString&)
{
    NX_ASSERT(false);
    return false;
}

qint64 QnClientStorageResource::getFreeSpace() {
    QnMutexLocker lock(&m_mutex);
    return m_freeSpace;
}

void QnClientStorageResource::setFreeSpace( qint64 value ) {
    {
        QnMutexLocker lock(&m_mutex);
        if (m_freeSpace == value)
            return;
        m_freeSpace = value;
    }
    emit freeSpaceChanged(::toSharedPointer(this));
}

qint64 QnClientStorageResource::getTotalSpace() {
    QnMutexLocker lock(&m_mutex);
    return m_totalSpace;
}

void QnClientStorageResource::setTotalSpace( qint64 value ) {
    {
        QnMutexLocker lock(&m_mutex);
        if (m_totalSpace == value)
            return;
        m_totalSpace = value;
    }
    emit totalSpaceChanged(::toSharedPointer(this));
}

int QnClientStorageResource::getCapabilities() const {
    QnMutexLocker lock(&m_mutex);
    return m_writable
        ? QnAbstractStorageResource::cap::WriteFile
        : 0;
}

void QnClientStorageResource::setWritable( bool isWritable ) {
    {
        QnMutexLocker lock(&m_mutex);
        if (m_writable == isWritable)
            return;
        m_writable = isWritable;
    }
    emit isWritableChanged(::toSharedPointer(this));
}


