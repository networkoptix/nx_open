// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/common/system_context_aware.h>

#include "../event_filter_field.h"
#include "../field_types.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API TextLookupField:
    public EventFilterField,
    public common::SystemContextAware
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.textLookup")

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(TextLookupCheckType checkType READ checkType WRITE setCheckType NOTIFY checkTypeChanged)

public:
    TextLookupField(common::SystemContext* context, const FieldDescriptor* descriptor);

    /*
     * Check type dependent value. If the check type is linked with keywords - space separated string,
     * lookup list id otherwise.
     */
    QString value() const;
    void setValue(const QString& value);

    TextLookupCheckType checkType() const;
    void setCheckType(TextLookupCheckType type);

    bool match(const QVariant& eventValue) const override;
    static QJsonObject openApiDescriptor(const QVariantMap& properties);

signals:
    void valueChanged();
    void checkTypeChanged();

private:
    QString m_value;
    TextLookupCheckType m_checkType{TextLookupCheckType::containsKeywords};
    mutable std::optional<QStringList> m_list;
};

} // namespace nx::vms::rules
