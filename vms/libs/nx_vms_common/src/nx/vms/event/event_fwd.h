// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QList>
#include <QtCore/QSharedPointer>

#include <nx/vms/api/types/event_rule_types.h>

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

class Rule;
typedef QSharedPointer<Rule> RulePtr;
typedef QList<RulePtr> RuleList;

struct EventParameters;
struct ActionParameters;
struct EventMetaData;

class StringsHelper;

using EventReason = nx::vms::api::EventReason;
using EventState = nx::vms::api::EventState;
using EventType = nx::vms::api::EventType;
using ActionType = nx::vms::api::ActionType;

} // namespace nx::vms::event
