// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QStringList>

#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>
#include <nx/vms/client/desktop/current_system_context_aware.h>

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API PixelationObjectSelectionDialog:
    public QmlDialogWrapper,
    public SystemContextAware
{
    Q_OBJECT

public:
    PixelationObjectSelectionDialog(
        SystemContext* systemContext,
        bool allObjects,
        const QStringList& objectTypeIds,
        QWidget* parent = nullptr);

    bool allObjects() const;
    QStringList objectTypeIds() const;
};

} // namespace nx::vms::client::desktop
