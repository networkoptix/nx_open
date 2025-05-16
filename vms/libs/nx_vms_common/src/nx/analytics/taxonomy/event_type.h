// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QString>

#include <nx/analytics/taxonomy/abstract_attribute.h>
#include <nx/analytics/taxonomy/base_object_event_type.h>
#include <nx/analytics/taxonomy/scope.h>
#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics::taxonomy {

struct InternalState;
class ErrorHandler;
class AbstractResourceSupportProxy;

class NX_VMS_COMMON_API EventType: public BaseObjectEventType<EventType, nx::vms::api::analytics::EventTypeDescriptor>
{
    using base_type = BaseObjectEventType<EventType, nx::vms::api::analytics::EventTypeDescriptor>;

    Q_OBJECT

    Q_PROPERTY(EventType* baseType READ base CONSTANT)
    Q_PROPERTY(std::vector<EventType*> derivedTypes READ derivedTypes CONSTANT)

    Q_PROPERTY(bool isStateDependent READ isStateDependent CONSTANT)
    Q_PROPERTY(bool isRegionDependent READ isRegionDependent CONSTANT)
    Q_PROPERTY(bool isHidden READ isHidden CONSTANT)
    Q_PROPERTY(bool useTrackBestShotAsPreview READ useTrackBestShotAsPreview CONSTANT)

public:
    EventType(
        nx::vms::api::analytics::EventTypeDescriptor eventTypeDescriptor,
        AbstractResourceSupportProxy* resourceSupportProxy);


    bool isStateDependent() const;
    bool isRegionDependent() const;
    bool isHidden() const;
    bool useTrackBestShotAsPreview() const;
};

} // namespace nx::analytics::taxonomy
