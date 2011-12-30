#include "media_resource.h"
#include "resource_media_layout.h"
#include "plugins/resources/archive/archive_stream_reader.h"


//QnDefaultMediaResourceLayout globalDefaultMediaResourceLayout;

QnMediaResource::QnMediaResource():
    QnResource()
{
    addFlag(QnResource::media);
    m_customVideoLayout = 0;
}

QnMediaResource::~QnMediaResource()
{
    delete m_customVideoLayout;
}

QImage QnMediaResource::getImage(int /*channnel*/, QDateTime /*time*/, QnStreamQuality /*quality*/) const
{
    return QImage();
}

static QnDefaultDeviceVideoLayout defaultVideoLayout;
const QnVideoResourceLayout* QnMediaResource::getVideoLayout(const QnAbstractMediaStreamDataProvider* dataProvider)
{
    QVariant val;
    getParam("VideoLayout", val, QnDomainMemory);
    QString strVal = val.toString();
    if (strVal.isEmpty())
    {
        const QnArchiveStreamReader* archive = dynamic_cast<const QnArchiveStreamReader*>(dataProvider);
        if (archive == 0)
            return &defaultVideoLayout;
        else
            return archive->getDPVideoLayout();
    }
    else {
        if (m_customVideoLayout == 0)
            m_customVideoLayout = CLCustomDeviceVideoLayout::fromString(strVal);
        return m_customVideoLayout;
    }
}

static QnEmptyAudioLayout audioLayout;
const QnResourceAudioLayout* QnMediaResource::getAudioLayout(const QnAbstractMediaStreamDataProvider* /*dataProvider*/)
{
    return &audioLayout;
}
