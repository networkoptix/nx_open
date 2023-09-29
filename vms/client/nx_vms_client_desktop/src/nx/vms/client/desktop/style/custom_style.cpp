// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "custom_style.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGraphicsOpacityEffect>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QTabWidget>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/format.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/common/utils/object_companion.h>
#include <nx/vms/client/desktop/common/utils/page_size_adjuster.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/style.h>
#include <nx/vms/common/html/html.h>
#include <ui/common/palette.h>
#include <utils/math/color_transformations.h>

using namespace nx::vms::common;
using namespace nx::vms::client;
using namespace nx::vms::client::desktop;

namespace {

static const std::vector<QPalette::ColorRole> kTextRoles{
    QPalette::ButtonText,
    QPalette::BrightText,
    QPalette::WindowText,
    QPalette::Text};

} // namespace

// TODO: #vkutin Default disabledOpacity to nx::style::Hints::kDisabledItemOpacity in new versions.
void setWarningStyle(QWidget* widget, qreal disabledOpacity)
{
    auto palette = widget->palette();
    setWarningStyle(&palette, disabledOpacity);
    widget->setPalette(palette);
}

void resetStyle(QWidget* widget)
{
    const auto style = widget->style() ? widget->style() : qApp->style();
    style->unpolish(widget);
    widget->setPalette(QPalette());
    style->polish(widget);
}

void setCustomStyle(QPalette* palette, const QColor& color, qreal disabledOpacity)
{
    for (const auto& role: kTextRoles)
        palette->setColor(role, color);

    if (qFuzzyIsNull(disabledOpacity - 1.0))
        return;

    const auto disabledColor = toTransparent(color, disabledOpacity);
    for (const auto& role: kTextRoles)
        palette->setColor(QPalette::Disabled, role, disabledColor);
}

void setWarningStyle(QPalette* palette, qreal disabledOpacity)
{
    setCustomStyle(palette, core::colorTheme()->color("red_l2"), disabledOpacity);
}

void setWarningStyleOn(QWidget* widget, bool on, qreal disabledOpacity)
{
    if (on)
        setWarningStyle(widget, disabledOpacity);
    else
        resetStyle(widget);
}

QString setWarningStyleHtml(const QString& source)
{
    return html::colored(source, core::colorTheme()->color("red_l2"));
}

void setWarningFrame(QWidget* widget, int frameWidth, int roundingRadius)
{
    static const auto styleTemplateRaw = nx::format(R"qss(
        .QWidget {
            border-style: solid;
            border-color: %1;
            border-width: %2;
            border-radius: %3;
        })qss");

    const auto color = core::colorTheme()->color("red_l2");
    const auto styleTemplate =
        styleTemplateRaw.args(color.name(QColor::HexArgb), frameWidth, roundingRadius);

    widget->setStyleSheet(styleTemplate);
}

void resetButtonStyle(QAbstractButton* button)
{
    button->setProperty(nx::style::Properties::kAccentStyleProperty, false);
    button->setProperty(nx::style::Properties::kWarningStyleProperty, false);
    button->setProperty(nx::style::Properties::kTextButtonBackgroundProperty, false);
    button->update();
}

void setAccentStyle(QAbstractButton* button)
{
    button->setProperty(nx::style::Properties::kAccentStyleProperty, true);
    button->update();
}


void setAccentStyle(QDialog* dialog, QDialogButtonBox::StandardButton standardButton)
{
    if (!NX_ASSERT(dialog, "Dialog can't be null"))
        return;

    const auto buttonBox = dialog->findChild<QDialogButtonBox*>();
    if (!buttonBox)
        return;

    if (const auto button = buttonBox->button(standardButton))
        setAccentStyle(button);
}

void setWarningButtonStyle(QAbstractButton* button)
{
    button->setProperty(nx::style::Properties::kWarningStyleProperty, true);
    button->update();
}

void setTextButtonWithBackgroundStyle(QAbstractButton* button)
{
    button->setProperty(nx::style::Properties::kTextButtonBackgroundProperty, true);
    button->update();
}

void setTabShape(QTabBar* tabBar, nx::style::TabShape tabShape)
{
    tabBar->setProperty(nx::style::Properties::kTabShape, QVariant::fromValue(tabShape));
}

void autoResizePagesToContents(QStackedWidget* pages,
    QSizePolicy visiblePagePolicy,
    bool resizeToVisible,
    std::function<void()> extraHandler)
{
    StackedWidgetPageSizeAdjuster::install(pages, visiblePagePolicy, resizeToVisible, extraHandler);
}

void autoResizePagesToContents(QTabWidget* pages,
    QSizePolicy visiblePagePolicy,
    bool resizeToVisible,
    std::function<void()> extraHandler)
{
    TabWidgetPageSizeAdjuster::install(pages, visiblePagePolicy, resizeToVisible, extraHandler);
}

void fadeWidget(
    QWidget* widget,
    qreal initialOpacity, /* -1 for opacity the widget has at the moment of the call */
    qreal targetOpacity,
    int delayTimeMs,
    qreal fadeSpeed, /* opacity units per second */
    std::function<void()> finishHandler,
    int animationFps)
{
    NX_ASSERT(widget, "No widget is specified");
    if (!widget)
        return;

    auto oldEffect = widget->graphicsEffect();
    auto oldOpacityEffect = qobject_cast<QGraphicsOpacityEffect*>(oldEffect);

    NX_ASSERT(!oldEffect || oldOpacityEffect, "Widget already has a graphics effect");
    if (oldEffect && !oldOpacityEffect)
        return;

    bool zeroSpeed = qFuzzyIsNull(fadeSpeed);
    NX_ASSERT(!zeroSpeed, "Opacity speed should not be this low");
    if (zeroSpeed)
        return;

    /* Choose initial opacity: */
    if (initialOpacity < 0.0)
    {
        initialOpacity = oldOpacityEffect
            ? qBound(0.0, oldOpacityEffect->opacity(), 1.0)
            : 1.0;
    }

    /* Bind target opacity into allowed range: */
    targetOpacity = qBound(0.0, targetOpacity, 1.0);

    /* Create and install an opacity effect: */
    auto opacityEffect = new QGraphicsOpacityEffect(widget);
    opacityEffect->setOpacity(initialOpacity);
    widget->setGraphicsEffect(opacityEffect);

    /* Check if no fade animation is required: */
    qreal opacityDelta = targetOpacity - initialOpacity;
    if (qFuzzyIsNull(opacityDelta))
    {
        if (finishHandler)
            finishHandler();
        return;
    }

    /* Conform speed sign with fade direction: */
    if (fadeSpeed * opacityDelta < 0.0)
        fadeSpeed = -fadeSpeed;

    qreal opacityPerMs = fadeSpeed / 1000.0;

    int animationStepMs = qMax(1000 / animationFps, 1);

    /* Perform delayed start of the animation: */
    QTimer::singleShot(qMax(delayTimeMs, 0), opacityEffect,
        [opacityEffect, initialOpacity, targetOpacity, opacityPerMs, animationStepMs, finishHandler]()
        {
            /* Create and start animation timer: */
            auto animationTimer = new QTimer(opacityEffect);
            animationTimer->setSingleShot(false);
            animationTimer->start(animationStepMs);

            QElapsedTimer stopwatch;
            stopwatch.start();

            QObject::connect(animationTimer, &QTimer::timeout, opacityEffect,
                [animationTimer, opacityEffect, initialOpacity, targetOpacity, opacityPerMs, stopwatch, finishHandler]()
                {
                    /* Compute new opacity: */
                    auto opacity = initialOpacity + opacityPerMs * stopwatch.elapsed();
                    opacityEffect->setOpacity(qBound(0.0, opacity, 1.0));

                    /* Check if animation end is reached: */
                    bool finished = opacityPerMs < 0.0
                        ? opacity <= targetOpacity
                        : opacity >= targetOpacity;

                    if (finished)
                    {
                        animationTimer->stop();
                        animationTimer->deleteLater();

                        if (finishHandler)
                            finishHandler();
                    }
                });
        });
}

void setMonospaceFont(QWidget* widget)
{
    widget->ensurePolished();
    widget->setFont(monospaceFont(widget->font()));
}

QFont monospaceFont(const QFont& font)
{
    QFont mono(font);
    mono.setFamily("Roboto Mono");

    return mono;
}
