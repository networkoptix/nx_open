#pragma once

#include <ui/graphics/items/standard/graphics_label.h>

class QnClockLabel: public GraphicsLabel
{
    Q_OBJECT
    using base_type = GraphicsLabel;

public:
    QnClockLabel(QGraphicsItem* parent = nullptr);
    virtual ~QnClockLabel() override;

protected:
    virtual void timerEvent(QTimerEvent*) override;

private:
    int m_timerId = 0;
};
