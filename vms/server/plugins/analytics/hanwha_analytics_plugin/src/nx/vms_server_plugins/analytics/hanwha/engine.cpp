#include "engine.h"

#include <map>

#include <QtCore/QString>
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>

#include <nx/network/http/http_client.h>
#include <plugins/resource/hanwha/hanwha_cgi_parameters.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/std/algorithm.h>

#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>

#include "device_agent.h"
#include "device_agent_builder.h"
#include "common.h"
#include "string_helper.h"

namespace nx::vms_server_plugins::analytics::hanwha {

namespace {

static const QString kSamsungTechwinVendor("samsung");
static const QString kHanwhaTechwinVendor("hanwha");

// TODO: Decide on these unused constants.
static const std::chrono::milliseconds kAttributesTimeout(4000);
static const QString kAttributesPath("/stw-cgi/attributes.cgi/cgis");

static bool isFirmwareActual(const std::string& firmwareVersion)
{
    if (ini().disableFirmwareVersionCheck)
        return true;

    // Firmware version has a format like this: 1.40.03_20190919_R32
    if (firmwareVersion.empty())
        return false;

    QString extendedVersion = QString::fromStdString(firmwareVersion);
    QStringList versionPartitions = extendedVersion.split('_'); //< {1.40.03 , 20190919 , R32}
    if (versionPartitions.empty())
        return false;

    QStringList versionList = versionPartitions[0].split('.'); //< {1 , 40 , 03}
    if (versionList.size() < 2)
        return false;

    int major = versionList[0].toInt();
    int minor = versionList[1].toInt();

    int revision = (versionList.size() >= 3) ? versionList[2].toInt() : 0;
    if (revision == 99 && (minor % 10) == 9)
        return false; //< beta version

#if 0
    // Only old devices use version 2 (they have their own numbering)
    return major > 1 || (major == 1 && minor >= 41);
#endif
    return major == 1 && minor >= 41;
}

QJsonValue filterOutItemsAndSections(QJsonValue value)
{
    if (!value.isObject())
        return QJsonValue::Undefined;
    auto object = value.toObject();
    object["items"] = QJsonArray();
    object["sections"] = QJsonArray();
    return object;
}

} // namespace

//-------------------------------------------------------------------------------------------------

Ini::Ini(): IniConfig("nx_hanwha_plugin.ini")
{
    reload();
}

Ini& ini()
{
    static Ini ini;
    return ini;
}

//-------------------------------------------------------------------------------------------------

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::SharedResources::SharedResources(
    const QString& sharedId,
    const Hanwha::EngineManifest& engineManifest,
    const nx::utils::Url& url,
    const QAuthenticator& auth)
    :
    monitor(std::make_unique<MetadataMonitor>(engineManifest, url, auth)),
    sharedContext(std::make_shared<vms::server::plugins::HanwhaSharedResourceContext>(
        /*serverRuntimeEventManager*/ nullptr,
        /*resourcePool*/ nullptr,
        /*globalSettings*/ nullptr,
        sharedId))
{
    sharedContext->setResourceAccess(url, auth);
}

void Engine::SharedResources::setResourceAccess(
    const nx::utils::Url& url,
    const QAuthenticator& auth)
{
    sharedContext->setResourceAccess(url, auth);
    monitor->setResourceAccess(url, auth);
}

//-------------------------------------------------------------------------------------------------

Engine::Engine(Plugin* plugin): m_plugin(plugin)
{
    QByteArray manifestData;
    QFile f(":/hanwha/manifest.json");
    QString filename = QFileInfo(f).absoluteFilePath();
    if (f.open(QFile::ReadOnly))
        manifestData = f.readAll();
    else
        NX_WARNING(this, "Unable to open Engine manifest in Server resource %1", filename);

    {
        QFile file("plugins/hanwha/manifest.json");
        filename = QFileInfo(file).absoluteFilePath();
        if (file.open(QFile::ReadOnly))
        {
            NX_INFO(this, "Switching to external Engine manifest file %1", filename);
            manifestData = file.readAll();
        }
    }
    bool success = false;
    m_manifest = QJson::deserialized<Hanwha::EngineManifest>(
        manifestData, /*defaultValue*/ {}, &success);
    if (!success)
        NX_WARNING(this, "Invalid Engine manifest JSON in file %1", filename);

    m_manifest.initializeObjectTypeMap();

    QByteArray attributeFiltersData;
    QFile attributeFiltersFile(":/hanwha/object_metadata_attribute_filters.json");
    if (attributeFiltersFile.open(QFile::ReadOnly))
        attributeFiltersData = attributeFiltersFile.readAll();
    {
        QFile file("plugins/hanwha/object_metadata_attribute_filters.json");
        if (file.open(QFile::ReadOnly))
        {
            NX_INFO(this,
                lm("Switch to external object metadata attribute filters file %1").
                arg(QFileInfo(file).absoluteFilePath()));
            attributeFiltersData = file.readAll();
        }
    }
    m_objectMetadataAttributeFilters =
        QJson::deserialized<Hanwha::ObjectMetadataAttributeFilters>(attributeFiltersData);
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    if (!isCompatible(deviceInfo))
    {
        *outResult = error(ErrorCode::otherError, "Device is not compatible with the Engine");
        return;
    }

    auto sharedRes = sharedResources(deviceInfo);
    auto sharedResourceGuard = nx::utils::makeScopeGuard(
        [&sharedRes, deviceInfo, this]()
        {
            if (sharedRes->deviceAgentCount == 0)
                m_sharedResources.remove(QString::fromUtf8(deviceInfo->sharedId()));
        });

    std::shared_ptr<nx::vms::server::plugins::HanwhaSharedResourceContext>& context =
        sharedRes->sharedContext;

    DeviceAgentBuilder builder(deviceInfo, this, context);
    std::unique_ptr<DeviceAgent> deviceAgent = builder.createDeviceAgent();
    if (deviceAgent)
        deviceAgent->loadAndHoldDeviceSettings();

    ++sharedRes->deviceAgentCount;

    *outResult = deviceAgent.release();
}

void Engine::getManifest(Result<const IString*>* outResult) const
{
    *outResult = new nx::sdk::String(QJson::serialized(m_manifest));
}

/*virtual*/ void Engine::setEngineInfo(const nx::sdk::analytics::IEngineInfo* /*engineInfo*/)
{
}

/*virtual*/ void Engine::doSetSettings(
    Result<const ISettingsResponse*>* /*outResult*/, const IStringMap* /*settings*/)
{
    // There are no Engine settings for this plugin.
}

/*virtual*/ void Engine::getPluginSideSettings(
    Result<const ISettingsResponse*>* /*outResult*/) const
{
}

/*virtual*/ void Engine::doExecuteAction(
    Result<IAction::Result>* /*outResult*/, const IAction* /*action*/)
{
}

const Hanwha::EngineManifest& Engine::manifest() const
{
    return m_manifest;
}

const Hanwha::ObjectMetadataAttributeFilters& Engine::objectMetadataAttributeFilters() const
{
    return m_objectMetadataAttributeFilters;
}

MetadataMonitor* Engine::monitor(
    const QString& sharedId,
    const nx::utils::Url& url,
    const QAuthenticator& auth)
{
    std::shared_ptr<SharedResources> monitorCounter;
    {
        QnMutexLocker lock(&m_mutex);
        auto sharedResourcesItr = m_sharedResources.find(sharedId);
        if (sharedResourcesItr == m_sharedResources.cend())
        {
            sharedResourcesItr = m_sharedResources.insert(
                sharedId,
                std::make_shared<SharedResources>(
                    sharedId,
                    m_manifest,
                    url,
                    auth));
        }

        monitorCounter = sharedResourcesItr.value();
        ++monitorCounter->monitorUsageCount;
    }

    return monitorCounter->monitor.get();
}

void Engine::deviceAgentStoppedToUseMonitor(const QString& sharedId)
{
    QnMutexLocker lock(&m_mutex);
    auto sharedResources = m_sharedResources.value(sharedId);
    if (!sharedResources)
        return;

    if (sharedResources->monitorUsageCount)
        --sharedResources->monitorUsageCount;

    if (sharedResources->monitorUsageCount <= 0)
        m_sharedResources[sharedId]->monitor->stopMonitoring();
}

void Engine::deviceAgentIsAboutToBeDestroyed(const QString& sharedId)
{
    QnMutexLocker lock(&m_mutex);
    auto sharedResources = m_sharedResources.value(sharedId);
    if (!sharedResources)
        return;

    --sharedResources->deviceAgentCount;
    NX_ASSERT(sharedResources->deviceAgentCount >= 0);
    if (sharedResources->deviceAgentCount <= 0)
        m_sharedResources.remove(sharedId);
}

std::shared_ptr<Engine::SharedResources> Engine::sharedResources(const IDeviceInfo* deviceInfo)
{
    const nx::utils::Url url(deviceInfo->url());

    QAuthenticator auth;
    auth.setUser(deviceInfo->login());
    auth.setPassword(deviceInfo->password());

    QnMutexLocker lock(&m_mutex);
    auto sharedResourcesItr = m_sharedResources.find(deviceInfo->sharedId());
    if (sharedResourcesItr == m_sharedResources.cend())
    {
        sharedResourcesItr = m_sharedResources.insert(
            deviceInfo->sharedId(),
            std::make_shared<SharedResources>(
                deviceInfo->sharedId(),
                m_manifest,
                url,
                auth));
    }
    else
    {
        sharedResourcesItr.value()->setResourceAccess(url, auth);
    }

    return sharedResourcesItr.value();
}

/*virtual*/ void Engine::setHandler(IEngine::IHandler* /*handler*/)
{
    // TODO: Use the handler for error reporting.
}

/*virtual*/ bool Engine::isCompatible(const IDeviceInfo* deviceInfo) const
{
    const QString vendor = QString(deviceInfo->vendor()).toLower();
    return vendor.startsWith(kHanwhaTechwinVendor) || vendor.startsWith(kSamsungTechwinVendor);
}

std::shared_ptr<vms::server::plugins::HanwhaSharedResourceContext> Engine::sharedContext(
    QString m_sharedId)
{
    auto sharedResourcesItr = m_sharedResources.find(m_sharedId);
    if (sharedResourcesItr != m_sharedResources.cend())
    {
        return sharedResourcesItr.value()->sharedContext;
    }
    else
        return {};
}

} // namespace nx::vms_server_plugins::analytics::hanwha

//-------------------------------------------------------------------------------------------------

namespace {

static const std::string kPluginManifest = /*suppress newline*/ 1 + (const char*) R"json(
{
    "id": "nx.hanwha",
    "name": "Hanwha analytics",
    "description": "Supports built-in analytics on Hanwha cameras",
    "version": "1.0.1",
    "vendor": "Hanwha"
}
)json";

} // namespace

extern "C" {

NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin()
{
    return new nx::sdk::analytics::Plugin(
        kPluginManifest,
        [](nx::sdk::analytics::IPlugin* plugin)
        {
            return new nx::vms_server_plugins::analytics::hanwha::Engine(
                dynamic_cast<nx::sdk::analytics::Plugin*>(plugin));
        });
}

} // extern "C"
