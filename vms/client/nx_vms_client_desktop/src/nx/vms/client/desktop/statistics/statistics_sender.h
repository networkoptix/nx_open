// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

/**
 * As soon as corresponding System Context loaded system settings, checks if statistics can be sent
 * and sends it if allowed.
 */
class StatisticsSender:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT

public:
    StatisticsSender(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~StatisticsSender() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
