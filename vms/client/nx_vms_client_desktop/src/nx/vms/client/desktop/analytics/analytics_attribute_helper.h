// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QVector>

#include <analytics/common/object_metadata.h>
#include <nx/utils/impl_ptr.h>

namespace nx::analytics::taxonomy {
class AbstractAttribute;
class AbstractStateWatcher;
} // namespace nx::analytics::taxonomy

namespace nx::vms::client::desktop::analytics {

class AttributeHelper
{
public:
    explicit AttributeHelper(
        nx::analytics::taxonomy::AbstractStateWatcher* taxonomyStateWatcher);
    virtual ~AttributeHelper();

    using AttributePath = QVector<nx::analytics::taxonomy::AbstractAttribute*>;
    AttributePath attributePath(const QString& objectTypeId, const QString& attributeName) const;

    nx::common::metadata::GroupedAttributes preprocessAttributes(const QString& objectTypeId,
        const nx::common::metadata::Attributes& sourceAttributes) const;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::analytics
