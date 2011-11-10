#include "avi_device.h"
#include "avi_archive_delegate.h"
#include "../archive_stream_reader.h"


QnAviResource::QnAviResource(const QString& file)
{
    setUrl(QDir::cleanPath(file));
	m_name = QFileInfo(file).fileName();
}

void QnAviResource::deserialize(const QnResourceParameters& parameters)
{
    QnAbstractArchiveResource::deserialize(parameters);

    const char* FILE = "file";

    if (parameters.contains(FILE))
    {
        QString file = parameters[FILE];
        setUrl(QDir::cleanPath(file));
        m_name = QFileInfo(file).fileName();
    }
}

QnAviResource::~QnAviResource()
{

}

QString QnAviResource::toString() const
{
	return m_name;
}

QnAbstractStreamDataProvider* QnAviResource::createDataProviderInternal(ConnectionRole role)
{
    QnArchiveStreamReader* result = new QnArchiveStreamReader(toSharedPointer());

#ifdef RTSP_SERVER_TEST
    if (role == QnResource::Role_Default && !checkFlag(local)){
        result->setArchiveDelegate(new QnRtspClientArchiveDelegate());
        return result;
    }
#endif
    result->setArchiveDelegate(new QnAviArchiveDelegate());
    return result;
}

const QnVideoResourceLayout* QnAviResource::getVideoLayout(const QnAbstractMediaStreamDataProvider* dataProvider) const
{
    const QnArchiveStreamReader* archiveReader = dynamic_cast<const QnArchiveStreamReader*> (dataProvider);
    if (archiveReader)
        archiveReader->getDPVideoLayout();
    else
        return QnMediaResource::getVideoLayout();
}

const QnResourceAudioLayout* QnAviResource::getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider) const
{
    const QnArchiveStreamReader* archiveReader = dynamic_cast<const QnArchiveStreamReader*> (dataProvider);
    if (archiveReader)
        archiveReader->getDPAudioLayout();
    else
        return QnMediaResource::getAudioLayout();
}
