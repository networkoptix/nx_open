#pragma once

#include <nx/network/http/http_client.h>
#include <nx/network/http/http_types.h>
#include <core/resource/resource_fwd.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

struct IqInvisionResponse
{
    int statusCode = nx_http::StatusCode::undefined;
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
    std::unique_ptr<nx_http::HttpClient> makeHttpClient() const;
    QUrl buildOidUrl(const QString& oid) const;
    QUrl buildSetOidUrl(const QString& oid, const QString& value) const;

    IqInvisionResponse doRequest(const QUrl& url);
    nx::Buffer parseResponseData(
        const nx::Buffer& responseData,
        const nx::Buffer& contentType) const;

    nx::Buffer parseHtmlResponseData(const nx::Buffer& responseData) const;

private:
    const QnPlIqResourcePtr m_resource;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
