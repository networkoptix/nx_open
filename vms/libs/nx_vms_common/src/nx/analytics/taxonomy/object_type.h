// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/abstract_object_type.h>
#include <nx/analytics/taxonomy/utils.h>
#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics::taxonomy {

struct InternalState;
class ErrorHandler;
class AbstractResourceSupportProxy;

template<typename Descriptor, typename AbstractResolvedType, typename ResolvedType>
class BaseObjectEventTypeImpl;

class ObjectType: public AbstractObjectType
{
public:
    ObjectType(
        nx::vms::api::analytics::ObjectTypeDescriptor objectTypeDescriptor,
        AbstractResourceSupportProxy* resourceSupportProxy,
        QObject* parent = nullptr);

    virtual QString id() const override;

    virtual QString name() const override;

    virtual QString icon() const override;

    virtual AbstractObjectType* base() const override;

    virtual std::vector<AbstractObjectType*> derivedTypes() const override;

    virtual std::vector<AbstractAttribute*> baseAttributes() const override;

    virtual std::vector<AbstractAttribute*> ownAttributes() const override;

    virtual std::vector<AbstractAttribute*> attributes() const override;

    virtual std::vector<AbstractAttribute*> supportedAttributes() const override;

    virtual std::vector<AbstractAttribute*> supportedOwnAttributes() const override;

    virtual bool hasEverBeenSupported() const override;

    virtual bool isSupported(nx::Uuid engineId, nx::Uuid deviceId) const override;

    virtual bool isReachable() const override;

    virtual bool isNonIndexable() const override;

    virtual bool isLiveOnly() const override;

    virtual const std::vector<AbstractScope*>& scopes() const override;

    virtual nx::vms::api::analytics::ObjectTypeDescriptor serialize() const override;

    void addDerivedType(AbstractObjectType* derivedObjectType);

    void resolve(InternalState* inOutInternalState, ErrorHandler* errorHandler);

    void resolveSupportedAttributes(InternalState* inOutInternalState, ErrorHandler* errorHandler);

    void resolveReachability(bool hasPublicDescendants);

    void resolveReachability();

private:
    void resolveScopes(InternalState* inOutInternalState, ErrorHandler* errorHandler);

private:
    std::shared_ptr<BaseObjectEventTypeImpl<
        nx::vms::api::analytics::ObjectTypeDescriptor,
        AbstractObjectType,
        ObjectType>> m_impl;
};

} // namespace nx::analytics::taxonomy
