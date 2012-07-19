#ifndef __SIGN_DIALOG_DISPLAY_H__
#define __SIGN_DIALOG_DISPLAY_H__

#include <utils/common/cryptographic_hash.h>

#include <camera/camdisplay.h>

class QnSignDialogDisplay: public CLCamDisplay
{
    Q_OBJECT
public:
    QnSignDialogDisplay();
    virtual ~QnSignDialogDisplay();
signals:
    void gotSignature(QByteArray calculatedSign, QByteArray signFromPicture);
    void calcSignInProgress(QByteArray calculatedSign, int progress);
    void gotImageSize(int width, int height);
protected:
    virtual bool canAcceptData() const override;
    virtual bool processData(QnAbstractDataPacketPtr data) override;
    void finilizeSign();
private:
    QnCryptographicHash m_mdctx;
    QnCompressedVideoDataPtr m_prevFrame;
    bool m_eofProcessed;
    qint64 m_lastDisplayTime;
    qint64 m_lastDisplayTime2;
    QSize m_prevImageSize;
    bool m_firstFrameDisplayed;
};

#endif //  __SIGN_DIALOG_DISPLAY_H__
