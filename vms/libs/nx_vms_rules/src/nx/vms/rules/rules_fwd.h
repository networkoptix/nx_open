// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>

#include <nx/vms/api/rules/common.h>

namespace nx::vms::rules {

class Field;
class EventFilterField;
class ActionBuilderField;

class EventConnector;
class ActionExecutor;

class BasicEvent;
class BasicAction;

class ActionBuilder;
class EventFilter;

class Engine;
class Rule;
class Router;

struct FieldDescriptor;
struct ItemDescriptor;
struct Group;

using EventPtr = QSharedPointer<BasicEvent>;
using ActionPtr = QSharedPointer<BasicAction>;

using RulePtr = std::shared_ptr<Rule>;
using ConstRulePtr = std::shared_ptr<const Rule>;

class AggregatedEvent;
using AggregatedEventPtr = QSharedPointer<AggregatedEvent>;

using EventData = nx::vms::api::rules::PropertyMap;
using ActionData = nx::vms::api::rules::PropertyMap;

using State = nx::vms::api::rules::State;

} // namespace nx::vms::rules
