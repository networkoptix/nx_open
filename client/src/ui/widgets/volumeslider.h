#ifndef VOLUMESLIDER_H
#define VOLUMESLIDER_H

#include "ui/widgets2/graphicsslider.h"

class ToolTipItem;

class VolumeSlider : public GraphicsSlider
{
    Q_OBJECT
    Q_PROPERTY(bool muted READ isMute WRITE setMute)

public:
    explicit VolumeSlider(Qt::Orientation orientation = Qt::Horizontal, QGraphicsItem *parent = 0);
    ~VolumeSlider();

    bool isMute() const;

    void setToolTipItem(ToolTipItem *toolTip);

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

public Q_SLOTS:
    void setMute(bool mute);

protected:
    void timerEvent(QTimerEvent *);

private Q_SLOTS:
    void onValueChanged(int);

private:
    Q_DISABLE_COPY(VolumeSlider)

    ToolTipItem *m_toolTip;
    int m_toolTipTimerId;
};

#endif // VOLUMESLIDER_H
