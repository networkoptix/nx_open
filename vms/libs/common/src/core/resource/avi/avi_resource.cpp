#include "avi_resource.h"

#include <QtCore/QDir>
#include <QtGui/QImage>

#include "avi_archive_delegate.h"

#include <core/resource/storage_resource.h>
#include <utils/fs/file.h>

#include <nx/streaming/archive_stream_reader.h>
#include <core/resource/avi/single_shot_file_reader.h>

#include "core/storage/file_storage/layout_storage_resource.h"
#include "nov_archive_delegate.h"

#include "filetypesupport.h"

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

QnAviResource::QnAviResource(const QString& file, QnCommonModule* commonModule)
{
    //setUrl(QDir::cleanPath(file));
    setCommonModule(commonModule);
    setUrl(file);
    QString shortName = QFileInfo(file).fileName();
    setName(shortName.mid(shortName.indexOf(QLatin1Char('?'))+1));
    if (FileTypeSupport::isImageFileExt(file))
    {
        addFlags(Qn::still_image);
        m_imageAspectRatio = getAspectRatioFromImage(file);
    }
    m_timeZoneOffset = Qn::InvalidUtcOffset;
    setId(guidFromArbitraryData(getUniqueId().toUtf8()));
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
    return m_aviMetadata.is_initialized();
}

const QnAviArchiveMetadata& QnAviResource::aviMetadata() const
{
    NX_ASSERT(m_aviMetadata.is_initialized());
    return m_aviMetadata.get();
}

QString QnAviResource::toString() const
{
    return getName();
}

QnAviArchiveDelegate* QnAviResource::createArchiveDelegate() const
{
    QnLayoutFileStorageResource* layoutFile = dynamic_cast<QnLayoutFileStorageResource*>(m_storage.data());
    QnAviArchiveDelegate* aviDelegate = layoutFile ? new QnNovArchiveDelegate() : new QnAviArchiveDelegate();
    if (m_storage)
        aviDelegate->setStorage(m_storage);
    return aviDelegate;
}

QnAbstractStreamDataProvider* QnAviResource::createDataProvider(
    const QnResourcePtr& resource,
    Qn::ConnectionRole /*role*/)
{
    if (FileTypeSupport::isImageFileExt(resource->getUrl()))
        return new QnSingleShotFileStreamreader(resource);

    const auto aviResource = resource.dynamicCast<QnAviResource>();
    NX_ASSERT(aviResource);
    if (!aviResource)
        return nullptr;

    QnArchiveStreamReader* result = new QnArchiveStreamReader(aviResource);
    result->setArchiveDelegate(aviResource->createArchiveDelegate());
    return result;
}

QnConstResourceVideoLayoutPtr QnAviResource::getVideoLayout(const QnAbstractStreamDataProvider* dataProvider) const
{
    const QnArchiveStreamReader* archiveReader = dynamic_cast<const QnArchiveStreamReader*> (dataProvider);
    if (archiveReader)
    {
        QnMutexLocker lock(&m_mutex);
        if (m_videoLayout)
            return m_videoLayout;
        lock.unlock();
        auto result = archiveReader->getDPVideoLayout();
        lock.relock();
        m_videoLayout = result;
        return result;
    }

    return QnMediaResource::getVideoLayout();
}

bool QnAviResource::hasVideo(const QnAbstractStreamDataProvider* dataProvider) const
{
    if (dataProvider)
        return dataProvider->hasVideo();

    if (!m_hasVideo.is_initialized())
    {
        const QScopedPointer<QnAviArchiveDelegate> archiveDelegate(createArchiveDelegate());
        archiveDelegate->open(toSharedPointer(this));
        m_hasVideo = archiveDelegate->hasVideo();
    }
    return *m_hasVideo;
}

QnConstResourceAudioLayoutPtr QnAviResource::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const
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

void QnAviResource::setTimeZoneOffset(qint64 value)
{
    m_timeZoneOffset = value;
}

qint64 QnAviResource::timeZoneOffset() const
{
    return m_timeZoneOffset;
}

void QnAviResource::setAviMetadata(const QnAviArchiveMetadata& value)
{
    QnMutexLocker lock(&m_mutex);
    m_aviMetadata.reset(value);
}

QnMediaDewarpingParams QnAviResource::getDewarpingParams() const
{
    QnMutexLocker lock(&m_mutex);
    return m_aviMetadata ? m_aviMetadata->dewarpingParams : base_type::getDewarpingParams();
}

void QnAviResource::setDewarpingParams(const QnMediaDewarpingParams& params)
{
    QnMutexLocker lock(&m_mutex);
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
    QnMutexLocker lock(&m_mutex);
    return m_aviMetadata && !qFuzzyIsNull(m_aviMetadata->overridenAr)
        ? QnAspectRatio::closestStandardRatio(m_aviMetadata->overridenAr)
        : base_type::customAspectRatio();
}

bool QnAviResource::isEmbedded() const
{
    return m_storage.dynamicCast<QnLayoutFileStorageResource>();
}
