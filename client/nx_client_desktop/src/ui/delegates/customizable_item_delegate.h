#pragma once

#include <functional>
#include <QtWidgets/QStyledItemDelegate>

class QnCustomizableItemDelegate: public QStyledItemDelegate
{
    Q_OBJECT
    using base_type = QStyledItemDelegate;

public:
    /* Forward constructor(s). */
    using QStyledItemDelegate::QStyledItemDelegate;

    /*
    Custom initStyleOption.
    Base implementation is called before user functor.
    */
    using InitStyleOption = std::function<
        void (QStyleOptionViewItem*, const QModelIndex&)>;

    void setCustomInitStyleOption(InitStyleOption initStyleOption);

    /*
    Custom sizeHint.
    Base implementation is not called, but initStyleOption is already called.
    Call QnCustomizableItemDelegate::baseSizeHint from user functor if needed.
    */
    using SizeHint = std::function<
        QSize(const QStyleOptionViewItem&, const QModelIndex&)>;

    void setCustomSizeHint(SizeHint sizeHint);

    QSize baseSizeHint(const QStyleOptionViewItem& option,
        const QModelIndex& index) const;

    /*
    Custom paint.
    Base implementation is not called, but initStyleOption is already called.
    Call QnCustomizableItemDelegate::basePaint from user functor if needed.
    */
    using Paint = std::function<
        void (QPainter*, const QStyleOptionViewItem&, const QModelIndex&)>;

    void setCustomPaint(Paint paint);

    void basePaint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const;

public:
    /* Virtual overrides. */

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
