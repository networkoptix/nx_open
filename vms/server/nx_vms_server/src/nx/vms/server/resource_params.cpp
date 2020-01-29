#include <nx/utils/url.h>
#include <nx/network/http/http_client.h>

#include "resource_params.h"

static const std::chrono::seconds kResourceDataReadingTimeout(5);

namespace nx::vms::server {

static QByteArray loadDataFromFile(const QString& fileName)
{
    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        return file.readAll();
    return QByteArray();
}

static QByteArray loadDataFromUrl(nx::utils::Url url)
{
    auto httpClient = std::make_unique<nx::network::http::HttpClient>();
    httpClient->setResponseReadTimeout(kResourceDataReadingTimeout);
    httpClient->setMessageBodyReadTimeout(kResourceDataReadingTimeout);
    if (httpClient->doGet(url)
        && httpClient->response()->statusLine.statusCode == nx::network::http::StatusCode::ok)
    {
        const auto value = httpClient->fetchEntireMessageBody();
        if (value)
            return *value;
    }
    return QByteArray();
}

ResourceParams ResourceParams::load(
    const std::vector<std::variant<QString, nx::utils::Url, ResourceParams>>& sources,
    const std::function<bool(const ResourceParams&)>& choose)
{
    ResourceParams chosen;
    for (const auto& source: sources)
    {
        ResourceParams candidate;
        if (std::holds_alternative<nx::utils::Url>(source))
        {
            const auto url = std::get<nx::utils::Url>(source);
            candidate.source = url.toString();
            if (!url.isValid() || url.host().isEmpty())
            {
                NX_WARNING(NX_SCOPE_TAG,
                    "Ignore invalid url %1 to load resource_data.json from", candidate.source);
                continue;
            }
            candidate.data = loadDataFromUrl(url);
        }
        else if (std::holds_alternative<QString>(source))
        {
            candidate.source = std::get<QString>(source);
            candidate.data = loadDataFromFile(candidate.source);
        }
        else
        {
            candidate = std::get<ResourceParams>(source);
        }
        if (candidate.data.isEmpty())
        {
            NX_WARNING(NX_SCOPE_TAG, "Ignore empty resource_data.json from %1", candidate.source);
            continue;
        }
        if (choose(candidate))
            chosen = candidate;
    }
    return chosen;
}

} // namespace nx::vms::server

