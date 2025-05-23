// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <qt_graphics_items/graphics_widget.h>

#include <api/model/api_ioport_data.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/camera/iomodule/io_module_monitor.h>
#include <nx/vms/client/desktop/window_context_aware.h>

class QnIoModuleOverlayWidgetPrivate;
class QnIoModuleOverlayContents;

/*
Resource overlay providing user interaction with I/O module
*/
class QnIoModuleOverlayWidget:
    public GraphicsWidget,
    public nx::vms::client::desktop::WindowContextAware
{
    Q_OBJECT
    using base_type = GraphicsWidget;

public:
    QnIoModuleOverlayWidget(
        const QnVirtualCameraResourcePtr& module,
        const nx::vms::client::core::IOModuleMonitorPtr& monitor,
        nx::vms::client::desktop::WindowContext* windowContext,
        QGraphicsWidget* parent = nullptr);
    virtual ~QnIoModuleOverlayWidget();

    using Style = nx::vms::api::IoModuleVisualStyle;

    /** Overlay style set in accordance with resource settings: */
    Style overlayStyle() const;

    /** Whether user is allowed to toggle output ports: */
    bool userInputEnabled() const;
    void setUserInputEnabled(bool value);

private:
    QScopedPointer<QnIoModuleOverlayWidgetPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnIoModuleOverlayWidget)
};

/*
Visual contents of I/O module overlay
*/
class QnIoModuleOverlayContents: public QGraphicsWidget
{
    Q_OBJECT
    using base_type = QGraphicsWidget;

public:
    QnIoModuleOverlayContents();
    virtual ~QnIoModuleOverlayContents();

    virtual void portsChanged(const QnIOPortDataList& ports, bool userInputEnabled) = 0;
    virtual void stateChanged(const QnIOPortData& port, const QnIOStateData& state) = 0;

    QnIoModuleOverlayWidget* overlayWidget() const;

signals:
    void userClicked(const QString& port);
};
