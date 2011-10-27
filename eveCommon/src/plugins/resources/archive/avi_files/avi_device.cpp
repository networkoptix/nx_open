#include "avi_device.h"
#include "util.h"
#include "avi_archive_delegate.h"
#include "../archive_stream_reader.h"
#include "../rtsp/rtsp_client_archive_delegate.h"

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

QnAbstractStreamDataProvider* QnAviResource::createDataProvider(ConnectionRole role)
{
    QnArchiveStreamReader* result = new QnArchiveStreamReader(this);
    if (role == QnResource::Role_Default) 
    {
        if (checkFlag(local))
            result->setArchiveDelegate(new QnAviArchiveDelegate());
        else
            result->setArchiveDelegate(new QnRtspClientArchiveDelegate());
    }
    else if (role == QnResource::Role_DirectConnection) {
        result->setArchiveDelegate(new QnAviArchiveDelegate());
    }
    else if (role == QnResource::Role_RemoveConnection) {
        result->setArchiveDelegate(new QnAviArchiveDelegate());
    }
    else {
        Q_ASSERT_X(1, Q_FUNC_INFO, "Can't instance MediaProvider. Unknown role");
    }
    return result;
}
