// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "attribute.h"

#include <nx/vms/client/desktop/analytics/taxonomy/color_set.h>
#include <nx/vms/client/desktop/analytics/taxonomy/enumeration.h>
#include <nx/vms/client/desktop/analytics/taxonomy/node.h>

namespace nx::vms::client::desktop::analytics::taxonomy {

struct Attribute::Private
{
    QString name;
    Attribute::Type type = Attribute::Type::undefined;
    QString subtype;
    AbstractEnumeration* enumeration = nullptr;
    AbstractColorSet* colorSet = nullptr;
    AbstractAttributeSet* attributeSet = nullptr;
    QString unit;
    QVariant minValue;
    QVariant maxValue;
    bool isReferencedInCondition = false;
};

Attribute::Attribute(QObject* parent):
    AbstractAttribute(parent),
    d(new Private())
{
}

Attribute::~Attribute()
{
}

QString Attribute::name() const
{
    return d->name;
}

AbstractAttribute::Type Attribute::type() const
{
    return d->type;
}

QString Attribute::subtype() const
{
    return d->subtype;
}

AbstractEnumeration* Attribute::enumeration() const
{
    return d->enumeration;
}

AbstractAttributeSet* Attribute::attributeSet() const
{
    return d->attributeSet;
}

AbstractColorSet* Attribute::colorSet() const
{
    return d->colorSet;
}

QString Attribute::unit() const
{
    return d->unit;
}

QVariant Attribute::minValue() const
{
    return d->minValue;
}

QVariant Attribute::maxValue() const
{
    return d->maxValue;
}

bool Attribute::isReferencedInCondition() const
{
    return d->isReferencedInCondition;
}

void Attribute::setName(QString name)
{
    d->name = name;
}

void Attribute::setType(Type type)
{
    d->type = type;
}

void Attribute::setSubtype(QString subtype)
{
    d->subtype = subtype;
}

void Attribute::setEnumeration(AbstractEnumeration* enumeration)
{
    d->enumeration = enumeration;
}

void Attribute::setColorSet(AbstractColorSet* colorSet)
{
    d->colorSet = colorSet;
}

void Attribute::setAttributeSet(AbstractAttributeSet* attributeSet)
{
    d->attributeSet = attributeSet;
}

void Attribute::setUnit(QString unit)
{
    d->unit = unit;
}

void Attribute::setMinValue(QVariant minValue)
{
    d->minValue = minValue;
}

void Attribute::setMaxValue(QVariant maxValue)
{
    d->maxValue = maxValue;
}

void Attribute::setReferencedInCondition(bool value)
{
    d->isReferencedInCondition = value;
}

} // namespace nx::vms::client::desktop::analytics::taxonomy
