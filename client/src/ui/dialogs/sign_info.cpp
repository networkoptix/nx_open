#include "sign_info.h"
#include "ui/style/skin.h"
#include "sign_dialog.h"

QnSignInfo::QnSignInfo(QWidget* parent): QLabel(parent)
{
    m_finished = false;
    m_textureWidth = 1920.0;
    m_textureHeight = 1080.0; // default aspect ratio 16:9 (exact size is not important here)
    m_progress = 0;
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(update()));
    m_timer.start(16);
    m_DrawDetailText = false;
}

void QnSignInfo::at_gotSignature(QByteArray calculatedSign, QByteArray signFromFrame)
{
    m_finished = true;

    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    m_sign = calculatedSign;
    m_signFromFrame = signFromFrame;
    update();
}

void QnSignInfo::at_gotSignatureDescription(QString version, QString hwId, QString licensedTo)
{
    m_signHelper.setVersionStr(version);
    m_signHelper.setHwIdStr(hwId);
    m_signHelper.setLicensedToStr(licensedTo);
}

void QnSignInfo::at_calcSignInProgress(QByteArray sign, int progress)
{
    SCOPED_MUTEX_LOCK( lock, &m_mutex);
    m_sign = sign;
    m_progress = progress;
    update();
}

void QnSignInfo::resizeEvent(QResizeEvent *)
{
    m_videoRect = SignDialog::calcVideoRect(width(), height(), m_textureWidth, m_textureHeight);
    update();
}

void QnSignInfo::paintEvent(QPaintEvent *)
{
    //QTime t;
    //t.restart();

    static const int TEXT_FLASHING_PERIOD = 1000;
    float opacity = qAbs(sin(QDateTime::currentMSecsSinceEpoch() / qreal(TEXT_FLASHING_PERIOD * 2) * M_PI))*0.5 + 0.5;

    {
        SCOPED_MUTEX_LOCK( lock, &m_mutex);
        //if (m_sign.isEmpty())
        //    return;
        m_signHelper.setSign(m_sign);
    }
    QPixmap pixmap(m_textureWidth, m_textureHeight);
    QPainter p(&pixmap);

    if (m_finished)
        m_signHelper.setSignOpacity(opacity, m_sign == m_signFromFrame ? QColor(0,128,0) : QColor(128,0,0));
    m_signHelper.draw(p, QSize(m_textureWidth, m_textureHeight), m_DrawDetailText);
    QString text;
    text = tr("Analyzing: %1%").arg(m_progress);
    p.end();

    //qDebug() << "t1=" << t.elapsed();

    QPainter p2(this);
    if (m_finished)
    {
        if (m_signFromFrame.isEmpty()) {
            p2.setPen(QColor(128,0,0));
            text = tr("Watermark not found");
        }
        else if (m_sign == m_signFromFrame) {
            p2.setPen(QColor(0,128,0));
            text = tr("Watermark matched");
        }
        else {
            p2.setPen(QColor(128,0,0));
            text = tr("Invalid watermark");
        }
    }
    else {
        p2.setPen(Qt::black);
    }

    p2.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    p2.drawPixmap(m_videoRect, pixmap);
    QFontMetrics metric = m_signHelper.updateFontSize(p2, QSize(width()*2, height()*2));
    if (m_finished)
        p2.setOpacity(opacity);
    if (!m_DrawDetailText)
        p2.drawText(m_videoRect.left() + 16, m_videoRect.top() + metric.height() + 16, text);

    //qDebug() << "t2=" << t.elapsed();
}

void QnSignInfo::setImageSize(int textureWidth, int textureHeight)
{
    m_textureWidth = textureWidth;
    m_textureHeight = textureHeight;
    m_videoRect = SignDialog::calcVideoRect(width(), height(), m_textureWidth, m_textureHeight);
    update();
}

void QnSignInfo::setDrawDetailTextMode(bool value)
{
    m_DrawDetailText = value;
}
