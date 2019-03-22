#pragma once

#include <vector>

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QMetaType>
#include <QtCore/QSharedPointer>

#include <common/common_globals.h>

#include <nx/fusion/model_functions_fwd.h>

#include <nx/vms/api/types/event_rule_types.h>

// TODO: #vkutin Think of a location to put this to
struct QnCameraConflictList;

namespace nx::vms::event {

class AbstractEvent;
typedef QSharedPointer<AbstractEvent> AbstractEventPtr;
typedef QList<AbstractEventPtr> AbstractEventList;

class AbstractAction;
typedef QSharedPointer<AbstractAction> AbstractActionPtr;
typedef QList<AbstractActionPtr> AbstractActionList;

class PanicAction;
typedef QSharedPointer<PanicAction> PanicActionPtr;
typedef QList<PanicAction> PanicActionList;

struct ActionData;
typedef std::vector<ActionData> ActionDataList;

typedef QMap<QString, QVariant> QnBusinessParams;

class Rule;
typedef QSharedPointer<Rule> RulePtr;
typedef QList<RulePtr> RuleList;

struct EventParameters;
struct ActionParameters;
struct EventMetaData;

class StringsHelper;

using EventReason = nx::vms::api:: EventReason;
using EventState = nx::vms::api::EventState;
using EventType = nx::vms::api::EventType;
using ActionType = nx::vms::api::ActionType;

} // namespace nx::vms::event
