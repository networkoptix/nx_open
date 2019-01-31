#pragma once

#include <nx/network/http/http_client.h>
#include <nx/network/http/http_types.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/server/resource/resource_fwd.h>

namespace nx {
namespace vms::server {
namespace plugins {

struct IqInvisionResponse
{
    int statusCode = nx::network::http::StatusCode::undefined;
    nx::Buffer data;

    bool isSuccessful() const;
    QString toString() const;
};

class IqInvisionRequestHelper
{
public:
    IqInvisionRequestHelper(const QnPlIqResourcePtr resource);

    IqInvisionResponse oid(const QString& oid);

    IqInvisionResponse setOid(const QString& oid, const QString& value);

private:
    std::unique_ptr<nx::network::http::HttpClient> makeHttpClient() const;
    nx::utils::Url buildOidUrl(const QString& oid) const;
    nx::utils::Url buildSetOidUrl(const QString& oid, const QString& value) const;

    IqInvisionResponse doRequest(const nx::utils::Url& url);
    nx::Buffer parseResponseData(
        const nx::Buffer& responseData,
        const nx::Buffer& contentType) const;

    nx::Buffer parseHtmlResponseData(const nx::Buffer& responseData) const;

private:
    const QnPlIqResourcePtr m_resource;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx
