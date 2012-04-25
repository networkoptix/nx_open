#ifndef QN_VOLUME_SLIDER_H
#define QN_VOLUME_SLIDER_H

#include "tool_tip_slider.h"

class QnToolTipItem;

class QnVolumeSlider : public QnToolTipSlider
{
    Q_OBJECT
    Q_PROPERTY(bool muted READ isMute WRITE setMute)

    typedef QnToolTipSlider base_type;

public:
    explicit QnVolumeSlider(QGraphicsItem *parent = 0);
    virtual ~QnVolumeSlider();

    bool isMute() const;

public slots:
    void setMute(bool mute);

    void stepBackward();
    void stepForward();

protected:
    virtual void sliderChange(SliderChange change) override;
};

#endif // QN_VOLUME_SLIDER_H
