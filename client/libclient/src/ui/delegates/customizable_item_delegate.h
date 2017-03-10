#pragma once

#include <functional>
#include <QtWidgets/QStyledItemDelegate>

class QnCustomizableItemDelegate: public QStyledItemDelegate
{
    Q_OBJECT
    using base_type = QStyledItemDelegate;

public:
    QnCustomizableItemDelegate(QObject* parent = nullptr);

    using InitStyleOption = std::function<
        void (QStyleOptionViewItem*, const QModelIndex&)>;
    using CustomInitStyleOption = std::function<
        void (InitStyleOption /*base*/, QStyleOptionViewItem*, const QModelIndex&)>;

    using SizeHint = std::function<
        QSize (const QStyleOptionViewItem&, const QModelIndex&)>;
    using CustomSizeHint = std::function<
        QSize (SizeHint /*base*/, const QStyleOptionViewItem&, const QModelIndex&)>;

    using Paint = std::function<
        void (QPainter*, const QStyleOptionViewItem&, const QModelIndex&)>;
    using CustomPaint = std::function<
        void (Paint /*base*/, QPainter*, const QStyleOptionViewItem&, const QModelIndex&)>;

    void setCustomInitStyleOption(CustomInitStyleOption initStyleOption);
    void setCustomSizeHint(CustomSizeHint sizeHint);
    void setCustomPaint(CustomPaint paint);

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

    virtual QSize sizeHint(const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

protected:
    virtual void initStyleOption(QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

private:
    CustomInitStyleOption m_initStyleOption;
    CustomSizeHint m_sizeHint;
    CustomPaint m_paint;

    const InitStyleOption m_baseInitStyleOption;
    const SizeHint m_baseSizeHint;
    const Paint m_basePaint;
};
