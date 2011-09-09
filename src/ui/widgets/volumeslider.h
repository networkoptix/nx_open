#ifndef VOLUMESLIDER_H
#define VOLUMESLIDER_H

#include "graphicsslider.h"

class VolumeSlider : public GraphicsSlider
{
    Q_OBJECT

public:
    explicit VolumeSlider(Qt::Orientation orientation = Qt::Horizontal, QGraphicsItem *parent = 0);
    ~VolumeSlider();

    bool isMute() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

public Q_SLOTS:
    void setMute(bool mute);

protected:
    void sliderChange(SliderChange change);

    void timerEvent(QTimerEvent *);

private Q_SLOTS:
    void onValueChanged(int);

private:
    Q_DISABLE_COPY(VolumeSlider)

    int m_timerId;
    QString m_text;
};

#endif // VOLUMESLIDER_H
