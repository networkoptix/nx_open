#pragma once

#include <QObject>
#include <QTimer>

#include <nx/utils/uuid.h>

#include "basic_action.h"
#include "basic_event.h" // EventPtr

namespace nx::vms::rules {

class ActionField;
class BasicAction;

class NX_VMS_RULES_API /*FieldBased*/ActionBuilder: public QObject
{
    Q_OBJECT

public:
    using ActionConstructor = std::function<BasicAction*()>;

    ActionBuilder(const QnUuid& id, const ActionConstructor& ctor);
    virtual ~ActionBuilder();

    bool addField(const QString& name, ActionField* field);

    void process(const EventPtr& event);

    QnUuid id() const;

    void setAggregationInterval(std::chrono::seconds interval);
    std::chrono::seconds aggregationInterval() const;

    void connectSignals();

signals:
    void action(const ActionPtr& action);
    void stateChanged();

private:
    void onTimeout();
    void updateState();

private:
    QnUuid m_id;
    ActionConstructor m_constructor;
    QHash<QString, ActionField*> m_fields;
    std::chrono::seconds m_interval = std::chrono::seconds(0);
    QTimer m_timer;
    bool m_updateInProgress = false;
};

} // namespace nx::vms::rules