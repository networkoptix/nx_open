// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "update_information.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::common::update {

bool Information::isValid() const { return !version.isNull(); }

bool Information::isEmpty() const { return packages.isEmpty(); }

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Information, (json), Information_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Status, (json), UpdateStatus_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ClientPackageStatus, (json), ClientPackageStatus_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    OverallClientPackageStatus, (json), OverallClientPackageStatus_Fields)

InformationError fetchErrorToInformationError(const vms::update::FetchError error)
{
    using FetchError = vms::update::FetchError;

    switch (error)
    {
        case FetchError::networkError:
            return InformationError::networkError;
        case FetchError::httpError:
            return InformationError::httpError;
        case FetchError::parseError:
            return InformationError::jsonError;
        case FetchError::paramsError:
            return InformationError::noError;
    }

    return InformationError::noError;
}

Status::Status(
    const QnUuid& serverId,
    Status::Code code,
    Status::ErrorCode errorCode,
    int progress)
    :
    serverId(serverId),
    code(code),
    errorCode(errorCode),
    progress(progress)
{
}

bool Status::suitableForRetrying() const
{
    return code == Code::error || code == Code::idle;
}

uint qHash(Status::ErrorCode key, uint seed)
{
    return ::qHash(static_cast<uint>(key), seed);
}

} // namespace nx::vms::common::update
