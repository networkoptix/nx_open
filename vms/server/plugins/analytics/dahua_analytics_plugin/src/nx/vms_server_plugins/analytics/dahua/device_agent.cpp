#include "device_agent.h"

#include <algorithm>
#include <set>

#include <nx/utils/string.h>
#include <nx/utils/url.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/log/assert.h>
#include <nx/vms_server_plugins/utils/exception.h>
#include <nx/vms_server_plugins/utils/http.h>
#include <nx/fusion/serialization/json.h>
#include <nx/network/http/futures/client.h>
#include <nx/sdk/helpers/string.h>

#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

#include "engine.h"

namespace nx::vms_server_plugins::analytics::dahua {

using namespace nx::utils;
using namespace nx::vms_server_plugins::utils;
using namespace nx::network;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

class DeviceAgent::ManifestBuilder
{
public:
    static QJsonObject build(const DeviceAgent& deviceAgent)
    {
        return ManifestBuilder(deviceAgent).build();
    }

private:
    explicit ManifestBuilder(const DeviceAgent& deviceAgent):
        m_supportedEventTypes(deviceAgent.fetchSupportedEventTypes())
    {
    }

    QJsonObject build() const
    {
        return QJsonObject{
            {"groups", buildGroups()},
            {"eventTypes", buildEventTypes()},
            {"objectTypes", buildObjectTypes()},
        };
    }

    QJsonArray buildGroups() const
    {
        QJsonArray jsonGroups;

        for (const auto group: kEventTypeGroups)
        {
            if (std::none_of(
                    m_supportedEventTypes.begin(), m_supportedEventTypes.end(),
                    [&](const auto type) { return type->group == group; }))
            {
                continue;
            }

            QJsonObject jsonGroup = {
                {"id", group->id},
                {"name", group->prettyName},
            };

            jsonGroups.push_back(std::move(jsonGroup));
        }

        return jsonGroups;
    }

    QJsonArray buildEventTypes() const
    {
        QJsonArray jsonTypes;

        for (const auto type: m_supportedEventTypes)
        {
            QJsonObject jsonType = {
                {"id", type->id},
                {"name", type->prettyName},
            };

            std::vector<QString> flags;
            if (type->isStateDependent)
                flags.push_back("stateDependent");
            if (!flags.empty())
                jsonType["flags"] = nx::utils::join(flags, "|");

            if (const auto group = type->group)
                jsonType["groupId"] = group->id;

            jsonTypes.push_back(std::move(jsonType));
        }

        return jsonTypes;
    }
           
    QJsonArray buildObjectTypes() const
    {
        std::set<const ObjectType*> supportedTypes;
        for (const auto eventType: m_supportedEventTypes)
        {
            const auto& types = eventType->objectTypes;
            supportedTypes.insert(types.begin(), types.end());
        }

        QJsonArray jsonTypes;

        for (const auto type: supportedTypes)
        {
            QJsonObject jsonType = {
                {"id", type->id},
                {"name", type->prettyName},
            };

            jsonTypes.push_back(std::move(jsonType));
        }

        return jsonTypes;
    }
             
private:
    const std::vector<const EventType*> m_supportedEventTypes;
};

DeviceAgent::DeviceAgent(Engine* engine, Ptr<const nx::sdk::IDeviceInfo> info):
    m_engine(engine),
    m_info(std::move(info)),
    m_metadataMonitor(this)
{
}

DeviceAgent::~DeviceAgent()
{
    m_metadataMonitor.setNeededTypes(nullptr);
}

Engine* DeviceAgent::engine() const
{
    return m_engine;
}

Ptr<const IDeviceInfo> DeviceAgent::info() const
{
    return m_info;
}

Ptr<DeviceAgent::IHandler> DeviceAgent::handler() const
{
    return m_handler;
}

void DeviceAgent::doSetSettings(
    Result<const ISettingsResponse*>* /*outResult*/, const IStringMap* /*values*/)
{
}

void DeviceAgent::getPluginSideSettings(
    Result<const ISettingsResponse*>* /*outResult*/) const
{
}

void DeviceAgent::getManifest(Result<const IString*>* outResult) const
{
    interceptExceptions(outResult,
        [&]()
        {
            const QJsonObject manifest = ManifestBuilder::build(*this);
            return new sdk::String(QJson::serialize(manifest).toStdString());
        });
}

void DeviceAgent::setHandler(IHandler* handler)
{
    if (!NX_ASSERT(!m_handler))
        return;

    m_handler = addRefToPtr(handler);
}

void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* outResult, const IMetadataTypes* types)
{
    interceptExceptions(outResult,
        [&]()
        {
            m_metadataMonitor.setNeededTypes(addRefToPtr(types));
        });
}

std::vector<const EventType*> DeviceAgent::fetchSupportedEventTypes() const
{
    try
    {
        Url url = m_info->url();
        url.setUserName(m_info->login());
        url.setPassword(m_info->password());
        url.setPath("/cgi-bin/eventManager.cgi");
        url.setQuery("action=getExposureEvents");

        std::set<const EventType*> types;

        http::futures::Client httpClient;
        const auto response = Exception::translate(
            [&]{ return http::futures::Client().get(url).get(); });
        verifySuccessOrThrow(response);

        NX_VERBOSE(this, "Raw list of event types supported by camera:\n%1",
            response.messageBody);

        for (auto line: response.messageBody.split('\n'))
        {
            line = line.trimmed();
            if (line.isEmpty())
                continue;

            const auto pair = line.split('=');
            if (pair.size() != 2)
                throw Exception("Failed to parse response from camera");

            if (const auto type = EventType::findByNativeId(QString::fromUtf8(pair[1])))
                types.insert(type);
        }

        // Hard-coded as this model currently fails to report support for this event even though
        // it does support it.
        const auto model = QString(m_info->model()).toLower();
        if (model.startsWith("itc215-pw6m-irlzf"))
            types.insert(&EventType::kTrafficJunction);

        NX_VERBOSE(this, "Ids of event types supported by camera:\n%1",
            [&]()
            {
                std::vector<QString> ids;
                for (const auto type: types)
                    ids.push_back(NX_FMT("\t%1", type->id));
                return nx::utils::join(ids, "\n");
            }());

        return std::vector(types.begin(), types.end());
    }
    catch (Exception& exception)
    {
        exception.addContext("Failed to fetch supported event types");
        throw;
    }
}

} // namespace nx::vms_server_plugins::analytics::dahua
