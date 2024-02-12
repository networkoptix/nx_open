// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/update/update_check.h>

#include "update_status.h"

namespace nx::vms::common::update {

struct NX_VMS_COMMON_API Information: nx::vms::update::PublicationInfo
{
    QList<nx::Uuid> participants = {};
    std::chrono::milliseconds lastInstallationRequestTime{-1};

    bool isValid() const;
    bool isEmpty() const;
};

#define Information_Fields \
    PublicationInfo_Fields \
    (participants) \
    (lastInstallationRequestTime)

QN_FUSION_DECLARE_FUNCTIONS(Information, (json), NX_VMS_COMMON_API)

enum class InformationError
{
    noError,
    networkError,
    httpError,
    jsonError,
    emptyPackagesUrls,
    brokenPackageError,
    missingPackageError,
    incompatibleVersion,
    incompatibleCloudHost,
    notFoundError,
    /** Error when client tried to contact mediaserver for update verification. */
    serverConnectionError,
    /**
     * Error when there's an attempt to parse packages.json for a version < 4.0 or an attempt to
     * parse update.json for a version >= 4.0.
     */
    incompatibleParser,
    noNewVersion,
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(InformationError*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<InformationError>;
    return visitor(
        Item{InformationError::noError, "no error"},
        Item{InformationError::networkError, "network error"},
        Item{InformationError::httpError, "http error"},
        Item{InformationError::jsonError, "json error"},
        Item{InformationError::brokenPackageError, "local update package is corrupted"},
        Item{InformationError::missingPackageError, "missing files in the update package"},
        Item{InformationError::incompatibleVersion, "incompatible version"},
        Item{InformationError::incompatibleCloudHost, "incompatible cloud host"},
        Item{InformationError::notFoundError, "not found"},
        Item{InformationError::serverConnectionError, "failed to get response from mediaserver"},
        Item{InformationError::noNewVersion, "no new version"}
    );
}

NX_VMS_COMMON_API InformationError fetchErrorToInformationError(
    const nx::vms::update::FetchError error);

NX_VMS_COMMON_API void PrintTo(InformationError value, ::std::ostream* stream);

} // namespace nx::vms::common::update
