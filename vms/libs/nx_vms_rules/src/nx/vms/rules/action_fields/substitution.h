// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../action_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API Substitution: public ActionField
{
    Q_OBJECT

    Q_CLASSINFO("metatype", "nx.substitution")

    Q_PROPERTY(QString fieldName MEMBER m_eventFieldName)

public:
    Substitution();

    // QString metatype() const final override { return "nx.substitution"; }

    virtual QVariant build(const EventData& eventData) const override;

private:
    QString m_eventFieldName;
};

} // namespace nx::vms::rules
