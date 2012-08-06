#include "sync_dialog_display.h"
#include "export/sign_helper.h"
#include "utils/common/synctime.h"
#include "plugins/resources/archive/archive_stream_reader.h"

QnSignDialogDisplay::QnSignDialogDisplay(): 
    QnCamDisplay(false),
    m_mdctx(EXPORT_SIGN_METHOD)
{
    m_firstFrameDisplayed = false;
    m_lastDisplayTime = 0;
    m_lastDisplayTime2 = 0;
    m_eofProcessed = false;

    m_mdctx.addData(EXPORT_SIGN_MAGIC, sizeof(EXPORT_SIGN_MAGIC));
}

QnSignDialogDisplay::~QnSignDialogDisplay()
{}

void QnSignDialogDisplay::finilizeSign()
{
    QSharedPointer<CLVideoDecoderOutput> lastFrame = m_display[0]->flush(QnFrameScaler::factor_any, 0);

    QByteArray calculatedSign = m_mdctx.result();

    QnSignHelper signHelper;
    QByteArray signFromPicture = signHelper.getSign(lastFrame.data(), calculatedSign.size());
    emit calcSignInProgress(calculatedSign, 100);
    emit gotSignature(calculatedSign, signFromPicture);
}

bool QnSignDialogDisplay::processData(QnAbstractDataPacketPtr data)
{
    QnEmptyMediaDataPtr eofPacket = qSharedPointerDynamicCast<QnEmptyMediaData>(data);
    QnCompressedVideoDataPtr video = qSharedPointerDynamicCast<QnCompressedVideoData>(data);

    if (m_prevSpeed != m_speed) 
    {
        processNewSpeed(m_speed);
        m_prevSpeed = m_speed;
    }

    if (eofPacket)
    {
        if (m_eofProcessed)
            return true;
        m_eofProcessed = true;

        if (m_prevFrame)
        {
            if (m_prevFrame->flags & AV_PKT_FLAG_KEY)
                m_display[0]->dispay(m_prevFrame, true, QnFrameScaler::factor_any); // repeat last frame on the screen (may be skipped because of time check)
            finilizeSign();
        }
        else {
            emit calcSignInProgress(QByteArray(), 100);
            emit gotSignature(QByteArray(), QByteArray());
        }
    }
    else if (video) 
    {
        video->channelNumber = 0; // ignore layout info
        if (m_prevFrame && m_prevFrame->data.size() > 4) {
            const quint8* data = (const quint8*) m_prevFrame->data.data();
            QnSignHelper::updateDigest(m_prevFrame->context->ctx(), m_mdctx, data, m_prevFrame->data.size());
        }

        if (!m_firstFrameDisplayed || ((video->flags & AV_PKT_FLAG_KEY) && qnSyncTime->currentMSecsSinceEpoch() - m_lastDisplayTime > 100)) // max display rate 10 fps
        {
            QnVideoStreamDisplay::FrameDisplayStatus status = m_display[0]->dispay(video, true, QnFrameScaler::factor_any);
            if (!m_firstFrameDisplayed)
                m_firstFrameDisplayed = status == QnVideoStreamDisplay::Status_Displayed;
            QSize imageSize = m_display[0]->getImageSize();
            if (imageSize != m_prevImageSize)
            {
                m_prevImageSize = imageSize;
                emit gotImageSize(imageSize.width(), imageSize.height());
            }
            m_lastDisplayTime = qnSyncTime->currentMSecsSinceEpoch();
        }
        if (qnSyncTime->currentMSecsSinceEpoch() - m_lastDisplayTime2 > 100) // max display rate 10 fps
        {
            QnCryptographicHash tmpctx = m_mdctx;
            QByteArray calculatedSign = tmpctx.result();
            QnArchiveStreamReader* reader = dynamic_cast<QnArchiveStreamReader*> (video->dataProvider);
            int progress = 100;
            if (reader)
                progress =  (video->timestamp - reader->startTime()) / (double) (reader->endTime() - reader->startTime()) * 100;
            m_lastDisplayTime2 = qnSyncTime->currentMSecsSinceEpoch();

            emit calcSignInProgress(calculatedSign, progress);
        }
        m_prevFrame = video;
    }

    if (video && (video->flags & QnAbstractMediaData::MediaFlags_StillImage))
        finilizeSign();

    return true;
}

bool QnSignDialogDisplay::canAcceptData() const 
{
    return QnAbstractDataConsumer::canAcceptData();
}
