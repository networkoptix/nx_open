// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <finders/abstract_systems_finder.h>
#include <network/base_system_description.h>
#include <nx/vms/client/core/settings/welcome_screen_info.h>
#include <watchers/cloud_status_watcher.h>

class AbstractSystemsController: public QObject
{
    Q_OBJECT

public:
    AbstractSystemsController(QObject* parent = nullptr);
    virtual ~AbstractSystemsController() override;

public:
    virtual QnCloudStatusWatcher::Status cloudStatus() const = 0;
    virtual QString cloudLogin() const = 0;
    virtual QnAbstractSystemsFinder::SystemDescriptionList systemsList() const = 0;
    virtual nx::vms::client::core::welcome_screen::TileVisibilityScope visibilityScope(
        const QnUuid& localId) const = 0;
    virtual bool setScopeInfo(
        const QnUuid& localId,
        const QString& name,
        nx::vms::client::core::welcome_screen::TileVisibilityScope visibilityScope) = 0;

signals:
    void cloudStatusChanged();
    void cloudLoginChanged();

    void systemDiscovered(const QnSystemDescriptionPtr& systemDescription);
    void systemLost(const QString& systemId);
};
