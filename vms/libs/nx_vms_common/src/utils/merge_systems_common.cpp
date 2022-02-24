// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "merge_systems_common.h"

#include <QtCore/QMap>

namespace {

const QMap<MergeSystemsStatus, QString> kErrorToString{
    {MergeSystemsStatus::ok, QString()},
    {MergeSystemsStatus::unknownError, "UNKNOWN_ERROR"},
    {MergeSystemsStatus::notFound, "FAIL"},
    {MergeSystemsStatus::incompatibleInternal, "INCOMPATIBLE_INTERNAL"},
    {MergeSystemsStatus::incompatibleVersion, "INCOMPATIBLE"},
    {MergeSystemsStatus::unauthorized, "UNAUTHORIZED"},
    {MergeSystemsStatus::forbidden, "FORBIDDEN"},
    {MergeSystemsStatus::configurationFailed, "CONFIGURATION_ERROR"},
    {MergeSystemsStatus::backupFailed, "BACKUP_ERROR"},
    {MergeSystemsStatus::starterLicense, "STARTER_LICENSE_ERROR"},
    {MergeSystemsStatus::dependentSystemBoundToCloud, "DEPENDENT_SYSTEM_BOUND_TO_CLOUD"},
    {MergeSystemsStatus::bothSystemBoundToCloud, "BOTH_SYSTEM_BOUND_TO_CLOUD"},
    {MergeSystemsStatus::cloudSystemsHaveDifferentOwners, "CLOUD_SYSTEMS_HAVE_DIFFERENT_OWNERS"},
    {MergeSystemsStatus::unconfiguredSystem, "UNCONFIGURED_SYSTEM"},
    {MergeSystemsStatus::duplicateMediaServerFound, "DUPLICATE_MEDIASERVER_FOUND"},
    {MergeSystemsStatus::nvrLicense, "NVR_LICENSE_ERROR"}
};

} // namespace

QString toString(MergeSystemsStatus value)
{
    return kErrorToString.value(value,
        kErrorToString[MergeSystemsStatus::unknownError]);
}

MergeSystemsStatus fromString(const QString& str)
{
    return kErrorToString.key(str, MergeSystemsStatus::unknownError);
}

bool allowsToMerge(MergeSystemsStatus status)
{
    switch (status)
    {
        case MergeSystemsStatus::ok:
        case MergeSystemsStatus::starterLicense:
        case MergeSystemsStatus::nvrLicense:
            return true;
        default:
            return false;
    }
}
