// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/thread/mutex.h>
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
    Q_CLASSINFO("type", "textLookup")

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(TextLookupCheckType checkType READ checkType WRITE setCheckType NOTIFY checkTypeChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)

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

    bool isEnabled() const;
    void setEnabled(bool enabled);

    bool match(const QVariant& eventValue) const override;
    static QJsonObject openApiDescriptor(const QVariantMap& properties);

signals:
    void valueChanged();
    void checkTypeChanged();
    void enabledChanged();

private:
    struct Cache;

    QString m_value;
    TextLookupCheckType m_checkType{TextLookupCheckType::containsKeywords};
    bool m_enabled{false};

    mutable nx::Mutex m_mutex;
    mutable std::shared_ptr<Cache> m_cache;

private:
    std::shared_ptr<Cache> prepareCache() const;
    void invalidateCache();
};

} // namespace nx::vms::rules
