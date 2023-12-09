// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "item_view_hover_tracker.h"

#include <QtGui/QHoverEvent>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <utils/common/event_processors.h>

namespace nx::vms::client::desktop {

ItemViewHoverTracker::ItemViewHoverTracker(QAbstractItemView* parent):
    QObject(parent),
    m_itemView(parent)
{
    NX_ASSERT(m_itemView);
    m_itemView->setMouseTracking(true);
    m_itemView->setAttribute(Qt::WA_Hover);

    installEventHandler(
        m_itemView, {QEvent::HoverEnter, QEvent::HoverLeave, QEvent::HoverMove}, this,
        [this](QObject* /*object*/, QEvent* event)
        {
            if (!m_itemView->isEnabled() || !m_itemView->model() || !m_itemView->viewport())
            {
                setHoveredIndex({});
                return;
            }

            if (event->type() == QEvent::HoverLeave)
            {
                setHoveredIndex({});
                return;
            };

            const auto eventPos = static_cast<QHoverEvent*>(event)->position().toPoint();
            if (m_itemView->childAt(eventPos) != m_itemView->viewport())
            {
                setHoveredIndex({});
                return;
            }

            const auto viewportPos = m_itemView->viewport()->mapFrom(m_itemView, eventPos);
            const auto indexAtPos = m_itemView->indexAt(viewportPos);

            if (!m_hoverMaskPredicate || m_hoverMaskPredicate(indexAtPos, viewportPos))
                setHoveredIndex(indexAtPos);
            else
                setHoveredIndex({});
        });
}

QModelIndex ItemViewHoverTracker::hoveredIndex() const
{
    return m_hoveredIndex;
}

std::optional<int> ItemViewHoverTracker::mouseCursorRole() const
{
    return m_mouseCursorRole;
}

void ItemViewHoverTracker::setMouseCursorRole(std::optional<int> role)
{
    m_mouseCursorRole = role;
}

void ItemViewHoverTracker::setHoverMaskPredicate(const HoverMaskPredicate& predicate)
{
    m_hoverMaskPredicate = predicate;
}

void ItemViewHoverTracker::updateMouseCursor()
{
    if (m_hoveredIndex.isValid() && m_mouseCursorRole)
    {
        const auto cursorShapeData = m_hoveredIndex.data(*m_mouseCursorRole);
        if (!cursorShapeData.isNull())
        {
            bool ok = false;
            const auto cursorShape = static_cast<Qt::CursorShape>(cursorShapeData.toInt(&ok));
            if (ok && cursorShape >= 0 && cursorShape <= Qt::LastCursor)
            {
                m_itemView->setCursor(cursorShape);
                return;
            }
        }
    }

    m_itemView->unsetCursor();
}

void ItemViewHoverTracker::setHoveredIndex(const QModelIndex& index)
{
    bool selectRows = m_itemView->selectionBehavior() == QAbstractItemView::SelectRows;
    bool selectColumns = m_itemView->selectionBehavior() == QAbstractItemView::SelectColumns;

    if (!selectRows)
        m_itemView->setProperty(style::Properties::kHoveredRowProperty, QVariant());

    if (index != m_hoveredIndex)
    {
        bool rowChange = selectRows && index.row() != m_hoveredIndex.row();
        bool columnChange = selectColumns && index.column() != m_hoveredIndex.column();

        // Emit leave signals and mark row or column rectangle for repaint.
        if (m_hoveredIndex.isValid())
        {
            if (columnChange)
            {
                m_itemView->viewport()->update(columnRect(m_hoveredIndex));
                emit columnLeave(m_hoveredIndex.column());
            }

            if (rowChange)
            {
                m_itemView->viewport()->update(rowRect(m_hoveredIndex));
                emit rowLeave(m_hoveredIndex.row());
            }

            emit itemLeave(m_hoveredIndex);
        }

        m_hoveredIndex = index;
        m_itemView->setProperty(
            style::Properties::kHoveredIndexProperty, QVariant::fromValue(m_hoveredIndex));

        updateMouseCursor();

        if (index.isValid())
        {
            if (rowChange)
            {
                m_itemView->setProperty(
                    style::Properties::kHoveredRowProperty, m_hoveredIndex.row());
            }

            // Emit enter signals and mark row or column rectangle for repaint.
            emit itemEnter(m_hoveredIndex);

            if (rowChange)
            {
                m_itemView->viewport()->update(rowRect(m_hoveredIndex));
                emit rowEnter(m_hoveredIndex.row());
            }

            if (columnChange)
            {
                m_itemView->viewport()->update(columnRect(m_hoveredIndex));
                emit columnEnter(m_hoveredIndex.column());
            }
        }
        else
        {
            // Clear current row index property.
            if (rowChange)
                m_itemView->setProperty(style::Properties::kHoveredRowProperty, -1);
        }
    }
}

QRect ItemViewHoverTracker::columnRect(const QModelIndex& index) const
{
    QRect rect = m_itemView->visualRect(index);
    rect.setTop(0);
    rect.setHeight(m_itemView->viewport()->height());
    return rect;
}

QRect ItemViewHoverTracker::rowRect(const QModelIndex& index) const
{
    QRect rect = m_itemView->visualRect(index);
    rect.setLeft(0);
    rect.setWidth(m_itemView->viewport()->width());
    return rect;
}

} // namespace nx::vms::client::desktop
