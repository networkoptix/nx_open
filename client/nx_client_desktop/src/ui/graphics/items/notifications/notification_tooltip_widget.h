#pragma once

#include <ui/graphics/items/generic/graphics_tooltip_widget.h>

class QnNotificationToolTipWidget: public QnGraphicsToolTipWidget
{
    Q_OBJECT
    using base_type = QnGraphicsToolTipWidget;

public:
    QnNotificationToolTipWidget(QGraphicsItem* parent = nullptr);

    /** Rectangle where the tooltip should fit - in coordinates of list widget (parent's parent). */
    void setEnclosingGeometry(const QRectF& enclosingGeometry);

    virtual void updateTailPos() override;

signals:
    void closeTriggered();
    void buttonClicked(const QString& alias);

private:
    QRectF m_enclosingRect;
};
