#pragma once

#include <optional>
#include <variant>
#include <utility>

#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/settings_response.h>
#include <nx/sdk/analytics/rect.h>
#include <nx/utils/url.h>

#include <QtCore/QString>
#include <QtCore/QJsonValue>

#include "geometry.h"

namespace nx::vms_server_plugins::analytics::vivotek {

class CameraSettings
{
public:
    template <typename Value>
    struct Entry
    {
        std::optional<Value> value;
        std::optional<QString> error;
    };

    struct Vca
    {
        Entry<int> sensitivity;

        struct Installation
        {
            Entry<int> height;
            Entry<int> tiltAngle;
            Entry<int> rollAngle;

            std::vector<Entry<Polygon>> excludedRegions;

            Installation();
        } installation;

        struct CrowdDetection
        {
            struct Rule
            {
                QString name;
                Entry<Polygon> region;

                Entry<int> minPersonCount;
                Entry<int> entranceDelay;
                Entry<int> exitDelay;
            };
            std::vector<Rule> rules;

            CrowdDetection();
        };
        std::optional<CrowdDetection> crowdDetection;

        struct LoiteringDetection
        {
            struct Rule
            {
                QString name;
                Entry<Polygon> region;

                Entry<int> minDuration;
            };
            std::vector<Rule> rules;

            LoiteringDetection();
        };
        std::optional<LoiteringDetection> loiteringDetection;

        struct IntrusionDetection
        {
            enum class Direction
            {
                outToIn,
                inToOut,
            };

            struct Rule
            {
                QString name;
                Entry<Polygon> region;

                Entry<Direction> direction;
            };
            std::vector<Rule> rules;

            IntrusionDetection();
        };
        std::optional<IntrusionDetection> intrusionDetection;

        struct LineCrossingDetection
        {
            enum class Direction
            {
                any,
                leftToRight,
                rightToLeft,
            };

            struct Rule
            {
                QString name;
                Entry<Line> line;
                Entry<Direction> direction;
            };
            std::vector<Rule> rules;
            
            LineCrossingDetection();
        };
        std::optional<LineCrossingDetection> lineCrossingDetection;

        struct MissingObjectDetection
        {
            struct Rule
            {
                QString name;
                Entry<Polygon> region;

                Entry<SizeConstraints> sizeConstraints;
                Entry<int> minDuration;
                Entry<bool> requiresHumanInvolvement;
            };
            std::vector<Rule> rules;

            MissingObjectDetection();
        };
        std::optional<MissingObjectDetection> missingObjectDetection;

        struct UnattendedObjectDetection
        {
            struct Rule
            {
                QString name;
                Entry<Polygon> region;

                Entry<SizeConstraints> sizeConstraints;
                Entry<int> minDuration;
                Entry<bool> requiresHumanInvolvement;
            };
            std::vector<Rule> rules;

            UnattendedObjectDetection();
        };
        std::optional<UnattendedObjectDetection> unattendedObjectDetection;

        struct FaceDetection
        {
            struct Rule
            {
                QString name;
                Entry<Polygon> region;
            };
            std::vector<Rule> rules;

            FaceDetection();
        };
        std::optional<FaceDetection> faceDetection;

        struct RunningDetection
        {
            struct Rule
            {
                Entry<QString> name;
                Entry<int> minPersonCount;
                Entry<int> minSpeed;
                Entry<int> minDuration;
                Entry<int> maxMergeInterval;
            };
            std::vector<Rule> rules;

            RunningDetection();
        };
        std::optional<RunningDetection> runningDetection;
    };
    std::optional<Vca> vca;

public:
    void fetchFrom(const nx::utils::Url& cameraUrl);
    void storeTo(const nx::utils::Url& cameraUrl);

    void parseFromServer(const nx::sdk::IStringMap& values);
    nx::sdk::Ptr<nx::sdk::SettingsResponse> serializeToServer() const;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
