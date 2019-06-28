#pragma once

#include <QObject>
#include <QTimer>

#include <nx/utils/uuid.h>

#include "basic_action.h"
#include "basic_event.h" // EventPtr

namespace nx::vms::rules {

class ActionField;

class NX_VMS_RULES_API /*FieldBased*/ActionBuilder: public QObject
{
    Q_OBJECT

public:
    ActionBuilder(const QnUuid& id);
    virtual ~ActionBuilder();

    QnUuid id() const;

    void process(const EventPtr& event);

    void setAggregationInterval(std::chrono::seconds interval);
    std::chrono::seconds aggregationInterval() const;

signals:
    void action(const ActionPtr& action);

private:
    void onTimeout();

private:
    QnUuid m_id;
    QHash<QString, ActionField*> m_fields;
    std::chrono::seconds m_interval = std::chrono::seconds(0);
    QTimer m_timer;
};

} // namespace nx::vms::rules