// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>

#include <nx/reflect/enum_instrument.h>

Q_MOC_INCLUDE("nx/vms/client/core/analytics/taxonomy/attribute_set.h")
Q_MOC_INCLUDE("nx/vms/client/core/analytics/taxonomy/color_set.h")
Q_MOC_INCLUDE("nx/vms/client/core/analytics/taxonomy/enumeration.h")

namespace nx::vms::client::core::analytics::taxonomy {

class AttributeSet;
class Enumeration;
class ColorSet;

/**
 * Attribute of a state view Object Type. It can be a result of combination of attributes with the
 * same name of multiple taxonomy entities (e.g. union of colors for Color types and union of enum
 * items for Enum types).
 */
class NX_VMS_CLIENT_CORE_API Attribute: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name MEMBER name CONSTANT)
    Q_PROPERTY(Type type MEMBER type CONSTANT)
    Q_PROPERTY(QString subtype MEMBER subtype CONSTANT)
    Q_PROPERTY(nx::vms::client::core::analytics::taxonomy::Enumeration*
        enumeration MEMBER enumeration CONSTANT)
    Q_PROPERTY(nx::vms::client::core::analytics::taxonomy::AttributeSet*
        attributeSet MEMBER attributeSet CONSTANT)
    Q_PROPERTY(nx::vms::client::core::analytics::taxonomy::ColorSet*
        colorSet MEMBER colorSet CONSTANT)
    Q_PROPERTY(QString unit MEMBER unit CONSTANT)
    Q_PROPERTY(QVariant minValue MEMBER minValue CONSTANT)
    Q_PROPERTY(QVariant maxValue MEMBER maxValue CONSTANT)
    Q_PROPERTY(bool isReferencedInCondition MEMBER isReferencedInCondition CONSTANT)

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

    using QObject::QObject;

    QString name;
    Type type = Type::undefined;
    QString subtype;
    Enumeration* enumeration = nullptr;
    ColorSet* colorSet = nullptr;
    AttributeSet* attributeSet = nullptr;
    QString unit;
    QVariant minValue;
    QVariant maxValue;
    bool isReferencedInCondition = false;
};

} // namespace nx::vms::client::core::analytics::taxonomy
