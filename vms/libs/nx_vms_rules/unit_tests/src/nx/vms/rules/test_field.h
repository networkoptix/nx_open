// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>

#include <QtCore/QThread>

#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/rules/action_builder_field.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter_field.h>
#include <nx/vms/rules/utils/type.h>
#include <utils/common/delayed.h>

namespace nx::vms::rules::test {

/** This field class may be used as list of supported property types. */
class TestEventField: public nx::vms::rules::EventFilterField
{
    Q_OBJECT
    Q_CLASSINFO("type", "test")

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
    Q_CLASSINFO("type", "test")

public:
    using vms::rules::ActionBuilderField::ActionBuilderField;

    virtual QVariant build(const AggregatedEventPtr&) const override { return {}; }
    static QJsonObject openApiDescriptor(const QVariantMap& /*properties*/) { return {}; }
};

/**
 * Waits for a callback delivered in another thread, like the email message field which loads the
 * camera history via QnCameraHistoryPool::updateCameraHistorySync() to build a screenshot.
 */
class CallbackWaitingField: public nx::vms::rules::ActionBuilderField
{
    Q_OBJECT
    Q_CLASSINFO("type", "callbackWaiting")

public:
    /** The real field waits forever, so the timeout means a deadlock. */
    static constexpr std::chrono::seconds kWaitTimeout{5};

    CallbackWaitingField(
        QThread* callbackThread, std::atomic<int>* timeouts, const FieldDescriptor* descriptor):
        ActionBuilderField(descriptor),
        m_callbackThread(callbackThread),
        m_timeouts(timeouts)
    {
    }

    static bool registerIn(Engine* engine, QThread* callbackThread, std::atomic<int>* timeouts)
    {
        return engine->registerActionField(utils::type<CallbackWaitingField>(),
            {std::bind_front(&create, callbackThread, timeouts)});
    }

    virtual QVariant build(const AggregatedEventPtr&) const override
    {
        const auto delivered = std::make_shared<std::promise<void>>();
        auto future = delivered->get_future();
        executeInThread(m_callbackThread, [delivered]() { delivered->set_value(); });

        if (future.wait_for(kWaitTimeout) == std::future_status::timeout)
            ++*m_timeouts;

        return QString("screenshot");
    }

    static QJsonObject openApiDescriptor(const QVariantMap& /*properties*/) { return {}; }

private:
    static ActionBuilderField* create(
        QThread* callbackThread, std::atomic<int>* timeouts, const FieldDescriptor* descriptor)
    {
        return new CallbackWaitingField(callbackThread, timeouts, descriptor);
    }

private:
    QThread* const m_callbackThread;
    std::atomic<int>* const m_timeouts;
};

} // namespace nx::vms::rules::test
