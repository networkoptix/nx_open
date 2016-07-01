#pragma once

#include <QtCore/QObject>
#include <QtWidgets/QAbstractItemView>

class QnItemViewHoverTracker : public QObject
{
    Q_OBJECT

public:
    QnItemViewHoverTracker(QAbstractItemView* parent);

    const QModelIndex& hoveredIndex() const;

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

private:
    QAbstractItemView* m_itemView;
    QPersistentModelIndex m_hoveredIndex;
};
