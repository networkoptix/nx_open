#pragma once

#include <nx/vms/api/rules/event_info.h>

#include <chrono>

namespace nx::vms::rules {

enum class ActionType
{
    Instant = 0,
    ActAtStart = 1 << 0,
    ActAtEnd = 1 << 1,
    Prolonged = ActAtStart | ActAtEnd
};

class NX_VMS_RULES_API BasicAction: public QObject
{
    Q_OBJECT

public:
    BasicAction();

};

using ActionPtr = QSharedPointer<BasicAction>;

} // namespace nx::vms::rules