#ifndef __SIGN_INGO_H__
#define __SIGN_INGO_H__

#include <QtWidgets/QLabel>
#include <QtCore/QTimer>
#include "export/sign_helper.h"

// TODO: #Elric not a dialog, a label! move to widgets!

class QnSignInfo: public QLabel
{
    Q_OBJECT
public:
    QnSignInfo(QWidget* parent = 0);

    void setImageSize(int width, int height);
    void setDrawDetailTextMode(bool value); // draw detail text instead of match info
protected:
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void paintEvent(QPaintEvent *event) override;

public slots:
    void at_gotSignature(QByteArray calculatedSign, QByteArray signFromFrame);
    void at_gotSignatureDescription(QString version, QString hwId, QString licensedTo);
    void at_calcSignInProgress(QByteArray sign, int progress);
private:
    QnSignHelper m_signHelper;
    QByteArray m_sign;
    QByteArray m_signFromFrame;
    QnMutex m_mutex;
    QRect m_videoRect;
    double m_textureWidth;
    double m_textureHeight;
    bool m_finished;
    int m_progress;
    QTimer m_timer;
    bool m_DrawDetailText;
};

#endif // __SIGN_INGO_H__
