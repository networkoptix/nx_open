// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <vector>

#include <QString>

#include "abstract_attribute.h"

namespace nx::vms::client::desktop::analytics::taxonomy {

/**
 * Represents a taxonomy entity (Object or Event type) with attributes. Nodes of the state view
 * are not necessary in "one-to-one" relationship with the corresponding Object or Event type.
 * A node can be built from multiple Object types (e.g. some Object type and its hidden derived
 * descendants), and potentially be a virtual entity that has no direct match in the analytics
 * taxonomy.
 */
class AbstractNode: public QObject
{
    Q_OBJECT
    Q_PROPERTY(std::vector<QString> typeIds READ typeIds CONSTANT)
    Q_PROPERTY(std::vector<QString> fullSubtreeTypeIds READ fullSubtreeTypeIds CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString iconSource READ icon CONSTANT)
    Q_PROPERTY(std::vector<nx::vms::client::desktop::analytics::taxonomy::AbstractAttribute*>
        attributes READ attributes CONSTANT)
    Q_PROPERTY(std::vector<nx::vms::client::desktop::analytics::taxonomy::AbstractNode*>
        derivedNodes READ derivedNodes CONSTANT)

public:
    AbstractNode(QObject* parent): QObject(parent) {}

    virtual ~AbstractNode() {}

    /** Ids of Object or Event types from which the node is built. */
    virtual std::vector<QString> typeIds() const = 0;

    /**
     * Ids of Object or Event types from which the node is built and ids of all its explicit and
     * hidden descendants that passes the filter which was used to build the state view.
     */
    virtual std::vector<QString> fullSubtreeTypeIds() const = 0;

    virtual QString name() const = 0;

    virtual QString icon() const = 0;

    virtual std::vector<AbstractAttribute*> attributes() const = 0;

    virtual std::vector<AbstractNode*> derivedNodes() const = 0;
};

} // namespace nx::vms::client::desktop::analytics::taxonomy

Q_DECLARE_METATYPE(std::vector<nx::vms::client::desktop::analytics::taxonomy::AbstractNode*>)
Q_DECLARE_METATYPE(std::vector<nx::vms::client::desktop::analytics::taxonomy::AbstractAttribute*>)
