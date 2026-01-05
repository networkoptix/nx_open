// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_type_selection_dialog.h"

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

ObjectTypeSelectionDialog::ObjectTypeSelectionDialog(
    SystemContext* systemContext,
    bool allObjects,
    const QStringList& objectTypeIds,
    QWidget* parent)
    :
    QmlDialogWrapper(
        QUrl("Nx/Dialogs/Analytics/ObjectTypeSelectionDialog.qml"),
        QVariantMap{
            {"allObjects", allObjects},
            {"objectTypeIds", objectTypeIds}
        },
        parent),
    SystemContextAware(systemContext)
{
}

bool ObjectTypeSelectionDialog::allObjects() const
{
    return rootObjectHolder()->object()->property("allObjects").toBool();
}

QStringList ObjectTypeSelectionDialog::objectTypeIds() const
{
    return rootObjectHolder()->object()->property("objectTypeIds").toStringList();
}

void ObjectTypeSelectionDialog::openSelectionDialog(
    api::ObjectTypeSettings& settings,
    QWidget* parent,
    const std::function<void()>& callback)
{
    auto dialog = new ObjectTypeSelectionDialog(
        appContext()->currentSystemContext(),
        settings.isAllObjectTypes,
        settings.objectTypeIds,
        parent);

    connect(
        dialog,
        &QmlDialogWrapper::done,
        dialog,
        [dialog, &settings, callback](bool accepted)
        {
            if (accepted)
            {
                settings.isAllObjectTypes = dialog->allObjects();
                settings.objectTypeIds = dialog->objectTypeIds();

                if (callback)
                    callback();
            }

            dialog->deleteLater();
        });

    dialog->open();
}

} // namespace nx::vms::client::desktop
