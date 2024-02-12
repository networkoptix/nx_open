// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/abstract_attribute.h>
#include <nx/analytics/taxonomy/abstract_resource_support_proxy.h>
#include <nx/analytics/taxonomy/utils.h>

namespace nx::analytics::taxonomy {

/**
 * Attributes sometimes need to be wrapped to Proxy Attributes because under different
 * circumstances they can have different lists of Device Agents that support them. For example,
 * some Object Type will likely have different support lists for its different roles: as a
 * standalone Analytics Object, or as an Attribute of some other Object.
 */
class ProxyAttribute: public AbstractAttribute
{
public:
    ProxyAttribute(
        AbstractAttribute* attribute,
        AttributeSupportInfoTree attributeSupportInfoTree,
        AbstractResourceSupportProxy* resourceSupportProxy,
        QString prefix,
        QString rootParentTypeId,
        EntityType rootEntityType);

    virtual ~ProxyAttribute() override;

    virtual QString name() const override;

    virtual Type type() const override;

    virtual QString subtype() const override;

    virtual AbstractEnumType* enumType() const override;

    virtual AbstractObjectType* objectType() const override;

    virtual AbstractColorType* colorType() const override;

    virtual QString unit() const override;

    virtual QVariant minValue() const override;

    virtual QVariant maxValue() const override;

    virtual bool isSupported(nx::Uuid engineId, nx::Uuid deviceId) const override;

    virtual QString condition() const override;

private:
    AbstractAttribute* m_proxiedAttribute = nullptr;
    AbstractObjectType* m_proxyObjectType = nullptr;
    std::set<nx::Uuid> m_supportByEngine;
    QString m_prefix;
    QString m_rootParentTypeId;
    EntityType m_rootEntityType;
    AbstractResourceSupportProxy* m_resourceSupportProxy = nullptr;
};

} // namespace nx::analytics::taxonomy
