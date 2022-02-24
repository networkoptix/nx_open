// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <core/resource_management/resource_data_pool.h>
#include <nx/network/http/http_client.h>
#include <nx/network/ssl/helpers.h>

#include "resource_params_data.h"

static const std::chrono::seconds kResourceDataReadingTimeout(5);
static const std::chrono::seconds kResourceDataFetchEntireBodyTimeout(10);

namespace nx::vms::utils {

ResourceParamsData ResourceParamsData::load(const nx::utils::Url& url)
{
    ResourceParamsData result;
    result.location = url.toString();
    if (!url.isValid() || url.host().isEmpty())
    {
        NX_WARNING(NX_SCOPE_TAG, "Invalid url %1 for resource_data.json", result.location);
        return result;
    }
    nx::network::http::HttpClient httpClient{nx::network::ssl::kDefaultCertificateCheck};
    httpClient.setResponseReadTimeout(kResourceDataReadingTimeout);
    httpClient.setMessageBodyReadTimeout(kResourceDataReadingTimeout);
    if (httpClient.doGet(url)
        && httpClient.response()->statusLine.statusCode == nx::network::http::StatusCode::ok)
    {
        auto body = httpClient.fetchEntireMessageBody(kResourceDataFetchEntireBodyTimeout);
        if (body)
            result.value = body->takeByteArray();
    }
    if (result.value.isEmpty())
        NX_WARNING(NX_SCOPE_TAG, "Failed to GET resource_data.json from %1", result.location);
    else
        NX_DEBUG(NX_SCOPE_TAG, "Loaded %1 bytes of data from %2", result.value.size(), result.location);
    return result;
}

ResourceParamsData ResourceParamsData::load(QFile&& file)
{
    ResourceParamsData result;
    result.location = file.fileName();
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to open file %1", result.location);
        return result;
    }
    result.value = file.readAll();
    if (result.value.isEmpty())
        NX_WARNING(NX_SCOPE_TAG, "Empty resource_data.json from %1", result.location);
    return result;
}

ResourceParamsData ResourceParamsData::getWithGreaterVersion(
    const std::vector<ResourceParamsData>& datas)
{
    ResourceParamsData result;
    nx::utils::SoftwareVersion resultVersion;
    for (const auto& data: datas)
    {
        if (data.value.isEmpty())
            continue;
        auto version = QnResourceDataPool::getVersion(data.value);
        if (!result.value.isEmpty() && version <= resultVersion)
        {
            NX_DEBUG(NX_SCOPE_TAG,
                "Skip resource_data.json version %1 from %2. Version %3 from %4 is greater.",
                version, data.location, resultVersion, result.location);
            continue;
        }
        if (!NX_ASSERT(QnResourceDataPool::validateData(data.value),
            "Skip invalid resource_data.json from %1", data.location))
        {
            continue;
        }
        resultVersion = std::move(version);
        result = data;
    }
    return result;
}

} // namespace nx::vms::utils
