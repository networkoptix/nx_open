#pragma once

#if defined(ENABLE_HANWHA)

#include <map>

#include <QtCore/QUrl>

#include <core/resource/resource_fwd.h>
#include <plugins/resource/hanwha/hanwha_attributes.h>
#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>
#include <plugins/resource/hanwha/hanwha_response.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaRequestHelper
{
public:
    using Parameters = std::map<QString, QString>;

    HanwhaRequestHelper(const QnSecurityCamResourcePtr& resource);
    HanwhaRequestHelper(const QAuthenticator& auth, const QString& url);

    HanwhaAttributes fetchAttributes(const QString& attributesPath);

    HanwhaCgiParameters fetchCgiParameters(const QString& cgiParametersPath);

    HanwhaResponse doRequest(
        const QString& cgi,
        const QString& submenu,
        const QString& action,
        const Parameters& parameters = Parameters(),
        const QString& groupBy = QString());

    HanwhaResponse view(
        const QString& path,
        const Parameters& parameters = Parameters(),
        const QString& groupBy = QString());

    HanwhaResponse set(
        const QString& path,
        const Parameters& parameters = Parameters());

    HanwhaResponse update(
        const QString& path,
        const Parameters& parameters = Parameters());

    HanwhaResponse add(
        const QString& path,
        const Parameters& parameters = Parameters());

    HanwhaResponse remove(
        const QString& path,
        const Parameters& parameters = Parameters());

    HanwhaResponse control(
        const QString& path,
        const Parameters& parameters = Parameters());

    void setIgnoreMutexAnalyzer(bool ignoreMutexAnalyzer);

private:
    QUrl buildRequestUrl(
        const QString& cgi,
        const QString& submenu,
        const QString& action,
        std::map<QString, QString> parameters) const;

    QUrl buildAttributesUrl(const QString& attributesPath) const;

    bool doRequestInternal(
        const QUrl& url,
        const QAuthenticator& auth,
        nx::Buffer* outBuffer,
        nx_http::StatusCode::Value* outStatusCode);

    HanwhaResponse splitAndDoRequest(
        const QString& action,
        const QString& path,
        const Parameters& parameters,
        const QString& groupBy = QString());

private:
    const QAuthenticator m_auth;
    const QString m_url;
    bool m_ignoreMutexAnalyzer = false;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
