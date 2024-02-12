// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QVariantMap>

#include <nx/utils/uuid.h>

#include "system_visibility_scope_info.h"
#include "welcome_screen_info.h"

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API SystemsVisibilityManager: public QObject
{
    Q_OBJECT
    typedef QObject base_type;

public:
    SystemsVisibilityManager(QObject* parent = nullptr);
    virtual ~SystemsVisibilityManager() override;

    static SystemsVisibilityManager* instance();

    void removeSystemData(const nx::Uuid& localId);

    welcome_screen::TileVisibilityScope cloudTileScope() const;
    void setCloudTileScope(welcome_screen::TileVisibilityScope visibilityScope);

    QString systemName(const nx::Uuid& localId) const;
    welcome_screen::TileVisibilityScope scope(const nx::Uuid& localId) const;

    bool setScopeInfo(const nx::Uuid& localId, const QString& name,
        welcome_screen::TileVisibilityScope visibilityScope);

private:
    SystemVisibilityScopeInfoHash m_visibilityScopes;
};

} // namespace nx::vms::client::core

#define qnSystemsVisibilityManager nx::vms::client::core::SystemsVisibilityManager::instance()
