#pragma once

#include <QtCore/QObject>

#include <nx/vms/event/event_fwd.h>

namespace nx {
namespace client {
namespace mobile {

// Tries to manually gather event rules for mobile client from old servers
class EventRulesWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    EventRulesWatcher(QObject* parent = nullptr);

    void handleConnectionChanged();

private:
    void handleRulesReset(const nx::vms::event::RuleList& rules);

private:
    bool m_updated = false;
};

} // namespace mobile
} // namespace client
} // namespace mobile
