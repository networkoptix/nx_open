#include "avi_resource.h"
#include "avi_archive_delegate.h"

#include <core/resource/storage_resource.h>

#include "../archive_stream_reader.h"
#include "../filetypesupport.h"
#include "../single_shot_file_reader.h"


QnAviResource::QnAviResource(const QString& file)
{
    //setUrl(QDir::cleanPath(file));
    setUrl(file);
    QString shortName = QFileInfo(file).fileName();
    setName(shortName.mid(shortName.indexOf(QLatin1Char('?'))+1));
    memset(m_motionBuffer, 0, sizeof(m_motionBuffer));
}

QnAviResource::~QnAviResource()
{
}

void QnAviResource::deserialize(const QnResourceParameters& parameters)
{
    QnAbstractArchiveResource::deserialize(parameters);

    if (parameters.contains(QLatin1String("file")))
    {
        QString file = parameters[QLatin1String("file")];
        setUrl(QDir::cleanPath(file));
        setName(QFileInfo(file).fileName());
    }
}

QString QnAviResource::toString() const
{
    return getName();
}

QnAbstractStreamDataProvider* QnAviResource::createDataProviderInternal(ConnectionRole /*role*/)
{
    if (FileTypeSupport::isImageFileExt(getUrl())) {
        return new QnSingleShotFileStreamreader(toSharedPointer());
    }

    QnArchiveStreamReader* result = new QnArchiveStreamReader(toSharedPointer());
    QnAviArchiveDelegate* aviDelegate = new QnAviArchiveDelegate();
    if (m_storage)
        aviDelegate->setStorage(m_storage);
    result->setArchiveDelegate(aviDelegate);
    if (hasFlags(still_image))
        result->setCycleMode(false);

    return result;
}

const QnResourceVideoLayout* QnAviResource::getVideoLayout(const QnAbstractMediaStreamDataProvider* dataProvider)
{
    const QnArchiveStreamReader* archiveReader = dynamic_cast<const QnArchiveStreamReader*> (dataProvider);
    if (archiveReader)
        return archiveReader->getDPVideoLayout();

    return QnMediaResource::getVideoLayout();
}

const QnResourceAudioLayout* QnAviResource::getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider)
{
    const QnArchiveStreamReader* archiveReader = dynamic_cast<const QnArchiveStreamReader*> (dataProvider);
    if (archiveReader)
        return archiveReader->getDPAudioLayout();

    return QnMediaResource::getAudioLayout();
}

void QnAviResource::setStorage(QnStorageResourcePtr storage)
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
