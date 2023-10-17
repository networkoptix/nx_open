// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QTimer>
#include <QtWidgets/QSlider>

#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop::workbench::timeline {

class SliderToolTip;

class VolumeSlider: public QSlider
{
    Q_OBJECT
    Q_PROPERTY(qreal muted READ isMute WRITE setMute)

    using base_type = QSlider;

public:
    explicit VolumeSlider(WindowContext* context, QWidget* parent = nullptr);
    virtual ~VolumeSlider() override;

    bool isMute() const;
    void setTooltipsVisible(bool enabled);

public slots:
    void setMute(bool mute);

    void stepBackward();
    void stepForward();

    void hideToolTip();
    void showToolTip();

protected:
    virtual void sliderChange(SliderChange change) override;
    virtual bool eventFilter(QObject* watched, QEvent* event) override;
    virtual void wheelEvent(QWheelEvent* event) override;

protected slots:
    void updateTooltipPos();
    virtual void enterEvent(QEnterEvent* event) override;
    virtual void leaveEvent(QEvent* event) override;

private:
    bool m_updating = false;
    bool m_tooltipsEnabled = true;

    QTimer m_hideTooltipTimer;
    std::unique_ptr<SliderToolTip> m_tooltip;
};

} // namespace nx::vms::client::desktop::workbench::timeline
