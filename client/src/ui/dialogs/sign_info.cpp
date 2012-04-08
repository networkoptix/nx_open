#include "sign_info.h"
#include "ui/style/skin.h"
#include "sign_dialog.h"

QnSignInfo::QnSignInfo(QWidget* parent): QLabel(parent)
{
    m_textureWidth = 1920.0;
    m_textureHeight = 1080.0; // default aspect ratio 16:9 (exact size is not important here)
    m_signHelper.setLogo(qnSkin->pixmap("logo_1920_1080.png"));
}

void QnSignInfo::at_gotSignature(QByteArray calculatedSign, QByteArray signFromFrame)
{
    bool matched = calculatedSign == signFromFrame;

    QMutexLocker lock(&m_mutex);
    m_sign = calculatedSign;
    update();
}

void QnSignInfo::at_calcSignInProgress(QByteArray sign, int progress)
{
    QMutexLocker lock(&m_mutex);
    m_sign = sign;
    update();
}

void QnSignInfo::resizeEvent ( QResizeEvent * event )
{
    m_videoRect = SignDialog::calcVideoRect(width(), height(), m_textureWidth, m_textureHeight);
    update();
}

void QnSignInfo::paintEvent(QPaintEvent* event)
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_sign.isEmpty())
            return;
        m_signHelper.setSign(m_sign);
    }
    QPainter p(this);
    p.translate(m_videoRect.left(), m_videoRect.top());
    m_signHelper.draw(p, m_videoRect.size(), false);
}

void QnSignInfo::setImageSize(int textureWidth, int textureHeight)
{
    m_textureWidth = textureWidth;
    m_textureHeight = textureHeight;
    m_videoRect = SignDialog::calcVideoRect(width(), height(), m_textureWidth, m_textureHeight);
    update();
}
