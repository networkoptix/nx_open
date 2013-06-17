#ifndef STYLED_TOOLTIP_WIDGET_H
#define STYLED_TOOLTIP_WIDGET_H

#include <ui/graphics/items/generic/tool_tip_widget.h>

class QnStyledTooltipWidget : public QnToolTipWidget {
    Q_OBJECT
    typedef QnToolTipWidget base_type;
public:
    QnStyledTooltipWidget(QGraphicsItem *parent = 0);
    virtual ~QnStyledTooltipWidget();

protected:
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;
    virtual void updateTailPos();
};

#endif // STYLED_TOOLTIP_WIDGET_H
