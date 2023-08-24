// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/server_rest_connection_fwd.h>
#include <nx/vms/api/data/ldap.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class LdapStatusWatcher: public QObject, public SystemContextAware
{
    Q_OBJECT

public:
    LdapStatusWatcher(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~LdapStatusWatcher() override;

    std::optional<api::LdapStatus> status() const;
    bool isOnline() const;

    void refresh();

signals:
    void statusChanged();
    void refreshFinished();

private:
    void setStatus(const api::LdapStatus& status);

private:
    std::optional<api::LdapStatus> m_status;
    rest::Handle m_refreshHandle = 0;
};

} // namespace nx::vms::client::desktop
