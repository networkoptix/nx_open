// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>

#include "../interfaces.h"

namespace nx::vms::client::desktop::integrations {

/**
 * Adds event rules for intercom call and door opening.
 */
class IntercomIntegration:
    public Integration,
    public IServerApiProcessor,
    public nx::vms::client::core::RemoteConnectionAware
{
    Q_OBJECT
    using base_type = Integration;

public:
    explicit IntercomIntegration(QObject* parent = nullptr);
    virtual ~IntercomIntegration() override;

    virtual void connectionEstablished(
        const QnUserResourcePtr& currentUser,
        ec2::AbstractECConnectionPtr connection) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::integrations
