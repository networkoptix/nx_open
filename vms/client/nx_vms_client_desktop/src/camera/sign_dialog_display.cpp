// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sign_dialog_display.h"
#include "export/sign_helper.h"
#include "utils/common/synctime.h"
#include "nx/streaming/archive_stream_reader.h"
#include "core/resource/avi/avi_archive_delegate.h"
#include "core/resource/resource.h"

QnSignDialogDisplay::QnSignDialogDisplay(QnMediaResourcePtr resource):
    QnCamDisplay(resource, /*Stream reader*/ nullptr)
{
    m_firstFrameDisplayed = false;
    m_lastDisplayTime = 0;
    m_lastDisplayTime2 = 0;
    m_eofProcessed = false;
    m_reader = 0;
    m_hasProcessedMedia = false;
}

QnSignDialogDisplay::~QnSignDialogDisplay()
{
}

void QnSignDialogDisplay::finalizeSign()
{
    QByteArray signFromPicture;
    QByteArray calculatedSign;
    if (m_reader)
    {
        QnAviArchiveDelegate* aviFile = dynamic_cast<QnAviArchiveDelegate*> (m_reader->getArchiveDelegate());
        if (aviFile)
        {
            auto signPattern = aviFile->metadata()->signature;
            if (signPattern.isEmpty())
                signPattern = QnSignHelper::loadSignatureFromFileEnd(m_reader->getResource()->getUrl());
            if (!signPattern.isEmpty())
            {
                calculatedSign = m_mediaSigner.buildSignature(signPattern);
                QByteArray baPattern = QByteArray(signPattern).trimmed();
                QList<QByteArray> patternParams = baPattern.split(QnSignHelper::getSignPatternDelim());

                signFromPicture = QnSignHelper::getDigestFromSign(patternParams[0]);
                if (patternParams.size() >= 4)
                {
                    emit gotSignatureDescription(
                        QString::fromUtf8(patternParams[1]),
                        QString::fromUtf8(patternParams[2]),
                        QString::fromUtf8(patternParams[3]));
                }
            }
            else
            {
                calculatedSign = m_mediaSigner.currentResult();
            }
        }
    }

    emit calcSignInProgress(calculatedSign, 100);
    emit gotSignature(calculatedSign, signFromPicture);
}

bool QnSignDialogDisplay::processData(const QnAbstractDataPacketPtr& data)
{
    QnAbstractMediaDataPtr media = std::dynamic_pointer_cast<QnAbstractMediaData>(data);
    if (!media)
        return true;

    QnArchiveStreamReader* reader = dynamic_cast<QnArchiveStreamReader*> (media->dataProvider);
    if (reader)
        m_reader = reader;

    QnEmptyMediaDataPtr eofPacket = std::dynamic_pointer_cast<QnEmptyMediaData>(data);

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
            finalizeSign();
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
        auto codecParameters = media->context ? media->context->getAvCodecParameters() : nullptr;
        // update digest from current frame
        if (media && media->dataSize() > 4)
        {
            const quint8* data = (const quint8*)media->data();
            m_mediaSigner.processMedia(codecParameters, data, static_cast<int>(media->dataSize()));
        }
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
            QByteArray calculatedSign = m_mediaSigner.currentResult();
            int progress = 100;
            if (reader)
                progress = (media->timestamp - reader->startTime()) / (double)(reader->endTime() - reader->startTime()) * 100;
            m_lastDisplayTime2 = qnSyncTime->currentMSecsSinceEpoch();

            emit calcSignInProgress(calculatedSign, progress);
        }
    }

    if (video && (video->flags & QnAbstractMediaData::MediaFlags_StillImage))
        finalizeSign();

    return true;
}

bool QnSignDialogDisplay::canAcceptData() const
{
    return QnAbstractDataConsumer::canAcceptData();
}
