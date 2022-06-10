// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>

#include "../action_field.h"
#include "../event_field.h"

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

    QSet<QnUuid> ids() const { return m_ids; }
    void setIds(const QSet<QnUuid>& ids)
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
    QSet<QnUuid> m_ids;
};

class NX_VMS_RULES_API ResourceFilterEventField:
    public EventField,
    public ResourceFilterFieldBase<ResourceFilterEventField>
{
    Q_OBJECT

    Q_PROPERTY(bool acceptAll READ acceptAll WRITE setAcceptAll NOTIFY acceptAllChanged)
    Q_PROPERTY(QnUuidSet ids READ ids WRITE setIds NOTIFY idsChanged)

signals:
    void acceptAllChanged();
    void idsChanged();

public:
    virtual bool match(const QVariant& value) const override;
};

class NX_VMS_RULES_API ResourceFilterActionField:
    public ActionField,
    public ResourceFilterFieldBase<ResourceFilterActionField>
{
    Q_OBJECT

    Q_PROPERTY(bool acceptAll READ acceptAll WRITE setAcceptAll NOTIFY acceptAllChanged)
    Q_PROPERTY(QnUuidSet ids READ ids WRITE setIds NOTIFY idsChanged)

signals:
    void acceptAllChanged();
    void idsChanged();

public:
    virtual QVariant build(const EventPtr& eventData) const override;
};

} // namespace nx::vms::rules
