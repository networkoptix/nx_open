// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <any>

#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/common/system_context_aware.h>

#include "../event_filter_field.h"
#include "../field_types.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API ObjectLookupField:
    public EventFilterField,
    public common::SystemContextAware
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.objectLookup")

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(ObjectLookupCheckType checkType READ checkType WRITE setCheckType NOTIFY checkTypeChanged)

public:
    ObjectLookupField(common::SystemContext* context, const FieldDescriptor* descriptor);

    /*
     * Check type dependent value. If the check type is linked with attributes - atributes string,
     * lookup list id otherwise.
     */
    QString value() const;
    void setValue(const QString& value);

    ObjectLookupCheckType checkType() const;
    void setCheckType(ObjectLookupCheckType type);

    bool match(const QVariant& eventValue) const override;
    static QJsonObject openApiDescriptor(const QVariantMap& properties);

signals:
    void valueChanged();
    void checkTypeChanged();

private:
    QString m_value;
    ObjectLookupCheckType m_checkType{ObjectLookupCheckType::hasAttributes};
    mutable std::any m_listOrMatcher;
};

} // namespace nx::vms::rules
