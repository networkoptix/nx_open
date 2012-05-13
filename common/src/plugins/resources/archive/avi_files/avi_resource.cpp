#include "avi_resource.h"
#include "avi_archive_delegate.h"
#include "../archive_stream_reader.h"
#include "../filetypesupport.h"
#include "../single_shot_file_reader.h"

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
    if (FileTypeSupport::isImageFileExt(getUrl())) {
        return new QnSingleShotFileStreamreader(toSharedPointer());
    }

    QnArchiveStreamReader* result = new QnArchiveStreamReader(toSharedPointer());
    result->setArchiveDelegate(new QnAviArchiveDelegate());
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
