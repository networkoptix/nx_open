// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::analytics::taxonomy {

class AbstractObjectType;
class AbstractAttribute;

} // namespace nx::analytics::taxonomy

namespace nx::vms::client::core::analytics::taxonomy {

/**
 * A filter to build a state view. The resulting view is built only from such Object types
 * that passes the filter. Attributes that don't pass the filter are not present in the state view.
 */
class NX_VMS_CLIENT_CORE_API AbstractStateViewFilter: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)

public:
    AbstractStateViewFilter(QObject* parent = nullptr): QObject(parent) {}

    virtual ~AbstractStateViewFilter() {}

    /**
     * @return Id of the filter. For some filters id can hold some taxonomy information (see
     * EngineStateViewFilter for example).
     */
    virtual QString id() const = 0;

    virtual QString name() const = 0;

    /** @return True if the given Object type passes the filter, false otherwise. */
    virtual bool matches(const nx::analytics::taxonomy::AbstractObjectType* objectType) const = 0;

    /** @return True if the given Attribute passes the filter, false otherwise. */
    virtual bool matches(const nx::analytics::taxonomy::AbstractAttribute* attribute) const = 0;
};

} // namespace nx::vms::client::core::analytics::taxonomy
