// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class ServerSettingsDialogStore;

class ServerSettingsSaasStateWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit ServerSettingsSaasStateWatcher(
        ServerSettingsDialogStore* store,
        QObject* parent = nullptr);

    virtual ~ServerSettingsSaasStateWatcher() override;

    void setServer(const QnMediaServerResourcePtr& server);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // nx::vms::client::desktop
