// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "license_issue_event.h"

namespace nx::vms::event {

namespace {

constexpr auto kDelimiter = ';';

} // namespace

LicenseIssueEvent::LicenseIssueEvent(
    const QnResourcePtr& resource,
    qint64 timeStamp,
    EventReason reasonCode,
    const UuidSet& disabledCameras)
    :
    base_type(
        EventType::licenseIssueEvent,
        resource,
        timeStamp,
        reasonCode,
        encodeCameras(disabledCameras))
{
}

QString LicenseIssueEvent::encodeCameras(const UuidSet& cameras)
{
    QStringList stringList;
    for (const auto id : cameras)
        stringList.push_back(id.toString());

    return stringList.join(kDelimiter);
}

UuidSet LicenseIssueEvent::decodeCameras(const EventParameters& params)
{
    UuidSet result;

    for (const auto& str: params.description.split(kDelimiter))
        result.insert(nx::Uuid(str));

    return result;
}

} // namespace nx::vms::event
