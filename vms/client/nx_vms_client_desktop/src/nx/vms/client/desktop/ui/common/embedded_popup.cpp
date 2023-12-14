// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "embedded_popup.h"

#include <memory>
#include <optional>

#if defined(Q_OS_WIN)
    #include <Windows.h>
#endif

#include <QtCore/QMarginsF>
#include <QtCore/QPointer>
#include <QtWidgets/QWidget>
#include <QtQuickWidgets/QQuickWidget>
#include <QtQml/QtQml>

#include <client_core/client_core_module.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/widgets/main_window.h>
#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>

namespace nx::vms::client::desktop {

struct EmbeddedPopup::Private
{
    EmbeddedPopup* const q;

    QPointer<QWidget> viewport;
    QPointer<QQuickItem> contentItem;
    QPointer<QQuickItem> parentItem;
    QPointer<QQuickWindow> parentWindow;

    QRectF desiredGeometry;
    QRectF geometry;

    qreal margins = -1;
    std::optional<qreal> leftMargin;
    std::optional<qreal> topMargin;
    std::optional<qreal> rightMargin;
    std::optional<qreal> bottomMargin;

    bool visibleToParent = true;
    bool visible = false;

    std::unique_ptr<QQuickWidget> quickWidget{ini().debugDisableQmlTooltips
        ? (QQuickWidget*) nullptr
        : new QQuickWidget(qnClientCoreModule->mainQmlEngine(), nullptr)};

    std::unique_ptr<QObject> viewportEventHandler;

    QMarginsF effectiveMargins() const;
    qreal effectiveLeftMargin() const { return leftMargin.value_or(margins); }
    qreal effectiveTopMargin() const { return topMargin.value_or(margins); }
    qreal effectiveRightMargin() const { return rightMargin.value_or(margins); }
    qreal effectiveBottomMargin() const { return bottomMargin.value_or(margins); }

    void updateGeometry();
    void updateVisibility();
    void updateParentWindow();
    void updateTransientParent();

    QPointF mapToViewport(const QPointF& pos) const;
    QPointF mapFromViewport(const QPointF& pos) const;
    QPointF mapToGlobal(const QPointF& pos) const;
};

// ------------------------------------------------------------------------------------------------
// EmbeddedPopup

EmbeddedPopup::EmbeddedPopup(QObject* parent):
    QObject(parent),
    d(new Private{this})
{
    if (!d->quickWidget)
        return;

    // If a parent widget is not set here - the Metal backend refuses to draw transparent window.
    d->viewport = appContext()->mainWindowContext()->mainWindowWidget();
    d->quickWidget->setParent(d->viewport);

    d->quickWidget->setClearColor(Qt::transparent);
    d->quickWidget->setAttribute(Qt::WA_TranslucentBackground);
    d->quickWidget->setAttribute(Qt::WA_ShowWithoutActivating);
    d->quickWidget->setAttribute(Qt::WA_AlwaysStackOnTop);
    d->quickWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    d->quickWidget->setVisible(d->visible);
}

EmbeddedPopup::~EmbeddedPopup()
{
    setParentItem({});
}

QWidget* EmbeddedPopup::viewport() const
{
    return d->viewport;
}

void EmbeddedPopup::setViewport(QWidget* value)
{
    if (d->viewport == value)
        return;

    d->viewport = value;
    d->updateTransientParent();
    d->viewportEventHandler.reset(installEventHandler(
        d->viewport, {QEvent::Resize, QEvent::Move}, this, [this]() { d->updateGeometry(); }));

    emit viewportChanged();

    d->updateVisibility();
    d->updateGeometry();
}

QQuickItem* EmbeddedPopup::contentItem() const
{
    return d->contentItem;
}

void EmbeddedPopup::setContentItem(QQuickItem* value)
{
    if (d->contentItem == value)
        return;

    if (!NX_ASSERT(!d->contentItem))
        d->contentItem->setParentItem(nullptr);

    d->contentItem = value;

    if (d->quickWidget)
        d->contentItem->setParentItem(d->quickWidget->quickWindow()->contentItem());

    emit contentItemChanged();
}

QQuickItem* EmbeddedPopup::parentItem() const
{
    return d->parentItem;
}

void EmbeddedPopup::setParentItem(QQuickItem* value)
{
    if (d->parentItem == value)
        return;

    if (d->parentItem)
        d->parentItem->disconnect(this);

    d->parentItem = value;
    if (d->parentItem)
    {
        connect(
            d->parentItem, &QQuickItem::visibleChanged, this, [this] { d->updateVisibility(); });
        connect(
            d->parentItem, &QQuickItem::windowChanged, this, [this] { d->updateParentWindow(); });
    }

    d->updateParentWindow();
    d->updateVisibility();
    d->updateGeometry();

    emit parentItemChanged();
}

qreal EmbeddedPopup::x() const
{
    return d->geometry.left();
}

void EmbeddedPopup::setX(qreal value)
{
    if (qFuzzyEquals(d->desiredGeometry.left(), value))
        return;

    d->desiredGeometry.moveLeft(value);
    d->updateGeometry();
}

qreal EmbeddedPopup::y() const
{
    return d->geometry.top();
}

void EmbeddedPopup::setY(qreal value)
{
    if (qFuzzyEquals(d->desiredGeometry.top(), value))
        return;

    d->desiredGeometry.moveTop(value);
    d->updateGeometry();
}

qreal EmbeddedPopup::width() const
{
    return d->geometry.width();
}

void EmbeddedPopup::setWidth(qreal value)
{
    if (qFuzzyEquals(d->desiredGeometry.width(), value))
        return;

    d->desiredGeometry.setWidth(value);
    d->updateGeometry();
}

qreal EmbeddedPopup::height() const
{
    return d->geometry.height();
}

void EmbeddedPopup::setHeight(qreal value)
{
    if (qFuzzyEquals(d->desiredGeometry.height(), value))
        return;

    d->desiredGeometry.setHeight(value);
    d->updateGeometry();
}

bool EmbeddedPopup::isVisible() const
{
    return d->visible;
}

void EmbeddedPopup::setVisibleToParent(bool value)
{
    if (d->visibleToParent == value)
        return;

    d->visibleToParent = value;
    d->updateVisibility();
}

qreal EmbeddedPopup::margins() const
{
    return d->margins;
}

qreal EmbeddedPopup::leftMargin() const
{
    return d->effectiveLeftMargin();
}

qreal EmbeddedPopup::rightMargin() const
{
    return d->effectiveRightMargin();
}

qreal EmbeddedPopup::topMargin() const
{
    return d->effectiveTopMargin();
}

qreal EmbeddedPopup::bottomMargin() const
{
    return d->effectiveBottomMargin();
}

void EmbeddedPopup::setMargins(qreal value)
{
    if (qFuzzyEquals(d->margins, value))
        return;

    const auto oldMargins = d->effectiveMargins();

    d->margins = value;
    emit marginsChanged();

    if (!qFuzzyEquals(oldMargins.left(), d->effectiveLeftMargin()))
        emit leftMarginChanged();

    if (!qFuzzyEquals(oldMargins.top(), d->effectiveTopMargin()))
        emit topMarginChanged();

    if (!qFuzzyEquals(oldMargins.right(), d->effectiveRightMargin()))
        emit rightMarginChanged();

    if (!qFuzzyEquals(oldMargins.bottom(), d->effectiveBottomMargin()))
        emit bottomMarginChanged();

    d->updateGeometry();
}

void EmbeddedPopup::setLeftMargin(qreal value)
{
    const auto oldMargin = d->effectiveLeftMargin();
    d->leftMargin = value;
    if (qFuzzyEquals(oldMargin, value))
        return;

    emit leftMarginChanged();
    d->updateGeometry();
}

void EmbeddedPopup::setRightMargin(qreal value)
{
    const auto oldMargin = d->effectiveRightMargin();
    d->rightMargin = value;
    if (qFuzzyEquals(oldMargin, value))
        return;

    emit rightMarginChanged();
    d->updateGeometry();
}

void EmbeddedPopup::setTopMargin(qreal value)
{
    const auto oldMargin = d->effectiveTopMargin();
    d->topMargin = value;
    if (qFuzzyEquals(oldMargin, value))
        return;

    emit topMarginChanged();
    d->updateGeometry();
}

void EmbeddedPopup::setBottomMargin(qreal value)
{
    const auto oldMargin = d->effectiveBottomMargin();
    d->bottomMargin = value;
    if (qFuzzyEquals(oldMargin, value))
        return;

    emit bottomMarginChanged();
    d->updateGeometry();
}

void EmbeddedPopup::resetLeftMargin()
{
    const auto oldMargin = d->effectiveLeftMargin();
    d->leftMargin = std::nullopt;
    if (qFuzzyEquals(oldMargin, d->effectiveLeftMargin()))
        return;

    emit leftMarginChanged();
    d->updateGeometry();
}

void EmbeddedPopup::resetRightMargin()
{
    const auto oldMargin = d->effectiveRightMargin();
    d->rightMargin = std::nullopt;
    if (qFuzzyEquals(oldMargin, d->effectiveRightMargin()))
        return;

    emit rightMarginChanged();
    d->updateGeometry();
}

void EmbeddedPopup::resetTopMargin()
{
    const auto oldMargin = d->effectiveTopMargin();
    d->topMargin = std::nullopt;
    if (qFuzzyEquals(oldMargin, d->effectiveTopMargin()))
        return;

    emit topMarginChanged();
    d->updateGeometry();
}

void EmbeddedPopup::resetBottomMargin()
{
    const auto oldMargin = d->effectiveBottomMargin();
    d->bottomMargin = std::nullopt;
    if (qFuzzyEquals(oldMargin, d->effectiveBottomMargin()))
        return;

    emit bottomMarginChanged();
    d->updateGeometry();
}

void EmbeddedPopup::raise()
{
    if (d->quickWidget)
        d->quickWidget->raise();
}

void EmbeddedPopup::registerQmlType()
{
    qmlRegisterType<EmbeddedPopup>("nx.vms.client.desktop", 1, 0, "EmbeddedPopup");
}

// ------------------------------------------------------------------------------------------------
// EmbeddedPopup::Private

QMarginsF EmbeddedPopup::Private::effectiveMargins() const
{
    return QMarginsF(
        effectiveLeftMargin(),
        effectiveTopMargin(),
        effectiveRightMargin(),
        effectiveBottomMargin());
}

void EmbeddedPopup::Private::updateGeometry()
{
    const auto oldGeometry = geometry;
    QRectF windowGeometry;

    if (parentItem && Private::viewport)
    {
        const auto viewportTopLeft = mapToViewport(desiredGeometry.topLeft());
        const auto viewportBottomRight = mapToViewport(desiredGeometry.bottomRight());

        qreal left = viewportTopLeft.x();
        qreal top = viewportTopLeft.y();
        qreal right = viewportBottomRight.x();
        qreal bottom = viewportBottomRight.y();

        const QMarginsF m = effectiveMargins();
        const QSizeF viewportSize = Private::viewport->size();

        const QRectF targetRect(
            QPointF(
                m.left() < 0 ? -1.0e10 : m.left(),
                m.top() < 0 ? -1.0e10 : m.top()),
            QPointF(
                m.right() < 0 ? 1.0e10 : (viewportSize.width() - m.right()),
                m.bottom() < 0 ? 1.0e10 : (viewportSize.height() - m.bottom())));

        if (right - left > targetRect.width())
            right = left + targetRect.width();
        if (bottom - top > targetRect.height())
            bottom = top + targetRect.height();

        if (targetRect.left() > left)
        {
            right = targetRect.left() + (right - left);
            left = targetRect.left();
        }
        else if (targetRect.right() < right)
        {
            left = targetRect.right() - (right - left);
            right = targetRect.right();
        }

        if (targetRect.top() > top)
        {
            bottom = targetRect.top() + (bottom - top);
            top = targetRect.top();
        }
        else if (targetRect.bottom() < bottom)
        {
            top = targetRect.bottom() - (bottom - top);
            bottom = targetRect.bottom();
        }

        const auto viewportGeometry = QRectF(QPointF(left, top), QPointF(right, bottom));
        if (!viewportGeometry.isValid())
            return;

        geometry = QRectF(mapFromViewport(viewportGeometry.topLeft()), viewportGeometry.size());
        windowGeometry = QRectF(mapToGlobal(viewportGeometry.topLeft()), viewportGeometry.size());
    }
    else
    {
        windowGeometry = QRectF(0, 0, 0, 0);
        geometry = QRectF(0, 0, 0, 0);
    }

    if (!qFuzzyEquals(oldGeometry.left(), geometry.left()))
        emit q->xChanged();
    if (!qFuzzyEquals(oldGeometry.top(), geometry.top()))
        emit q->yChanged();

    if (!qFuzzyEquals(oldGeometry.width(), geometry.width()))
        emit q->widthChanged();
    if (!qFuzzyEquals(oldGeometry.height(), geometry.height()))
        emit q->heightChanged();

    executeLater(
        [this, windowGeometry]
        {
            if (!quickWidget || !quickWidget->parentWidget())
                return;
            const auto offset = quickWidget->parentWidget()->mapToGlobal(QPoint{0, 0});
            quickWidget->setGeometry(windowGeometry.toAlignedRect().translated(-offset));
        },
        q);
}

QPointF EmbeddedPopup::Private::mapToViewport(const QPointF& pos) const
{
    if (!parentItem || !Private::viewport)
        return pos;

    const auto globalPos = parentItem->mapToGlobal(pos);
    return Private::viewport->mapFromGlobal(globalPos.toPoint());
}

QPointF EmbeddedPopup::Private::mapFromViewport(const QPointF& pos) const
{
    if (!parentItem || !Private::viewport)
        return pos;

    const auto globalPos = Private::viewport->mapToGlobal(pos.toPoint());
    return parentItem->mapFromGlobal(globalPos);
}

QPointF EmbeddedPopup::Private::mapToGlobal(const QPointF& pos) const
{
    if (!Private::viewport)
        return pos;

    return Private::viewport->mapToGlobal(pos.toPoint());
}

void EmbeddedPopup::Private::updateVisibility()
{
    const bool newVisible = visibleToParent && parentItem && parentItem->isVisible();
    if (newVisible == visible)
        return;

    visible = newVisible;

    // Showing must be delayed because it causes contentItem polishing and size recalculation
    // and can lead to unexpected binding loops.
    if (quickWidget)
    {
        if (visible)
            executeLater([this]() { quickWidget->setVisible(visible); }, quickWidget.get());
        else
            quickWidget->setVisible(visible);
    }

    emit q->visibleChanged();
}

void EmbeddedPopup::Private::updateTransientParent()
{
    if (!viewport || !quickWidget)
        return;

    auto newWindow = viewport->window();
    if (!newWindow)
        return;

    if (quickWidget->parentWidget() == newWindow)
        return;

    quickWidget->setParent(newWindow);
}

void EmbeddedPopup::Private::updateParentWindow()
{
    const auto window = parentItem ? parentItem->window() : nullptr;
    if (window == parentWindow)
        return;

    // There may be no viewport parent window handle at the time of component construction, so try
    // to update transient parent when parent item is changed.
    updateTransientParent();

    if (parentWindow)
        parentWindow->disconnect(q);

    parentWindow = window;
    updateGeometry();
}

} // namespace nx::vms::client::desktop
