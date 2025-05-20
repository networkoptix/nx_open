// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/abstract_resource_support_proxy.h>
#include <nx/analytics/taxonomy/object_type.h>
#include <nx/analytics/taxonomy/utils.h>

namespace nx::analytics::taxonomy {

class ProxyObjectType: public ObjectType
{

public:
    ProxyObjectType(
        ObjectType* proxiedObjectType,
        std::map<QString, AttributeSupportInfoTree> attributeSupportInfoTree,
        AbstractResourceSupportProxy* resourceSupportProxy,
        QString prefix,
        QString rootParentTypeId,
        EntityType rootEntityType);

    virtual QString id() const override;

    virtual QString name() const override;

    virtual QString icon() const override;

    virtual ObjectType* base() const override;

    virtual std::vector<ObjectType*> derivedTypes() const override;

    virtual std::vector<AbstractAttribute*> baseAttributes() const override;

    virtual std::vector<AbstractAttribute*> ownAttributes() const override;

    virtual std::vector<AbstractAttribute*> supportedAttributes() const override;

    virtual std::vector<AbstractAttribute*> supportedOwnAttributes() const override;

    virtual std::vector<AbstractAttribute*> attributes() const override;

    virtual bool hasEverBeenSupported() const override;

    virtual bool isSupported(nx::Uuid engineId, nx::Uuid deviceId) const override;

    virtual bool isReachable() const override;

    virtual bool isLiveOnly() const override;

    virtual bool isNonIndexable() const override;

    virtual const std::vector<Scope>& scopes() const override;

    virtual nx::vms::api::analytics::ObjectTypeDescriptor serialize() const override;

private:
    ObjectType* m_proxiedObjectType = nullptr;
    mutable std::map<QString, AttributeSupportInfoTree> m_attributeSupportInfoTree;
    mutable std::optional<std::vector<AbstractAttribute*>> m_supportedAttributes;
    mutable std::optional<std::vector<AbstractAttribute*>> m_supportedOwnAttributes;
    AbstractResourceSupportProxy* m_resourceSupportProxy;
    QString m_prefix;
    QString m_rootParentTypeId;
    EntityType m_rootEntityType;
};

} // namespace nx::analytics::taxonomy
