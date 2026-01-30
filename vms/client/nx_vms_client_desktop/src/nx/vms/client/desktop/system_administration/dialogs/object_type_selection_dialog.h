// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QStringList>

#include <nx/vms/api/data/pixelation_settings.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>
#include <nx/vms/client/desktop/current_system_context_aware.h>

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API ObjectTypeSelectionDialog:
    public QmlDialogWrapper,
    public SystemContextAware
{
    Q_OBJECT

public:
    ObjectTypeSelectionDialog(
        SystemContext* systemContext,
        bool allObjects,
        const QStringList& objectTypeIds,
        const QStringList& availableTypeIds,
        QWidget* parent = nullptr);

    bool allObjects() const;
    QStringList objectTypeIds() const;

    static void openSelectionDialog(
        api::ObjectTypeSettings& settings,
        QWidget* parent,
        const std::function<void()>& callback,
        const std::optional<nx::Uuid>& cameraResource = std::nullopt);
};

} // namespace nx::vms::client::desktop
