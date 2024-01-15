// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "avi_resource.h"

#include <QtCore/QDir>
#include <QtGui/QImage>

#include <core/resource/avi/single_shot_file_reader.h>
#include <core/resource/storage_resource.h>
#include <core/storage/file_storage/layout_storage_resource.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/utils/fs/file.h>
#include <nx/vms/time/timezone.h>

#include "avi_archive_delegate.h"
#include "filetypesupport.h"
#include "nov_archive_delegate.h"

namespace {

QnAspectRatio getAspectRatioFromImage(const QString& fileName)
{
    NX_ASSERT(FileTypeSupport::isImageFileExt(fileName), "File is not image!");

    QImage image;
    return (QFile::exists(fileName) && image.load(fileName)
        ? QnAspectRatio(image.width(), image.height())
        : QnAspectRatio());
}

} // namespace

QnAviResource::QnAviResource(const QString& file)
{
    //setUrl(QDir::cleanPath(file));
    setUrl(file);
    QString shortName = QFileInfo(file).fileName();
    setName(shortName.mid(shortName.indexOf(QLatin1Char('?'))+1));
    if (FileTypeSupport::isImageFileExt(file))
    {
        addFlags(Qn::still_image);
        removeFlags(Qn::video | Qn::audio);
        m_imageAspectRatio = getAspectRatioFromImage(file);
    }
    setIdUnsafe(QnUuid::fromArbitraryData(getUrl()));
}

QnAviResource::~QnAviResource()
{
}

QnAspectRatio QnAviResource::imageAspectRatio() const
{
    return m_imageAspectRatio;
}

bool QnAviResource::hasAviMetadata() const
{
    return m_aviMetadata.has_value();
}

const QnAviArchiveMetadata& QnAviResource::aviMetadata() const
{
    NX_ASSERT(m_aviMetadata.has_value());
    return m_aviMetadata.value();
}

QString QnAviResource::toString() const
{
    return getName();
}

QnAviArchiveDelegate* QnAviResource::createArchiveDelegate() const
{
    auto layoutFile = dynamic_cast<QnLayoutFileStorageResource*>(m_storage.data());
    QnAviArchiveDelegate* aviDelegate = layoutFile
        ? new QnNovArchiveDelegate()
        : new QnAviArchiveDelegate();
    if (m_storage)
        aviDelegate->setStorage(m_storage);
    return aviDelegate;
}

QnAbstractStreamDataProvider* QnAviResource::createDataProvider(
    const QnResourcePtr& resource,
    Qn::ConnectionRole /*role*/)
{
    const auto aviResource = resource.dynamicCast<QnAviResource>();
    NX_ASSERT(aviResource);
    if (!aviResource)
        return nullptr;

    if (FileTypeSupport::isImageFileExt(resource->getUrl()))
        return new QnSingleShotFileStreamreader(resource);

    QnArchiveStreamReader* result = new QnArchiveStreamReader(aviResource);
    result->setArchiveDelegate(aviResource->createArchiveDelegate());
    return result;
}

QnConstResourceVideoLayoutPtr QnAviResource::getVideoLayout(const QnAbstractStreamDataProvider* dataProvider)
{
    // Try to return cached value, which is calculated on first opening.
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_videoLayout)
            return m_videoLayout;
    }

    // Calculate using provided archive reader.
    const auto archiveReader = dynamic_cast<const QnArchiveStreamReader*>(dataProvider);
    if (archiveReader)
    {
        auto result = archiveReader->getDPVideoLayout();
        NX_ASSERT(result);

        if (result)
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_videoLayout = result;
            return result;
        }
    }

    // Calculate using delegate
    const QScopedPointer<QnAviArchiveDelegate> archiveDelegate(createArchiveDelegate());
    archiveDelegate->open(toSharedPointer(this));

    auto result = archiveDelegate->getVideoLayout();
    NX_ASSERT(result);

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_videoLayout = result;
    return result;
}

bool QnAviResource::hasVideo(const QnAbstractStreamDataProvider* dataProvider) const
{
    if (dataProvider)
        return dataProvider->hasVideo();

    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_hasVideo.has_value())
    {
        // Read actual data from the file. Warning: this can be very slow!
        const QScopedPointer<QnAviArchiveDelegate> archiveDelegate(createArchiveDelegate());
        archiveDelegate->open(toSharedPointer(this));
        m_hasVideo = archiveDelegate->hasVideo();
    }
    return *m_hasVideo;
}

AudioLayoutConstPtr QnAviResource::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const
{
    const QnArchiveStreamReader* archiveReader = dynamic_cast<const QnArchiveStreamReader*> (dataProvider);
    if (archiveReader)
        return archiveReader->getDPAudioLayout();

    return QnMediaResource::getAudioLayout();
}

void QnAviResource::setStorage(const QnStorageResourcePtr& storage)
{
    m_storage = storage;
}

QnStorageResourcePtr QnAviResource::getStorage() const
{
    return m_storage;
}

void QnAviResource::setMotionBuffer(const QnMetaDataLightVector& data, int channel)
{
    m_motionBuffer[channel] = data;
}

const QnMetaDataLightVector& QnAviResource::getMotionBuffer(int channel) const
{
    return m_motionBuffer[channel];
}

void QnAviResource::setTimeZone(const QTimeZone& value)
{
    m_timeZone = value;
}

QTimeZone QnAviResource::timeZone() const
{
    return m_timeZone;
}

void QnAviResource::setAviMetadata(const QnAviArchiveMetadata& value)
{
    bool isRotationChanged = false;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (!m_previousRotation.has_value())
            m_previousRotation = base_type::forcedRotation().value_or(0);

        m_aviMetadata = value;
        isRotationChanged = m_aviMetadata->rotation != m_previousRotation.value();
        m_previousRotation = m_aviMetadata->rotation;
    }

    setTimeZone(nx::vms::time::timeZone(
        value.timeZoneId,
        std::chrono::milliseconds(value.timeZoneOffset)));

    if (isRotationChanged)
        emit rotationChanged();
}

nx::vms::api::dewarping::MediaData QnAviResource::getDewarpingParams() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_aviMetadata ? m_aviMetadata->dewarpingParams : base_type::getDewarpingParams();
}

std::optional<int> QnAviResource::forcedRotation() const
{
    const auto userSetRotation = base_type::forcedRotation();
    if (userSetRotation)
        return userSetRotation;

    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_aviMetadata)
        return m_aviMetadata->rotation;
    return std::nullopt;
}

void QnAviResource::setDewarpingParams(const nx::vms::api::dewarping::MediaData& params)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_aviMetadata)
    {
        if (m_aviMetadata->dewarpingParams == params)
            return;

        m_aviMetadata->dewarpingParams = params;
        lock.unlock();
        emit mediaDewarpingParamsChanged(toResourcePtr());
    }
    else
    {
        base_type::setDewarpingParams(params);
    }
}

QnAspectRatio QnAviResource::customAspectRatio() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_aviMetadata && !qFuzzyIsNull(m_aviMetadata->overridenAr)
        ? QnAspectRatio::closestStandardRatio(m_aviMetadata->overridenAr)
        : base_type::customAspectRatio();
}

bool QnAviResource::isEmbedded() const
{
    return (bool) m_storage.dynamicCast<QnLayoutFileStorageResource>();
}
