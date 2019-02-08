#pragma once

#include <QtWidgets/QStyledItemDelegate>

namespace nx::vms::client::desktop {

class StyledComboBoxDelegate : public QStyledItemDelegate
{
    Q_OBJECT
    using base_type = QStyledItemDelegate;

public:
    StyledComboBoxDelegate(QObject* parent = nullptr);
    virtual ~StyledComboBoxDelegate() override = default;

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
        override;

    static bool isSeparator(const QModelIndex& index);

protected:
    virtual void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
        override;
};

} // namespace nx::vms::client::desktop
