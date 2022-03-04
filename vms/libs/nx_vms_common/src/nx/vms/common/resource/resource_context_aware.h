// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms::common {

class ResourceContext;

/**
 * Helper class for the ResourceContext-dependent classes. Must be destroyed before the Context is.
 */
class NX_VMS_COMMON_API ResourceContextAware
{
public:
    ResourceContextAware(ResourceContext* context):
        m_context(context)
    {
    }

protected:
    // TODO: #sivanov After context is moved out of CommonModule, it can be converted to QObject
    // or shared pointer. Then we can store smart pointer here and validate it in the destructor.
    ResourceContext* const m_context;
};

} // namespace nx::vms::common
