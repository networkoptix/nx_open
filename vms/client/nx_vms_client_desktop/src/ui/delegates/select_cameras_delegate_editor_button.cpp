#include "select_cameras_delegate_editor_button.h"

#include <ui/dialogs/resource_selection_dialog.h>

using namespace nx::vms::client::desktop;

QnSelectCamerasDialogButton::QnSelectCamerasDialogButton(QWidget* parent):
    base_type(parent)
{
    m_selectCamerasFunction =
        [this](QnUuidSet& resources)
        {
            return CameraSelectionDialog::selectCameras<CameraSelectionDialog::DummyPolicy>(
                resources, this);
        };

    connect(this, SIGNAL(clicked()), this, SLOT(at_clicked()));
}

QnSelectCamerasDialogButton::~QnSelectCamerasDialogButton()
{
}

void QnSelectCamerasDialogButton::at_clicked()
{
    if (!m_selectCamerasFunction)
    {
        NX_ASSERT(false);
        return;
    }

    auto camerasIds = getResources();
    auto dialogAccepted = m_selectCamerasFunction(camerasIds);
    if (!dialogAccepted)
        return;

    setResources(camerasIds);
    emit commit();
}
