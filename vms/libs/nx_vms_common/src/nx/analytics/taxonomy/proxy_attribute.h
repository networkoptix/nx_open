// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/abstract_attribute.h>
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
        AttributeSupportInfoTree attributeSupportInfoTree);

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

    virtual bool isSupported(QnUuid engineId, QnUuid deviceId) const override;

private:
    AbstractAttribute* m_proxiedAttribute = nullptr;
    AbstractObjectType* m_proxyObjectType = nullptr;
    std::map<QnUuid, std::set<QnUuid>> m_ownSupportInfo;
};

} // namespace nx::analytics::taxonomy
