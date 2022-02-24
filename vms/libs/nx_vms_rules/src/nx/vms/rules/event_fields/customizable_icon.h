// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../event_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API CustomizableIcon: public EventField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.customizableIcon")

    Q_PROPERTY(QString name READ name WRITE setName)

public:
    CustomizableIcon();

    virtual bool match(const QVariant& value) const override;

    QString name() const;
    void setName(const QString& name);

private:
    QString m_name;
};

} // namespace nx::vms::rules
