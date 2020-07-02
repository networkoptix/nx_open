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
        Entry<bool> enabled;

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

        //struct LineCrossingDetection
        //{
        //    struct Rule
        //    {
        //        NX_CAMERA_SETTINGS_ENTRY(Vca.LineCrossingDetection.Rule#, Line, NamedLine) line;
        //    };
        //    std::vector<Rule> rules;
        //};
        //std::optional<LineCrossingDetection> lineCrossingDetection;

        //struct MissingObjectDetection
        //{
        //    struct Rule
        //    {
        //        NX_CAMERA_SETTINGS_ENTRY(Vca.MissingObjectDetection.Rule#, Region,
        //            NamedPolygon) region;

        //        NX_CAMERA_SETTINGS_ENTRY(Vca.MissingObjectDetection.Rule#, MinSize,
        //            nx::sdk::analytics::Rect) minSize;

        //        NX_CAMERA_SETTINGS_ENTRY(Vca.MissingObjectDetection.Rule#, MaxSize,
        //            nx::sdk::analytics::Rect) maxSize;

        //        NX_CAMERA_SETTINGS_ENTRY(Vca.MissingObjectDetection.Rule#, Delay, int) delay;

        //        NX_CAMERA_SETTINGS_ENTRY(
        //            Vca.MissingObjectDetection.Rule#, RequiresHumanInvolvement,
        //            bool) requiresHumanInvolvement;
        //    };
        //    std::vector<Rule> rules;
        //};
        //std::optional<MissingObjectDetection> missingObjectDetection;

        //struct UnattendedObjectDetection
        //{
        //    struct Rule
        //    {
        //        NX_CAMERA_SETTINGS_ENTRY(Vca.UnattendedObjectDetection.Rule#, Region,
        //            NamedPolygon) region;

        //        NX_CAMERA_SETTINGS_ENTRY(Vca.UnattendedObjectDetection.Rule#, MinSize,
        //            nx::sdk::analytics::Rect) minSize;

        //        NX_CAMERA_SETTINGS_ENTRY(Vca.UnattendedObjectDetection.Rule#, MaxSize,
        //            nx::sdk::analytics::Rect) maxSize;

        //        NX_CAMERA_SETTINGS_ENTRY(Vca.UnattendedObjectDetection.Rule#, Delay, int) delay;

        //        NX_CAMERA_SETTINGS_ENTRY(
        //            Vca.UnattendedObjectDetection.Rule#, RequiresHumanInvolvement,
        //            bool) requiresHumanInvolvement;
        //    };
        //    std::vector<Rule> rules;
        //};
        //std::optional<UnattendedObjectDetection> unattendedObjectDetection;

        //struct FaceDetection
        //{
        //    struct Rule
        //    {
        //        NX_CAMERA_SETTINGS_ENTRY(Vca.FaceDetection.Rule#, Region, NamedPolygon) region;
        //    };
        //    std::vector<Rule> rules;
        //};
        //std::optional<FaceDetection> faceDetection;

        //struct RunningDetection
        //{
        //    struct Rule
        //    {
        //        NX_CAMERA_SETTINGS_ENTRY(Vca.RunningDetection.Rule#, Name, QString) name;
        //        NX_CAMERA_SETTINGS_ENTRY(Vca.RunningDetection.Rule#, MinCount, int) minCount;
        //        NX_CAMERA_SETTINGS_ENTRY(Vca.RunningDetection.Rule#, MinSpeed, int) minSpeed;
        //        NX_CAMERA_SETTINGS_ENTRY(Vca.RunningDetection.Rule#, Delay, int) delay;
        //    };
        //    std::vector<Rule> rules;
        //};
        //std::optional<RunningDetection> runningDetection;
    };
    std::optional<Vca> vca;

public:
    void fetchFrom(const nx::utils::Url& cameraUrl);
    void storeTo(const nx::utils::Url& cameraUrl);

    void parseFromServer(const nx::sdk::IStringMap& values);
    nx::sdk::Ptr<nx::sdk::SettingsResponse> serializeToServer() const;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
