#pragma once

#include <QtCore/QString>

#include <boost/optional/optional.hpp>

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>

namespace nx {
namespace mediaserver {
namespace plugins {

static const char* kPluginManifestTemplate = R"manifest(
    {
        "driverId": "{6541F47D-99B8-4049-BF55-697BD6DD1BBD}",
        "driverName": {
            "value": "Hanwha metadata driver",
            "localization": {
            }
        },
        "outputEventTypes": [
            {
                "eventTypeId": "%1",
                "eventName": {
                    "value": "Face detection",
                    "localization": {
                    }
                }
            },
            {
                "eventTypeId": "%2",
                "eventName": {
                    "value": "Virtual line",
                    "localization": {
                    }
                }
            },
            {
                "eventTypeId": "%3",
                "eventName": {
                    "value": "Entering",
                    "localization": {
                    }
                }
            },
            {
                "eventTypeId": "%4",
                "eventName": {
                    "value": "Exiting",
                    "localization": {
                    }
                }
            },
            {
                "eventTypeId": "%5",
                "eventName": {
                    "value": "Appearing",
                    "localization": {
                    }
                }
            },
            {
                "eventTypeId": "%6",
                "eventName": {
                    "value": "Disappearing",
                    "localization": {
                    }
                }
            },
            {
                "eventTypeId": "%7",
                "eventName": {
                    "value": "Audio detection",
                    "localization": {
                    }
                }
            },
            {
                "eventTypeId": "%8",
                "eventName": {
                    "value": "Tampering",
                    "localization": {
                    }
                }
            },
            {
                "eventTypeId": "%9",
                "eventName": {
                    "value": "Defocusing",
                    "localization": {
                    }
                }
            },
            {
                "eventTypeId": "%10",
                "eventName": {
                    "value": "Dry contact input",
                    "localization": {
                    }
                }
            },
            {
                "eventTypeId": "%11",
                "eventName": {
                    "value": "Motion detection",
                    "localization": {
                    }
                }
            },
            {
                "eventTypeId": "%12",
                "eventName": {
                    "value": "Sound classification",
                    "localization": {
                    }
                }
            },
            {
                "eventTypeId": "%13",
                "eventName": {
                    "value": "Loitering",
                    "localization": {
                    }
                }
            }
        ]
    }
    )manifest";

static const char* kDeviceManifestTemplate = R"manifest(
    {
        "supportedEventTypes": [
            %1
        ]
    }
    )manifest";

static const nxpl::NX_GUID kHanwhaFaceDetectionEventId =
    {{0x5D,0xFC, 0x6D, 0x1B, 0x3B, 0x92, 0x4E, 0x55, 0xA6, 0x27, 0x0D, 0xB2, 0xA2, 0x42, 0x2D, 0xB3}};

static const nxpl::NX_GUID kHanwhaVirtualLineEventId =
    {{0x67, 0xED, 0xDD, 0x6A, 0xA3, 0x1F, 0x41, 0xC4, 0x91, 0xB6, 0xB6, 0x29, 0x8F, 0xE2, 0x57, 0xA1}};

static const nxpl::NX_GUID kHanwhaEnteringEventId =
    {{0x6F, 0x60, 0x24, 0x03, 0xCC, 0xA4, 0x4F, 0x85, 0x87, 0x77, 0xF3, 0x40, 0xE8, 0x23, 0x9A, 0x23}};

static const nxpl::NX_GUID kHanwhaExitingEventId =
    {{0x6F, 0x60, 0x24, 0x03, 0xCC, 0xA4, 0x4F, 0x85, 0x87, 0x77, 0xF3, 0x40, 0xE8, 0x23, 0x9A, 0x23}};

static const nxpl::NX_GUID kHanwhaAppearingEventId =
    {{0xA4, 0x52, 0xE4, 0x9A, 0xD1, 0xD8, 0x44, 0xEA, 0x84, 0xDA, 0xF5, 0x81, 0x41, 0x4E, 0x81, 0x53}};

static const nxpl::NX_GUID kHanwhaDisappearingEventId =
    {{0x42, 0xD4, 0x99, 0x4A, 0xA2, 0x67, 0x4C, 0x39, 0x98, 0xAA, 0xCB, 0xC4, 0xD4, 0x1C, 0xA4, 0x2B}};

static const nxpl::NX_GUID kHanwhaAudioDetectionEventId =
    {{0xCA, 0xBB, 0x46, 0x44, 0xF6, 0xC2, 0x4F, 0x09, 0x8E, 0xDF, 0xE0, 0xFA, 0xFC, 0xC4, 0xF8, 0x33}};

static const nxpl::NX_GUID kHanwhaTamperingEventId =
    {{0x65, 0x7B, 0x39, 0xC2, 0x36, 0xC1, 0x46, 0x44, 0x93, 0x57, 0x58, 0x59, 0x36, 0x58, 0xD5, 0x67}};

static const nxpl::NX_GUID kHanwhaDefocusingEventId =
    {{0x21, 0x66, 0x68, 0xD9, 0xEA, 0x9B, 0x48, 0x3A, 0xA4, 0x3E, 0x94, 0x14, 0xED, 0x76, 0x0A, 0x01}};

static const nxpl::NX_GUID kHanwhaDryContactInputEventId =
    {{0x1B, 0xAB, 0x8A, 0x57, 0x5F, 0x19, 0x4E, 0x3A, 0xB7, 0x3B, 0x36, 0x41, 0x05, 0x8D, 0x46, 0xB8}};

static const nxpl::NX_GUID kHanwhaMotionDetectionEventId =
    {{0xF1, 0xF7, 0x23, 0xBB, 0x20, 0xC8, 0x45, 0x3A, 0xAF, 0xCE, 0x86, 0xF3, 0x18, 0xCB, 0xF0, 0x97}};

static const nxpl::NX_GUID kHanwhaSoundClassificationEventId =
    {{0x29, 0x28, 0x48, 0x45, 0xE4, 0x69, 0x48, 0x21, 0xA2, 0x21, 0x18, 0x51, 0x46, 0xF4, 0x0E, 0xAE}};

static const nxpl::NX_GUID kHanwhaLoiteringEventId =
    {{0x1B, 0x54, 0x1C, 0xB7, 0xE3, 0x77, 0x48, 0x18, 0x88, 0x01, 0x32, 0x32, 0x81, 0x53, 0x0E, 0x7B}};

inline QString str(const nxpl::NX_GUID& guid)
{
    return QString::fromStdString(nxpt::NxGuidHelper::toStdString(guid));
}

enum class HanwhaEventItemType
{
    // TODO: #dmishin fill proper event item types
    ein,
    zwei,
    drei,
    none
};

struct HanwhaEvent
{
    nxpl::NX_GUID typeId;
    QString caption;
    QString description;
    boost::optional<int> channel;
    boost::optional<int> region;
    bool isActive = false;
    HanwhaEventItemType itemType; //< e.g Gunshot for sound classification
};

using HanwhaEventList = std::vector<HanwhaEvent>;

} // namespace plugins
} // namespace mediaserver
} // namespace nx
