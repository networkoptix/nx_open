// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/rules/action_builder_field.h>
#include <nx/vms/rules/event_filter_field.h>

namespace nx::vms::rules::test {

/** This field class may be used as list of supported property types. */
class TestEventField: public nx::vms::rules::EventFilterField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.field.test")

    Q_PROPERTY(nx::Uuid id MEMBER id)
    Q_PROPERTY(UuidSet idSet MEMBER idSet)
    Q_PROPERTY(QString string MEMBER string)
    Q_PROPERTY(QStringList strings MEMBER strings)
    Q_PROPERTY(bool flag MEMBER flag)
    Q_PROPERTY(int number MEMBER number)
    Q_PROPERTY(nx::vms::api::rules::State state MEMBER state)
    Q_PROPERTY(nx::vms::api::EventLevels levels MEMBER levels)

public:
    using vms::rules::EventFilterField::EventFilterField;

    virtual bool match(const QVariant&) const override { return false; };
    static QJsonObject openApiDescriptor(const QVariantMap&) { return {}; }

public:
    nx::Uuid id;
    UuidSet idSet;

    QString string;
    QStringList strings;

    bool flag = false;
    int number = 0;

    State state = State::none;
    nx::vms::api::EventLevels levels = {};
};

class TestActionField: public nx::vms::rules::ActionBuilderField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.field.test")

public:
    using vms::rules::ActionBuilderField::ActionBuilderField;

    virtual QVariant build(const AggregatedEventPtr&) const override { return {}; }
    static QJsonObject openApiDescriptor(const QVariantMap& properties) { return {}; }
};

} // namespace nx::vms::rules::test
