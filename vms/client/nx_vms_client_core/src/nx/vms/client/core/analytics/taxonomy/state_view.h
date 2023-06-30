// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

Q_MOC_INCLUDE("nx/vms/client/core/analytics/taxonomy/object_type.h")

namespace nx::vms::client::core::analytics::taxonomy {

class ObjectType;

/**
 * Represents a view of the analytics taxonomy state. It contains only such object types (see
 * ObjectType) and attributes (see Attribute) that should be displayed in the GUI and match
 * corresponding filter (see AbstractStateViewFilter), which was used to build the view (see
 * StateViewBuilder). Basically it is a set of tree-like structures containing ObjectType items as
 * internal nodes and Attribute items as leafs, though in some cases a node can have 0 attributes
 * and some attributes can have nested attributes (see AttributeSet).
 */
class NX_VMS_CLIENT_CORE_API StateView: public QObject
{
    Q_OBJECT
    Q_PROPERTY(std::vector<nx::vms::client::core::analytics::taxonomy::ObjectType*>
        objectTypes READ objectTypes CONSTANT)
public:
    StateView(std::vector<ObjectType*> objectTypes, QObject* parent);

    std::vector<ObjectType*> objectTypes() const { return m_objectTypes; }

    /**
     * Find object type by it's ID. State-view object types are used. Object is searched using main
     * object type ID first, then by dependent object type ids.
     */
    Q_INVOKABLE nx::vms::client::core::analytics::taxonomy::ObjectType*
        objectTypeById(const QString& objectTypeId) const;

private:
    std::vector<ObjectType*> m_objectTypes;
};

} // namespace nx::vms::client::core::analytics::taxonomy
