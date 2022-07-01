// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>

#include <nx/vms/api/rules/common.h>

class QJsonValue;

namespace nx::vms::rules {

class Field;
class EventField;
class ActionField;

class EventConnector;
class ActionExecutor;

class BasicEvent;
class BasicAction;

class AggregatedEvent;

class ActionBuilder;
class EventFilter;

class Engine;
class Rule;
class Router;

struct FieldDescriptor;
struct ItemDescriptor;

using EventPtr = QSharedPointer<BasicEvent>;
using ActionPtr = QSharedPointer<BasicAction>;

using AggregatedEventPtr = QSharedPointer<AggregatedEvent>;

using EventData = QMap<QString, QJsonValue>; // TODO: #spanasenko Move to separate class?

using FilterManifest = QMap<QString, QString>; //< Field name to field type mapping.

using State = nx::vms::api::rules::State;

} // namespace nx::vms::rules
