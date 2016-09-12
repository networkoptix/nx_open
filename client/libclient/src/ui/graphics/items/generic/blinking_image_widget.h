#pragma once

#include <ui/graphics/items/generic/image_button_widget.h>

class QnToolTipWidget;
class QnParticleItem;

/**
* An image button widget that displays thumbnail behind the button.
*/
class QnBlinkingImageButtonWidget: public QnImageButtonWidget, public AnimationTimerListener {
    Q_OBJECT

   using base_type = QnImageButtonWidget;

public:
    QnBlinkingImageButtonWidget(QGraphicsItem *parent = NULL);

    public slots:
    void setNotificationCount(int count);
    void setColor(const QColor &color);

protected:
    virtual void tick(int deltaMSecs) override;

    private slots:
    void showBalloon();
    void hideBalloon();

    void updateParticleGeometry();
    void updateParticleVisibility();
    void updateToolTip();
    void updateBalloonTailPos();
    void updateBalloonGeometry();

    void at_particle_visibleChanged();

private:
    QnToolTipWidget* m_balloon;
    QnParticleItem* m_particle;
    qint64 m_time;
    int m_count;
};
