// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRegularExpression>

#include <nx/vms/client/core/network/remote_connection_aware.h>

#include "../interfaces.h"

class QnWorkbenchContext;

namespace nx::vms::client::desktop::integrations {

class OverlappedIdStore;

/**
 * Adds menu action for Hanhwa NVR devices,
 * allowing to choose which Overlapped ID is applied on NVR.
 */
class OverlappedIdIntegration:
    public Integration,
    public IMenuActionFactory,
    public nx::vms::client::core::RemoteConnectionAware
{
    Q_OBJECT
    using base_type = Integration;

public:
    explicit OverlappedIdIntegration(QObject* parent);
    virtual ~OverlappedIdIntegration() override;

public:
    virtual void registerActions(ui::action::MenuFactory* factory) override;

private:
    void openOverlappedIdDialog(QnWorkbenchContext* context);

private:
    std::unique_ptr<OverlappedIdStore> m_store;
};

} // namespace nx::vms::client::desktop::integrations
