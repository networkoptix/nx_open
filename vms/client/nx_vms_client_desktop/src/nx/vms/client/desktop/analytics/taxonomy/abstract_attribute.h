// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>

#include "abstract_color_set.h"
#include "abstract_enumeration.h"
#include "abstract_attribute_set.h"

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::client::desktop::analytics::taxonomy {

class AbstractNode;

/**
 * Attribute of a state view node. The differnce between attribute of an Object or Event type and
 * state view node attribute is that the latter can be a result of combination of attributes with
 * the same name of multiple taxonomy entities (e.g. union of colors for Color types and union of
 * enum items for Enum types).
 */
class NX_VMS_CLIENT_DESKTOP_API AbstractAttribute: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(Type type READ type CONSTANT)
    Q_PROPERTY(QString subtype READ subtype CONSTANT)
    Q_PROPERTY(nx::vms::client::desktop::analytics::taxonomy::AbstractEnumeration*
        enumeration READ enumeration CONSTANT)
    Q_PROPERTY(nx::vms::client::desktop::analytics::taxonomy::AbstractAttributeSet*
        attributeSet READ attributeSet CONSTANT)
    Q_PROPERTY(nx::vms::client::desktop::analytics::taxonomy::AbstractColorSet*
        colorSet READ colorSet CONSTANT)
    Q_PROPERTY(QString unit READ unit CONSTANT)
    Q_PROPERTY(QVariant minValue READ minValue CONSTANT)
    Q_PROPERTY(QVariant maxValue READ maxValue CONSTANT)

public:
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Type,
        undefined,
        number,
        boolean,
        string,
        colorSet,
        enumeration,
        attributeSet)

    Q_ENUM(Type)

public:
    AbstractAttribute(QObject* parent): QObject(parent) {};

    virtual ~AbstractAttribute() {}

    virtual QString name() const = 0;

    virtual Type type() const = 0;

    virtual QString subtype() const = 0;

    virtual AbstractEnumeration* enumeration() const = 0;

    virtual AbstractAttributeSet* attributeSet() const = 0;

    virtual AbstractColorSet* colorSet() const = 0;

    virtual QString unit() const = 0;

    virtual QVariant minValue() const = 0;

    virtual QVariant maxValue() const = 0;
};

} // nx::vms::client::desktop_client::analytics::taxonomy
