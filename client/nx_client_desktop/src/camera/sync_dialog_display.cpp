#include "sync_dialog_display.h"
#include "export/sign_helper.h"
#include "utils/common/synctime.h"
#include "nx/streaming/archive_stream_reader.h"
#include "core/resource/avi/avi_archive_delegate.h"

QnSignDialogDisplay::QnSignDialogDisplay(QnMediaResourcePtr resource):
    QnCamDisplay(resource, 0),
    m_mdctx(EXPORT_SIGN_METHOD)
{
    m_firstFrameDisplayed = false;
    m_lastDisplayTime = 0;
    m_lastDisplayTime2 = 0;
    m_eofProcessed = false;
    m_reader = 0;

    m_mdctx.addData(EXPORT_SIGN_MAGIC, sizeof(EXPORT_SIGN_MAGIC));
    m_hasProcessedMedia = false;
}

QnSignDialogDisplay::~QnSignDialogDisplay()
{
}

void QnSignDialogDisplay::finilizeSign()
{
    QByteArray signFromPicture;
    QByteArray calculatedSign;
#ifdef SIGN_FRAME_ENABLED
    calculatedSign = m_mdctx.result();
    QSharedPointer<CLVideoDecoderOutput> lastFrame = m_display[0]->flush(QnFrameScaler::factor_any, 0);
    QnSignHelper signHelper;
    signFromPicture = signHelper.getSign(lastFrame.data(), calculatedSign.size());
#else
    if (m_reader)
    {
        QnAviArchiveDelegate* aviFile = dynamic_cast<QnAviArchiveDelegate*> (m_reader->getArchiveDelegate());
        if (aviFile)
        {
            auto signPattern = aviFile->metadata().signature;
            if (!signPattern.isEmpty())
            {
                QByteArray baPattern = QByteArray(signPattern).trimmed();
                QByteArray magic = QnSignHelper::getSignMagic();
                QList<QByteArray> patternParams = baPattern.split(QnSignHelper::getSignPatternDelim());

                baPattern.replace(0, magic.size(), magic);
                QnSignHelper::updateDigest(nullptr, m_mdctx, (const quint8*)baPattern.data(), baPattern.size());
                calculatedSign = m_mdctx.result();

                signFromPicture = QnSignHelper::getDigestFromSign(patternParams[0]);
                if (patternParams.size() >= 4)
                {
                    emit gotSignatureDescription(QString::fromUtf8(patternParams[1]), QString::fromUtf8(patternParams[2]), QString::fromUtf8(patternParams[3]));
                }
            }
            else
            {
                calculatedSign = m_mdctx.result();
            }
        }
    }
#endif
    emit calcSignInProgress(calculatedSign, 100);
    emit gotSignature(calculatedSign, signFromPicture);
}

bool QnSignDialogDisplay::processData(const QnAbstractDataPacketPtr& data)
{
    QnArchiveStreamReader* reader = dynamic_cast<QnArchiveStreamReader*> (data->dataProvider);
    if (reader)
        m_reader = reader;

    QnEmptyMediaDataPtr eofPacket = std::dynamic_pointer_cast<QnEmptyMediaData>(data);

    QnAbstractMediaDataPtr media = std::dynamic_pointer_cast<QnAbstractMediaData>(data);
    QnCompressedVideoDataPtr video = std::dynamic_pointer_cast<QnCompressedVideoData>(data);
    QnCompressedAudioDataPtr audio = std::dynamic_pointer_cast<QnCompressedAudioData>(data);
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

        if (m_lastKeyFrame)
            m_display[0]->display(m_lastKeyFrame, true, QnFrameScaler::factor_any); // repeat last frame on the screen (may be skipped because of time check)
        if (m_hasProcessedMedia)
        {
            finilizeSign();
        }
        else
        {
            emit calcSignInProgress(QByteArray(), 100);
            emit gotSignature(QByteArray(), QByteArray());
        }
    }
    else if (video || audio)
    {
        m_hasProcessedMedia = true;
#ifndef SIGN_FRAME_ENABLED
        // update digest from current frame
        if (media && media->dataSize() > 4)
        {
            const quint8* data = (const quint8*)media->data();
            QnSignHelper::updateDigest(media->context, m_mdctx, data, static_cast<int>(media->dataSize()));
        }
#else
        // update digest from previous frames because of last frame it is sign frame itself
        if (m_prevFrame && m_prevFrame->data.size() > 4)
        {
            const quint8* data = (const quint8*)m_prevFrame->data.data();
            QnSignHelper::updateDigest(m_prevFrame->context, m_mdctx, data, m_prevFrame->data.size());
        }
#endif
        if (video)
        {
            video->channelNumber = 0; // ignore layout info
            if (!m_firstFrameDisplayed || ((video->flags & AV_PKT_FLAG_KEY) && qnSyncTime->currentMSecsSinceEpoch() - m_lastDisplayTime > 100)) // max display rate 10 fps
            {
                QnVideoStreamDisplay::FrameDisplayStatus status = m_display[0]->display(video, true, QnFrameScaler::factor_any);
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
            if (video->flags & AV_PKT_FLAG_KEY)
                m_lastKeyFrame = video;
        }
        if (qnSyncTime->currentMSecsSinceEpoch() - m_lastDisplayTime2 > 100) // max display rate 10 fps
        {
            nx::utils::QnCryptographicHash tmpctx = m_mdctx;
            QByteArray calculatedSign = tmpctx.result();
            int progress = 100;
            if (reader)
                progress = (media->timestamp - reader->startTime()) / (double)(reader->endTime() - reader->startTime()) * 100;
            m_lastDisplayTime2 = qnSyncTime->currentMSecsSinceEpoch();

            emit calcSignInProgress(calculatedSign, progress);
        }
        m_prevFrame = media;
    }

    if (video && (video->flags & QnAbstractMediaData::MediaFlags_StillImage))
        finilizeSign();

    return true;
}

bool QnSignDialogDisplay::canAcceptData() const
{
    return QnAbstractDataConsumer::canAcceptData();
}
