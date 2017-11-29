#pragma once

#if defined(ENABLE_HANWHA)

#include <QtCore/QUrl>

#include <nx/utils/thread/semaphore.h>

#include <core/resource/resource_fwd.h>
#include <plugins/resource/hanwha/hanwha_attributes.h>
#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>
#include <plugins/resource/hanwha/hanwha_response.h>
#include <plugins/resource/hanwha/hanwha_common.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaSharedResourceContext;

class HanwhaRequestHelper
{
public:
    using Parameters = std::map<QString, QString>;

    HanwhaRequestHelper(const std::shared_ptr<HanwhaSharedResourceContext>& resourceContext);

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

    HanwhaResponse check(
        const QString& path,
        const Parameters& parameters = Parameters());

    void setIgnoreMutexAnalyzer(bool ignoreMutexAnalyzer);

    static QUrl buildRequestUrl(
        QUrl deviceUrl,
        const QString& cgi,
        const QString& submenu,
        const QString& action,
        const Parameters& parameters);

    static QUrl buildRequestUrl(
        const HanwhaSharedResourceContext* sharedContext,
        const QString& path,
        const Parameters& parameters);

private:
    QUrl buildRequestUrl(
        const QString& cgi,
        const QString& submenu,
        const QString& action,
        const Parameters& parameters) const;

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

    QUrl makeBypassUrl(const QUrl& url) const;

private:
    const std::shared_ptr<HanwhaSharedResourceContext> m_resourceContext;
    const QString m_channel;
    bool m_ignoreMutexAnalyzer = false;
    bool m_bypass = false;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
