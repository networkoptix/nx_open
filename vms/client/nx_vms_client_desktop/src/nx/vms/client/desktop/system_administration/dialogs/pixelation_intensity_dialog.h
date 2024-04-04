// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QStringList>

#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API PixelationIntensityDialog: public QmlDialogWrapper
{
    Q_OBJECT

public:
    PixelationIntensityDialog(double intensity, QWidget* parent = nullptr);

    double intensity() const;
};

} // namespace nx::vms::client::desktop
