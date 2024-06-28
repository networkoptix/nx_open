// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/settings/welcome_screen_info.h>
#include <nx/vms/client/core/system_finder/abstract_system_finder.h>

class NX_VMS_CLIENT_CORE_API AbstractSystemsController: public QObject
{
    Q_OBJECT

public:
    AbstractSystemsController(QObject* parent = nullptr);
    virtual ~AbstractSystemsController() override;

public:
    virtual nx::vms::client::core::CloudStatusWatcher::Status cloudStatus() const = 0;
    virtual QString cloudLogin() const = 0;
    virtual nx::vms::client::core::SystemDescriptionList systemsList() const = 0;
    virtual nx::vms::client::core::welcome_screen::TileVisibilityScope visibilityScope(
        const nx::Uuid& localId) const = 0;
    virtual bool setScopeInfo(
        const nx::Uuid& localId,
        const QString& name,
        nx::vms::client::core::welcome_screen::TileVisibilityScope visibilityScope) = 0;

signals:
    void cloudStatusChanged();
    void cloudLoginChanged();

    void systemDiscovered(const nx::vms::client::core::SystemDescriptionPtr& systemDescription);
    void systemLost(const QString& systemId, const nx::Uuid& localId);
};
