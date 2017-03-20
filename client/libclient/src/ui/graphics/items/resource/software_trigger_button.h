#pragma once

#include <ui/graphics/items/generic/image_button_widget.h>

class QnStyledTooltipWidget;
class HoverFocusProcessor;

class QnSoftwareTriggerButton: public QnImageButtonWidget
{
    Q_OBJECT
    using base_type = QnImageButtonWidget;

public:
    QnSoftwareTriggerButton(QGraphicsItem* parent = nullptr);
    virtual ~QnSoftwareTriggerButton();

    QString toolTip() const;
    void setToolTip(const QString& toolTip);

    Qt::Edge toolTipEdge() const;
    void setToolTipEdge(Qt::Edge edge);

protected:
    //TODO: #vkutin #common Remove this when normal icons have proper background.
    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

private:
    void updateToolTipPosition();
    void updateToolTipTailEdge();
    void updateToolTipVisibility();

private:
    QnStyledTooltipWidget* const m_toolTip;
    HoverFocusProcessor* const m_toolTipHoverProcessor;
    Qt::Edge m_toolTipEdge;
};
