// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bubble_tooltip.h"

#include <memory>

#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtQuickWidgets/QQuickWidget>

#include <nx/vms/client/core/utils/qml_helpers.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/utils/mouse_spy.h>
#include <nx/vms/client/desktop/utils/qml_property.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/event_processors.h>

namespace nx::vms::client::desktop {

using nx::vms::client::core::invokeQmlMethod;

using namespace std::chrono;

struct BubbleToolTip::Private
{
    BubbleToolTip* const q;

    std::unique_ptr<QQuickWidget> widget{ini().debugDisableQmlTooltips
        ? (QQuickWidget*) nullptr
        : new QQuickWidget(appContext()->qmlEngine(), /*parent*/ nullptr)};

    QRectF targetRect;
    QRectF enclosingRect;
    Qt::Orientation orientation = Qt::Horizontal;
    bool suppressedOnMouseClick = true;
    int tooltipOffset = 0;

    const QmlProperty<bool> visible{widget.get(), "visible"};
    const QmlProperty<int> pointerEdge{widget.get(), "pointerEdge"};
    const QmlProperty<qreal> normalizedPointerPos{widget.get(), "normalizedPointerPos"};
    const QmlProperty<QString> text{widget.get(), "text"};

    State state = State::hidden;

    void updatePosition();
    void setState(State value);
};

BubbleToolTip::BubbleToolTip(WindowContext* context, QObject* parent):
    BubbleToolTip(context, QUrl("qrc:/qml/Nx/Controls/Bubble.qml"), parent)
{
}

BubbleToolTip::BubbleToolTip(
    WindowContext* context,
    const QUrl& componentUrl,
    QObject* parent)
    :
    QObject(parent),
    WindowContextAware(context),
    d(new Private{.q = this})
{
    connect(MouseSpy::instance(), &MouseSpy::mouseMove, this,
        [this]()
        {
            if (d->state == State::suppressed)
                show();
        });

    connect(MouseSpy::instance(), &MouseSpy::mousePress, this,
        [this]()
        {
            if (d->suppressedOnMouseClick)
                suppress();
        });

    if (!d->widget)
        return;

    // If a parent widget is not set here - the Metal backend refuses to draw transparent window.
    d->widget->setParent(context->mainWindowWidget());

    d->widget->setVisible(false);
    d->widget->setResizeMode(QQuickWidget::SizeViewToRootObject);
    d->widget->setClearColor(Qt::transparent);
    d->widget->setAttribute(Qt::WA_TransparentForMouseEvents);
    d->widget->setAttribute(Qt::WA_TranslucentBackground);
    d->widget->setAttribute(Qt::WA_ShowWithoutActivating);
    d->widget->setAttribute(Qt::WA_AlwaysStackOnTop);
    d->widget->setMouseTracking(true);
    d->widget->setSource(componentUrl);
    d->widget->setObjectName("BubbleTooltip");

    QmlProperty<bool>(d->widget.get(), "containDecorationsInside") = true;

    d->visible.connectNotifySignal([this]() { d->widget->setVisible(d->visible()); });

    installEventHandler(d->widget.get(), QEvent::Resize, this,
        [this]() { d->updatePosition(); });
}

BubbleToolTip::~BubbleToolTip()
{
    // Required here for forward-declared scoped pointer destruction.
}

void BubbleToolTip::show()
{
    if (d->widget)
    {
        d->widget->setParent(mainWindowWidget());
        invokeQmlMethod<void>(d->widget->rootObject(), "show");
        d->widget->raise();
    }

    d->setState(State::shown);
}

void BubbleToolTip::hide(bool immediately)
{
    if (d->state == State::hidden)
        return;

    if (d->widget)
        invokeQmlMethod<void>(d->widget->rootObject(), "hide", immediately);

    d->setState(State::hidden);
}

void BubbleToolTip::suppress(bool immediately)
{
    if (d->state == State::suppressed)
        return;

    if (d->widget)
        invokeQmlMethod<void>(d->widget->rootObject(), "hide", immediately);
    if (d->state == State::shown)
        d->setState(State::suppressed);
}

QString BubbleToolTip::text() const
{
    return d->text();
}

void BubbleToolTip::setText(const QString& value)
{
    d->text = value;
}

Qt::Orientation BubbleToolTip::orientation() const
{
    return d->orientation;
}

void BubbleToolTip::setOrientation(Qt::Orientation value)
{
    if (d->orientation == value)
        return;

    d->orientation = value;
    d->updatePosition();
}

bool BubbleToolTip::suppressedOnMouseClick() const
{
    return d->suppressedOnMouseClick;
}

void BubbleToolTip::setSuppressedOnMouseClick(bool value)
{
    d->suppressedOnMouseClick = value;
}

int BubbleToolTip::tooltipOffset() const
{
    return d->tooltipOffset;
}

void BubbleToolTip::setTooltipOffset(int offset)
{
    if (d->tooltipOffset == offset)
        return;

    d->tooltipOffset = offset;
    d->updatePosition();
}

void BubbleToolTip::setTarget(const QRect& targetRect)
{
    if (d->targetRect == targetRect)
        return;

    d->targetRect = targetRect;
    d->updatePosition();
}

void BubbleToolTip::setTarget(const QPoint& targetPoint)
{
    const QRectF targetRect(targetPoint, QSizeF(0, 0));
    if (d->targetRect == targetRect)
        return;

    d->targetRect = targetRect;
    d->updatePosition();
}

void BubbleToolTip::setEnclosingRect(const QRect& value)
{
    if (d->enclosingRect == value)
        return;

    d->enclosingRect = value;
    d->updatePosition();
}

BubbleToolTip::State BubbleToolTip::state() const
{
    return d->state;
}

QQuickWidget* BubbleToolTip::widget() const
{
    return d->widget.get();
}

void BubbleToolTip::Private::updatePosition()
{
    if (!widget)
        return;

    if (targetRect.width() < 0 || targetRect.height() < 0)
    {
        widget->move(-widget->width(), -widget->height());
        return;
    }

    const auto minIntersection = std::min(64.0,
        (orientation == Qt::Vertical ? targetRect.width() : targetRect.height()) / 2.0);

    const auto params = invokeQmlMethod<QJSValue>(widget->rootObject(), "calculateParameters",
        (int) orientation,
        QRectF(targetRect).adjusted(0.5, 0.5, 0.5, 0.5),
        enclosingRect,
        minIntersection);

    if (params.isUndefined() || !NX_ASSERT(params.isObject()))
    {
        widget->move(-widget->width(), -widget->height());
        return;
    }

    pointerEdge = params.property("pointerEdge").toInt();
    normalizedPointerPos = params.property("normalizedPointerPos").toNumber();

    const auto x = params.property("x").toNumber();
    const auto y = params.property("y").toNumber();

    auto pos = widget->parentWidget()
        ? widget->parentWidget()->mapFromGlobal(QPoint(x, y))
        : QPoint(x, y);

    switch (pointerEdge)
    {
        case Qt::TopEdge:
            pos.ry() += tooltipOffset;
            break;

        case Qt::LeftEdge:
            pos.rx() += tooltipOffset;
            break;

        case Qt::RightEdge:
            pos.rx() -= tooltipOffset;
            break;

        case Qt::BottomEdge:
            pos.ry() -= tooltipOffset;
            break;
    }

    widget->move(pos);
}

void BubbleToolTip::Private::setState(State value)
{
    if (state == value)
        return;

    state = value;
    emit q->stateChanged(state);
}

} // namespace nx::vms::client::desktop
