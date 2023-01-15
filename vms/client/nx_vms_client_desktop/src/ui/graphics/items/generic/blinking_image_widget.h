// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/graphics/items/generic/image_button_widget.h>

class QnParticleItem;

/**
* An image button widget that displays thumbnail behind the button.
*/
class QnBlinkingImageButtonWidget: public QnImageButtonWidget, public AnimationTimerListener
{
    Q_OBJECT

    using base_type = QnImageButtonWidget;

public:
    QnBlinkingImageButtonWidget(QGraphicsItem* parent = nullptr);

    void setParticleVisible(bool visible);

public slots:
    void setNotificationCount(int count);
    void setColor(const QColor& color);

protected:
    virtual void tick(int deltaMSecs) override;

private:
    void updateParticleGeometry();
    void updateParticleVisibility();
    void updateToolTip();

private:
    QnParticleItem* m_particle;
    qint64 m_time;
    int m_count;
};
