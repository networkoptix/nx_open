#pragma once

#include <QtCore/QString>

#include <core/resource/resource_fwd.h>

#include <nx/sdk/common_settings.h>
#include <nx/sdk/analytics/engine.h>
#include <nx/sdk/analytics/device_agent.h>
#include <nx/mediaserver/sdk_support/pointers.h>

namespace nx::mediaserver::analytics::debug_helpers {

/** @return nullptr if the file does not exist, or on error. */
sdk_support::UniquePtr<nx::sdk::Settings> loadSettingsFromFile(
    const QString& fileDescription,
    const QString& filename);

static QString settingsFilename(
    const char* const path,
    const QString& pluginLibName,
    const QString& extraSuffix = "");

void setDeviceAgentSettings(
        sdk_support::SharedPtr<nx::sdk::analytics::DeviceAgent> deviceAgent,
        const QnVirtualCameraResourcePtr& device);

void setEngineSettings(
        sdk_support::SharedPtr<nx::sdk::analytics::Engine> engine);


} // namespace nx::mediaserver::analytics::debug_helpers
