// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "select_cameras_delegate_editor_button.h"

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
}

QnSelectCamerasDialogButton::~QnSelectCamerasDialogButton()
{
}

void QnSelectCamerasDialogButton::handleButtonClicked()
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
