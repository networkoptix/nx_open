// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>

namespace nx::vms::client::desktop {

class ServerOnlineStatusWatcher: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    ServerOnlineStatusWatcher(QObject* parent = nullptr);

    void setServer(const QnMediaServerResourcePtr& server);

    bool isOnline() const;

signals:
    void statusChanged();

private:
    QnMediaServerResourcePtr m_server;
};

} // namespace nx::vms::client::desktop
