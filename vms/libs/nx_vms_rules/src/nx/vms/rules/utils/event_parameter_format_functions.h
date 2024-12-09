// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QString>

#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/rules_fwd.h>

#include "event_parameter_helper.h"

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::rules::utils {

/** Data required for substitution evaluation or filtering. */
struct SubstitutionContext
{
    QString name;

    AggregatedEventPtr event;
    std::optional<ItemDescriptor> manifest;

    nx::vms::rules::State state;
    QString objectTypeId;
    // TODO: #vbutkevich. Remove once the server can collect all attributes for objectType.
    QStringList attributes;
};

constexpr auto kEventAttributesPrefix = QLatin1StringView("event.attributes.");
constexpr auto kEventFieldsPrefix = QLatin1StringView("event.fields.");
constexpr auto kEventDetailsPrefix = QLatin1StringView("event.details.");

QString eventType(SubstitutionContext* substitution, common::SystemContext* context);
QString eventCaption(SubstitutionContext* substitution, common::SystemContext* context);
QString eventName(SubstitutionContext* substitution, common::SystemContext* context);
QString eventDescription(
    SubstitutionContext* substitution, common::SystemContext* context);

// Keep in sync with StringHelper::eventDescription().
QString extendedEventDescription(
    SubstitutionContext* substitution, common::SystemContext* context);

QString eventTime(SubstitutionContext* substitution, common::SystemContext* context);
QString eventTimeStart(SubstitutionContext* substitution, common::SystemContext* context);
QString eventTimeEnd(SubstitutionContext* substitution, common::SystemContext* context);
template<class T>
QString eventTimestamp(SubstitutionContext* substitution, common::SystemContext*)
{
    const auto count = std::chrono::duration_cast<T>(substitution->event->timestamp()).count();
    return QString::fromStdString(reflect::toString(count));
}
QString eventSource(SubstitutionContext* substitution, common::SystemContext* context);

QString deviceIp(SubstitutionContext* substitution, common::SystemContext* context);
QString deviceId(SubstitutionContext* substitution, common::SystemContext* context);
QString deviceMac(SubstitutionContext* substitution, common::SystemContext* context);
QString deviceType(SubstitutionContext* substitution, common::SystemContext* context);

QString deviceName(SubstitutionContext* substitution, common::SystemContext* context);

QString siteName(SubstitutionContext* substitution, common::SystemContext* context);
QString userName(SubstitutionContext* substitution, common::SystemContext* context);
QString eventAttribute(SubstitutionContext* substitution, common::SystemContext* context);
QString eventAttributeName(SubstitutionContext* substitution);
QString serverName(SubstitutionContext* substitution, common::SystemContext* context);

QString eventField(SubstitutionContext* substitution, common::SystemContext* context);
QString eventDetail(SubstitutionContext* substitution, common::SystemContext* context);

} // namespace nx::vms::rules::utils
