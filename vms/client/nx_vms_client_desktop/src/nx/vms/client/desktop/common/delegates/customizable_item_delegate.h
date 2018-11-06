#pragma once

#include <functional>
#include <QtWidgets/QStyledItemDelegate>

namespace nx::vms::client::desktop {

class CustomizableItemDelegate: public QStyledItemDelegate
{
    Q_OBJECT
    using base_type = QStyledItemDelegate;

public:
    using base_type::base_type; //< Forward constructors.

    /**
     * Custom initStyleOption.
     * Base implementation is called before user functor.
     */
    using InitStyleOption = std::function<
        void (QStyleOptionViewItem*, const QModelIndex&)>;

    void setCustomInitStyleOption(InitStyleOption initStyleOption);

    /**
     * Custom sizeHint.
     * Base implementation is not called, but initStyleOption is already called.
     * Call CustomizableItemDelegate::baseSizeHint from user functor if needed.
     */
    using SizeHint = std::function<
        QSize(const QStyleOptionViewItem&, const QModelIndex&)>;

    void setCustomSizeHint(SizeHint sizeHint);

    QSize baseSizeHint(const QStyleOptionViewItem& option,
        const QModelIndex& index) const;

    /**
     * Custom paint.
     * Base implementation is not called, but initStyleOption is already called.
     * Call CustomizableItemDelegate::basePaint from user functor if needed.
     */
    using Paint = std::function<
        void (QPainter*, const QStyleOptionViewItem&, const QModelIndex&)>;

    void setCustomPaint(Paint paint);

    void basePaint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const;

public:
    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    virtual QSize sizeHint(const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

protected:
    virtual void initStyleOption(QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

private:
    InitStyleOption m_initStyleOption;
    SizeHint m_sizeHint;
    Paint m_paint;
};

} // namespace nx::vms::client::desktop
