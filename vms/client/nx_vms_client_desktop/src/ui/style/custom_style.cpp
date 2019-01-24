#include "custom_style.h"

#include <QtCore/QTimer>
#include <QtCore/QElapsedTimer>

#include <QtWidgets/QPushButton>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QGraphicsOpacityEffect>
#include <QtWidgets/QApplication>

#include <ui/style/generic_palette.h>
#include <ui/style/nx_style.h>
#include <ui/style/helper.h>
#include <ui/common/palette.h>
#include <ui/style/globals.h>
#include <utils/math/color_transformations.h>

#include <nx/vms/client/desktop/common/utils/object_companion.h>
#include <nx/vms/client/desktop/common/utils/page_size_adjuster.h>

using namespace nx::vms::client::desktop;

namespace {

static const std::vector<QPalette::ColorRole> kTextRoles{
    QPalette::ButtonText,
    QPalette::BrightText,
    QPalette::WindowText,
    QPalette::Text};

} // namespace

// TODO: #vkutin Default disabledOpacity to style::Hints::kDisabledItemOpacity in new versions.
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
    setCustomStyle(palette, qnGlobals->errorTextColor(), disabledOpacity);
}

void setWarningStyleOn(QWidget* widget, bool on, qreal disabledOpacity)
{
    if (on)
        setWarningStyle(widget, disabledOpacity);
    else
        resetStyle(widget);
}

QString setWarningStyleHtml( const QString &source )
{
    return lit("<font color=\"%1\">%2</font>").arg(qnGlobals->errorTextColor().name(), source);
}

void resetButtonStyle(QAbstractButton* button)
{
    button->setProperty(style::Properties::kAccentStyleProperty, false);
    button->setProperty(style::Properties::kWarningStyleProperty, false);
    button->update();
}

void setAccentStyle(QAbstractButton* button)
{
    button->setProperty(style::Properties::kAccentStyleProperty, true);
    button->update();
}

void setWarningButtonStyle(QAbstractButton* button)
{
    button->setProperty(style::Properties::kWarningStyleProperty, true);
    button->update();
}

void setTabShape(QTabBar* tabBar, style::TabShape tabShape)
{
    tabBar->setProperty(style::Properties::kTabShape, QVariant::fromValue(tabShape));
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
        [widget, opacityEffect, initialOpacity, targetOpacity, opacityPerMs, animationStepMs, finishHandler]()
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
    mono.setFamily(lit("Roboto Mono"));
    return mono;
}
