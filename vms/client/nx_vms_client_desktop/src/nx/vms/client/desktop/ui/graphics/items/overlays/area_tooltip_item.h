#pragma once

#include <memory>

#include <QtWidgets/QGraphicsItem>
#include <QtGui/QFont>

namespace nx::vms::client::desktop {

class AreaTooltipItem: public QGraphicsItem
{
    using base_type = QGraphicsItem;

public:
    AreaTooltipItem(QGraphicsItem* parent = nullptr);
    virtual ~AreaTooltipItem() override;

    QString text() const;
    void setText(const QString& text);

    QFont font() const;
    void setFont(const QFont& font);

    QColor textColor() const;
    void setTextColor(const QColor& color);

    QColor backgroundColor() const;
    void setBackgroundColor(const QColor& color);

    virtual QRectF boundingRect() const override;
    virtual void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget = nullptr) override;

    QMarginsF textMargins() const;

    QRectF targetObjectGeometry() const;
    void setTargetObjectGeometry(const QRectF& geometry);

private:
    class Private;
    const std::unique_ptr<Private> d;
};

} // namespace nx::vms::client::desktop
