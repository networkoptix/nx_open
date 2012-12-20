#include "media_resource.h"
#include "resource_media_layout.h"
#include "plugins/resources/archive/archive_stream_reader.h"


QString QnStreamQualityToString(QnStreamQuality value) {
    switch(value) {
        case QnQualityLowest:
            return QObject::tr("Lowest");
        case QnQualityLow:
            return QObject::tr("Low");
        case QnQualityNormal:
            return QObject::tr("Normal");
        case QnQualityHigh:
            return QObject::tr("High");
        case QnQualityHighest:
            return QObject::tr("Highest");
        case QnQualityPreSet:
            return QObject::tr("Preset");
    default:
        break;
    }
    return QObject::tr("Undefined");
}

//QnDefaultMediaResourceLayout globalDefaultMediaResourceLayout;

QnMediaResource::QnMediaResource():
    QnResource()
{
    addFlags(QnResource::media);
    m_customVideoLayout = 0;
}

QnMediaResource::~QnMediaResource()
{
    delete m_customVideoLayout;
}

QImage QnMediaResource::getImage(int /*channel*/, QDateTime /*time*/, QnStreamQuality /*quality*/) const
{
    return QImage();
}

static QnDefaultResourceVideoLayout defaultVideoLayout;
const QnResourceVideoLayout* QnMediaResource::getVideoLayout(const QnAbstractMediaStreamDataProvider* dataProvider)
{
    QVariant val;
    getParam(QLatin1String("VideoLayout"), val, QnDomainMemory);
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
            m_customVideoLayout = QnCustomResourceVideoLayout::fromString(strVal);
        return m_customVideoLayout;
    }
}

static QnEmptyResourceAudioLayout audioLayout;
const QnResourceAudioLayout* QnMediaResource::getAudioLayout(const QnAbstractMediaStreamDataProvider* /*dataProvider*/)
{
    return &audioLayout;
}
