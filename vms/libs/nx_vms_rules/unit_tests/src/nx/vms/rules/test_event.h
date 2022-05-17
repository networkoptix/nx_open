// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_event.h>
#include <nx/vms/rules/utils/type.h>

namespace nx::vms::rules::test {

class TestEvent: public nx::vms::rules::BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.test")

    Q_PROPERTY(int intField MEMBER m_intField)
    Q_PROPERTY(QString stringField MEMBER m_stringField)
    Q_PROPERTY(bool boolField MEMBER m_boolField)
    Q_PROPERTY(double floatField MEMBER m_floatField)

public:
    static ItemDescriptor manifest()
    {
        return ItemDescriptor{
            .id = utils::type<TestEvent>(),
            .displayName = "Test event",
            .flags = {ItemFlag::instant, ItemFlag::prolonged},
        };
    }

private:
    int m_intField{};
    QString m_stringField;
    bool m_boolField{};
    double m_floatField{};
};

} // namespace nx::vms::rules::test
