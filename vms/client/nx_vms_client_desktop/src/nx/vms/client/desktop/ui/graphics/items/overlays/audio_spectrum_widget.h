// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <qt_graphics_items/graphics_widget.h>

#include <nx/vms/client/desktop/camera/camera_fwd.h>

namespace nx::vms::client::desktop {

class AudioSpectrumWidget: public GraphicsWidget
{
    Q_OBJECT
    using base_type = GraphicsWidget;

public:
    AudioSpectrumWidget(QnResourceDisplayPtr display, QGraphicsWidget* parent);
    virtual ~AudioSpectrumWidget() override;

    bool isMuted() const;
    void setMuted(bool muted);

signals:
    void mutedChanged();

private:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    struct Private;
    const std::unique_ptr<Private> d;
};

} // namespace nx::vms::client::desktop
