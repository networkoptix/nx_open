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

HanwhaRequestHelper::HanwhaRequestHelper(const QnSecurityCamResourcePtr& resource):
    m_resource(resource)
{
}

HanwhaAttributes HanwhaRequestHelper::fetchAttributes(const QString& attributesPath)
{
    nx::Buffer buffer;
    auto url = buildAttributesUrl(attributesPath);

    qDebug() << "URL" << url;
    if (!doRequestInternal(url, m_resource->getAuth(), &buffer))
        return HanwhaAttributes();

    return HanwhaAttributes(buffer);
}

HanwhaCgiParameters HanwhaRequestHelper::fetchCgiParameters(const QString& cgiParametersPath)
{
    nx::Buffer buffer;
    auto url = buildAttributesUrl(cgiParametersPath);

    if (!doRequestInternal(url, m_resource->getAuth(), &buffer))
        return HanwhaCgiParameters();

    return HanwhaCgiParameters(buffer);
}

HanwhaResponse HanwhaRequestHelper::doRequest(
    const QString& cgi,
    const QString& submenu,
    const QString& action,
    const HanwhaRequestHelper::Parameters& parameters)
{
    nx::Buffer buffer;
    auto url = buildRequestUrl(cgi, submenu, action, parameters);
    
    if (!doRequestInternal(url, m_resource->getAuth(), &buffer))
        return HanwhaResponse();

    return HanwhaResponse(buffer);
}

HanwhaResponse HanwhaRequestHelper::view(const QString& path, const Parameters& parameters)
{
    return splitAndDoRequest(lit("view"), path, parameters);
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

QUrl HanwhaRequestHelper::buildRequestUrl(
    const QString& cgi,
    const QString& submenu,
    const QString& action,
    std::map<QString, QString> parameters) const
{
    QUrl url(m_resource->getUrl());
    QUrlQuery query;

    url.setPath(kPathTemplate.arg(cgi));
    query.addQueryItem(kSubmenu, submenu);
    query.addQueryItem(kAction, action);

    for (const auto& parameter: parameters)
        query.addQueryItem(parameter.first, parameter.second);
    
    url.setQuery(query);

    return url;
}

QUrl HanwhaRequestHelper::buildAttributesUrl(const QString& attributesPath) const
{
    QUrl url(m_resource->getUrl());
    url.setQuery(QUrlQuery());

    url.setPath(kAttributesPathTemplate.arg(attributesPath));

    return url;
}

bool HanwhaRequestHelper::doRequestInternal(
    const QUrl& url,
    const QAuthenticator& auth,
    nx::Buffer* outBuffer)
{
    NX_ASSERT(outBuffer);
    if (!outBuffer)
        return false;

    nx_http::HttpClient httpClient;

    httpClient.setUserName(auth.user());
    httpClient.setUserPassword(auth.password());
    httpClient.setSendTimeoutMs(kHttpTimeout.count());
    httpClient.setMessageBodyReadTimeoutMs(kHttpTimeout.count());
    httpClient.setResponseReadTimeoutMs(kHttpTimeout.count());

    if (!httpClient.doGet(url))
        return false;

    while (!httpClient.eof())
        outBuffer->append(httpClient.fetchMessageBodyBuffer());

    return true;
}

HanwhaResponse HanwhaRequestHelper::splitAndDoRequest(
    const QString& action,
    const QString& path,
    const Parameters& parameters)
{
    auto split = path.split(L'/');
    if (split.size() != 2)
        return HanwhaResponse();

    return doRequest(split[0], split[1], action, parameters);
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
