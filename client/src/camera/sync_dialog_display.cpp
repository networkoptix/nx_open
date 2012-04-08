#include "sync_dialog_display.h"
#include "export/sign_helper.h"
#include "utils/common/synctime.h"
#include "plugins/resources/archive/archive_stream_reader.h"

QnSignDialogDisplay::QnSignDialogDisplay(): CLCamDisplay(false) 
{
    m_firstFrameDisplayed = false;
    m_mdctx = 0;
    m_lastDisplayTime = 0;
    m_lastDisplayTime2 = 0;
    m_eofProcessed = false;
    const EVP_MD *md = EVP_get_digestbyname(EXPORT_SIGN_METHOD);
    if (md)
    {
        m_mdctx = new EVP_MD_CTX;
        EVP_MD_CTX_init(m_mdctx);
        EVP_DigestInit_ex(m_mdctx, md, NULL);

        m_tmpMdCtx = new EVP_MD_CTX;
        EVP_MD_CTX_init(m_tmpMdCtx);
        EVP_DigestInit_ex(m_mdctx, md, NULL);

        EVP_DigestUpdate(m_mdctx, EXPORT_SIGN_MAGIC, sizeof(EXPORT_SIGN_MAGIC));
    }
}

QnSignDialogDisplay::~QnSignDialogDisplay()
{
    delete m_mdctx;
    delete m_tmpMdCtx;
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
                m_display[0]->dispay(m_prevFrame, true, CLVideoDecoderOutput::factor_any); // repeat last frame on the screen (may be skipped because of time check)
            QSharedPointer<CLVideoDecoderOutput> lastFrame = m_display[0]->flush(CLVideoDecoderOutput::factor_any, 0);

            quint8 md_value[EVP_MAX_MD_SIZE];
            quint32 md_len = 0;
            EVP_DigestFinal_ex(m_mdctx, md_value, &md_len);
            QByteArray calculatedSign((const char*)md_value, md_len);

            QnSignHelper signHelper;
            QByteArray signFromPicture = signHelper.getSign(lastFrame.data(), md_len);
            emit calcSignInProgress(calculatedSign, 100);
            emit gotSignature(calculatedSign, signFromPicture);
        }
    }
    else if (video) 
    {
        video->channelNumber = 0; // ignore layout info
        if (m_mdctx && m_prevFrame && m_prevFrame->data.size() > 4) {
            const quint8* data = (const quint8*) m_prevFrame->data.data();
            QnSignHelper::updateDigest(m_prevFrame->context->ctx(), m_mdctx, data, m_prevFrame->data.size());
        }

        if (!m_firstFrameDisplayed || (video->flags & AV_PKT_FLAG_KEY) && qnSyncTime->currentMSecsSinceEpoch() - m_lastDisplayTime > 100) // max display rate 10 fps
        {
            CLVideoStreamDisplay::FrameDisplayStatus status = m_display[0]->dispay(video, true, CLVideoDecoderOutput::factor_any);
            if (!m_firstFrameDisplayed)
                m_firstFrameDisplayed = status == CLVideoStreamDisplay::Status_Displayed;
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
            quint8 md_value[EVP_MAX_MD_SIZE];
            quint32 md_len = 0;
            EVP_MD_CTX_copy_ex(m_tmpMdCtx, m_mdctx);
            EVP_DigestFinal_ex(m_tmpMdCtx, md_value, &md_len);

            QByteArray calculatedSign((const char*)md_value, md_len);
            QnArchiveStreamReader* reader = dynamic_cast<QnArchiveStreamReader*> (video->dataProvider);
            int progress =  (video->timestamp - reader->startTime()) / (double) (reader->endTime() - reader->startTime()) * 100;
            m_lastDisplayTime2 = qnSyncTime->currentMSecsSinceEpoch();

            emit calcSignInProgress(calculatedSign, progress);
        }
        m_prevFrame = video;
    }

    return true;
}

bool QnSignDialogDisplay::canAcceptData() const 
{
    return QnAbstractDataConsumer::canAcceptData();
}
