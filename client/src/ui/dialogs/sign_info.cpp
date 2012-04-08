#include "sign_info.h"
#include "ui/style/skin.h"
#include "sign_dialog.h"
#include "utils/common/synctime.h"

QnSignInfo::QnSignInfo(QWidget* parent): QLabel(parent)
{
    m_finished = false;
    m_textureWidth = 1920.0;
    m_textureHeight = 1080.0; // default aspect ratio 16:9 (exact size is not important here)
    m_progress = 0;
    m_signHelper.setLogo(qnSkin->pixmap("logo_1920_1080.png"));
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(update()));
    m_timer.start(16);
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
    static const int TEXT_FLASHING_PERIOD = 1000;

    {
        QMutexLocker lock(&m_mutex);
        if (m_sign.isEmpty())
            return;
        m_signHelper.setSign(m_sign);
    }
    QPixmap pixmap(m_textureWidth, m_textureHeight);
    QPainter p(&pixmap);

    m_signHelper.draw(p, QSize(m_textureWidth, m_textureHeight), false);
    QString text;
    text = QString("Analyzing: %1%").arg(m_progress);
    //int dots = (QDateTime::currentDateTime().time().second() % 3) + 1;
    //text += QString("..........").left(dots);

    //m_signHelper.drawTextLine(p, QSize(m_textureWidth, m_textureHeight), 1, text);
    p.end();

    QPainter p2(this);
    p2.setPen(Qt::black);
    if (m_finished)
    {
        if (m_signFromFrame.isEmpty()) {
            p2.setPen(QColor(128,0,0));
            text = "Signature not found";
        }
        else if (m_sign == m_signFromFrame) {
            p2.setPen(QColor(0,128,0));
            text = "Signature mached";
        }
        else {
            p2.setPen(QColor(128,0,0));
            text = "Invalid signature";
        }
        p2.setOpacity(qAbs(sin(qnSyncTime->currentMSecsSinceEpoch() / qreal(TEXT_FLASHING_PERIOD * 2) * M_PI))*0.5 + 0.5);
    }

    p2.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    p2.drawPixmap(m_videoRect, pixmap);
    QFontMetrics metric = m_signHelper.updateFontSize(p2, QSize(width()*2, height()*2));
    p2.drawText(m_videoRect.left() + 16, m_videoRect.top() + metric.height() + 16, text);
    
}

void QnSignInfo::setImageSize(int textureWidth, int textureHeight)
{
    m_textureWidth = textureWidth;
    m_textureHeight = textureHeight;
    m_videoRect = SignDialog::calcVideoRect(width(), height(), m_textureWidth, m_textureHeight);
    update();
}
