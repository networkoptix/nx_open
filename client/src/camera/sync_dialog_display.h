#ifndef __SIGN_DIALOG_DISPLAY_H__
#define __SIGN_DIALOG_DISPLAY_H__

#include "camera/camdisplay.h"
#include <openssl/evp.h>

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
private:
    EVP_MD_CTX* m_mdctx;
    EVP_MD_CTX* m_tmpMdCtx;
    QnCompressedVideoDataPtr m_prevFrame;
    bool m_eofProcessed;
    qint64 m_lastDisplayTime;
    qint64 m_lastDisplayTime2;
    QSize m_prevImageSize;
    bool m_firstFrameDisplayed;
};

#endif //  __SIGN_DIALOG_DISPLAY_H__
