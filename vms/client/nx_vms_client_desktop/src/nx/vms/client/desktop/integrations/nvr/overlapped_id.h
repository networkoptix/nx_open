// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRegularExpression>

#include "../interfaces.h"

namespace nx::vms::client::desktop {

class WindowContext;

namespace integrations {

class OverlappedIdStore;

/**
 * Adds menu action for Hanhwa NVR devices,
 * allowing to choose which Overlapped ID is applied on NVR.
 */
class OverlappedIdIntegration:
    public Integration,
    public IMenuActionFactory
{
    Q_OBJECT
    using base_type = Integration;

public:
    explicit OverlappedIdIntegration(QObject* parent);
    virtual ~OverlappedIdIntegration() override;

public:
    virtual void registerActions(menu::MenuFactory* factory) override;

private:
    void openOverlappedIdDialog(WindowContext* context);

private:
    std::shared_ptr<OverlappedIdStore> m_store;
};

} // namespace integrations
} // namespace nx::vms::client::desktop
