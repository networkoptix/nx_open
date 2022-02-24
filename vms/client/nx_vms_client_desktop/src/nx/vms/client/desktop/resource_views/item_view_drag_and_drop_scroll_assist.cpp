// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "item_view_drag_and_drop_scroll_assist.h"

#include <cmath>
#include <algorithm>

#include <QtCore/QEvent>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QEasingCurve>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QScrollBar>
#include <QtQml/QtQml>

#include <nx/utils/math/fuzzy.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/style/helper.h>

namespace {

static constexpr int kScrollAssistAreaHeight = nx::style::Metrics::kViewRowHeight * 2;
static const auto kScrollAssistAreaSensitivityEasing = QEasingCurve(QEasingCurve::InCubic);

static constexpr double kBaseScrollVelocity = 3.0; //< Pixels per millisecond at edge proximity 1.
static constexpr double kRapidScrollVelocityMultiplier = 10.0;

static constexpr auto kScrollBarValuePropertyName = "value";

/**
 * Defines non-linear relation between cursor proximity to the edge of scroll area and resulting
 * scroll speed. Values in the beginning of the range affect velocity much less (fine positioning)
 * then ones from the end of the range (coarse positioning).
 * @param edgeProximity Value in the [0, 1] range which describes mouse cursor nearness to the
 *     scroll triggering edge of the scroll area.
 * @returns Scrolling velocity in pixels per millisecond.
 */
double velocityByEdgeProximity(double edgeProximity)
{
    const auto edgeProximityMultiplier =
        std::max(0.0001, kScrollAssistAreaSensitivityEasing.valueForProgress(edgeProximity));

    double result = kBaseScrollVelocity * edgeProximityMultiplier;

    if (QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier))
        result *= kRapidScrollVelocityMultiplier;

    return result;
}

/**
 * @param cursorPos The cursor position in the same coordinate system as scrollAreaGeometry defined.
 * @returns Value from [0.0, 1.0] range which means cursor nearness to the scroll area top edge.
 */
double topScrollAssistEdgeProximity(const QRectF& scrollAreaGeometry, const QPointF& cursorPos)
{
    const auto topScrollAssistArea = scrollAreaGeometry.adjusted(
        0, 0, 0, -scrollAreaGeometry.height() + kScrollAssistAreaHeight);

    if (!topScrollAssistArea.contains(cursorPos))
        return 0.0;

    return 1.0 - static_cast<double>(cursorPos.y() - topScrollAssistArea.top())
        / topScrollAssistArea.height();
}

/**
 * @param cursorPos The cursor position in the same coordinate system as scrollAreaGeometry defined.
 * @returns Value from [0.0, 1.0] range which means cursor nearness to the scroll area bottom edge.
 */
double bottomScrollAssistEdgeProximity(const QRectF& scrollAreaGeometry, const QPointF& cursorPos)
{
    const auto bottomScrollAssistArea = scrollAreaGeometry.adjusted(
        0, scrollAreaGeometry.height() - kScrollAssistAreaHeight, 0, 0);

    if (!bottomScrollAssistArea.contains(cursorPos))
        return 0.0;

    return 1.0 - static_cast<double>(bottomScrollAssistArea.bottom() - cursorPos.y())
        / bottomScrollAssistArea.height();
}

} // namespace

namespace nx::vms::client::desktop {

void ItemViewDragAndDropScrollAssist::setupScrollAssist(
    QAbstractScrollArea* scrollArea,
    QAbstractItemView* itemView)
{
    if (!scrollArea || !itemView)
    {
        NX_ASSERT(false, "Invalid null parameter");
        return;
    }

    if (scrollArea->findChild<ItemViewDragAndDropScrollAssist*>() != nullptr)
    {
        NX_ASSERT(false, "Item view drag and drop scroll assist has been already set up.");
        return;
    }

    if (scrollArea != itemView
        && !scrollArea->findChildren<QAbstractItemView*>().contains(itemView))
    {
        NX_ASSERT(false, "Item view isn't related to the provieded scroll area.");
        return;
    }

    new ItemViewDragAndDropScrollAssist(scrollArea, itemView);
}

void ItemViewDragAndDropScrollAssist::removeScrollAssist(QAbstractScrollArea* scrollArea)
{
    if (!scrollArea)
    {
        NX_ASSERT(false, "Invalid null parameter");
        return;
    }

    if (auto scrollAssist = scrollArea->findChild<ItemViewDragAndDropScrollAssist*>())
        scrollAssist->deleteLater();
}

ItemViewDragAndDropScrollAssist::ItemViewDragAndDropScrollAssist(
    QAbstractScrollArea* scrollArea,
    QAbstractItemView* itemView)
    :
    base_type(scrollArea),
    m_scrollArea(scrollArea)
{
    itemView->setAutoScroll(false);
    itemView->viewport()->installEventFilter(this);
}

ItemViewDragAndDropScrollAssist::~ItemViewDragAndDropScrollAssist()
{
    stopScrolling();
}

bool ItemViewDragAndDropScrollAssist::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type())
    {
        case QEvent::DragEnter:
        case QEvent::DragMove:
            updateScrollingStateAndParameters();
            return isScrolling() && event->type() == QEvent::DragMove;

        case QEvent::DragLeave:
        case QEvent::Drop:
        case QEvent::Hide:
        case QEvent::DeferredDelete:
            stopScrolling();
            return false;

        default:
            return false;
    }
}

bool ItemViewDragAndDropScrollAssist::isScrolling() const
{
    return !m_animation.isNull();
}

void ItemViewDragAndDropScrollAssist::updateScrollingStateAndParameters()
{
    const auto cursorPos = m_scrollArea->mapFromGlobal(QCursor::pos());
    const auto geometry = m_scrollArea->geometry();

    const auto topEdgeProximity = topScrollAssistEdgeProximity(geometry, cursorPos);
    const auto bottomEdgeProximity = bottomScrollAssistEdgeProximity(geometry, cursorPos);

    if (qFuzzyIsNull(std::max(topEdgeProximity, bottomEdgeProximity)))
    {
        stopScrolling();
        return;
    }

    const bool isForwardScroll = bottomEdgeProximity > topEdgeProximity;
    const auto edgeProximity = std::max(topEdgeProximity, bottomEdgeProximity);

    if (!isScrolling())
        initializeScrolling(isForwardScroll, edgeProximity);
    else
        updateScrollingVelocity(edgeProximity);
}

void ItemViewDragAndDropScrollAssist::initializeScrolling(
    bool isForwardScroll,
    double edgeProximity)
{
    const auto verticalScrollBar = m_scrollArea->verticalScrollBar();

    const auto startValue = verticalScrollBar->value();
    const auto endValue = isForwardScroll
        ? verticalScrollBar->maximum()
        : verticalScrollBar->minimum();
    const auto pixelDistance = std::abs(endValue - startValue);

    const int durationMs = pixelDistance / velocityByEdgeProximity(edgeProximity);

    if (durationMs <= 0 || pixelDistance <= 0)
        return;

    m_animation = new QPropertyAnimation(verticalScrollBar, kScrollBarValuePropertyName);
    m_animation->setStartValue(startValue);
    m_animation->setEndValue(endValue);
    m_animation->setDuration(durationMs);
    m_animation->setEasingCurve(QEasingCurve(QEasingCurve::Linear));

    m_animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void ItemViewDragAndDropScrollAssist::stopScrolling() const
{
    if (m_animation)
        m_animation->stop();
}

void ItemViewDragAndDropScrollAssist::updateScrollingVelocity(double edgeProximity) const
{
    if (!isScrolling())
        return;

    const auto pixelDistance =
        std::abs(m_animation->endValue().toInt() - m_animation->startValue().toInt());

    const int durationMs = pixelDistance / velocityByEdgeProximity(edgeProximity);

    if (durationMs <= 0 || pixelDistance <= 0)
        return;

    const auto animationProgress =
        static_cast<double>(m_animation->currentTime()) / m_animation->duration();

    m_animation->setDuration(durationMs);
    m_animation->setCurrentTime(std::lround(animationProgress * durationMs));
}

qreal ProximityScrollHelper::velocity(const QRectF& geometry, const QPointF& position) const
{
    const auto topEdgeProximity = topScrollAssistEdgeProximity(geometry, position);
    const auto bottomEdgeProximity = bottomScrollAssistEdgeProximity(geometry, position);

    if (qFuzzyIsNull(std::max(topEdgeProximity, bottomEdgeProximity)))
        return 0.0;

    const auto speed = velocityByEdgeProximity(std::max(topEdgeProximity, bottomEdgeProximity));
    return bottomEdgeProximity > topEdgeProximity ? speed : -speed;
}

void ProximityScrollHelper::registerQmlType()
{
    static ProximityScrollHelper instance;
    qmlRegisterSingletonInstance<ProximityScrollHelper>(
        "nx.vms.client.desktop", 1, 0, "ProximityScrollHelper", &instance);
}

} // namespace nx::vms::client::desktop
