// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

/**
 * Maintains data loading, which is required for the client functionality, but can be safely
 * delayed (and thus not included in full info). Loading starts after full info is received, so
 * permissions can be checked on the client side before requesting.
 */
class DelayedDataLoader:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT

public:
    DelayedDataLoader(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~DelayedDataLoader() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
