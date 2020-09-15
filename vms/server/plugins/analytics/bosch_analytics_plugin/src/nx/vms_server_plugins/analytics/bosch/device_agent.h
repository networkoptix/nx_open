// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>

#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/consuming_device_agent.h>

#include <nx/network/http/http_client.h>

#include "engine.h"
#include "id_cache.h"
#include "metadata_xml_parser.h"

namespace nx::vms_server_plugins::analytics::bosch {

using nx::sdk::analytics::IDataPacket;
using nx::sdk::analytics::IMetadataPacket;
using nx::sdk::analytics::ICustomMetadataPacket;

using nx::sdk::Result;
using nx::sdk::IDeviceInfo;
using nx::sdk::ISettingsResponse;
using nx::sdk::IString;
using nx::sdk::IStringMap;

using nx::sdk::Ptr;
using nx::sdk::makePtr;

//using nx::sdk::analytics::Rect;
using nx::sdk::analytics::EventMetadata;
using nx::sdk::analytics::EventMetadataPacket;
using nx::sdk::analytics::ObjectMetadata;
using nx::sdk::analytics::ObjectMetadataPacket;

struct DeviceInfo
{
    nx::utils::Url url;
    QString model;
    QString firmware;
    QAuthenticator auth;
    QString uniqueId;
    QString sharedId;
    int channelNumber = 0;

    void init(const IDeviceInfo* deviceInfo);
};

class DeviceAgent: public nx::sdk::RefCountable<nx::sdk::analytics::IConsumingDeviceAgent>
{
public:
    DeviceAgent(Engine* engine, const IDeviceInfo* deviceInfo);

    virtual ~DeviceAgent() override;

protected:
    virtual void doSetSettings(
        Result<const ISettingsResponse*>* outResult, const IStringMap* settings) override;

    virtual void getPluginSideSettings(
        Result<const ISettingsResponse*>* outResult) const override;

    virtual void getManifest(Result<const IString*>* outResult) const override;

    virtual void setHandler(
        nx::sdk::analytics::IDeviceAgent::IHandler* IHandler) override;

    virtual void doSetNeededMetadataTypes(
        Result<void>* outValue,
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

    virtual void doPushDataPacket(
        Result<void>* outResult, IDataPacket* dataPacket) override;

    virtual void finalize() override {}

private:

    int buildInitialManifest();

    void updateAgentManifest(const ParsedMetadata& parsedMetadata);

    Ptr<EventMetadataPacket> buildEventPacket(
        const ParsedMetadata& parsedMetadata, int64_t ts) const;

    Ptr<EventMetadataPacket> buildObjectDetectionEventPacket(int64_t ts) const;

    Ptr<ObjectMetadataPacket> buildObjectPacket(
        const ParsedMetadata& parsedMetadata, int64_t ts) const;

private:
    mutable nx::Mutex m_mutex;
    Bosch::DeviceAgentManifest m_manifest;

    Engine* const m_engine;
    nx::sdk::Ptr<nx::sdk::analytics::IDeviceAgent::IHandler> m_handler;
    DeviceInfo m_deviceInfo;

    /** Initially empty, may be replenished after each incoming metadata (doPushDataPacket call). */
    QSet<QString> m_wsntTopics;

    IdCache m_idCache;
    mutable UuidCache m_uuidCache;

public:
    /** This event type corresponds to no wsnt topic, it is added explicitly manually. */
    static const inline QString kObjectDetectionEventTypeId = "nx.bosch.Detect_any_object";
    static const inline QString kObjectDetectionObjectTypeId = "nx.bosch.ObjectDetection.AnyObject";
};

} // namespace nx::vms_server_plugins::analytics::bosch
