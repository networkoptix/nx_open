#pragma once

#include <QtWidgets/QStyledItemDelegate>

class QnStyledComboBoxDelegate : public QStyledItemDelegate
{
    Q_OBJECT
    typedef QStyledItemDelegate base_type;

public:
    QnStyledComboBoxDelegate(QObject* parent = nullptr);

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    static bool isSeparator(const QModelIndex& index);

protected:
    virtual void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override;
};
