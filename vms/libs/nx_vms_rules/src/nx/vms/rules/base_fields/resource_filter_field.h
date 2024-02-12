// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>

#include "../action_builder_field.h"
#include "../event_filter_field.h"
#include "../field_types.h"

namespace nx::vms::rules {

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
    virtual QVariant build(const AggregatedEventPtr& eventAggregator) const override;

    void setSelection(const UuidSelection& selection);
};

} // namespace nx::vms::rules
