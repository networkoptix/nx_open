// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../event_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API CustomizableText: public EventField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.customizableText")

    Q_PROPERTY(QString text READ text WRITE setText)

public:
    CustomizableText();

    virtual bool match(const QVariant& value) const override;

    QString text() const;
    void setText(const QString& text);

private:
    QString m_text;
};

} // namespace nx::vms::rules
