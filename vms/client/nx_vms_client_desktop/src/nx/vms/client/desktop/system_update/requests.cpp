// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "requests.h"

#include <api/server_rest_connection.h>

using namespace nx::vms::update;

namespace nx::vms::client::desktop::system_update {

rest::Handle requestUpdateInformation(
    const rest::ServerConnectionPtr& connection,
    const common::update::UpdateInfoParams& params,
    rest::JsonResultCallback&& callback,
    QThread* targetThread)
{
    return connection->getJsonResult(
        "/ec2/updateInformation",
        params.toRestParams(),
        std::move(callback),
        targetThread);
}

rest::Handle requestUpdateStatus(
    const rest::ServerConnectionPtr& connection,
    rest::JsonResultCallback&& callback,
    QThread* targetThread)
{
    return connection->getJsonResult(
        "/ec2/updateStatus",
        network::rest::Params{},
        std::move(callback),
        targetThread);
}

UpdateContents getUpdateContents(
    rest::ServerConnectionPtr proxyConnection,
    const utils::Url& url,
    const common::update::UpdateInfoParams& params,
    const bool skipVersionEqualToCurrent)
{
    using common::update::InformationError;

    const bool skipDirectCheck = !url.isValid()
       || std::holds_alternative<common::update::TargetVersionParams>(params)
       || std::holds_alternative<common::update::InstalledVersionParams>(params);

    UpdateContents contents;

    if (!skipDirectCheck)
    {
        const bool certainVersionRequested = std::holds_alternative<CertainVersionParams>(params);
        if (certainVersionRequested)
        {
            contents.sourceType = UpdateSourceType::internetSpecific;
            contents.changeset = std::get<CertainVersionParams>(params).version.toString();
        }

        const PublicationInfoResult result = getPublicationInfo(
            url, *params.toPublicationInfoParams());

        if (const auto error = std::get_if<FetchError>(&result))
        {
            contents.error = common::update::fetchErrorToInformationError(*error);
        }
        else if (std::holds_alternative<std::nullptr_t>(result))
        {
            contents.error = InformationError::noNewVersion;
        }
        else
        {
            contents.info = common::update::Information{std::get<PublicationInfo>(result)};
            contents.source = nx::format("%1 by getUpdateContents", url);
        }

        api::SoftwareVersion currentVersion;
        if (const auto p = std::get_if<LatestVmsVersionParams>(&params))
            currentVersion = p->currentVersion;
        else if (const auto p = std::get_if<LatestDesktopClientVersionParams>(&params))
            currentVersion = p->currentVersion;
        else if (const auto p = std::get_if<CertainVersionParams>(&params))
            currentVersion = p->currentVersion;

        // Usually we need the update contents with a version greater than the current
        // version (except the case when we request for a certain version), but sometimes we need
        // to get the update contents even if the version is the same as the current one.
        if (!certainVersionRequested
            && contents.info.version == currentVersion
            && skipVersionEqualToCurrent)
        {
            contents.error = InformationError::noNewVersion;
        }

        if (contents.error != InformationError::networkError)
            return contents;
    }

    NX_WARNING(NX_SCOPE_TAG, "Checking for updates using mediaserver as proxy");

    auto promise = std::make_shared<std::promise<bool>>();
    contents.source = nx::format("%1 by getUpdateContents proxied by mediaserver", url);

    auto proxyCheck = promise->get_future();
    requestUpdateInformation(proxyConnection, params,
        [promise = std::move(promise), &contents](
            bool success, rest::Handle /*handle*/, const nx::network::rest::JsonResult& result)
        {
            if (success)
            {
                if (result.error != nx::network::rest::Result::NoError)
                {
                    NX_DEBUG(NX_SCOPE_TAG,
                        "getUpdateContents: "
                            "Error in response to /ec2/updateInformation request: %1",
                        result.errorString);

                    nx::reflect::fromString(result.errorString.toStdString(), &contents.error);
                }
                else
                {
                    contents.error = InformationError::noError;
                    contents.info = result.deserialized<common::update::Information>();
                }
            }
            else
            {
                contents.error = InformationError::networkError;
            }

            promise->set_value(success);
        });

    proxyCheck.wait();

    return contents;
}

} // namespace nx::vms::client::desktop::system_update
