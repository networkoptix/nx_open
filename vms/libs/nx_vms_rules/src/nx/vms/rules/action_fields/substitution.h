// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../action_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API Substitution: public ActionField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.substitution")

    Q_PROPERTY(QString fieldName MEMBER m_eventFieldName)

public:
    Substitution();

    virtual QVariant build(const AggregatedEvent& aggregatedEvent) const override;

private:
    QString m_eventFieldName;
};

} // namespace nx::vms::rules
