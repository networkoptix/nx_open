// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/menu/action_factories.h>

namespace nx::vms::client::desktop {
namespace menu {

class RadassActionFactory: public Factory
{
    Q_OBJECT
    using base_type = Factory;

public:
    RadassActionFactory(Manager* parent);
    virtual QList<QAction*> newActions(const Parameters& parameters,
        QObject* parent) override;
};

} // namespace menu
} // namespace nx::vms::client::desktop
