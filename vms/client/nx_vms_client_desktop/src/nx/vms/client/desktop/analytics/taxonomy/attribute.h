// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/analytics/taxonomy/abstract_attribute.h>

namespace nx::vms::client::desktop::analytics::taxonomy {

class Attribute: public AbstractAttribute
{
public:
    Attribute(QObject* parent);

    virtual ~Attribute() override;

    virtual QString name() const override;

    virtual Type type() const override;

    virtual QString subtype() const override;

    virtual AbstractEnumeration* enumeration() const override;

    virtual AbstractAttributeSet* attributeSet() const override;

    virtual AbstractColorSet* colorSet() const override;

    virtual QString unit() const override;

    virtual QVariant minValue() const override;

    virtual QVariant maxValue() const override;

    virtual bool isReferencedInCondition() const override;

    void setName(QString name);

    void setType(Type type);

    void setSubtype(QString subtype);

    void setEnumeration(AbstractEnumeration* enumeration);

    void setColorSet(AbstractColorSet* colorSet);

    void setAttributeSet(AbstractAttributeSet* attributeSet);

    void setUnit(QString unit);

    void setMinValue(QVariant minValue);

    void setMaxValue(QVariant maxValue);

    void setReferencedInCondition(bool value);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::analytics::taxonomy
