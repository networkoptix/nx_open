// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "pixelation_object_selection_dialog.h"

#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

PixelationObjectSelectionDialog::PixelationObjectSelectionDialog(
    SystemContext* systemContext,
    bool allObjects,
    const QStringList& objectTypeIds,
    QWidget* parent)
    :
    QmlDialogWrapper(
        QUrl("Nx/Dialogs/Analytics/PixelationObjectSelectionDialog.qml"),
        QVariantMap{
            {"allObjects", allObjects},
            {"objectTypeIds",  objectTypeIds}
        },
        parent),
    SystemContextAware(systemContext)
{
}

bool PixelationObjectSelectionDialog::allObjects() const
{
    return rootObjectHolder()->object()->property("allObjects").toBool();
}

QStringList PixelationObjectSelectionDialog::objectTypeIds() const
{
    return rootObjectHolder()->object()->property("objectTypeIds").toStringList();
}

} // namespace nx::vms::client::desktop
