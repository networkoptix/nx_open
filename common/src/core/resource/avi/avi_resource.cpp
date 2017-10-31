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

QnAviResource::QnAviResource(const QString& file)
{
    //setUrl(QDir::cleanPath(file));
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
    NX_EXPECT(m_aviMetadata.is_initialized());
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

QnAbstractStreamDataProvider* QnAviResource::createDataProviderInternal(Qn::ConnectionRole /*role*/)
{
    if (FileTypeSupport::isImageFileExt(getUrl())) {
        return new QnSingleShotFileStreamreader(toSharedPointer());
    }

    QnArchiveStreamReader* result = new QnArchiveStreamReader(toSharedPointer());

    result->setArchiveDelegate(createArchiveDelegate());
    return result;
}

QnConstResourceVideoLayoutPtr QnAviResource::getVideoLayout(const QnAbstractStreamDataProvider* dataProvider) const
{
    const QnArchiveStreamReader* archiveReader = dynamic_cast<const QnArchiveStreamReader*> (dataProvider);
    if (archiveReader)
        return archiveReader->getDPVideoLayout();

    return QnMediaResource::getVideoLayout();
}

bool QnAviResource::hasVideo(const QnAbstractStreamDataProvider* dataProvider) const
{
    return dataProvider ? dataProvider->hasVideo() : true;
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

qreal QnAviResource::customAspectRatio() const
{
    QnMutexLocker lock(&m_mutex);
    return m_aviMetadata ? m_aviMetadata->overridenAr : base_type::customAspectRatio();
}
