#include "avi_resource.h"

#ifdef ENABLE_ARCHIVE

#include <QtCore/QDir>

#include "avi_archive_delegate.h"

#include <core/resource/storage_resource.h>
#include <utils/fs/file.h>

#include <plugins/resource/archive/archive_stream_reader.h>
#include <plugins/resource/avi/single_shot_file_reader.h>

#include "plugins/storage/file_storage/layout_storage_resource.h"
#include "nov_archive_delegate.h"

#include "filetypesupport.h"

QnAviResource::QnAviResource(const QString& file)
{
    //setUrl(QDir::cleanPath(file));
    setUrl(file);
    QString shortName = QFileInfo(file).fileName();
    setName(shortName.mid(shortName.indexOf(QLatin1Char('?'))+1));
    if (FileTypeSupport::isImageFileExt(file)) 
        addFlags(Qn::still_image);
    m_timeZoneOffset = Qn::InvalidUtcOffset;
    setId(guidFromArbitraryData(getUniqueId().toUtf8()));
}

QnAviResource::~QnAviResource()
{
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

#endif // ENABLE_ARCHIVE
