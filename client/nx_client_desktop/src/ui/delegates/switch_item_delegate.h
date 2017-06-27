#pragma once

#include <QtWidgets/QStyledItemDelegate>

class QnSwitchItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
    typedef QStyledItemDelegate base_type;

public:
    explicit QnSwitchItemDelegate(QObject *parent = nullptr);

    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    bool hideDisabledItems() const;
    void setHideDisabledItems(bool hide);

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;

private:
    bool m_hideDisabledItems;
};
