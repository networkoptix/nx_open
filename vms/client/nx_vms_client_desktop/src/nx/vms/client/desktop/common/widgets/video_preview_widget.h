// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQuickWidgets/QQuickWidget>

#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {

class VideoPreviewWidget: public QQuickWidget
{
    Q_OBJECT

public:
    VideoPreviewWidget(QWidget* parent = nullptr);
    virtual ~VideoPreviewWidget() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

protected:
    virtual void mouseDoubleClickEvent(QMouseEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QnVirtualCameraResourcePtr m_camera;
};

} // namespace nx::vms::client::desktop
