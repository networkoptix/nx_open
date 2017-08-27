#pragma once

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
    using Parameters = std::map<QString, QString>;
public:
    HanwhaRequestHelper(const QnSecurityCamResourcePtr& resource);

    HanwhaAttributes fetchAttributes(const QString& attributesPath);

    HanwhaCgiParameters fetchCgiParameters(const QString& cgiParametersPath);

    HanwhaResponse doRequest(
        const QString& cgi,
        const QString& submenu,
        const QString& action,
        const Parameters& parameters = Parameters());

    HanwhaResponse view(
        const QString& path,
        const Parameters& parameters = Parameters());

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

private:
    QUrl buildRequestUrl(
        const QString& cgi,
        const QString& submenu,
        const QString& action,
        std::map<QString, QString> parameters) const;

    QUrl buildAttributesUrl(const QString& attributesPath) const;

    bool doRequestInternal(const QUrl& url, const QAuthenticator& auth, nx::Buffer* outBuffer);

    HanwhaResponse splitAndDoRequest(
        const QString& action,
        const QString& path,
        const Parameters& parameters);

private:
    const QnSecurityCamResourcePtr m_resource;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

