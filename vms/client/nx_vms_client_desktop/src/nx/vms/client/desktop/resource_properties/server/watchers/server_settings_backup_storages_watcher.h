// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class ServerSettingsDialogStore;

class ServerSettingsBackupStoragesWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit ServerSettingsBackupStoragesWatcher(
        ServerSettingsDialogStore* store,
        QObject* parent = nullptr);

    virtual ~ServerSettingsBackupStoragesWatcher() override;

    void setServer(const QnMediaServerResourcePtr& value);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // nx::vms::client::desktop
