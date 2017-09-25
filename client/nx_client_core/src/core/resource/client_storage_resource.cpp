#include "client_storage_resource.h"

#include <cassert>

#include <core/resource/media_server_resource.h>
#include <client_core/client_core_module.h>

QnClientStorageResource::QnClientStorageResource(QnCommonModule* commonModule)
    : base_type(commonModule)
    , m_freeSpace(QnStorageResource::kUnknownSize)
    , m_totalSpace(QnStorageResource::kUnknownSize)
    , m_writable(false)
    , m_active(false)
{
    addFlags(Qn::remote);
}

QnClientStorageResourcePtr QnClientStorageResource::newStorage( const QnMediaServerResourcePtr &parentServer, const QString &url ) {
    NX_ASSERT(parentServer, Q_FUNC_INFO, "Server must exist here");

    QnClientStorageResourcePtr storage(new QnClientStorageResource(qnClientCoreModule->commonModule()));

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

void QnClientStorageResource::updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers)
{
    base_type::updateInternal(other, notifiers);

    QnClientStorageResource* localOther = dynamic_cast<QnClientStorageResource*>(other.data());

    /* Do not overwrite space info data with invalid values. */
    if (localOther && localOther->isSpaceInfoAvailable())
    {
        if (m_freeSpace != localOther->m_freeSpace)
        {
            m_freeSpace = localOther->m_freeSpace;
            notifiers << [r = toSharedPointer(this)]{emit r->freeSpaceChanged(r); };
        }

        if (m_totalSpace != localOther->m_totalSpace)
        {
            m_totalSpace = localOther->m_totalSpace;
            notifiers << [r = toSharedPointer(this)]{ emit r->totalSpaceChanged(r); };
        }

        if (m_writable != localOther->m_writable)
        {
            m_writable = localOther->m_writable;
            notifiers << [r = toSharedPointer(this)]{ emit r->isWritableChanged(r); };
        }
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


QIODevice* QnClientStorageResource::open(const QString&, QIODevice::OpenMode)
{
    NX_ASSERT(false);
    return NULL;
}

Qn::StorageInitResult QnClientStorageResource::initOrUpdate()
{
    NX_ASSERT(false);
    return Qn::StorageInit_CreateFailed;
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

qint64 QnClientStorageResource::getTotalSpace() const {
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


