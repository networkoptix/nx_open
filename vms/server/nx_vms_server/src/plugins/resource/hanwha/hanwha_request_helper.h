#pragma once

#include <QtCore/QUrl>

#include <boost/optional.hpp>

#include <nx/utils/thread/rw_lock.h>

#include <core/resource/resource_fwd.h>
#include <plugins/resource/hanwha/hanwha_attributes.h>
#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>
#include <plugins/resource/hanwha/hanwha_response.h>
#include <plugins/resource/hanwha/hanwha_common.h>

class QAuthenticator;

namespace nx {
namespace vms::server {
namespace plugins {

class HanwhaSharedResourceContext;

class HanwhaRequestHelper
{
public:
    using Parameters = std::map<QString, QString>;

    HanwhaRequestHelper(
        const std::shared_ptr<HanwhaSharedResourceContext>& resourceContext,
        boost::optional<int> bypassChannel = boost::none);

    HanwhaAttributes fetchAttributes(const QString& attributesPath);

    HanwhaCgiParameters fetchCgiParameters(const QString& cgiParametersPath);

    HanwhaResponse doRequest(
        const QString& cgi,
        const QString& submenu,
        const QString& action,
        nx::utils::RwLockType lockType,
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

    HanwhaResponse control(
        const QString& path,
        const Parameters& parameters = Parameters());

    HanwhaResponse check(
        const QString& path,
        const Parameters& parameters = Parameters());

    void setGroupBy(const QString& groupBy);

    static utils::Url buildRequestUrl(
        utils::Url deviceUrl,
        const QString& cgi,
        const QString& submenu,
        const QString& action,
        const Parameters& parameters,
        const boost::optional<int>& bypassChannel = boost::none);

    static utils::Url buildRequestUrl(
        const HanwhaSharedResourceContext* sharedContext,
        const QString& path,
        const Parameters& parameters,
        const boost::optional<int> bypassChannel = boost::none);

private:
    utils::Url buildRequestUrl(
        const QString& cgi,
        const QString& submenu,
        const QString& action,
        const Parameters& parameters) const;

    utils::Url buildAttributesUrl(const QString& attributesPath) const;

    bool doRequestInternal(
        const utils::Url& url,
        const QAuthenticator& auth,
        nx::utils::RwLockType requestType,
        nx::Buffer* outBuffer,
        nx::network::http::StatusCode::Value* outStatusCode);

    HanwhaResponse splitAndDoRequest(
        const QString& action,
        const QString& path,
        const Parameters& parameters,
        nx::utils::RwLockType requestType);

    static utils::Url makeBypassUrl(const utils::Url &url, int bypassChannel);

private:
    const std::shared_ptr<HanwhaSharedResourceContext> m_resourceContext;
    QString m_groupBy;
    boost::optional<int> m_bypassChannel;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx
