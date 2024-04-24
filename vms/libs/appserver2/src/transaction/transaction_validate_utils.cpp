// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "transaction_validate_utils.h"

#include <core/resource/videowall_resource.h>
#include <nx/utils/url.h>
#include <nx_ec/data/api_conversion_functions.h>

namespace ec2::detail {

bool validateForbiddenSpaces(const QString& value)
{
    if (value.isEmpty())
        return true;
    return !value.front().isSpace() && !value.back().isSpace();
}

bool validateNotEmptyWithoutSpaces(const QString& value)
{
    return !value.isEmpty() && validateForbiddenSpaces(value);
}

bool validateUrlOrEmpty(const QString& value)
{
    if (value.isEmpty())
        return true;
    return validateForbiddenSpaces(value) && nx::utils::Url(value).isValid();
}

bool validateUrl(const QString& value)
{
    return !value.isEmpty() && validateUrlOrEmpty(value);
}

Result validateModifyResourceParam(const nx::vms::api::LayoutData& data, const QnResourcePtr& existing)
{
    if (!validateResourceName(data, existing))
        return invalidParameterError("name");

    return {};
}

Result validateModifyResourceParam(const nx::vms::api::VideowallData& data, const QnResourcePtr& existing)
{
    using nx::vms::api::VideowallMatrixData;
    using nx::vms::api::VideowallItemData;

    if (!validateResourceName(data, existing))
        return invalidParameterError("name");

    nx::vms::api::VideowallData existingData;
    std::map<nx::Uuid, const VideowallItemData*> oldItems;
    std::map<nx::Uuid, const VideowallMatrixData*> oldMatrices;

    if (auto resource = existing.dynamicCast<QnVideoWallResource>())
    {
        fromResourceToApi(resource, existingData);

        for (const auto& item: existingData.items)
            oldItems[item.guid] = &item;

        for (const auto& matrix: existingData.matrices)
            oldMatrices[matrix.id] = &matrix;
    }

    size_t index = 0;
    for (const auto& item: data.items)
    {
        if (!validateName(item, getMapValue(oldItems, item.guid)))
            return invalidParameterError(nx::format("items[%1].name", index));
        ++index;
    }

    index = 0;
    for (const auto& matrix: data.matrices)
    {
        if (!validateName(matrix, getMapValue(oldMatrices, matrix.id)))
            return invalidParameterError(nx::format("matrices[%1].name", index));
        ++index;
    }

    return {};
}

Result validateModifyResourceParam(const nx::vms::api::WebPageData& data, const QnResourcePtr& existing)
{
    if (!validateResourceName(data, existing))
        return invalidParameterError("name");
    if (!validateResourceUrl(data, existing))
        return invalidParameterError("url");

    return {};
}

Result validateModifyResourceParam(const nx::vms::api::CameraData& data, const QnResourcePtr& existing)
{
    if (!validateResourceNameOrEmpty(data, existing))
        return invalidParameterError("name");
    if (!validateResourceUrlOrEmpty(data, existing))
        return invalidParameterError("url");

    return {};
}

Result invalidParameterError(const QString& paramName)
{
    return {ErrorCode::badRequest,
        nx::format(ServerApiErrors::tr("Invalid parameter `%1`"), paramName)};
}

} // namespace ec2::detail
