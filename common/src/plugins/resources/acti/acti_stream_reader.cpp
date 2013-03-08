#include <QTextStream>
#include "acti_resource.h"
#include "acti_stream_reader.h"
#include "utils/common/sleep.h"
#include "utils/common/synctime.h"
#include "utils/media/nalUnits.h"
#include "utils/network/tcp_connection_priv.h"

QnActiStreamReader::QnActiStreamReader(QnResourcePtr res):
    CLServerPushStreamreader(res),
    m_multiCodec(res)
{
    m_actiRes = res.dynamicCast<QnActiResource>();
}

QnActiStreamReader::~QnActiStreamReader()
{
    stop();
}

void QnActiStreamReader::openStream()
{
    // set fps
    //http://192.168.0.100/cgi-bin/encoder?USER=Admin&PWD=123456&CHANNEL=2&VIDEO_FPS_NUM

    // set resolution
    // http://192.168.0.100/cgi-bin/encoder?USER=Admin&PWD=123456&CHANNEL=2&VIDEO_RESOLUTION

    QString streamUrl = m_actiRes->getRtspUrl(m_role == QnResource::Role_LiveVideo ? 1 : 2);

    m_multiCodec.setRequest(streamUrl);
    m_multiCodec.openStream();
    if (m_multiCodec.getLastResponseCode() == CODE_AUTH_REQUIRED)
        m_resource->setStatus(QnResource::Unauthorized);

}

void QnActiStreamReader::closeStream()
{
    m_multiCodec.closeStream();
}

bool QnActiStreamReader::isStreamOpened() const
{
    return m_multiCodec.isStreamOpened();
}


void QnActiStreamReader::pleaseStop()
{
    QnLongRunnable::pleaseStop();
    m_multiCodec.pleaseStop();
}

QnAbstractMediaDataPtr QnActiStreamReader::getNextData()
{
    if (!isStreamOpened()) {
        openStream();
        if (!isStreamOpened())
            return QnAbstractMediaDataPtr(0);
    }

    if (needMetaData())
        return getMetaData();

    QnAbstractMediaDataPtr rez;
    for (int i = 0; i < 2 && !rez; ++i)
        rez = m_multiCodec.getNextData();

    if (!rez)
        closeStream();

    return rez;
}

void QnActiStreamReader::updateStreamParamsBasedOnQuality()
{
    if (isRunning())
        pleaseReOpen();
}

void QnActiStreamReader::updateStreamParamsBasedOnFps()
{
    if (isRunning())
        pleaseReOpen();
}

const QnResourceAudioLayout* QnActiStreamReader::getDPAudioLayout() const
{
    return m_multiCodec.getAudioLayout();
}
