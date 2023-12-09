// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <optional>

#include <QtCore/QObject>
#include <QtWidgets/QAbstractItemView>

namespace nx::vms::client::desktop {

class ItemViewHoverTracker: public QObject
{
    Q_OBJECT

public:
    explicit ItemViewHoverTracker(QAbstractItemView* parent);

    QModelIndex hoveredIndex() const;

    std::optional<int> mouseCursorRole() const;
    void setMouseCursorRole(std::optional<int> role);

    /**
     * Predicate that determines the area within an item that triggers hovered state. If predicate
     * isn't set, the whole item area triggers hovered state.
     */
    using HoverMaskPredicate = std::function<bool(const QModelIndex&, const QPoint&)>;
    void setHoverMaskPredicate(const HoverMaskPredicate& predicate);

signals:
    void itemEnter(const QModelIndex& index);
    void itemLeave(const QModelIndex& index);
    void columnEnter(int column);
    void columnLeave(int column);
    void rowEnter(int row);
    void rowLeave(int row);

private:
    void setHoveredIndex(const QModelIndex& index);
    QRect columnRect(const QModelIndex& index) const;
    QRect rowRect(const QModelIndex& index) const;

    void updateMouseCursor();

private:
    QAbstractItemView* const m_itemView = nullptr;
    QPersistentModelIndex m_hoveredIndex;
    std::optional<int> m_mouseCursorRole;

    HoverMaskPredicate m_hoverMaskPredicate;
};

} // namespace nx::vms::client::desktop
