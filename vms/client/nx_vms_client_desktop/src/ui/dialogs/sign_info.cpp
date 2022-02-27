// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sign_info.h"

#include <QtCore/QDateTime>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlContext>

#include <client_core/client_core_module.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <ui/workbench/workbench_context.h>

#include "sign_dialog.h"

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

void QnSignInfo::at_gotSignature(
    QByteArray calculatedSign, QByteArray calculatedSign2, QByteArray signFromFrame)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_finished = true;
        m_sign = calculatedSign;
        m_sign2 = calculatedSign2;
        m_signFromFrame = signFromFrame;
        m_signIsMatched = m_sign == m_signFromFrame || m_sign2 == m_signFromFrame;
    }
    auto contextAware = dynamic_cast<QnWorkbenchContextAware*>(
        qnClientCoreModule->mainQmlEngine()->rootContext()->contextProperty("context")
        .value<QObject*>());

    if (contextAware)
    {
        nx::vms::client::desktop::ui::action::Parameters actionParameters;
        actionParameters.setArgument(Qn::ValidRole, m_signIsMatched);
        contextAware->context()->menu()->trigger(
            nx::vms::client::desktop::ui::action::WatermarkCheckedEvent, actionParameters);
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
        NX_MUTEX_LOCKER lock(&m_mutex);
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
    QByteArray sign2;
    QByteArray signFromFrame;
    bool signIsMatched;

    // We gather a copy of all thread-shared data.
    // We are completely thread-safe after that.
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        finished = m_finished;
        sign = m_sign;
        sign2 = m_sign2;
        signFromFrame = m_signFromFrame;
        signIsMatched = m_signIsMatched;
    }

    if (sign != signFromFrame)
        sign = sign2;

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

bool QnSignInfo::signIsMatched()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_signIsMatched;
}
