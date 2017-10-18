#if defined(ENABLE_HANWHA)

#include "hanwha_resource.h"
#include "hanwha_request_helper.h"
#include "hanwha_utils.h"

#include <QtCore/QUrlQuery>

#include <chrono>

#include <core/resource/security_cam_resource.h>
#include <nx/network/http/http_client.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

namespace {

const std::chrono::milliseconds kHttpTimeout(4000);

const QString kPathTemplate = lit("/stw-cgi/%1.cgi");
const QString kAttributesPathTemplate = lit("/stw-cgi/attributes.cgi/%1");

const QString kSubmenu = lit("msubmenu");
const QString kAction = lit("action");

} // namespace

HanwhaRequestHelper::HanwhaRequestHelper(const HanwhaResourcePtr& resource):
    m_auth(resource->getAuth()),
    m_url(resource->getUrl()),
    m_requestSemaphore(resource->requestSemaphore())
{
}

HanwhaRequestHelper::HanwhaRequestHelper(const QAuthenticator& auth, const QString& url):
    m_auth(auth),
    m_url(url)
{
}

HanwhaAttributes HanwhaRequestHelper::fetchAttributes(const QString& attributesPath)
{
    nx::Buffer buffer;
    nx_http::StatusCode::Value statusCode = nx_http::StatusCode::undefined;
    auto url = buildAttributesUrl(attributesPath);

    if (!doRequestInternal(url, m_auth, &buffer, &statusCode))
        return HanwhaAttributes(statusCode);

    return HanwhaAttributes(buffer, statusCode);
}

HanwhaCgiParameters HanwhaRequestHelper::fetchCgiParameters(const QString& cgiParametersPath)
{
    nx::Buffer buffer;
    nx_http::StatusCode::Value statusCode = nx_http::StatusCode::undefined;
    auto url = buildAttributesUrl(cgiParametersPath);

    if (!doRequestInternal(url, m_auth, &buffer, &statusCode))
        return HanwhaCgiParameters(statusCode);

    return HanwhaCgiParameters(buffer, statusCode);
}

HanwhaResponse HanwhaRequestHelper::doRequest(
    const QString& cgi,
    const QString& submenu,
    const QString& action,
    const HanwhaRequestHelper::Parameters& parameters,
    const QString& groupBy)
{
    nx::Buffer buffer;
    auto url = buildRequestUrl(cgi, submenu, action, parameters);
    
    nx_http::StatusCode::Value statusCode = nx_http::StatusCode::undefined;
    if (!doRequestInternal(url, m_auth, &buffer, &statusCode))
        return HanwhaResponse(statusCode, url.toString());

    return HanwhaResponse(buffer, statusCode, url.toString(), groupBy);
}

HanwhaResponse HanwhaRequestHelper::view(
    const QString& path,
    const Parameters& parameters,
    const QString& groupBy)
{
    return splitAndDoRequest(lit("view"), path, parameters, groupBy);
}

HanwhaResponse HanwhaRequestHelper::set(const QString& path, const Parameters& parameters)
{
    return splitAndDoRequest(lit("set"), path, parameters);
}

HanwhaResponse HanwhaRequestHelper::update(const QString& path, const Parameters& parameters)
{
    return splitAndDoRequest(lit("update"), path, parameters);
}

HanwhaResponse HanwhaRequestHelper::add(const QString& path, const Parameters& parameters)
{
    return splitAndDoRequest(lit("add"), path, parameters);
}

HanwhaResponse HanwhaRequestHelper::remove(const QString& path, const Parameters& parameters)
{
    return splitAndDoRequest(lit("remove"), path, parameters);
}

HanwhaResponse HanwhaRequestHelper::control(const QString& path, const Parameters& parameters)
{
    return splitAndDoRequest(lit("control"), path, parameters);
}

void HanwhaRequestHelper::setIgnoreMutexAnalyzer(bool ignoreMutexAnalyzer)
{
    m_ignoreMutexAnalyzer = ignoreMutexAnalyzer;
}

utils::Url HanwhaRequestHelper::buildRequestUrl(
    nx::utils::Url deviceUrl,
    const QString& cgi,
    const QString& submenu,
    const QString& action,
    std::map<QString, QString> parameters)
{
    QUrlQuery query;

    deviceUrl.setPath(kPathTemplate.arg(cgi));
    query.addQueryItem(kSubmenu, submenu);
    query.addQueryItem(kAction, action);

    for (const auto& parameter : parameters)
        query.addQueryItem(parameter.first, parameter.second);

    deviceUrl.setQuery(query);
    return deviceUrl;
}

nx::utils::Url HanwhaRequestHelper::buildRequestUrl(
    const QString& cgi,
    const QString& submenu,
    const QString& action,
    std::map<QString, QString> parameters) const
{
    return buildRequestUrl(m_url, cgi, submenu, action, std::move(parameters));
}

utils::Url HanwhaRequestHelper::buildAttributesUrl(const QString& attributesPath) const
{
    utils::Url url(m_url);
    url.setQuery(QUrlQuery());

    url.setPath(kAttributesPathTemplate.arg(attributesPath));

    return url;
}

bool HanwhaRequestHelper::doRequestInternal(
    const utils::Url& url,
    const QAuthenticator& auth,
    nx::Buffer* outBuffer,
    nx_http::StatusCode::Value* outStatusCode)
{
    NX_ASSERT(outBuffer);
    if (!outBuffer)
        return false;

    nx_http::HttpClient httpClient;

    httpClient.setIgnoreMutexAnalyzer(m_ignoreMutexAnalyzer);
    httpClient.setUserName(auth.user());
    httpClient.setUserPassword(auth.password());
    httpClient.setSendTimeoutMs(kHttpTimeout.count());
    httpClient.setMessageBodyReadTimeoutMs(kHttpTimeout.count());
    httpClient.setResponseReadTimeoutMs(kHttpTimeout.count());

    QnSemaphoreLocker lock(m_requestSemaphore);
    if (!httpClient.doGet(url))
        return false;

    while (!httpClient.eof())
        outBuffer->append(httpClient.fetchMessageBodyBuffer());

    *outStatusCode = (nx_http::StatusCode::Value)httpClient.response()->statusLine.statusCode;

    return true;
}

HanwhaResponse HanwhaRequestHelper::splitAndDoRequest(
    const QString& action,
    const QString& path,
    const Parameters& parameters,
    const QString& groupBy)
{
    auto split = path.split(L'/');
    if (split.size() != 2)
    {
        QString parameterString;
        for (const auto& parameter: parameters)
            parameterString += parameter.first + lit("=") + parameter.second + lit("&");

        QString urlString = lit("Path: %1, Action: %2, Parameters: %3")
            .arg(path)
            .arg(action)
            .arg(parameterString);

        return HanwhaResponse(nx_http::StatusCode::undefined, urlString);
    }

    return doRequest(split[0], split[1], action, parameters, groupBy);
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
