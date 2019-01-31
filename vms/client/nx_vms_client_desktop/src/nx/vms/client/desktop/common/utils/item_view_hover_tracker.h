#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QAbstractItemView>

namespace nx::vms::client::desktop {

class ItemViewHoverTracker: public QObject
{
    Q_OBJECT

public:
    explicit ItemViewHoverTracker(QAbstractItemView* parent);

    const QModelIndex& hoveredIndex() const;

    int mouseCursorRole() const;
    void setMouseCursorRole(int value);
    static constexpr int kNoMouseCursorRole = -1;

signals:
    void itemEnter(const QModelIndex& index);
    void itemLeave(const QModelIndex& index);
    void columnEnter(int column);
    void columnLeave(int column);
    void rowEnter(int row);
    void rowLeave(int row);

private:
    void changeHover(const QModelIndex& index);
    QRect columnRect(const QModelIndex& index) const;
    QRect rowRect(const QModelIndex& index) const;

    void updateMouseCursor();

private:
    QAbstractItemView* const m_itemView = nullptr;
    QPersistentModelIndex m_hoveredIndex;
    int m_mouseCursorRole = kNoMouseCursorRole;
};

} // namespace nx::vms::client::desktop
