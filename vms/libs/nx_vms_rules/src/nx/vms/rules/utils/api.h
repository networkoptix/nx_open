// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>

#include <nx/utils/uuid.h>
#include <nx/vms/api/rules/rule.h>

#include "../rules_fwd.h"

namespace nx::vms::api::rules {

struct ActionBuilder;
struct EventFilter;
struct Field;
struct Rule;

struct ActionInfo;
struct EventInfo;

} // namespace nx::vms::api::rules


namespace nx::vms::rules {

namespace api = ::nx::vms::api::rules;

NX_VMS_RULES_API api::Rule serialize(const Rule* rule);
NX_VMS_RULES_API api::EventFilter serialize(const EventFilter* filter);
NX_VMS_RULES_API api::ActionBuilder serialize(const ActionBuilder *builder);

NX_VMS_RULES_API api::ActionInfo serialize(
    const BasicAction* action, const QSet<QByteArray>& excludedProperties = {});
NX_VMS_RULES_API api::EventInfo serialize(const BasicEvent* action, UuidList ruleIds = {});

NX_VMS_RULES_API nx::vms::api::rules::RuleV4 toApi(
    const nx::vms::rules::Engine* engine,
    const nx::vms::api::rules::Rule& rule);
NX_VMS_RULES_API std::optional<nx::vms::api::rules::Rule> fromApi(
    const nx::vms::rules::Engine* engine,
    nx::vms::api::rules::RuleV4&& rule,
    QString author,
    QString* error = nullptr);

} // namespace nx::vms::rules
