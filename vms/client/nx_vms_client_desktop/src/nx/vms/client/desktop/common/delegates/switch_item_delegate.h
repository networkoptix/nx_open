#pragma once

#include <QtWidgets/QStyledItemDelegate>

namespace nx::vms::client::desktop {

class SwitchItemDelegate: public QStyledItemDelegate
{
    Q_OBJECT
    using base_type = QStyledItemDelegate;

public:
    explicit SwitchItemDelegate(QObject* parent = nullptr);
    virtual ~SwitchItemDelegate() override = default;

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
        override;

    bool hideDisabledItems() const;
    void setHideDisabledItems(bool hide);

protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override;

private:
    bool m_hideDisabledItems = false;
};

} // namespace nx::vms::client::desktop
