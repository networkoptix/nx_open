// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QSharedPointer>

#include <nx/vms/api/types/event_rule_types.h>

namespace nx::vms::event {

class AbstractAction;
typedef QSharedPointer<AbstractAction> AbstractActionPtr;

class Rule;
typedef QSharedPointer<Rule> RulePtr;
typedef QList<RulePtr> RuleList;

struct EventParameters;
struct EventMetaData;
struct ActionParameters;

using EventReason = nx::vms::api::EventReason;
using EventState = nx::vms::api::EventState;
using EventType = nx::vms::api::EventType;
using ActionType = nx::vms::api::ActionType;

} // namespace nx::vms::event
