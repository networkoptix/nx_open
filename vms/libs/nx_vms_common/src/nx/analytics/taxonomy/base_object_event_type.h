// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/abstract_resource_support_proxy.h>
#include <nx/analytics/taxonomy/attribute.h>
#include <nx/analytics/taxonomy/attribute_resolver.h>
#include <nx/analytics/taxonomy/color_type.h>
#include <nx/analytics/taxonomy/engine.h>
#include <nx/analytics/taxonomy/entity_type.h>
#include <nx/analytics/taxonomy/enum_type.h>
#include <nx/analytics/taxonomy/error_handler.h>
#include <nx/analytics/taxonomy/group.h>
#include <nx/analytics/taxonomy/scope.h>
#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics::taxonomy {

class NX_VMS_COMMON_API AbstractObjectEventType: public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString iconSource READ icon CONSTANT)
    Q_PROPERTY(std::vector<nx::analytics::taxonomy::AbstractAttribute*> attributes
        READ attributes CONSTANT)
    Q_PROPERTY(std::vector<nx::analytics::taxonomy::AbstractAttribute*> supportedAttributes
        READ supportedAttributes CONSTANT)
    Q_PROPERTY(std::vector<nx::analytics::taxonomy::AbstractAttribute*> ownAttributes
        READ ownAttributes CONSTANT)
    Q_PROPERTY(std::vector<nx::analytics::taxonomy::AbstractAttribute*> supportedOwnAttributes
        READ supportedOwnAttributes CONSTANT)
    Q_PROPERTY(bool hasEverBeenSupported READ hasEverBeenSupported CONSTANT)
    Q_PROPERTY(bool isReachable READ isReachable CONSTANT)

public:

    AbstractObjectEventType(QObject* parent = nullptr): QObject(parent) {}

    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString icon() const = 0;
    virtual std::vector<AbstractAttribute*> baseAttributes() const = 0;
    virtual std::vector<AbstractAttribute*> ownAttributes() const = 0;
    virtual std::vector<AbstractAttribute*> attributes() const = 0;
    virtual std::vector<AbstractAttribute*> supportedAttributes() const = 0;
    virtual std::vector<AbstractAttribute*> supportedOwnAttributes() const = 0;
    virtual bool hasEverBeenSupported() const = 0;
    virtual bool isReachable() const = 0;
};

template <typename DerivedType, typename DescriptorType>
class NX_VMS_COMMON_API BaseObjectEventType: public AbstractObjectEventType
{
public:
    BaseObjectEventType(
        EntityType entityType,
        DescriptorType& descriptor,
        QString typeName,
        AbstractResourceSupportProxy* resourceSupportProxy,
        QObject* parent = nullptr)
        :
        AbstractObjectEventType(parent),
            m_entityType(entityType),
            m_descriptor(descriptor),
            m_typeName(std::move(typeName)),
            m_resourceSupportProxy(resourceSupportProxy)
        {
        }

    virtual QString id() const override;
    virtual QString name() const override;
    virtual QString icon() const override;
    virtual std::vector<AbstractAttribute*> baseAttributes() const override;
    virtual std::vector<AbstractAttribute*> ownAttributes() const override;
    virtual std::vector<AbstractAttribute*> attributes() const override;
    virtual std::vector<AbstractAttribute*> supportedAttributes() const override;
    virtual std::vector<AbstractAttribute*> supportedOwnAttributes() const override;
    virtual bool hasEverBeenSupported() const override;
    virtual bool isReachable() const override;

    virtual DerivedType* base() const;
    virtual std::vector<DerivedType*> derivedTypes() const;
    virtual bool isSupported(nx::Uuid engineId, nx::Uuid deviceId) const;
    virtual const std::vector<Scope>& scopes() const;
    virtual void addDerivedType(DerivedType* derivedType);
    virtual bool isLeaf() const;
    virtual void resolveReachability();
    virtual void resolveReachability(bool hasPublicDescendants);
    virtual void resolveScopes(InternalState* inOutInternalState, ErrorHandler* errorHandler);
    virtual void resolve(InternalState* inOutInternalState, ErrorHandler* errorHandler);
    virtual void resolveSupportedAttributes(InternalState* inOutInternalState, ErrorHandler* errorHandler);

    virtual DescriptorType serialize() const;
    const DescriptorType& descriptor() const;

protected:
    const EntityType m_entityType;
    DescriptorType m_descriptor;

    DerivedType* m_base = nullptr;
    std::vector<DerivedType*> m_derivedTypes;

    struct AttributeContext
    {
        AbstractAttribute* attribute;
        bool isOwn = false;
    };

    std::vector<AttributeContext> m_attributes;
    std::vector<AbstractAttribute*> m_ownAttributes;
    std::vector<Scope> m_scopes;

    QString m_typeName;
    bool m_isReachable = false;
    bool m_isResolved = false;
    bool m_areSupportedAttributesResolved = false;

    AbstractResourceSupportProxy* m_resourceSupportProxy;
};

} // namespace nx::analytics::taxonomy
