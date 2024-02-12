// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QVariant>

#include <nx/analytics/taxonomy/abstract_color_type.h>
#include <nx/analytics/taxonomy/abstract_enum_type.h>

Q_MOC_INCLUDE("nx/analytics/taxonomy/abstract_object_type.h")

namespace nx::analytics::taxonomy {

class AbstractObjectType;

class NX_VMS_COMMON_API AbstractAttribute: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(Type type READ type CONSTANT)
    Q_PROPERTY(QString subtype READ subtype CONSTANT)
    Q_PROPERTY(nx::analytics::taxonomy::AbstractEnumType* enumType READ enumType CONSTANT)
    Q_PROPERTY(nx::analytics::taxonomy::AbstractObjectType* objectType READ objectType CONSTANT)
    Q_PROPERTY(nx::analytics::taxonomy::AbstractColorType* colorType READ colorType CONSTANT)
    Q_PROPERTY(QString unit READ unit CONSTANT)
    Q_PROPERTY(QVariant minValue READ minValue CONSTANT)
    Q_PROPERTY(QVariant maxValue READ maxValue CONSTANT)

public:
    enum class Type
    {
        undefined,
        number,
        boolean,
        string,
        color,
        enumeration,
        object,
    };
    Q_ENUM(Type)

public:
    AbstractAttribute(QObject* parent = nullptr):
        QObject(parent)
    {
    }

    virtual ~AbstractAttribute() {}

    virtual QString name() const = 0;

    virtual Type type() const = 0;

    virtual QString subtype() const = 0;

    virtual AbstractEnumType* enumType() const = 0;

    virtual AbstractObjectType* objectType() const = 0;

    virtual AbstractColorType* colorType() const = 0;

    virtual QString unit() const = 0;

    virtual QVariant minValue() const = 0;

    virtual QVariant maxValue() const = 0;

    virtual QString condition() const = 0;

    Q_INVOKABLE virtual bool isSupported(nx::Uuid engineId, nx::Uuid deviceId) const = 0;
};

} // namespace nx::analytics::taxonomy
