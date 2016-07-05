#ifndef STYLED_TOOLTIP_WIDGET_H
#define STYLED_TOOLTIP_WIDGET_H

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

protected:
    virtual void updateTailPos();

private:
    qreal m_tailLength;
};

#endif // STYLED_TOOLTIP_WIDGET_H
