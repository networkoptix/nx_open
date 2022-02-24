// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <nx/analytics/taxonomy/abstract_attribute.h>
#include <nx/analytics/taxonomy/utils.h>

namespace nx::analytics::taxonomy {

class ProxyObjectTypeAttribute: public AbstractAttribute
{
public:
    ProxyObjectTypeAttribute(
        AbstractAttribute* proxiedAttribute,
        AttributeTree supportedAttributeTree);

    virtual QString name() const override;

    virtual Type type() const override;

    virtual QString subtype() const override;

    virtual AbstractEnumType* enumType() const override;

    virtual AbstractObjectType* objectType() const override;

    virtual AbstractColorType* colorType() const override;

    virtual QString unit() const override;

    virtual QVariant minValue() const override;

    virtual QVariant maxValue() const override;

private:
    AbstractAttribute* m_proxiedAttribute = nullptr;
    AttributeTree m_supportedAttributeTree;

    mutable AbstractObjectType* m_proxyObjectType = nullptr;
};

} // namespace nx::analytics::taxonomy
