#pragma once

#include <QtWidgets/QStyledItemDelegate>

class QnSwitchItemDelegate : public QStyledItemDelegate
{
    typedef QStyledItemDelegate base_type;

public:
    explicit QnSwitchItemDelegate(QObject *parent = nullptr);

    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;
};
