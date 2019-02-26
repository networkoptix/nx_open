#pragma once

#include <ui/delegates/select_resources_delegate_editor_button.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>

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
            [this](QnUuidSet& resources)
            {
                using namespace nx::vms::client::desktop;
                return CameraSelectionDialog::selectCameras<ResourcePolicy>(resources, this);
            };
    }

private slots:
    void at_clicked();

private:
    std::function<bool(QnUuidSet& camerasIds)> m_selectCamerasFunction;
};
