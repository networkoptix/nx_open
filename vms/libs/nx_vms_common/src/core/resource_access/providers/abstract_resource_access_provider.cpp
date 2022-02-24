// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_resource_access_provider.h"
#include <core/resource_access/resource_access_subject.h>

namespace nx::core::access {

AbstractResourceAccessProvider::AbstractResourceAccessProvider(
    Mode mode,
    QObject* parent)
    :
    base_type(parent),
    QnUpdatable(),
    m_mode(mode)
{
}

AbstractResourceAccessProvider::~AbstractResourceAccessProvider()
{
}

void AbstractResourceAccessProvider::clear()
{
}

Mode AbstractResourceAccessProvider::mode() const
{
    return m_mode;
}

} // namespace nx::core::access
