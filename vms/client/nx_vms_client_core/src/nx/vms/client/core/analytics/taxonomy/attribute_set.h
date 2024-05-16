// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QObject>

#include <nx/analytics/taxonomy/abstract_object_type.h>
#include <nx/utils/impl_ptr.h>

Q_MOC_INCLUDE("nx/vms/client/core/analytics/taxonomy/attribute.h")

namespace nx::vms::client::core::analytics::taxonomy {

class Attribute;
class AbstractStateViewFilter;

/**
 * Special attribute type that contains other attributes. Corresponds Object attribute in the
 * analytics taxonomy.
 */
class NX_VMS_CLIENT_CORE_API AttributeSet: public QObject
{
    Q_OBJECT
    Q_PROPERTY(std::vector<nx::vms::client::core::analytics::taxonomy::Attribute*>
        attributes READ attributes CONSTANT)
public:
    // AttributeSet doesn't own the filter. The filter should live longer than the AttributeSet.
    AttributeSet(const AbstractStateViewFilter* filter, QObject* parent);
    virtual ~AttributeSet() override;

    std::vector<Attribute*> attributes() const;

    void addObjectType(nx::analytics::taxonomy::AbstractObjectType* objectType);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core::analytics::taxonomy
