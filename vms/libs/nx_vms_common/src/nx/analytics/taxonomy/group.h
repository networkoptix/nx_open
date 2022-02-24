// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/abstract_group.h>

#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics::taxonomy {

class Group: public AbstractGroup
{
public:
    Group(nx::vms::api::analytics::GroupDescriptor groupDescriptor, QObject* parent = nullptr);

    virtual QString id() const override;

    virtual QString name() const override;

    virtual nx::vms::api::analytics::GroupDescriptor serialize() const override;

private:
    nx::vms::api::analytics::GroupDescriptor m_descriptor;
};

} // namespace nx::analytics::taxonomy
