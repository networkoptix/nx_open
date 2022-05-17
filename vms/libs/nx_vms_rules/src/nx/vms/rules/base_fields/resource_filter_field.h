// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>

#include "../action_field.h"
#include "../event_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API ResourceFilterFieldBase
{
public:
    bool acceptAll() const;
    void setAcceptAll(bool anyCamera);

    QSet<QnUuid> ids() const;
    void setIds(const QSet<QnUuid>& ids);

protected:
    // This field type should be used as base class only.
    ResourceFilterFieldBase() = default;

private:
    bool m_acceptAll = false;
    QSet<QnUuid> m_ids;
};

class NX_VMS_RULES_API ResourceFilterEventField: public EventField, public ResourceFilterFieldBase
{
    Q_OBJECT

    Q_PROPERTY(bool acceptAll READ acceptAll WRITE setAcceptAll)
    Q_PROPERTY(QSet<QnUuid> ids READ ids WRITE setIds)

public:
    virtual bool match(const QVariant& value) const override;
};

class NX_VMS_RULES_API ResourceFilterActionField:
    public ActionField,
    public ResourceFilterFieldBase
{
    Q_OBJECT

    Q_PROPERTY(bool acceptAll READ acceptAll WRITE setAcceptAll)
    Q_PROPERTY(QSet<QnUuid> ids READ ids WRITE setIds)

public:
    virtual QVariant build(const EventPtr& eventData) const override;
};

} // namespace nx::vms::rules
