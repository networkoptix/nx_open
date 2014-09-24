#ifndef __SIGN_DIALOG_DISPLAY_H__
#define __SIGN_DIALOG_DISPLAY_H__

#include <utils/common/cryptographic_hash.h>

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
    void finilizeSign();
private:
    QnCryptographicHash m_mdctx;
    QnAbstractMediaDataPtr m_prevFrame;
    QnCompressedVideoDataPtr m_lastKeyFrame;
    bool m_eofProcessed;
    qint64 m_lastDisplayTime;
    qint64 m_lastDisplayTime2;
    QSize m_prevImageSize;
    bool m_firstFrameDisplayed;
    QnArchiveStreamReader* m_reader;
};

#endif //  __SIGN_DIALOG_DISPLAY_H__
