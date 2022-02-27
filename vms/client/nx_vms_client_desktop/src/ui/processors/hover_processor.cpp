// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "hover_processor.h"

#include <QtWidgets/QWidget>
#include <QtCore/QTimerEvent>

#include <utils/common/delayed.h>

#include <nx/utils/log/log.h>

HoverFocusProcessor::HoverFocusProcessor(QGraphicsItem* parent):
    QGraphicsObject(parent),
    m_hoverEnterDelay(0),
    m_hoverLeaveDelay(0),
    m_focusEnterDelay(0),
    m_focusLeaveDelay(0),
    m_hoverEnterTimerId(0),
    m_hoverLeaveTimerId(0),
    m_focusEnterTimerId(0),
    m_focusLeaveTimerId(0),
    m_hovered(false),
    m_focused(false)
{
    setFlags(ItemHasNoContents);
    setAcceptedMouseButtons(Qt::NoButton);
    setEnabled(false);
    setVisible(false);
}

HoverFocusProcessor::~HoverFocusProcessor()
{
    return;
}

void HoverFocusProcessor::setHoverEnterDelay(int hoverEnterDelayMSec)
{
    if (hoverEnterDelayMSec < 0)
    {
        NX_ASSERT(false, "Invalid negative hover enter delay '%1'.", hoverEnterDelayMSec);
        return;
    }

    m_hoverEnterDelay = hoverEnterDelayMSec;
}

void HoverFocusProcessor::setHoverLeaveDelay(int hoverLeaveDelayMSec)
{
    if (hoverLeaveDelayMSec < 0)
    {
        NX_ASSERT(false, "Invalid negative hover leave delay '%1'.", hoverLeaveDelayMSec);
        return;
    }

    m_hoverLeaveDelay = hoverLeaveDelayMSec;
}

void HoverFocusProcessor::setFocusEnterDelay(int focusEnterDelayMSec)
{
    if (focusEnterDelayMSec < 0)
    {
        NX_ASSERT(false, "Invalid negative focus enter delay '%1'.", focusEnterDelayMSec);
        return;
    }

    m_focusEnterDelay = focusEnterDelayMSec;
}

void HoverFocusProcessor::setFocusLeaveDelay(int focusLeaveDelayMSec)
{
    if (focusLeaveDelayMSec < 0)
    {
        NX_ASSERT(false, "Invalid negative focus leave delay '%1'.", focusLeaveDelayMSec);
        return;
    }

    m_focusLeaveDelay = focusLeaveDelayMSec;
}

void HoverFocusProcessor::addTargetWidget(QWidget* widget)
{
    if (!NX_ASSERT(widget))
        return;

    if (!m_widgets.contains(widget))
        m_widgets.push_back(widget);

    widget->installEventFilter(this);
}

void HoverFocusProcessor::addTargetItem(QGraphicsItem* item)
{
    if (!NX_ASSERT(item))
        return;

    if (m_items.contains(item))
        return;

    if (scene() != nullptr)
        item->installSceneEventFilter(this);
    item->setAcceptHoverEvents(true);

    m_items.push_back(item);
}

void HoverFocusProcessor::removeTargetWidget(QWidget* widget)
{
    /* Removing an item that is not there is OK, even if it's a nullptr item. */
    if (m_widgets.removeOne(widget))
        widget->removeEventFilter(this);
}

void HoverFocusProcessor::removeTargetItem(QGraphicsItem* item)
{
    /* Removing an item that is not there is OK, even if it's a nullptr item. */
    bool removed = m_items.removeOne(item);
    if (removed && scene())
        item->removeSceneEventFilter(this);
}

QList<QGraphicsItem*> HoverFocusProcessor::targetItems() const
{
    return m_items.materialized();
}

void HoverFocusProcessor::forceHoverLeave()
{
    killHoverEnterTimer();

    if (m_hoverLeaveDelay == 0)
        processHoverLeave();
    else if (m_hoverLeaveTimerId == 0)
        m_hoverLeaveTimerId = startTimer(m_hoverLeaveDelay);
}

void HoverFocusProcessor::forceHoverEnter()
{
    killHoverLeaveTimer();

    if (m_hoverEnterDelay == 0)
        processHoverEnter();
    else if (m_hoverEnterTimerId == 0)
        m_hoverEnterTimerId = startTimer(m_hoverEnterDelay);
}

void HoverFocusProcessor::forceFocusLeave()
{
    killFocusEnterTimer();

    if (m_focusLeaveDelay == 0)
        processFocusLeave();
    else if (m_focusLeaveTimerId == 0)
        m_focusLeaveTimerId = startTimer(m_focusLeaveDelay);
}

void HoverFocusProcessor::forceFocusEnter()
{
    killFocusLeaveTimer();

    if (m_focusEnterDelay == 0)
        processFocusEnter();
    else if (m_focusEnterTimerId == 0)
        m_focusEnterTimerId = startTimer(m_focusEnterDelay);
}

bool HoverFocusProcessor::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type())
    {
        case QEvent::Enter:
            forceHoverEnter();
            event->ignore();
            break;
        case QEvent::Leave:
            forceHoverLeave();
            event->ignore();
            break;
        default:
            break;
    }

    return base_type::eventFilter(watched, event);
}

bool HoverFocusProcessor::sceneEventFilter(QGraphicsItem* watched, QEvent* event)
{
    switch (event->type())
    {
        case QEvent::GraphicsSceneHoverEnter:
            forceHoverEnter();
            event->ignore();
            break;
        case QEvent::GraphicsSceneHoverLeave:
            forceHoverLeave();
            event->ignore();
            break;
        case QEvent::FocusIn:
            forceFocusEnter();
            event->ignore();
            break;
        case QEvent::FocusOut:
            forceFocusLeave();
            event->ignore();
            break;
        default:
            break;
    }

    return base_type::sceneEventFilter(watched, event);
}

void HoverFocusProcessor::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == m_hoverEnterTimerId)
        processHoverEnter();
    else if (event->timerId() == m_hoverLeaveTimerId)
        processHoverLeave();
    else if (event->timerId() == m_focusEnterTimerId)
        processFocusEnter();
    else if (event->timerId() == m_focusLeaveTimerId)
        processFocusLeave();
}

QVariant HoverFocusProcessor::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (parentItem())
    {
        switch (change)
        {
            case ItemSceneChange:
            {
                if (!scene())
                    break;

                for (const auto& item: m_items)
                {
                    if (item && item.data()->scene() == scene())
                        item.data()->removeSceneEventFilter(this);
                }

                break;
            }

            case ItemSceneHasChanged:
            {
                if (!scene())
                    break;

                for (const auto& item: m_items)
                {
                    if (!item)
                        continue;

                    if (item.data()->scene() == scene())
                    {
                        item.data()->installSceneEventFilter(this);
                    }
                    else
                    {
                        /*
                         * In case this notification has been called from QGraphicsScene::addItem()
                         * and the hover processor was added to the scene before its watched items.
                         */
                        executeDelayed(
                            [this, item]()
                            {
                                if (item && scene() && item.data()->scene() == scene())
                                    item.data()->installSceneEventFilter(this);
                            });
                    }
                }

                break;
            }

            default:
                break;
        }
    }

    return base_type::itemChange(change, value);
}

void HoverFocusProcessor::processHoverEnter()
{
    killHoverEnterTimer();

    m_hovered = true;
    emit hoverEntered();
    if (m_focused)
        emit hoverAndFocusEntered();
}

void HoverFocusProcessor::processHoverLeave()
{
    killHoverLeaveTimer();

    m_hovered = false;
    emit hoverLeft();
    if (!m_focused)
        emit hoverAndFocusLeft();
}

void HoverFocusProcessor::processFocusEnter()
{
    killFocusEnterTimer();

    m_focused = true;
    emit focusEntered();
    if (m_hovered)
        emit hoverAndFocusEntered();
}

void HoverFocusProcessor::processFocusLeave()
{
    killFocusLeaveTimer();

    m_focused = false;
    emit focusLeft();
    if (!m_hovered)
        emit hoverAndFocusLeft();
}

void HoverFocusProcessor::killHoverEnterTimer()
{
    if (m_hoverEnterTimerId == 0)
        return;

    killTimer(m_hoverEnterTimerId);
    m_hoverEnterTimerId = 0;
}

void HoverFocusProcessor::killHoverLeaveTimer()
{
    if (m_hoverLeaveTimerId == 0)
        return;

    killTimer(m_hoverLeaveTimerId);
    m_hoverLeaveTimerId = 0;
}

void HoverFocusProcessor::killFocusEnterTimer()
{
    if (m_focusEnterTimerId == 0)
        return;

    killTimer(m_focusEnterTimerId);
    m_focusEnterTimerId = 0;
}

void HoverFocusProcessor::killFocusLeaveTimer()
{
    if (m_focusLeaveTimerId == 0)
        return;

    killTimer(m_focusLeaveTimerId);
    m_focusLeaveTimerId = 0;
}
