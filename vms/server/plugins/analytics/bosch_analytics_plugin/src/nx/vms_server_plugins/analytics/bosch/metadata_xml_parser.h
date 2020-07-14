// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>
#include <QtCore/QXmlStreamReader>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include <nx/network/http/http_client.h>

#include "engine.h"

namespace nx::vms_server_plugins::analytics::bosch {

struct OnvifEventMessage
{
    QString propertyOperation;
    int source = 1;
    bool state = false;
};

struct ParsedEvent
{
    QString topic;
    bool isActive = false;

};

struct Point
{
    float x = 0.f; //< [-1, 1]
    float y = 0.f; //< [-1, 1]
};

struct Rect
{
    float bottom = -1.f;
    float top = 1.f;
    float right = 1.f;
    float left =-1.f;
};

struct Shape
{
    Rect boundingBox;
    Point centerOfGravity;
};

struct ParsedObject
{
    int id = 0;
    float velocity = 0.f;
    float area = 0.f;
    Shape shape;
};

struct ParsedMetadata
{
    QList<ParsedEvent> events;
    QList<ParsedObject> objects;
};

//template<int N>
//class KnownTags
//{
////    bool data[N];
//    const char* tags[N];
//    constexpr index(const char*)
//    {
//        auto it = std::find(std::cbegin(data), std::cend(data), [](const char*) {strcmp});
//        if (it == std::cend(data))
//        {
//            skip();
//            return -1;
//        }
//        else
//            return it - std::cbegin(data);
//
//    }
//};

class BaseMetadataXmlParser
{
public:
    explicit BaseMetadataXmlParser(const QByteArray& xmlData): reader(xmlData) {}

    bool start(const char* rootTagName);
    bool next();
    void skip();
    bool isOnTag(const char* tag) const;

    template<class ...T>
    constexpr int knownTagIndex(T... t)
    {
        bool data[]{ isOnTag(t)... }; //< transform parameter list to array
        auto tagIterator = std::find(std::cbegin(data), std::cend(data), true);
        if (tagIterator == std::cend(data))
        {
            skip();
            return 0;
        }
        else
            return tagIterator - std::cbegin(data) + 1;
    }

    QString text();
    QXmlStreamAttributes attributeList() const;

private:
    QXmlStreamReader reader;
};

class MetadataXmlParser: private BaseMetadataXmlParser
{
public:
    explicit MetadataXmlParser(const QByteArray& xmlData): BaseMetadataXmlParser(xmlData) {}
    ParsedMetadata parse();

    QList<ParsedObject> parseVideoAnalyticsElement();
    ParsedObject parseObjectElement();
    Shape parseShapeElement();

    QList<ParsedEvent> parseEventElement();
    ParsedEvent parseNotificationMessageElement();
    OnvifEventMessage parseOnvifMessageElement();

};

} // namespace nx::vms_server_plugins::analytics::bosch
