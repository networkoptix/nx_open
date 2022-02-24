// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "item_view_hover_tracker.h"

#include <QtGui/QHoverEvent>

#include <nx/vms/client/desktop/style/helper.h>
#include <utils/common/event_processors.h>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

ItemViewHoverTracker::ItemViewHoverTracker(QAbstractItemView* parent):
    QObject(parent),
    m_itemView(parent)
{
    NX_ASSERT(m_itemView);
    m_itemView->setMouseTracking(true);
    m_itemView->setAttribute(Qt::WA_Hover);

    installEventHandler(m_itemView, { QEvent::HoverEnter, QEvent::HoverLeave, QEvent::HoverMove }, this,
        [this](QObject* /*object*/, QEvent* event)
        {
            static const QModelIndex kNoHover;

            if (!m_itemView->isEnabled() || !m_itemView->model() || !m_itemView->viewport())
                changeHover(kNoHover);

            switch (event->type())
            {
                case QEvent::HoverLeave:
                {
                    changeHover(kNoHover);
                    break;
                }

                case QEvent::HoverEnter:
                case QEvent::HoverMove:
                {
                    QPoint pos = static_cast<QHoverEvent*>(event)->pos();
                    changeHover(m_itemView->childAt(pos) == m_itemView->viewport() ?
                        m_itemView->indexAt(m_itemView->viewport()->mapFrom(m_itemView, pos)) :
                        kNoHover);
                    break;
                }

                default:
                    break;
            }
        });
}

const QModelIndex& ItemViewHoverTracker::hoveredIndex() const
{
    return m_hoveredIndex;
}

int ItemViewHoverTracker::mouseCursorRole() const
{
    return m_mouseCursorRole;
}

void ItemViewHoverTracker::setMouseCursorRole(int value)
{
    if (value == m_mouseCursorRole)
        return;

    m_mouseCursorRole = value;

    if (m_mouseCursorRole != kNoMouseCursorRole)
        updateMouseCursor();
    else
        m_itemView->unsetCursor();
}

void ItemViewHoverTracker::updateMouseCursor()
{
    if (m_hoveredIndex.isValid())
    {
        bool ok = false;
        auto shape = static_cast<Qt::CursorShape>(m_hoveredIndex.data(m_mouseCursorRole).toInt(&ok));
        if (ok && shape >= 0 && shape <= Qt::LastCursor)
        {
            m_itemView->setCursor(shape);
            return;
        }
    }

    m_itemView->unsetCursor();
}

void ItemViewHoverTracker::changeHover(const QModelIndex& index)
{
    bool selectRows = m_itemView->selectionBehavior() == QAbstractItemView::SelectRows;
    bool selectColumns = m_itemView->selectionBehavior() == QAbstractItemView::SelectColumns;

    if (!selectRows)
        m_itemView->setProperty(style::Properties::kHoveredRowProperty, QVariant());

    if (index != m_hoveredIndex)
    {
        bool rowChange = selectRows && index.row() != m_hoveredIndex.row();
        bool columnChange = selectColumns && index.column() != m_hoveredIndex.column();

        /* Emit leave signals and mark row or column rectangle for repaint: */
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
        m_itemView->setProperty(style::Properties::kHoveredIndexProperty, QVariant::fromValue(m_hoveredIndex));

        if (m_mouseCursorRole != kNoMouseCursorRole)
            updateMouseCursor();

        if (index.isValid())
        {
            if (rowChange)
                m_itemView->setProperty(style::Properties::kHoveredRowProperty, m_hoveredIndex.row());

            /* Emit enter signals and mark row or column rectangle for repaint: */
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
            /* Clear current row index property: */
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
