// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <vector>

namespace nx::vms::client::desktop::analytics::taxonomy {

class AbstractAttribute;

/**
 * Special attribute type that contains other attributes. Corresponds Object attribute in the
 * analytics taxonomy.
 */
class AbstractAttributeSet: public QObject
{
    Q_OBJECT
    Q_PROPERTY(std::vector<nx::vms::client::desktop::analytics::taxonomy::AbstractAttribute*>
        attributes READ attributes CONSTANT)

public:
    AbstractAttributeSet(QObject* parent): QObject(parent) {}

    virtual ~AbstractAttributeSet() {}

    virtual std::vector<AbstractAttribute*> attributes() const = 0;
};

} // namespace nx::vms::client::desktop::analytics::taxonomy
