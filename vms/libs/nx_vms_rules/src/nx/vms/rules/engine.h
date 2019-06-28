#pragma once

#include <QList>
#include <QObject>

#include <nx/utils/singleton.h>

#include "basic_event.h"
#include "basic_action.h"

namespace nx::vms::api::rules {
    struct ActionBuilder;
    struct EventFilter;
    struct Field;
    struct Rule;
}

namespace nx::vms::rules {

class EventField;
class ActionField;

class EventConnector;

class ActionBuilder;
class EventFilter;
class Rule;

namespace api = ::nx::vms::api::rules;

class NX_VMS_RULES_API Engine:
    public QObject,
    public Singleton<Engine>
{
    Q_OBJECT

public:
    Engine();

    bool addEventConnector(EventConnector* eventConnector);

    //template<class T>
    //inline bool registerEventField()
    //{
    //    return registerEventField(T::manifest(), T::checker());
    //}

public:
    using EventFieldConstructor = std::function<EventField*(const QnUuid&, const QString&)>;
    using ActionFieldConstructor = std::function<ActionField*(const QnUuid&, const QString&)>;

private: //< ?
    bool registerEventField(
        const QJsonObject& manifest,
        const std::function<bool()>& checker);

    bool registerActionField(
        const QJsonObject& manifest,
        const std::function<QJsonValue()>& reducer);

    bool registerEventType(
        const QJsonObject& manifest,
        const std::function<EventPtr()>& constructor/*, QObject* source*/);

    bool registerActionType(
        const QJsonObject& manifest,
        const std::function<EventPtr()>& constructor,
        QObject* executor);

    Rule* buildRule(const api::Rule& serialized) const;

private:
    EventFilter* buildEventFilter(const api::EventFilter& serialized) const;
    ActionBuilder* buildActionBuilder(const api::ActionBuilder& serialized) const;

    EventField* buildEventField(const api::Field& serialized) const;
    ActionField* buildActionField(const api::Field& serialized) const;

private:
    void processEvent(const EventPtr &event);

private:
    QList<EventConnector*> m_connectors;
    QList<EventConnector*> m_executors;

    QHash<QString, EventFieldConstructor> m_eventFields;
    QHash<QString, ActionFieldConstructor> m_actionFields;

    QHash<QnUuid, void*> m_rules;
};

} // namespace nx::vms::rules