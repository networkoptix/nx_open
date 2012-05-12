#include "avi_resource.h"
#include "avi_archive_delegate.h"
#include "../archive_stream_reader.h"
#include "../filetypesupport.h"

QnAviResource::QnAviResource(const QString& file)
{
    //setUrl(QDir::cleanPath(file));
    setUrl(file);
    setName(QFileInfo(file).fileName());
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
    QnArchiveStreamReader* result = new QnArchiveStreamReader(toSharedPointer());
    QnAviArchiveDelegate* aviDelegate = new QnAviArchiveDelegate();
    if (m_storage)
        aviDelegate->setStorage(m_storage);
    result->setArchiveDelegate(aviDelegate);
    if (checkFlags(still_image))
        result->setCycleMode(false);

    return result;
}

const QnVideoResourceLayout* QnAviResource::getVideoLayout(const QnAbstractMediaStreamDataProvider* dataProvider)
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
