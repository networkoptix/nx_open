// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "pixelation_intensity_dialog.h"

namespace nx::vms::client::desktop {

PixelationIntensityDialog::PixelationIntensityDialog(
    double intensity,
    QWidget* parent)
    :
    QmlDialogWrapper(
        QUrl("Nx/Dialogs/Analytics/PixelationIntensityDialog.qml"),
        QVariantMap{{"intensity", intensity}},
        parent)
{
}

double PixelationIntensityDialog::intensity() const
{
    return rootObjectHolder()->object()->property("intensity").toDouble();
}

} // namespace nx::vms::client::desktop
