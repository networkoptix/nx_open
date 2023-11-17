// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_resource.h"

#include <core/resource/media_server_resource.h>
#include <core/resource/resource_property_key.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/reflect/to_string.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <nx/utils/log/log.h>

const qint64 QnStorageResource::kNasStorageLimit = 50LL * 1024 * 1024 * 1024; // 50 gb
const qint64 QnStorageResource::kThirdPartyStorageLimit = 10LL * 1024 * 1024 * 1024; // 10 gb

const auto kStorageArchiveModeDefaultValue = nx::vms::api::StorageArchiveMode::undefined;

QnStorageResource::~QnStorageResource()
{
}

QnMediaServerResourcePtr QnStorageResource::getParentServer() const {
    return getParentResource().dynamicCast<QnMediaServerResource>();
}


void QnStorageResource::setSpaceLimit(qint64 value)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_spaceLimit = value;
    }
    emit spaceLimitChanged(::toSharedPointer(this));
}

qint64 QnStorageResource::getSpaceLimit() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_spaceLimit;
}

void QnStorageResource::setStorageType(const QString& type)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_storageType = type;
    emit typeChanged(::toSharedPointer(this));
}

QString QnStorageResource::getStorageType() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_storageType;
}

void QnStorageResource::setMaxStoreTime(int timeInSeconds)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_maxStoreTime = timeInSeconds;
}

int QnStorageResource::getMaxStoreTime() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_maxStoreTime;
}

void QnStorageResource::setUsedForWriting(bool isUsedForWriting) {
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_usedForWriting == isUsedForWriting)
            return;
        m_usedForWriting = isUsedForWriting;
    }
    emit isUsedForWritingChanged(::toSharedPointer(this));
}

bool QnStorageResource::isUsedForWriting() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_usedForWriting;
}

qint64 QnStorageResource::bitrateBps() const
{
    float rez = 0;
    NX_MUTEX_LOCKER lock(&m_bitrateMtx);
    for(const QnAbstractMediaStreamDataProvider* provider: m_providers)
        rez += provider->bitrateBitsPerSecond();
    return rez;
}

void QnStorageResource::addBitrate(QnAbstractMediaStreamDataProvider* provider)
{
    NX_MUTEX_LOCKER lock(&m_bitrateMtx);
    m_providers << provider;
}

void QnStorageResource::releaseBitrate(QnAbstractMediaStreamDataProvider* provider)
{
    NX_MUTEX_LOCKER lock(&m_bitrateMtx);
    m_providers.remove(provider);
}

QString QnStorageResource::urlToPath(const QString& url)
{
    if (!url.contains("://"))
        return url;
    else
        return QUrl(url).path();
}

QString QnStorageResource::urlWithoutCredentials() const
{
    return urlWithoutCredentials(getUrl());
}

QString QnStorageResource::urlWithoutCredentials(const QString& url)
{
    if (!url.contains("://"))
        return url;

    QUrl result(url);
    result.setUserName(QString());
    result.setPassword(QString());

    return result.toString();
}

void QnStorageResource::updateInternal(const QnResourcePtr& source, NotifierList& notifiers)
{
    NX_ASSERT(
        source->getParentId() == getParentId() && source->getUrl() == getUrl(),
        "Abnormal storage resource update. This parentId: %1, other parentId: %2"
        "This url: %3, other url: %4",
        getParentId(), source->getParentId(), getUrl(), source->getUrl());

    base_type::updateInternal(source, notifiers);
    QnStorageResource* localOther = dynamic_cast<QnStorageResource*>(source.data());
    if (localOther)
    {
        if (m_usedForWriting != localOther->m_usedForWriting)
        {
            m_usedForWriting = localOther->m_usedForWriting;
            notifiers << [r = toSharedPointer(this)]{emit r->isUsedForWritingChanged(r);};
        }

        if (m_isBackup != localOther->m_isBackup)
        {
            m_isBackup = localOther->m_isBackup;
            notifiers << [r = toSharedPointer(this)]{ emit r->isBackupChanged(r); };
        }

        if (m_spaceLimit != localOther->m_spaceLimit)
        {
            m_spaceLimit = localOther->m_spaceLimit;
            notifiers << [r = toSharedPointer(this)](){ emit r->spaceLimitChanged(r); };
        }

        m_maxStoreTime = localOther->m_maxStoreTime;
    }
}

void QnStorageResource::setUrl(const QString& value)
{
    QnResource::setUrl(value);
}

void QnStorageResource::fillID()
{
    setIdUnsafe(fillID(getParentId(), getUrl()));
}

QnUuid QnStorageResource::fillID(const QnUuid& mserverId, const QString& url)
{
    QByteArray data1 = mserverId.toRfc4122();
    QByteArray data2 = url.toUtf8();
    return QnUuid::fromArbitraryData(data1.append(data2));
}

bool QnStorageResource::isExternal() const
{
    QString storageUrl = getUrl();
    return storageUrl.trimmed().startsWith("\\\\")
        || QUrl(storageUrl).path().mid(1).startsWith("\\\\")
        || storageUrl.indexOf("://") != -1;
}

QString QnStorageResource::toNativeDirPath(const QString &dirPath)
{
    QString result = QDir::toNativeSeparators(dirPath);
    if(!result.endsWith(QDir::separator()))
        result.append(QDir::separator());
    return result;
}

void QnStorageResource::setBackup(bool value) {
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        if (m_isBackup == value)
            return;
        m_isBackup = value;
    }
    emit isBackupChanged(::toSharedPointer(this));
}

bool QnStorageResource::isBackup() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_isBackup;
}

bool QnStorageResource::isWritable() const
{
    return getCapabilities() & QnAbstractStorageResource::cap::WriteFile;
}

bool QnStorageResource::isDbReady() const
{
    return getCapabilities() & QnAbstractStorageResource::cap::DBReady;
}

QIODevice* QnStorageResource::open(const QString& fileName, QIODevice::OpenMode openMode)
{
    return openInternal(fileName, openMode);
}

void QnStorageResource::setRuntimeStatusFlags(nx::vms::api::StorageRuntimeFlags flags)
{
    m_runtimeStatusFlags = flags;
}

nx::vms::api::StorageRuntimeFlags QnStorageResource::runtimeStatusFlags() const
{
    return m_runtimeStatusFlags;
}

void QnStorageResource::setPersistentStatusFlags(nx::vms::api::StoragePersistentFlags flags)
{
    setProperty(
        ResourcePropertyKey::kPersistentStorageStatusFlagsKey,
        QString::fromStdString(nx::reflect::toString(flags)));
}

nx::vms::api::StoragePersistentFlags QnStorageResource::persistentStatusFlags() const
{
    const auto resultStr = getProperty(ResourcePropertyKey::kPersistentStorageStatusFlagsKey);
    if (resultStr.isEmpty())
        return nx::vms::api::StoragePersistentFlags();

    return nx::reflect::fromString<nx::vms::api::StoragePersistentFlags>(resultStr.toStdString());
}

void QnStorageResource::setStorageArchiveMode(nx::vms::api::StorageArchiveMode value)
{
    setProperty(ResourcePropertyKey::kStorageArchiveMode,
        value == kStorageArchiveModeDefaultValue
            ? QString()
            : QString::fromStdString(nx::reflect::toString(value))
    );
}

nx::vms::api::StorageArchiveMode QnStorageResource::storageArchiveMode() const
{
    return nx::reflect::fromString<nx::vms::api::StorageArchiveMode>(
        getProperty(ResourcePropertyKey::kStorageArchiveMode).toStdString(),
        kStorageArchiveModeDefaultValue);
}
