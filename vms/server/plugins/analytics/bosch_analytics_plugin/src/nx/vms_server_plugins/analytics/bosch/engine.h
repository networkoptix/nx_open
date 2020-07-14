// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray.h>

#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/analytics/helpers/engine.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>

#include "common.h"

namespace nx::vms_server_plugins::analytics::bosch {

using nx::sdk::analytics::IEngineInfo;
using nx::sdk::analytics::IEngine;
using nx::sdk::analytics::IDeviceAgent;
using nx::sdk::analytics::IAction;

using nx::sdk::RefCountable;
using nx::sdk::Result;

using nx::sdk::IDeviceInfo;
using nx::sdk::IString;
using nx::sdk::IStringMap;
using nx::sdk::ISettingsResponse;

struct EngineInfo
{
    std::string id;
    std::string name;
    void loadFrom(const IEngineInfo* info) { id = info->id(); name = info->name(); }
};

class QManifestByteArray: public QByteArray
{
public:
    QManifestByteArray();
};

class Engine: public RefCountable<IEngine>
{
public:
    Engine(nx::sdk::analytics::Plugin* plugin);

    const Bosch::EngineManifest& manifest() const { return m_manifest; }
    const QByteArray& manifestByteArray() const { return m_manifestByteArray; }
    const EngineInfo& engineInfo() const { return m_engineInfo; }

protected:
    virtual void setEngineInfo(const IEngineInfo* engineInfo) override;

    virtual void doSetSettings(
        Result<const ISettingsResponse*>* outResult, const IStringMap* settings) override;

    virtual void getPluginSideSettings(Result<const ISettingsResponse*>* outResult) const override;

    virtual void getManifest(Result<const IString*>* outResult) const override;

    virtual bool isCompatible(const IDeviceInfo* deviceInfo) const override;

    virtual void doObtainDeviceAgent(
        Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo) override;

    virtual void doExecuteAction(
        Result<IAction::Result>* outResult, const IAction* action) override;

    virtual void setHandler(IHandler* handler) override;

private:
    nx::sdk::analytics::Plugin* const m_plugin;
    const QManifestByteArray m_manifestByteArray;
    const Bosch::EngineManifest m_manifest;
    EngineInfo m_engineInfo;

};

} // namespace nx::vms_server_plugins::analytics::bosch
