// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/abstract_attribute.h>
#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics::taxonomy {

class Attribute: public AbstractAttribute
{
public:
    /** Boolean, string and number attribute constructor. */
    Attribute(
        nx::vms::api::analytics::AttributeDescription attributeDescription,
        QObject* parent = nullptr);

    Attribute(
        nx::vms::api::analytics::AttributeDescription attributeDescription,
        ObjectType* objectType,
        QObject* parent = nullptr);

    Attribute(
        nx::vms::api::analytics::AttributeDescription attributeDescription,
        AbstractEnumType* enumType,
        QObject* parent = nullptr);

    Attribute(
        nx::vms::api::analytics::AttributeDescription attributeDescription,
        AbstractColorType* colorType,
        QObject* parent = nullptr);

    virtual const std::string& name() const override;

    virtual Type type() const override;

    virtual const std::string& subtype() const override;

    virtual AbstractEnumType* enumType() const override;

    virtual ObjectType* objectType() const override;

    virtual AbstractColorType* colorType() const override;

    virtual const std::string& unit() const override;

    virtual QVariant minValue() const override;

    virtual QVariant maxValue() const override;

    virtual bool isSupported(nx::Uuid engineId, nx::Uuid deviceId) const override;

    virtual const std::string& condition() const override;

    void setBaseAttribute(AbstractAttribute* attribute);

private:
    nx::vms::api::analytics::AttributeDescription m_attributeDescription;

    AbstractEnumType* m_enumType = nullptr;
    ObjectType* m_objectType = nullptr;
    AbstractColorType* m_colorType = nullptr;

    AbstractAttribute* m_base = nullptr;
};

} // namespace nx::analytics::taxonomy
