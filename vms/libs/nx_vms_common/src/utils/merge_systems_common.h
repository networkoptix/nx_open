// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

enum class MergeSystemsStatus
{
    ok,
    unknownError,
    notFound,
    incompatibleInternal,
    incompatibleVersion,
    unauthorized,
    forbidden,
    configurationFailed,
    backupFailed,
    starterLicense,
    dependentSystemBoundToCloud,
    bothSystemBoundToCloud,
    cloudSystemsHaveDifferentOwners,
    unconfiguredSystem,
    duplicateMediaServerFound,
    nvrLicense,
    certificateInvalid,
    certificateRejected,
    tooManyServers,
};

NX_VMS_COMMON_API QString toString(MergeSystemsStatus value);
NX_VMS_COMMON_API MergeSystemsStatus fromString(const QString& str);

/** Check if systems are allowed to merge. */
NX_VMS_COMMON_API bool allowsToMerge(MergeSystemsStatus status);
