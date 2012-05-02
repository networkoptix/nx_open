#ifndef __SIGN_INGO_H__
#define __SIGN_INGO_H__

#include <QLabel>
#include "export/sign_helper.h"

class QnSignInfo: public QLabel
{
    Q_OBJECT
public:
    QnSignInfo(QWidget* parent = 0);

    void setImageSize(int width, int height);

protected:
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void paintEvent(QPaintEvent *event) override;

public slots:
    void at_gotSignature(QByteArray calculatedSign, QByteArray signFromFrame);
    void at_calcSignInProgress(QByteArray sign, int progress);

private:
    QnSignHelper m_signHelper;
    QByteArray m_sign;
    QByteArray m_signFromFrame;
    QMutex m_mutex;
    QRect m_videoRect;
    double m_textureWidth;
    double m_textureHeight;
    bool m_finished;
    int m_progress;
    QTimer m_timer;
};

#endif // __SIGN_INGO_H__
