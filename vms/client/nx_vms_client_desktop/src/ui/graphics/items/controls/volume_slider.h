#pragma once

#include <ui/graphics/items/generic/tool_tip_slider.h>

class QnVolumeSlider: public QnToolTipSlider
{
    Q_OBJECT
    Q_PROPERTY(bool muted READ isMute WRITE setMute)

    using base_type = QnToolTipSlider;

public:
    explicit QnVolumeSlider(QGraphicsItem* parent = nullptr);
    virtual ~QnVolumeSlider();

    bool isMute() const;

public slots:
    void setMute(bool mute);

    void stepBackward();
    void stepForward();

protected:
    virtual void sliderChange(SliderChange change) override;

    virtual void setupShowAnimator(VariantAnimator* animator) const override;
    virtual void setupHideAnimator(VariantAnimator* animator) const override;

private:
    bool m_updating;
};
