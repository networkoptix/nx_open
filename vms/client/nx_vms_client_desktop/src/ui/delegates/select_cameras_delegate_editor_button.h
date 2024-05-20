// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <ui/delegates/select_resources_delegate_editor_button.h>

class QnSelectCamerasDialogButton: public QnSelectResourcesDialogButton
{
    Q_OBJECT
    using base_type = QnSelectResourcesDialogButton;

public:
    explicit QnSelectCamerasDialogButton(QWidget* parent = nullptr);
    virtual ~QnSelectCamerasDialogButton() override;

    template<typename ResourcePolicy>
    void setResourcePolicy()
    {
        m_selectCamerasFunction =
            [this](UuidSet& resources)
            {
                using namespace nx::vms::client::desktop;
                return CameraSelectionDialog::selectCameras<ResourcePolicy>(
                    appContext()->currentSystemContext(), resources, this);
            };
    }

protected:
    virtual void handleButtonClicked() override;

private:
    std::function<bool(UuidSet& camerasIds)> m_selectCamerasFunction;
};
