#include "iqinvision_request_helper.h"

#include <chrono>

#include <QtCore/QUrlQuery>
#include <QtCore/QXmlStreamReader>

#include <nx/network/http/http_client.h>

#include "iqinvision_resource.h"

namespace nx {
namespace vms::server {
namespace plugins {

namespace {

static const QString kOidStyle = lit("B");
static std::chrono::seconds kSendTimeout(4);
static std::chrono::seconds kReceiveTimeout(4);

static const QString kHtmlResponseStartElement = lit("<xmp>");
static const QString kHtmlResponseEndElement = lit("</xmp>");

} // namespace

IqInvisionRequestHelper::IqInvisionRequestHelper(const QnPlIqResourcePtr resource):
    m_resource(resource)
{
    NX_ASSERT(m_resource);
}

IqInvisionResponse IqInvisionRequestHelper::oid(const QString & oid)
{
    return doRequest(buildOidUrl(oid));
}

IqInvisionResponse IqInvisionRequestHelper::setOid(const QString & oid, const QString & value)
{
    return doRequest(buildSetOidUrl(oid, value));
}

std::unique_ptr<nx::network::http::HttpClient> IqInvisionRequestHelper::makeHttpClient() const
{
    using namespace std::chrono;

    auto httpClient = std::make_unique<nx::network::http::HttpClient>();
    auto auth = m_resource->getAuth();

    httpClient->setSendTimeout(kSendTimeout);
    httpClient->setMessageBodyReadTimeout(kReceiveTimeout);
    httpClient->setResponseReadTimeout(kReceiveTimeout);
    httpClient->setUserName(auth.user());
    httpClient->setUserPassword(auth.password());

    return httpClient;
}

nx::utils::Url IqInvisionRequestHelper::buildOidUrl(const QString & oid) const
{
    nx::utils::Url url(m_resource->getUrl());
    url.setPath(lit("/get.oid"));
    url.setQuery(oid);
    return url;
}

nx::utils::Url IqInvisionRequestHelper::buildSetOidUrl(const QString & oid, const QString & value) const
{
    nx::utils::Url url(m_resource->getUrl());
    url.setPath(lit("set.oid"));

    QUrlQuery query;
    // TODO: #dmihsin determine OID style denending on camera type.
    query.addQueryItem(lm("OidT%1%2").args(kOidStyle, value), value);
    url.setQuery(query);

    return url;
}

IqInvisionResponse IqInvisionRequestHelper::doRequest(const nx::utils::Url& url)
{
    IqInvisionResponse result;
    auto httpClient = makeHttpClient();
    const bool gotResponse = httpClient->doGet(url);
    if (!gotResponse)
        return result;

    const auto responseData = httpClient->fetchEntireMessageBody();
    if (!responseData)
        return result;

    const auto httpResponse = httpClient->response();
    if (!httpResponse)
        return result;

    result.statusCode = httpResponse->statusLine.statusCode;
    result.data = parseResponseData(*responseData, httpClient->contentType());

    return result;
}

nx::Buffer IqInvisionRequestHelper::parseResponseData(
    const nx::Buffer& responseData,
    const nx::Buffer& contentType) const
{
    if (contentType == lit("text/html"))
        return parseHtmlResponseData(responseData);

    return responseData;
}

nx::Buffer IqInvisionRequestHelper::parseHtmlResponseData(const nx::Buffer& responseData) const
{
    // TODO: #dmishin try to find more reliable way to parse HTML response.
    auto startIndex = responseData.indexOf(kHtmlResponseStartElement);
    if (startIndex < 0)
        return responseData;

    startIndex += kHtmlResponseStartElement.length();

    const auto endIndex = responseData.indexOf(kHtmlResponseEndElement, startIndex);
    if (endIndex < 0)
        return responseData;

    return responseData.mid(startIndex, endIndex - startIndex);
}

bool IqInvisionResponse::isSuccessful() const
{
    return nx::network::http::StatusCode::isSuccessCode(statusCode);
}

QString IqInvisionResponse::toString() const
{
    return QString::fromUtf8(data);
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
