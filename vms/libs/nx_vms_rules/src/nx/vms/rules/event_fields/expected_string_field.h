// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../event_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API ExpectedStringField: public EventField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.expectedString")

    Q_PROPERTY(QString expected READ expected WRITE setExpected)

public:
    virtual bool match(const QVariant& value) const override;

    QString expected() const;
    void setExpected(const QString& value);

private:
    QString m_expected;
};

} // namespace nx::vms::rules
