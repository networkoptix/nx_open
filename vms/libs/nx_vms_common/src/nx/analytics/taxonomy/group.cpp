// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "group.h"

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

Group::Group(GroupDescriptor groupDescriptor, QObject* parent):
    AbstractGroup(parent),
    m_descriptor(std::move(groupDescriptor))
{
}

QString Group::id() const
{
    return m_descriptor.id;
}

QString Group::name() const
{
    return m_descriptor.name;
}

GroupDescriptor Group::serialize() const
{
    return m_descriptor;
}

} // namespace nx::analytics::taxonomy
