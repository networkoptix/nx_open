// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/action_field.h>
#include <nx/vms/rules/event_field.h>

namespace nx::vms::rules::test {

/** This field class may be used as list of supported property types. */
class TestEventField: public nx::vms::rules::EventField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.field.test")

    Q_PROPERTY(QnUuid id MEMBER id)
    Q_PROPERTY(QnUuidList ids MEMBER ids)
    Q_PROPERTY(QString string MEMBER string)
    Q_PROPERTY(QStringList strings MEMBER strings)
    Q_PROPERTY(bool flag MEMBER flag)

public:
    virtual bool match(const QVariant&) const override { return false; };

public:
    QnUuid id;
    QnUuidList ids;

    QString string;
    QStringList strings;

    bool flag = false;
};

class TestActionField: public nx::vms::rules::ActionField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.field.test")

public:
    virtual QVariant build(const EventData& eventData) const override { return {}; }
};

} // namespace nx::vms::rules::test
