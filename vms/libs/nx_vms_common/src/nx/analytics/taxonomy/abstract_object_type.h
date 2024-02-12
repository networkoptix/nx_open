// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QString>

#include <nx/analytics/taxonomy/abstract_attribute.h>
#include <nx/analytics/taxonomy/abstract_scope.h>

namespace nx::analytics::taxonomy {

class NX_VMS_COMMON_API AbstractObjectType: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString iconSource READ icon CONSTANT)
    Q_PROPERTY(nx::analytics::taxonomy::AbstractObjectType* baseType READ base CONSTANT)
    Q_PROPERTY(std::vector<nx::analytics::taxonomy::AbstractObjectType*> derivedTypes
        READ derivedTypes CONSTANT)
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
    Q_PROPERTY(bool isNonIndexable READ isNonIndexable CONSTANT)
    Q_PROPERTY(bool isLiveOnly READ isLiveOnly CONSTANT)
    Q_PROPERTY(const std::vector<nx::analytics::taxonomy::AbstractScope*>& scopes READ scopes CONSTANT)

public:
    AbstractObjectType(QObject* parent = nullptr):
        QObject(parent)
    {
    }

    virtual ~AbstractObjectType() {}

    virtual QString id() const = 0;

    virtual QString name() const = 0;

    virtual QString icon() const = 0;

    virtual AbstractObjectType* base() const = 0;

    virtual std::vector<AbstractObjectType*> derivedTypes() const = 0;

    /**
     * @return A list of Attributes that have been inherited from the base Type.
     */
    virtual std::vector<AbstractAttribute*> baseAttributes() const = 0;

    /**
     * @return A list of Attributes that are declared by the Type directly, including overridden
     *     base Attributes.
     */
    virtual std::vector<AbstractAttribute*> ownAttributes() const = 0;

    /**
     * @return A list of Attributes that have been declared as supported for the Type by any
     *     Device Agent.
     */
    virtual std::vector<AbstractAttribute*> supportedAttributes() const = 0;

    virtual std::vector<AbstractAttribute*> supportedOwnAttributes() const = 0;

    virtual std::vector<AbstractAttribute*> attributes() const = 0;

    /**
     * @return Whether the Type has ever been declared as supported by any Device Agent.
     */
    virtual bool hasEverBeenSupported() const = 0;

    Q_INVOKABLE virtual bool isSupported(nx::Uuid engineId, nx::Uuid deviceId) const = 0;

    /**
     * @return Whether the Type is not a hidden derived Type and has ever been supported by any
     *     Device Agent or is a direct or indirect base for any supported Type.
     */
    virtual bool isReachable() const = 0;

    virtual bool isNonIndexable() const = 0;

    virtual bool isLiveOnly() const = 0;

    virtual const std::vector<AbstractScope*>& scopes() const = 0;

    virtual nx::vms::api::analytics::ObjectTypeDescriptor serialize() const = 0;
};

} // namespace nx::analytics::taxonomy
