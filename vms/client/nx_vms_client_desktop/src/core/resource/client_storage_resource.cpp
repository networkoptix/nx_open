// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_storage_resource.h"

#include <cassert>

#include <core/resource/media_server_resource.h>

#include <nx/vms/api/data/media_server_data.h>

QnClientStorageResource::QnClientStorageResource():
    base_type()
{
    addFlags(Qn::remote);
}

QnClientStorageResourcePtr QnClientStorageResource::newStorage(
    const QnMediaServerResourcePtr& parentServer,
    const QString& url)
{
    NX_ASSERT(parentServer, "Server must exist here");

    QnClientStorageResourcePtr storage(new QnClientStorageResource());
    storage->setTypeId(nx::vms::api::StorageData::kResourceTypeId);

    if (parentServer) {
        storage->setParentId(parentServer->getId());
        storage->setIdUnsafe(fillID(parentServer->getId(), url));
    }

    storage->setName(QnUuid::createUuid().toString());
    storage->setUrl(url);
    return storage;
}


QnClientStorageResource::~QnClientStorageResource()
{}

void QnClientStorageResource::updateInternal(const QnResourcePtr& source, NotifierList& notifiers)
{
    base_type::updateInternal(source, notifiers);

    auto sourceStorage = source.dynamicCast<QnClientStorageResource>();

    /* Do not overwrite space info data with invalid values. */
    if (sourceStorage && sourceStorage->isSpaceInfoAvailable())
    {
        if (m_freeSpace != sourceStorage->m_freeSpace)
        {
            m_freeSpace = sourceStorage->m_freeSpace;
            notifiers << [r = toSharedPointer(this)]{emit r->freeSpaceChanged(r); };
        }

        if (m_totalSpace != sourceStorage->m_totalSpace)
        {
            m_totalSpace = sourceStorage->m_totalSpace;
            notifiers << [r = toSharedPointer(this)]{ emit r->totalSpaceChanged(r); };
        }

        if (m_writable != sourceStorage->m_writable)
        {
            m_writable = sourceStorage->m_writable;
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
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_active;
}


void QnClientStorageResource::setActive(bool value)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_active == value)
            return;
        m_active = value;
    }
    emit isActiveChanged(::toSharedPointer(this));
}


QIODevice* QnClientStorageResource::openInternal(const QString&, QIODevice::OpenMode)
{
    NX_ASSERT(false);
    return nullptr;
}

nx::vms::api::StorageInitResult QnClientStorageResource::initOrUpdate()
{
    NX_ASSERT(false);
    return nx::vms::api::StorageInitResult::createFailed;
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
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_freeSpace;
}

void QnClientStorageResource::setFreeSpace( qint64 value ) {
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_freeSpace == value)
            return;
        m_freeSpace = value;
    }
    emit freeSpaceChanged(::toSharedPointer(this));
}

qint64 QnClientStorageResource::getTotalSpace() const {
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_totalSpace;
}

void QnClientStorageResource::setTotalSpace( qint64 value ) {
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_totalSpace == value)
            return;
        m_totalSpace = value;
    }
    emit totalSpaceChanged(::toSharedPointer(this));
}

int QnClientStorageResource::getCapabilities() const {
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_writable
        ? QnAbstractStorageResource::cap::WriteFile
        : 0;
}

void QnClientStorageResource::setWritable( bool isWritable ) {
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_writable == isWritable)
            return;
        m_writable = isWritable;
    }
    emit isWritableChanged(::toSharedPointer(this));
}
