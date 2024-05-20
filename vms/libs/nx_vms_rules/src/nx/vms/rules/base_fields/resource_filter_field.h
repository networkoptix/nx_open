// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>

#include "../action_builder_field.h"
#include "../event_filter_field.h"
#include "../field_types.h"

namespace nx::vms::rules {

struct ResourceFilterFieldProperties
{
    /** Whether given field should be visible in the editor. */
    bool visible{true};

    /** Value for the the just created field's `acceptAll` property. */
    bool acceptAll{false};

    /** Value for the the just created field's `ids` property. */
    UuidSet ids;

    bool allowEmptySelection{false};
    QString validationPolicy;

    QVariantMap toVariantMap() const
    {
        QVariantMap result;

        result.insert("acceptAll", acceptAll);
        if (!ids.empty())
            result.insert("ids", QVariant::fromValue(ids));

        result.insert("visible", visible);
        result.insert("allowEmptySelection", allowEmptySelection);
        result.insert("validationPolicy", validationPolicy);

        return result;
    }

    static ResourceFilterFieldProperties fromVariantMap(const QVariantMap& properties)
    {
        ResourceFilterFieldProperties result;

        result.acceptAll = properties.value("acceptAll").toBool();
        result.ids = properties.value("ids").value<UuidSet>();

        result.visible = properties.value("visible", true).toBool();
        result.allowEmptySelection = properties.value("allowEmptySelection", false).toBool();
        result.validationPolicy = properties.value("validationPolicy").toString();

        return result;
    }
};

template<class T>
class NX_VMS_RULES_API ResourceFilterFieldBase
{
public:
    bool acceptAll() const { return m_acceptAll; }
    void setAcceptAll(bool anyCamera)
    {
        if (m_acceptAll != anyCamera)
        {
            m_acceptAll = anyCamera;
            emit static_cast<T*>(this)->acceptAllChanged();
        }
    };

    QSet<nx::Uuid> ids() const { return m_ids; }
    void setIds(const QSet<nx::Uuid>& ids)
    {
        if (m_ids != ids)
        {
            m_ids = ids;
            emit static_cast<T*>(this)->idsChanged();
        }
    }

    UuidSelection selection() const
    {
        return {m_ids, m_acceptAll};
    }

    ResourceFilterFieldProperties properties() const
    {
        return ResourceFilterFieldProperties::fromVariantMap(
            static_cast<const T*>(this)->descriptor()->properties);
    }

protected:
    // This field type should be used as base class only.
    ResourceFilterFieldBase() = default;

private:
    bool m_acceptAll = false;
    QSet<nx::Uuid> m_ids;
};

class NX_VMS_RULES_API ResourceFilterEventField:
    public EventFilterField,
    public ResourceFilterFieldBase<ResourceFilterEventField>
{
    Q_OBJECT

    Q_PROPERTY(bool acceptAll READ acceptAll WRITE setAcceptAll NOTIFY acceptAllChanged)
    Q_PROPERTY(UuidSet ids READ ids WRITE setIds NOTIFY idsChanged)

signals:
    void acceptAllChanged();
    void idsChanged();

public:
    using EventFilterField::EventFilterField;
    using ResourceFilterFieldBase<ResourceFilterEventField>::properties;

    virtual bool match(const QVariant& value) const override;
};

class NX_VMS_RULES_API ResourceFilterActionField:
    public ActionBuilderField,
    public ResourceFilterFieldBase<ResourceFilterActionField>
{
    Q_OBJECT

    Q_PROPERTY(bool acceptAll READ acceptAll WRITE setAcceptAll NOTIFY acceptAllChanged)
    Q_PROPERTY(UuidSet ids READ ids WRITE setIds NOTIFY idsChanged)

signals:
    void acceptAllChanged();
    void idsChanged();

public:
    using ActionBuilderField::ActionBuilderField;
    using ResourceFilterFieldBase<ResourceFilterActionField>::properties;

    virtual QVariant build(const AggregatedEventPtr& eventAggregator) const override;

    void setSelection(const UuidSelection& selection);
};

} // namespace nx::vms::rules
