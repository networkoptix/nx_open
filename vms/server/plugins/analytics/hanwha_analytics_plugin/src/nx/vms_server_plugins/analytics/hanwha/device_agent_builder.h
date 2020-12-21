#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QSet>
#include <QJsonValue>

#include <nx/sdk/i_device_info.h>
#include <plugins/resource/hanwha/hanwha_shared_resource_context.h>
#include <plugins/resource/hanwha/hanwha_information.h>
#include <plugins/resource/hanwha/hanwha_request_helper.h>

#include "device_agent.h"
#include "engine.h"
#include "common.h"
#include "settings_capabilities.h"
#include "setting_primitives.h"

namespace nx::vms_server_plugins::analytics::hanwha {

class DeviceAgentBuilder
{
    class InformationTypeDirect {};
    class InformationTypeBypassed {};

    struct Information
    {
        Information(InformationTypeDirect,
            std::shared_ptr<nx::vms::server::plugins::HanwhaSharedResourceContext>& context,
            int channelNumber);

        Information(InformationTypeBypassed,
            std::shared_ptr<nx::vms::server::plugins::HanwhaSharedResourceContext>& context,
            int channelNumber);

        nx::vms::server::plugins::HanwhaAttributes attributes;
        nx::vms::server::plugins::HanwhaCgiParameters cgiParameters;
        std::map<QString, QString> eventStatusMap;
        int channelNumber = 0;
        bool isNvr = false;

        bool isValid() const;
    };

public:
    DeviceAgentBuilder(
        const nx::sdk::IDeviceInfo* deviceInfo,
        Engine* engine,
        std::shared_ptr<nx::vms::server::plugins::HanwhaSharedResourceContext>& context);

    std::unique_ptr<DeviceAgent> createDeviceAgent() const;

private:
    bool hasSubmenu(const QString endpoint, const QString submenu);

    SettingsCapabilities fetchSettingsCapabilities() const;

    RoiResolution fetchRoiResolution() const;

    bool fetchIsObjectDetectionSupported() const;

    QStringList fetchEventTypeFamilyNamesInternal(const Information& info) const;
    QStringList fetchEventTypeFamilyNames() const;

    QStringList fetchInternalEventTypeNamesForPopulousFamiliesInternal(
        const Information& info) const;
    QStringList fetchInternalEventTypeNamesForPopulousFamilies() const;

    QString fetchBoxTemperatureEventTypeNameInternal(const Information& info) const;
    QString fetchBoxTemperatureEventTypeName() const;

    QStringList obtainEventTypeIds(
        const QStringList& eventTypeBunchNames,
        const QStringList& eventTypeInternalNamesForBunches) const;

    QStringList addTemepatureChangeEventTypeNameIfNeeded(const QStringList& eventTypeIds) const;

    QStringList addObjectDetectionEventTypeNamesIfNeeded(const QStringList& eventTypeIds) const;

    QStringList buildSupportedEventTypeIds() const;

    QStringList buildSupportedObjectTypeIds() const;

    QJsonValue buildSettingsModel(
        bool isObjectDetectionSupported,
        const SettingsCapabilities& settingsCapabilities,
        const QSet<QString>& supportedEventTypeSet) const;

    Hanwha::DeviceAgentManifest buildDeviceAgentManifest(
        const SettingsCapabilities& settingsCapabilities) const;

private:
    // m_deviceInfo and m_engine are transit parameters that are not used by the builder directly
    // (except in constructor), but are passed to DeviceAgent constructor.
    const nx::sdk::IDeviceInfo* const m_deviceInfo;
    Engine* const m_engine;

    const Hanwha::EngineManifest& m_engineManifest;
    const QString m_deviceDebugInfo; //< Used in NX_DEBUG. Has format `name (id)`.
    const Information m_directInfo;
    const Information m_bypassedInfo; //< Is used for NVRs only. Is used together with m_directInfo.
};

} // namespace nx::vms_server_plugins::analytics::hanwha
