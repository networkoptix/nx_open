// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <export/signer.h>

#include <camera/cam_display.h>

class QnArchiveStreamReader;

class QnSignDialogDisplay: public QnCamDisplay
{
    Q_OBJECT
public:
    QnSignDialogDisplay(QnMediaResourcePtr resource);
    virtual ~QnSignDialogDisplay();
signals:
    void gotSignature(QByteArray calculatedSign, QByteArray signFromPicture);
    void gotSignatureDescription(QString version, QString hwId, QString licensedTo);
    void calcSignInProgress(QByteArray calculatedSign, int progress);
    void gotImageSize(int width, int height);
protected:
    virtual bool canAcceptData() const override;
    virtual bool processData(const QnAbstractDataPacketPtr& data) override;
    void finalizeSign();
private:
    MediaSigner m_mediaSigner;
    QnCompressedVideoDataPtr m_lastKeyFrame;
    bool m_eofProcessed;
    qint64 m_lastDisplayTime;
    qint64 m_lastDisplayTime2;
    QSize m_prevImageSize;
    bool m_firstFrameDisplayed;
    QnArchiveStreamReader* m_reader;
    bool m_hasProcessedMedia;
};
