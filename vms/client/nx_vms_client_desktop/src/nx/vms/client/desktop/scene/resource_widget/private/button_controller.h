// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <api/model/api_ioport_data.h>
#include <api/server_rest_connection_fwd.h>
#include <camera/iomodule/iomodule_monitor.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

class GraphicsWidget;
class QnMediaResourceWidget;
class QnScrollableItemsWidget;

namespace nx::vms::client::desktop {

class ButtonController: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit ButtonController(QnMediaResourceWidget* mediaResourceWidget);
    virtual ~ButtonController() override;

    void setButtonContainer(QnScrollableItemsWidget* buttonsContainer);

    virtual void createButtons() = 0;
    virtual void removeButtons() = 0;

protected:
    virtual void handleChangedIOState(const QnIOStateData& value) = 0;
    virtual void handleApiReply(
        GraphicsWidget* button,
        bool success,
        rest::Handle requestId,
        nx::network::rest::JsonResult& result) = 0;

    void ensureButtonState(
        nx::vms::api::ExtendedCameraOutput outputType,
        std::function<GraphicsWidget*()> createButton);

    void createButtonIfNeeded(
        nx::vms::api::ExtendedCameraOutput outputType,
        std::function<GraphicsWidget*()> createButton);

    void removeButton(nx::vms::api::ExtendedCameraOutput outputType);

    void buttonClickHandler(
        GraphicsWidget* button,
        nx::vms::api::ExtendedCameraOutput outputType,
        nx::vms::api::EventState newState);

    QString getOutputId(nx::vms::api::ExtendedCameraOutput outputType) const;

    void openIoModuleConnection();

protected:
    QnMediaResourceWidget* m_parentWidget = nullptr;
    QnScrollableItemsWidget* m_buttonsContainer = nullptr;

    QnVirtualCameraResourcePtr m_camera;

    QMap<nx::vms::api::ExtendedCameraOutput, nx::Uuid> m_outputTypeToButtonId;

private:
    QMap<QString, QString> m_outputNameToId;

    QnIOModuleMonitorPtr m_ioModuleMonitor;
};

} // namespace nx::vms::client::desktop
