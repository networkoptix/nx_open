// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/base_object_event_type.h>
#include <nx/analytics/taxonomy/scope.h>
#include <nx/analytics/taxonomy/utils.h>
#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics::taxonomy {

struct InternalState;
class ErrorHandler;
class AbstractResourceSupportProxy;

class NX_VMS_COMMON_API ObjectType:
    public BaseObjectEventType<ObjectType, nx::vms::api::analytics::ObjectTypeDescriptor>
{
    using base_type = BaseObjectEventType<ObjectType, nx::vms::api::analytics::ObjectTypeDescriptor>;

    Q_OBJECT

    Q_PROPERTY(ObjectType* baseType READ base CONSTANT)
    Q_PROPERTY(int baseTypeDepth READ baseTypeDepth CONSTANT)
    Q_PROPERTY(std::vector<ObjectType*> derivedTypes READ derivedTypes CONSTANT)

    Q_PROPERTY(bool isNonIndexable READ isNonIndexable CONSTANT)
    Q_PROPERTY(bool isLiveOnly READ isLiveOnly CONSTANT)

public:
    ObjectType(
        nx::vms::api::analytics::ObjectTypeDescriptor objectTypeDescriptor,
        AbstractResourceSupportProxy* resourceSupportProxy);

    virtual bool isNonIndexable() const;
    virtual bool isLiveOnly() const;
    int baseTypeDepth() const;
};

} // namespace nx::analytics::taxonomy
