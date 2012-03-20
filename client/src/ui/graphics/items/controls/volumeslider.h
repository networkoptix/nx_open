#ifndef VOLUMESLIDER_H
#define VOLUMESLIDER_H

#include "ui/graphics/items/standard/graphicsslider.h"

class QnToolTipItem;

class VolumeSlider : public GraphicsSlider
{
    Q_OBJECT
    Q_PROPERTY(bool muted READ isMute WRITE setMute)

public:
    explicit VolumeSlider(Qt::Orientation orientation = Qt::Horizontal, QGraphicsItem *parent = 0);
    ~VolumeSlider();

    bool isMute() const;

    void setToolTipItem(QnToolTipItem *toolTip);

#if 0
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
#endif

public Q_SLOTS:
    void setMute(bool mute);

protected:
    void timerEvent(QTimerEvent *);

private Q_SLOTS:
    void onValueChanged(int);

private:
    Q_DISABLE_COPY(VolumeSlider)

    QnToolTipItem *m_toolTip;
    int m_toolTipTimerId;
};

#endif // VOLUMESLIDER_H
