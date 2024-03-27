// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "video_preview_widget.h"

#include <QtQuick/QQuickItem>

#include <core/resource/camera_resource.h>
#include <nx/vms/client/desktop/application_context.h>

using namespace nx::vms::client::desktop;

VideoPreviewWidget::VideoPreviewWidget(QWidget* parent)
    : QQuickWidget(appContext()->qmlEngine(), parent)
{
    setSource(QUrl("Nx/Items/VideoPreviewItem.qml"));
    setResizeMode(QQuickWidget::SizeRootObjectToView);
}

VideoPreviewWidget::~VideoPreviewWidget()
{
}

QnVirtualCameraResourcePtr VideoPreviewWidget::camera() const
{
    return m_camera;
}

void VideoPreviewWidget::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    m_camera = camera;
    rootObject()->setProperty("cameraResource", QVariant::fromValue(camera.data()));
}

void VideoPreviewWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    QWidget::mouseDoubleClickEvent(event);
}

void VideoPreviewWidget::mouseMoveEvent(QMouseEvent* event)
{
    QWidget::mouseMoveEvent(event);
}

void VideoPreviewWidget::mousePressEvent(QMouseEvent* event)
{
    QWidget::mousePressEvent(event);
}

void VideoPreviewWidget::mouseReleaseEvent(QMouseEvent* event)
{
    QWidget::mouseReleaseEvent(event);
}
