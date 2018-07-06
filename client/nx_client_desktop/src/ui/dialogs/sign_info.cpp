#include "sign_info.h"

#include <QtCore/QDateTime>

#include <client_core/client_core_module.h>

#include "ui/style/skin.h"
#include "sign_dialog.h"

#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

QnSignInfo::QnSignInfo(QWidget* parent):
    QLabel(parent),
    m_signHelper(qnClientCoreModule->commonModule())
{
    m_finished = false;
    m_progress = 0;
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(update()));
    m_timer.start(16);
    m_DrawDetailText = false;
}

void QnSignInfo::at_gotSignature(QByteArray calculatedSign, QByteArray signFromFrame)
{
    {
        QnMutexLocker lock(&m_mutex);
        m_finished = true;
        m_sign = calculatedSign;
        m_signFromFrame = signFromFrame;
    }
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
    {
        QnMutexLocker lock(&m_mutex);
        m_sign = sign;
        m_progress = progress;
    }
    update();
}

void QnSignInfo::paintEvent(QPaintEvent*)
{
    static const int TEXT_FLASHING_PERIOD = 1000;
    float opacity = qAbs(sin(QDateTime::currentMSecsSinceEpoch() / qreal(TEXT_FLASHING_PERIOD * 2) * M_PI))*0.5 + 0.5;
    bool finished = false;
    QByteArray sign;
    QByteArray signFromFrame;

    // We gather a copy of all thread-shared data.
    // We are completely thread-safe after that.
    {
        QnMutexLocker lock(&m_mutex);
        finished = m_finished;
        sign = m_sign;
        signFromFrame = m_signFromFrame;
    }

    bool signIsMatched = (sign == signFromFrame);

    m_signHelper.setSign(sign);
    QPainter painter(this);

    if (finished)
        m_signHelper.setSignOpacity(opacity, signIsMatched ? QColor(0, 128, 0) : QColor(128, 0, 0));
    m_signHelper.draw(painter, QSize(width(), height()), m_DrawDetailText);

    QString text = tr("Analyzing: %1%").arg(m_progress);
    if (finished)
    {
        if (signFromFrame.isEmpty())
        {
            painter.setPen(QColor(128, 0, 0));
            text = tr("Watermark Not Found");
        }
        else if (signIsMatched)
        {
            painter.setPen(QColor(0, 128, 0));
            text = tr("Watermark Matched");
        }
        else
        {
            painter.setPen(QColor(128, 0, 0));
            text = tr("Invalid watermark");
        }
    }
    else
    {
        painter.setPen(Qt::black);
    }

    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    // Drawing video rect
    QFontMetrics metric = m_signHelper.updateFontSize(painter, QSize(width() * 2, height() * 2));
    if (finished)
        painter.setOpacity(opacity);
    if (!m_DrawDetailText)
        painter.drawText(16, metric.height() + 16, text);
}

void QnSignInfo::setImageSize(int textureWidth, int textureHeight)
{
    update();
}

void QnSignInfo::setDrawDetailTextMode(bool value)
{
    m_DrawDetailText = value;
}
