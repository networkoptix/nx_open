// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sign_info.h"

#include <QtCore/QDateTime>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>

#include <client_core/client_core_module.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/window_context.h>

#include "sign_dialog.h"

using namespace nx::vms::client::desktop;

QnSignInfo::QnSignInfo(QWidget* parent):
    QLabel(parent),
    QnWorkbenchContextAware(parent),
    m_signHelper()
{
    m_finished = false;
    m_progress = 0;
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(update()));
    m_timer.start(16);
    m_DrawDetailText = false;
}

void QnSignInfo::at_gotSignature(
    QByteArray calculatedSign, QByteArray signFromFrame)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_finished = true;
        m_sign = calculatedSign;
        m_signFromFrame = signFromFrame;
        m_signIsMatched = (m_sign == m_signFromFrame);
    }

    menu::Parameters actionParameters;
    actionParameters.setArgument(Qn::ValidRole, m_signIsMatched);
    menu()->trigger(menu::WatermarkCheckedEvent, actionParameters);

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
    QByteArray signFromFrame;
    bool signIsMatched;

    // We gather a copy of all thread-shared data.
    // We are completely thread-safe after that.
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        finished = m_finished;
        sign = m_sign;
        signFromFrame = m_signFromFrame;
        signIsMatched = m_signIsMatched;
    }

    m_signHelper.setSign(sign);
    QPainter painter(this);

    using namespace nx::vms::client::core;
    if (finished)
        m_signHelper.setSignOpacity(opacity, signIsMatched
            ? colorTheme()->color("green_core")
            : colorTheme()->color("red_core"));
    m_signHelper.draw(painter, QSize(width(), height()), m_DrawDetailText);

    QString text = tr("Analyzing: %1%").arg(m_progress);
    if (finished)
    {
        if (signFromFrame.isEmpty())
        {
            painter.setPen(
                colorTheme()->color("red_core"));
            text = tr("Watermark Not Found");
        }
        else if (signIsMatched)
        {
            painter.setPen(
                colorTheme()->color("green_core"));
            text = tr("Watermark Matched");
        }
        else
        {
            painter.setPen(
                colorTheme()->color("red_core"));
            text = tr("Invalid watermark");
        }
    }
    else
    {
        painter.setPen(colorTheme()->color("dark1"));
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
