// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../action_factories.h"

namespace nx::vms::client::desktop {
namespace menu {

class RotateActionFactory: public Factory
{
    Q_OBJECT
    using base_type = Factory;

public:
    RotateActionFactory(Manager* parent);

    virtual ActionList newActions(
        const menu::Parameters& parameters,
        QObject* parent) override;
};

} // namespace menu {
} // namespace nx::vms::client::desktop
