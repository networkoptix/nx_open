// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "item_view_drag_and_drop_scroll_assist.h"

#include <cmath>
#include <algorithm>

#include <QtCore/QEvent>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QEasingCurve>
#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QScrollBar>
#include <QtQml/QtQml>

#include <nx/utils/math/fuzzy.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/common/utils/proximity_scroll_helper.h>
#include <nx/vms/client/desktop/style/helper.h>

namespace {

static constexpr auto kScrollBarValuePropertyName = "value";

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
        NX_ASSERT(false, "Item view isn't related to the provided scroll area.");
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
    m_scrollArea(scrollArea),
    m_proximityScrollHelper(new core::ProximityScrollHelper(this))
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

    const auto velocity = m_proximityScrollHelper->velocity(geometry, cursorPos);

    if (qFuzzyIsNull(velocity))
    {
        stopScrolling();
        return;
    }

    if (!isScrolling())
        initializeScrolling(velocity);
    else
        updateScrollingVelocity(velocity);
}

void ItemViewDragAndDropScrollAssist::initializeScrolling(
    double velocity)
{
    const bool isForwardScroll = velocity > 0;
    const auto verticalScrollBar = m_scrollArea->verticalScrollBar();

    const auto startValue = verticalScrollBar->value();
    const auto endValue = isForwardScroll
        ? verticalScrollBar->maximum()
        : verticalScrollBar->minimum();
    const auto pixelDistance = std::abs(endValue - startValue);

    const int durationMs = pixelDistance / std::fabs(velocity);

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

void ItemViewDragAndDropScrollAssist::updateScrollingVelocity(double velocity) const
{
    if (!isScrolling())
        return;

    const auto pixelDistance =
        std::abs(m_animation->endValue().toInt() - m_animation->startValue().toInt());

    const int durationMs = pixelDistance / std::fabs(velocity);

    if (durationMs <= 0 || pixelDistance <= 0)
        return;

    const auto animationProgress =
        static_cast<double>(m_animation->currentTime()) / m_animation->duration();

    m_animation->setDuration(durationMs);
    m_animation->setCurrentTime(std::lround(animationProgress * durationMs));
}

} // namespace nx::vms::client::desktop
