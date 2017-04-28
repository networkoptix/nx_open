#pragma once

#include <ui/graphics/items/generic/tool_tip_widget.h>

class QnStyledTooltipWidget: public QnToolTipWidget
{
    Q_OBJECT
    typedef QnToolTipWidget base_type;

public:
    QnStyledTooltipWidget(QGraphicsItem *parent = nullptr);
    virtual ~QnStyledTooltipWidget();

    virtual void setGeometry(const QRectF &rect) override;

    qreal tailLength() const;
    void setTailLength(qreal tailLength);

    Qt::Edge tailEdge() const;
    void setTailEdge(Qt::Edge tailEdge);

protected:
    using base_type::setTailPos;
    virtual void updateTailPos();

private:
    qreal m_tailLength;
    Qt::Edge m_tailEdge;
};
