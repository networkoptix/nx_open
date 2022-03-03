// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>

namespace nx::vms::rules {

class EventField;
class ActionField;

class EventConnector;
class ActionExecutor;

class BasicEvent;
class BasicAction;

class ActionBuilder;
class EventFilter;

class Rule;
class Router;

struct FieldDescriptor;
struct ItemDescriptor;

using EventPtr = QSharedPointer<BasicEvent>;
using ActionPtr = QSharedPointer<BasicAction>;

using EventData = QHash<QString, QVariant>; // TODO: #spanasenko Move to separate class?

using FilterManifest = QMap<QString, QString>; //< Field name to field type mapping.

} // namespace nx::vms::rules
