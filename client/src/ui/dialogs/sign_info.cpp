#include "sign_info.h"
#include "ui/style/skin.h"
#include "sign_dialog.h"

QnSignInfo::QnSignInfo(QWidget* parent): QLabel(parent)
{
    m_finished = false;
    m_textureWidth = 1920.0;
    m_textureHeight = 1080.0; // default aspect ratio 16:9 (exact size is not important here)
    m_progress = 0;
    m_signHelper.setLogo(qnSkin->pixmap("logo_1920_1080.png"));
}

void QnSignInfo::at_gotSignature(QByteArray calculatedSign, QByteArray signFromFrame)
{
    m_finished = true;

    QMutexLocker lock(&m_mutex);
    m_sign = calculatedSign;
    m_signFromFrame = signFromFrame;
    update();
}

void QnSignInfo::at_calcSignInProgress(QByteArray sign, int progress)
{
    QMutexLocker lock(&m_mutex);
    m_sign = sign;
    m_progress = progress;
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
    QString text;
    text = QString("Analizing: %1%").arg(m_progress);
    //int dots = (QDateTime::currentDateTime().time().second() % 3) + 1;
    //text += QString("..........").left(dots);

    p.setPen(Qt::black);
    if (m_finished)
    {
        if (m_signFromFrame.isEmpty()) {
            p.setPen(Qt::red);
            text = "Signature not found";
        }
        else if (m_sign == m_signFromFrame) {
            text = "Signature mached";
        }
        else {
            p.setPen(Qt::red);
            text = "Invalid signature";
        }
    }
    m_signHelper.drawTextLine(p, m_videoRect.size(), 0, text);
    
}

void QnSignInfo::setImageSize(int textureWidth, int textureHeight)
{
    m_textureWidth = textureWidth;
    m_textureHeight = textureHeight;
    m_videoRect = SignDialog::calcVideoRect(width(), height(), m_textureWidth, m_textureHeight);
    update();
}
