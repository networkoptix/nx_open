#include "video_camera.h"
#include "core/dataprovider/media_streamdataprovider.h"

QnVideoCamera::QnVideoCamera(QnResourcePtr resource): 
    QnResourceConsumer(resource),
    m_reader(0)
{

}

QnVideoCamera::~QnVideoCamera()
{
    delete m_reader;
}

void QnVideoCamera::beforeDisconnectFromResource()
{
    if (m_reader)
        m_reader->pleaseStop();
}

QnAbstractMediaStreamDataProvider* QnVideoCamera::getLiveReader()
{
    if (!m_reader) {
        m_reader = dynamic_cast<QnAbstractMediaStreamDataProvider*> (m_resource->createDataProvider(QnResource::Role_LiveVideo));
        m_reader->setQuality(QnQualityHighest);
    }
    return m_reader;
}
