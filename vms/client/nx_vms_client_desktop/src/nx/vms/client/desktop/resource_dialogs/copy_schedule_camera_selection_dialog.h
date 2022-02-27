// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "camera_selection_dialog.h"

class QCheckBox;

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;

class CopyScheduleCameraSelectionDialog: public CameraSelectionDialog
{
    Q_OBJECT
    using base_type = CameraSelectionDialog;

public:
    CopyScheduleCameraSelectionDialog(
        const CameraSettingsDialogState& settings,
        QWidget* parent);

    bool copyArchiveLength() const;
    void setCopyArchiveLength(bool copyArchiveLength);

private:
    QCheckBox* copyArchiveLengthCheckBox() const;
};

} // namespace nx::vms::client::desktop
