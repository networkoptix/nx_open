// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "saas_issue_event.h"

namespace nx::vms::event {

namespace {

constexpr auto kDelimiter = ';';

} // namespace

SaasIssueEvent::SaasIssueEvent(
    const QnResourcePtr& server,
    qint64 timeStamp,
    EventReason reasonCode,
    const QStringList& licenseKeys)
    :
    base_type(
        EventType::saasIssueEvent,
        server,
        timeStamp,
        reasonCode,
        encodeLicenseKeys(licenseKeys))
{
}

SaasIssueEvent::SaasIssueEvent(
    const QnResourcePtr& server,
    qint64 timeStamp,
    EventReason reasonCode,
    const UuidSet& resources)
    :
    base_type(
        EventType::saasIssueEvent,
        server,
        timeStamp,
        reasonCode,
        encodeResources(resources))
{
}

bool SaasIssueEvent::isLicenseMigrationIssue(EventReason reason)
{
    switch (reason)
    {
        case EventReason::licenseMigrationFailed:
        case EventReason::licenseMigrationSkipped:
            return true;

        case EventReason::notEnoughLocalRecordingServices:
        case EventReason::notEnoughCloudRecordingServices:
        case EventReason::notEnoughIntegrationServices:
            return false;

        default:
            NX_ASSERT(false, "Unhandled reason code");
            return false;
    }
}

QString SaasIssueEvent::encodeLicenseKeys(const QStringList& licenseKeys)
{
    return licenseKeys.join(kDelimiter);
}

QString SaasIssueEvent::encodeResources(const UuidSet& resources)
{
    QStringList stringList;
    for (const auto& id: resources)
        stringList.push_back(id.toSimpleString());

    return stringList.join(kDelimiter);
}

QStringList SaasIssueEvent::decodeLicenseKeys(const EventParameters& params)
{
    return params.description.split(kDelimiter);
}

UuidSet SaasIssueEvent::decodeResources(const EventParameters& params)
{
    UuidSet result;

    for (const auto& str: params.description.split(kDelimiter))
        result.insert(nx::Uuid(str));

    return result;
}

} // namespace nx::vms::event
